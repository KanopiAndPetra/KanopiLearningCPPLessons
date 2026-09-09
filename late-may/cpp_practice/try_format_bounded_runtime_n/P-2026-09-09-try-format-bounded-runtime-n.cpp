// P-2026-09-09 — try_format_bounded_runtime_n — CONSTEVAL sibling of
// P-2026-09-08's try_format_runtime_n.  Closes Sep 8's "Where we go
// next" item #1 — the natural sibling of P-2026-09-07's
// try_format_bounded with today's bounded-buffer output location.
//
// The shape:
//
//   petra::try_format_bounded_runtime_n<Args...>(char* out,
//                                                std::size_t cap,
//                                                std::format_string<Args...> fmt,
//                                                Args&&... args)
//       -> std::expected<std::size_t, petra::FormatError>
//
// On success: returns expected<size_t> = bytes actually written into
// out[0, cap).  Like Sep 8, does NOT NUL-terminate (matches
// std::format_to_n's raw contract — Aug 25's format_to_n_into is the
// NUL-terminating sibling for the "I just want a string" use case).
//
// On TooLarge (pass-1 measure reports needed > cap): returns
// unexpected{FormatError{TooLarge, "formatted output would be N bytes;
// cap is C", needed_bytes=N}}.  out is UNTOUCHED.
//
// On format_error (pass 1 throws std::format_error from the
// std::make_format_args side): returns
// unexpected{FormatError{Format, e.what()}}.  out is UNTOUCHED.
//
// On partial-write invariant violation (pass 2 wrote fewer bytes than
// pass 1 measured — defensive check): returns
// unexpected{FormatError{TooLarge, ..., needed_bytes=Y}}.  out may be
// partially populated.
//
// The consteval surface: malformed format strings are HARD COMPILE
// ERRORS at the call site (Sep 7 documented the consteval sibling of
// Sep 6).  Today's surface is the consteval sibling of Sep 8 — the
// consteval check applies to the format-string parameter (the parse
// side), and the runtime wrapper catches std::format_error from the
// std::make_format_args side (e.g. an integer that doesn't fit the
// requested width).
//
// How today's lesson relates to the three predecessors:
//
//   Lesson                       fmt type           output          success type
//   ---------------------------- -----------------  --------------  ----------------
//   Sep 6 (runtime, bounded)     std::string_view   std::string     std::string
//   Sep 7 (consteval, bounded)   std::format_string std::string     std::string
//   Sep 8 (runtime, n-buffer)    std::string_view   char*           std::size_t
//   TODAY  (consteval, n-buffer) std::format_string char*           std::size_t
//
// Today's wrapper is the CONSTEVAL BOUNDED-BUFFER combination: Sep 7's
// format-string check + Sep 8's output location + Sep 8's return type.
//
// The two-pass measure-then-write design carries over verbatim from
// Sep 6 / Sep 7 / Sep 8.  Pass 1 measures via std::vformat_to +
// petra::counting_output_iterator; pass 2 (only on pass-1 success +
// needed <= cap) writes via std::vformat_to +
// petra::detail::bounded_char_writer.  The pass-1/pass-2 invariant
// (pass 2 writes exactly `needed` bytes) is preserved.
//
// C++23, std::format, std::vformat_to, std::format_string<Args...>
// (P2216R3), std::make_format_args, std::format_error, std::expected
// (P0323R12), std::println (P2093R14).  Requires -fexperimental-library
// on Apple Clang 21.0.0 / libc++ 21 for std::println.
//
// Build:
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
//           P-2026-09-09-try-format-bounded-runtime-n.cpp \
//           -o /tmp/P-2026-09-09-try-format-bounded-runtime-n
//
// Strict:
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
//           -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion \
//           -Wsign-conversion \
//           P-2026-09-09-try-format-bounded-runtime-n.cpp \
//           -o /tmp/P-2026-09-09-try-format-bounded-runtime-n-strict
//
// ASan + UBSan:
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
//           -fsanitize=address -fsanitize=undefined \
//           -fno-omit-frame-pointer -O1 \
//           P-2026-09-09-try-format-bounded-runtime-n.cpp \
//           -o /tmp/P-2026-09-09-try-format-bounded-runtime-n-asan

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

// ----- FormatError (reused verbatim from Sep 6 / Sep 7 / Sep 8) ----------
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
    TooLarge,        // raised by the bounded wrappers (Sep 6 / Sep 7 / Sep 8 /
                     // today) when the measure pass reports more bytes than
                     // the caller cap.  Carries the would-be byte count in
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

