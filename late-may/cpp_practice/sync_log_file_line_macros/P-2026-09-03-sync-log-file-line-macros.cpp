// P-2026-09-03 — sync_log_file_line_macros: __FILE__ / __LINE__
// capture for the Sep 2 SYNC_LOG_* macros, preserving the
// atomic-line property.
//
// Why today
// ---------
// The Sep 2 lesson (P-2026-09-02-osyncstream-log-sink) shipped
// std::osyncstream-backed SYNC_LOG_DEBUG/INFO/WARN/ERROR macros
// closing the FIRST of three new forward-on items Sep 1's "Where
// we go next" section named.  Sep 2 named the SECOND:
//
//   > __FILE__ / __LINE__ capture.  A production logger prepends
//   > __FILE__:__LINE__ to each line.  The SYNC macros can be
//   > retemplated to take a hidden const char* and int from
//   > __FILE__ / __LINE__; today's surface keeps the macros minimal.
//
// Today IS that lesson.  It does four things:
//
//   1. Mirrors the Sep 2 surface in this TU (Parts 1-4 lifted
//      verbatim): petra::LogLevel enum, petra::log_level_tag
//      (5-char aligned tags), petra::log_sink()& (raw, default
//      &std::cerr), petra::ostream_println (Aug 28 / Sep 1
//      adapter), petra::sync_sink_target()& (the
//      reference_wrapper<std::ostream>), petra::sync_log_emit /
//      sync_log_emit_runtime (the two-helper consteval-vs-runtime
//      split), and the Sep 2 SYNC_LOG_* macros.
//
//   2. Adds petra::sync_log_emit_loc / sync_log_emit_loc_runtime
//      — the file-and-line variant of the Sep 2 helpers.  Both
//      emit the prefix `[LEVEL ] file:line ` AND the payload
//      through the SAME std::osyncstream so the committed line
//      remains atomic w.r.t. concurrent callers.  The full line
//      shape is:
//
//          [<LEVEL>] <file>:<line> <payload>\n
//
//      where <file> is a path (the literal __FILE__ passed in)
//      and <line> is the integer literal __LINE__.  The level
//      tag keeps the 5-char Sep 1 / Sep 2 alignment ("INFO "
//      has a trailing space so all four levels column-align).
//
//   3. Adds LOCSYNC_LOG_DEBUG / LOCSYNC_LOG_INFO /
//      LOCSYNC_LOG_WARN / LOCSYNC_LOG_ERROR (consteval path)
//      plus LOCSYNC_LOG_*_RUNTIME siblings.  Both expand through
//      a `do { ... } while (0)` wrapper so if/else hygiene is
//      preserved.  The macros pass __FILE__ and __LINE__
//      automatically; the call site never names them.
//
//   4. Verifies the headline property the lesson exists to pin:
//      the full line `[LEVEL ] file:line payload\n` is committed
//      atomically even under concurrent load.  Section 11 runs
//      4 threads / 250 commits / 1000 total commits and asserts
//      every committed line matches the regex
//      ^\[(INFO |WARN )\] .+:[0-9]+ t[0-3]=\d+$.  Section 13
//      widens to 8 threads / 200 commits / 1600 total commits.
//      Section 15 runs 5x determinism.
//
// Why the LOC macros don't make the file:line the whole point
// ------------------------------------------------------------
// The atomic-line property is the property the lesson inherits
// from Sep 2.  Adding file:line to the prefix DOES NOT change
// the property: the file:line, the level tag, the payload, and
// the trailing \n all route through the SAME std::osyncstream
// and the syncbuf commits them as one indivisible write to the
// underlying std::ostream.  The `LOCSYNC_*` prefix is just a
// reminder at the call site that the line carries location
// info; the runtime guarantees are identical to Sep 2.
//
// Why std::osyncstream and not a mutex
// ------------------------------------
// std::osyncstream (P0053R7 in C++20) batches writes through a
// per-thread syncbuf and flushes them atomically when the
// osyncstream is destroyed (or std::flush is called).  A mutex
// around the whole line would also work but produces a single
// global lock and serialises ALL threads; std::osyncstream
// parallelises the BUFFERING and serialises only the COMMIT
// (each flushed line is one indivisible write to the underlying
// std::ostream).  This is the right shape for a logging sink.
//
// -----------------------------------------------------------------------------
// Platform note: enabling std::osyncstream on Apple Clang 21.0.0 / libc++ 21
// -----------------------------------------------------------------------------
// On this toolchain (cloned from the Sep 1 / Sep 2 preflight probes),
// the <syncstream> header is shipped but its `std::osyncstream`
// typedef is gated behind `_LIBCPP_HAS_EXPERIMENTAL_SYNCSTREAM`:
// in libc++ 21 the test is `#if _LIBCPP_STD_VER >= 20 &&
// _LIBCPP_HAS_EXPERIMENTAL_SYNCSTREAM`, and the latter defaults
// to 0.  Without a flag, `std::osyncstream` is NOT defined and
// the program fails to compile with "no type named 'osyncstream'
// in namespace 'std'".
//
// The enabled path is `clang++ -fexperimental-library`, which
// flips `__has_feature(experimental_library)` and is the same
// flag libc++'s experimental-library guide lists for PSTL and
// TZDB.  Today's CMakeLists.txt (and the direct compile
// commands in the lesson) all pass `-fexperimental-library`.
// (Sep 2 lesson's headline-flavoured finding; replicated here
// only as the build recipe, not as a finding — Sep 2 owns it.)
//
// Part 1 — petra::ostream_println (lifted verbatim from Sep 2)
// Part 2 — petra::LogLevel, petra::log_level_tag, petra::log_sink
// Part 3 — petra::sync_sink_target (the wrapped underlying sink)
// Part 4 — SYNC_LOG_* macros + sync_log_emit helpers (lifted)
// Part 5 — petra::sync_log_emit_loc / sync_log_emit_loc_runtime
// Part 6 — LOCSYNC_LOG_* macros (file/line-aware)
// Sections 1-16 — correctness verification

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <functional>
#include <iostream>
#include <iterator>
#include <mutex>
#include <ostream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <syncstream>
#include <thread>
#include <utility>
#include <vector>

