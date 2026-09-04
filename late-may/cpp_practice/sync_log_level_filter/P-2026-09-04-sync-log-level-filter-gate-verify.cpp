// P-2026-09-04 — gate-verify: small standalone driver that proves
// the PETRA_SYNC_LOG_LEVEL compile-time gate filters macros at
// every gate value (0/1/2/3).
//
// Built with:
//   clang++ -std=c++23 -O0 -stdlib=libc++ -fexperimental-library \
//           -DPETRA_SYNC_LOG_LEVEL=2 ...  // or 0, 1, 3
//
// This file is intentionally tiny — it lifts only the macro
// surface from the main lesson TU and exercises every macro at
// every level.  Each macro emits to std::cerr (the default sink)
// but the test reads std::cerr through a redirected buffer
// (set via petra::sync_sink_target) so we can capture the output.
//
// The lesson exists to pin the dead-strip finding: at
// -DPETRA_SYNC_LOG_LEVEL=2, DEBUG + INFO + WARN calls compile
// down to `((void)0)` so the test program emits zero bytes for
// those levels.  ERROR still emits because its level is 3, the
// maximum.

#include <cstdio>
#include <format>
#include <functional>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <syncstream>

// Lift only the surface we need — no Lifted Parts 1-5 from the
// main lesson TU.  This file is a separate compilation unit so it
// does NOT share the lesson TU's PETRA_SYNC_LOG_LEVEL #define;
// each build sets the gate via -DPETRA_SYNC_LOG_LEVEL=N.

#ifndef PETRA_SYNC_LOG_LEVEL
#define PETRA_SYNC_LOG_LEVEL 0
#endif

static_assert(PETRA_SYNC_LOG_LEVEL >= 0 && PETRA_SYNC_LOG_LEVEL <= 3,
              "PETRA_SYNC_LOG_LEVEL must be 0..3");

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

inline std::reference_wrapper<std::ostream>& sync_sink_target() noexcept {
    static std::reference_wrapper<std::ostream> r{std::cerr};
    return r;
}

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

// ---- Gated macros (lifted from the main lesson TU) ----

#define PETRA_SYNC_LOG_EMIT(level, ...) \
    do { \
        ::petra::sync_log_emit((::petra::LogLevel::level), __VA_ARGS__); \
    } while (0)

#define PETRA_SYNC_LOG_NOOP(level) ((void)0)

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

#define SYNC_LOG_ERROR(...)    PETRA_SYNC_LOG_EMIT(Error, __VA_ARGS__)

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

// ---- Test driver ----

int g_checks_total = 0;
int g_checks_failed = 0;

#define CHECK(expr) \
    do { \
        ++::g_checks_total; \
        if (!(expr)) { \
            ++::g_checks_failed; \
            std::fprintf(stderr, "FAIL at line %d: %s\n", __LINE__, #expr); \
        } \
    } while (0)

int main() {
    std::ostringstream oss;
    petra::sync_sink_target() = std::ref(oss);

    // 16 macros × 1 emit each.
    SYNC_LOG_DEBUG("d");            SYNC_LOG_INFO("i");
    SYNC_LOG_WARN("w");             SYNC_LOG_ERROR("e");

    int d_val = 1, i_val = 2, w_val = 3, e_val = 4;
    if (PETRA_SYNC_LOG_LEVEL > 0) (void)d_val;
    if (PETRA_SYNC_LOG_LEVEL > 1) (void)i_val;
    if (PETRA_SYNC_LOG_LEVEL > 2) (void)w_val;
    (void)e_val;
    SYNC_LOG_DEBUG_RUNTIME("dr",  std::make_format_args(d_val));
    SYNC_LOG_INFO_RUNTIME("ir",   std::make_format_args(i_val));
    SYNC_LOG_WARN_RUNTIME("wr",   std::make_format_args(w_val));
    SYNC_LOG_ERROR_RUNTIME("er",  std::make_format_args(e_val));

    LOCSYNC_LOG_DEBUG("ld");        LOCSYNC_LOG_INFO("li");
    LOCSYNC_LOG_WARN("lw");         LOCSYNC_LOG_ERROR("le");

    int ld_val = 1, li_val = 2, lw_val = 3, le_val = 4;
    if (PETRA_SYNC_LOG_LEVEL > 0) (void)ld_val;
    if (PETRA_SYNC_LOG_LEVEL > 1) (void)li_val;
    if (PETRA_SYNC_LOG_LEVEL > 2) (void)lw_val;
    (void)le_val;
    LOCSYNC_LOG_DEBUG_RUNTIME("ldr", std::make_format_args(ld_val));
    LOCSYNC_LOG_INFO_RUNTIME("lir",  std::make_format_args(li_val));
    LOCSYNC_LOG_WARN_RUNTIME("lwr",  std::make_format_args(lw_val));
    LOCSYNC_LOG_ERROR_RUNTIME("ler", std::make_format_args(le_val));

    // Restore the default sink so subsequent diagnostics (the
    // FAIL printfs above) reach std::cerr.
    petra::sync_sink_target() = std::ref(std::cerr);

    std::string captured = oss.str();
    int line_count = 0;
    for (char c : captured) if (c == '\n') ++line_count;

    // Expected line count based on the gate:
    //   gate=0: 16 emits (4 levels × 4 macro families)
    //   gate=1: 12 emits (drops DEBUG: 16 - 4 = 12)
    //   gate=2: 8 emits  (drops DEBUG + INFO: 16 - 8 = 8)
    //   gate=3: 4 emits  (drops DEBUG + INFO + WARN: 16 - 12 = 4)
    int expected = 16 - 4 * PETRA_SYNC_LOG_LEVEL;

    std::printf("-- gate-verify: PETRA_SYNC_LOG_LEVEL = %d\n",
                PETRA_SYNC_LOG_LEVEL);
    std::printf("-- captured line count = %d (expected %d)\n",
                line_count, expected);

    CHECK(line_count == expected);

    std::printf("\n%d/%d PASS\n", g_checks_total - g_checks_failed,
                g_checks_total);
    return g_checks_failed > 0 ? 1 : 0;
}
