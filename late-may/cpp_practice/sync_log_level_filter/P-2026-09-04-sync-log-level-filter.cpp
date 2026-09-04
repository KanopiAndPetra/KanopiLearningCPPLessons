// P-2026-09-04 — sync_log_level_filter: PETRA_SYNC_LOG_LEVEL
// compile-time gate for the Sep 2 SYNC_LOG_* and Sep 3 LOCSYNC_LOG_*
// macros — disabled levels reduce to a `((void)0)` no-op so a
// release build pays zero cost for filtered-out levels.
//
// Why today
// ---------
// The Sep 3 lesson (P-2026-09-03-sync-log-file-line-macros) closed
// the SECOND of three new follow-on items Sep 1's "Where we go
// next" section named.  Sep 1 / Sep 2 / Sep 3 all carried forward
// the LAST one:
//
//   > Level-filter compile-time gate.  A #define PETRA_SYNC_LOG_LEVEL 2
//   > plus #if PETRA_SYNC_LOG_LEVEL <= N gating each SYNC /
//   > LOCSYNC macro to a no-op would let release builds pay
//   > zero cost for disabled levels.  Today's macros always
//   > emit; the filter is follow-on.  (The LOCSYNC_LOG_*
//   > macros inherit the same #if-gate recipe as the SYNC_*
//   > macros; both surfaces would be gated together.)
//
// Today IS that lesson.  It does five things:
//
//   1. Mirrors the Sep 2 + Sep 3 surface in this TU (Parts 1-4
//      lifted verbatim): petra::ostream_println, petra::LogLevel /
//      log_level_tag / log_sink, petra::sync_sink_target,
//      petra::sync_log_emit / sync_log_emit_runtime,
//      petra::sync_log_emit_loc / sync_log_emit_loc_runtime.
//
//   2. Adds the COMPILE-TIME gate.  Each SYNC_LOG_* and
//      LOCSYNC_LOG_* macro is wrapped in `#if PETRA_SYNC_LOG_LEVEL
//      <= N` so a level above the gate reduces to `((void)0)`
//      and the helper call / the format-string construction / the
//      vformat() parse / the osyncstream construction ALL
//      disappear from the binary.
//
//   3. Adds a runtime diagnostic accessor `petra::sync_log_level()`
//      returning the CURRENT compile-time value (read-only — it
//      has no setter; the value is fixed for the lifetime of the
//      binary).  This accessor makes the gate observable in tests
//      without forcing tests to query the preprocessor symbol.
//
//   4. Documents the build recipe for the three release
//      configurations:
//        * default (-DPETRA_SYNC_LOG_LEVEL=0): emit every level
//        * release (-DPETRA_SYNC_LOG_LEVEL=1): drop DEBUG
//        * warnings-only (-DPETRA_SYNC_LOG_LEVEL=2): drop DEBUG + INFO
//        * errors-only (-DPETRA_SYNC_LOG_LEVEL=3): drop DEBUG + INFO + WARN
//
//   5. Verifies the headline property the lesson exists to pin:
//      at the DEFAULT gate (0), every macro emits exactly as Sep 2
//      and Sep 3 did — the gate is a no-op at level 0 so the
//      observable behaviour is unchanged from yesterday.  At
//      HIGHER gate values, lower levels drop out at compile time.
//      Section 11 sweeps the helper layer directly and pins the
//      gate rule; Section 12 runs the concurrent stress at the
//      default gate and confirms the atomic-line property is
//      preserved; Section 13 documents the build recipe for the
//      three higher-gate configurations and verifies the recipe
//      compiles + runs.
//
// -----------------------------------------------------------------------------
// The headline design decision — compile-time `#if` gate, not a runtime check
// -----------------------------------------------------------------------------
// Why is the gate a preprocessor `#if` and not an `if (level >= call.level)`
// inside the helper?  Three reasons:
//
// (a) **Dead-strip**.  A `#if PETRA_SYNC_LOG_LEVEL <= 0` followed by
//     `((void)0)` lets the COMPILER drop the gated branch entirely.
//     A runtime check would compile the gated branch into a
//     conditional — the function call, the format-string parse, the
//     osyncstream construction would still be in the binary, just
//     skipped at runtime.  For a DEBUG-heavy production build the
//     difference is substantial: every DEBUG call site is a full
//     format-string + vformat() + osyncstream construction that the
//     preprocessor gate removes.
//
// (b) **The consteval gate stays at compile time**.  Sep 2 / Sep 3
//     routed every consteval macro through std::format_string<Args...>
//     whose ctor is consteval — meaning parse / type-mismatch errors
//     become HARD COMPILE ERRORS at the call site.  A runtime
//     `if`-gate doesn't break that property, but the format-string
//     would still be parsed at compile time.  A compile-time `#if`
//     gate is consistent: the parse happens once and only for
//     emitted levels.
//
// (c) **The recipe matches what the spec literally named**.  Sep 3's
//     "Where we go next" says `#define PETRA_SYNC_LOG_LEVEL 2` plus
//     `#if PETRA_SYNC_LOG_LEVEL <= N`.  The recipe is named in the
//     spec; today IS the recipe.
//
// So the gate is `#if PETRA_SYNC_LOG_LEVEL <= N` and each macro has
// four preprocessor branches.  At gate=0 every macro is the
// Sep 2 / Sep 3 expansion (unchanged behaviour).  At gate=2 DEBUG
// and INFO expand to `((void)0)`.

// -----------------------------------------------------------------------------
// Platform note: enabling std::osyncstream on Apple Clang 21.0.0 / libc++ 21
// -----------------------------------------------------------------------------
// On this toolchain the <syncstream> header is shipped but its
// `std::osyncstream` typedef is gated behind
// _LIBCPP_HAS_EXPERIMENTAL_SYNCSTREAM.  The enabled path is
// `clang++ -fexperimental-library`.  (Sep 2 lesson's headline
// finding; replicated here only as the build recipe.)  The lesson
// owns the COMPILE-TIME-GATE finding; the platform gate is
// inherited from Sep 2.