// =====================================================================
// Part 1 — petra::ostream_println (lifted verbatim from Sep 2)
// =====================================================================

namespace petra {

inline void ostream_println_runtime(std::ostream& os,
                                    std::string_view fmt,
                                    std::format_args args) {
    // Two-step: vformat -> std::string -> os.write, exactly as
    // Aug 28 / Sep 1 / Sep 2 ship.
    std::string s = std::vformat(fmt, args);
    os.write(s.data(), static_cast<std::streamsize>(s.size()));
    os.put('\n');
}

template <typename... Args>
void ostream_println(std::ostream& os,
                     std::format_string<Args...> fmt,
                     Args&&... args) {
    // Consteval-checked entry — std::format_string<Args...> ctor
    // runs at compile time.  Forwards to the runtime sibling with
    // std::make_format_args.  std::make_format_args takes lvalue
    // refs to the args (Args&...), so we pass `args...` directly
    // inside the function body where the parameter pack names are
    // lvalues (Sep 1 lesson's recipe).
    petra::ostream_println_runtime(
        os, fmt.get(),
        std::make_format_args(args...));
}

} // namespace petra

// =====================================================================
// Part 2 — petra::LogLevel, petra::log_level_tag, petra::log_sink
// (lifted from Sep 1 / Sep 2)
// =====================================================================

namespace petra {

enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3 };

// 5-char aligned tag so log lines align in columns.  The three
// 4-char levels get a trailing space.
[[nodiscard]] constexpr std::string_view log_level_tag(LogLevel lvl) noexcept {
    switch (lvl) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "?????";
}

// Function-static raw sink pointer.  Default is &std::cerr.
// Setting it to nullptr silences every macro (Sep 1 Section 9).
inline std::ostream*& log_sink() noexcept {
    static std::ostream* p = &std::cerr;
    return p;
}

// Helper used by the raw LOG_* macros, lifted from Sep 1.  In
// the sync version we emit the prefix and the payload through
// the SAME std::osyncstream so they commit as one atomic line.
inline void log_emit_prefixed(LogLevel lvl) {
    std::ostream* os = petra::log_sink();
    if (os == nullptr) return;
    os->write("[", 1);
    os->write(petra::log_level_tag(lvl).data(),
              static_cast<std::streamsize>(petra::log_level_tag(lvl).size()));
    os->write("] ", 2);
}

template <typename... Args>
void log_emit(petra::LogLevel lvl,
              std::format_string<Args...> fmt,
              Args&&... args) {
    log_emit_prefixed(lvl);
    if (petra::log_sink() == nullptr) return;
    std::ostream* os = petra::log_sink();
    std::string s = std::vformat(fmt.get(),
                                 std::make_format_args(args...));
    os->write(s.data(), static_cast<std::streamsize>(s.size()));
    os->put('\n');
}

inline void log_emit_runtime(petra::LogLevel lvl,
                              std::string_view fmt,
                              std::format_args args) {
    petra::log_emit_prefixed(lvl);
    if (petra::log_sink() == nullptr) return;
    std::ostream* os = petra::log_sink();
    std::string s = std::vformat(fmt, args);
    os->write(s.data(), static_cast<std::streamsize>(s.size()));
    os->put('\n');
}

} // namespace petra

// =====================================================================
// Part 3 — petra::sync_sink_target (lifted from Sep 2)
// =====================================================================
//
// petra::sync_sink_target()& returns a reference to a
// function-static std::reference_wrapper<std::ostream> that the
// user can swap (std::cerr, a test std::ostringstream, a
// std::ofstream, etc.).  Sep 2 chose reference_wrapper over a
// raw pointer because we want the swap target to live on the
// stack/heap as a normal std::ostream without owning pointers;
// today keeps that choice because the file/line-aware helpers
// just construct a std::osyncstream from the SAME underlying
// reference and there is no semantic change.
// =====================================================================

namespace petra {

inline std::reference_wrapper<std::ostream>& sync_sink_target() noexcept {
    static std::reference_wrapper<std::ostream> r{std::cerr};
    return r;
}

} // namespace petra

// =====================================================================
// Part 4 — petra::sync_log_emit + sync_log_emit_runtime (lifted)
// =====================================================================
//
// sync_log_emit constructs a fresh std::osyncstream wrapping
// petra::sync_sink_target().get().  The osyncstream buffers into
// a per-thread syncbuf; the explicit sync.flush() at the end of
// the function commits the WHOLE line atomically (P0053R7 commit
// semantics).  The two-phase write problem Sep 1 documented (a
// concurrent log call could interleave between the prefix and
// the payload on the default std::cerr sink) does NOT exist
// here because the prefix AND the payload are emitted through
// the SAME osyncstream and the syncbuf commits the entire
// buffer atomically.
//
// sync_log_emit_runtime is the runtime-format-string sibling
// (same shape as Sep 1's log_emit / log_emit_runtime split).
// =====================================================================

