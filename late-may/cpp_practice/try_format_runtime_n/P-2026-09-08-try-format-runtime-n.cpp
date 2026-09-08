// P-2026-09-08 — try_format_runtime_n — std::expected<std::size_t,
// FormatError> wrapper around std::vformat_to that measures the byte count
// first via the hand-rolled counting_output_iterator (Sep 6's — libc++ 21
// does NOT ship std::counting_output_iterator), checks the measured byte
// count against the caller-provided byte cap, and writes the formatted
// output into a caller-provided char* buffer (the std::format_to_n
// shape — NOT std::format_to / std::format_to_string).
//
// Closes Sep 7's "Where we go next" item #1 — the bounded-buffer
// sibling of Sep 7's try_format_bounded.  The format-string parameter
// is std::string_view (the RUNTIME sibling — the consteval sibling of
// this shape is the next open item, Sep 7's item #4, and will be a
// separate lesson).  The error type is petra::FormatError{kind, message,
// needed_bytes} REUSED verbatim from Sep 6 / Sep 7.
//
// The shape:
//
//   petra::try_format_runtime_n<Args...>(char* out, std::size_t cap,
//                                        std::string_view fmt,
//                                        Args&&... args)
//       -> std::expected<std::size_t, petra::FormatError>
//
// On success: returns expected<size_t> = bytes actually written into
// out[0, cap).  The caller is responsible for NUL-termination if it
// wants a C-string (NOT done here — this matches std::format_to_n's
// raw contract; if the caller wants NUL-termination Aug 25's
// format_to_n_into is the matching sibling).  The number of bytes
// written equals the measured byte count (no partial truncation —
// the pass-1 measure-then-write pair guarantees this; Section 12 of
// Sep 6 documented the same invariant for try_format_bounded).
//
// On TooLarge (pass-1 measure reports needed > cap): returns
// unexpected{FormatError{TooLarge, "formatted output would be N bytes;
// cap is C", needed_bytes=N}}.  out is UNTOUCHED.
//
// On format_error (pass 1 throws std::format_error): returns
// unexpected{FormatError{Format, e.what()}}.  out is UNTOUCHED.
//
// On partial-write invariant violation (pass 2 wrote fewer bytes than
// pass 1 measured — defensive check, should never fire on libc++ 21):
// returns unexpected{FormatError{TooLarge, "back_insert wrote X bytes;
// measure pass said Y", needed_bytes=Y}}.  out may be partially
// populated by pass 2.
//
// The two-pass measure-then-write design is the same shape Sep 6 /
// Sep 7 used: pass 1 measures via std::vformat_to +
// petra::counting_output_iterator; pass 2 (only on pass-1 success +
// needed <= cap) writes via std::vformat_to + a hand-rolled
// bounded_char_writer that sinks through out[i] = c with an
// external counter (because std::vformat_to copies the iterator
// internally, the counter MUST live externally — same trap Sep 6
// hit when it borrowed Aug 28's machinery).
//
// C++23, std::format, std::vformat_to, std::make_format_args,
// std::expected (P0323R12), std::println (P2093R14).  Requires
// -fexperimental-library on Apple Clang 21.0.0 / libc++ 21 for
// std::println.
//
// Build:
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
//           P-2026-09-08-try-format-runtime-n.cpp \
//           -o /tmp/P-2026-09-08-try-format-runtime-n
//
// Strict:
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
//           -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion \
//           -Wsign-conversion \
//           P-2026-09-08-try-format-runtime-n.cpp \
//           -o /tmp/P-2026-09-08-try-format-runtime-n-strict
//
// ASan + UBSan:
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
//           -fsanitize=address -fsanitize=undefined \
//           -fno-omit-frame-pointer -O1 \
//           P-2026-09-08-try-format-runtime-n.cpp \
//           -o /tmp/P-2026-09-08-try-format-runtime-n-asan

#include <atomic>
#include <cstddef>
#include <cstring>
#include <expected>
#include <format>
#include <functional>
#include <iterator>
#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace petra {

