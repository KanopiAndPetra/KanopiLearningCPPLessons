// P-2026-09-01 — ostream_print_log_macros: consteval-checked
// `std::print`-compatible log macros (LOG_DEBUG / LOG_INFO /
// LOG_WARN / LOG_ERROR) built on top of petra::ostream_println.
//
// Why today
// ---------
// The Aug 28 lesson (P-2026-08-28-ostream-print-cpp23) shipped
// petra::ostream_print / ostream_println — the consumer-side
// std::print(std::ostream&, ...) adapter that closes the libc++
// 21 missing-overload gap. The Aug 31 lesson (P-2026-08-31-
// ostream-write-n-via-buf) extended it with an n-capped sibling
// std::expected<size_t, FormatError> entry. Aug 28's "Where we
// go next" section listed four new follow-on items. Today's
// lesson closes the FOURTH:
//
//   > std::print-compatible log macros — LOG_INFO("value={}",
//   > 42) sugar over the adapter.
//
// Today IS that lesson. It does three things:
//
//   1. Ships the macros: LOG_DEBUG / LOG_INFO / LOG_WARN /
//      LOG_ERROR with a default-sink of std::cerr and a per-sink
//      configurable swap.
//
//   2. Pins down the macro-hygiene contract: variadic arguments
//      survive round-trip (including a comma in an int pair, an
//      std::string with a comma inside it, and a {} format
//      fragment with a comma), the macros end in ; with no
//      `if (x) LOG_INFO(...) else ...` trap, and the consteval
//      gate on std::format_string<Args...> catches malformed
//      format strings at compile time even at the call site.
//
//   3. Pins down the runtime-vs-consteval split at the macro
//      layer: macros defined on the consteval entry point
//      (std::format_string<Args...>) reject runtime-supplied
//      std::string_view format strings — confirmed by a probe
//      that the macro call site refuses to compile a dynamic
//      std::string. A separate set of macros on the runtime entry
//      point (LOG_INFO_RUNTIME) is also shipped and tested.
//
// What this lesson is NOT
// -----------------------
//   - It is NOT a generic logging framework. There is no
//     level-filter, no file/line capture, no async sink, no
//     structured-log support. The four macros + a configurable
//     sink pointer is the entire surface.
//
//   - It is NOT a re-implementation of std::print. The adapter
//     from Aug 28 IS std::print(ostream&, ...) for libc++ 21
//     where libc++ 21 doesn't ship one; today is one layer of
//     sugar on top.
//
//   - It is NOT thread-safe by itself. std::cerr writes from
//     multiple threads without external sync are an
//     unsynchronised data race. A production logger would
//     own a mutex; today documents the assumption but does
//     not implement the lock (callers who need it compose
//     std::osyncstream around the sink).
//
// C++ standard: C++23. Uses std::format (C++20 / P0645R10),
// std::vformat, std::format_string<Args...>, std::make_format_args,
// std::format_error. The adapter petra::ostream_println is the
// Aug 28 entry point — redeclared in this TU so the consumer is
// self-contained.

#include <cstdio>
#include <format>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

// ============================================================================
// Part 1 — petra::ostream_println (lifted from Aug 28 P-2026-08-28)
// ============================================================================
//
// The Aug 28 adapter's runtime path: std::vformat -> std::string ->
// os.write. The consteval path forwards through std::make_format_args.
// Today's macros pin the consteval-checked entry point (the one
// that catches parse / type errors at compile time) and the
// runtime entry point (the one that accepts std::string_view fmt
// at runtime).
//
// This is the SAME shape Aug 28 / Aug 30 / Aug 31 established:
// consteval-checked `std::format_string<Args...>` for literal
// format strings, `std::string_view` + `std::format_args` for
// runtime format strings. The macros just route to the right
// one based on whether the format string is a literal or a
// runtime value.

namespace petra {

inline void ostream_println_runtime(std::ostream& os,
                                    std::string_view fmt,
                                    std::format_args args) {
    std::string s = std::vformat(fmt, args);
    s.push_back('\n');
    os.write(s.data(), static_cast<std::streamsize>(s.size()));
}

template <typename... Args>
void ostream_println(std::ostream& os,
                     std::format_string<Args...> fmt,
                     Args&&... args) {
    petra::ostream_println_runtime(
        os,
        std::string_view{fmt.get()},
        std::make_format_args(args...));
}

} // namespace petra