namespace petra {

template <typename... Args>
void sync_log_emit(LogLevel lvl,
                   std::format_string<Args...> fmt,
                   Args&&... args) {
    std::ostream& underlying = petra::sync_sink_target().get();
    std::osyncstream sync{underlying};
    // ONE osyncstream for the whole line — prefix + payload
    // both route through it so the sync.flush() at the end
    // commits them as a single atomic line.
    sync << '[' << petra::log_level_tag(lvl) << "] ";
    std::string payload = std::vformat(fmt.get(),
                                      std::make_format_args(args...));
    sync.write(payload.data(),
               static_cast<std::streamsize>(payload.size()));
    sync.put('\n');
    sync.flush();
}

inline void sync_log_emit_runtime(LogLevel lvl,
                                   std::string_view fmt,
                                   std::format_args args) {
    std::ostream& underlying = petra::sync_sink_target().get();
    std::osyncstream sync{underlying};
    sync << '[' << petra::log_level_tag(lvl) << "] ";
    std::string payload = std::vformat(fmt, args);
    sync.write(payload.data(),
               static_cast<std::streamsize>(payload.size()));
    sync.put('\n');
    sync.flush();
}

} // namespace petra

// =====================================================================
// Macros — SYNC_LOG_* (consteval) + SYNC_LOG_*_RUNTIME (lifted)
// =====================================================================
//
// `do { ... } while (0)` so `if (cond) SYNC_LOG_INFO(...); else
// ...;` binds correctly (same recipe as Sep 1 / Sep 2).  The
// helper-template split keeps the consteval gate on the
// literal-format-string path; the runtime sibling takes
// (std::string_view fmt, std::format_args args).
// =====================================================================

#define PETRA_SYNC_LOG_EMIT(level, ...) \
    do { \
        ::petra::sync_log_emit((::petra::LogLevel::level), __VA_ARGS__); \
    } while (0)

#define PETRA_SYNC_LOG_EMIT_RUNTIME(level, fmt, args) \
    do { \
        ::petra::sync_log_emit_runtime((::petra::LogLevel::level), (fmt), args); \
    } while (0)

#define SYNC_LOG_DEBUG(...)    PETRA_SYNC_LOG_EMIT(Debug, __VA_ARGS__)
#define SYNC_LOG_INFO(...)     PETRA_SYNC_LOG_EMIT(Info,  __VA_ARGS__)
#define SYNC_LOG_WARN(...)     PETRA_SYNC_LOG_EMIT(Warn,  __VA_ARGS__)
#define SYNC_LOG_ERROR(...)    PETRA_SYNC_LOG_EMIT(Error, __VA_ARGS__)

#define SYNC_LOG_DEBUG_RUNTIME(fmt, args) \
    PETRA_SYNC_LOG_EMIT_RUNTIME(Debug, fmt, args)
#define SYNC_LOG_INFO_RUNTIME(fmt, args) \
    PETRA_SYNC_LOG_EMIT_RUNTIME(Info,  fmt, args)
#define SYNC_LOG_WARN_RUNTIME(fmt, args) \
    PETRA_SYNC_LOG_EMIT_RUNTIME(Warn,  fmt, args)
#define SYNC_LOG_ERROR_RUNTIME(fmt, args) \
    PETRA_SYNC_LOG_EMIT_RUNTIME(Error, fmt, args)

// =====================================================================
// Part 5 — petra::sync_log_emit_loc + sync_log_emit_loc_runtime
// =====================================================================
//
// sync_log_emit_loc is the file-and-line variant of sync_log_emit.
// The committed line shape is:
//
//     [<LEVEL>] <file>:<line> <payload>\n
//
// where <file> is the C string passed in (the literal __FILE__
// at the call site, captured by the LOCSYNC_LOG_* macro) and
// <line> is the integer __LINE__ captured at the call site.
// The order is `tag SPACE file COLON line SPACE payload` so a
// log line parser sees:
//
//     1. the level (the same 5-char aligned Sep 1 / Sep 2 tag)
//     2. the file:line (separated from the tag by a single space,
//        separated from the payload by a single space, with a
//        single colon between file and line — the gnu / glog /
//        spdlog convention)
//     3. the payload (whatever the format string expands to)
//
// The headline property: the WHOLE line — tag, file, line, and
// payload — is emitted through the SAME std::osyncstream so the
// sync.flush() at the end commits them as one indivisible write
// to the underlying std::ostream.  Adding file:line to the
// prefix does NOT weaken the atomicity guarantee; the only
// thing the file:line does is make the prefix longer, and the
// osyncstream buffer batches the whole longer line just the
// same.
//
// sync_log_emit_loc_runtime is the runtime-format-string sibling
// (same shape as Sep 1 / Sep 2's runtime surface).
// =====================================================================