// ----- FormatError (reused from Sep 6 / Sep 7) ----------------------------
//
// Structured error type for std::format_error plus the bounded
// wrapper's TooLarge variant (which carries the would-be byte count).
// FormatErrorKind has four values; on libc++ 21 only Format and TooLarge
// are observed in practice — FormatParse / FormatArgument are reserved
// for future stdlib versions that ship the std::format_parse_error /
// std::format_argument_error subclass types.

enum class FormatErrorKind : unsigned char {
    FormatParse,     // reserved — not throwable on libc++ 21.
    FormatArgument,  // reserved — not throwable on libc++ 21.
    Format,          // mapped from std::format_error (catch-all on libc++ 21).
    TooLarge,        // raised by petra::try_format_runtime_n / Sep 6's
                     // try_format_runtime_bounded / Sep 7's try_format_bounded
                     // when the measure pass reports more bytes than the
                     // caller cap.  Carries the would-be byte count in
                     // FormatError::needed_bytes.
};

struct FormatError {
    FormatErrorKind kind{FormatErrorKind::Format};
    std::string message;
    std::size_t needed_bytes{0};  // populated for TooLarge.

    FormatError() = default;
    FormatError(FormatErrorKind k, std::string m,
                std::size_t needed = 0) noexcept
        : kind(k), message(std::move(m)), needed_bytes(needed) {}

    [[nodiscard]] bool operator==(const FormatError& other) const noexcept {
        return kind == other.kind && message == other.message
            && needed_bytes == other.needed_bytes;
    }
    [[nodiscard]] bool operator!=(const FormatError& other) const noexcept {
        return !(*this == other);
    }
};

// ----- counting_output_iterator (reused from Sep 6 / Sep 7) ------------
//
// Hand-rolled counting_output_iterator modeled on the C++23 [format.range]
// example shape.  The user passes a callback (function pointer) plus a
// void* user-data slot.  Each byte that would have been written through
// the iterator increments an internal counter and invokes the callback
// with the live count.  The callback design is essential because
// std::vformat_to (via libc++ 21's __direct_iterator_buffer path) makes
// an internal copy of the iterator — any state stored inside the
// iterator object itself is invisible to the caller after the call
// (Sep 6 documented this with a trace probe: the iterator's `this`
// pointer inside std::vformat_to is at a DIFFERENT address than the
// caller's iterator, and the live count is updated on the copy, not
// the original).
//
// The proxy type is required to satisfy std::output_iterator<const char&>;
// std::indirectly_writable<I, const char&> requires the proxy's
// operator= to be const-qualified so that const_cast<const proxy&&> = char
// (one of the four expressions the concept checks) is well-formed.

class counting_output_iterator {
public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = void;
    using callback_type = void (*)(std::size_t, void*);

    struct proxy {
        counting_output_iterator* it_;

        // const-qualified so const_cast<const proxy&&> = char in the
        // std::indirectly_writable concept is well-formed.  Accept char
        // by value (libc++'s __direct_iterator_buffer writes a single
        // char per slot).
        constexpr const proxy& operator=(char) const noexcept {
            ++it_->count_;
            if (it_->cb_) it_->cb_(it_->count_, it_->data_);
            return *this;
        }
    };

    constexpr counting_output_iterator(callback_type cb, void* data) noexcept
        : cb_(cb), data_(data) {}

    constexpr proxy operator*() const noexcept {
        return proxy{const_cast<counting_output_iterator*>(this)};
    }
    constexpr counting_output_iterator& operator++() noexcept { return *this; }
    constexpr counting_output_iterator operator++(int) noexcept { return *this; }

    std::size_t count() const noexcept { return count_; }

    callback_type callback() const noexcept { return cb_; }
    void* user_data() const noexcept { return data_; }

private:
    std::size_t count_{0};
    callback_type cb_;
    void* data_;
};

// Compile-time assertion that the hand-rolled iterator's proxy satisfies
// std::indirectly_writable.  Without this assertion a refactor that drops
// the `const` qualifier on proxy::operator= would silently break the
// wrapper at the first call site (the vformat_to overload set rejects
// non-output iterators with a 'no matching function' error that doesn't
// point at the iterator).
static_assert(std::indirectly_writable<counting_output_iterator, const char&>,
              "petra::counting_output_iterator must satisfy "
              "std::indirectly_writable for const char&");
