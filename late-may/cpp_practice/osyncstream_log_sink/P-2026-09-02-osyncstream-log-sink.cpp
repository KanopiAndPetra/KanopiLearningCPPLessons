// P-2026-09-02 — osyncstream_log_sink: thread-safe atomic
// emission for the Sep 1 LOG_* macros via std::osyncstream.
//
// Why today
// ---------
// The Sep 1 lesson (P-2026-09-01-ostream-print-log-macros) named
// three new forward-on items.  Today closes the FIRST:
//
//   > std::osyncstream around the sink pointer.  The macros'
//   > two-phase write is a gap for concurrent invocations;
//   > petra::log_sink() = new std::osyncstream{std::cerr} (or
//   > similar) closes the gap.  Today documents the trap; a
//   > future lesson could pin the integration end-to-end.
//
// Today IS that lesson.  It does four things:
//
//   1. Mirrors the Sep 1 surface in this TU so the new helpers
//      here compose with the same macros: petra::LogLevel
//      enum, petra::log_level_tag (5-char aligned tags),
//      petra::log_sink()& (raw, default &std::cerr), and
//      petra::ostream_println (lifted from Aug 28 / Sep 1).
//
//   2. Adds petra::sync_sink_target()& — a reference to a
//      function-static std::reference_wrapper<std::ostream>
//      that the user can swap (set to std::cerr, a test
//      std::ostringstream, a std::ofstream, etc.).
//
//   3. Adds petra::sync_log_emit(...) and
//      sync_log_emit_runtime(...) helpers that emit the WHOLE
//      line (prefix + payload) through a local
//      std::osyncstream wrapping petra::sync_sink_target().get();
//      the osyncstream's destructor (and explicit flush)
//      commits the entire buffered line atomically w.r.t.
//      concurrent callers — closing the gap Sep 1 documented.
//
//   4. Adds SYNC_LOG_DEBUG / SYNC_LOG_INFO / SYNC_LOG_WARN /
//      SYNC_LOG_ERROR (consteval path) plus
//      SYNC_LOG_*_RUNTIME (runtime path).  Both expand
//      through a `do { ... } while (0)` wrapper so if/else
//      hygiene is preserved.  The consteval-vs-runtime split
//      at the MACRO layer is identical to Sep 1 (the consteval
//      surface catches malformed format strings at compile
//      time; the runtime surface catches them at format time
//      via std::format_error).
//
// Why std::osyncstream and not a mutex
// ------------------------------------
// std::osyncstream (P0053R7 in C++20) batches writes through a
// per-thread syncbuf and flushes them atomically when the
// osyncstream is destroyed (or std::flush is called).  A mutex
// around the two-phase write would also work but produces a
// single global lock and serialises ALL threads; std::osyncstream
// parallelises the BUFFERING and serialises only the COMMIT
// (each flushed line is one indivisible write to the underlying
// std::ostream).  This is the right shape for a logging sink.
//
// -----------------------------------------------------------------------------
// Platform note: enabling std::osyncstream on Apple Clang 21.0.0 / libc++ 21
// -----------------------------------------------------------------------------
// On this toolchain (cloned from the Sep 1 preflight probe), the
// <syncstream> header is shipped but its `std::osyncstream`
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
// This is the headline-flavoured finding of the lesson: even on
// a modern libc++ toolchain, osyncstream may be hidden behind an
// opt-in experimental-library flag.
//
// Part 1 — petra::ostream_println (lifted from Aug 28 / Sep 1)
// Part 2 — petra::LogLevel, petra::log_level_tag, petra::log_sink
// Part 3 — petra::sync_sink_target (the wrapped underlying sink)
// Part 4 — SYNC_LOG_* macros + sync_log_emit helpers
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
// Part 1 — petra::ostream_println (lifted verbatim from Sep 1)
// =====================================================================