namespace petra {

template <typename... Args>
void sync_log_emit_loc(LogLevel lvl,
                       const char* file,
                       int line,
                       std::format_string<Args...> fmt,
                       Args&&... args) {
    std::ostream& underlying = petra::sync_sink_target().get();
    std::osyncstream sync{underlying};
    // ONE osyncstream for the whole line — tag + file:line +
    // payload all route through it so the sync.flush() at the
    // end commits them as one atomic line.
    sync << '[' << petra::log_level_tag(lvl) << "] "
         << file << ':' << line << ' ';
    std::string payload = std::vformat(fmt.get(),
                                      std::make_format_args(args...));
    sync.write(payload.data(),
               static_cast<std::streamsize>(payload.size()));
    sync.put('\n');
    sync.flush();
}

inline void sync_log_emit_loc_runtime(LogLevel lvl,
                                      const char* file,
                                      int line,
                                      std::string_view fmt,
                                      std::format_args args) {
    std::ostream& underlying = petra::sync_sink_target().get();
    std::osyncstream sync{underlying};
    sync << '[' << petra::log_level_tag(lvl) << "] "
         << file << ':' << line << ' ';
    std::string payload = std::vformat(fmt, args);
    sync.write(payload.data(),
               static_cast<std::streamsize>(payload.size()));
    sync.put('\n');
    sync.flush();
}

} // namespace petra

// =====================================================================
// Part 6 — LOCSYNC_LOG_* macros (file/line-aware)
// =====================================================================
//
// The LOCSYNC_LOG_* macros are the surface the lesson exists to
// ship.  The macros capture __FILE__ and __LINE__ at the call
// site and forward them to petra::sync_log_emit_loc /
// sync_log_emit_loc_runtime.  The call site never names the
// file/line — the macro inserts them automatically.
//
// The consteval-vs-runtime split is identical to Sep 1 / Sep 2:
// the LOCSYNC_LOG_* macros (no _RUNTIME suffix) take a literal
// format string and route through sync_log_emit_loc, which
// takes std::format_string<Args...> by value; the
// std::format_string<Args...>::basic_format_string ctor is
// consteval so malformed format strings are HARD COMPILE
// ERRORS at the call site.  The LOCSYNC_LOG_*_RUNTIME siblings
// take (std::string_view fmt, std::format_args args) and parse
// errors throw std::format_error at format time.
//
// `do { ... } while (0)` wrapping is preserved so the macros
// compose with if/else the same way Sep 1 / Sep 2 macros do.
// =====================================================================

#define PETRA_LOCSYNC_LOG_EMIT(level, ...) \
    do { \
        ::petra::sync_log_emit_loc((::petra::LogLevel::level), \
                                   __FILE__, __LINE__, __VA_ARGS__); \
    } while (0)

#define PETRA_LOCSYNC_LOG_EMIT_RUNTIME(level, fmt, args) \
    do { \
        ::petra::sync_log_emit_loc_runtime((::petra::LogLevel::level), \
                                           __FILE__, __LINE__, \
                                           (fmt), args); \
    } while (0)

#define LOCSYNC_LOG_DEBUG(...)    PETRA_LOCSYNC_LOG_EMIT(Debug, __VA_ARGS__)
#define LOCSYNC_LOG_INFO(...)     PETRA_LOCSYNC_LOG_EMIT(Info,  __VA_ARGS__)
#define LOCSYNC_LOG_WARN(...)     PETRA_LOCSYNC_LOG_EMIT(Warn,  __VA_ARGS__)
#define LOCSYNC_LOG_ERROR(...)    PETRA_LOCSYNC_LOG_EMIT(Error, __VA_ARGS__)

#define LOCSYNC_LOG_DEBUG_RUNTIME(fmt, args) \
    PETRA_LOCSYNC_LOG_EMIT_RUNTIME(Debug, fmt, args)
#define LOCSYNC_LOG_INFO_RUNTIME(fmt, args) \
    PETRA_LOCSYNC_LOG_EMIT_RUNTIME(Info,  fmt, args)
#define LOCSYNC_LOG_WARN_RUNTIME(fmt, args) \
    PETRA_LOCSYNC_LOG_EMIT_RUNTIME(Warn,  fmt, args)
#define LOCSYNC_LOG_ERROR_RUNTIME(fmt, args) \
    PETRA_LOCSYNC_LOG_EMIT_RUNTIME(Error, fmt, args)

// =====================================================================
// Mini test framework (mirrors Sep 2 — CHECK records pass/fail, a
// global counter, and any failure message including the source
// file/line via __FILE__ and __LINE__.  We use the C preprocessor
// __FILE__ and __LINE__ here — NOT the LOCSYNC_LOG_* macros —
// because the test framework wants the literal preprocessor values
// of the CHECK site, not a runtime __FILE__ capture, and because
// we want the failure report to be observable regardless of which
// sync sink is currently bound.
// =====================================================================

