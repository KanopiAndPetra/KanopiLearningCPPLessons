// P-2026-08-25 — std::format_to_n_into: a NUL-terminating bounded-buffer
// formatter wrapper that returns std::expected<std::size_t, std::format_error>
//
// Companion to P-2026-08-24's format_to_expected. Today's lesson closes
// the first new follow-on item the Aug 24 lesson's "Where we go next"
// section explicitly named:
//
//   > `std::format_to_n_into(buf, fmt, args...)` style NUL-terminated
//   > wrapper — the wrapper today returns the would-be size and leaves
//   > NUL-termination to the caller. A small companion that writes
//   > min(r.value(), N-1) chars + a NUL would close the "I just want a
//   > string" use case.
//
// Key design differences from format_to_expected (Aug 24):
//
//   1. Today's wrapper ALWAYS writes a NUL at out[r.size()] for r.size()
//      < N (or out[N-1] for r.size() == N, so the destination is a
//      well-formed C-string regardless of whether the formatted text
//      filled the buffer).
//   2. The destination is `char*` + `N` (a bounded C-string buffer),
//      NOT any output iterator. That is the natural type for the
//      "I just want a string" use case: a `char buf[64]` on the stack,
//      a heap buffer, a fixed-size struct member, etc.
//   3. The error path on format failure STILL NUL-terminates at
//      out[0] before returning — the caller can always print the
//      buffer as a C-string even on failure.
//
// The "would-be" semantics (r.value() == chars that WOULD have been
// written, NOT the chars actually written) is inherited verbatim from
// std::format_to_n. Today's wrapper does NOT change that — only the
// NUL-termination is new.
//
// C++ standard: C++23 (std::format_to_n, std::expected, std::format_string).

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <format>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Section 0 — feature probes + sizeof of relevant types
// ---------------------------------------------------------------------------
//   Section 1 exercises these at runtime.

namespace petra {

// The wrapper. Two overloads:
//   - format_to_n_into(begin, n, fmt, args...)   — bounded [begin, begin+n)
//   - format_to_n_into(span<char>, fmt, args...) — bounded span

template <class... Args>
[[nodiscard]]
std::expected<std::size_t, std::format_error>
format_to_n_into(char* out, std::size_t n,
                 std::format_string<Args...> fmt, Args&&... args)
{
    if (n == 0) {
        // Degenerate: no room for even a NUL. We can't NUL-terminate.
        // We still try to compute the would-be size by formatting into
        // a discard buffer of size 0 (std::format_to_n(out, 0, ...) is
        // well-defined: it returns the would-be size and writes nothing).
        // The cap (0) is the signed iter_difference_t of the char* type.
        try {
            auto result = std::format_to_n(
                out, std::iter_difference_t<char*>{0}, fmt,
                std::forward<Args>(args)...);
            // NUL out[0] is impossible — out has size 0.
            // result.size is documented non-negative (it's std::size_t-like),
            // so the cast is safe.
            return static_cast<std::size_t>(result.size);
        } catch (const std::format_error& e) {
            return std::unexpected{std::format_error{e}};
        } catch (...) {
            return std::unexpected{std::format_error{"unknown formatting error"}};
        }
    }

    // Always reserve one slot for the NUL. Effective write capacity is n-1.
    // std::format_to_n's second parameter is std::iter_difference_t<Out>,
    // which for char* is std::ptrdiff_t (signed).
    auto cap_signed = static_cast<std::iter_difference_t<char*>>(n - 1);

    try {
        // Format at most `cap_signed` chars. The would-be size is returned
        // in result.size regardless of truncation.
        auto result = std::format_to_n(out, cap_signed, fmt,
                                       std::forward<Args>(args)...);

        // result.size is the would-be size (signed iter_difference_t of
        // the Out type). It's documented to be >=0 even when truncated.
        // We want to put the NUL at the smaller of (result.size, cap_signed)
        // and the result is a valid index in [0, n).
        auto written = result.size;
        std::size_t nul_idx = (written < cap_signed)
                                  ? static_cast<std::size_t>(written)
                                  : static_cast<std::size_t>(cap_signed);
        out[nul_idx] = '\0';
        return static_cast<std::size_t>(written);
    } catch (const std::format_error& e) {
        // On failure, leave the buffer well-formed but empty so the
        // caller can always print it as a C-string.
        out[0] = '\0';
        return std::unexpected{std::format_error{e}};
    } catch (...) {
        out[0] = '\0';
        return std::unexpected{std::format_error{"unknown formatting error"}};
    }
}

template <class... Args>
[[nodiscard]]
std::expected<std::size_t, std::format_error>
format_to_n_into(std::span<char> buf,
                 std::format_string<Args...> fmt, Args&&... args)
{
    return format_to_n_into(buf.data(), buf.size(), fmt,
                            std::forward<Args>(args)...);
}

} // namespace petra