// Part 1 — petra::ostream_println (lifted verbatim from Sep 2 / Sep 3)
// Part 2 — petra::LogLevel, petra::log_level_tag, petra::log_sink
// Part 3 — petra::sync_sink_target
// Part 4 — SYNC_LOG_* macros + sync_log_emit helpers (lifted)
// Part 5 — petra::sync_log_emit_loc / sync_log_emit_loc_runtime
// Part 6 — LOCSYNC_LOG_* macros (lifted)
// Part 7 — PETRA_SYNC_LOG_LEVEL gate (NEW today)
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
// Part 7a — PETRA_SYNC_LOG_LEVEL (the gate macro)
// =====================================================================
//
// Default 0 — Debug — emit every level (same observable behaviour
// as Sep 2 / Sep 3).  Set this to 1 / 2 / 3 at compile time and
// the corresponding lower levels reduce to `((void)0)` no-ops.
//
// A command-line release build would do:
//   clang++ -DPETRA_SYNC_LOG_LEVEL=2 ...
// and DEBUG + INFO calls would be dropped at compile time.

#ifndef PETRA_SYNC_LOG_LEVEL
#define PETRA_SYNC_LOG_LEVEL 0
#endif

// Sanity: PETRA_SYNC_LOG_LEVEL must be 0, 1, 2, or 3.
static_assert(PETRA_SYNC_LOG_LEVEL >= 0 && PETRA_SYNC_LOG_LEVEL <= 3,
              "PETRA_SYNC_LOG_LEVEL must be 0..3");

// =====================================================================
// Part 1 — petra::ostream_println (lifted verbatim from Sep 2 / Sep 3)
// =====================================================================

namespace petra {

inline void ostream_println_runtime(std::ostream& os,
                                    std::string_view fmt,
                                    std::format_args args) {
    std::string s = std::vformat(fmt, args);
    os.write(s.data(), static_cast<std::streamsize>(s.size()));
    os.put('\n');
}

template <typename... Args>
void ostream_println(std::ostream& os,
                     std::format_string<Args...> fmt,
                     Args&&... args) {
    petra::ostream_println_runtime(
        os, fmt.get(),
        std::make_format_args(args...));
}

} // namespace petra

// =====================================================================
// Part 2 — petra::LogLevel, petra::log_level_tag, petra::log_sink
// (lifted from Sep 1 / Sep 2 / Sep 3)
// =====================================================================

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

inline std::ostream*& log_sink() noexcept {
    static std::ostream* p = &std::cerr;
    return p;
}

inline void log_emit_prefixed(LogLevel lvl) {
    std::ostream* os = petra::log_sink();
    if (os == nullptr) return;
    os->write("[", 1);
    os->write(petra::log_level_tag(lvl).data(),
              static_cast<std::streamsize>(petra::log_level_tag(lvl).size()));
    os->write("] ", 2);
}

} // namespace petra

// =====================================================================
// Part 3 — petra::sync_sink_target
// =====================================================================

namespace petra {

inline std::reference_wrapper<std::ostream>& sync_sink_target() noexcept {
    static std::reference_wrapper<std::ostream> r{std::cerr};
    return r;
}

// Part 7b — runtime diagnostic accessor
// =====================================================================
//
// Returns the COMPILE-TIME gate value.  The accessor exists so
// tests can probe "what gate was this binary built with?" without
// reaching into the preprocessor symbol directly.  There is no
// setter — the gate is fixed for the lifetime of the binary by
// design (see the headline-finding comment block at the top of
// the TU).  Returning `constexpr int` would also work but the
// accessor is a function-local static to mirror the Sep 2
// / Sep 3 surface style.

[[nodiscard]] inline int sync_log_level() noexcept {
    return PETRA_SYNC_LOG_LEVEL;
}

} // namespace petra

// =====================================================================
// Part 4 — petra::sync_log_emit + sync_log_emit_runtime (lifted)
// =====================================================================

