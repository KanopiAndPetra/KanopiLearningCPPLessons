// P-2026-08-24 — std::format_to_n wrapped in std::expected<size_t, format_error>
//
// A focused tour that closes the second item from the Aug 22 lesson's
// "Where we go next" follow-on list, verbatim:
//
//   > "A format_to_n_into style consumer-side helper that returns
//      std::expected<std::size_t, FormatError> — composes format_to_n
//      with expected's error channel. Small, focused."
//
// Aug 22 covered std::print / std::format / std::vprint_unicode (the
// streaming-println surface). Today covers the COMPLEMENTARY side:
// a BOUNDED-BUFFER consumer that uses std::format_to_n (P2216R3 —
// already in libc++ 21) and routes any failure through std::expected
// (P0323R12 — the C++23 result type, already covered Jul 12).
//
// The single API we build today:
//
//   template <class Out, class... Args>
//   std::expected<std::size_t, std::format_error>
//   format_to_expected(Out out, std::iter_difference_t<Out> n,
//                      std::format_string<Args...> fmt, Args&&... args);
//
// — returns the number of characters that WOULD HAVE been written to
//   the buffer on success (NOT the number of chars actually written,
//   which may be < n on truncation), or std::format_error on the
//   single failure mode std::format_to_n can raise at runtime. The
//   format string is consteval-checked (P2216R3) so it cannot fail
//   at runtime; the try/catch is a forward-compatibility seam for
//   future implementations that surface std::format_error through
//   exceptions.
//
// Sections (about 60 tests verified):
//   1. Toolchain + feature probes + sizeof of the relevant types
//   2. The core API: a 9-line wrapper, success path
//   3. Short output that fits within N — exact return count
//   4. Truncation: output larger than N — "would-be" semantics
//   5. back_inserter: growing destination, N is a hint
//   6. Composing with std::expected — and_then / transform / transform_error
//   7. Heterogeneous buffer types — array / vector / span / char*
//   8. std::format_error's surface — what() / hierarchy / error_channel
//   9. Round-trip — parse-then-format, single error channel
//  10. Determinism stress — 50 rounds, ASan/UBSan smoke

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

namespace {

// ---------------------------------------------------------------------------
// Test harness
// ---------------------------------------------------------------------------

int g_pass = 0;
int g_fail = 0;

#define CHECK(expr) do {                                       \
    if (expr) { ++g_pass; }                                    \
    else      { ++g_fail;                                      \
                std::fprintf(stderr,                            \
                    "  FAIL @ %s:%d: %s\n",                   \
                    __FILE__, __LINE__, #expr);                \
            }                                                   \
} while (0)

#define CHECK_EQ(a, b) do {                                    \
    auto _av = (a); auto _bv = (b);                            \
    if (_av == _bv) { ++g_pass; }                              \
    else { ++g_fail;                                           \
           std::fprintf(stderr,                                 \
               "  FAIL @ %s:%d: %s == %s  (%lld vs %lld)\n", \
               __FILE__, __LINE__, #a, #b,                     \
               (long long)_av, (long long)_bv);                \
    }                                                          \
} while (0)

#define CHECK_STREQ(a, b) do {                                 \
    std::string_view _av = (a); std::string_view _bv = (b);    \
    if (_av == _bv) { ++g_pass; }                              \
    else { ++g_fail;                                           \
           std::fprintf(stderr,                                 \
               "  FAIL @ %s:%d: '%.*s' vs '%.*s'\n",         \
               __FILE__, __LINE__,                             \
               (int)_av.size(), _av.data(),                    \
               (int)_bv.size(), _bv.data());                   \
    }                                                          \
} while (0)

#define CHECK_EQ_U(a, b) do {                                  \
    auto _av = (a); auto _bv = (b);                            \
    if (_av == _bv) { ++g_pass; }                              \
    else { ++g_fail;                                           \
           std::fprintf(stderr,                                 \
               "  FAIL @ %s:%d: %zu vs %zu\n",                \
               __FILE__, __LINE__, (size_t)_av, (size_t)_bv); \
    }                                                          \
} while (0)

// ---------------------------------------------------------------------------
// The API being studied
// ---------------------------------------------------------------------------
//
// This is the entire "library" — a 9-line wrapper that composes two
// C++23 facilities. The lesson is about UNDERSTANDING what it returns
// and WHY, not about building a production formatter.

