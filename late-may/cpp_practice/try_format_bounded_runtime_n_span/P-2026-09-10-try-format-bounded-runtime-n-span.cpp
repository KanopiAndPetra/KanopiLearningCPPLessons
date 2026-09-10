// P-2026-09-10 — try_format_bounded_runtime_n_span: STDSPAN sibling of
// P-2026-09-09's try_format_bounded_runtime_n.  Closes Sep 9's "Where we
// go next" item #3 — the std::span<char> overload of Sep 9's wrapper,
// analogous to Aug 25's format_to_n_into(std::span<char>, ...) span
// overload.  Aug 25's lesson is the NUL-terminating sibling; today is
// the raw (no-NUL-terminating) sibling.
//
// The shape:
//
//   petra::try_format_bounded_runtime_n<Args...>(std::span<char> out,
//                                                std::format_string<Args...> fmt,
//                                                Args&&... args)
//       -> std::expected<std::size_t, petra::FormatError>
//
// On success: returns expected<size_t> = bytes actually written into
// out[0, out.size()).  Like Sep 9, does NOT NUL-terminate (matches
// std::format_to_n's raw contract).  Bytes past `written` are NOT
// touched (the bounded writer stops at the cap — proven in Sep 9's
// Section 17 and re-pinned here in Section 5).
//
// On TooLarge: returns unexpected{FormatError{TooLarge, ..., needed}}.
// On Format (std::format_error from std::make_format_args): returns
// unexpected{FormatError{Format, e.what()}}.
// In both cases `out` is UNTOUCHED.
//
// The span overload is a THIN FORWARDING wrapper around Sep 9's
// `char* + std::size_t` implementation.  Sep 9's "where we go next"
// item #3 explicitly noted: "Today's implementation is already
// span-compatible (it just needs a forwarding wrapper that extracts
// `.data()` and `.size()`)." — today lands it.
//
// How today's lesson relates to Sep 9:
//
//   Sep 9 (char*+size_t, consteval fmt)  → TODAY (span<char>, consteval fmt)
//   This is the natural broadening to the std::span<char> surface —
//   the same way Aug 25 broadened its raw-bounded-buffer wrapper to a
//   std::span<char> overload for the "I want a span, not a
//   pointer+size" use case.
//
// What gets reused:
//   - petra::FormatError{kind, message, needed_bytes} — verbatim from
//     Sep 6 / Sep 7 / Sep 8 / Sep 9.
//   - petra::FormatErrorKind {FormatParse, FormatArgument, Format,
//     TooLarge} — verbatim.
//   - petra::counting_output_iterator — verbatim from Sep 6 / Sep 7 /
//     Sep 8 / Sep 9.  (We are still on libc++ 21 which doesn't ship
//     std::counting_output_iterator; Sep 9's static_assert is the
//     migration trigger when a future libc++ does ship it.)
//   - petra::detail::bounded_char_writer — verbatim from Sep 8 / Sep 9.
//   - petra::detail::try_format_bounded_runtime_n_impl — verbatim from
//     Sep 9.  Today's span overload is a thin shim that extracts
//     `.data()` + `.size()` and forwards.
//
// What is NEW:
//   - petra::try_format_bounded_runtime_n<Args...>(std::span<char>,
//     std::format_string<Args...>, Args&&...) — the SPAN primary
//     overload.  Lives in the same `petra` namespace; no new types.
//   - One additional acceptance probe: std::span<char> must be
//     trivially copyable, must have `.data()` + `.size()`, must
//     satisfy the contiguous_range / sized_range /
//     std::ranges::contiguous_range concept.  Section 1 pins these.
//
// C++23, std::span<char> (P0122R7), std::format, std::vformat_to,
// std::format_string<Args...> (P2216R3), std::make_format_args,
// std::format_error, std::expected (P0323R12), std::println
// (P2093R14), std::ranges::contiguous_range.  Requires
// -fexperimental-library on Apple Clang 21.0.0 / libc++ 21 for
// std::println.
//
// Build:
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
//           P-2026-09-10-try-format-bounded-runtime-n-span.cpp \
//           -o /tmp/P-2026-09-10-try-format-bounded-runtime-n-span
//
// Strict:
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
//           -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion \
//           -Wsign-conversion \
//           P-2026-09-10-try-format-bounded-runtime-n-span.cpp \
//           -o /tmp/P-2026-09-10-try-format-bounded-runtime-n-span-strict
//
// ASan + UBSan:
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
//           -fsanitize=address -fsanitize=undefined \
//           -fno-omit-frame-pointer -O1 \
//           P-2026-09-10-try-format-bounded-runtime-n-span.cpp \
//           -o /tmp/P-2026-09-10-try-format-bounded-runtime-n-span-asan