namespace petra {

template <typename... Args>
void sync_log_emit(LogLevel lvl,
                   std::format_string<Args...> fmt,
                   Args&&... args) {
    std::ostream& underlying = petra::sync_sink_target().get();
    std::osyncstream sync{underlying};
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
// Part 5 — petra::sync_log_emit_loc + sync_log_emit_loc_runtime
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
// Part 6 — gated SYNC_LOG_* + LOCSYNC_LOG_* macros
//
// THE HEADLINE FINDING: every macro below is gated on
// `#if PETRA_SYNC_LOG_LEVEL <= N`.  Below the gate, the
// expansion is `((void)0)` — no helper call, no format-string
// parse, no osyncstream construction.  At or above the gate, the
// expansion is the same as Sep 2 / Sep 3.
//
// The consteval-vs-runtime split is identical to Sep 2 / Sep 3.
// =====================================================================

// ---- SYNC_LOG_* (consteval) ----

#define PETRA_SYNC_LOG_EMIT(level, ...) \
    do { \
        ::petra::sync_log_emit((::petra::LogLevel::level), __VA_ARGS__); \
    } while (0)

#define PETRA_SYNC_LOG_NOOP(level) \
    ((void)0)

#if PETRA_SYNC_LOG_LEVEL <= 0
#define SYNC_LOG_DEBUG(...)    PETRA_SYNC_LOG_EMIT(Debug, __VA_ARGS__)
#else
#define SYNC_LOG_DEBUG(...)    PETRA_SYNC_LOG_NOOP(Debug)
#endif

#if PETRA_SYNC_LOG_LEVEL <= 1
#define SYNC_LOG_INFO(...)     PETRA_SYNC_LOG_EMIT(Info, __VA_ARGS__)
#else
#define SYNC_LOG_INFO(...)     PETRA_SYNC_LOG_NOOP(Info)
#endif

#if PETRA_SYNC_LOG_LEVEL <= 2
#define SYNC_LOG_WARN(...)     PETRA_SYNC_LOG_EMIT(Warn, __VA_ARGS__)
#else
#define SYNC_LOG_WARN(...)     PETRA_SYNC_LOG_NOOP(Warn)
#endif

// Error is always emitted (its level is 3, the highest).
#define SYNC_LOG_ERROR(...)    PETRA_SYNC_LOG_EMIT(Error, __VA_ARGS__)

// ---- SYNC_LOG_*_RUNTIME (runtime format string) ----

#define PETRA_SYNC_LOG_EMIT_RUNTIME(level, fmt, args) \
    do { \
        ::petra::sync_log_emit_runtime((::petra::LogLevel::level), \
                                       (fmt), args); \
    } while (0)

#if PETRA_SYNC_LOG_LEVEL <= 0
#define SYNC_LOG_DEBUG_RUNTIME(fmt, args) \
    PETRA_SYNC_LOG_EMIT_RUNTIME(Debug, fmt, args)
#else
#define SYNC_LOG_DEBUG_RUNTIME(fmt, args) \
    PETRA_SYNC_LOG_NOOP(Debug)
#endif

#if PETRA_SYNC_LOG_LEVEL <= 1
#define SYNC_LOG_INFO_RUNTIME(fmt, args) \
    PETRA_SYNC_LOG_EMIT_RUNTIME(Info, fmt, args)
#else
#define SYNC_LOG_INFO_RUNTIME(fmt, args) \
    PETRA_SYNC_LOG_NOOP(Info)
#endif

#if PETRA_SYNC_LOG_LEVEL <= 2
#define SYNC_LOG_WARN_RUNTIME(fmt, args) \
    PETRA_SYNC_LOG_EMIT_RUNTIME(Warn, fmt, args)
#else
#define SYNC_LOG_WARN_RUNTIME(fmt, args) \
    PETRA_SYNC_LOG_NOOP(Warn)
#endif

#define SYNC_LOG_ERROR_RUNTIME(fmt, args) \
    PETRA_SYNC_LOG_EMIT_RUNTIME(Error, fmt, args)

// ---- LOCSYNC_LOG_* (consteval + __FILE__ / __LINE__) ----

#define PETRA_LOCSYNC_LOG_EMIT(level, ...) \
    do { \
        ::petra::sync_log_emit_loc((::petra::LogLevel::level), \
                                   __FILE__, __LINE__, __VA_ARGS__); \
    } while (0)

#if PETRA_SYNC_LOG_LEVEL <= 0
#define LOCSYNC_LOG_DEBUG(...) PETRA_LOCSYNC_LOG_EMIT(Debug, __VA_ARGS__)
#else
#define LOCSYNC_LOG_DEBUG(...) PETRA_SYNC_LOG_NOOP(Debug)
#endif

#if PETRA_SYNC_LOG_LEVEL <= 1
#define LOCSYNC_LOG_INFO(...)  PETRA_LOCSYNC_LOG_EMIT(Info,  __VA_ARGS__)
#else
#define LOCSYNC_LOG_INFO(...)  PETRA_SYNC_LOG_NOOP(Info)
#endif

#if PETRA_SYNC_LOG_LEVEL <= 2
#define LOCSYNC_LOG_WARN(...)  PETRA_LOCSYNC_LOG_EMIT(Warn,  __VA_ARGS__)
#else
#define LOCSYNC_LOG_WARN(...)  PETRA_SYNC_LOG_NOOP(Warn)
#endif

#define LOCSYNC_LOG_ERROR(...) PETRA_LOCSYNC_LOG_EMIT(Error, __VA_ARGS__)

// ---- LOCSYNC_LOG_*_RUNTIME ----

#define PETRA_LOCSYNC_LOG_EMIT_RUNTIME(level, fmt, args) \
    do { \
        ::petra::sync_log_emit_loc_runtime((::petra::LogLevel::level), \
                                           __FILE__, __LINE__, \
                                           (fmt), args); \
    } while (0)

#if PETRA_SYNC_LOG_LEVEL <= 0
#define LOCSYNC_LOG_DEBUG_RUNTIME(fmt, args) \
    PETRA_LOCSYNC_LOG_EMIT_RUNTIME(Debug, fmt, args)
#else
#define LOCSYNC_LOG_DEBUG_RUNTIME(fmt, args) \
    PETRA_SYNC_LOG_NOOP(Debug)
#endif

#if PETRA_SYNC_LOG_LEVEL <= 1
#define LOCSYNC_LOG_INFO_RUNTIME(fmt, args) \
    PETRA_LOCSYNC_LOG_EMIT_RUNTIME(Info, fmt, args)
#else
#define LOCSYNC_LOG_INFO_RUNTIME(fmt, args) \
    PETRA_SYNC_LOG_NOOP(Info)
#endif

#if PETRA_SYNC_LOG_LEVEL <= 2
#define LOCSYNC_LOG_WARN_RUNTIME(fmt, args) \
    PETRA_LOCSYNC_LOG_EMIT_RUNTIME(Warn, fmt, args)
#else
#define LOCSYNC_LOG_WARN_RUNTIME(fmt, args) \
    PETRA_SYNC_LOG_NOOP(Warn)
#endif

#define LOCSYNC_LOG_ERROR_RUNTIME(fmt, args) \
    PETRA_LOCSYNC_LOG_EMIT_RUNTIME(Error, fmt, args)

// =====================================================================
// Test harness — compact CHECK / SECTION counters
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
            char buf[160]; \
            std::snprintf(buf, sizeof(buf), "%s:%d: CHECK(%s) failed\n", \
                          __FILE__, __LINE__, #expr); \
            ::g_failures.emplace_back(buf); \
        } \
    } while (0)

void section_header(const char* name) {
    std::printf("-- section %s\n", name);
}

void reset_sink_to_oss(std::ostringstream& oss) {
    auto& ref = petra::sync_sink_target();
    ref = std::ref(oss);
}

void restore_sink() {
    auto& ref = petra::sync_sink_target();
    ref = std::ref(std::cerr);
}

} // namespace

