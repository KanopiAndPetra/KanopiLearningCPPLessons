// P-2026-08-28 — std::ostream_print: a consumer-side adapter that
// closes the std::print(std::ostream&, ...) overload gap left by
// libc++ 21.
//
// Why today
// ---------
// The Aug 27 (P-2026-08-27) lesson closed the consumer-side
// expected<E, ...> projection follow-on. The Aug 22 (P-2026-08-22)
// lesson, on std::print / std::println / std::format, listed two
// open follow-on items in its "Where we go next" section. Both were
// re-listed by Aug 27. The first of those two is:
//
//   > std::format to std::ostream — the missing overload.
//   > P2093R14 mentions it; libc++ 21 has not implemented it. The
//   > workaround (std::format -> std::string -> <<) is fine but a
//   > real std::print(std::ostream&, ...) would close the gap.
//
// Today IS that lesson. We design and ship a small
// `petra::ostream_print(std::ostream&, fmt, args...)` adapter (and
// its bounded-buffer sibling) that uses std::vformat_to from
// P2216R3 with a custom ostream-backed output iterator. We exercise
// it end-to-end against std::stringstream / std::ofstream /
// std::ostringstream and pin down the design decisions that the
// missing libc++ overload would have hidden (where bytes go, what
// happens on format_error, what happens on overflow when we use
// the bounded variant, what happens with UTF-8, what happens with
// the expected<size_t, format_error> sized-bounded variant).
//
// | Surface                                       | libc++21 | Aug28 adapter |
// |-----------------------------------------------|----------|---------------|
// | std::print(FILE*, fmt, args...)               |    ✅    |      ✅       |
// | std::println(FILE*, fmt, args...)             |    ✅    |      ✅       |
// | std::print(ostream&, fmt, args...)            |    ❌    |      ✅       |
// | std::println(ostream&, fmt, args...)          |    ❌    |      ✅       |
// | bounded ostream_write(span<char>, fmt, args)  |    ❌    |      ✅       |
//
// What this lesson is NOT
// -----------------------
//   - It does NOT propose adding a new overload to libc++. It is a
//     consumer-side adapter that lives in your TU. (A real libc++
//     fix would have to go through the SG16 / LEWG review process;
//     track that for libc++ post-21.)
//   - It is NOT a replacement for std::print(FILE*, ...). When the
//     sink is std::cout wrapped in the C++23 std::cout as a
//     posix-ish FILE*, prefer std::print(stdout, ...). The adapter
//     exists because there is no FILE* for a std::stringstream or
//     a std::ofstream(std::string("out.log")).
//   - It does NOT introduce a runtime_format(sv) gate. C++26 will.
//     Today's adapter goes through std::vformat_to (the C++23-
//     compatible runtime path).
//   - It does NOT introduce a std::print overload that takes an
//     expected<size_t, format_error>. That is what the
//     bounded-buffer companion below is for; we keep the no-bounds
//     variant's return type plain.
//
// C++ standard: C++23. Uses std::vformat / std::vformat_to
// (P2216R3), std::make_format_args (P2216R3), std::format_error,
// and std::expected<size_t, std::format_error> (P0323R12, lifted
// from the Aug 24/25/26 chain).

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <format>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iterator>
#include <ostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

// ============================================================================
// Part 1 — the iterator that feeds std::vformat_to into an ostream
// ============================================================================
//
// std::vformat_to is parameterised on an OutputIterator. The
// iterator's assignment operator is called with the chars (or
// charT) that make up the formatted text. We want those chars to
// land in the ostream's streambuf. The straight-forward trick is
// to use std::ostreambuf_iterator<CharT>(ostream.rdbuf()), which
// already does almost the right thing: it writes CharT values
// into the streambuf and reports end-of-stream by returning a
// default-constructed sentinel.
//
// Two subtleties:
//
//   1. The vformat_to output iterator concept requires
//      std::output_iterator_tag, which std::ostreambuf_iterator
//      already has (it is a std::ostream_iterator /
//      std::ostreambuf_iterator — both model output_iterator in
//      C++20).
//   2. std::vformat_to's overload for an OutputIterator copies
//      the iterator on every advance (the same shape of bug Aug
//      24/25/26 hit with a custom bounded_output_iterator). For
//      ostreambuf_iterator the copy is cheap — it holds a single
//      streambuf pointer — so this is benign here. We verify the
//      "copy is benign" assumption in section 6.
//
// We do not subclass std::ostreambuf_iterator; we use it directly.