#include <cstddef>
#include <cstring>
#include <expected>
#include <format>
#include <iterator>
#include <memory>
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace petra {

// ----- FormatError (reused verbatim from Sep 6 / Sep 7 / Sep 8 / Sep 9) ---
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
    TooLarge,        // raised by the bounded wrappers (Sep 6 / Sep 7 /
                     // Sep 8 / Sep 9 / today) when the measure pass
                     // reports more bytes than the caller cap.  Carries
                     // the would-be byte count in FormatError::needed_bytes.
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

// ----- counting_output_iterator (reused verbatim from Sep 6..9) -----------
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

// ----- bounded_char_writer (reused verbatim from Sep 8 / Sep 9) ------------
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

// ----- try_format_bounded_runtime_n_impl (reused verbatim from Sep 9) ----
//
// Two-pass measure-then-write pair.  Verbatim from Sep 9 — only the
// public surface changes today (the char*-and-cap shape becomes a
// std::span<char> primary overload that forwards to this body).
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

    // Pass 2: write into out[0, cap).
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

// ----- public surface: char* + cap (verbatim from Sep 9) ------------------
//
// Sep 9's primary surface — kept as a forwarding shim so the existing
// Sep 9 lesson and its tests continue to work when today's header is
// in scope.  The body identical to Sep 9's.

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n(char* out, std::size_t cap,
                              std::format_string<Args...> fmt,
                              const Args&... args) {
    return detail::try_format_bounded_runtime_n_impl(out, cap, fmt, args...);
}

// ----- public surface: std::span<char> (NEW — today's header) -------------
//
// The SPAN primary overload.  Mirrors std::format_to_n_into's
// std::span<char> overload from Aug 25 — but Aug 25's wrapper NUL-
// terminates, today's wrapper does NOT (raw std::format_to_n shape,
// like Sep 9's char*-and-cap form).  Extracts .data() + .size() and
// forwards to the existing impl — the impl is span-compatible as
// Sep 9's "where we go next" item #3 noted.
//
// The use case: callers who already hold a std::span<char> (e.g.
// from a `std::vector<char>::data()` with an explicit length, a
// fixed-size struct member, or a parsed wire buffer) can pass the
// span directly without having to unpack its .data() + .size()
// components at every call site.

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n(std::span<char> out,
                              std::format_string<Args...> fmt,
                              const Args&... args) {
    // .data() returns the same pointer the caller holds; .size() is
    // the cap.  If out is empty, .data() may return nullptr (the
    // std::span<char> contract: `data()` is required to return a
    // valid pointer only when size() > 0 on libc++ 21 — for size()
    // == 0 the returned pointer is unspecified).  We forward to
    // the impl which early-returns on cap == 0 / "v={}" with cap=0
    // / etc.  When size() == 0 the impl returns success with 0
    // bytes written; when size() > 0 the impl writes through
    // .data() into the caller's buffer.
    return detail::try_format_bounded_runtime_n_impl(out.data(), out.size(),
                                                      fmt, args...);
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
// out[written..n) is left untouched on the success path).
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
        // Span overload return type matches the char*+cap sibling.
        CHECK((std::is_same_v<
              decltype(petra::try_format_bounded_runtime_n(
                  std::span<char>{}, std::string_view{})),
              std::expected<std::size_t, petra::FormatError>>));
        // char*-and-cap overload return type is unchanged from Sep 9.
        CHECK((std::is_same_v<
              decltype(petra::try_format_bounded_runtime_n(
                  static_cast<char*>(nullptr), std::size_t{0},
                  std::string_view{})),
              std::expected<std::size_t, petra::FormatError>>));
        // The consteval ctor: compile-time sizeof the type.
        // The consteval ctor: compile-time sizeof the type.
        CHECK((std::is_same_v<decltype(std::format_string<int>{"value={}"}),
                              std::format_string<int>>));
        // FormatErrorKind enum values compare unequal and have stable
        // underlying values (0, 1, 2, 3) — same values Sep 6/7/8/9
        // pin.
        CHECK(petra::FormatErrorKind::FormatParse
              != petra::FormatErrorKind::FormatArgument);
        CHECK(petra::FormatErrorKind::TooLarge != petra::FormatErrorKind::Format);
        CHECK(static_cast<unsigned char>(petra::FormatErrorKind::FormatParse) == 0);
        CHECK(static_cast<unsigned char>(petra::FormatErrorKind::FormatArgument) == 1);
        CHECK(static_cast<unsigned char>(petra::FormatErrorKind::Format) == 2);
        CHECK(static_cast<unsigned char>(petra::FormatErrorKind::TooLarge) == 3);
        // std::span<char> must be a contiguous range whose iterators
        // are random-access, and the type must be trivially copyable
        // (it's a view over a contiguous buffer — no ownership).
        CHECK((std::ranges::contiguous_range<std::span<char>>));
        CHECK(std::ranges::sized_range<std::span<char>>);
        CHECK(std::is_trivially_copyable_v<std::span<char>>);
        CHECK(std::is_standard_layout_v<std::span<char>>);
        CHECK(std::is_same_v<decltype(std::declval<std::span<char>>().data()), char*>);
        CHECK(std::is_same_v<decltype(std::declval<std::span<char>>().size()),
                              std::size_t>);
    }

    // Section 2 — fits-exactly happy path (mirrors Sep 9 Section 2).
    section("2.fits-exactly");
    {
        // cap == needed: 4 bytes for "v=42".
        auto buf = poison_buf(4);
        std::span<char> sp{buf.data(), 4};
        auto r = petra::try_format_bounded_runtime_n(sp, "v={}", 42);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 4);
        CHECK(r.has_value() && std::string_view{sp.data(), 4} == "v=42");
    }
    {
        // cap == needed: 7 bytes for "d=3.142".
        auto buf = poison_buf(7);
        std::span<char> sp{buf.data(), 7};
        auto r = petra::try_format_bounded_runtime_n(sp, "d={:.3f}", 3.14159);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 7);
        CHECK(r.has_value() && std::string_view{sp.data(), 7} == "d=3.142");
    }
    {
        // No replacement fields — literal "ABCDE" (5 bytes) fits
        // exactly into a 5-byte buffer.
        auto buf = poison_buf(5);
        std::span<char> sp{buf.data(), 5};
        auto r = petra::try_format_bounded_runtime_n(sp, "ABCDE");
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 5);
        CHECK(r.has_value() && std::string_view{sp.data(), 5} == "ABCDE");
    }

    // Section 3 — cap larger than needed (no truncation).
    section("3.span-larger-than-needed");
    {
        // cap=10 but "v=42" needs 4 bytes.
        auto buf = poison_buf(10);
        std::span<char> sp{buf.data(), 10};
        auto r = petra::try_format_bounded_runtime_n(sp, "v={}", 42);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 4);
        CHECK(r.has_value() && std::string_view{sp.data(), 4} == "v=42");
        // Bytes past written are still the poison byte 'Q' (the
        // bounded writer stops at the cap and does NOT NUL-terminate
        // — distinguishing today from Aug 25's format_to_n_into which
        // DOES NUL-terminate).
        CHECK(sp[4] == 'Q');
        CHECK(sp[9] == 'Q');
    }

    // Section 4 — TooLarge: cap smaller than needed.
    section("4.too-large");
    {
        // "v=42" needs 4 bytes — cap=3 → TooLarge.
        auto buf = poison_buf(4);
        std::span<char> sp{buf.data(), 3};
        auto r = petra::try_format_bounded_runtime_n(sp, "v={}", 42);
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
    }
    {
        // "d=3.142" needs 7 bytes — cap=6 → TooLarge.
        auto buf = poison_buf(7);
        std::span<char> sp{buf.data(), 6};
        auto r = petra::try_format_bounded_runtime_n(sp, "d={:.3f}", 3.14159);
        CHECK(!r.has_value());
        CHECK(!r.has_value() && r.error().needed_bytes == 7);
        // Buffer is UNTOUCHED.
        CHECK(buf[0] == 'Q');
        CHECK(buf[6] == 'Q');
    }
    {
        // Empty span (size=0) with "" → success (zero bytes fit).
        auto buf = poison_buf(1);
        std::span<char> sp{buf.data(), 0};
        auto r = petra::try_format_bounded_runtime_n(sp, "");
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 0);
        // Buffer is UNTOUCHED on success-with-written==0.
        CHECK(buf[0] == 'Q');
    }
    {
        // Empty span (size=0) with "x" → TooLarge (1 byte needed).
        auto buf = poison_buf(1);
        std::span<char> sp{buf.data(), 0};
        auto r = petra::try_format_bounded_runtime_n(sp, "x");
        CHECK(!r.has_value());
        CHECK(!r.has_value() && r.error().needed_bytes == 1);
        // Buffer is UNTOUCHED.
        CHECK(buf[0] == 'Q');
    }

    // Section 5 — consteval fmt positive.
    section("5.consteval-fmt-positive");
    {
        // Valid format strings produce identical results to
        // std::format.  All four are std::format_string<Args...>-typed
        // (the consteval check has already passed by the time we
        // reach the runtime body).
        auto buf1 = poison_buf(100);
        std::span<char> sp1{buf1.data(), 100};
        auto r1 = petra::try_format_bounded_runtime_n(sp1, "value={}", 42);
        CHECK(r1.has_value() && r1.value() == 8);
        CHECK(r1.has_value() && std::string_view{sp1.data(), r1.value()} == "value=42");

        auto buf2 = poison_buf(100);
        std::span<char> sp2{buf2.data(), 100};
        auto r2 = petra::try_format_bounded_runtime_n(sp2, "hex={:#x}", 0xcafe);
        CHECK(r2.has_value() && r2.value() == 10);
        CHECK(r2.has_value() && std::string_view{sp2.data(), r2.value()} == "hex=0xcafe");

        auto buf3 = poison_buf(100);
        std::span<char> sp3{buf3.data(), 100};
        auto r3 = petra::try_format_bounded_runtime_n(sp3, "{:.3f}", 3.14159);
        CHECK(r3.has_value() && r3.value() == 5);
        CHECK(r3.has_value() && std::string_view{sp3.data(), r3.value()} == "3.142");

        auto buf4 = poison_buf(100);
        std::span<char> sp4{buf4.data(), 100};
        auto r4 = petra::try_format_bounded_runtime_n(sp4, "{}", 3.14159);
        CHECK(r4.has_value() && r4.value() == 7);
        CHECK(r4.has_value() && std::string_view{sp4.data(), r4.value()} == "3.14159");
    }

    // Section 6 — atomicity (failing call leaves prior success untouched).
    section("6.atomicity");
    {
        // Fill buffer with the previous success ("v=42").
        char buf[16];
        std::memcpy(buf, "v=42", 4);
        for (int i = 4; i < 16; ++i) buf[i] = 'Q';
        std::span<char> sp{buf, 16};

        // Failing call: cap=0, fmt "longer" → TooLarge.
        std::span<char> empty_sp{buf, 0};
        auto r = petra::try_format_bounded_runtime_n(empty_sp, "longer");
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
        std::span<char> sp{buf.data(), 100};
        auto r = petra::try_format_bounded_runtime_n(
            sp,
            "name={} age={:#x} pi={:.5f} pad={:>10}",
            std::string{"alice"}, 42, 3.14159265, std::string{"hi"});
        CHECK(r.has_value());
        // "name=alice age=0x2a pi=3.14159 pad=        hi" — 45 bytes.
        CHECK(r.has_value() && r.value() == 45);
        CHECK(r.has_value() && std::string_view{sp.data(), r.value()} ==
              "name=alice age=0x2a pi=3.14159 pad=        hi");
    }

    // Section 8 — UTF-8 mid-codepoint byte round-trip.
    section("8.utf8-byte-roundtrip");
    {
        // "héllo" in UTF-8: 'h'=0x68, 'é'=0xc3 0xa9, 'l'=0x6c,
        // 'l'=0x6c, 'o'=0x6f.  Total 6 bytes.  Use a literal to keep
        // the byte count deterministic.
        auto buf = poison_buf(16);
        std::span<char> sp{buf.data(), 16};
        auto r = petra::try_format_bounded_runtime_n(sp, "{}", "héllo");
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 6);
        CHECK(r.has_value() && std::string_view{sp.data(), r.value()} == "héllo");
    }

    // Section 9 — embedded NUL byte round-trip.
    section("9.embedded-nul");
    {
        // "a\0b" — 3 bytes.  std::string preserves the NUL.
        std::string with_nul{'a', '\0', 'b'};
        auto buf = poison_buf(16);
        std::span<char> sp{buf.data(), 16};
        auto r = petra::try_format_bounded_runtime_n(sp, "{}", with_nul);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 3);
        CHECK(r.has_value() && sp[0] == 'a');
        CHECK(r.has_value() && sp[1] == '\0');
        CHECK(r.has_value() && sp[2] == 'b');
    }

    // Section 10 — exact byte-count invariant (pass 2 written == pass 1
    // measured).
    section("10.byte-count-invariant");
    {
        auto buf = poison_buf(32);
        std::span<char> sp{buf.data(), 32};
        // ("hello {}", "world") — 11 bytes total.
        std::string world{"world"};
        auto r = petra::try_format_bounded_runtime_n(sp, "hello {}", world);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 11);

        // Independent measurement probe: same format + args via
        // std::vformat_to + counting_output_iterator reports the
        // same byte count (same invariant Sep 6..9 pin).
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

    // Section 11 — TooLarge boundary sweep on the span surface.
    section("11.too-large-boundary");
    {
        // "v=42" needs 4 bytes.  span size 3, 4, 5 → TooLarge,
        // success, success.
        {
            auto buf = poison_buf(4);
            std::span<char> sp{buf.data(), 3};
            auto r = petra::try_format_bounded_runtime_n(sp, "v={}", 42);
            CHECK(!r.has_value());
            CHECK(buf[0] == 'Q');
            CHECK(buf[3] == 'Q');
        }
        {
            // span size == needed → success.
            auto buf = poison_buf(4);
            std::span<char> sp{buf.data(), 4};
            auto r = petra::try_format_bounded_runtime_n(sp, "v={}", 42);
            CHECK(r.has_value());
            CHECK(r.has_value() && r.value() == 4);
            CHECK(std::string_view{sp.data(), 4} == "v=42");
        }
        {
            // span size > needed → success, bytes past written
            // untouched.
            auto buf = poison_buf(8);
            std::span<char> sp{buf.data(), 8};
            auto r = petra::try_format_bounded_runtime_n(sp, "v={}", 42);
            CHECK(r.has_value());
            CHECK(r.has_value() && r.value() == 4);
            CHECK(std::string_view{sp.data(), 4} == "v=42");
            CHECK(buf[4] == 'Q');  // poison byte preserved.
            CHECK(buf[7] == 'Q');
        }
    }

    // Section 12 — sub-span pass: a span over a SUB-range of the
    // underlying buffer must NOT touch the parts of the buffer
    // outside the span.
    section("12.subspan-no-bleed");
    {
        // Build a 16-byte buffer; the wrapper sees only a 6-byte
        // sub-span starting at offset 5 ("ABCDE" needs 5 bytes; cap
        // = 6 leaves 1 spare).  Bytes outside [5, 11) must stay the
        // poison byte.
        auto buf = poison_buf(16);
        std::span<char> sub{buf.data() + 5, 6};
        auto r = petra::try_format_bounded_runtime_n(sub, "ABCDE");
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 5);
        // Bytes 0..4 untouched.
        CHECK(buf[0] == 'Q');
        CHECK(buf[4] == 'Q');
        // Bytes 5..9 = "ABCDE".
        CHECK(buf[5] == 'A');
        CHECK(buf[6] == 'B');
        CHECK(buf[7] == 'C');
        CHECK(buf[8] == 'D');
        CHECK(buf[9] == 'E');
        // Byte 10 (sub.span's last) untouched.
        CHECK(buf[10] == 'Q');
        // Bytes 11..15 untouched.
        CHECK(buf[11] == 'Q');
        CHECK(buf[15] == 'Q');
    }

    // Section 13 — char*+cap and span<char> overloads produce
    // identical results for identical buffer + format + args.
    section("13.char*-and-cap-and-span-agree");
    {
        // Build two 8-byte buffers with the same poison byte.
        auto buf_a = poison_buf(8);
        auto buf_b = poison_buf(8);

        // The char*-and-cap overload writes through buf_a.data().
        auto r_a = petra::try_format_bounded_runtime_n(buf_a.data(), 8,
                                                        "v={}", 42);
        // The span overload writes through std::span<char> of
        // buf_b — same underlying buffer shape.
        std::span<char> sp{buf_b.data(), 8};
        auto r_b = petra::try_format_bounded_runtime_n(sp, "v={}", 42);

        CHECK(r_a.has_value());
        CHECK(r_b.has_value());
        CHECK(r_a.value() == r_b.value());
        CHECK(r_a.value() == 4);
        CHECK(std::string_view{buf_a.data(), 4}
              == std::string_view{buf_b.data(), 4});
        CHECK(std::string_view{buf_a.data(), 4} == "v=42");
        // Bytes past written are poison in both.
        CHECK(buf_a[4] == 'Q');
        CHECK(buf_b[4] == 'Q');
    }

    // Section 14 — 50-round determinism on the size measurement.
    section("14.determinism");
    {
        std::size_t first = 0;
        bool all_equal = true;
        for (int i = 0; i < 50; ++i) {
            auto buf = poison_buf(64);
            std::span<char> sp{buf.data(), 64};
            auto r = petra::try_format_bounded_runtime_n(sp, "i={}", i);
            if (!r.has_value()) { all_equal = false; break; }
            if (i == 0) first = r.value();
            // The expected size for "i=" + std::to_string(i).size() bytes.
            std::size_t expected = 2 + std::to_string(i).size();
            if (r.value() != expected) { all_equal = false; break; }
            // The byte content matches std::to_string(i).
            if (std::string_view{sp.data(), r.value()} !=
                ("i=" + std::to_string(i))) {
                all_equal = false; break;
            }
        }
        CHECK(all_equal);
        CHECK(first == 3);  // "i=0" is 3 bytes.
    }

    // Section 15 — 4 threads x 25 calls each (each thread has its own
    // char[32] buffer; each call constructs its own std::span<char>
    // over that buffer; the wrapper is thread-safe as long as the
    // output buffer is not shared).
    section("15.threads");
    {
        std::atomic<int> mismatches{0};
        auto worker = [&](int tid) {
            for (int i = 0; i < 25; ++i) {
                char buf[32];
                for (int j = 0; j < 32; ++j) buf[j] = 'Q';
                std::span<char> sp{buf, 32};
                auto r = petra::try_format_bounded_runtime_n(
                    sp, "t={} i={} v={}", tid, i, i * 7);
                if (!r.has_value()) {
                    ++mismatches;
                    continue;
                }
                std::string expected =
                    "t=" + std::to_string(tid)
                    + " i=" + std::to_string(i)
                    + " v=" + std::to_string(i * 7);
                if (std::string_view{sp.data(), r.value()} != expected) {
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

    // Section 16 — distinct from Aug 25's format_to_n_into span
    // overload: today's wrapper does NOT NUL-terminate; Aug 25's
    // DOES.
    section("16.distinct-from-aug-25-nul-terminating");
    {
        // Aug 25's format_to_n_into NUL-terminates at out[r.value()]
        // (for r.value() < n) or out[n-1] (for r.value() >= n).
        // Today's wrapper does NOT — bytes past `written` are left
        // untouched.  We verify by inspecting byte out[r.value()]
        // and the byte just past it — those would be 0 (\0) if
        // NUL-terminating, and 'Q' (poison) if raw.
        //
        // ("ABCDE", cap=10) — r.value() == 5; Aug 25 would write
        // \0 at out[5]; today leaves out[5] == 'Q'.
        auto buf = poison_buf(10);
        std::span<char> sp{buf.data(), 10};
        auto r = petra::try_format_bounded_runtime_n(sp, "ABCDE");
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == 5);
        // Bytes 0..4 are "ABCDE".
        CHECK(sp[0] == 'A');
        CHECK(sp[4] == 'E');
        // Byte 5 is NOT \0 — it's the poison byte 'Q'.  This is
        // the raw std::format_to_n shape.
        CHECK(sp[5] == 'Q');
        CHECK(sp[9] == 'Q');
        // Bytes 6..9 untouched.
    }

    // Section 17 — bounded writer still drops overflow silently on
    // the span path.  When the cap is hit mid-write, additional
    // writes are silently dropped (matches std::format_to_n's n
    // cap).
    section("17.bounded-writer-overflow");
    {
        // Build a 4-byte span; format into it a string that exceeds
        // 4 bytes — the wrapper reports would-be size and returns
        // TooLarge (so the writer is NOT invoked on the public
        // surface).  Instead exercise the writer directly here.
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
    }

    std::println("");
    std::println("== {} PASS, {} FAIL ==", g_pass, g_fail);
    std::println("");

    return g_fail == 0 ? 0 : 1;
}