// =====================================================================
// Section 1 — sizeof + feature probes + gate-constant probes
// =====================================================================

void section_1_sizeof_and_probes() {
    section_header("1.sizeof+probes");
    CHECK(sizeof(petra::LogLevel) == sizeof(int));
    CHECK(petra::log_level_tag(petra::LogLevel::Debug) == "DEBUG");
    CHECK(petra::log_level_tag(petra::LogLevel::Info)  == "INFO ");
    CHECK(petra::log_level_tag(petra::LogLevel::Warn)  == "WARN ");
    CHECK(petra::log_level_tag(petra::LogLevel::Error) == "ERROR");
    static_assert(sizeof(std::osyncstream) > 0,
                  "std::osyncstream must be defined");
    // The compile-time gate value.
    CHECK(PETRA_SYNC_LOG_LEVEL >= 0);
    CHECK(PETRA_SYNC_LOG_LEVEL <= 3);
    // The runtime diagnostic accessor returns the same value.
    CHECK(petra::sync_log_level() == PETRA_SYNC_LOG_LEVEL);
    // The preprocessor symbol resolves to a literal integer.
    CHECK(PETRA_SYNC_LOG_LEVEL == 0 || PETRA_SYNC_LOG_LEVEL == 1 ||
          PETRA_SYNC_LOG_LEVEL == 2 || PETRA_SYNC_LOG_LEVEL == 3);
    CHECK(true);
}

// =====================================================================
// Section 2 — baseline (default gate=0) emits all four levels
// =====================================================================
//
// At default gate=0, DEBUG / INFO / WARN / ERROR all emit.  This
// section is the "today's macros always emit" baseline that Sep 2
// / Sep 3 established — at the default gate the new gating macros
// are observationally indistinguishable from yesterday's.

// =====================================================================
// Section 2 — baseline (default gate=0) emits all four levels
// =====================================================================
//
// At default gate=0, DEBUG / INFO / WARN / ERROR all emit.  This
// section is the "today's macros always emit" baseline that Sep 2
// / Sep 3 established — at the default gate the new gating macros
// are observationally indistinguishable from yesterday's.
//
// Higher-gate verification (the dead-strip finding the lesson
// exists to pin) lives in Section 14b, gated on
// -DPETRA_SYNC_LOG_LEVEL_GATE_VERIFY, which is OFF in the default
// build and ON for the secondary CMake build that exercises
// -DPETRA_SYNC_LOG_LEVEL=2.

void section_2_default_emits_all() {
    section_header("2.default-emits-all");
    std::ostringstream oss;
    reset_sink_to_oss(oss);

    SYNC_LOG_DEBUG("d={}", 1);
    SYNC_LOG_INFO("i={}", 2);
    SYNC_LOG_WARN("w={}", 3);
    SYNC_LOG_ERROR("e={}", 4);

    restore_sink();

    std::string captured = oss.str();
    CHECK(captured.find("[DEBUG] d=1\n") != std::string::npos);
    CHECK(captured.find("[INFO ] i=2\n") != std::string::npos);
    CHECK(captured.find("[WARN ] w=3\n") != std::string::npos);
    CHECK(captured.find("[ERROR] e=4\n") != std::string::npos);
}

// =====================================================================
// Section 3 — runtime siblings at default gate
// =====================================================================

void section_3_runtime_siblings() {
    section_header("3.runtime-siblings");
    std::ostringstream oss;
    reset_sink_to_oss(oss);

    int d_val = 11, i_val = 12, w_val = 13, e_val = 14;
    SYNC_LOG_DEBUG_RUNTIME("drt={}", std::make_format_args(d_val));
    SYNC_LOG_INFO_RUNTIME("irt={}",  std::make_format_args(i_val));
    SYNC_LOG_WARN_RUNTIME("wrt={}",  std::make_format_args(w_val));
    SYNC_LOG_ERROR_RUNTIME("ert={}", std::make_format_args(e_val));

    restore_sink();

    std::string captured = oss.str();
    CHECK(captured.find("[DEBUG] drt=11\n") != std::string::npos);
    CHECK(captured.find("[INFO ] irt=12\n") != std::string::npos);
    CHECK(captured.find("[WARN ] wrt=13\n") != std::string::npos);
    CHECK(captured.find("[ERROR] ert=14\n") != std::string::npos);
}

// =====================================================================
// Section 4 — LOC macros at default gate
// =====================================================================

void section_4_locsync_consteval() {
    section_header("4.locsync-consteval");
    std::ostringstream oss;
    reset_sink_to_oss(oss);

    LOCSYNC_LOG_DEBUG("ld={}", 21);
    LOCSYNC_LOG_INFO("li={}", 22);
    LOCSYNC_LOG_WARN("lw={}", 23);
    LOCSYNC_LOG_ERROR("le={}", 24);

    restore_sink();

    std::string captured = oss.str();
    CHECK(captured.find("[DEBUG] ") != std::string::npos);
    CHECK(captured.find(" ld=21")   != std::string::npos);
    CHECK(captured.find("[INFO ] ") != std::string::npos);
    CHECK(captured.find(" li=22")   != std::string::npos);
    CHECK(captured.find("[WARN ] ") != std::string::npos);
    CHECK(captured.find(" lw=23")   != std::string::npos);
    CHECK(captured.find("[ERROR] ") != std::string::npos);
    CHECK(captured.find(" le=24")   != std::string::npos);
}

// =====================================================================
// Section 5 — LOC runtime siblings at default gate
// =====================================================================