// ----- counting_output_iterator (reused verbatim from Sep 6 / Sep 7 / Sep 8)
//
// Hand-rolled counting_output_iterator modeled on the C++23 [format.range]
// example shape.  The user passes a callback (function pointer) plus a
// void* user-data slot.  Each byte that would have been written through
// the iterator increments an internal counter and invokes the callback
// with the live count.  The callback design is essential because
// std::vformat_to (via libc++ 21's __direct_iterator_buffer path) makes
// an internal copy of the iterator — any state stored inside the
// iterator object itself is invisible to the caller after the call.

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
        // std::indirectly_writable concept is well-formed.
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

static_assert(std::indirectly_writable<counting_output_iterator, const char&>,
              "petra::counting_output_iterator must satisfy "
              "std::indirectly_writable for const char&");
static_assert(std::output_iterator<counting_output_iterator, const char&>,
              "petra::counting_output_iterator must satisfy "
              "std::output_iterator for const char&");

}  // namespace petra

// ----- bounded_char_writer (reused verbatim from Sep 8) ------------------
//
// Wraps a caller-provided char* + cap, increments an external counter
// on each successful write (passed via a callback exactly like
// counting_output_iterator — because std::vformat_to copies the
// iterator internally), and stops accepting bytes once the cap is
// reached (the overflow byte is NOT written — the writer goes to a
// failed-stop state, mirroring how std::format_to_n's `n` parameter
// bounds the write).

namespace petra {
namespace detail {

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

static_assert(std::indirectly_writable<bounded_char_writer, const char&>,
              "petra::bounded_char_writer must satisfy "
              "std::indirectly_writable for const char&");
static_assert(std::output_iterator<bounded_char_writer, const char&>,
              "petra::bounded_char_writer must satisfy "
              "std::output_iterator for const char&");

// ----- try_format_bounded_runtime_n implementation ---------------------
//
// Two-pass measure-then-write pair.  Same shape as Sep 6 / Sep 7 / Sep 8
// — only the format-string parameter type changes (today:
// std::format_string<Args...>, consteval-checked).
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
try_format_bounded_runtime_n_impl(char* out, std::size_t cap,
                                   std::format_string<Args...> fmt,
                                   const Args&... args) {
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };

    // Pass 1: measure.  Any std::format_error here is the
    // std::make_format_args side (NOT the parse side — that's caught
    // by the consteval ctor of std::format_string<Args...> at the
    // call site, point-blank).
    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt.get(),
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

    // Pass 2: write into out[0, cap).  Same shape as Sep 8 — the
    // bounded_char_writer sinks through out[written] = c and stops
    // accepting bytes once written == cap.
    std::size_t written = 0;
    try {
        bounded_char_writer sink{out, cap, cb, &written};
        (void)std::vformat_to(std::move(sink), fmt.get(),
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        // Should be unreachable — pass 1 already exercised the same
        // call.  But we keep the catch so the wrapper is robust
        // against std::make_format_args / std::vformat_to state
        // changes between the two calls.
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }

    // Invariant check: pass 2 wrote exactly `needed` bytes.
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

// Public surface — CONSTEVAL format-string form + BOUNDED-BUFFER output
// location.  The natural sibling of P-2026-09-07's try_format_bounded
// (which writes into a std::string) with today's bounded-buffer output
// location (the char* buffer).
//
// Mirrors std::format_to_n's signature shape: (out, n, fmt, args...)
// — out first, n second, args last.  std::format_string<Args...>'s
// implicit conversion from `const S&` lets the caller pass a string
// literal directly without an explicit .ctor invocation.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n(char* out, std::size_t cap,
                              std::format_string<Args...> fmt,
                              const Args&... args) {
    return detail::try_format_bounded_runtime_n_impl(out, cap, fmt, args...);
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
// comma.
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
              decltype(petra::try_format_bounded_runtime_n(
                  static_cast<char*>(nullptr), std::size_t{0},
                  std::string_view{})),
              std::expected<std::size_t, petra::FormatError>>));
        // The consteval ctor: compile-time sizeof the type.
        CHECK((std::is_same_v<decltype(std::format_string<int>{"value={}"}),
                              std::format_string<int>>));
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

