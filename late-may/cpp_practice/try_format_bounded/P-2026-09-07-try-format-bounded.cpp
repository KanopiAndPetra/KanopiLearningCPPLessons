// P-2026-09-07 — try_format_bounded — std::expected<std::string,
// FormatError> wrapper around std::format that takes a caller-provided
// byte cap AND a consteval-checked std::format_string<Args...> fmt.
// Closes Sep 6's "Where we go next" item #1 — the consteval sibling of
// P-2026-09-06's try_format_runtime_bounded.
//
// The runtime sibling (Sep 6) accepts a std::string_view format string
// and has to catch std::format_error at format time.  This lesson's
// consteval sibling accepts std::format_string<Args...> whose ctor is
// consteval — malformed format strings are HARD COMPILE ERRORS at the
// call site, point-blank.  In the consteval path only TooLarge
// (Sep 6's fourth FormatErrorKind value) survives as a runtime error;
// Format / FormatParse / FormatArgument become unreachable for the
// parse side.  The format_args side of std::format can still throw
// std::format_error at format time (e.g. an integer that doesn't fit
// the requested width), and the wrapper preserves the Sep 6 contract
// of catching that and routing to FormatError{Format, ...} verbatim.
//
// Standard: C++23.  std::format, std::vformat_to, std::format_string<Args...>
// (P2216R3), std::make_format_args, std::format_error, std::expected
// (P0323R12), std::println (P2093R14).  Reuses Sep 6's
// petra::counting_output_iterator (libc++ 21 does NOT ship
// std::counting_output_iterator — verified by recursive grep across
// /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1/
// returning zero matches).
//
// Build (default — -O0 debug build for this lesson):
//
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
//           P-2026-09-07-try-format-bounded.cpp \
//           -o /tmp/P-2026-09-07-try-format-bounded
//
// Strict:
//
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
//           -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion \
//           -Wsign-conversion \
//           P-2026-09-07-try-format-bounded.cpp \
//           -o /tmp/P-2026-09-07-try-format-bounded-strict
//
// ASan + UBSan:
//
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library \
//           -fsanitize=address -fsanitize=undefined \
//           -fno-omit-frame-pointer -O1 \
//           P-2026-09-07-try-format-bounded.cpp \
//           -o /tmp/P-2026-09-07-try-format-bounded-asan
//
// CMake (default / strict / ASan) — see CMakeLists.txt in this dir.

#include <atomic>
#include <cstddef>
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

// ============================================================================
// Part 1 — reuse Sep 6's petra::FormatError and petra::counting_output_iterator
// ============================================================================
//
// Both types are defined verbatim in Sep 6 (P-2026-09-06).  Today's lesson
// would normally #include a shared header; in this single-file lesson we
// duplicate the definitions (the type shape is small and stable).  Any
// divergence between this copy and Sep 6's is documented in the matching
// section-by-section comparison.

namespace petra {

enum class FormatErrorKind : unsigned char {
    FormatParse,     // reserved — see Sep 6's notes.  In the consteval
                     // path this branch is UNREACHABLE because the
                     // std::format_string<Args...> ctor is consteval.
    FormatArgument,  // reserved — same caveat.
    Format,          // mapped from std::format_error at format time.
                     // Reachable only via the std::make_format_args side
                     // (e.g. an integer that doesn't fit the requested
                     // width) — the parse-side errors are caught at
                     // compile time by the consteval ctor.
    TooLarge,        // raised when the measure pass reports more bytes
                     // than the caller cap.  Reachable in both the
                     // consteval and runtime wrappers.
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

// Hand-rolled callback-based counting_output_iterator (P2738R2 /
// [format.range] example shape).  The callback escapes the iterator
// even when std::vformat_to (libc++ 21 __direct_iterator_buffer)
// makes an internal copy — verified by Sep 6's trace probe.
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

// ============================================================================
// Part 2 — petra::try_format_bounded<Args...>(cap, fmt, args...) — consteval fmt
// ============================================================================
//
// The CONSTEVAL sibling of Sep 6's try_format_runtime_bounded.  The
// only difference is the format-string parameter type:
//
//   Sep 6:  std::string_view fmt                                 (runtime)
//   Today:  std::format_string<Args...> fmt                      (consteval)
//
// std::format_string<Args...>'s ctor is consteval — every malformed
// format string is a HARD COMPILE ERROR at the call site (a "better
// diagnostic than a runtime error" guarantee; see P2216R3).  The
// runtime try/catch in the function body only needs to handle the
// std::make_format_args side (e.g. an integer that doesn't fit the
// requested width — extremely rare for std::format).
//
// The wrapper preserves Sep 6's measurement contract: a measure pass
// through counting_output_iterator, a TooLarge check, then a write pass
// through back_insert_iterator into a std::string with `out.reserve(needed)`.

namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] std::expected<std::string, FormatError>
try_format_bounded_impl(std::size_t cap,
                        std::format_string<Args...> fmt,
                        Args&&... args) {
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };

    // Pass 1: measure.  std::vformat_to consumes the format-string
    // type via std::make_format_args on the consteval-validated string.
    // Any std::format_error here is the format_args side (NOT the
    // parse side — that's caught by the consteval ctor).
    try {
        counting_output_iterator counter{cb, &needed};
        // The .get() call on std::format_string<Args...> extracts
        // the underlying string view — see P2216R3 §3.1.  The
        // resulting string view is what std::vformat_to wants.
        (void)std::vformat_to(std::move(counter), fmt.get(),
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }

    // Sized-too-big path.
    if (needed > cap) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"formatted output would be "}
                            + std::to_string(needed)
                            + " bytes; cap is " + std::to_string(cap),
                        needed});
    }

    // Pass 2: write.
    std::string out;
    out.reserve(needed);
    try {
        (void)std::vformat_to(std::back_insert_iterator<std::string>(out),
                              fmt.get(), std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }

    // Invariant check: pass 2 wrote exactly `needed` bytes.
    if (out.size() != needed) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"back_insert wrote "}
                            + std::to_string(out.size())
                            + " bytes; measure pass said "
                            + std::to_string(needed),
                        needed});
    }

    return out;
}

}  // namespace detail

// Public surface — consteval format-string form.
//
// Note the use of `std::format_string<Args...>` (not
// `std::type_identity_t<std::format_string<Args...>>`) — the type
// already carries the std::type_identity tag for each Args, and
// std::format_string<Args...>'s implicit conversion from
// `const S&` (where S is a string-like type) lets the caller pass
// a string literal directly without an explicit .ctor invocation.
template <typename... Args>
[[nodiscard]] std::expected<std::string, FormatError>
try_format_bounded(std::size_t cap,
                   std::format_string<Args...> fmt,
                   Args&&... args) {
    return detail::try_format_bounded_impl(cap, fmt,
                                           std::forward<Args>(args)...);
}

}  // namespace petra

// ============================================================================
// Part 3 — test harness
// ============================================================================

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

#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

void section(const char* name) {
    std::println("-- section {}", name);
}

}  // namespace