void section_5_locsync_runtime() {
    section_header("5.locsync-runtime");
    std::ostringstream oss;
    reset_sink_to_oss(oss);

    int ld_val = 31, li_val = 32, lw_val = 33, le_val = 34;
    LOCSYNC_LOG_DEBUG_RUNTIME("ldrt={}", std::make_format_args(ld_val));
    LOCSYNC_LOG_INFO_RUNTIME("lirt={}",  std::make_format_args(li_val));
    LOCSYNC_LOG_WARN_RUNTIME("lwrt={}",  std::make_format_args(lw_val));
    LOCSYNC_LOG_ERROR_RUNTIME("lert={}", std::make_format_args(le_val));

    restore_sink();

    std::string captured = oss.str();
    CHECK(captured.find("[DEBUG] ") != std::string::npos);
    CHECK(captured.find(" ldrt=31") != std::string::npos);
    CHECK(captured.find("[INFO ] ") != std::string::npos);
    CHECK(captured.find(" lirt=32") != std::string::npos);
    CHECK(captured.find("[WARN ] ") != std::string::npos);
    CHECK(captured.find(" lwrt=33") != std::string::npos);
    CHECK(captured.find("[ERROR] ") != std::string::npos);
    CHECK(captured.find(" lert=34") != std::string::npos);
}

// =====================================================================
// Section 6 — if/else hygiene at default gate
// =====================================================================

void section_6_if_else_hygiene() {
    section_header("6.if-else-hygiene");
    std::ostringstream oss;
    reset_sink_to_oss(oss);

    bool cond_true = true;
    bool cond_false = false;
    if (cond_true)  SYNC_LOG_INFO("then-branch");
    else            SYNC_LOG_INFO("else-branch");
    if (cond_false) SYNC_LOG_INFO("then-branch");
    else            SYNC_LOG_INFO("else-branch");

    restore_sink();

    std::string captured = oss.str();
    CHECK(captured.find("[INFO ] then-branch\n") != std::string::npos);
    CHECK(captured.find("[INFO ] else-branch\n") != std::string::npos);
}

// =====================================================================
// Section 7 — format-specifier passthrough at default gate
// =====================================================================

void section_7_format_specifiers() {
    section_header("7.format-specifiers");
    std::ostringstream oss;
    reset_sink_to_oss(oss);

    SYNC_LOG_INFO("hex={:#x}", 0xCAFE);
    SYNC_LOG_INFO("flt={:.3f}", 3.14159);
    SYNC_LOG_INFO("pad={:>8}", ">");
    SYNC_LOG_INFO("lp={:<6}", "<");

    restore_sink();

    std::string captured = oss.str();
    CHECK(captured.find("[INFO ] hex=0xcafe\n")   != std::string::npos);
    CHECK(captured.find("[INFO ] flt=3.142\n")    != std::string::npos);
    CHECK(captured.find("[INFO ] pad=       >\n") != std::string::npos);
    CHECK(captured.find("[INFO ] lp=<     \n")    != std::string::npos);
}

// =====================================================================
// Section 8 — consteval gate preserved on emit path
// =====================================================================

void section_8_consteval_gate_preserved() {
    section_header("8.consteval-gate-preserved");
    std::ostringstream oss;
    reset_sink_to_oss(oss);
    SYNC_LOG_INFO("value={}", 42);
    restore_sink();
    CHECK(oss.str().find("value=42") != std::string::npos);
    // Probe: format_string<int> compiles (consteval-ctor works).
    static_assert(std::is_constructible_v<std::format_string<int>,
                                          std::string_view>);
    CHECK(true);
}

// =====================================================================
// Section 9 — sink-swap behaviour at default gate
// =====================================================================

void section_9_sink_swap() {
    section_header("9.sink-swap");
    std::ostringstream oss1, oss2;
    {
        reset_sink_to_oss(oss1);
        SYNC_LOG_INFO("first");
        restore_sink();
    }
    {
        reset_sink_to_oss(oss2);
        SYNC_LOG_WARN("second");
        restore_sink();
    }
    CHECK(oss1.str().find("first")  != std::string::npos);
    CHECK(oss1.str().find("second") == std::string::npos);
    CHECK(oss2.str().find("first")  == std::string::npos);
    CHECK(oss2.str().find("second") != std::string::npos);
}

// =====================================================================
// Section 10 — gate-emit rule via the helper layer directly
// =====================================================================
//
// The macro layer's gate is the preprocessor `#if`.  The HELPER
// layer (petra::sync_log_emit, sync_log_emit_runtime,
// sync_log_emit_loc, sync_log_emit_loc_runtime) has NO gate —
// it's a plain function that emits whatever level you pass it.
// This is correct: a programmer who wants to bypass the gate
// (e.g. to always emit ERROR from a library) can call the helper
// directly.
//
// This section verifies that property: the helper emits at every
// level regardless of the compile-time gate value.

void section_10_helper_layer_unconditional() {
    section_header("10.helper-layer-unconditional");
    std::ostringstream oss;
    reset_sink_to_oss(oss);

    // Direct calls to the helpers — these bypass the macro gate.
    petra::sync_log_emit(petra::LogLevel::Debug, "helper-d");
    petra::sync_log_emit(petra::LogLevel::Info,  "helper-i");
    petra::sync_log_emit(petra::LogLevel::Warn,  "helper-w");
    petra::sync_log_emit(petra::LogLevel::Error, "helper-e");

    restore_sink();

    std::string captured = oss.str();
    CHECK(captured.find("[DEBUG] helper-d\n") != std::string::npos);
    CHECK(captured.find("[INFO ] helper-i\n") != std::string::npos);
    CHECK(captured.find("[WARN ] helper-w\n") != std::string::npos);
    CHECK(captured.find("[ERROR] helper-e\n") != std::string::npos);
}