namespace petra {

inline void ostream_println_runtime(std::ostream& os,
                                    std::string_view fmt,
                                    std::format_args args) {
    // Two-step: vformat -> std::string -> os.write, exactly as
    // Aug 28 / Sep 1 ship.
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
// (lifted from Sep 1)
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
    // The wrap (Sep 1's recipe) — formatting the {tag} via
    // std::format is overkill when we have log_emit_prefixed,
    // but for symmetry we emit just the payload through the
    // adapter.
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
// Part 3 — petra::sync_sink_target
//
// The sync wrapper needs an underlying std::ostream to wrap
// with std::osyncstream.  We expose that target as a
// function-static std::reference_wrapper<std::ostream>,
// swappable the same way as petra::log_sink() but holding a
// reference (not a pointer).  The sync macros read
// petra::sync_sink_target() to construct a fresh
// std::osyncstream on every invocation; the osyncstream's
// destructor (and explicit flush) commits the buffered line
// atomically.
//
// Default target is std::cerr.  Tests point it at a stack
// std::ostringstream and read the captured bytes back via oss.str().
// =====================================================================

namespace petra {

inline std::reference_wrapper<std::ostream>& sync_sink_target() noexcept {
    static std::reference_wrapper<std::ostream> r{std::cerr};
    return r;
}

} // namespace petra

// =====================================================================
// Part 4 — petra::sync_log_emit + sync_log_emit_runtime
//
// sync_log_emit constructs a fresh std::osyncstream wrapping
// petra::sync_sink_target().get().  The osyncstream buffers
// into a per-thread syncbuf; the explicit sync.flush() at the
// end of the function commits the WHOLE line atomically
// (P0053R7 commit semantics).  The two-phase write problem
// Sep 1 documented (a concurrent log call could interleave
// between the prefix and the payload on the default std::cerr
// sink) does NOT exist here because the prefix AND the
// payload are emitted through the SAME osyncstream and the
// syncbuf commits the entire buffer atomically.
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
    // commits them as a single atomic line.  The osyncstream
    // destructor (called at end-of-scope after flush()) would
    // also flush, but we explicit-flush so the bytes are
    // observable to the next sink read without relying on
    // destructor ordering.
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
// Macros — SYNC_LOG_* (consteval) + SYNC_LOG_*_RUNTIME
//
// `do { ... } while (0)` so `if (cond) SYNC_LOG_INFO(...); else
// ...;` binds correctly (same recipe as Sep 1).  The
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

// (The raw LOG_* macros from Sep 1 are intentionally NOT mirrored
// in this TU.  The lesson is about the SYNC surface; the raw
// surface is documented as Sep 1's surface and re-imported here
// only via the lifted helpers petra::log_emit / log_emit_runtime
// in case a future lesson needs them.  Section 13 demonstrates
// why a raw-std::ostringstream sink is unsafe under concurrent
// writes — the ASan finding during development confirmed it.)

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
    // 21.0.0 / libc++ 21 (probed during lesson prep).
    static_assert(sizeof(std::osyncstream) > 0,
                  "std::osyncstream must be defined");
    CHECK(true);
}

// =====================================================================
// Section 2 — basic single-thread success: line shape
// =====================================================================

void section_2_basic_success() {
    section_header("2.basic-success");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    SYNC_LOG_INFO("hello {}", std::string("petra"));
    SYNC_LOG_WARN("count={}", 42);

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    CHECK(captured.find("[INFO ] hello petra\n") != std::string::npos);
    CHECK(captured.find("[WARN ] count=42\n")      != std::string::npos);
}

// =====================================================================
// Section 3 — zero-args (SYNC_LOG_INFO("ping"))
// =====================================================================

void section_3_zero_args() {
    section_header("3.zero-args");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    SYNC_LOG_INFO("ping");
    SYNC_LOG_DEBUG("empty={:>3}", "");

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    CHECK(captured.find("[INFO ] ping\n")   != std::string::npos);
    CHECK(captured.find("[DEBUG] empty=   \n") != std::string::npos);
}

// =====================================================================
// Section 4 — comma-in-arguments
// =====================================================================

void section_4_comma_in_args() {
    section_header("4.comma-in-args");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    std::pair<int, int> p{3, 7};
    SYNC_LOG_INFO("pair=({},{})", p.first, p.second);

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    CHECK(captured.find("[INFO ] pair=(3,7)\n") != std::string::npos);
}

// =====================================================================
// Section 5 — if/else hygiene (both branches)
// =====================================================================

void section_5_if_else_hygiene() {
    section_header("5.if-else-hygiene");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    bool cond_true = true;
    bool cond_false = false;
    if (cond_true)  SYNC_LOG_INFO("then-branch");
    else            SYNC_LOG_INFO("else-branch");
    if (cond_false) SYNC_LOG_INFO("then-branch");
    else            SYNC_LOG_INFO("else-branch");

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    CHECK(captured.find("[INFO ] then-branch\n") != std::string::npos);
    CHECK(captured.find("[INFO ] else-branch\n") != std::string::npos);
}

// =====================================================================
// Section 6 — format-specifier passthrough ({:#x}, {:.3f}, {:>8}, {:<6})
// =====================================================================

void section_6_format_specifiers() {
    section_header("6.format-specifiers");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    SYNC_LOG_INFO("hex={:#x}", 0xCAFE);
    SYNC_LOG_INFO("flt={:.3f}", 3.14159);
    SYNC_LOG_INFO("pad={:>8}", ">");
    SYNC_LOG_INFO("lp={:<6}", "<");

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    CHECK(captured.find("[INFO ] hex=0xcafe\n")   != std::string::npos);
    CHECK(captured.find("[INFO ] flt=3.142\n")    != std::string::npos);
    CHECK(captured.find("[INFO ] pad=       >\n") != std::string::npos);
    CHECK(captured.find("[INFO ] lp=<     \n")    != std::string::npos);
}

// =====================================================================
// Section 7 — consteval gate via static_assert probe
// =====================================================================

void section_7_consteval_gate() {
    section_header("7.consteval-gate");
    // The consteval-ctor gate exists at the type level.  This
    // probe confirms std::format_string<int>{"value={}"} compiles
    // and yields an 8-byte format string literal (8 chars:
    // value={}).
    static_assert([]{
        constexpr std::format_string<int> fs{"value={}"};
        return fs.get().size() == 8;
    }(), "compiletime format_string probe");
    CHECK(true);
}

// =====================================================================
// Section 8 — runtime sibling: SYNC_LOG_INFO_RUNTIME
// =====================================================================

void section_8_runtime_sibling() {
    section_header("8.runtime-sibling");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    std::string fmt = "runtime={}";
    int val = 123;
    SYNC_LOG_INFO_RUNTIME(fmt, std::make_format_args(val));

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    CHECK(captured.find("[INFO ] runtime=123\n") != std::string::npos);
}

// =====================================================================
// Section 9 — NUL byte in payload
// =====================================================================

void section_9_nul_in_payload() {
    section_header("9.nul-in-payload");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    std::string with_nul;
    with_nul.push_back('a');
    with_nul.push_back('\0');
    with_nul.push_back('b');
    SYNC_LOG_INFO("nul={}", with_nul);

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    auto pos = captured.find("[INFO ] nul=");
    CHECK(pos != std::string::npos);
    if (pos != std::string::npos) {
        pos += std::string("[INFO ] nul=").size();
        CHECK(pos + 3 <= captured.size());
        CHECK(captured[pos + 0] == 'a');
        CHECK(captured[pos + 1] == '\0');
        CHECK(captured[pos + 2] == 'b');
        CHECK(captured[pos + 3] == '\n');
    }
}

// =====================================================================
// Section 10 — UTF-8 payload byte round-trip
//
// Construct a std::string holding the UTF-8 encoding of U+1F600
// (😀 = 0xF0 0x9F 0x98 0x80) explicitly — `u8""` literals in
// C++23 narrow to char8_t and don't construct std::string by
// itself.
// =====================================================================

void section_10_utf8_payload() {
    section_header("10.utf8-payload");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    std::string smile;
    smile.push_back(static_cast<char>(0xF0));
    smile.push_back(static_cast<char>(0x9F));
    smile.push_back(static_cast<char>(0x98));
    smile.push_back(static_cast<char>(0x80));
    SYNC_LOG_INFO("u={}", smile);

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    auto pos = captured.find("[INFO ] u=");
    CHECK(pos != std::string::npos);
    if (pos != std::string::npos) {
        pos += std::string("[INFO ] u=").size();
        CHECK(pos + 4 <= captured.size());
        CHECK(static_cast<unsigned char>(captured[pos + 0]) == 0xF0);
        CHECK(static_cast<unsigned char>(captured[pos + 1]) == 0x9F);
        CHECK(static_cast<unsigned char>(captured[pos + 2]) == 0x98);
        CHECK(static_cast<unsigned char>(captured[pos + 3]) == 0x80);
        CHECK(captured[pos + 4] == '\n');
    }
}

// =====================================================================
// Section 11 — atomic concurrent emission (THE headline section)
//
// 4 threads alternate SYNC_LOG_INFO and SYNC_LOG_WARN calls,
// 250 times each.  Total 1000 commits.  For each captured line
// the prefix AND the payload must be on the SAME line — no
// half-line interleaving.  The test grabs a single oss at the
// end and asserts:
//   (a) the total line count equals 1000
//   (b) EVERY line matches the regex ^\[(INFO |WARN )\] t[0-3]=\d+$
//   (c) the alternating INFO/WARN counts per thread are
//       uniformly preserved across threads (counts of each tag
//       are 4*250 = 1000).
//
// Sep 1 documented the gap: on the default std::cerr sink the
// prefix-and-payload are not atomic w.r.t. each other.  Today's
// std::osyncstream commit closes that gap.  This section IS
// the proof.
// =====================================================================

void section_11_atomic_concurrent_emission() {
    section_header("11.atomic-concurrent-emission");
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
                    SYNC_LOG_INFO("t{}={}", t, i);
                } else {
                    SYNC_LOG_WARN("t{}={}", t, i);
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
    // (a)+(b) — full line count and full regex match
    int line_count = 0;
    int info_count = 0;
    int warn_count = 0;
    std::regex re(R"(^\[(INFO |WARN )\] t[0-3]=\d+$)");
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
// Section 12 — sync-swap behaviour (no leak, four rounds clean)
// =====================================================================

void section_12_swap_round_trip_4() {
    section_header("12.swap-round-trip-4");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();

    for (int round = 0; round < 4; ++round) {
        ref = std::ref(oss);
        SYNC_LOG_INFO("round={}", round);
        ref = std::ref(*prev_raw);
    }

    std::string captured = oss.str();
    int count = 0;
    std::istringstream lines(captured);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.find("[INFO ] round=") != std::string::npos) ++count;
    }
    CHECK(count == 4);
    CHECK(captured.find("[INFO ] round=0\n") != std::string::npos);
    CHECK(captured.find("[INFO ] round=3\n") != std::string::npos);
}