static_assert(std::output_iterator<counting_output_iterator, const char&>,
              "petra::counting_output_iterator must satisfy "
              "std::output_iterator for const char&");

}  // namespace petra

// ----- try_format_runtime_n ---------------------------------------------

namespace petra {

namespace detail {

// bounded_char_writer — the pass-2 write sink.  Wraps a caller-provided
// char* + cap, increments an external counter on each successful write
// (passed via a callback exactly like counting_output_iterator —
// because std::vformat_to copies the iterator internally), and stops
// accepting bytes once the cap is reached (the overflow byte is NOT
// written — the writer goes to a failed-stop state, mirroring how
// std::format_to_n's `n` parameter bounds the write).
//
// On the success path std::vformat_to will write exactly `needed`
// bytes (because pass 1 measured `needed` and we early-returned on
// `needed > cap`), so the counter at the end of the call equals the
// number of bytes actually written.  The defensive check on
// out_written != needed in try_format_runtime_n_impl guards against a
// hypothetical future libc++ that breaks the pass-1/pass-2 invariant.
//
// The iterator design mirrors counting_output_iterator verbatim — the
// only difference is the proxy::operator= callback (which writes to
// the char* buffer instead of just incrementing a counter).

class bounded_char_writer {
public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = void;
    using callback_type = void (*)(std::size_t, void*);

    struct proxy {
        bounded_char_writer* it_;

        // const-qualified for the same std::indirectly_writable concept
        // reason as counting_output_iterator::proxy::operator=.
        constexpr const proxy& operator=(char c) const noexcept {
            if (it_->written_ < it_->cap_) {
                it_->buf_[it_->written_] = c;
                ++it_->written_;
                if (it_->cb_) it_->cb_(it_->written_, it_->data_);
            }
            // overflow: silently drop (matches std::format_to_n's n cap).
            return *this;
        }
    };

    constexpr bounded_char_writer(char* buf, std::size_t cap,
                                  callback_type cb, void* data) noexcept
        : buf_(buf), cap_(cap), cb_(cb), data_(data) {}

    constexpr proxy operator*() const noexcept {
        return proxy{const_cast<bounded_char_writer*>(this)};
    }
    constexpr bounded_char_writer& operator++() noexcept { return *this; }
    constexpr bounded_char_writer operator++(int) noexcept { return *this; }

    std::size_t written() const noexcept { return written_; }

private:
    char* buf_;
    std::size_t cap_;
    std::size_t written_{0};
    callback_type cb_;
    void* data_;
};

// Compile-time assertion that bounded_char_writer satisfies the same
// iterator concepts as counting_output_iterator.
static_assert(std::indirectly_writable<bounded_char_writer, const char&>,
              "petra::bounded_char_writer must satisfy "
              "std::indirectly_writable for const char&");
static_assert(std::output_iterator<bounded_char_writer, const char&>,
              "petra::bounded_char_writer must satisfy "
              "std::output_iterator for const char&");

// Two-pass measure-then-write pair.
//
// Pass 1: std::vformat_to + counting_output_iterator measures the
//         formatted output's byte count via the callback.
// Pass 2: std::vformat_to + bounded_char_writer writes into the
//         caller's char* buffer up to the cap (which we early-returned
//         on if needed > cap).
//
// On any pass-1 std::format_error, returns a Format unexpected.
// On needed > cap, returns a TooLarge unexpected with the actual
// needed byte count.  The caller's buffer is UNTOUCHED in both cases.
// On success, returns expected<size_t> = bytes actually written.

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_impl(char* out, std::size_t cap,
                          std::string_view fmt, const Args&... args) {
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };

    // Pass 1: measure.
    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt,
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }

    // Sized-too-big path.  out is untouched (we haven't written yet).
    if (needed > cap) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"formatted output would be "}
                            + std::to_string(needed)
                            + " bytes; cap is " + std::to_string(cap),
                        needed});
    }

    // Pass 2: write into out[0, cap).  std::vformat_to +
    // bounded_char_writer is guaranteed not to throw format_error here
    // because pass 1 already accepted the format string and the
    // format_args are the same.
    std::size_t written = 0;
    try {
        bounded_char_writer sink{out, cap, cb, &written};
        (void)std::vformat_to(std::move(sink), fmt,
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        // Should be unreachable — pass 1 already exercised the same
        // call.  But we keep the catch so the wrapper is robust
        // against std::make_format_args / std::vformat_to state
        // changes between the two calls.  In this branch out MAY be
        // partially written (the bytes std::vformat_to managed to emit
        // before the throw).  We document this in the lesson notes.
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }

    // Invariant check: pass 2 wrote exactly `needed` bytes.  If a
    // future libc++ ever makes this invariant violated, the caller
    // gets a clear error rather than a silently-truncated buffer.
    if (written != needed) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"bounded_char_writer wrote "}
                            + std::to_string(written)
                            + " bytes; measure pass said "
                            + std::to_string(needed),
                        needed});
    }

    return written;
}

}  // namespace detail

// Public surface.  Runtime-format-string form.  Mirrors std::format_to_n's
// signature shape: (out, n, fmt, args...) — out first, n second,
// args last.
//
// std::string_view fmt + const Args&... matches Sep 6's
// try_format_runtime_bounded.  The consteval sibling of this
// signature (std::format_string<Args...>) is Sep 7's item #4 —
// a separate lesson.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(char* out, std::size_t cap,
                     std::string_view fmt, const Args&... args) {
    return detail::try_format_runtime_n_impl(out, cap, fmt, args...);
}

}  // namespace petra

// ----- test harness ------------------------------------------------------

namespace {

int g_pass = 0;
int g_fail = 0;

void check(bool cond, const char* expr, const char* file, int line) {
    if (cond) {
        ++g_pass;
        std::println("  PASS  {}", expr);
    } else {
        ++g_fail;
        std::println("  FAIL  {}  ({}:{})", expr, file, line);
    }
}

// CHECK takes a single expression.  Use a variadic macro so an
// expression containing commas inside braces (e.g.
// std::string_view{buf.data(), 4}) is treated as ONE macro argument
// rather than two — the preprocessor otherwise splits on the inner
// comma.  Same convention Sep 5 / Sep 6 / Sep 7 use.
#define CHECK(...) check((__VA_ARGS__), #__VA_ARGS__, __FILE__, __LINE__)

void section(const char* name) {
    std::println("-- section {}", name);
}

// Helper: build a buffer of `n` bytes initialised to a poison value
// (so we can prove the wrapper only touches [0, written) and that
// out[written..n) is left untouched on the success path).  Returns a
// vector<char> with the poison byte so the caller can compare
// post-state to expected.
std::vector<char> poison_buf(std::size_t n, char poison = 'Q') {
    return std::vector<char>(n, poison);
}

}  // namespace