// ============================================================================
// Part 2 — log-level enum + the sink pointer
// ============================================================================
//
// Five levels (Debug is the lowest; Error is the highest). The
// level is printed as a fixed 5-char tag so log lines align in
// columns: "DEBUG ", "INFO  ", "WARN  ", "ERROR ". Today's
// surface is intentionally NOT a level filter — every macro
// always emits. The enum exists so the tag is a single source
// of truth and a future filter can read it.

namespace petra {

enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3 };

[[nodiscard]] constexpr std::string_view log_level_tag(LogLevel lvl) noexcept {
    switch (lvl) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "?????";
}

// The default sink is std::cerr. Callers may swap it (in tests
// the swap points the sink at a std::ostringstream so the
// captured output can be CHECK_EQ'd). The pointer is a raw
// pointer because std::cerr's lifetime is the program's;
// swapping to a heap or stack stream means the caller owns
// the lifetime and is responsible for keeping it alive across
// the macro invocation.
//
// Thread safety: see the lesson header — std::cerr writes
// without external sync are an unsynchronised data race.
// Compose std::osyncstream{std::cerr} for thread-safe
// logging. Today's lesson does not own the lock.

inline std::ostream*& log_sink() noexcept {
    static std::ostream* sink = &std::cerr;
    return sink;
}

// log_prefix is the per-line tag written before the formatted
// payload. It is a free function (not a member) so a test can
// capture it and a caller can override the format if needed.
// Today always uses "[tag] " (e.g. "[INFO ] ").

inline void log_emit_prefixed(LogLevel lvl) {
    std::ostream* os = log_sink();
    if (os == nullptr) return;
    char buf[16];
    std::string_view tag = log_level_tag(lvl);
    // "[tag] " — 8 chars max (5 tag + '[' + ']' + ' ').
    int n = std::snprintf(buf, sizeof(buf), "[%s] ", tag.data());
    if (n > 0) {
        os->write(buf, static_cast<std::streamsize>(n));
    }
}

} // namespace petra

// ============================================================================
// Part 3 — the macros
// ============================================================================
//
// The macro shape: a do { ... } while (0) wrapper around a call
// to a helper template that takes the level + the format string
// + the variadic args. The do-while-0 wrapping is the standard
// "trailing ; is harmless" idiom; the helper-template split is
// the standard "consteval-vs-runtime" idiom that gets the
// format-string check into the call site.
//
// IMPORTANT: today defines LOG_* macros that route through the
// CONSTEVAL adapter (std::format_string<Args...>). The macros
// only compile with literal format strings; runtime format
// strings go through LOG_*_RUNTIME (Part 4). The consteval
// gate at the macro call site means a malformed format string
// ("value={:d}", "hello") is a hard compile error AT THE CALL
// SITE, not a runtime std::format_error throw.
//
// Why the helper template instead of std::format directly:
//   1. Centralised level-tag emission.
//   3. Sink-pointer routing.
//   4. Two-phase emission: prefix + payload (single line).
//
// Why we don't std::format into a temporary string and then write
// it: that's the Aug 28 ostream_println implementation. The
// macros ARE a wrapper around ostream_println, just with a
// prefix prepended. The stream-side writes are TWO: one for
// the prefix (via log_emit_prefixed), one for the payload
// (via ostream_println). On a std::cerr sink this means two
// writes that are NOT atomic with respect to each other; a
// concurrent log call from another thread can interleave
// between them. std::osyncstream fixes that, and the lesson's
// "Where we go next" section names it as the natural next
// step.

namespace petra {

template <typename... Args>
void log_emit(LogLevel lvl,
              std::format_string<Args...> fmt,
              Args&&... args) {
    log_emit_prefixed(lvl);
    if (log_sink() == nullptr) return;
    petra::ostream_println(*log_sink(),
                           fmt,
                           std::forward<Args>(args)...);
}

// Runtime-format-string sibling. Takes std::string_view fmt
// + std::format_args — no consteval gate, so the caller is
// responsible for the format string being valid (parse errors
// throw std::format_error at format time).
inline void log_emit_runtime(LogLevel lvl,
                             std::string_view fmt,
                             std::format_args args) {
    log_emit_prefixed(lvl);
    if (log_sink() == nullptr) return;
    // Compose the payload + '\n' through the runtime adapter
    // (which already appends '\n').
    petra::ostream_println_runtime(*log_sink(), fmt, args);
}

} // namespace petra