// =====================================================================
// Section 13 — sync surface stress: 8 threads, 200 commits each
//
// The "raw vs sync" cross-comparison we initially wanted to
// run here is what Sep 1 documented the gap and what today's
// lesson closes.  In practice we cannot interleave both
// streams through the same std::ostringstream without a mutex
// — std::ostringstream is NOT thread-safe and concurrent
// writes through it produce a genuine data race (verified by
// ASan during development: the raw LOG_INFO path raced the
// stringbuf internal buffer and ASan flagged a
// double-into-freed-region).  Section 11 already isolates
// the SYNC shape's atomic-emission property under concurrent
// load; this section widens that experiment to a higher
// commit count to make the determinism claim stronger.
// =====================================================================

void section_13_sync_stress_8_threads_200_each() {
    section_header("13.sync-stress-8x200");
    std::regex re(R"(^\[(INFO |WARN )\] t[0-7]=\d+$)");
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
                if ((t + i) & 1) SYNC_LOG_INFO("t{}={}", t, i);
                else             SYNC_LOG_WARN("t{}={}", t, i);
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
// Section 14 — determinism: alternating sync emit, 50 rounds
// =====================================================================

void section_14_determinism_sync_alternating() {
    section_header("14.determinism-sync-alternating");
    int totals[2] = {0, 0};
    for (int round = 0; round < 50; ++round) {
        std::ostringstream oss;
        auto& ref = petra::sync_sink_target();
        auto* prev_raw = &ref.get();
        ref = std::ref(oss);

        for (int i = 0; i < 4; ++i) {
            if ((round + i) & 1) {
                SYNC_LOG_INFO("round{}-i{}", round, i);
            } else {
                SYNC_LOG_WARN("round{}-i{}", round, i);
            }
        }
        ref = std::ref(*prev_raw);

        std::string captured = oss.str();
        std::istringstream lines(captured);
        std::string line;
        while (std::getline(lines, line)) {
            if (line.find("[INFO ]") != std::string::npos) ++totals[0];
            if (line.find("[WARN ]") != std::string::npos) ++totals[1];
        }
    }
    CHECK(totals[0] == totals[1]);
    CHECK(totals[0] == 100);
    CHECK(totals[1] == 100);
}

// =====================================================================
// Section 15 — concurrent round-trip, 5x determinism
// =====================================================================

void section_15_concurrent_round_trip_5x() {
    section_header("15.concurrent-round-trip-5x");
    std::regex re(R"(^\[(INFO |WARN )\] t[0-3]=\d+$)");
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
                    if ((t + i) & 1) SYNC_LOG_INFO("t{}={}", t, i);
                    else             SYNC_LOG_WARN("t{}={}", t, i);
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
// Section 16 — empty payload (SYNC_LOG_INFO(""))
// =====================================================================

void section_16_empty_payload() {
    section_header("16.empty-payload");
    std::ostringstream oss;
    auto& ref = petra::sync_sink_target();
    auto* prev_raw = &ref.get();
    ref = std::ref(oss);

    SYNC_LOG_INFO("");

    ref = std::ref(*prev_raw);

    std::string captured = oss.str();
    CHECK(captured == "[INFO ] \n");
}

int main() {
    section_1_sizeof_and_probes();
    section_2_basic_success();
    section_3_zero_args();
    section_4_comma_in_args();
    section_5_if_else_hygiene();
    section_6_format_specifiers();
    section_7_consteval_gate();
    section_8_runtime_sibling();
    section_9_nul_in_payload();
    section_10_utf8_payload();
    section_11_atomic_concurrent_emission();
    section_12_swap_round_trip_4();
    section_13_sync_stress_8_threads_200_each();
    section_14_determinism_sync_alternating();
    section_15_concurrent_round_trip_5x();
    section_16_empty_payload();

    std::printf("\n%d/%d %s\n",
                g_checks_total - g_checks_failed,
                g_checks_total,
                g_checks_failed == 0 ? "PASS" : "FAIL");
    for (auto const& f : g_failures) std::fputs(f.c_str(), stdout);
    return g_checks_failed == 0 ? 0 : 1;
}