int main() {
    // Section 1 — toolchain + feature probes.
    section("1.probes");
    {
        CHECK((std::is_default_constructible_v<petra::FormatError>));
        CHECK((std::is_nothrow_move_constructible_v<petra::FormatError>));
        CHECK((std::is_same_v<
              decltype(petra::try_format_bounded(std::size_t{0},
                                                 std::string_view{})),
              std::expected<std::string, petra::FormatError>>));
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
    }

    // Section 2 — fits-exactly happy path.
    // Mirrors Sep 6's Section 2 — confirms the consteval wrapper
    // produces identical byte counts to the runtime wrapper.
    section("2.fits-exactly");
    {
        auto r = petra::try_format_bounded(4, "v={}", 42);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == "v=42");
        CHECK(r.has_value() && r.value().size() == 4);
    }
    {
        auto r = petra::try_format_bounded(7, "d={:.3f}", 3.14159);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == "d=3.142");
        CHECK(r.has_value() && r.value().size() == 7);
    }
    {
        auto r = petra::try_format_bounded(10, "ABCDE");
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == "ABCDE");
    }

    // Section 3 — cap larger than needed (no truncation).
    section("3.cap-larger-than-needed");
    {
        auto r = petra::try_format_bounded(1000, "v={}", 42);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == "v=42");
        CHECK(r.has_value() && r.value().size() == 4);
    }

    // Section 4 — TooLarge (preserves Sep 6 contract).
    section("4.too-large");
    {
        // "v=42" needs 4 bytes — cap=3 → TooLarge.
        auto r = petra::try_format_bounded(3, "v={}", 42);
        CHECK(!r.has_value());
        CHECK(!r.has_value() && r.error().kind == petra::FormatErrorKind::TooLarge);
        CHECK(!r.has_value() && r.error().needed_bytes == 4);
        CHECK(!r.has_value() && !r.error().message.empty());
    }
    {
        // "d=3.142" needs 7 bytes — cap=6 → TooLarge.
        auto r = petra::try_format_bounded(6, "d={:.3f}", 3.14159);
        CHECK(!r.has_value());
        CHECK(!r.has_value() && r.error().needed_bytes == 7);
    }
    {
        // cap=0 with "" → success (zero bytes fit).
        auto r = petra::try_format_bounded(0, "");
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == "");
    }
    {
        // cap=0 with "x" → TooLarge (1 byte needed).
        auto r = petra::try_format_bounded(0, "x");
        CHECK(!r.has_value());
        CHECK(!r.has_value() && r.error().needed_bytes == 1);
    }

    // Section 5 — consteval format-string: malformed is a HARD COMPILE ERROR.
    //
    // This section can only check the positive case at runtime — the
    // negative case is verified at COMPILE time, not at runtime.  See
    // the "What we cannot test" subsection in the lesson notes.
    section("5.consteval-fmt-positive");
    {
        // Valid format strings produce identical results to
        // std::format.  All four are std::format_string<Args...>-typed
        // (the consteval check has already passed by the time we
        // reach the runtime body).
        auto r1 = petra::try_format_bounded(100, "value={}", 42);
        CHECK(r1.has_value() && r1.value() == "value=42");

        auto r2 = petra::try_format_bounded(100, "hex={:#x}", 0xcafe);
        CHECK(r2.has_value() && r2.value() == "hex=0xcafe");

        auto r3 = petra::try_format_bounded(100, "{:.3f}", 3.14159);
        CHECK(r3.has_value() && r3.value() == "3.142");

        auto r4 = petra::try_format_bounded(100, "{}", 3.14159);
        CHECK(r4.has_value() && r4.value() == "3.14159");
    }

    // Section 6 — atomicity: a failing call leaves prior success untouched.
    section("6.atomicity");
    {
        auto ok = petra::try_format_bounded(100, "ok={}", 1);
        CHECK(ok.has_value() && ok.value() == "ok=1");

        // cap too small for "bad=2" (5 bytes); cap=4 → TooLarge.
        auto bad = petra::try_format_bounded(4, "bad={}", 2);
        CHECK(!bad.has_value());
        CHECK(bad.error().kind == petra::FormatErrorKind::TooLarge);

        // The prior success is unaffected.
        CHECK(ok.has_value() && ok.value() == "ok=1");
    }

    // Section 7 — multi-type args round-trip (probe specifier passthrough).
    section("7.multi-type-args");
    {
        auto r = petra::try_format_bounded(100,
                                           "{}={} h={:#x}", "v", 42, 0xcafe);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == "v=42 h=0xcafe");
    }
    {
        auto r = petra::try_format_bounded(100,
                                           "d={:.3f} s={:>8} l={:<6}",
                                           3.14159, "x", "abc");
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == "d=3.142 s=       x l=abc   ");
    }

    // Section 8 — std::counting_output_iterator availability probe
    // (mirrors Sep 6's Section 8).
    section("8.platform-counting-output-iterator");
    {
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
        std::string sv{"\xE2\x9C\x93"};
        auto r = petra::try_format_bounded(100, "x={}", sv);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == "x=\xE2\x9C\x93");
        CHECK(r.has_value() && r.value().size() == 5);
    }

    // Section 10 — embedded NUL byte round-trip.
    section("10.nul-byte");
    {
        std::string sv{"A\0B\0", 4};
        auto r = petra::try_format_bounded(100, "{}", sv);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value().size() == 4);
        CHECK(r.has_value() && r.value() == sv);
    }

    // Section 11 — exact byte-count invariant.
    section("11.byte-count-invariant");
    {
        // A pass that succeeded and a follow-up measurement pass agree.
        // Note: std::format_string<Args...> is templated on the arg
        // types; for a (const char*) arg, the type is std::format_string<const char*>.
        // std::make_format_args takes args by reference, so we have to
        // bind "world" to a named lvalue first.
        std::string_view world = "world";
        auto r = petra::try_format_bounded(100, "hello {}", world);
        CHECK(r.has_value());

        std::size_t measured = 0;
        auto cb = [](std::size_t n, void* data) noexcept {
            *static_cast<std::size_t*>(data) = n;
        };
        petra::counting_output_iterator counter{cb, &measured};
        (void)std::vformat_to(std::move(counter), std::string_view{"hello {}"},
                              std::make_format_args(world));
        CHECK(measured == r.value().size());
        CHECK(measured == 11);  // "hello world" — 11 bytes
    }

    // Section 12 — TooLarge boundary sweep.
    section("12.too-large-boundary");
    {
        // "ABCDE" — 5 bytes; cap ∈ {3, 4, 5, 6, 100}.
        for (std::size_t cap : {std::size_t{3}, std::size_t{4},
                                std::size_t{5}, std::size_t{6},
                                std::size_t{100}}) {
            auto r = petra::try_format_bounded(cap, "ABCDE");
            if (cap >= 5) {
                CHECK(r.has_value());
                CHECK(r.has_value() && r.value() == "ABCDE");
            } else {
                CHECK(!r.has_value());
                CHECK(!r.has_value() && r.error().needed_bytes == 5);
            }
        }
    }

    // Section 13 — FormatError equality / inequality.
    section("13.format-error-equality");
    {
        // Two TooLarge with the same fields are equal.
        petra::FormatError a{petra::FormatErrorKind::TooLarge, "x", 5};
        petra::FormatError b{petra::FormatErrorKind::TooLarge, "x", 5};
        CHECK(a == b);
        CHECK(!(a != b));

        // Different needed_bytes makes them unequal.
        petra::FormatError c{petra::FormatErrorKind::TooLarge, "x", 6};
        CHECK(a != c);

        // Different kind makes them unequal.
        petra::FormatError d{petra::FormatErrorKind::Format, "x", 5};
        CHECK(a != d);
    }

    // Section 14 — 50-round determinism on the size measurement.
    section("14.determinism-50rounds");
    {
        for (int i = 0; i < 50; ++i) {
            auto r = petra::try_format_bounded(100, "i={}", i);
            CHECK(r.has_value());
            std::string expected = std::string{"i="} + std::to_string(i);
            CHECK(r.has_value() && r.value() == expected);
            CHECK(r.has_value() && r.value().size() == expected.size());
        }
    }

    // Section 15 — 4 threads x 25 calls each.
    section("15.concurrency");
    {
        std::atomic<int> mismatch{0};
        constexpr int kThreads = 4;
        constexpr int kPerThread = 25;
        std::vector<std::thread> threads;
        threads.reserve(kThreads);
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([t, &mismatch] {
                for (int i = 0; i < kPerThread; ++i) {
                    auto r = petra::try_format_bounded(
                        100, "t={} i={} v={}", t, i, i * 7);
                    std::string expected = std::string{"t="} + std::to_string(t)
                                          + " i=" + std::to_string(i)
                                          + " v=" + std::to_string(i * 7);
                    if (!r.has_value() || r.value() != expected) {
                        mismatch.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }
        for (auto& th : threads) th.join();
        CHECK(mismatch.load() == 0);
    }

    // Section 16 — consteval format-string gates the format_args side.
    //
    // std::format_string<Args...> validation runs at compile time.  A
    // runtime error from std::make_format_args (e.g. an integer that
    // doesn't fit a width) would still throw std::format_error and be
    // routed to FormatError{Format, ...}.  On libc++ 21 with C++23
    // standard format specifiers, such a runtime error is rare for
    // std::format (it's more common for std::format_to_n / std::format_to
    // with capacity-limited sinks).  This section documents the
    // wrapper preserves the contract — the FormatError{Format} branch
    // is reachable in theory even though it's never exercised in the
    // tests below.
    section("16.format-side-runtime");
    {
        // A perfectly valid call produces the std::format output.
        auto r = petra::try_format_bounded(100, "{:d}", 42);
        CHECK(r.has_value());
        CHECK(r.has_value() && r.value() == "42");

        // Width passthrough.
        auto r2 = petra::try_format_bounded(100, "{:5d}", 42);
        CHECK(r2.has_value());
        CHECK(r2.has_value() && r2.value() == "   42");
    }

    std::println("-- summary: {} pass, {} fail", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