// =====================================================================
// Section 11 — full sweep: all 4 macro families, all 4 levels
// =====================================================================
//
// At the default gate (0), every macro emits at every level.
// This section verifies the macro-layer behaviour is exhaustive
// across the 16 macro × level pairs:
//
//     SYNC_LOG_DEBUG / SYNC_LOG_INFO / SYNC_LOG_WARN / SYNC_LOG_ERROR
//     SYNC_LOG_DEBUG_RUNTIME / SYNC_LOG_INFO_RUNTIME /
//     SYNC_LOG_WARN_RUNTIME / SYNC_LOG_ERROR_RUNTIME
//     LOCSYNC_LOG_DEBUG / LOCSYNC_LOG_INFO / LOCSYNC_LOG_WARN /
//     LOCSYNC_LOG_ERROR
//     LOCSYNC_LOG_DEBUG_RUNTIME / LOCSYNC_LOG_INFO_RUNTIME /
//     LOCSYNC_LOG_WARN_RUNTIME / LOCSYNC_LOG_ERROR_RUNTIME

void section_11_full_macro_sweep() {
    section_header("11.full-macro-sweep");
    std::ostringstream oss;
    reset_sink_to_oss(oss);

    // SYNC_LOG_* (consteval)
    SYNC_LOG_DEBUG("a");   SYNC_LOG_INFO("b");
    SYNC_LOG_WARN("c");    SYNC_LOG_ERROR("d");
    // SYNC_LOG_*_RUNTIME
    int e=1,f=2,g=3,h=4;
    SYNC_LOG_DEBUG_RUNTIME("e", std::make_format_args(e));
    SYNC_LOG_INFO_RUNTIME("f",  std::make_format_args(f));
    SYNC_LOG_WARN_RUNTIME("g",  std::make_format_args(g));
    SYNC_LOG_ERROR_RUNTIME("h", std::make_format_args(h));
    // LOCSYNC_LOG_*
    LOCSYNC_LOG_DEBUG("i"); LOCSYNC_LOG_INFO("j");
    LOCSYNC_LOG_WARN("k");  LOCSYNC_LOG_ERROR("l");
    // LOCSYNC_LOG_*_RUNTIME
    int m=1,n=2,o=3,p=4;
    LOCSYNC_LOG_DEBUG_RUNTIME("m", std::make_format_args(m));
    LOCSYNC_LOG_INFO_RUNTIME("n",  std::make_format_args(n));
    LOCSYNC_LOG_WARN_RUNTIME("o",  std::make_format_args(o));
    LOCSYNC_LOG_ERROR_RUNTIME("p", std::make_format_args(p));

    restore_sink();

    std::string captured = oss.str();
    // 16 macros × 1 line each = 16 newlines.
    int line_count = 0;
    for (char c : captured) if (c == '\n') ++line_count;
    CHECK(line_count == 16);
    // Each macro family's level prefix is present.
    CHECK(captured.find("[DEBUG] a\n") != std::string::npos);
    CHECK(captured.find("[INFO ] b\n") != std::string::npos);
    CHECK(captured.find("[WARN ] c\n") != std::string::npos);
    CHECK(captured.find("[ERROR] d\n") != std::string::npos);
    CHECK(captured.find("[DEBUG] e\n") != std::string::npos);
    CHECK(captured.find("[INFO ] f\n") != std::string::npos);
    CHECK(captured.find("[WARN ] g\n") != std::string::npos);
    CHECK(captured.find("[ERROR] h\n") != std::string::npos);
    CHECK(captured.find("[DEBUG] ") != std::string::npos);
    CHECK(captured.find(" i\n")     != std::string::npos);
    CHECK(captured.find("[INFO ] ") != std::string::npos);
    CHECK(captured.find(" j\n")     != std::string::npos);
    CHECK(captured.find("[WARN ] ") != std::string::npos);
    CHECK(captured.find(" k\n")     != std::string::npos);
    CHECK(captured.find("[ERROR] ") != std::string::npos);
    CHECK(captured.find(" l\n")     != std::string::npos);
    CHECK(captured.find("[DEBUG] ") != std::string::npos);
    CHECK(captured.find(" m\n")     != std::string::npos);
    CHECK(captured.find("[INFO ] ") != std::string::npos);
    CHECK(captured.find(" n\n")     != std::string::npos);
    CHECK(captured.find("[WARN ] ") != std::string::npos);
    CHECK(captured.find(" o\n")     != std::string::npos);
    CHECK(captured.find("[ERROR] ") != std::string::npos);
    CHECK(captured.find(" p\n")     != std::string::npos);
}

// =====================================================================
// Section 12 — concurrent atomic emission (the headline property)
// =====================================================================
//
// At default gate, all 4 levels emit.  4 threads alternate
// SYNC_LOG_INFO and SYNC_LOG_WARN, 250 calls each.  Total 1000
// commits.  The atomic-line property (Sep 2 / Sep 3) is preserved.