namespace {

int g_checks_total = 0;
int g_checks_failed = 0;
std::vector<std::string> g_failures;

#define CHECK(expr) \
    do { \
        ++::g_checks_total; \
        if (!(expr)) { \
            ++::g_checks_failed; \
            char buf[512]; \
            std::snprintf(buf, sizeof(buf), \
                          "  FAIL  %s:%d  CHECK(%s)\n", \
                          __FILE__, __LINE__, #expr); \
            ::g_failures.emplace_back(buf); \
        } \
    } while (0)

void section_header(const char* name) {
    std::printf("-- section %s\n", name);
}

} // namespace

// =====================================================================
// Section 1 — sizeof + feature probes
// =====================================================================

void section_1_sizeof_and_probes() {
    section_header("1.sizeof+probes");
    CHECK(sizeof(petra::LogLevel) == sizeof(int));
    CHECK(petra::log_level_tag(petra::LogLevel::Debug) == "DEBUG");
    CHECK(petra::log_level_tag(petra::LogLevel::Info)  == "INFO ");
    CHECK(petra::log_level_tag(petra::LogLevel::Warn)  == "WARN ");
    CHECK(petra::log_level_tag(petra::LogLevel::Error) == "ERROR");
    // <syncstream> present and std::osyncstream symbol exposed —
    // the latter requires -fexperimental-library on Apple Clang
    // 21.0.0 / libc++ 21 (probed during Sep 2 lesson prep and
    // replicated here).
    static_assert(sizeof(std::osyncstream) > 0,
                  "std::osyncstream must be defined");
    // sync_log_emit_loc overload set resolves the consteval path
    // when the literal format string is a std::format_string<Args...>.
    // Probe the type with a static_assert on the call expression's
    // return type.
    static_assert(std::is_same_v<
        decltype(petra::sync_log_emit_loc(petra::LogLevel::Info,
                                          "f.cpp", 7,
                                          std::format_string<int>{"v={}."},
                                          1)),
        void>,
        "sync_log_emit_loc consteval overload must resolve to void");
    CHECK(true);
}

// =====================================================================
// Section 2 — basic single-thread success: line shape with file/line
// =====================================================================

void section_2_basic_success_loc() {
    section_header("2.basic-success-loc");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    // LOCSYNC_LOG_INFO("hello {}", std::string("petra"));
    // The macro captures __FILE__ (this source's path) and the
    // __LINE__ of the call site — verified in section 3 to be
    // the line right above (which is the line we expect the
    // call to expand to).  For section 2 we only check the
    // shape: [<LEVEL>] <file>:<line> <payload>\n
    LOCSYNC_LOG_INFO("hello {}", std::string("petra"));
    LOCSYNC_LOG_WARN("count={}", 42);

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    // Section 3 pins the file/line content; section 2 pins the
    // SHAPE: starts with "[INFO ] " (level tag), then a file
    // path (anything), then ":", then an integer, then a space,
    // then the payload.  (std::getline strips the trailing \n,
    // so the regex anchors at $ only, no \n.)
    std::regex re_shape(R"(^\[(INFO |WARN )\] [^:]+:[0-9]+ hello petra$)");
    bool line1_ok = false;
    bool line2_ok = false;
    std::istringstream lines(captured);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.find("hello petra") != std::string::npos) {
            line1_ok = std::regex_match(line, re_shape);
        }
        if (line.find("count=42") != std::string::npos) {
            // Same shape with a different payload.
            std::regex re_shape2(R"(^\[(INFO |WARN )\] [^:]+:[0-9]+ count=42$)");
            line2_ok = std::regex_match(line, re_shape2);
        }
    }
    CHECK(line1_ok);
    CHECK(line2_ok);
}

// =====================================================================
// Section 3 — file/line accuracy: __FILE__ / __LINE__ are the
// preprocessor values at the LOCSYNC_LOG_* call site
// =====================================================================
//
// We use sentinel payloads: each LOCSYNC_LOG_INFO call below
// expands with __FILE__ set to this source's path and __LINE__
// set to the literal line number of the call.  We verify both
// values appear literally in the committed line.
// =====================================================================

void section_3_file_line_accuracy() {
    section_header("3.file-line-accuracy");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    // The two LOCSYNC_LOG_* macro CALLS below are on literal
    // lines 605 and 606 of this source file.  The macro captures
    // __FILE__ (this source's path) and __LINE__ at each call
    // site — and section 3 verifies both values appear literally
    // in the committed line.
    LOCSYNC_LOG_INFO("first-line");   // line 605: __LINE__ = 605
    LOCSYNC_LOG_WARN("second-line");  // line 606: __LINE__ = 606

    ref = std::ref(*prev_raw);

    // __FILE__ at the macro expansion site is this source's path.
    constexpr const char* expected_file = __FILE__;
    constexpr int first_line_no  = 605;   // LOCSYNC_LOG_INFO("first-line")
    constexpr int second_line_no = 606;   // LOCSYNC_LOG_WARN("second-line")
    std::string captured = oss.str();

    // std::getline strips the trailing \n; no \n in the regex.
    std::regex re_first(R"(^\[INFO \] )"
                        + std::string(expected_file)
                        + R"(:)" + std::to_string(first_line_no)
                        + R"( first-line$)");
    std::regex re_second(R"(^\[WARN \] )"
                         + std::string(expected_file)
                         + R"(:)" + std::to_string(second_line_no)
                         + R"( second-line$)");

    bool first_ok = false;
    bool second_ok = false;
    std::istringstream lines(captured);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.find("first-line") != std::string::npos)
            first_ok = std::regex_match(line, re_first);
        if (line.find("second-line") != std::string::npos)
            second_ok = std::regex_match(line, re_second);
    }
    CHECK(first_ok);
    CHECK(second_ok);
}

// =====================================================================
// Section 4 — zero-args (LOCSYNC_LOG_INFO("ping"))
// =====================================================================

void section_4_zero_args_loc() {
    section_header("4.zero-args-loc");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    LOCSYNC_LOG_INFO("ping");
    LOCSYNC_LOG_DEBUG("empty={:>3}", "");

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    std::regex re_ping(R"(^\[INFO \] )"
                       + std::string(__FILE__)
                       + R"(:[0-9]+ ping$)");
    std::regex re_empty(R"(^\[DEBUG\] )"
                        + std::string(__FILE__)
                        + R"(:[0-9]+ empty=   $)");
    bool ping_ok = false;
    bool empty_ok = false;
    std::istringstream lines(captured);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.find("ping") != std::string::npos)
            ping_ok = std::regex_match(line, re_ping);
        if (line.find("empty=") != std::string::npos)
            empty_ok = std::regex_match(line, re_empty);
    }
    CHECK(ping_ok);
    CHECK(empty_ok);
}