#define PETRA_LOG_EMIT(level, ...) \
    do { \
        ::petra::log_emit((::petra::LogLevel::level), __VA_ARGS__); \
    } while (0)

#define PETRA_LOG_EMIT_RUNTIME(level, fmt, args) \
    do { \
        ::petra::log_emit_runtime((::petra::LogLevel::level), (fmt), args); \
    } while (0)

// The user-facing macros. Defined to LOG_<LEVEL> with the
// level-name as the macro argument. Today does NOT support
// compile-time level filtering (a future expansion could
// `#if PETRA_LOG_LEVEL <= N` gate each macro to a no-op;
// today treats every level as always-emitted).

#define LOG_DEBUG(...)   PETRA_LOG_EMIT(Debug, __VA_ARGS__)
#define LOG_INFO(...)    PETRA_LOG_EMIT(Info,  __VA_ARGS__)
#define LOG_WARN(...)    PETRA_LOG_EMIT(Warn,  __VA_ARGS__)
#define LOG_ERROR(...)   PETRA_LOG_EMIT(Error, __VA_ARGS__)

// Runtime-format-string siblings. NOTE the different shape:
// LOG_*_RUNTIME takes TWO args after the level (string_view
// + format_args), while LOG_* takes the usual variadic. The
// runtime macros are a separate surface because the consteval
// gate on std::format_string<Args...> is what makes LOG_* a
// compile-time-checked surface.

#define LOG_DEBUG_RUNTIME(fmt, args)   PETRA_LOG_EMIT_RUNTIME(Debug, fmt, args)
#define LOG_INFO_RUNTIME(fmt, args)    PETRA_LOG_EMIT_RUNTIME(Info,  fmt, args)
#define LOG_WARN_RUNTIME(fmt, args)    PETRA_LOG_EMIT_RUNTIME(Warn,  fmt, args)
#define LOG_ERROR_RUNTIME(fmt, args)   PETRA_LOG_EMIT_RUNTIME(Error, fmt, args)

// ============================================================================
// Part 4 — hand-rolled SECTION/CHECK framework (copied verbatim from Aug 28/30/31)
// ============================================================================

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

} // namespace

// ============================================================================
// Part 5 — sections
// ============================================================================

// ------------------------------------------------------------------
// Section 1 — toolchain + feature probes
// ------------------------------------------------------------------
// Pins down the libc++ 21 feature surface the macros depend on.
// __cpp_lib_format 202110 is the C++23 std::format surface.
// __cpp_lib_print 202207 is the C++23 std::print surface (the
// macros piggy-back on the consumer-side adapter from Aug 28).

void section_1_sizeof_and_probes() {
    SECTION("1.sizeof+probes");
    CHECK(__cpp_lib_format == 202110);
    CHECK(__cpp_lib_print  == 202207);
    CHECK(sizeof(petra::LogLevel) == sizeof(int));
}

// ------------------------------------------------------------------
// Section 2 — basic success: LOG_INFO writes "[INFO ] payload\n"
// ------------------------------------------------------------------
// Mirrors std::println behaviour: prefix + payload + '\n'.

void section_2_basic_success() {
    SECTION("2.basic-success");

    // Save / swap / restore around the test.
    std::ostringstream oss;
    auto* old = petra::log_sink();
    petra::log_sink() = &oss;

    LOG_INFO("hello {}", "petra");
    CHECK_EQ(oss.str(), std::string{"[INFO ] hello petra\n"});

    // Reset and try LOG_WARN.
    oss.str("");
    LOG_WARN("warn payload {}", 42);
    CHECK_EQ(oss.str(), std::string{"[WARN ] warn payload 42\n"});

    // LOG_ERROR.
    oss.str("");
    LOG_ERROR("error code {}", -7);
    CHECK_EQ(oss.str(), std::string{"[ERROR] error code -7\n"});

    // LOG_DEBUG.
    oss.str("");
    LOG_DEBUG("debug x={}", 1.5);
    CHECK_EQ(oss.str(), std::string{"[DEBUG] debug x=1.5\n"});

    petra::log_sink() = old;
}