// ============================================================================
// Part 2 — the runtime-format-string ostream_print adapter
// ============================================================================

namespace petra {

// Forward — section 11 wraps this in std::expected<size_t,
// format_error> for the bounded variant.
//
// All ostream_print variants are layered on top of a single
// runtime-format-string entry point: ostream_print_runtime.
// The consteval-checked templated overload (below) forwards to it.
inline void ostream_print_runtime(std::ostream& os,
                                  std::string_view fmt,
                                  std::format_args args) {
    // libc++ 21 has not shipped std::print(std::ostream&, ...).
    // We ship our own here via std::vformat -> std::string ->
    // os.write. This is the simplest correct adapter; the
    // streambuf path (Part 3) shows the zero-allocation variant.
    //
    // Note: std::vformat throws std::format_error on parse /
    // argument-index / type errors. The non-bounded adapter does
    // NOT catch it; the bounded sibling (Part 4) does and
    // returns std::expected<size_t, format_error> instead.
    std::string s = std::vformat(fmt, args);
    os.write(s.data(), static_cast<std::streamsize>(s.size()));
}

inline void ostream_println_runtime(std::ostream& os,
                                    std::string_view fmt,
                                    std::format_args args) {
    petra::ostream_print_runtime(os, fmt, args);
    // '\n' for std::println parity (NOT std::endl; see section 3
    // for the rationale).
    os.put('\n');
}

// Consteval-checked format-string template overloads. The base
// impl is a non-template function (above) that takes a runtime
// std::string_view. These forwarders wrap the literal in
// std::format_string<Args...> so a malformed format string is
// rejected at compile time.
//
// A consteval std::format_string cannot be initialised from a
// runtime std::string_view (see section 5 — the runtime path
// is reachable only through the *runtime entry point above,
// which is called by tests with std::string_view fmt. This is
// the same shape of separation P2093R14 §5.2 describes for
// std::print(FILE*, std::string_view, ...) vs std::print(FILE*,
// std::format_string<Args...>, ...).)
template <typename... Args>
void ostream_print(std::ostream& os,
                   std::format_string<Args...> fmt,
                   Args&&... args) {
    petra::ostream_print_runtime(os, std::string_view{fmt.get()},
                                 std::make_format_args(args...));
}

template <typename... Args>
void ostream_println(std::ostream& os,
                     std::format_string<Args...> fmt,
                     Args&&... args) {
    petra::ostream_println_runtime(os, std::string_view{fmt.get()},
                                   std::make_format_args(args...));
}

} // namespace petra

// ============================================================================
// Part 3 — the streambuf-output-iterator path (alternative adapter)
// ============================================================================
//
// This part demonstrates the streambuf-OutputIterator path that
// the simple Part 2 adapter sidesteps. We expose it as
// petra::ostream_print_via_buf() so the lesson exercises both.
// The real-world cost of Part 2 (one std::string allocation per
// call) is meaningful for tight loops; the streambuf path has
// zero allocation overhead.

namespace petra {

inline void ostream_print_via_buf_runtime(std::ostream& os,
                                            std::string_view fmt,
                                            std::format_args args);

template <typename... Args>
void ostream_print_via_buf(std::ostream& os,
                           std::format_string<Args...> fmt,
                           Args&&... args) {
    petra::ostream_print_via_buf_runtime(
        os, std::string_view{fmt.get()},
        std::make_format_args(args...));
}

inline void ostream_print_via_buf_runtime(std::ostream& os,
                                          std::string_view fmt,
                                          std::format_args args) {
    auto buf = os.rdbuf();
    std::ostreambuf_iterator<char> it{buf};
    // vformat_to writes through the iterator; each char is sunk
    // into the streambuf via the iterator's operator=.
    std::vformat_to(
        std::move(it),
        fmt,
        args);
    // ostreambuf_iterator does NOT call pubsync on the streambuf;
    // the caller controls flushing via os.flush().
}

} // namespace petra