// =====================================================================
// Section 5 — comma-in-arguments (LOCSYNC_LOG_INFO("p=({},{})", a, b))
// =====================================================================

void section_5_comma_in_args_loc() {
    section_header("5.comma-in-args-loc");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    std::pair<int, int> p{3, 7};
    LOCSYNC_LOG_INFO("pair=({},{})", p.first, p.second);

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    std::regex re(R"(^\[INFO \] )"
                  + std::string(__FILE__)
                  + R"(:[0-9]+ pair=\(3,7\)$)");
    bool ok = false;
    std::istringstream lines(captured);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.find("pair=(3,7)") != std::string::npos)
            ok = std::regex_match(line, re);
    }
    CHECK(ok);
}

// =====================================================================
// Section 6 — if/else hygiene (both branches, with file/line on each)
// =====================================================================

void section_6_if_else_hygiene_loc() {
    section_header("6.if-else-hygiene-loc");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    bool cond_true = true;
    bool cond_false = false;
    if (cond_true)  LOCSYNC_LOG_INFO("then-branch");
    else            LOCSYNC_LOG_INFO("else-branch");
    if (cond_false) LOCSYNC_LOG_INFO("then-branch");
    else            LOCSYNC_LOG_INFO("else-branch");

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    std::regex re_then(R"(^\[INFO \] )"
                       + std::string(__FILE__)
                       + R"(:[0-9]+ then-branch$)");
    std::regex re_else(R"(^\[INFO \] )"
                       + std::string(__FILE__)
                       + R"(:[0-9]+ else-branch$)");
    int then_count = 0;
    int else_count = 0;
    std::istringstream lines(captured);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.find("then-branch") != std::string::npos
            && std::regex_match(line, re_then))
            ++then_count;
        if (line.find("else-branch") != std::string::npos
            && std::regex_match(line, re_else))
            ++else_count;
    }
    CHECK(then_count == 1);
    CHECK(else_count == 1);
}

// =====================================================================
// Section 7 — format-specifier passthrough on the LOCSYNC surface
// ({:#x}, {:.3f}, {:>8}, {:<6}) — the consteval gate is identical
// to Sep 1 / Sep 2 so we just pin the output shape.
// =====================================================================

void section_7_format_specifiers_loc() {
    section_header("7.format-specifiers-loc");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    LOCSYNC_LOG_INFO("hex={:#x}", 0xCAFE);
    LOCSYNC_LOG_INFO("flt={:.3f}", 3.14159);
    LOCSYNC_LOG_INFO("pad={:>8}", ">");
    LOCSYNC_LOG_INFO("lp={:<6}", "<");

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    std::string file_prefix = std::string("[INFO ] ") + __FILE__ + ":";
    bool hex_ok  = captured.find(file_prefix) != std::string::npos
                   && captured.find("hex=0xcafe\n")    != std::string::npos;
    bool flt_ok  = captured.find(file_prefix) != std::string::npos
                   && captured.find("flt=3.142\n")     != std::string::npos;
    bool pad_ok  = captured.find(file_prefix) != std::string::npos
                   && captured.find("pad=       >\n") != std::string::npos;
    bool lp_ok   = captured.find(file_prefix) != std::string::npos
                   && captured.find("lp=<     \n")    != std::string::npos;
    CHECK(hex_ok);
    CHECK(flt_ok);
    CHECK(pad_ok);
    CHECK(lp_ok);
}

// =====================================================================
// Section 8 — consteval gate via static_assert probe on the LOC
// helpers — same recipe as Sep 1 / Sep 2 Section 7.
// =====================================================================

void section_8_consteval_gate_loc() {
    section_header("8.consteval-gate-loc");
    // std::format_string<int>{"value={}."} is a literal of
    // 9 chars; if the consteval-ctor gate were broken this
    // static_assert would NOT compile.
    static_assert([]{
        constexpr std::format_string<int> fs{"value={}."};
        return fs.get().size() == 9;
    }(), "compiletime format_string probe");
    // And the LOC sync_log_emit_loc overload must be reachable
    // through a literal format string.  We can't bind the
    // call to a constant expression (it's a runtime sink),
    // but the static_assert on its return type (Section 1)
    // is enough — the same probe applies here.
    CHECK(true);
}

// =====================================================================
// Section 9 — runtime sibling: LOCSYNC_LOG_INFO_RUNTIME
// =====================================================================

void section_9_runtime_sibling_loc() {
    section_header("9.runtime-sibling-loc");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    std::string fmt = "runtime={}";
    int val = 123;
    LOCSYNC_LOG_INFO_RUNTIME(fmt, std::make_format_args(val));

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    std::regex re(R"(^\[INFO \] )"
                  + std::string(__FILE__)
                  + R"(:[0-9]+ runtime=123$)");
    bool ok = false;
    std::istringstream lines(captured);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.find("runtime=123") != std::string::npos)
            ok = std::regex_match(line, re);
    }
    CHECK(ok);
}

// =====================================================================
// Section 10 — NUL byte in payload (file:line prefix must still
// surround the payload, including any embedded NUL)
// =====================================================================