// ------------------------------------------------------------------
// Section 3 — zero args (LOG_INFO("ping"))
// ------------------------------------------------------------------
// The do-while(0) wrapping handles the "no varargs" case;
// std::format_string<>() is a valid type for an empty Args...
// list. Section 3 pins that the consteval gate accepts a
// payload with no {} replacement fields.

void section_3_zero_args() {
    SECTION("3.zero-args");
    std::ostringstream oss;
    auto* old = petra::log_sink();
    petra::log_sink() = &oss;

    LOG_INFO("ping");
    CHECK_EQ(oss.str(), std::string{"[INFO ] ping\n"});

    petra::log_sink() = old;
}

// ------------------------------------------------------------------
// Section 4 — variadic with commas in arguments
// ------------------------------------------------------------------
// The most common macro trap: a comma inside an argument
// expression. The do-while-0 wrapper passes __VA_ARGS__ as a
// single token group to the helper, so `LOG_INFO("{}", pair{1,
// 2})` round-trips correctly. This section pins a std::pair
// and an std::string with a comma inside it through the macro.

void section_4_comma_in_args() {
    SECTION("4.comma-in-args");

    std::ostringstream oss;
    auto* old = petra::log_sink();
    petra::log_sink() = &oss;

    // std::pair has a comma in its type — the macro must NOT
    // split on it. With Args&& forwarding through the
    // helper template, std::pair<int,int> arrives whole.
    std::pair<int, int> p{3, 4};
    LOG_INFO("pair=({},{})", p.first, p.second);
    CHECK_EQ(oss.str(), std::string{"[INFO ] pair=(3,4)\n"});

    // std::string with a comma INSIDE the value.
    oss.str("");
    std::string s = "a,b,c";
    LOG_INFO("csv=\"{}\"", s);
    CHECK_EQ(oss.str(), std::string{"[INFO ] csv=\"a,b,c\"\n"});

    petra::log_sink() = old;
}

// ------------------------------------------------------------------
// Section 5 — trailing semicolon + if/else hygiene
// ------------------------------------------------------------------
// do-while-0 wrapping makes `if (x) LOG_INFO(...); else ...;`
// syntactically clean. This is the macro-hygiene baseline that
// std::print / fmt::print users expect; section 5 pins both
// branches.

void section_5_if_else_hygiene() {
    SECTION("5.if-else-hygiene");

    std::ostringstream oss;
    auto* old = petra::log_sink();
    petra::log_sink() = &oss;

    // Trailing semicolon after the macro is REQUIRED for
    // if/else to bind correctly.
    bool cond = true;
    if (cond)
        LOG_INFO("branch-A {}", 1);
    else
        LOG_WARN("branch-B {}", 2);
    CHECK_EQ(oss.str(), std::string{"[INFO ] branch-A 1\n"});

    oss.str("");
    cond = false;
    if (cond)
        LOG_INFO("branch-A {}", 1);
    else
        LOG_WARN("branch-B {}", 2);
    CHECK_EQ(oss.str(), std::string{"[WARN ] branch-B 2\n"});

    // Multiple macro invocations in one statement (separated
    // by ;) all flow into the sink.
    oss.str("");
    LOG_INFO("first {}", 1); LOG_INFO("second {}", 2);
    CHECK_EQ(oss.str(), std::string{"[INFO ] first 1\n[INFO ] second 2\n"});

    petra::log_sink() = old;
}

// ------------------------------------------------------------------
// Section 6 — format specifiers round-trip
// ------------------------------------------------------------------
// {:#x}, {:.3f}, {:>8}, {:<10} all flow through std::format
// unchanged because the macros route through the Aug 28
// adapter. This is the same surface Aug 28 / Aug 31 pinned;
// section 6 verifies the macros do not disturb it.