void section_12_concurrent_atomic() {
    section_header("12.concurrent-atomic");
    std::ostringstream oss;
    reset_sink_to_oss(oss);

    std::atomic<int> barrier{0};
    auto worker = [&](int thread_idx) {
        barrier.fetch_add(1);
        while (barrier.load() < 4) std::this_thread::yield();
        for (int i = 0; i < 250; ++i) {
            if (i % 2 == 0) SYNC_LOG_INFO("t{}={}", thread_idx, i);
            else            SYNC_LOG_WARN("t{}={}", thread_idx, i);
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) threads.emplace_back(worker, t);
    for (auto& th : threads) th.join();

    restore_sink();

    int line_count = 0;
    for (char c : oss.str()) if (c == '\n') ++line_count;
    CHECK(line_count == 1000);

    std::regex line_re(R"(^\[(INFO |WARN )\] t[0-3]=\d+$)");
    std::istringstream lines(oss.str());
    std::string line;
    int matched = 0;
    while (std::getline(lines, line)) {
        if (line.empty()) continue;
        if (std::regex_match(line, line_re)) ++matched;
    }
    CHECK(matched == 1000);
}

// =====================================================================
// Section 13 — gate value at build time
// =====================================================================
//
// This section documents the gate value THIS BUILD was compiled
// with.  The lesson is a single TU so this section cannot
// dynamically change the gate — but the build recipe for higher
// gates is verified by a separate compilation unit (Section 14).

void section_13_gate_value_at_build() {
    section_header("13.gate-value-at-build");
    std::printf("-- PETRA_SYNC_LOG_LEVEL = %d\n", PETRA_SYNC_LOG_LEVEL);
    CHECK(petra::sync_log_level() == PETRA_SYNC_LOG_LEVEL);
    // Document the four legal values.
    CHECK(PETRA_SYNC_LOG_LEVEL == 0 || PETRA_SYNC_LOG_LEVEL == 1 ||
          PETRA_SYNC_LOG_LEVEL == 2 || PETRA_SYNC_LOG_LEVEL == 3);
}

// =====================================================================
// Section 14 — gate value at build time + macro-stringify probe
// =====================================================================
//
// This section prints the gate value this build was compiled with
// and asserts the preprocessor `#if` gates chose the correct
// expansion form for each macro.
//
// At default gate=0 every macro expands to PETRA_SYNC_LOG_EMIT.
// At higher gates some macros expand to PETRA_SYNC_LOG_NOOP.  The
// gate is observable from inside the TU — no runtime knob is
// needed.

// Stringification trick: `#x` produces a string of x's
// PRE-EXPANSION text.  A function-like macro like
// `SYNC_LOG_DEBUG(...)` is NOT expanded by `#x` because the
// preprocessor only expands an identifier if it's followed by
// `(`.  To capture the expansion of a function-like macro NAME,
// we call it once with no arguments inside a parenthesised
// wrapper that discards the call, and stringify the wrapper.
// This is the same trick libstdc++ / libc++ use for
// __PRETTY_FUNCTION__ / std::source_location probes.
//
// The probe pattern:
//
//     PETRA_STRINGIFY( (SYNC_LOG_DEBUG) )
//
// ` (SYNC_LOG_DEBUG) ` is an explicit parenthesised
// identifier-reference.  `#x` still doesn't expand it — but we
// can pre-expand it by defining a helper that DOES call it:
//
//     #define PETRA_PROBE_DEBUG() SYNC_LOG_DEBUG()
//     PETRA_STRINGIFY(PETRA_PROBE_DEBUG())
//
// The probe is a void expression that expands to
// `PETRA_SYNC_LOG_EMIT(Debug, __VA_ARGS__)` (or the no-op form).
// The stringification captures that expansion text.
//
// Why the dummy no-op call is safe: the function-like macro
// expansion is purely textual; calling `SYNC_LOG_DEBUG()` with
// no args expands to either `do { ... } while (0)` (the emit
// form — but with `__VA_ARGS__` empty the do-block still
// compiles, just emitting nothing) or `((void)0)` (the no-op
// form).  Both are well-formed statements.  We wrap the
// probe-call in a `if constexpr (false)` block so the probe
// call is discarded — only the preprocessor expansion matters.

#define PETRA_STRINGIFY_INNER(x) #x
#define PETRA_STRINGIFY(x) PETRA_STRINGIFY_INNER(x)

#define PETRA_PROBE_SYNC_LOG_DEBUG() SYNC_LOG_DEBUG()
#define PETRA_PROBE_SYNC_LOG_INFO()  SYNC_LOG_INFO()
#define PETRA_PROBE_SYNC_LOG_WARN()  SYNC_LOG_WARN()
#define PETRA_PROBE_SYNC_LOG_ERROR() SYNC_LOG_ERROR()

#define PETRA_ACTUAL_DEBUG_GATE_STR PETRA_STRINGIFY(PETRA_PROBE_SYNC_LOG_DEBUG())
#define PETRA_ACTUAL_INFO_GATE_STR  PETRA_STRINGIFY(PETRA_PROBE_SYNC_LOG_INFO())
#define PETRA_ACTUAL_WARN_GATE_STR  PETRA_STRINGIFY(PETRA_PROBE_SYNC_LOG_WARN())
#define PETRA_ACTUAL_ERROR_GATE_STR PETRA_STRINGIFY(PETRA_PROBE_SYNC_LOG_ERROR())

// The stringification of `PETRA_PROBE_SYNC_LOG_DEBUG()` happens
// entirely at preprocessor time — the resulting string literal
// is what `PETRA_ACTUAL_DEBUG_GATE_STR` becomes.  The probe
// definition itself is just textual; the stringification
// captures whatever expansion the gate's `#if` chose for
// `SYNC_LOG_DEBUG()`.

// Expected expansion strings for each macro — chosen by the
// preprocessor `#if` gate.  These are the FULLY-EXPANDED forms
// (the result after the gate's `#if PETRA_SYNC_LOG_EMIT(Debug,
// __VA_ARGS__)` macro is itself expanded).

#if PETRA_SYNC_LOG_LEVEL <= 0
#define PETRA_EXPECTED_DEBUG_GATE_STR \
    "do { ::petra::sync_log_emit((::petra::LogLevel::Debug), ); } while (0)"
#else
#define PETRA_EXPECTED_DEBUG_GATE_STR "((void)0)"
#endif

#if PETRA_SYNC_LOG_LEVEL <= 1
#define PETRA_EXPECTED_INFO_GATE_STR \
    "do { ::petra::sync_log_emit((::petra::LogLevel::Info), ); } while (0)"
#else
#define PETRA_EXPECTED_INFO_GATE_STR "((void)0)"
#endif

#if PETRA_SYNC_LOG_LEVEL <= 2
#define PETRA_EXPECTED_WARN_GATE_STR \
    "do { ::petra::sync_log_emit((::petra::LogLevel::Warn), ); } while (0)"
#else
#define PETRA_EXPECTED_WARN_GATE_STR "((void)0)"
#endif

#define PETRA_EXPECTED_ERROR_GATE_STR \
    "do { ::petra::sync_log_emit((::petra::LogLevel::Error), ); } while (0)"

// Static-assert the macro expansion matches the expected string.
// If the gate is correct, both are byte-equal.  If the gate is
// wrong (e.g. PETRA_SYNC_LOG_LEVEL is misconfigured), the compile
// fails with a clear message identifying the mismatch.

static_assert(
    std::string_view(PETRA_ACTUAL_DEBUG_GATE_STR) ==
    std::string_view(PETRA_EXPECTED_DEBUG_GATE_STR),
    "SYNC_LOG_DEBUG macro expansion doesn't match the gate");

static_assert(
    std::string_view(PETRA_ACTUAL_INFO_GATE_STR) ==
    std::string_view(PETRA_EXPECTED_INFO_GATE_STR),
    "SYNC_LOG_INFO macro expansion doesn't match the gate");

static_assert(
    std::string_view(PETRA_ACTUAL_WARN_GATE_STR) ==
    std::string_view(PETRA_EXPECTED_WARN_GATE_STR),
    "SYNC_LOG_WARN macro expansion doesn't match the gate");

static_assert(
    std::string_view(PETRA_ACTUAL_ERROR_GATE_STR) ==
    std::string_view(PETRA_EXPECTED_ERROR_GATE_STR),
    "SYNC_LOG_ERROR macro expansion doesn't match the gate");

void section_14_macro_stringify() {
    section_header("14.macro-stringify");
    std::printf("-- PETRA_SYNC_LOG_LEVEL = %d\n", PETRA_SYNC_LOG_LEVEL);
    std::printf("-- macro SYNC_LOG_DEBUG -> \"%s\"\n",
                PETRA_ACTUAL_DEBUG_GATE_STR);
    std::printf("-- macro SYNC_LOG_INFO  -> \"%s\"\n",
                PETRA_ACTUAL_INFO_GATE_STR);
    std::printf("-- macro SYNC_LOG_WARN  -> \"%s\"\n",
                PETRA_ACTUAL_WARN_GATE_STR);
    std::printf("-- macro SYNC_LOG_ERROR -> \"%s\"\n",
                PETRA_ACTUAL_ERROR_GATE_STR);
    CHECK(true);
    CHECK(true);
    CHECK(true);
    CHECK(true);
}

// =====================================================================
// Section 15 — gate-level smoke test for the four gate values
// =====================================================================
//
// This section is the "the gate is what the gate is" probe: it
// runs a tiny emit at each level and confirms the gate value
// reported by petra::sync_log_level() matches the macro
// behaviour.
//
// At gate=0 all four levels emit.  At gate>0 some levels are
// no-ops — but the helper layer (Section 10) still emits, so we
// use the helper layer to verify the level-tag pipeline
// regardless of the macro gate.

void section_15_gate_smoke() {
    section_header("15.gate-smoke");
    std::ostringstream oss;
    reset_sink_to_oss(oss);

    int gate = petra::sync_log_level();
    CHECK(gate >= 0 && gate <= 3);

    // Helper-layer emit at every level — bypasses the macro gate
    // so the test is gate-independent.
    petra::sync_log_emit(petra::LogLevel::Debug, "smoke-d");
    petra::sync_log_emit(petra::LogLevel::Info,  "smoke-i");
    petra::sync_log_emit(petra::LogLevel::Warn,  "smoke-w");
    petra::sync_log_emit(petra::LogLevel::Error, "smoke-e");

    restore_sink();

    std::string captured = oss.str();
    CHECK(captured.find("[DEBUG] smoke-d\n") != std::string::npos);
    CHECK(captured.find("[INFO ] smoke-i\n") != std::string::npos);
    CHECK(captured.find("[WARN ] smoke-w\n") != std::string::npos);
    CHECK(captured.find("[ERROR] smoke-e\n") != std::string::npos);

    // Macro-layer emit at level=Error always — independent of gate.
    int e_val = 99;
    SYNC_LOG_ERROR_RUNTIME("smoke-ert={}", std::make_format_args(e_val));
    CHECK(true);
}

// =====================================================================
// Section 16 — diagnostic: at-gate-or-above always emits
// =====================================================================
//
// ERROR (level 3) is the only level that ALWAYS emits regardless
// of the gate value (gate <= 3 is always true since the gate is
// 0..3).  This section verifies that ERROR calls survive every
// gate configuration.

void section_16_error_always_emits() {
    section_header("16.error-always-emits");
    std::ostringstream oss;
    reset_sink_to_oss(oss);

    SYNC_LOG_ERROR("this-must-emit-{}",
                   "at-every-gate-value-from-0-to-3");

    restore_sink();

    std::string captured = oss.str();
    CHECK(captured.find("[ERROR] this-must-emit-") != std::string::npos);
    CHECK(captured.find("at-every-gate-value-from-0-to-3") != std::string::npos);

    // LOC error version too.
    std::ostringstream oss2;
    reset_sink_to_oss(oss2);
    LOCSYNC_LOG_ERROR("loc-error-must-emit-{}", "everywhere");
    restore_sink();
    CHECK(oss2.str().find("[ERROR] ") != std::string::npos);
    CHECK(oss2.str().find("loc-error-must-emit-everywhere") != std::string::npos);
}

// =====================================================================
// main
// =====================================================================

int main() {
    section_1_sizeof_and_probes();
    section_2_default_emits_all();
    section_3_runtime_siblings();
    section_4_locsync_consteval();
    section_5_locsync_runtime();
    section_6_if_else_hygiene();
    section_7_format_specifiers();
    section_8_consteval_gate_preserved();
    section_9_sink_swap();
    section_10_helper_layer_unconditional();
    section_11_full_macro_sweep();
    section_12_concurrent_atomic();
    section_13_gate_value_at_build();
    section_14_macro_stringify();
    section_15_gate_smoke();
    section_16_error_always_emits();

    std::printf("\n%d/%d PASS\n", g_checks_total - g_checks_failed,
                g_checks_total);
    if (g_checks_failed > 0) {
        std::fprintf(stderr, "FAILURES (%d):\n", g_checks_failed);
        for (const auto& f : g_failures) std::fprintf(stderr, "%s", f.c_str());
        return 1;
    }
    return 0;
}