void section_10_nul_in_payload_loc() {
    section_header("10.nul-in-payload-loc");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    std::string with_nul;
    with_nul.push_back('a');
    with_nul.push_back('\0');
    with_nul.push_back('b');
    LOCSYNC_LOG_INFO("nul={}", with_nul);

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    std::string prefix = std::string("[INFO ] ") + __FILE__ + ":";
    auto pos = captured.find(prefix);
    CHECK(pos != std::string::npos);
    // After the prefix we expect " nul=" then 'a', NUL, 'b',
    // then '\n'.
    if (pos != std::string::npos) {
        auto p2 = captured.find("nul=", pos);
        CHECK(p2 != std::string::npos);
        if (p2 != std::string::npos) {
            p2 += 4;  // past "nul="
            CHECK(p2 + 4 <= captured.size());
            CHECK(captured[p2 + 0] == 'a');
            CHECK(captured[p2 + 1] == '\0');
            CHECK(captured[p2 + 2] == 'b');
            CHECK(captured[p2 + 3] == '\n');
        }
    }
}

// =====================================================================
// Section 11 — atomic concurrent emission with file/line on every
// committed line (THE headline section)
//
// 4 threads alternate LOCSYNC_LOG_INFO and LOCSYNC_LOG_WARN
// calls, 250 times each.  Total 1000 commits.  For each captured
// line the prefix (level + file:line) AND the payload must be
// on the SAME line — no half-line interleaving.  The regex
// matches the full line shape: ^\[(INFO |WARN )\] .+:[0-9]+
// t[0-3]=\d+$.  The atomic-line property is preserved EVEN
// WITH the file:line in the prefix; the prefix is longer, but
// the osyncstream buffer batches the whole longer line just
// the same.
// =====================================================================

void section_11_atomic_concurrent_emission_loc() {
    section_header("11.atomic-concurrent-emission-loc");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    constexpr int per_thread = 250;
    constexpr int n_threads  = 4;
    std::vector<std::thread> workers;
    workers.reserve(n_threads);
    std::atomic<int> ready{0};
    std::atomic<bool> go{false};

    for (int t = 0; t < n_threads; ++t) {
        workers.emplace_back([&, t]{
            ready.fetch_add(1, std::memory_order_release);
            while (!go.load(std::memory_order_acquire)) { /* spin */ }
            for (int i = 0; i < per_thread; ++i) {
                if ((t + i) & 1) {
                    LOCSYNC_LOG_INFO("t{}={}", t, i);
                } else {
                    LOCSYNC_LOG_WARN("t{}={}", t, i);
                }
            }
        });
    }

    while (ready.load(std::memory_order_acquire) < n_threads) {
        std::this_thread::yield();
    }
    go.store(true, std::memory_order_release);
    for (auto& th : workers) th.join();

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    int line_count = 0;
    int info_count = 0;
    int warn_count = 0;
    std::regex re(R"(^\[(INFO |WARN )\] .+:[0-9]+ t[0-3]=\d+$)");
    std::istringstream lines(captured);
    std::string line;
    bool all_match = true;
    while (std::getline(lines, line)) {
        if (line.empty()) continue;
        ++line_count;
        if (line.find("INFO") != std::string::npos) ++info_count;
        if (line.find("WARN") != std::string::npos) ++warn_count;
        if (!std::regex_match(line, re)) all_match = false;
    }
    CHECK(line_count == n_threads * per_thread);
    CHECK(info_count == warn_count);
    CHECK(info_count + warn_count == n_threads * per_thread);
    CHECK(all_match);
}

// =====================================================================
// Section 12 — sink-swap behaviour with file/line on every swap
// =====================================================================

void section_12_swap_round_trip_4_loc() {
    section_header("12.swap-round-trip-4-loc");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();

    for (int round = 0; round < 4; ++round) {
        ref = std::ref(oss);
        LOCSYNC_LOG_INFO("round={}", round);
        ref = std::ref(*prev_raw);
    }

    std::string captured = oss.str();
    std::string prefix = std::string("[INFO ] ") + __FILE__ + ":";
    int count = 0;
    bool all_match_shape = true;
    std::regex re_shape(R"(^\[INFO \] )"
                        + std::string(__FILE__)
                        + R"(:[0-9]+ round=[0-3]$)");
    std::istringstream lines(captured);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.find("round=") != std::string::npos) {
            ++count;
            if (!std::regex_match(line, re_shape)) all_match_shape = false;
        }
    }
    CHECK(count == 4);
    CHECK(all_match_shape);
    CHECK(captured.find(prefix) != std::string::npos);
}

// =====================================================================
// Section 13 — LOC surface stress: 8 threads, 200 commits each
//
// Same shape as Sep 2 Section 13; the wider experiment is to
// confirm the longer prefix (level + file:line) doesn't degrade
// the atomicity guarantee.  The committed-line regex is the same
// section-11 regex with the [0-3] -> [0-7] thread-index range.
// =====================================================================

void section_13_loc_stress_8_threads_200_each() {
    section_header("13.loc-stress-8x200");
    std::regex re(R"(^\[(INFO |WARN )\] .+:[0-9]+ t[0-7]=\d+$)");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    constexpr int per_thread = 200;
    constexpr int n_threads  = 8;
    std::vector<std::thread> workers;
    workers.reserve(n_threads);
    std::atomic<int> ready{0};
    std::atomic<bool> go{false};

    for (int t = 0; t < n_threads; ++t) {
        workers.emplace_back([&, t]{
            ready.fetch_add(1, std::memory_order_release);
            while (!go.load(std::memory_order_acquire)) {}
            for (int i = 0; i < per_thread; ++i) {
                if ((t + i) & 1) LOCSYNC_LOG_INFO("t{}={}", t, i);
                else             LOCSYNC_LOG_WARN("t{}={}", t, i);
            }
        });
    }
    while (ready.load(std::memory_order_acquire) < n_threads)
        std::this_thread::yield();
    go.store(true, std::memory_order_release);
    for (auto& th : workers) th.join();

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    int line_count = 0;
    int match_count = 0;
    std::istringstream lines(captured);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.empty()) continue;
        ++line_count;
        if (std::regex_match(line, re)) ++match_count;
    }
    CHECK(line_count == n_threads * per_thread);
    CHECK(match_count == n_threads * per_thread);
}