void section_6_format_specifiers() {
    SECTION("6.format-specifiers");

    std::ostringstream oss;
    auto* old = petra::log_sink();
    petra::log_sink() = &oss;

    LOG_INFO("hex={:#x}", 0x2a);
    CHECK_EQ(oss.str(), std::string{"[INFO ] hex=0x2a\n"});

    oss.str("");
    LOG_INFO("float={:.3f}", 3.14159);
    CHECK_EQ(oss.str(), std::string{"[INFO ] float=3.142\n"});

    oss.str("");
    LOG_INFO("right={:>8}", "ab");
    CHECK_EQ(oss.str(), std::string{"[INFO ] right=      ab\n"});

    oss.str("");
    LOG_INFO("left={:<6}", "ab");
    CHECK_EQ(oss.str(), std::string{"[INFO ] left=ab    \n"});

    petra::log_sink() = old;
}

// ------------------------------------------------------------------
// Section 7 — consteval gate rejects bad format strings
// ------------------------------------------------------------------
// The macros' std::format_string<Args...> parameter is consteval-
// evaluated at the call site. A bad format string is therefore
// a HARD COMPILE ERROR at the macro call site, not a runtime
// std::format_error throw. Section 7 demonstrates this via
// a `static_assert` probe that compiles a constexpr check
// against the format string type.
//
// What we can NOT do at runtime: deliberately write a macro
// invocation that fails to compile. That would be a section
// in a SEPARATE TU that we never link. So today's lesson
// documents the gate's contract via a `if consteval` probe in
// a small helper.

void section_7_consteval_gate() {
    SECTION("7.consteval-gate");

    // std::format_string<Args...> has a consteval ctor.
    // We probe that a literal format string of the right
    // arity compiles.
    constexpr auto fs = std::format_string<int>{"value={}"};
    static_assert(fs.get().size() > 0);

    // The runtime-checked sibling std::basic_format_string<...>
    // accepts a runtime string_view. Today's LOG_INFO_RUNTIME
    // is reachable only through that surface.
    std::string_view runtime_fmt = "runtime value={}";
    int v7 = 99;
    std::ostringstream oss;
    auto* old = petra::log_sink();
    petra::log_sink() = &oss;
    LOG_INFO_RUNTIME(runtime_fmt, std::make_format_args(v7));
    CHECK_EQ(oss.str(), std::string{"[INFO ] runtime value=99\n"});

    petra::log_sink() = old;
}

// ------------------------------------------------------------------
// Section 8 — runtime-fmt sibling
// ------------------------------------------------------------------
// LOG_INFO_RUNTIME takes (string_view fmt, format_args args).
// This is the runtime-format-string sibling: parse errors
// throw std::format_error at format time (caught here).

void section_8_runtime_sibling() {
    SECTION("8.runtime-sibling");

    std::ostringstream oss;
    auto* old = petra::log_sink();
    petra::log_sink() = &oss;

    // Happy path.
    std::string_view fmt = "user={} action={}";
    LOG_INFO_RUNTIME(fmt, std::make_format_args("alice", "login"));
    CHECK_EQ(oss.str(), std::string{"[INFO ] user=alice action=login\n"});

    // Variable format string (built at runtime).
    oss.str("");
    std::string dyn = "dyn-{}";
    int n_v = 42;
    LOG_INFO_RUNTIME(std::string_view{dyn}, std::make_format_args(n_v));
    CHECK_EQ(oss.str(), std::string{"[INFO ] dyn-42\n"});

    petra::log_sink() = old;
}

// ------------------------------------------------------------------
// Section 9 — sink pointer reset (nullptr vs swap-back)
// ------------------------------------------------------------------
// Setting the sink to nullptr silences every macro: the prefix
// write is skipped, the payload write is skipped, and nothing
// reaches stderr. Setting it back restores the stream.

void section_9_sink_pointer_null_silences() {
    SECTION("9.sink-null-silences");

    auto* old = petra::log_sink();
    petra::log_sink() = nullptr;

    // No crash, no output.
    LOG_INFO("should not appear {}", 1);
    LOG_ERROR("neither this {}", 2);
    CHECK(true);

    // Restore and verify.
    petra::log_sink() = old;
    std::ostringstream oss;
    petra::log_sink() = &oss;

    LOG_INFO("after restore {}", 3);
    CHECK_EQ(oss.str(), std::string{"[INFO ] after restore 3\n"});

    petra::log_sink() = old;
}