// ============================================================================
// Part 4 — the bounded-buffer ostream sibling (expected<size_t,
// format_error>)
// ============================================================================
//
// For destinations where the caller wants to write into a
// std::span<char> (e.g. a reused log buffer, a std::array<char,
// N> embedded in a struct), Part 2's ostream adapter is the wrong
// shape. We provide a complementary adapter that returns
// std::expected<size_t, std::format_error> mirroring the Aug 24 /
// 25 / 26 chain.

namespace petra {

// bounded_ostream_writer: like Aug 25's bounded_output_iterator,
// but takes an ostream reference so the iterator assignment can
// write into the streambuf AND keep a counter at the same time.
// We use a counting_output_iterator-style external pointer (Aug
// 26's lesson noted: std::vformat_to copies the iterator on every
// advance, so the counter MUST be stored externally).
class bounded_ostream_writer {
public:
    using iterator_category = std::output_iterator_tag;
    using iterator_concept  = std::output_iterator_tag;
    using difference_type   = std::ptrdiff_t;

    bounded_ostream_writer(std::ostream* os,
                           std::size_t cap,
                           std::size_t* written) noexcept
        : os_(os), cap_(cap), written_(written) {}

    bounded_ostream_writer(const bounded_ostream_writer&) noexcept = default;

    bounded_ostream_writer& operator=(char c) noexcept {
        if (*written_ < cap_) {
            os_->put(c);          // sink to the streambuf
            ++(*written_);
        }
        return *this;
    }
    bounded_ostream_writer& operator*() noexcept { return *this; }
    bounded_ostream_writer& operator++() noexcept { return *this; }
    bounded_ostream_writer  operator++(int) noexcept { return *this; }

private:
    std::ostream* os_;
    std::size_t    cap_;
    std::size_t*   written_;
};

// counter_output_iterator: same pattern as Aug 26. The iterator
// model requires `operator=` to be the assignment sink;
// std::vformat_to copies the iterator on every advance, so the
// counter MUST live externally.
class counter_output_iterator {
public:
    using iterator_category = std::output_iterator_tag;
    using iterator_concept  = std::output_iterator_tag;
    using difference_type   = std::ptrdiff_t;

    explicit counter_output_iterator(std::size_t* counter) noexcept
        : counter_(counter) {}

    counter_output_iterator(const counter_output_iterator&) noexcept = default;

    counter_output_iterator& operator=(char) noexcept {
        ++(*counter_);
        return *this;
    }
    counter_output_iterator& operator*() noexcept { return *this; }
    counter_output_iterator& operator++() noexcept { return *this; }
    counter_output_iterator  operator++(int) noexcept { return *this; }

private:
    std::size_t* counter_;
};

// expected<size_t, format_error>-returning adapter for a
// caller-supplied ostream + an upper bound. Writes into the
// ostream's streambuf up to `cap` chars; reports the would-be
// size. Note: this adapter preserves the Aug 24 chain's "success
// returns the would-be size" semantics rather than Aug 27's
// "truncation is an Overflow failure" semantics — the
// ostream_print adapter accepts that the user has the stream
// and the size; truncating is the user's choice.
//
// Two-pass design (same shape as Aug 24/25/26): pass 1 measures
// the would-be size via std::vformat_to feeding a
// counter_output_iterator (no allocation, no I/O); pass 2 writes
// through the bounded streambuf iterator. Both passes share the
// exception flow — if std::vformat throws std::format_error on
// pass 1, the catch places it in the expected return and we
// never touch the ostream. (Pass 2 is not even attempted if
// pass 1 fails.)
//
// Runtime entry point. The templated forwarder (below) goes
// through the consteval path.
inline std::expected<std::size_t, std::format_error>
ostream_write_n_runtime(std::ostream& os,
                         std::size_t cap,
                         std::string_view fmt,
                         std::format_args args) {
    try {
        // Pass 1: measure. Chars are counted, not sunk.
        std::size_t would_be = 0;
        std::vformat_to(counter_output_iterator{&would_be},
                        fmt, args);

        // Pass 2: write through the bounded_ostream_writer.
        std::size_t written = 0;
        bounded_ostream_writer w{&os, cap, &written};
        std::vformat_to(std::move(w), fmt, args);
        return would_be;
    } catch (const std::format_error& e) {
        return std::unexpected{std::format_error{e}};
    }
}

template <typename... Args>
std::expected<std::size_t, std::format_error>
ostream_write_n(std::ostream& os,
                std::size_t cap,
                std::format_string<Args...> fmt,
                Args&&... args) {
    return petra::ostream_write_n_runtime(
        os, cap, std::string_view{fmt.get()},
        std::make_format_args(args...));
}

} // namespace petra