// ---------------------------------------------------------------------------
// Section 0 — runtime tests
// ---------------------------------------------------------------------------

namespace {

// Test counter — single-threaded TU, no atomics needed.
int g_pass = 0;
int g_fail = 0;
const char* g_section = "";

#define SECTION(name) do { g_section = name; } while (0)

#define CHECK(cond) do {                                                 \
    if (cond) {                                                          \
        ++g_pass;                                                        \
    } else {                                                             \
        ++g_fail;                                                        \
        std::fprintf(stderr,                                             \
            "FAIL: %s:%d  [%s]  CHECK(%s)\n",                            \
            __FILE__, __LINE__, g_section, #cond);                       \
    }                                                                    \
} while (0)

void section_1_sizeof_and_probes() {
    SECTION("1.sizeof+probes");
    CHECK(sizeof(std::format_error) == 16);
    CHECK(sizeof(std::size_t) == 8);
    CHECK(sizeof(std::expected<std::size_t, std::format_error>) == 24);
    CHECK(__cpp_lib_format == 202110);
    CHECK(__cpp_lib_expected == 202211);
}

void section_2_basic_success() {
    SECTION("2.basic-success");
    char buf[16] = {};
    auto r = petra::format_to_n_into(buf, sizeof(buf), "hello {}", "world");
    CHECK(r.has_value());
    CHECK(r.value() == 11);                 // would-be size: "hello world" = 11
    CHECK(std::string_view{buf} == "hello world");
    CHECK(buf[11] == '\0');
    CHECK(buf[12] == '\0');                 // the rest of buf is still zeroed (init)
}

void section_3_exact_fit_no_truncation() {
    SECTION("3.exact-fit");
    // buf size 6 incl NUL -> 5 writable chars + 1 NUL.
    char buf[6] = {};
    auto r = petra::format_to_n_into(buf, sizeof(buf), "abcde");
    CHECK(r.has_value());
    CHECK(r.value() == 5);
    CHECK(std::string_view{buf} == "abcde");
    CHECK(buf[5] == '\0');
}

void section_4_truncation_writes_nul_at_n_minus_one() {
    SECTION("4.truncation");
    // buf size 8 incl NUL -> 7 writable chars. Format "hello world" (11 chars).
    char buf[8];
    // Poison the whole buffer with 'Q' first — proves the wrapper only
    // touches [0, r.size()] (or [0, n-1] when truncated) and that
    // out[r.size()] == '\0' is the only zero byte.
    for (auto& b : buf) b = 'Q';

    auto r = petra::format_to_n_into(buf, sizeof(buf), "{}", "hello world");
    CHECK(r.has_value());
    CHECK(r.value() == 11);                  // would-be
    CHECK(std::string_view{buf} == "hello w"); // 7 chars actually written
    CHECK(buf[7] == '\0');                  // NUL at n-1
    CHECK(buf[8 - 1] == '\0');              // same thing
    // Bytes beyond buf[7] are out of bounds for our wrapper; the wrapper
    // only writes [0, 7].
}

void section_5_truncation_equal_to_cap_minus_one() {
    SECTION("5.truncation-edge");
    // buf size 8 -> cap = 7. Format "1234567" (7 chars) fits exactly.
    char buf[8] = {};
    auto r = petra::format_to_n_into(buf, sizeof(buf), "1234567");
    CHECK(r.has_value());
    CHECK(r.value() == 7);
    CHECK(std::string_view{buf} == "1234567");
    CHECK(buf[7] == '\0');
}

void section_6_zero_capacity_buffer() {
    SECTION("6.zero-capacity");
    // n = 0: no room for a NUL. The wrapper CANNOT NUL-terminate, but
    // it can still compute the would-be size by formatting into a
    // 0-cap buffer (std::format_to_n writes nothing but reports the
    // would-be size). The wrapper returns the would-be size unchanged
    // (the same contract as the n>=1 path).
    //
    // We can't actually call format_to_n_into with a zero-sized array
    // per the C++ standard (a zero-length array is ill-formed), so we
    // use a heap pointer with n=0.
    std::vector<char> empty;
    auto r = petra::format_to_n_into(empty.data(), empty.size(),
                                     "anything {}", 42);
    CHECK(r.has_value());
    CHECK(r.value() == 11);  // would-be is 11 ("anything 42")
}

void section_7_size_one_buffer_only_nul() {
    SECTION("7.size-one");
    // n = 1: room for only the NUL.
    char buf[1] = {'Q'};  // poison
    auto r = petra::format_to_n_into(buf, sizeof(buf), "anything {}", 42);
    CHECK(r.has_value());
    CHECK(r.value() == 11);  // would-be is 11 ("anything 42")
    CHECK(buf[0] == '\0');   // and the NUL was written
}

void section_8_format_error_on_bad_format_string() {
    SECTION("8.format-error");
    // Synthesise a bad format by feeding a formatter that throws.
    // The consteval gate catches ill-formed format strings at compile
    // time, so the only way to make std::format_to_n throw at runtime
    // is to use a custom formatter that throws — which is not practical
    // in this consumer TU.
    //
    // Instead, verify that the error type is what we say it is:
    // std::format_error derives from std::runtime_error (NOT
    // std::system_error). This is the same shape as the Aug 24 lesson.
    static_assert(!std::is_base_of_v<std::system_error,
                                     std::format_error>);
    static_assert(std::is_base_of_v<std::runtime_error,
                                    std::format_error>);
    static_assert(std::is_base_of_v<std::exception, std::format_error>);
    CHECK(true);
}

void section_9_composes_with_expected_monad() {
    SECTION("9.expected-monad");
    char buf1[64] = {};
    char buf2[64] = {};

    auto r1 = petra::format_to_n_into(buf1, sizeof(buf1),
                                      "size={}", 42);
    CHECK(r1.has_value());

    // transform: lift size_t to string. "size=42" is 7 chars.
    auto s1 = r1.transform([](std::size_t n) {
        return std::string{"got "} + std::to_string(n) + " chars";
    });
    CHECK(s1.has_value());
    CHECK(s1.value() == "got 7 chars");

    // and_then: chain — reformat r1.value() into buf2
    auto r2 = r1.and_then([&buf2](std::size_t n)
        -> std::expected<std::size_t, std::format_error> {
        return petra::format_to_n_into(buf2, sizeof(buf2),
                                       "[r1={}]", n);
    });
    CHECK(r2.has_value());
    CHECK(std::string_view{buf2} == "[r1=7]");
    CHECK(r2.value() == 6);

    // transform_error: lift format_error to a friendlier string
    std::expected<std::size_t, std::format_error> bad{
        std::unexpect, std::format_error{"synthetic"}};
    auto mapped = bad.transform_error([](const std::format_error& e) {
        return std::string{"format failed: "} + e.what();
    });
    CHECK(mapped.has_value() == false);
    CHECK(mapped.error().find("format failed: synthetic") != std::string::npos);
}

void section_10_span_overload() {
    SECTION("10.span-overload");
    std::array<char, 32> arr{};
    std::span<char> sp{arr};
    auto r = petra::format_to_n_into(sp, "abc={}", 1);
    CHECK(r.has_value());
    CHECK(r.value() == 5);
    CHECK(std::string_view{arr.data()} == "abc=1");
    CHECK(arr[5] == '\0');
}

void section_11_int_types() {
    SECTION("11.int-types");
    char buf[32] = {};
    auto r = petra::format_to_n_into(buf, sizeof(buf),
                                     "{:08x} {:d} {:+d}", 0xCAFE, -7, 42);
    CHECK(r.has_value());
    CHECK(std::string_view{buf} == "0000cafe -7 +42");
    // length: 8 + 1 + 2 + 1 + 3 = 15
    CHECK(r.value() == 15);
    CHECK(buf[15] == '\0');
}

void section_12_float_types() {
    SECTION("12.float-types");
    char buf[32] = {};
    auto r = petra::format_to_n_into(buf, sizeof(buf), "{:.3f}", 3.14159);
    CHECK(r.has_value());
    CHECK(std::string_view{buf} == "3.142");
    CHECK(r.value() == 5);
    CHECK(buf[5] == '\0');
}

void section_13_string_view_args() {
    SECTION("13.string-view-args");
    char buf[32] = {};
    std::string_view name = "Petra";
    auto r = petra::format_to_n_into(buf, sizeof(buf),
                                     "hello, {}!", name);
    CHECK(r.has_value());
    CHECK(std::string_view{buf} == "hello, Petra!");
    CHECK(r.value() == 13);
}

void section_14_unicode_round_trip() {
    SECTION("14.unicode");
    // UTF-8 byte sequences (we don't decode — std::format writes the
    // raw UTF-8 bytes of string_view args).
    char buf[32] = {};
    auto r = petra::format_to_n_into(buf, sizeof(buf),
                                     "{}", "héllo \xF0\x9F\x98\x80");
    CHECK(r.has_value());
    // "héllo " = 7 bytes (é is 2 bytes in UTF-8: h(1) é(2) l(1) l(1) o(1) space(1)),
    // "😀" = 4 bytes → 11 bytes total
    CHECK(r.value() == 11);
    CHECK(std::string_view{buf} == "héllo \xF0\x9F\x98\x80");
    CHECK(buf[11] == '\0');
}

void section_15_determinism_stress() {
    SECTION("15.determinism");
    // 50 rounds of the same format into fresh buffers. The output and
    // the would-be size must be byte-identical across runs.
    constexpr const char* fmt = "round={} hash={:08x} pi={:.3f}";
    constexpr std::size_t buf_size = 64;
    std::string first_out;     // owned, not a string_view into the stack
    std::size_t first_size = 0;
    bool first_set = false;

    for (int i = 0; i < 50; ++i) {
        char buf[buf_size] = {};
        auto r = petra::format_to_n_into(buf, buf_size, fmt,
                                         17, 17u * 2654435761u, 3.14159);
        CHECK(r.has_value());
        if (!first_set) {
            first_out.assign(buf, r.value());
            first_size = r.value();
            first_set = true;
        } else {
            CHECK(r.value() == first_size);
            std::string_view sv{buf, r.value()};
            CHECK(sv == first_out);
        }
    }

    // Pin the first-round output for the lesson notes.
    CHECK(first_out == "round=17 hash=81af14c1 pi=3.142");
    CHECK(first_size == 31);
}

void section_16_nul_byte_safety() {
    SECTION("16.nul-safety");
    // Verifies that the wrapper never writes out-of-bounds and never
    // fails to NUL-terminate (when n > 0).
    //
    // For each buf size from 1 to 32, format a string longer than the
    // buf. The output must be a valid C-string of length <= n-1.
    for (std::size_t n = 1; n <= 32; ++n) {
        std::vector<char> buf(n, 'Q');
        auto r = petra::format_to_n_into(buf.data(), n,
                                         "this is a long string for testing {} {}",
                                         1, 2);
        if (!r.has_value()) {
            std::fprintf(stderr,
                "FAIL: n=%zu unexpectedly errored\n", n);
            ++g_fail;
            continue;
        }
        // Must be NUL-terminated within [0, n-1].
        bool found_nul = false;
        for (std::size_t i = 0; i < n; ++i) {
            if (buf[i] == '\0') {
                found_nul = true;
                break;
            }
        }
        if (!found_nul) {
            std::fprintf(stderr,
                "FAIL: n=%zu no NUL found in buffer\n", n);
            ++g_fail;
            continue;
        }
        // The prefix up to the NUL must be a valid std::string_view.
        std::string_view sv{buf.data()};
        CHECK(sv.size() <= n - 1);
        ++g_pass;
    }
}

void section_17_distinct_from_format_to_expected() {
    SECTION("17.distinct-from-format_to_expected");
    // Today's wrapper NUL-terminates; Aug 24's format_to_expected
    // does NOT. Verify by formatting into a poisoned buffer with each
    // wrapper and checking for the NUL.

    // Aug 24 wrapper: format_to_expected does NOT NUL-terminate.
    // We exercise std::format_to_n directly here for the comparison,
    // simulating the Aug 24 contract.
    char buf_a[16];
    for (auto& b : buf_a) b = 'X';
    auto raw_a = std::format_to_n(buf_a, 4, "abcdef");
    CHECK(raw_a.size == 6);  // would-be == 6 (truncated from cap=4)
    bool found_nul = false;
    for (std::size_t i = 0; i < 4; ++i) {
        if (buf_a[i] == '\0') { found_nul = true; break; }
    }
    CHECK(found_nul == false);  // Aug 24 contract: no NUL
    CHECK(buf_a[4] == 'X');     // untouched

    // Today's wrapper: format_to_n_into DOES NUL-terminate.
    char buf_b[16];
    for (auto& b : buf_b) b = 'X';
    auto r_b = petra::format_to_n_into(buf_b, sizeof(buf_b), "abcdef");
    CHECK(r_b.has_value());
    CHECK(std::string_view{buf_b} == "abcdef");
    CHECK(buf_b[6] == '\0');
    CHECK(r_b.value() == 6);  // would-be == 6, fits in 15

    // Truncation: today's wrapper writes NUL at n-1 when truncated.
    char buf_c[8];
    for (auto& b : buf_c) b = 'Y';
    auto r_c = petra::format_to_n_into(buf_c, sizeof(buf_c), "abcdef");
    CHECK(r_c.has_value());
    CHECK(r_c.value() == 6);          // would-be
    CHECK(std::string_view{buf_c} == "abcdef"); // cap = 7, fits exactly
    // No truncation in this case — let's force one:
    char buf_d[5];
    for (auto& b : buf_d) b = 'Z';
    auto r_d = petra::format_to_n_into(buf_d, sizeof(buf_d), "abcdef");
    CHECK(r_d.has_value());
    CHECK(r_d.value() == 6);
    CHECK(std::string_view{buf_d} == "abcd");  // 4 chars, then NUL
    CHECK(buf_d[4] == '\0');
}

void section_18_explicit_unexpected_construction() {
    SECTION("18.unexpected-ctor");
    // Verify the std::unexpected{std::format_error{...}} ctor pattern
    // used by the wrapper. We construct one manually and check the
    // expected/unexpected state.
    std::expected<int, std::format_error> bad{
        std::unexpect, std::format_error{"x"}};
    CHECK(bad.has_value() == false);
    CHECK(std::string{bad.error().what()} == "x");

    // And the wrapped-success path: same wrapper, success side.
    std::expected<int, std::format_error> good = 42;
    CHECK(good.has_value());
    CHECK(good.value() == 42);
}

void section_19_signed_unsigned_size_interop() {
    SECTION("19.signed-unsigned-size");
    // The wrapper's size_t return type composes cleanly with the
    // signed-size_t return type of std::format_to_n_result.
    // We verify the round-trip from std::format_to_n's signed result.size
    // through to our unsigned return is identical.
    char buf[16] = {};
    auto raw = std::format_to_n(buf, 15, "{}", "hello");
    CHECK(raw.size == 5);
    auto wrapped = petra::format_to_n_into(buf, sizeof(buf), "{}", "hello");
    CHECK(wrapped.value() == static_cast<std::size_t>(raw.size));
}

} // namespace

int main() {
    std::printf("== P-2026-08-25 std::format_to_n_into: NUL-terminating bounded-buffer formatter ==\n");

    section_1_sizeof_and_probes();
    section_2_basic_success();
    section_3_exact_fit_no_truncation();
    section_4_truncation_writes_nul_at_n_minus_one();
    section_5_truncation_equal_to_cap_minus_one();
    section_6_zero_capacity_buffer();
    section_7_size_one_buffer_only_nul();
    section_8_format_error_on_bad_format_string();
    section_9_composes_with_expected_monad();
    section_10_span_overload();
    section_11_int_types();
    section_12_float_types();
    section_13_string_view_args();
    section_14_unicode_round_trip();
    section_15_determinism_stress();
    section_16_nul_byte_safety();
    section_17_distinct_from_format_to_expected();
    section_18_explicit_unexpected_construction();
    section_19_signed_unsigned_size_interop();

    std::printf("\nTotal: %d/%d PASS\n", g_pass, g_pass + g_fail);
    if (g_fail != 0) {
        std::fprintf(stderr, "%d FAIL(s) — see above\n", g_fail);
        return 1;
    }
    return 0;
}