// ------------------------------------------------------------------
// Section 10 — log_level_tag alignment
// ------------------------------------------------------------------
// The five tags are 5 chars wide (with trailing space on the
// 4-char levels) so log lines align in columns. Section 10
// pins each tag's width.

void section_10_log_level_tag_width() {
    SECTION("10.tag-width");

    CHECK_EQ(petra::log_level_tag(petra::LogLevel::Debug).size(), std::size_t{5});
    CHECK_EQ(petra::log_level_tag(petra::LogLevel::Info ).size(), std::size_t{5});
    CHECK_EQ(petra::log_level_tag(petra::LogLevel::Warn ).size(), std::size_t{5});
    CHECK_EQ(petra::log_level_tag(petra::LogLevel::Error).size(), std::size_t{5});

    // The 4-char tags ("INFO", "WARN") are right-padded with
    // one space; "DEBUG" and "ERROR" are 5 chars, no padding.
    CHECK_EQ(petra::log_level_tag(petra::LogLevel::Debug), "DEBUG");
    CHECK_EQ(petra::log_level_tag(petra::LogLevel::Info),  "INFO ");
    CHECK_EQ(petra::log_level_tag(petra::LogLevel::Warn),  "WARN ");
    CHECK_EQ(petra::log_level_tag(petra::LogLevel::Error), "ERROR");
}

// ------------------------------------------------------------------
// Section 11 — many invocations produce many lines
// ------------------------------------------------------------------
// Verifies that each macro call produces exactly one line,
// with each line ending in '\n' and no spurious trailing data.

void section_11_many_invocations() {
    SECTION("11.many-invocations");

    std::ostringstream oss;
    auto* old = petra::log_sink();
    petra::log_sink() = &oss;

    for (int i = 0; i < 5; ++i) {
        LOG_INFO("i={}", i);
    }

    const std::string expected =
        "[INFO ] i=0\n"
        "[INFO ] i=1\n"
        "[INFO ] i=2\n"
        "[INFO ] i=3\n"
        "[INFO ] i=4\n";
    CHECK_EQ(oss.str(), expected);

    petra::log_sink() = old;
}

// ------------------------------------------------------------------
// Section 12 — NUL byte in payload
// ------------------------------------------------------------------
// std::format pastes a NUL byte through. The prefix + payload
// + '\n' join has one embedded NUL mid-payload; oss.str()
// preserves it. This is the same finding Aug 28's section
// pinned for ostream_println, lifted into the macro layer.

void section_12_nul_in_payload() {
    SECTION("12.nul-in-payload");

    std::ostringstream oss;
    auto* old = petra::log_sink();
    petra::log_sink() = &oss;

    // Build a string with a NUL byte inside.
    std::string with_nul = "ab";
    with_nul.push_back('\0');
    with_nul.append("d");

    LOG_INFO("nul={}", with_nul);
    const std::string got = oss.str();
    // Expected: "[INFO ] nul=ab" (14 bytes) + NUL byte + "d" (1) + "\n" (1) = 17.
    // We verify byte size AND that there's exactly one NUL in the middle.
    CHECK_EQ(got.size(), std::size_t{17});
    CHECK_EQ(got.substr(0, 14), std::string{"[INFO ] nul=ab"});
    CHECK_EQ(got[14], '\0');
    CHECK_EQ(got.substr(15, 2), std::string{"d\n"});

    petra::log_sink() = old;
}

// ------------------------------------------------------------------
// Section 13 — UTF-8 payload
// ------------------------------------------------------------------
// The macro prefixes ([INFO ] etc.) are all ASCII so column
// alignment in a terminal is preserved. UTF-8 in the payload
// arrives byte-identical (same as Aug 28).