int main() {
    // Section 1 — toolchain + feature probes.
    section("1.probes");
    {
        CHECK((std::is_default_constructible_v<petra::FormatError>));
        CHECK((std::is_nothrow_move_constructible_v<petra::FormatError>));
        CHECK((std::is_same_v<
              decltype(petra::try_format_runtime_n(
                  static_cast<char*>(nullptr), std::size_t{0},
                  std::string_view{})),
              std::expected<std::size_t, petra::FormatError>>));
        CHECK(petra::FormatErrorKind::FormatParse
              != petra::FormatErrorKind::FormatArgument);
        CHECK(petra::FormatErrorKind::TooLarge != petra::FormatErrorKind::Format);
        CHECK(static_cast<unsigned char>(petra::FormatErrorKind::FormatParse) == 0);
        CHECK(static_cast<unsigned char>(petra::FormatErrorKind::FormatArgument) == 1);
        CHECK(static_cast<unsigned char>(petra::FormatErrorKind::Format) == 2);
        CHECK(static_cast<unsigned char>(petra::FormatErrorKind::TooLarge) == 3);
        // bounded_char_writer is a separate type from
        // counting_output_iterator even though they share structure.
        CHECK(!std::is_same_v<petra::counting_output_iterator,
                              petra::detail::bounded_char_writer>);
    }

    // Section 2 — happy path: fits exactly.
    section("2.fits-exactly");
    {
        // cap == needed: 4 bytes for "v=42".
        auto buf = poison_buf(4);
        auto r = petra::try_format_runtime_n(buf.data(), 4, "v={}", 42);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 4);
        CHECK(r.has_value() && std::string_view{buf.data(), 4} == "v=42");
    }
    {
        // cap == needed: 7 bytes for "d=3.142".
        auto buf = poison_buf(7);
        auto r = petra::try_format_runtime_n(buf.data(), 7,
                                             "d={:.3f}", 3.14159);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 7);
        CHECK(r.has_value() && std::string_view{buf.data(), 7} == "d=3.142");
    }
    {
        // No replacement fields — literal "ABCDE" (5 bytes) fits
        // exactly into a 5-byte buffer.
        auto buf = poison_buf(5);
        auto r = petra::try_format_runtime_n(buf.data(), 5, "ABCDE");
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 5);
        CHECK(r.has_value() && std::string_view{buf.data(), 5} == "ABCDE");
    }

    // Section 3 — happy path: cap larger than needed (no truncation).
    section("3.cap-larger-than-needed");
    {
        // cap=10 but "v=42" needs 4 bytes.
        auto buf = poison_buf(10);
        auto r = petra::try_format_runtime_n(buf.data(), 10, "v={}", 42);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 4);
        CHECK(r.has_value() && std::string_view{buf.data(), 4} == "v=42");
        // The wrapper does NOT write past written — bytes 4..9 are
        // still the poison byte 'Q' (this proves the bounded writer
        // stops at the cap and does NOT, e.g., NUL-terminate past
        // written — Aug 25's format_to_n_into is the NUL-terminating
        // sibling; today is the raw shape).
        CHECK(buf[4] == 'Q');
        CHECK(buf[9] == 'Q');
    }

    // Section 4 — TooLarge: cap smaller than needed.
    section("4.too-large");
    {
        // cap=3 but "v=42" needs 4 bytes.
        auto buf = poison_buf(3);
        auto r = petra::try_format_runtime_n(buf.data(), 3, "v={}", 42);
        CHECK(!r.has_value());
        CHECK(!r.has_value() && r.error().kind
              == petra::FormatErrorKind::TooLarge);
        CHECK(!r.has_value() && r.error().needed_bytes == 4);
        CHECK(!r.has_value() && !r.error().message.empty());
        // Buffer untouched — all 3 bytes still 'Q'.
        CHECK(buf[0] == 'Q');
        CHECK(buf[1] == 'Q');
        CHECK(buf[2] == 'Q');
    }
    {
        // cap=6 but "d=3.142" needs 7 bytes — TooLarge.
        auto buf = poison_buf(6);
        auto r = petra::try_format_runtime_n(buf.data(), 6,
                                             "d={:.3f}", 3.14159);
        CHECK(!r.has_value());
        CHECK(!r.has_value() && r.error().kind
              == petra::FormatErrorKind::TooLarge);
        CHECK(!r.has_value() && r.error().needed_bytes == 7);
        // Buffer untouched.
        CHECK(buf[0] == 'Q');
        CHECK(buf[5] == 'Q');
    }
    {
        // cap=0 but "" needs 0 bytes — should succeed (zero bytes fit).
        auto buf = poison_buf(0);
        auto r = petra::try_format_runtime_n(buf.data(), 0, "");
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 0);
    }
    {
        // cap=0 but "x" needs 1 byte — TooLarge.
        auto buf = poison_buf(0);
        auto r = petra::try_format_runtime_n(buf.data(), 0, "x");
        CHECK(!r.has_value());
        CHECK(!r.has_value() && r.error().needed_bytes == 1);
    }

    // Section 5 — Format error path: bad format string.
    section("5.format-error");
    {
        // Unterminated replacement field — caught at format time
        // because today's surface is the RUNTIME sibling.
        auto buf = poison_buf(10);
        auto r = petra::try_format_runtime_n(buf.data(), 10, "{:");
        CHECK(!r.has_value());
        CHECK(!r.has_value() && r.error().kind
              == petra::FormatErrorKind::Format);
        CHECK(!r.has_value() && !r.error().message.empty());
        // Buffer untouched on Format-error path.
        CHECK(buf[0] == 'Q');
    }
    {
        // Too-few args.
        auto buf = poison_buf(10);
        auto r = petra::try_format_runtime_n(buf.data(), 10,
                                             "{} {} {}", 1, 2);
        CHECK(!r.has_value());
        CHECK(!r.has_value() && r.error().kind
              == petra::FormatErrorKind::Format);
        CHECK(buf[0] == 'Q');
    }

    // Section 6 — atomicity: failing call leaves prior success
    // untouched (the previous success's `r.value()` is still valid
    // after a follow-up failure's `std::unexpected` construction).
    section("6.atomicity");
    {
        auto buf = poison_buf(10);
        auto ok = petra::try_format_runtime_n(buf.data(), 10, "ok={}", 1);
        CHECK(ok.has_value() && ok.value() == 4);
        CHECK(std::string_view{buf.data(), 4} == "ok=1");

        // Follow-up failing call with a different buffer.
        auto buf2 = poison_buf(10);
        auto bad = petra::try_format_runtime_n(buf2.data(), 10, "{:");
        CHECK(!bad.has_value());

        // The previous good value is unaffected.
        CHECK(ok.has_value() && ok.value() == 4);
        CHECK(std::string_view{buf.data(), 4} == "ok=1");
    }

    // Section 7 — multi-type args round-trip.
    section("7.multi-type-args");
    {
        // "{}={} h={:#x}" with args "v", 42, 0xcafe.
        // "v=42 h=0xcafe" — 13 bytes (v,=,4,2,space,h,=,0,x,c,a,f,e).
        auto buf = poison_buf(32);
        auto r = petra::try_format_runtime_n(buf.data(), 32,
                                             "{}={} h={:#x}",
                                             "v", 42, 0xcafe);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 13);
        CHECK(r.has_value() && std::string_view{buf.data(), 13}
              == "v=42 h=0xcafe");
    }
    {
        // "d={:.3f} s={:>8} l={:<6}" with args 3.14159, "x", "abc".
        // "d=3.142 s=       x l=abc   " — 27 bytes
        // (d=3.142=7, space=1, s=2, "       x"=8, space=1, l=2,
        //  "abc   "=6 -> 7+1+2+8+1+2+6 = 27).
        auto buf = poison_buf(64);
        auto r = petra::try_format_runtime_n(
            buf.data(), 64, "d={:.3f} s={:>8} l={:<6}",
            3.14159, "x", "abc");
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 27);
        CHECK(r.has_value() && std::string_view{buf.data(), 27}
              == "d=3.142 s=       x l=abc   ");
    }

    // Section 8 — std::counting_output_iterator availability probe.
    section("8.platform-counting-output-iterator");
    {
        // std::counting_output_iterator is NOT shipped by libc++ 21 at
        // all.  Pin the same way Sep 6 / Sep 7 did: a constexpr bool
        // static_assert declaring the absence.
        constexpr bool libcxx21_has_std_counting_output_iterator = false;
        static_assert(!libcxx21_has_std_counting_output_iterator,
                      "std::counting_output_iterator is documented as absent "
                      "on this toolchain — flip this only if a future libc++ "
                      "ships it AND we want to migrate from the hand-rolled "
                      "petra::counting_output_iterator to the standard one");
        CHECK(!libcxx21_has_std_counting_output_iterator);
    }

    // Section 9 — UTF-8 mid-codepoint byte round-trip.
    section("9.utf8");
    {
        // U+2713 CHECK MARK (✓) — 3 UTF-8 bytes: 0xE2 0x9C 0x93.
        // "x=" is 2 bytes, plus 3 UTF-8 bytes = 5 bytes total.
        std::string payload{"\xE2\x9C\x93"};
        auto buf = poison_buf(16);
        auto r = petra::try_format_runtime_n(buf.data(), 16,
                                             "x={}", payload);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 5);
        CHECK(r.has_value() && std::string_view{buf.data(), 5}
              == "x=\xE2\x9C\x93");
    }

    // Section 10 — embedded NUL byte round-trip.
    section("10.nul-byte");
    {
        // 4-byte payload "A\0B\0" — embedded NULs are values, not
        // terminators, when the caller provides an explicit byte count.
        // std::string_view spans up to (but not including) the first
        // NUL by default — so we use std::string with explicit length.
        std::string sv{"A\0B\0", 4};
        auto buf = poison_buf(16);
        auto r = petra::try_format_runtime_n(buf.data(), 16, "{}", sv);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 4);
        CHECK(r.has_value() && std::memcmp(buf.data(), "A\0B\0", 4) == 0);
    }

    // Section 11 — exact byte-count invariant: written == needed.
    section("11.byte-count-invariant");
    {
        // A pass that succeeded and a follow-up measurement pass
        // agree.
        std::string_view fmt{"hello {}"};
        auto buf = poison_buf(16);
        auto r = petra::try_format_runtime_n(buf.data(), 16, fmt,
                                             "world");
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 11);

        // Independent measurement via counting_output_iterator must
        // agree byte-for-byte with r.value().
        std::size_t measured = 0;
        auto cb = [](std::size_t n, void* data) noexcept {
            *static_cast<std::size_t*>(data) = n;
        };
        petra::counting_output_iterator counter{cb, &measured};
        (void)std::vformat_to(std::move(counter), fmt,
                              std::make_format_args("world"));
        CHECK(measured == r.value());
        CHECK(measured == 11);  // "hello world" — 11 bytes
    }

    // Section 12 — TooLarge boundary: cap == needed-1, cap == needed,
    // cap == needed+1.
    section("12.too-large-boundary");
    {
        // "ABCDE" — 5 bytes.
        for (std::size_t cap : {std::size_t{3}, std::size_t{4},
                                std::size_t{5}, std::size_t{6},
                                std::size_t{100}}) {
            auto buf = poison_buf(cap);
            auto r = petra::try_format_runtime_n(buf.data(), cap,
                                                 "ABCDE");
            if (cap >= 5) {
                CHECK(r.has_value());
                CHECK(r.has_value() && r.value() == 5);
                CHECK(r.has_value()
                      && std::string_view{buf.data(), 5} == "ABCDE");
            } else {
                CHECK(!r.has_value());
                CHECK(!r.has_value() && r.error().needed_bytes == 5);
                // Buffer untouched on TooLarge path.
                CHECK(buf[0] == 'Q');
                CHECK(buf[cap - 1] == 'Q');
            }
        }
    }

    // Section 13 — FormatError equality / inequality.
    section("13.format-error-equality");
    {
        petra::FormatError a{petra::FormatErrorKind::TooLarge, "x", 5};
        petra::FormatError b{petra::FormatErrorKind::TooLarge, "x", 5};
        CHECK(a == b);
        CHECK(!(a != b));

        petra::FormatError c{petra::FormatErrorKind::TooLarge, "x", 6};
        CHECK(a != c);  // different needed_bytes

        petra::FormatError d{petra::FormatErrorKind::Format, "x", 5};
        CHECK(a != d);  // different kind
    }

    // Section 14 — 50-round determinism on the size measurement.
    section("14.determinism-50rounds");
    {
        for (int i = 0; i < 50; ++i) {
            auto buf = poison_buf(16);
            auto r = petra::try_format_runtime_n(buf.data(), 16,
                                                 "i={}", i);
            CHECK(r.has_value());
            std::string expected = std::string{"i="} + std::to_string(i);
            CHECK(r.has_value() && r.value() == expected.size());
            CHECK(r.has_value()
                  && std::string_view{buf.data(), expected.size()}
                     == expected);
        }
    }

    // Section 15 — 4 threads x 25 calls each.  Each thread uses its
    // own buffer (no shared mutable state — the wrapper is thread-safe
    // as long as the output buffer is not shared).  The atomic
    // counter just tallies mismatches across all threads.
    section("15.concurrency");
    {
        std::atomic<int> mismatch{0};
        constexpr int kThreads = 4;
        constexpr int kPerThread = 25;
        std::vector<std::thread> threads;
        threads.reserve(kThreads);
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([t, &mismatch] {
                char buf[32];
                for (int i = 0; i < kPerThread; ++i) {
                    auto r = petra::try_format_runtime_n(
                        buf, 32, "t={} i={} v={}", t, i, i * 7);
                    std::string expected = std::string{"t="}
                                          + std::to_string(t)
                                          + " i=" + std::to_string(i)
                                          + " v=" + std::to_string(i * 7);
                    if (!r.has_value()
                        || r.value() != expected.size()
                        || std::string_view{buf, r.value()} != expected) {
                        mismatch.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }
        for (auto& th : threads) th.join();
        CHECK(mismatch.load() == 0);
    }

    // Section 16 — distinct from Sep 7's try_format_bounded: returns
    // size_t, not std::string; writes into a caller buffer, not a
    // std::string.  Verify the return type and the byte-count contract.
    //
    // Sep 7's lesson defined petra::try_format_bounded(cap, fmt, args...)
    // returning std::expected<std::string, petra::FormatError>; today's
    // sibling returns std::expected<std::size_t, petra::FormatError>
    // and writes into a caller-provided char* buffer.
    section("16.distinct-from-try-format-bounded");
    {
        // Today's wrapper returns size_t on success and writes into
        // the caller buffer.
        auto buf = poison_buf(16);
        auto today_r = petra::try_format_runtime_n(buf.data(), 16,
                                                   "v={}", 42);
        CHECK(today_r.has_value());
        CHECK(today_r.has_value() && today_r.value() == 4);
        CHECK(today_r.has_value()
              && std::string_view{buf.data(), 4} == "v=42");

        // Today vs Sep 7 byte-count agreement: both produce 4 bytes
        // for ("v={}", 42).  The difference is the SUCCESS TYPE
        // (size_t today vs std::string in Sep 7) and the OUTPUT
        // LOCATION (caller char* buffer today vs returned std::string
        // in Sep 7).
        CHECK(today_r.value() == 4);

        // On TooLarge the structured-error contract is identical to
        // Sep 6 / Sep 7: FormatError{kind=TooLarge, needed_bytes=N}.
        auto buf2 = poison_buf(3);
        auto today_too_large = petra::try_format_runtime_n(
            buf2.data(), 3, "v={}", 42);
        CHECK(!today_too_large.has_value());
        CHECK(today_too_large.error().kind
              == petra::FormatErrorKind::TooLarge);
        CHECK(today_too_large.error().needed_bytes == 4);
    }

    // Section 17 — bounded_char_writer never writes past the cap.
    //
    // Construct a deliberately-too-small buffer and verify the
    // bounded writer drops overflow bytes.  This is hard to exercise
    // through the public surface (the public surface early-returns on
    // needed > cap BEFORE entering pass 2), so we exercise the writer
    // directly here — this is a focused internal invariant test.
    section("17.bounded-writer-overflow");
    {
        char buf[4] = {'X', 'X', 'X', 'X'};
        petra::detail::bounded_char_writer sink{buf, 4, nullptr, nullptr};
        // Five writes — the 5th must NOT overflow.
        *sink = 'a'; *sink = 'b'; *sink = 'c'; *sink = 'd'; *sink = 'e';
        CHECK(sink.written() == 4);  // capped at the cap.
        CHECK(buf[0] == 'a');
        CHECK(buf[1] == 'b');
        CHECK(buf[2] == 'c');
        CHECK(buf[3] == 'd');
    }

    std::println("");
    std::println("-- summary: {} pass, {} fail", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}