template <class Out, class... Args>
std::expected<std::size_t, std::format_error>
format_to_expected(Out out, std::iter_difference_t<Out> n,
                   std::format_string<Args...> fmt, Args&&... args)
{
    try {
        auto result = std::format_to_n(out, n, fmt,
                                       std::forward<Args>(args)...);
        // format_to_n_result<Out> has .out (the iterator) and .size
        // (the number of chars that WOULD have been written). We
        // return .size — that is the standard semantics, distinct
        // from the count of chars actually written when n < would-be.
        return static_cast<std::size_t>(result.size);
    } catch (const std::format_error& e) {
        return std::unexpected{std::format_error{e}};
    } catch (...) {
        return std::unexpected{std::format_error{"unknown formatting error"}};
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Section 1 — toolchain + feature probes
// ---------------------------------------------------------------------------

namespace section1 {

void run() {
    std::printf("=== Section 1 — toolchain + feature probes ===\n");
    std::printf("  __cplusplus                  = %ld\n", (long)__cplusplus);
    std::printf("  __cpp_lib_format             = %ld\n", (long)__cpp_lib_format);
    std::printf("  __cpp_lib_expected           = %ld\n", (long)__cpp_lib_expected);
    std::printf("  sizeof(std::format_error)    = %zu B\n",
                sizeof(std::format_error));
    std::printf("  sizeof(format_to_n_result<char*>) = %zu B\n",
                sizeof(std::format_to_n_result<char*>));
    std::printf("  sizeof(expected<size_t, format_error>) = %zu B\n",
                sizeof(std::expected<std::size_t, std::format_error>));

    static_assert(__cpp_lib_format   >= 202110, "C++20 std::format");
    static_assert(__cpp_lib_expected >= 202211, "C++23 std::expected");

    CHECK(__cplusplus == 202302);
    CHECK(__cpp_lib_format   == 202110);
    CHECK(__cpp_lib_expected == 202211);

    // std::format_error derives from std::runtime_error, which on
    // libc++ 21 holds a std::string in SSO (16 B). So format_error is
    // 16 B on this toolchain.
    CHECK(sizeof(std::format_error) == 16u);

    // format_to_n_result<Out> on libc++ 21 is { Out, size_t } — so
    // for char* (8 B) and size_t (8 B) on arm64, 16 B.
    CHECK(sizeof(std::format_to_n_result<char*>) == 16u);

    // We deliberately do NOT assert the expected size: libc++ 21
    // places it as { union{T, E}, bool } but the alignment padding
    // can shift across versions. We just print it.
    CHECK(true);
}

}  // namespace section1

// ---------------------------------------------------------------------------
// Section 2 — the core API, success path
// ---------------------------------------------------------------------------

namespace section2 {

void run() {
    std::printf("=== Section 2 — core API: success path ===\n");

    char buf[64] = {};
    auto r = format_to_expected(buf, std::iter_difference_t<char*>{63},
                                "{} + {} = {}", 1, 2, 3);

    static_assert(std::is_same_v<decltype(r),
                  std::expected<std::size_t, std::format_error>>);

    CHECK(r.has_value());
    CHECK_EQ(r.value(), std::size_t{9});      // "1 + 2 = 3" is 9 chars
    CHECK_STREQ(std::string_view(buf, 9), "1 + 2 = 3");

    // A second call on the same buffer reuses it.
    auto r2 = format_to_expected(buf, std::iter_difference_t<char*>{63},
                                 "{}", "hello");
    CHECK(r2.has_value());
    CHECK_EQ(r2.value(), std::size_t{5});
    CHECK_STREQ(std::string_view(buf, 5), "hello");
}

}  // namespace section2

// ---------------------------------------------------------------------------
// Section 3 — short output that fits in N
// ---------------------------------------------------------------------------

namespace section3 {

void run() {
    std::printf("=== Section 3 — output fits within N ===\n");

    // Buffer of 16, output of 4 — returns 4 and writes 4 chars.
    char buf[16];
    std::memset(buf, 'X', sizeof(buf));  // poison to verify non-writes

    auto r = format_to_expected(buf, std::iter_difference_t<char*>{15},
                                "x={}", 42);
    CHECK(r.has_value());
    CHECK_EQ(r.value(), std::size_t{4});
    CHECK_STREQ(std::string_view(buf, 4), "x=42");
    CHECK_EQ(buf[4], 'X');               // byte we did NOT touch
    CHECK_EQ(buf[5], 'X');

    // Edge case: empty format string with no args, into buffer of 16.
    char buf2[16];
    std::memset(buf2, 'Y', sizeof(buf2));
    auto r2 = format_to_expected(buf2, std::iter_difference_t<char*>{15}, "");
    CHECK(r2.has_value());
    CHECK_EQ(r2.value(), std::size_t{0});
    CHECK_EQ(buf2[0], 'Y');

    // Format with width/precision, fits in N.
    char buf3[32];
    auto r3 = format_to_expected(buf3, std::iter_difference_t<char*>{31},
                                 "[{:>6}]", 7);
    CHECK(r3.has_value());
    CHECK_EQ(r3.value(), std::size_t{8});
    CHECK_STREQ(std::string_view(buf3, 8), "[     7]");
}

}  // namespace section3

// ---------------------------------------------------------------------------
// Section 4 — truncation: output LARGER than N
// ---------------------------------------------------------------------------

namespace section4 {

void run() {
    std::printf("=== Section 4 — truncation: output larger than N ===\n");

    // Buffer of 8 (size 7), output of 11 ("hello world" is 11 chars).
    char buf[8];
    std::memset(buf, 'Z', sizeof(buf));

    auto r = format_to_expected(buf, std::iter_difference_t<char*>{7},
                                "{}", "hello world");
    CHECK(r.has_value());
    // The .size() member of format_to_n_result is the WOULD-BE size,
    // not the truncated size. This is the key gotcha — see the
    // md notes.
    CHECK_EQ(r.value(), std::size_t{11});
    // The buffer holds exactly N chars (7). No implicit NUL.
    CHECK_STREQ(std::string_view(buf, 7), "hello w");

    // Demonstrate the "would-be" semantics with a tiny N.
    char buf2[4];
    auto r2 = format_to_expected(buf2, std::iter_difference_t<char*>{3},
                                 "{}", "ABCDE");
    CHECK(r2.has_value());
    CHECK_EQ(r2.value(), std::size_t{5});    // would-be = 5
    CHECK_STREQ(std::string_view(buf2, 3), "ABC");  // only 3 chars written

    // Demonstrate that format_to_n does NOT write a NUL terminator:
    // bytes after the written prefix are NOT zeroed.
    char buf3[8];
    std::memset(buf3, 'Q', sizeof(buf3));
    auto r3 = format_to_expected(buf3, std::iter_difference_t<char*>{7},
                                 "hi");
    CHECK(r3.has_value());
    CHECK_EQ(r3.value(), std::size_t{2});
    CHECK_EQ(buf3[0], 'h');
    CHECK_EQ(buf3[1], 'i');
    CHECK_EQ(buf3[2], 'Q');                 // untouched

    // Note: callers that want a NUL-terminated string must add one
    // themselves at position min(would-be, N). The wrapper does not
    // do this — it is a byte-counting consumer.
}

}  // namespace section4

// ---------------------------------------------------------------------------
// Section 5 — append vs. fixed
// ---------------------------------------------------------------------------

namespace section5 {

void run() {
    std::printf("=== Section 5 — append vs. fixed destination ===\n");

    // back_inserter grows the destination, BUT std::format_to_n
    // still caps the WRITE at N chars. n=15 is enough for the
    // "name=petra id=7" output (15 chars), so we get the full
    // string back and r.value() == 15.
    std::string s;
    auto r = format_to_expected(std::back_inserter(s),
                                std::iter_difference_t<
                                    std::back_insert_iterator<std::string>>{15},
                                "name={} id={}", "petra", 7);
    CHECK(r.has_value());
    CHECK_EQ(r.value(), std::size_t{15});
    CHECK_STREQ(s, "name=petra id=7");
    CHECK_EQ(s.size(), 15u);

    // n=7 is enough for "v=3.142" (7 chars). Back_inserter grows the
    // std::string, but the formatter is capped at N=7 chars written.
    std::string s2;
    auto r2 = format_to_expected(std::back_inserter(s2),
                                 std::iter_difference_t<
                                     std::back_insert_iterator<std::string>>{7},
                                 "v={:.3f}", 3.14159);
    CHECK(r2.has_value());
    CHECK_EQ(r2.value(), std::size_t{7});   // "v=3.142" is 7 chars
    CHECK_STREQ(s2, "v=3.142");

    // Multiple appends accumulate correctly when N is large enough
    // for each call. Each call's N must independently fit its output.
    std::string s3;
    format_to_expected(std::back_inserter(s3),
                       std::iter_difference_t<
                           std::back_insert_iterator<std::string>>{3},
                       "[A]");
    format_to_expected(std::back_inserter(s3),
                       std::iter_difference_t<
                           std::back_insert_iterator<std::string>>{3},
                       "[B]");
    format_to_expected(std::back_inserter(s3),
                       std::iter_difference_t<
                           std::back_insert_iterator<std::string>>{3},
                       "[C]");
    CHECK_STREQ(s3, "[A][B][C]");
    CHECK_EQ(s3.size(), 9u);

    // CRITICAL: std::format_to_n caps the write at N EVEN with a
    // back_inserter. N=3 truncates "ABCDE" to "ABC", and r.value()
    // returns the WOULD-BE size (5) — not the actual chars written.
    std::string s4;
    auto r4 = format_to_expected(std::back_inserter(s4),
                                 std::iter_difference_t<
                                     std::back_insert_iterator<std::string>>{3},
                                 "{}", "ABCDE");
    CHECK(r4.has_value());
    CHECK_EQ(r4.value(), std::size_t{5});   // would-be = 5
    CHECK_STREQ(s4, "ABC");                // actually wrote 3 chars
    CHECK_EQ(s4.size(), 3u);
}

}  // namespace section5

// ---------------------------------------------------------------------------
// Section 6 — composing with std::expected
// ---------------------------------------------------------------------------

namespace section6 {

void run() {
    std::printf("=== Section 6 — composing with std::expected ===\n");

    // The Jul 12 lesson introduced expected's monadic ops. They
    // compose here: take the wrapper's expected<size_t, format_error>,
    // run .transform to lift to expected<string, format_error>, then
    // .and_then to validate the count.

    char buf[32];

    auto e1 = format_to_expected(buf, std::iter_difference_t<char*>{31},
                                 "{:08x}", 0xdeadbeef);
    CHECK(e1.has_value());
    CHECK_EQ(e1.value(), std::size_t{8});
    CHECK_STREQ(std::string_view(buf, 8), "deadbeef");

    // .transform: turn size_t -> string
    auto e2 = e1.transform([](std::size_t n) {
        return std::string{"wrote "} + std::to_string(n) + " bytes";
    });
    static_assert(std::is_same_v<decltype(e2),
                  std::expected<std::string, std::format_error>>);
    CHECK(e2.has_value());
    CHECK_STREQ(e2.value(), "wrote 8 bytes");

    // .and_then: chain a second format step that depends on the first
    char buf2[32];
    auto e3 = e1.and_then(
        [&buf2](std::size_t n) -> std::expected<std::size_t, std::format_error> {
            return format_to_expected(buf2, std::iter_difference_t<char*>{31},
                                      "[n={}]", n)
                .transform([](std::size_t m) { return m + 100; });
        });
    CHECK(e3.has_value());
    CHECK_EQ(e3.value(), std::size_t{105});
    CHECK_STREQ(std::string_view(buf2, 5), "[n=8]");

    // .transform_error: synthesize an error to demonstrate the channel.
    // transform_error is a member of expected (NOT unexpected); call it
    // on a default-constructed expected with an unexpected value inside.
    std::expected<std::size_t, std::format_error> bad{
        std::unexpect, std::format_error{"synthetic"}};
    auto mapped = bad.transform_error([](const std::format_error& e) {
        return std::string{"format failed: "} + e.what();
    });
    static_assert(std::is_same_v<decltype(mapped),
                  std::expected<std::size_t, std::string>>);
    CHECK(!mapped.has_value());
    CHECK(mapped.error().find("synthetic") != std::string::npos);
}

}  // namespace section6

// ---------------------------------------------------------------------------
// Section 7 — heterogeneous buffer types
// ---------------------------------------------------------------------------

namespace section7 {

void run() {
    std::printf("=== Section 7 — heterogeneous buffer types ===\n");

    // std::array<char, N>
    std::array<char, 16> arr{};
    auto r1 = format_to_expected(arr.data(),
                                 std::iter_difference_t<char*>{15},
                                 "{:>5}", 42);
    CHECK(r1.has_value());
    CHECK_EQ(r1.value(), std::size_t{5});
    CHECK_STREQ(std::string_view(arr.data(), 5), "   42");

    // std::vector<char>::iterator
    std::vector<char> v(32, '\0');
    auto r2 = format_to_expected(v.begin(),
                                 static_cast<std::iter_difference_t<
                                     std::vector<char>::iterator>>(v.size()),
                                 "v={}", 99);
    CHECK(r2.has_value());
    CHECK_EQ(r2.value(), std::size_t{4});
    CHECK_STREQ(std::string_view(v.data(), 4), "v=99");

    // std::span<char> iterator
    std::vector<char> backing(16, '?');
    std::span<char> sp{backing};
    auto r3 = format_to_expected(sp.begin(),
                                 static_cast<std::iter_difference_t<
                                     std::span<char>::iterator>>(sp.size()),
                                 "{}", "span");
    CHECK(r3.has_value());
    CHECK_EQ(r3.value(), std::size_t{4});
    CHECK_STREQ(std::string_view(backing.data(), 4), "span");

    // raw char* (the canonical case from Section 2 / 4)
    char raw[16] = {};
    auto r4 = format_to_expected(raw, std::iter_difference_t<char*>{15},
                                 "{}-{}", "x", 1);
    CHECK(r4.has_value());
    CHECK_EQ(r4.value(), std::size_t{3});
    CHECK_STREQ(std::string_view(raw, 3), "x-1");

    // static_assert the return-type contract for arbitrary Out.
    static_assert(std::is_same_v<
        decltype(format_to_expected(arr.data(), 0, "{}", 1)),
        std::expected<std::size_t, std::format_error>>);
}

}  // namespace section7

// ---------------------------------------------------------------------------
// Section 8 — std::format_error's surface
// ---------------------------------------------------------------------------

namespace section8 {

void run() {
    std::printf("=== Section 8 — std::format_error surface ===\n");

    std::format_error fe{"a format error"};
    CHECK_STREQ(std::string_view(fe.what()), "a format error");

    // std::format_error derives from std::runtime_error, NOT from
    // std::system_error. It has no .code() method. The wrapper
    // returns std::format_error directly — the caller must .what()
    // it, not compare error codes.

    static_assert(!std::is_base_of_v<std::system_error, std::format_error>);
    static_assert(std::is_base_of_v<std::runtime_error, std::format_error>);
    CHECK(true);  // static_asserts above prove the hierarchy

    // Demonstrate the typed error channel by constructing an
    // expected<...> with std::format_error inside.
    std::expected<std::size_t, std::format_error> bad{
        std::unexpect, std::format_error{"width too small"}};
    auto mapped = bad.transform_error([](const std::format_error& e) {
        return std::string{"ERR: "} + e.what();
    });
    CHECK(!mapped.has_value());
    CHECK_STREQ(mapped.error(), "ERR: width too small");

    // what() on a real format_error carries the message verbatim.
    std::format_error real{"nope"};
    std::string msg = real.what();
    CHECK_STREQ(msg, "nope");
}

}  // namespace section8

// ---------------------------------------------------------------------------
// Section 9 — round-trip: parse-then-format
// ---------------------------------------------------------------------------

namespace section9 {

// Toy integer parser that returns expected<int, std::format_error>.
// We reuse format_error as the error channel so the whole pipeline
// has ONE error type — and one place to .what() for diagnostics.
std::expected<int, std::format_error>
parse_int(std::string_view s)
{
    if (s.empty())
        return std::unexpected{std::format_error{"empty input"}};
    int sign = 1;
    std::size_t i = 0;
    if (s.front() == '-') { sign = -1; i = 1; }
    else if (s.front() == '+') { i = 1; }
    if (i == s.size())
        return std::unexpected{std::format_error{"sign only"}};
    int v = 0;
    for (; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9')
            return std::unexpected{std::format_error{"non-digit at offset "
                                                     + std::to_string(i)}};
        v = v * 10 + (s[i] - '0');
    }
    return sign * v;
}

void run() {
    std::printf("=== Section 9 — round-trip: parse then format ===\n");

    // Happy path: parse "12345", format as "value=12345".
    char buf[32];
    auto parsed = parse_int("12345");
    static_assert(std::is_same_v<decltype(parsed),
                  std::expected<int, std::format_error>>);
    CHECK(parsed.has_value());
    CHECK_EQ(parsed.value(), 12345);

    auto formatted = format_to_expected(buf,
                                        std::iter_difference_t<char*>{31},
                                        "value={}", parsed.value());
    CHECK(formatted.has_value());
    CHECK_EQ(formatted.value(), std::size_t{11});
    CHECK_STREQ(std::string_view(buf, 11), "value=12345");

    // Compose the two expecteds in a single chain using and_then.
    char buf2[32];
    auto composed = parse_int("-987")
        .and_then([](int n) -> std::expected<std::string, std::format_error> {
            return std::string{"n="} + std::to_string(n);
        })
        .and_then([&buf2](const std::string& s)
                  -> std::expected<std::size_t, std::format_error> {
            return format_to_expected(buf2,
                                      std::iter_difference_t<char*>{31},
                                      "[{}]", s);
        });
    CHECK(composed.has_value());
    CHECK_EQ(composed.value(), std::size_t{8});    // "[n=-987]" is 8 chars
    CHECK_STREQ(std::string_view(buf2, 8), "[n=-987]");

    // Error path: empty input -> format_error propagates through the chain.
    char buf3[32];
    std::memset(buf3, 0, sizeof(buf3));
    auto error_chain = parse_int("")
        .and_then([](int n) -> std::expected<std::string, std::format_error> {
            return std::string{"n="} + std::to_string(n);
        })
        .and_then([&buf3](const std::string& s)
                  -> std::expected<std::size_t, std::format_error> {
            return format_to_expected(buf3,
                                      std::iter_difference_t<char*>{31},
                                      "[{}]", s);
        });
    CHECK(!error_chain.has_value());
    CHECK_STREQ(std::string_view(error_chain.error().what()), "empty input");

    // Error path: parse fails -> format never runs -> buffer untouched.
    CHECK_EQ(buf3[0], '\0');
}

}  // namespace section9

// ---------------------------------------------------------------------------
// Section 10 — ASan/UBSan smoke + determinism
// ---------------------------------------------------------------------------

namespace section10 {

void run() {
    std::printf("=== Section 10 — determinism stress ===\n");

    // Run the wrapper 50 times with the SAME input into a fresh
    // buffer; every output must be byte-identical. This catches
    // any UB in the wrapper that depends on prior state or
    // uninitialised stack.
    const int fixed_i = 17;
    std::string first;
    std::size_t first_size = 0;
    for (int rep = 0; rep < 50; ++rep) {
        char buf[64];
        std::memset(buf, 0, sizeof(buf));
        auto r = format_to_expected(buf,
                                    std::iter_difference_t<char*>{63},
                                    "round={} hash={:08x} pi={:.3f}",
                                    fixed_i, fixed_i * 2654435761u, 3.14159);
        CHECK(r.has_value());
        if (rep == 0) {
            first_size = r.value();
            first = std::string(buf, r.value());
        } else {
            CHECK_EQ_U(r.value(), first_size);
            std::string cur(buf, r.value());
            CHECK_STREQ(cur, first);
        }
    }
    CHECK(!first.empty());
    CHECK_EQ_U(first_size, 31u);  // "round=17 hash=0e3779b1 pi=3.142" is 31 chars

    // Truncation determinism: 50 rounds of the same truncation.
    // Use a fixed integer so each round formats the SAME prefix.
    const int fixed_trunc = 7;
    std::string first_prefix;
    std::size_t first_total = 0;
    for (int rep = 0; rep < 50; ++rep) {
        char buf[8];
        std::memset(buf, '?', sizeof(buf));
        auto r = format_to_expected(buf, std::iter_difference_t<char*>{7},
                                    "[{}]", fixed_trunc);
        CHECK(r.has_value());
        if (rep == 0) {
            first_total = r.value();
            first_prefix = std::string(buf, 7);
        } else {
            CHECK_EQ_U(r.value(), first_total);
            CHECK_STREQ(std::string_view(buf, 7), first_prefix);
        }
    }
    CHECK_STREQ(first_prefix, "[7]????");  // 3 written, 4 untouched '?'
}

}  // namespace section10

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    std::printf("P-2026-08-24 — std::format_to_n + std::expected<size_t, format_error>\n");
    std::printf("Apple Clang 21 / libc++ / C++23\n\n");

    section1::run();
    section2::run();
    section3::run();
    section4::run();
    section5::run();
    section6::run();
    section7::run();
    section8::run();
    section9::run();
    section10::run();

    std::printf("\n========================================\n");
    std::printf("Total: %d/%d PASS, %d FAIL\n",
                g_pass, g_pass + g_fail, g_fail);
    std::printf("========================================\n");
    return g_fail == 0 ? 0 : 1;
}