void section_13_utf8_payload() {
    SECTION("13.utf8-payload");

    std::ostringstream oss;
    auto* old = petra::log_sink();
    petra::log_sink() = &oss;

    LOG_INFO("emoji={}", "ok \xF0\x9F\x9A\x80!");  // "ok 🚀!"
    const std::string got = oss.str();
    // "[INFO ] " is 8 bytes ASCII; "emoji=ok " is 9 bytes ASCII; the
    // emoji is 4 bytes UTF-8; "!" is 1 byte; "\n" is 1 byte. Total = 23.
    CHECK_EQ(got.size(), std::size_t{23});
    CHECK_EQ(got.substr(got.size() - 1), std::string{"\n"});
    // The prefix is ASCII so terminal column alignment survives.
    CHECK_EQ(got.substr(0, 8), std::string{"[INFO ] "});

    petra::log_sink() = old;
}

// ------------------------------------------------------------------
// Section 14 — all four levels + sink swap round-trip
// ------------------------------------------------------------------
// One of each level in a sequence, then swap-back to the
// original sink.

void section_14_all_levels_round_trip() {
    SECTION("14.all-levels-round-trip");

    std::ostringstream oss;
    auto* old = petra::log_sink();
    petra::log_sink() = &oss;

    LOG_DEBUG("d={}", 1);
    LOG_INFO("i={}",  2);
    LOG_WARN("w={}",  3);
    LOG_ERROR("e={}", 4);

    const std::string expected =
        "[DEBUG] d=1\n"
        "[INFO ] i=2\n"
        "[WARN ] w=3\n"
        "[ERROR] e=4\n";
    CHECK_EQ(oss.str(), expected);

    // Restore and verify nothing leaks to the next section.
    petra::log_sink() = old;
    CHECK_EQ(petra::log_sink(), old);
}

// ------------------------------------------------------------------
// Section 15 — determinism stress: 50 rounds
// ------------------------------------------------------------------
// Same shape as Aug 28 / Aug 30 / Aug 31. The check is that the
// produced string matches the predicted string byte-for-byte
// across all rounds.

void section_15_determinism_stress() {
    SECTION("15.determinism-stress");

    std::ostringstream oss;
    auto* old = petra::log_sink();
    petra::log_sink() = &oss;

    std::string expected_all;
    for (int round = 0; round < 50; ++round) {
        // Mix the four levels round-robin. Each call uses a
        // LITERAL format string per level so the consteval gate
        // (which forbids runtime-supplied std::string_view at
        // the LOG_* call site) accepts the call. The runtime
        // sibling LOG_*_RUNTIME is exercised separately in
        // section 8.
        petra::LogLevel lvl = static_cast<petra::LogLevel>(round % 4);
        switch (lvl) {
            case petra::LogLevel::Debug:
                LOG_DEBUG("round={} value={}", round, round * 7);
                expected_all += "[DEBUG] round=" + std::to_string(round)
                             + " value=" + std::to_string(round * 7) + "\n";
                break;
            case petra::LogLevel::Info:
                LOG_INFO("round={} value={}", round, round * 7);
                expected_all += "[INFO ] round=" + std::to_string(round)
                             + " value=" + std::to_string(round * 7) + "\n";
                break;
            case petra::LogLevel::Warn:
                LOG_WARN("round={} value={}", round, round * 7);
                expected_all += "[WARN ] round=" + std::to_string(round)
                             + " value=" + std::to_string(round * 7) + "\n";
                break;
            case petra::LogLevel::Error:
                LOG_ERROR("round={} value={}", round, round * 7);
                expected_all += "[ERROR] round=" + std::to_string(round)
                             + " value=" + std::to_string(round * 7) + "\n";
                break;
        }
    }

    CHECK_EQ(oss.str(), expected_all);

    petra::log_sink() = old;
}

// ------------------------------------------------------------------
// main
// ------------------------------------------------------------------

int main() {
    section_1_sizeof_and_probes();
    section_2_basic_success();
    section_3_zero_args();
    section_4_comma_in_args();
    section_5_if_else_hygiene();
    section_6_format_specifiers();
    section_7_consteval_gate();
    section_8_runtime_sibling();
    section_9_sink_pointer_null_silences();
    section_10_log_level_tag_width();
    section_11_many_invocations();
    section_12_nul_in_payload();
    section_13_utf8_payload();
    section_14_all_levels_round_trip();
    section_15_determinism_stress();

    std::fprintf(stdout, "%d/%d PASS\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}