    // Section 2 — fits-exactly happy path.
    // Mirrors Sep 7's Section 2 — confirms the consteval wrapper
    // produces identical byte counts to the runtime wrapper.
    section("2.fits-exactly");
    {
        // cap == needed: 4 bytes for "v=42".
        auto buf = poison_buf(4);
        auto r = petra::try_format_bounded_runtime_n(buf.data(), 4,
                                                       "v={}", 42);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 4);
        CHECK(r.has_value() && std::string_view{buf.data(), 4} == "v=42");
    }
    {
        // cap == needed: 7 bytes for "d=3.142".
        auto buf = poison_buf(7);
        auto r = petra::try_format_bounded_runtime_n(buf.data(), 7,
                                                       "d={:.3f}", 3.14159);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 7);
        CHECK(r.has_value() && std::string_view{buf.data(), 7} == "d=3.142");
    }
    {
        // No replacement fields — literal "ABCDE" (5 bytes) fits
        // exactly into a 5-byte buffer.
        auto buf = poison_buf(5);
        auto r = petra::try_format_bounded_runtime_n(buf.data(), 5, "ABCDE");
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 5);
        CHECK(r.has_value() && std::string_view{buf.data(), 5} == "ABCDE");
    }

    // Section 3 — cap larger than needed (no truncation).
    section("3.cap-larger-than-needed");
    {
        // cap=10 but "v=42" needs 4 bytes.
        auto buf = poison_buf(10);
        auto r = petra::try_format_bounded_runtime_n(buf.data(), 10,
                                                       "v={}", 42);
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
        // "v=42" needs 4 bytes — cap=3 → TooLarge.
        auto buf = poison_buf(4);
        auto r = petra::try_format_bounded_runtime_n(buf.data(), 3,
                                                       "v={}", 42);
        CHECK(!r.has_value());
        CHECK(!r.has_value()
              && r.error().kind == petra::FormatErrorKind::TooLarge);
        CHECK(!r.has_value() && r.error().needed_bytes == 4);
        CHECK(!r.has_value() && !r.error().message.empty());
        // Buffer is UNTOUCHED on the failure path (every byte still
        // the poison byte 'Q').
        CHECK(buf[0] == 'Q');
        CHECK(buf[1] == 'Q');
        CHECK(buf[2] == 'Q');
        CHECK(buf[3] == 'Q');
    }
    {
        // "d=3.142" needs 7 bytes — cap=6 → TooLarge.
        auto buf = poison_buf(7);
        auto r = petra::try_format_bounded_runtime_n(buf.data(), 6,
                                                       "d={:.3f}", 3.14159);
        CHECK(!r.has_value());
        CHECK(!r.has_value() && r.error().needed_bytes == 7);
        // Buffer is UNTOUCHED.
        CHECK(buf[0] == 'Q');
        CHECK(buf[6] == 'Q');
    }
    {
        // cap=0 with "" → success (zero bytes fit).
        auto buf = poison_buf(1);
        auto r = petra::try_format_bounded_runtime_n(buf.data(), 0, "");
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 0);
        // Buffer is UNTOUCHED on success-with-written==0.
        CHECK(buf[0] == 'Q');
    }
    {
        // cap=0 with "x" → TooLarge (1 byte needed).
        auto buf = poison_buf(1);
        auto r = petra::try_format_bounded_runtime_n(buf.data(), 0, "x");
        CHECK(!r.has_value());
        CHECK(!r.has_value() && r.error().needed_bytes == 1);
        // Buffer is UNTOUCHED.
        CHECK(buf[0] == 'Q');
    }

    // Section 5 — consteval format-string: malformed is a HARD COMPILE
    // ERROR (the consteval sibling of Sep 8's runtime sibling).
    //
    // This section can only check the positive case at runtime — the
    // negative case is verified at COMPILE time, not at runtime.  See
    // the "Important compile-time findings" section in the lesson
    // notes for the /tmp probe files that verify the consteval ctor
    // rejects malformed format strings.
    section("5.consteval-fmt-positive");
    {
        // Valid format strings produce identical results to
        // std::format.  All four are std::format_string<Args...>-typed
        // (the consteval check has already passed by the time we
        // reach the runtime body).
        auto buf1 = poison_buf(100);
        auto r1 = petra::try_format_bounded_runtime_n(buf1.data(), 100,
                                                        "value={}", 42);
        CHECK(r1.has_value() && r1.value() == 8);
        CHECK(r1.has_value() && std::string_view{buf1.data(), r1.value()} == "value=42");

        auto buf2 = poison_buf(100);
        auto r2 = petra::try_format_bounded_runtime_n(buf2.data(), 100,
                                                        "hex={:#x}", 0xcafe);
        CHECK(r2.has_value() && r2.value() == 10);
        CHECK(r2.has_value() && std::string_view{buf2.data(), r2.value()} == "hex=0xcafe");

        auto buf3 = poison_buf(100);
        auto r3 = petra::try_format_bounded_runtime_n(buf3.data(), 100,
                                                        "{:.3f}", 3.14159);
        CHECK(r3.has_value() && r3.value() == 5);
        CHECK(r3.has_value() && std::string_view{buf3.data(), r3.value()} == "3.142");

        auto buf4 = poison_buf(100);
        auto r4 = petra::try_format_bounded_runtime_n(buf4.data(), 100,
                                                        "{}", 3.14159);
        CHECK(r4.has_value() && r4.value() == 7);
        CHECK(r4.has_value() && std::string_view{buf4.data(), r4.value()} == "3.14159");
    }

    // Section 6 — atomicity (failing call leaves prior success untouched).
    section("6.atomicity");
    {
        // Fill buffer with the previous success ("v=42").
        char buf[16];
        std::memcpy(buf, "v=42", 4);
        for (int i = 4; i < 16; ++i) buf[i] = 'Q';

        // Failing call: cap=0, fmt "longer" → TooLarge.
        auto r = petra::try_format_bounded_runtime_n(buf, 0, "longer");
        CHECK(!r.has_value());

        // The previous "v=42" is preserved.
        CHECK(std::string_view{buf, 4} == "v=42");
        // Bytes 4..15 are still the poison byte.
        for (int i = 4; i < 16; ++i) {
            CHECK(buf[i] == 'Q');
        }
    }

    // Section 7 — multi-type args round-trip.
    section("7.multi-type-args");
    {
        // string + int + hex int + float-with-precision + string-with-width.
        auto buf = poison_buf(100);
        auto r = petra::try_format_bounded_runtime_n(
            buf.data(), 100,
            "name={} age={:#x} pi={:.5f} pad={:>10}",
            std::string{"alice"}, 42, 3.14159265, std::string{"hi"});
        CHECK(r.has_value());
        // "name=alice age=0x2a pi=3.14159 pad=        hi" — 45 bytes.
        // name=alice = 10; " age=" = 5; "0x2a" = 4; " pi=" = 4;
        // "3.14159" = 7; " pad=" = 5; "        hi" = 10.  Total = 45.
        CHECK(r.has_value() && r.value() == 45);
        CHECK(r.has_value() && std::string_view{buf.data(), r.value()} ==
              "name=alice age=0x2a pi=3.14159 pad=        hi");
    }

    // Section 8 — std::counting_output_iterator availability probe.
    // (Pin so a future libc++ upgrade can be detected by re-running.)
    section("8.counting-output-iterator-availability");
    {
        // We cannot safely mention std::counting_output_iterator in a
        // way that SFINAE-detects its absence without risking a hard
        // error on libc++ 21 (where the name is genuinely undeclared).
        // So we use a simpler check: assert the hand-rolled type IS
        // visible (which would not be the case if the standard type
        // were shadowing it in the same TU), and document that
        // std::counting_output_iterator remains unnameable on
        // libc++ 21.
        //
        // Sep 6 / Sep 7 / Sep 8 / today all rely on the hand-rolled
        // petra::counting_output_iterator.  When libc++ ships
        // std::counting_output_iterator, the hand-rolled type can be
        // retired.  Sep 6 documented this with a recursive grep
        // across the libc++ 21 header tree.
        CHECK(std::is_default_constructible_v<petra::counting_output_iterator> ==
              false);  // hand-rolled type is NOT default-constructible
                       // (requires cb + data).
        CHECK(std::is_default_constructible_v<
              petra::detail::bounded_char_writer> == false);
    }

    // Section 9 — UTF-8 mid-codepoint byte round-trip.
    section("9.utf8-byte-roundtrip");
    {
        // "héllo" in UTF-8: 'h'=0x68, 'é'=0xc3 0xa9, 'l'=0x6c,
        // 'l'=0x6c, 'o'=0x6f.  Total 6 bytes.  Use a literal to keep
        // the byte count deterministic.
        auto buf = poison_buf(16);
        auto r = petra::try_format_bounded_runtime_n(buf.data(), 16,
                                                       "{}", "héllo");
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 6);
        CHECK(r.has_value() && std::string_view{buf.data(), r.value()} == "héllo");
    }

    // Section 10 — embedded NUL byte round-trip.
    // (Uses std::string with explicit length because std::string_view
    // stops at the first NUL.)
    section("10.embedded-nul");
    {
        // "a\0b" — 3 bytes (a, NUL, b).  std::string preserves the
        // NUL; std::string_view does not.  The consteval ctor of
        // std::format_string<Args...> takes a string-like type so
        // std::string works.
        std::string with_nul{'a', '\0', 'b'};
        auto buf = poison_buf(16);
        auto r = petra::try_format_bounded_runtime_n(buf.data(), 16,
                                                       "{}", with_nul);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 3);
        CHECK(r.has_value() && buf[0] == 'a');
        CHECK(r.has_value() && buf[1] == '\0');
        CHECK(r.has_value() && buf[2] == 'b');
    }

    // Section 11 — exact byte-count invariant (pass 2 written ==
    // pass 1 measured).
    section("11.byte-count-invariant");
    {
        auto buf = poison_buf(32);
        // ("hello {}", "world") — 11 bytes total.
        std::string world{"world"};
        auto r = petra::try_format_bounded_runtime_n(buf.data(), 32,
                                                       "hello {}", world);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 11);

        // Independent measurement probe: same format + args via
        // std::vformat_to + counting_output_iterator reports the
        // same byte count.
        std::size_t measured = 0;
        auto cb = [](std::size_t n, void* data) noexcept {
            *static_cast<std::size_t*>(data) = n;
        };
        petra::counting_output_iterator probe{cb, &measured};
        (void)std::vformat_to(std::move(probe), "hello {}",
                              std::make_format_args(world));
        CHECK(measured == 11);
        // The byte-by-byte invariant: pass 2 writes == pass 1
        // measures (11 == 11).
        CHECK(r.has_value() && r.value() == measured);
    }

    // Section 12 — TooLarge boundary sweep.
    section("12.too-large-boundary");
    {
        // "v=42" needs 4 bytes.  cap=3, 4, 5 → TooLarge, success,
        // success.  Verify the buffer is UNTOUCHED on TooLarge and
        // populated on success.
        {
            auto buf = poison_buf(4);
            auto r = petra::try_format_bounded_runtime_n(buf.data(), 3,
                                                           "v={}", 42);
            CHECK(!r.has_value());
            CHECK(buf[0] == 'Q');
            CHECK(buf[3] == 'Q');
        }
        {
            // cap == needed → success.
            auto buf = poison_buf(4);
            auto r = petra::try_format_bounded_runtime_n(buf.data(), 4,
                                                           "v={}", 42);
            CHECK(r.has_value());
            CHECK(r.has_value() && r.value() == 4);
            CHECK(std::string_view{buf.data(), 4} == "v=42");
        }
        {
            // cap > needed → success, bytes past written untouched.
            auto buf = poison_buf(8);
            auto r = petra::try_format_bounded_runtime_n(buf.data(), 8,
                                                           "v={}", 42);
            CHECK(r.has_value());
            CHECK(r.has_value() && r.value() == 4);
            CHECK(std::string_view{buf.data(), 4} == "v=42");
            CHECK(buf[4] == 'Q');  // poison byte preserved.
            CHECK(buf[7] == 'Q');
        }
    }

    // Section 13 — FormatError equality / inequality.
    section("13.format-error-equality");
    {
        auto e1 = petra::FormatError{petra::FormatErrorKind::TooLarge,
                                       "msg1", 4};
        auto e2 = petra::FormatError{petra::FormatErrorKind::TooLarge,
                                       "msg1", 4};
        auto e3 = petra::FormatError{petra::FormatErrorKind::TooLarge,
                                       "msg1", 5};
        auto e4 = petra::FormatError{petra::FormatErrorKind::Format,
                                       "msg1"};
        CHECK(e1 == e2);
        CHECK(e1 != e3);
        CHECK(e1 != e4);
    }

    // Section 14 — 50-round determinism on the size measurement.
    section("14.determinism");
    {
        std::size_t first = 0;
        bool all_equal = true;
        for (int i = 0; i < 50; ++i) {
            auto buf = poison_buf(64);
            auto r = petra::try_format_bounded_runtime_n(buf.data(), 64,
                                                           "i={}", i);
            if (!r.has_value()) { all_equal = false; break; }
            if (i == 0) first = r.value();
            // The expected size for "i=" + std::to_string(i).size() bytes.
            std::size_t expected = 2 + std::to_string(i).size();
            if (r.value() != expected) { all_equal = false; break; }
            // The byte content matches std::to_string(i).
            if (std::string_view{buf.data(), r.value()} !=
                ("i=" + std::to_string(i))) {
                all_equal = false; break;
            }
        }
        CHECK(all_equal);
        CHECK(first == 3);  // "i=0" is 3 bytes.
    }

    // Section 15 — 4 threads x 25 calls each (each thread has its own
    // char[32] buffer; the wrapper is thread-safe as long as the
    // output buffer is not shared).
    section("15.threads");
    {
        std::atomic<int> mismatches{0};
        auto worker = [&](int tid) {
            for (int i = 0; i < 25; ++i) {
                char buf[32];
                for (int j = 0; j < 32; ++j) buf[j] = 'Q';
                auto r = petra::try_format_bounded_runtime_n(buf, 32,
                                                               "t={} i={} v={}",
                                                               tid, i, i * 7);
                if (!r.has_value()) {
                    ++mismatches;
                    continue;
                }
                std::string expected =
                    "t=" + std::to_string(tid)
                    + " i=" + std::to_string(i)
                    + " v=" + std::to_string(i * 7);
                if (std::string_view{buf, r.value()} != expected) {
                    ++mismatches;
                }
                if (r.value() != expected.size()) {
                    ++mismatches;
                }
            }
        };
        std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back(worker, t);
        }
        for (auto& th : threads) th.join();
        CHECK(mismatches.load() == 0);
    }

    // Section 16 — distinct from Sep 8's try_format_runtime_n
    // (consteval fmt vs runtime fmt; byte-count and error contract
    // agree).
    section("16.distinct-from-sep-8");
    {
        // Same fmt + args, same buffer size — both wrappers produce
        // identical byte counts and identical structured-error
        // contracts.  Today's wrapper takes the fmt as a consteval
        // std::format_string<Args...>; Sep 8's takes a runtime
        // std::string_view.  At the call site they look the same
        // (string literals implicitly convert), but the consteval
        // check has already passed for today's wrapper by the time
        // the runtime body runs.
        auto buf = poison_buf(8);
        auto r_today = petra::try_format_bounded_runtime_n(buf.data(), 8,
                                                             "v={}", 42);
        CHECK(r_today.has_value() && r_today.value() == 4);
        CHECK(r_today.has_value() && std::string_view{buf.data(), r_today.value()} == "v=42");

        // On TooLarge: today's wrapper returns the same
        // FormatError{kind=TooLarge, needed_bytes=N} that Sep 8's
        // returns (the structured-error contract is identical).
        auto buf2 = poison_buf(8);
        auto r_today_tl = petra::try_format_bounded_runtime_n(buf2.data(), 3,
                                                                 "v={}", 42);
        CHECK(!r_today_tl.has_value());
        CHECK(!r_today_tl.has_value()
              && r_today_tl.error().kind == petra::FormatErrorKind::TooLarge);
        CHECK(!r_today_tl.has_value()
              && r_today_tl.error().needed_bytes == 4);
    }

    // Section 17 — bounded_char_writer never writes past the cap (5
    // writes into a 4-byte buffer; the 5th is dropped silently).
    section("17.bounded-writer-overflow");
    {
        // Directly-construct a bounded_char_writer with cap=4 and
        // exercise the overflow path.  This is the internal
        // verification Sep 8 documented — today's wrapper doesn't
        // reach this path through the public surface (the
        // TooLarge early-return prevents it), but the writer's
        // behavior is part of the contract.
        char buf[4] = {'a', 'b', 'c', 'd'};  // 4 bytes already populated.
        std::size_t written = 0;
        auto cb = [](std::size_t n, void* data) noexcept {
            *static_cast<std::size_t*>(data) = n;
        };
        petra::detail::bounded_char_writer sink{buf, 4, cb, &written};

        // Five writes: only the first four should land.
        for (int i = 0; i < 5; ++i) {
            *sink = static_cast<char>('a' + i);
        }
        CHECK(written == 4);
        CHECK(buf[0] == 'a');
        CHECK(buf[1] == 'b');
        CHECK(buf[2] == 'c');
        CHECK(buf[3] == 'd');
        // The 5th write was dropped — no byte was assigned past cap.
        // (We can verify this by checking that the buffer still has
        // only 4 bytes populated with 'a'..'d'.)
    }

    std::println("");
    std::println("== {} PASS, {} FAIL ==", g_pass, g_fail);
    std::println("");

    return g_fail == 0 ? 0 : 1;
}