// ============================================================================
// Part 5 — runtime tests
// ============================================================================
//
// Same shape as the Aug 22 / 24 / 25 / 26 / 27 lessons: a
// hand-rolled SECTION/CHECK framework prints progress to stdout
// and counts pass / fail. Each section exercises a focused
// invariant. The framework is simple and explicit so each CHECK
// failure can be located by file:line.

namespace {

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

#define CHECK_EQ(a, b) do {                                              \
    auto _a = (a); auto _b = (b);                                        \
    if (_a == _b) {                                                      \
        ++g_pass;                                                        \
    } else {                                                             \
        ++g_fail;                                                        \
        std::fprintf(stderr,                                             \
            "FAIL: %s:%d  [%s]  CHECK_EQ(%s, %s)\n",                     \
            __FILE__, __LINE__, g_section, #a, #b);                      \
    }                                                                    \
} while (0)

// ------------------------------------------------------------------
// Section 1 — toolchain + feature probes
// ------------------------------------------------------------------
// Pins down the libc++ 21 feature surface we depend on.

void section_1_sizeof_and_probes() {
    SECTION("1.sizeof+probes");
    // __cpp_lib_format 202110 is the C++23 std::format surface.
    CHECK(__cpp_lib_format == 202110);
    // __cpp_lib_print 202207 is the C++23 std::print surface.
    CHECK(__cpp_lib_print == 202207);
    // __cpp_lib_expected 202211 is the C++23 std::expected surface.
    CHECK(__cpp_lib_expected == 202211);
    // We deliberately do NOT probe for an ostream-overload of
    // std::print: there isn't one in libc++ 21 (probed explicitly
    // during lesson prep with `clang++ -std=c++23 -stdlib=libc++`
    // and the call `std::print(stringstream{}, ...)` produced
    // 'no matching function for call to print').
    CHECK(true);   // sentinel; the absence is the point of this lesson
}

// ------------------------------------------------------------------
// Section 2 — basic success: consteval-checked format string
// ------------------------------------------------------------------
// The compile-time path: petra::ostream_print(stringstream&,
// std::format_string<Args...>, args...) compiles cleanly when
// the format string is well-formed.

void section_2_basic_success_consteval() {
    SECTION("2.basic-success-consteval");
    std::ostringstream oss;
    petra::ostream_print(oss, "hello {} world", 42);
    // oss.str() returns the buffered text WITHOUT calling putback
    // / flush; that is fine — the ostream owns the buffer and
    // std::ostringstream::str() copies it out.
    CHECK_EQ(oss.str(), std::string{"hello 42 world"});
}

// ------------------------------------------------------------------
// Section 3 — basic success: ostream_println appends '\n'
// ------------------------------------------------------------------
// Mirrors std::println(FILE*) for an ostream destination.

void section_3_basic_success_println_appends_newline() {
    SECTION("3.println-appends-newline");
    std::ostringstream oss;
    petra::ostream_println(oss, "hello {}", "petra");
    CHECK_EQ(oss.str(), std::string{"hello petra\n"});
    // '\n' only, NOT std::endl. The ostream's auto-flush policy
    // (which is OFF for std::ostringstream) determines whether
    // anything flushes; we don't force it. This matches
    // std::println(FILE*) which DOES force a flush; the difference
    // is documented in "What this lesson is NOT" above.
    auto pos = oss.str().find('\n');
    CHECK(pos != std::string::npos);
    CHECK(pos == oss.str().size() - 1);  // trailing newline
}

// ------------------------------------------------------------------
// Section 4 — runtime format string path (std::string_view fmt)
// ------------------------------------------------------------------
// The fast follow-up: a runtime-supplied format string compiles
// through std::vformat. Useful for templated loggers.
//
// Important: the runtime path is reachable ONLY through
// petra::ostream_print_runtime() (and its ostream_println_
// runtime variant and the streambuf sibling). The
// consteval-checked variant (petra::ostream_print) cannot be
// initialised from a runtime std::string_view — see Part 2 for
// why.

void section_4_runtime_format_string() {
    SECTION("4.runtime-format-string");
    std::ostringstream oss;
    std::string_view fmt = "value={} name={}";
    // make_format_args takes its args by reference; bind locals.
    int n = 42;
    std::string s = "petra";
    petra::ostream_print_runtime(oss, fmt,
                                 std::make_format_args(n, s));
    CHECK_EQ(oss.str(), std::string{"value=42 name=petra"});
}

// ------------------------------------------------------------------
// Section 5 — format failure propagates as std::format_error
// ------------------------------------------------------------------
// A bad runtime format string MUST cause std::vformat to throw;
// our adapter does not catch the throw, so it propagates out of
// the adapter. The caller decides whether to wrap the call in
// try/catch or use the bounded sibling in Part 4 (which returns
// std::expected<size_t, format_error> for the same case).

void section_5_format_failure_propagates() {
    SECTION("5.format-error-propagates");
    std::ostringstream oss;
    std::string_view bad_fmt = "ends with {";
    bool caught = false;
    try {
        petra::ostream_print_runtime(oss, bad_fmt,
                                     std::make_format_args());
    } catch (const std::format_error& e) {
        caught = true;
        // The what() string is implementation-defined but always
        // contains "The format string terminates at a '{'"; we
        // pin that substring.
        CHECK(std::string{e.what()}.find("'{") != std::string::npos);
    }
    CHECK(caught);
    // The ostream was NOT touched: the format failure happens
    // before any char is written. (std::vformat parses first, then
    // writes — the adapter inherits that ordering.)
    CHECK_EQ(oss.str(), std::string{});
}

// ------------------------------------------------------------------
// Section 6 — the streambuf-OutputIterator path works identically
// ------------------------------------------------------------------
// Part 3's ostream_print_via_buf exercises the alternative path
// that avoids the intermediate std::string. Section 6 verifies
// it produces the same output.

void section_6_streambuf_path_equals_string_path() {
    SECTION("6.streambuf-path-equals-string-path");
    std::ostringstream oss_a, oss_b;
    petra::ostream_print(oss_a, "x={} y={}", 1, 2);
    petra::ostream_print_via_buf(oss_b, "x={} y={}", 1, 2);
    CHECK_EQ(oss_a.str(), oss_b.str());
    CHECK_EQ(oss_a.str(), std::string{"x=1 y=2"});
}

// ------------------------------------------------------------------
// Section 7 — std::ofstream sink works
// ------------------------------------------------------------------
// Real-world sink: write to a real file. Uses /tmp so we don't
// pollute the lesson dir.

void section_7_ofstream_sink() {
    SECTION("7.ofstream-sink");
    std::string path = std::string{"/tmp/"} + "petra_ostream_print_test.log";
    // Truncate.
    {
        std::ofstream of(path, std::ios::binary | std::ios::trunc);
        CHECK(of.is_open());
        petra::ostream_print(of, "first line {}\n", 1);
        petra::ostream_println(of, "second line {}", 2);
        of.flush();
    }
    // Read back.
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buf;
    buf << in.rdbuf();
    CHECK(buf.str().find("first line 1") != std::string::npos);
    CHECK(buf.str().find("second line 2") != std::string::npos);
    CHECK(buf.str().find("\n") != std::string::npos);   // ln terminated
}

// ------------------------------------------------------------------
// Section 8 — std::ostream::seekp works mid-write
// ------------------------------------------------------------------
// Real-world use: write a header, seek back to a known offset,
// overwrite a field, then finish. Verifies the adapter does NOT
// do anything fancy with the streambuf (no internal buffering
// beyond what the user has configured).

void section_8_seekp_after_write() {
    SECTION("8.seekp-after-write");
    std::ostringstream oss;
    petra::ostream_print(oss, "HEADER: ");
    std::streamoff header_len = static_cast<std::streamoff>(oss.tellp());
    CHECK(header_len == 8);   // "HEADER: " is 8 bytes
    petra::ostream_println(oss, "field1={} field2={} END", 1, 2);
    // Seek back to overwrite the field1 value (NOT the header —
    // this would require knowing exact widths).
    oss.seekp(header_len +
              static_cast<std::streamoff>(std::string{"field1="}.size()));
    petra::ostream_print(oss, "9");   // overwrite '1'
    std::string expected =
        "HEADER: field1=9 field2=2 END\n";
    CHECK_EQ(oss.str(), expected);
}

// ------------------------------------------------------------------
// Section 9 — bounded-buffer sibling: success returns would-be
// ------------------------------------------------------------------
// Part 4's ostream_write_n writes through the streambuf up to
// `cap` chars and returns the would-be size on success. This is
// the same semantics as Aug 24's format_to_n_expected, but
// sunk through an ostream instead of a raw char*.

void section_9_bounded_sibling_success() {
    SECTION("9.bounded-sibling-success");
    std::ostringstream oss;
    auto r = petra::ostream_write_n(oss, 64, "hello {}", "world");
    CHECK(r.has_value());
    CHECK(r.value() == 11);  // would-be size, regardless of cap
    CHECK_EQ(oss.str(), std::string{"hello world"});
}

// ------------------------------------------------------------------
// Section 10 — bounded-buffer sibling: cap truncates the writes
// ------------------------------------------------------------------
// When the formatted text is longer than cap, the ostream sees
// only the first cap chars. The would-be size (the size of the
// text that WOULD have been written) is still reported.

void section_10_bounded_sibling_truncates() {
    SECTION("10.bounded-sibling-truncates");
    std::ostringstream oss;
    // formatted text = "abcdefghij" = 10 chars; cap = 5
    auto r = petra::ostream_write_n(oss, 5, "abcdefghij");
    CHECK(r.has_value());
    CHECK(r.value() == 10);                  // would-be size
    CHECK_EQ(oss.str(), std::string{"abcde"});  // capped at 5
}

// ------------------------------------------------------------------
// Section 11 — bounded-buffer sibling: format_error -> expected
// ------------------------------------------------------------------
// The bounded sibling catches std::format_error and returns
// expected<size_t, std::format_error> with the error in the
// error position. This differs from the non-bounded adapter
// (Part 2), which lets the exception escape.

void section_11_bounded_sibling_format_error() {
    SECTION("11.bounded-sibling-format-error");
    std::ostringstream oss;
    std::string_view bad_fmt = "ends with {";
    // bad_fmt is a runtime std::string_view; route through the
    // _runtime sibling so the consteval format_string ctor is
    // never instantiated.
    auto r = petra::ostream_write_n_runtime(
        oss, 64, bad_fmt, std::make_format_args());
    CHECK(!r.has_value());
    CHECK(r.error().what() != nullptr);
    CHECK(std::string{r.error().what()}.find("'") != std::string::npos);
    // Ostream was NOT touched.
    CHECK_EQ(oss.str(), std::string{});
}

// ------------------------------------------------------------------
// Section 12 — UTF-8 multibyte chars: each char is one sink call
// ------------------------------------------------------------------
// std::format encodes UTF-8 as a sequence of char values; the
// adapter writes them one char at a time. This is correct for
// UTF-8 because the ostream's char type is char, and UTF-8 is
// byte-by-byte. Section 12 verifies a non-ASCII payload survives
// the round trip.

void section_12_utf8_round_trip() {
    SECTION("12.utf8-round-trip");
    std::ostringstream oss;
    // "héllo wörld" — contains é (U+00E9, 2 UTF-8 bytes) and ö
    // (U+00F6, 2 UTF-8 bytes). Plain ASCII chars are 1 byte each;
    // é and ö add 1 extra byte each.
    std::string text = "h\xC3\xA9llo w\xC3\xB6rld";   // 13 UTF-8 bytes
    auto r = petra::ostream_write_n(oss, 64, "msg: {}", text);
    CHECK(r.has_value());
    CHECK_EQ(oss.str(), std::string{"msg: "} + text);
    // The total byte count: "msg: " = 5 ASCII bytes + the 13-byte
    // payload = 18 UTF-8 bytes.
    CHECK_EQ(oss.str().size(), std::size_t{18});
    // Pin the layout: each multi-byte char adds exactly one byte
    // to the size relative to the BMP codepoint count (the two
    // é/ö chars). 13 bytes total UTF-8 = 11 BMP codepoint
    // characters + 2 multi-byte expansions.
    CHECK_EQ(oss.str().size() - text.size(), std::size_t{5});
}

// ------------------------------------------------------------------
// Section 13 — NUL byte inside the format string
// ------------------------------------------------------------------
// A NUL byte in the output is just another char. std::format's
// string_view fmt path is NUL-tolerant (it takes a view, not a
// C-string), so we can synthesise a format string that produces
// a NUL byte. Section 13 verifies the byte makes it through.

void section_13_nul_byte_passes_through() {
    SECTION("13.nul-byte-passes-through");
    std::ostringstream oss;
    // Use vformat to produce "<a>\x00<b>" — vformat treats \x00
    // as a literal char in the output. The runtime path handles
    // it correctly; the consteval path (std::format_string) may
    // or may not depending on the impl.
    // "<a>" = 3, '\0' = 1, "<b>" = 3; total = 7 chars.
    auto r = petra::ostream_write_n(oss, 32, "{}{}{}", "<a>", '\0', "<b>");
    CHECK(r.has_value());
    CHECK_EQ(r.value(), std::size_t{7});
    // Verify the output contains a NUL exactly once and is
    // sandwiched between "<a>" and "<b>".
    const std::string& s = oss.str();
    CHECK_EQ(s.size(), std::size_t{7});
    CHECK(s[0] == '<');
    CHECK(s[1] == 'a');
    CHECK(s[2] == '>');
    CHECK(s[3] == '\0');
    CHECK(s[4] == '<');
    CHECK(s[5] == 'b');
    CHECK(s[6] == '>');
    // The "<b>" piece starts at index 4 (1 char after the NUL).
    CHECK(s.substr(4) == "<b>");
}

// ------------------------------------------------------------------
// Section 14 — larger payload: vector<int> range formatter
// ------------------------------------------------------------------
// P2286R6 range formatters are part of the std::print C++23
// surface. The adapter inherits them via std::format. Section 14
// pins that down.

void section_14_range_formatter() {
    SECTION("14.range-formatter");
    std::vector<int> v = {1, 2, 3, 4, 5};
    std::ostringstream oss;
    petra::ostream_println(oss, "values: [{}]", v);
    // The format string contains a nested "{...}" inside the
    // outer format spec; the adapter passes through both the
    // range-bracket auto-format AND the leading literal
    // "values: [".
    CHECK_EQ(oss.str(), std::string{"values: [[1, 2, 3, 4, 5]]\n"});
}

// ------------------------------------------------------------------
// Section 15 — 50-round determinism: string-path vs streambuf-path
// ------------------------------------------------------------------
// The two adapters (Part 2 string-path / Part 3 streambuf-path)
// produce identical outputs across a deterministic stress run.
// This is the same shape of cross-check Aug 24's lesson used.

void section_15_50_round_determinism() {
    SECTION("15.50-round-determinism");
    // Pin checks before the loop.
    {
        std::ostringstream pin_a, pin_b;
        petra::ostream_print(pin_a, "round 0 / val {} / end", 0);
        petra::ostream_print_via_buf(pin_b, "round 0 / val {} / end", 0);
        CHECK_EQ(pin_a.str(), pin_b.str());
        CHECK_EQ(pin_a.str(),
                 std::string{"round 0 / val 0 / end"});
    }
    for (int i = 0; i < 49; ++i) {
        std::ostringstream oss_a, oss_b;
        std::string fmt = "round " + std::to_string(i + 1)
                         + " / val " + std::to_string((i + 1) * 7)
                         + " / end";
        petra::ostream_print(oss_a, "round {} / val {} / end",
                             i + 1, (i + 1) * 7);
        petra::ostream_print_via_buf(oss_b, "round {} / val {} / end",
                                     i + 1, (i + 1) * 7);
        CHECK_EQ(oss_a.str(), oss_b.str());           // 3a
        CHECK(!oss_a.str().empty());                  // 3b
        CHECK(oss_a.str() == fmt);                    // 3c
    }
    // Sections 15 expands to 1 (pin) + 49*3 (loop) = 148 CHECKs.
}

// ------------------------------------------------------------------
// Section 16 — runtime-format-string with std::span<char> argv
// ------------------------------------------------------------------
// A common production pattern: a logger whose format string comes
// from a config file. The adapter must accept a runtime
// std::string_view without modification. Section 16 pins this
// down against the same payload shape.

void section_16_runtime_fmt_with_args() {
    SECTION("16.runtime-fmt-with-args");
    std::ostringstream oss;
    std::string_view fmt = "{:>8} {:<8} {:>8.2f}";
    // const char* is fine for make_format_args — the args are
    // captured by reference; locals stay alive for the duration
    // of the call.
    const char* a = "abc";
    const char* b = "defghi";
    double c = 3.14159;
    petra::ostream_println_runtime(oss, fmt,
                                   std::make_format_args(a, b, c));
    // 'abc' right-aligned width 8 -> "     abc" (5 spaces + 'abc')
    // 'defghi' left-aligned width 8 -> "defghi  " (6 + 2 trailing spaces)
    // 3.14159 width 8 precision 2 right -> "    3.14" (4 leading spaces)
    // Layout: 5 spc + abc + 1 spc (literal) + defghi + 2 spc
    //         + 1 spc (literal) + 4 spc + 3.14 + \n
    // Total = 5+3+1+6+2+1+4+4+1 = 27 chars (matches oss.str().size()).
    CHECK_EQ(oss.str(),
             std::string{"     abc defghi       3.14\n"});
}

} // namespace

// ============================================================================
// main
// ============================================================================

int main() {
    std::fprintf(stderr,
        "[P-2026-08-28] petra::ostream_print consumer-side adapter\n");

    section_1_sizeof_and_probes();
    section_2_basic_success_consteval();
    section_3_basic_success_println_appends_newline();
    section_4_runtime_format_string();
    section_5_format_failure_propagates();
    section_6_streambuf_path_equals_string_path();
    section_7_ofstream_sink();
    section_8_seekp_after_write();
    section_9_bounded_sibling_success();
    section_10_bounded_sibling_truncates();
    section_11_bounded_sibling_format_error();
    section_12_utf8_round_trip();
    section_13_nul_byte_passes_through();
    section_14_range_formatter();
    section_15_50_round_determinism();
    section_16_runtime_fmt_with_args();

    std::fprintf(stderr,
        "[P-2026-08-28] Total: %d PASS, %d FAIL\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