// =====================================================================
// Section 14 — determinism: alternating LOC emit, 50 rounds
// =====================================================================

void section_14_determinism_loc_alternating() {
    section_header("14.determinism-loc-alternating");
    int totals[2] = {0, 0};
    for (int round = 0; round < 50; ++round) {
        std::ostringstream oss;
        auto& ref = petra::sync_sink_target();
        auto* prev_raw = &ref.get();
        ref = std::ref(oss);

        for (int i = 0; i < 4; ++i) {
            if ((round + i) & 1) {
                LOCSYNC_LOG_INFO("round{}-i{}", round, i);
            } else {
                LOCSYNC_LOG_WARN("round{}-i{}", round, i);
            }
        }
        ref = std::ref(*prev_raw);

        std::string captured = oss.str();
        std::regex re(R"(^\[(INFO |WARN )\] )"
                      + std::string(__FILE__)
                      + R"(:[0-9]+ round[0-9]+-i[0-3]$)");
        std::istringstream lines(captured);
        std::string line;
        while (std::getline(lines, line)) {
            if (std::regex_match(line, re)) {
                if (line.find("[INFO ]") != std::string::npos) ++totals[0];
                if (line.find("[WARN ]") != std::string::npos) ++totals[1];
            }
        }
    }
    CHECK(totals[0] == totals[1]);
    CHECK(totals[0] == 100);
    CHECK(totals[1] == 100);
}

// =====================================================================
// Section 15 — concurrent round-trip, 5x determinism
// =====================================================================

void section_15_concurrent_round_trip_5x_loc() {
    section_header("15.concurrent-round-trip-5x-loc");
    std::regex re(R"(^\[(INFO |WARN )\] .+:[0-9]+ t[0-3]=\d+$)");
    for (int run = 0; run < 5; ++run) {
        std::ostringstream oss;
        auto& ref = petra::sync_sink_target();
        auto* prev_raw = &ref.get();
        ref = std::ref(oss);

        constexpr int per_thread = 50;
        constexpr int n_threads  = 4;
        std::vector<std::thread> workers;
        workers.reserve(n_threads);
        std::atomic<int> ready{0};
        std::atomic<bool> go{false};
        for (int t = 0; t < n_threads; ++t) {
            workers.emplace_back([&, t]{
                ready.fetch_add(1, std::memory_order_release);
                while (!go.load(std::memory_order_acquire)) {}
                for (int i = 0; i < per_thread; ++i) {
                    if ((t + i) & 1) LOCSYNC_LOG_INFO("t{}={}", t, i);
                    else             LOCSYNC_LOG_WARN("t{}={}", t, i);
                }
            });
        }
        while (ready.load(std::memory_order_acquire) < n_threads)
            std::this_thread::yield();
        go.store(true, std::memory_order_release);
        for (auto& th : workers) th.join();

        ref = std::ref(*prev_raw);
        std::string captured = oss.str();
        int line_count = 0;
        int match_count = 0;
        std::istringstream lines(captured);
        std::string line;
        while (std::getline(lines, line)) {
            if (line.empty()) continue;
            ++line_count;
            if (std::regex_match(line, re)) ++match_count;
        }
        CHECK(line_count == n_threads * per_thread);
        CHECK(match_count == n_threads * per_thread);
    }
}

// =====================================================================
// Section 16 — empty payload (LOCSYNC_LOG_INFO(""))
// =====================================================================

void section_16_empty_payload_loc() {
    section_header("16.empty-payload-loc");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    // The LOCSYNC_LOG_INFO("") call below is on line 1146 of
    // this source file.  The macro captures __LINE__ at the
    // call site; the expected line below uses the literal value.
    LOCSYNC_LOG_INFO("");

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    // Expected exact: "[INFO ] " + __FILE__ + ":1146 \n"
    constexpr int empty_call_line = 1146;
    std::string expected = std::string("[INFO ] ")
                         + __FILE__
                         + ":"
                         + std::to_string(empty_call_line)
                         + " \n";
    CHECK(captured == expected);
}

int main() {
    section_1_sizeof_and_probes();
    section_2_basic_success_loc();
    section_3_file_line_accuracy();
    section_4_zero_args_loc();
    section_5_comma_in_args_loc();
    section_6_if_else_hygiene_loc();
    section_7_format_specifiers_loc();
    section_8_consteval_gate_loc();
    section_9_runtime_sibling_loc();
    section_10_nul_in_payload_loc();
    section_11_atomic_concurrent_emission_loc();
    section_12_swap_round_trip_4_loc();
    section_13_loc_stress_8_threads_200_each();
    section_14_determinism_loc_alternating();
    section_15_concurrent_round_trip_5x_loc();
    section_16_empty_payload_loc();

    std::printf("\n%d/%d %s\n",
                g_checks_total - g_checks_failed,
                g_checks_total,
                g_checks_failed == 0 ? "PASS" : "FAIL");
    for (auto const& f : g_failures) std::fputs(f.c_str(), stdout);
    return g_checks_failed == 0 ? 0 : 1;
}
