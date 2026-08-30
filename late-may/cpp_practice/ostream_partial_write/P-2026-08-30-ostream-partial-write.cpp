// P-2026-08-30 — ostream_write_checked: partial-write detection for the
// Aug 28 std::print(std::ostream&, ...) adapter.
//
// Why today
// ---------
// The Aug 28 (P-2026-08-28) lesson shipped petra::ostream_print /
// ostream_println / ostream_print_via_buf — a consumer-side
// std::print(std::ostream&, ...) adapter closing the overload gap
// libc++ 21 leaves open. Its "Where we go next" section listed four
// new follow-on items. The FIRST of those four is:
//
//   > std::ostream_print for a custom std::streambuf that fails to
//   > consume all chars — some streambufs (e.g. compression streams,
//   > network sinks) signal "downstream full" by failing to consume;
//   > today's ostreambuf_iterator silently reports end-of-stream and
//   > the adapter treats this as a successful truncating write. A
//   > future lesson could pin this case and emit a partial_write
//   > error kind on truncation.
//
// Today IS that lesson. We build the non-consuming sink that Aug 28
// only hypothesised (capped_streambuf), prove the silent-truncation
// bug is real, and then ship petra::ostream_write_checked returning
// std::expected<std::size_t, petra::WriteError> with a PartialWrite
// error kind carrying BOTH the requested and the accepted byte count.
//
// The headline finding
// --------------------
// The two adapter paths Aug 28 shipped fail DIFFERENTLY, and neither
// failure mode is the one you would guess:
//
//   | signal                 | vformat->string->os.write | vformat_to->ostreambuf_iterator |
//   |------------------------|---------------------------|---------------------------------|
//   | os.bad() after short   |          true             |            FALSE                |
//   | os.fail() after short  |          true             |            FALSE                |
//   | returned it.failed()   |          n/a              |            true                 |
//   | original it.failed()   |          n/a              |            FALSE (copy!)        |
//   | byte count available   |          no               |            no                   |
//
// std::ostreambuf_iterator writes through the streambuf DIRECTLY
// (sb->sputc), never touching basic_ios::rdstate. So the Aug 28
// streambuf path drops bytes with the stream still reporting good().
// That is exactly the silent truncation Aug 28 predicted — confirmed
// here in section 4.
//
// And the failed() flag lives on the RETURNED iterator, not the one
// you passed in: std::vformat_to takes the output iterator BY VALUE
// and copies it on every advance (the same copy-semantics trap the
// Aug 25/26/28 lessons hit with their custom bounded iterators). The
// caller's original copy still reports failed() == false. Section 5
// pins this.
//
// Neither path can tell you HOW MANY bytes landed. So the durable
// mechanism this lesson settles on is a counting_streambuf filter
// spliced between the ostream and its real buffer: it forwards every
// byte downstream and records offered vs. accepted. That works for
// BOTH paths, for any sink, and yields the exact counts the
// PartialWrite error needs.
//
// What this lesson is NOT
// -----------------------
//   - It is NOT the Aug 25/26/27 bounded-buffer family. Those cap
//     output at a caller-supplied n and report a would-be size.
//     Today the cap lives in the SINK and is not known to the caller
//     at all; truncation is discovered after the fact.
//   - It does NOT roll back a partial write. Sections 9 and 10 show
//     the format_error path IS atomic (libc++ 21 emits nothing before
//     throwing, verified up to a 5000-char prefix), but the
//     PartialWrite path is not recoverable — the bytes the sink
//     accepted are gone downstream. The error reports the split.
//   - It does NOT use std::print. libc++ 21 has no ostream overload;
//     that absence is what the Aug 28 adapter and today's checked
//     sibling exist for.
//
// C++ standard: C++23. Uses std::vformat / std::vformat_to (P2216R3),
// std::make_format_args, std::format_error, std::expected (P0323R12),
// and a user-defined std::formatter specialisation (P2286R6-adjacent).

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <expected>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <ostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// ============================================================================
// Part 1 — the non-consuming sink Aug 28 only hypothesised
// ============================================================================
//
// A streambuf that accepts at most `cap` bytes and then refuses. This
// is the shape of a compression stream whose window is full, a network
// sink whose socket buffer is full and is non-blocking, or a ring
// buffer that has wrapped. The refusal protocol is the standard one:
//
//   - overflow(ch) returns traits_type::eof() to mean "I did not take
//     that char" ([streambuf.virt.put]).
//   - xsputn(s, n) returns a value < n to mean "I only took that many".
//
// Both refusals are what a real sink does. Neither throws.

namespace petra {

class capped_streambuf : public std::streambuf {
public:
    explicit capped_streambuf(std::size_t cap) noexcept : cap_(cap) {}

    [[nodiscard]] const std::string& str() const noexcept { return sink_; }
    [[nodiscard]] std::size_t consumed() const noexcept { return sink_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return cap_; }
    [[nodiscard]] std::size_t refusals() const noexcept { return refusals_; }

protected:
    int_type overflow(int_type ch) override {
        if (traits_type::eq_int_type(ch, traits_type::eof())) {
            return traits_type::not_eof(ch);   // flush request, not a char
        }
        if (sink_.size() >= cap_) {
            ++refusals_;
            return traits_type::eof();          // "downstream full"
        }
        sink_.push_back(traits_type::to_char_type(ch));
        return ch;
    }

    // The bulk path. A short return is the "I only took this many"
    // signal; std::ostream::write turns that into badbit.
    std::streamsize xsputn(const char* s, std::streamsize n) override {
        const std::size_t want = static_cast<std::size_t>(n);
        const std::size_t room = cap_ - std::min(cap_, sink_.size());
        const std::size_t take = std::min(room, want);
        if (take < want) {
            ++refusals_;
        }
        sink_.append(s, take);
        return static_cast<std::streamsize>(take);
    }

private:
    std::size_t cap_;
    std::string sink_;
    std::size_t refusals_ = 0;
};

// ============================================================================
// Part 2 — the counting filter: the durable byte-accounting mechanism
// ============================================================================
//
// Neither adapter path reports a byte count on truncation. This filter
// supplies one. It is spliced between the ostream and the ostream's
// real streambuf, forwards every byte, and records:
//
//   offered  — bytes the formatter handed to the stream
//   accepted — bytes the downstream buffer actually took
//
// offered > accepted is the exact definition of a partial write, and it
// is sink-agnostic: it works for capped_streambuf, for a socket
// streambuf, for a filebuf on a full disk, for anything. It also works
// identically for BOTH adapter paths, which is why the checked adapter
// below routes everything through it instead of reading os.bad() or
// iterator.failed().
//
// It intentionally does NOT swallow the downstream refusal: the return
// values are forwarded verbatim so os.write still sets badbit and
// ostreambuf_iterator still sets failed(). The filter observes; it does
// not change behaviour.

class counting_streambuf : public std::streambuf {
public:
    explicit counting_streambuf(std::streambuf* down) noexcept : down_(down) {}

    [[nodiscard]] std::size_t offered() const noexcept { return offered_; }
    [[nodiscard]] std::size_t accepted() const noexcept { return accepted_; }
    [[nodiscard]] bool truncated() const noexcept { return accepted_ < offered_; }

    void reset() noexcept {
        offered_ = 0;
        accepted_ = 0;
    }

protected:
    int_type overflow(int_type ch) override {
        if (traits_type::eq_int_type(ch, traits_type::eof())) {
            return traits_type::not_eof(ch);
        }
        ++offered_;
        const int_type r = down_->sputc(traits_type::to_char_type(ch));
        if (!traits_type::eq_int_type(r, traits_type::eof())) {
            ++accepted_;
        }
        return r;
    }

    std::streamsize xsputn(const char* s, std::streamsize n) override {
        offered_ += static_cast<std::size_t>(n);
        const std::streamsize wrote = down_->sputn(s, n);
        accepted_ += static_cast<std::size_t>(wrote);
        return wrote;
    }

    int sync() override { return down_->pubsync(); }

private:
    std::streambuf* down_;
    std::size_t offered_ = 0;
    std::size_t accepted_ = 0;
};

// ----------------------------------------------------------------------------
// RAII guard that splices the filter in and takes it back out.
// ----------------------------------------------------------------------------
//
// Two traps this guard exists to handle, both verified in section 11:
//
//  1. basic_ios::rdbuf(sb*) calls clear() ([ios.members]/6). Installing
//     the filter therefore ERASES any badbit/failbit the stream already
//     carried, and restoring it erases the badbit the short write just
//     set. A naive guard silently launders stream errors. This one saves
//     rdstate() on entry, and on exit restores the union of the saved
//     state and the state accumulated while the filter was installed.
//
//  2. std::ostringstream::rdbuf() and std::ofstream::rdbuf() are
//     zero-argument member functions that HIDE the inherited
//     basic_ios::rdbuf(streambuf*) one-argument overload. Calling
//     oss.rdbuf(&filter) is a hard compile error ("too many arguments
//     to function call, expected 0, have 1"). Binding to std::ostream&
//     (as this guard's constructor parameter does) makes the inherited
//     overload visible again. Section 11 documents the workaround for
//     the case where you hold the derived type.

class scoped_counting_filter {
public:
    explicit scoped_counting_filter(std::ostream& os)
        : os_(os),
          saved_state_(os.rdstate()),
          filter_(os.rdbuf()),
          // rdbuf(sb*) calls clear(); saved_state_ was captured first.
          previous_(os.rdbuf(&filter_)) {}

    scoped_counting_filter(const scoped_counting_filter&) = delete;
    scoped_counting_filter& operator=(const scoped_counting_filter&) = delete;

    ~scoped_counting_filter() {
        // Capture the state the write produced BEFORE the restoring
        // rdbuf() call clears it.
        const std::ios_base::iostate produced = os_.rdstate();
        (void)os_.rdbuf(previous_);          // this calls clear()
        os_.setstate(saved_state_ | produced);
    }

    [[nodiscard]] std::size_t offered() const noexcept { return filter_.offered(); }
    [[nodiscard]] std::size_t accepted() const noexcept { return filter_.accepted(); }
    [[nodiscard]] bool truncated() const noexcept { return filter_.truncated(); }

private:
    std::ostream& os_;
    std::ios_base::iostate saved_state_;
    counting_streambuf filter_;
    std::streambuf* previous_;
};

// ============================================================================
// Part 3 — the error type
// ============================================================================
//
// Two failure kinds, matching the two things that can go wrong once the
// sink is allowed to refuse bytes:
//
//   Format       — the format string / arguments are bad. std::vformat
//                  or std::vformat_to threw std::format_error. Sections
//                  9 and 10 show this path is ATOMIC on libc++ 21: zero
//                  bytes reach the sink before the throw.
//   PartialWrite — the text formatted fine but the sink refused some of
//                  it. `requested` and `accepted` carry the split.
//
// This follows the Aug 27 LogError shape (domain error type projected
// out of std::format_error) rather than leaking std::format_error to
// the caller, and for the same reason: the caller wants to branch on a
// kind, not parse a message.

enum class WriteErrorKind { Format, PartialWrite };

[[nodiscard]] constexpr std::string_view write_error_kind_name(WriteErrorKind k) noexcept {
    switch (k) {
        case WriteErrorKind::Format:       return "Format";
        case WriteErrorKind::PartialWrite: return "PartialWrite";
    }
    return "?";
}

struct WriteError {
    WriteErrorKind kind{};
    std::string    message{};
    std::size_t    requested = 0;   // bytes the formatted text offered
    std::size_t    accepted  = 0;   // bytes the sink actually took

    [[nodiscard]] std::size_t dropped() const noexcept {
        return requested - std::min(requested, accepted);
    }

    friend bool operator==(const WriteError&, const WriteError&) = default;
};

} // namespace petra

// libc++ 21 has no formatter for arbitrary user types, so the CHECK
// macros below cannot print a WriteError without one. Same situation
// the Aug 27 lesson hit with LogError; same fix.
template <>
struct std::formatter<petra::WriteError> : std::formatter<std::string> {
    auto format(const petra::WriteError& e, std::format_context& ctx) const {
        return std::formatter<std::string>::format(
            std::format("WriteError{{{}, requested={}, accepted={}, dropped={}, msg=\"{}\"}}",
                        petra::write_error_kind_name(e.kind),
                        e.requested, e.accepted, e.dropped(), e.message),
            ctx);
    }
};

// ============================================================================
// Part 4 — the checked adapters
// ============================================================================

namespace petra {

// The runtime-format-string entry point. The consteval-checked
// template forwards to it — the same split Aug 28 established, forced
// by std::format_string<Args...>'s consteval constructor, which cannot
// be initialised from a runtime std::string_view.
//
// Returns the number of bytes the sink accepted on success.
//
// Path: std::vformat -> std::string -> os.write, under a spliced
// counting filter. The format happens BEFORE any byte moves, which is
// what makes the Format error path atomic by construction here (the
// throw escapes before os.write is ever reached).
[[nodiscard]] inline std::expected<std::size_t, WriteError>
ostream_write_checked_runtime(std::ostream& os,
                              std::string_view fmt,
                              std::format_args args) {
    std::string text;
    try {
        text = std::vformat(fmt, args);
    } catch (const std::format_error& e) {
        return std::expected<std::size_t, WriteError>{
            std::unexpect,
            WriteError{WriteErrorKind::Format, e.what(), 0, 0}};
    }

    std::size_t accepted = 0;
    {
        scoped_counting_filter guard{os};
        os.write(text.data(), static_cast<std::streamsize>(text.size()));
        accepted = guard.accepted();
    }   // guard restores the real streambuf and re-applies rdstate here

    if (accepted < text.size()) {
        return std::expected<std::size_t, WriteError>{
            std::unexpect,
            WriteError{WriteErrorKind::PartialWrite,
                       "sink refused bytes",
                       text.size(),
                       accepted}};
    }
    return accepted;
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, WriteError>
ostream_write_checked(std::ostream& os,
                      std::format_string<Args...> fmt,
                      Args&&... args) {
    return petra::ostream_write_checked_runtime(
        os, std::string_view{fmt.get()}, std::make_format_args(args...));
}

// The zero-allocation sibling: vformat_to straight through an
// ostreambuf_iterator, still under the counting filter. Same contract,
// same error type, no intermediate std::string.
//
// Note the two independent truncation signals available on this path:
// the returned iterator's failed(), and the filter's offered/accepted
// split. We assert they AGREE in section 8 — but only the filter can
// say how many bytes were lost.
[[nodiscard]] inline std::expected<std::size_t, WriteError>
ostream_write_checked_via_buf_runtime(std::ostream& os,
                                      std::string_view fmt,
                                      std::format_args args) {
    std::size_t offered = 0;
    std::size_t accepted = 0;
    bool iterator_failed = false;
    std::string what;
    bool threw = false;

    {
        scoped_counting_filter guard{os};
        try {
            using It = std::ostreambuf_iterator<char>;
            const It done = std::vformat_to(It{os.rdbuf()}, fmt, args);
            // failed() must be read off the RETURNED iterator; the one
            // we constructed was copied by value on every advance and
            // still reports false. See section 5.
            iterator_failed = done.failed();
        } catch (const std::format_error& e) {
            threw = true;
            what = e.what();
        }
        offered = guard.offered();
        accepted = guard.accepted();
    }

    if (threw) {
        // The measured `accepted` is carried into the error so callers
        // can see whether anything leaked before the throw. Section 10
        // shows libc++ 21 keeps this at 0 for every payload size tried.
        return std::expected<std::size_t, WriteError>{
            std::unexpect,
            WriteError{WriteErrorKind::Format, what, offered, accepted}};
    }
    if (iterator_failed || accepted < offered) {
        return std::expected<std::size_t, WriteError>{
            std::unexpect,
            WriteError{WriteErrorKind::PartialWrite,
                       "sink refused bytes",
                       offered,
                       accepted}};
    }
    return accepted;
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, WriteError>
ostream_write_checked_via_buf(std::ostream& os,
                              std::format_string<Args...> fmt,
                              Args&&... args) {
    return petra::ostream_write_checked_via_buf_runtime(
        os, std::string_view{fmt.get()}, std::make_format_args(args...));
}

// The Aug 28 unchecked adapters, re-declared here so the sections can
// contrast checked against unchecked on the same sink. Byte-identical
// to the Aug 28 lesson's implementations.
inline void ostream_print_runtime(std::ostream& os,
                                  std::string_view fmt,
                                  std::format_args args) {
    const std::string s = std::vformat(fmt, args);
    os.write(s.data(), static_cast<std::streamsize>(s.size()));
}

inline void ostream_print_via_buf_runtime(std::ostream& os,
                                          std::string_view fmt,
                                          std::format_args args) {
    (void)std::vformat_to(std::ostreambuf_iterator<char>{os.rdbuf()}, fmt, args);
}

} // namespace petra

// ============================================================================
// Test harness (same shape as the Aug 22-28 chain)
// ============================================================================

namespace {

int g_pass = 0;
int g_fail = 0;
const char* g_section = "";

void section(const char* name) {
    g_section = name;
    std::fprintf(stderr, "-- section %s\n", name);
}

} // namespace

#define SECTION(name) section(name)

#define CHECK(cond) do {                                                 \
    if (cond) {                                                          \
        ++g_pass;                                                        \
    } else {                                                             \
        ++g_fail;                                                        \
        std::fprintf(stderr, "FAIL: %s:%d  [%s]  CHECK(%s)\n",           \
                     __FILE__, __LINE__, g_section, #cond);              \
    }                                                                    \
} while (0)

#define CHECK_EQ(a, b) do {                                              \
    auto _a = (a); auto _b = (b);                                        \
    if (_a == _b) {                                                      \
        ++g_pass;                                                        \
    } else {                                                             \
        ++g_fail;                                                        \
        std::fprintf(stderr, "FAIL: %s:%d  [%s]  CHECK_EQ(%s, %s)\n",    \
                     __FILE__, __LINE__, g_section, #a, #b);             \
    }                                                                    \
} while (0)

// ------------------------------------------------------------------
// Section 1 — toolchain + feature probes
// ------------------------------------------------------------------

void section_1_probes() {
    SECTION("1.toolchain-probes");
    CHECK(__cpp_lib_format == 202110);     // C++23 std::format
    CHECK(__cpp_lib_expected == 202211);   // C++23 std::expected
    CHECK(__cpp_lib_print == 202207);      // C++23 std::print (FILE* only here)
    // std::ostreambuf_iterator<char> models std::output_iterator, which
    // is what makes it a legal std::vformat_to sink. (Parenthesised
    // because the concept's comma would split the macro argument.)
    constexpr bool models_output_iterator =
        std::output_iterator<std::ostreambuf_iterator<char>, char>;
    CHECK(models_output_iterator);
    // The iterator is a single streambuf pointer + a failed flag.
    CHECK(sizeof(std::ostreambuf_iterator<char>) <= 16);
}

// ------------------------------------------------------------------
// Section 2 — baseline: a healthy sink, both paths, accounting agrees
// ------------------------------------------------------------------

void section_2_healthy_sink_baseline() {
    SECTION("2.healthy-sink-baseline");

    {   // write path
        std::ostringstream oss;
        const auto r = petra::ostream_write_checked(oss, "hello {} world", 42);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{14});
        CHECK_EQ(oss.str(), std::string{"hello 42 world"});
        CHECK(oss.good());
    }
    {   // streambuf path — byte-identical output
        std::ostringstream oss;
        const auto r = petra::ostream_write_checked_via_buf(oss, "hello {} world", 42);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{14});
        CHECK_EQ(oss.str(), std::string{"hello 42 world"});
        CHECK(oss.good());
    }
    {   // the filter itself is transparent: offered == accepted on a
        // sink that takes everything
        std::ostringstream oss;
        petra::scoped_counting_filter guard{oss};
        oss.write("abcdef", 6);
        CHECK_EQ(guard.offered(), std::size_t{6});
        CHECK_EQ(guard.accepted(), std::size_t{6});
        CHECK(!guard.truncated());
    }
}

// ------------------------------------------------------------------
// Section 3 — capped sink, write path: badbit IS set
// ------------------------------------------------------------------
// The Aug 28 adapter's default path. std::ostream::write calls
// rdbuf()->sputn(); a short return makes write() call setstate(badbit)
// ([ostream.unformatted]/3). So this path at least SIGNALS the problem,
// even though it cannot say how many bytes were lost.

void section_3_capped_write_path_sets_badbit() {
    SECTION("3.capped-write-path-badbit");

    petra::capped_streambuf sink{5};
    std::ostream os{&sink};
    int n = 42;
    petra::ostream_print_runtime(os, "hello {}", std::make_format_args(n));

    CHECK_EQ(sink.str(), std::string{"hello"});     // 5 of 8 bytes
    CHECK_EQ(sink.consumed(), std::size_t{5});
    CHECK(os.bad());                                 // detectable
    CHECK(os.fail());                                // badbit implies fail()
    CHECK(!os.good());
    CHECK_EQ(sink.refusals(), std::size_t{1});       // one short xsputn
}

// ------------------------------------------------------------------
// Section 4 — capped sink, streambuf path: the SILENT truncation
// ------------------------------------------------------------------
// The bug Aug 28's "Where we go next" predicted, now demonstrated.
// std::ostreambuf_iterator writes through sb->sputc() directly and
// never consults or updates basic_ios::rdstate. Bytes vanish and the
// stream still reports good().

void section_4_capped_streambuf_path_is_silent() {
    SECTION("4.capped-streambuf-path-silent");

    petra::capped_streambuf sink{5};
    std::ostream os{&sink};
    int n = 42;
    petra::ostream_print_via_buf_runtime(os, "hello {}", std::make_format_args(n));

    CHECK_EQ(sink.str(), std::string{"hello"});      // 3 bytes lost
    CHECK_EQ(sink.consumed(), std::size_t{5});

    // *** the finding ***: the stream is CLEAN despite dropping bytes.
    CHECK(!os.bad());
    CHECK(!os.fail());
    CHECK(os.good());

    // The sink knows; the stream does not. Hence today's lesson.
    CHECK(sink.refusals() > 0);
}

// ------------------------------------------------------------------
// Section 5 — failed() lives on the RETURNED iterator, not yours
// ------------------------------------------------------------------
// std::vformat_to takes the output iterator by value and copies it on
// every advance. The failed() flag propagates out through the return
// value only. This is the same copy-semantics trap the Aug 25/26/28
// lessons hit with their custom bounded iterators, here in the
// standard library's own iterator.

void section_5_failed_flag_is_on_the_returned_iterator() {
    SECTION("5.failed-on-returned-iterator");

    petra::capped_streambuf sink{5};
    std::ostream os{&sink};

    using It = std::ostreambuf_iterator<char>;
    It mine{os.rdbuf()};
    int n = 42;
    const It returned = std::vformat_to(It{mine}, "hello {}", std::make_format_args(n));

    CHECK(returned.failed());     // the truncation signal
    CHECK(!mine.failed());        // *** our copy never learns ***
    CHECK_EQ(sink.consumed(), std::size_t{5});

    // On a healthy sink both report false, which is why this trap is
    // easy to miss in testing.
    std::ostringstream ok;
    It ok_mine{ok.rdbuf()};
    const It ok_returned = std::vformat_to(It{ok_mine}, "hello {}",
                                           std::make_format_args(n));
    CHECK(!ok_returned.failed());
    CHECK(!ok_mine.failed());
}

// ------------------------------------------------------------------
// Section 6 — the exact-fit boundary
// ------------------------------------------------------------------
// "hello 42" is 8 bytes. Sweeping the cap across the boundary pins down
// that failed() flips exactly at cap < len, not at cap <= len: an exact
// fit is a success, and the iterator does not over-offer a trailing
// byte on the healthy side.

void section_6_exact_fit_boundary() {
    SECTION("6.exact-fit-boundary");

    struct Row { std::size_t cap; bool expect_failed; std::size_t expect_consumed; };
    const std::array<Row, 5> rows{{
        {0, true,  0},
        {5, true,  5},
        {7, true,  7},
        {8, false, 8},   // exact fit — success
        {9, false, 8},
    }};

    for (const Row& row : rows) {
        petra::capped_streambuf sink{row.cap};
        std::ostream os{&sink};
        using It = std::ostreambuf_iterator<char>;
        int n = 42;
        const It done = std::vformat_to(It{os.rdbuf()}, "hello {}",
                                        std::make_format_args(n));
        CHECK_EQ(done.failed(), row.expect_failed);
        CHECK_EQ(sink.consumed(), row.expect_consumed);
    }
}

// ------------------------------------------------------------------
// Section 7 — checked adapter, success path
// ------------------------------------------------------------------

void section_7_checked_success() {
    SECTION("7.checked-success");

    {   // sink with room to spare
        petra::capped_streambuf sink{64};
        std::ostream os{&sink};
        const auto r = petra::ostream_write_checked(os, "id={} ok={}", 7, true);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{12});
        CHECK_EQ(sink.str(), std::string{"id=7 ok=true"});
    }
    {   // exact fit is still a success, on both paths
        petra::capped_streambuf sink{12};
        std::ostream os{&sink};
        const auto r = petra::ostream_write_checked(os, "id={} ok={}", 7, true);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{12});
    }
    {
        petra::capped_streambuf sink{12};
        std::ostream os{&sink};
        const auto r = petra::ostream_write_checked_via_buf(os, "id={} ok={}", 7, true);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{12});
        CHECK_EQ(sink.str(), std::string{"id=7 ok=true"});
    }
}

// ------------------------------------------------------------------
// Section 8 — checked adapter, PartialWrite path (both adapters)
// ------------------------------------------------------------------
// The point of the lesson: truncation is now a typed, counted error on
// BOTH paths — including the streambuf path that section 4 showed
// leaves the stream reporting good().

void section_8_checked_partial_write() {
    SECTION("8.checked-partial-write");

    {   // write path
        petra::capped_streambuf sink{5};
        std::ostream os{&sink};
        const auto r = petra::ostream_write_checked(os, "hello {}", 42);
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::WriteErrorKind::PartialWrite);
        CHECK_EQ(r.error().requested, std::size_t{8});
        CHECK_EQ(r.error().accepted, std::size_t{5});
        CHECK_EQ(r.error().dropped(), std::size_t{3});
        CHECK_EQ(sink.str(), std::string{"hello"});
    }
    {   // streambuf path — same typed error, same counts
        petra::capped_streambuf sink{5};
        std::ostream os{&sink};
        const auto r = petra::ostream_write_checked_via_buf(os, "hello {}", 42);
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::WriteErrorKind::PartialWrite);
        CHECK_EQ(r.error().accepted, std::size_t{5});
        CHECK_EQ(r.error().dropped(), std::size_t{1});
        CHECK_EQ(sink.str(), std::string{"hello"});
    }
    // NOTE the asymmetry in `requested` above, and it is not a bug:
    //   - the write path measures the FORMATTED TEXT (8), because it
    //     builds the std::string first, so dropped() == 3 (true loss);
    //   - the streambuf path measures BYTES OFFERED TO THE STREAM (6),
    //     because ostreambuf_iterator stops offering after the first
    //     refusal, so dropped() == 1 (bytes refused, not bytes lost).
    // Both are honest reports of what each path can actually observe.
    // A caller that needs true loss must use the write path or format
    // twice. Section 15's notes discuss the trade.
    {
        petra::capped_streambuf sink{5};
        std::ostream os{&sink};
        const auto r = petra::ostream_write_checked_via_buf(os, "hello {}", 42);
        CHECK(!r.has_value());
        CHECK_EQ(r.error().requested, std::size_t{6});   // offered, not formatted
        // The formatter is not asked to keep going after the sink says no.
        CHECK(r.error().requested < std::size_t{8});
    }
    {   // a zero-capacity sink: everything is dropped
        petra::capped_streambuf sink{0};
        std::ostream os{&sink};
        const auto r = petra::ostream_write_checked(os, "x{}", 1);
        CHECK(!r.has_value());
        CHECK_EQ(r.error().accepted, std::size_t{0});
        CHECK_EQ(r.error().requested, std::size_t{2});
        CHECK_EQ(sink.consumed(), std::size_t{0});
    }
    {   // the formatter for WriteError renders the split
        const petra::WriteError e{petra::WriteErrorKind::PartialWrite, "sink refused bytes", 8, 5};
        const std::string rendered = std::format("{}", e);
        CHECK(rendered.find("PartialWrite") != std::string::npos);
        CHECK(rendered.find("requested=8") != std::string::npos);
        CHECK(rendered.find("accepted=5") != std::string::npos);
        CHECK(rendered.find("dropped=3") != std::string::npos);
    }
}

// ------------------------------------------------------------------
// Section 9 — checked adapter, Format path
// ------------------------------------------------------------------

void section_9_checked_format_error() {
    SECTION("9.checked-format-error");

    {   // runtime format string with a type mismatch: {:d} on a string
        petra::capped_streambuf sink{1024};
        std::ostream os{&sink};
        std::string arg = "nope";
        int seven = 7;
        const auto r = petra::ostream_write_checked_runtime(
            os, "prefix-{} {:d}", std::make_format_args(seven, arg));
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::WriteErrorKind::Format);
        CHECK(!r.error().message.empty());
        // ATOMIC: nothing reached the sink, not even the literal prefix.
        CHECK_EQ(sink.consumed(), std::size_t{0});
    }
    {   // unmatched brace
        petra::capped_streambuf sink{1024};
        std::ostream os{&sink};
        const auto r = petra::ostream_write_checked_runtime(
            os, "oops {", std::make_format_args());
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::WriteErrorKind::Format);
        CHECK_EQ(sink.consumed(), std::size_t{0});
    }
    {   // argument index out of range
        petra::capped_streambuf sink{1024};
        std::ostream os{&sink};
        int one = 1;
        const auto r = petra::ostream_write_checked_runtime(
            os, "{} {}", std::make_format_args(one));
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::WriteErrorKind::Format);
        CHECK_EQ(sink.consumed(), std::size_t{0});
    }
    {   // same three failures on the streambuf path
        petra::capped_streambuf sink{1024};
        std::ostream os{&sink};
        std::string arg = "nope";
        int seven = 7;
        const auto r = petra::ostream_write_checked_via_buf_runtime(
            os, "prefix-{} {:d}", std::make_format_args(seven, arg));
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::WriteErrorKind::Format);
        CHECK_EQ(r.error().accepted, std::size_t{0});
        CHECK_EQ(sink.consumed(), std::size_t{0});
    }
}

// ------------------------------------------------------------------
// Section 10 — is the Format path atomic for LARGE payloads?
// ------------------------------------------------------------------
// A fair question: the write path is atomic by construction (format
// completes into a std::string before any byte moves). The streambuf
// path formats DIRECTLY into the sink, so a long literal prefix
// followed by a bad replacement field could plausibly leak. It does
// not, at any size tried — libc++ 21's vformat_to buffers internally
// and validates the whole spec before emitting. We pin the observation
// rather than the guarantee: this is an implementation property, NOT
// something [format.err] promises.

void section_10_format_error_atomicity_by_size() {
    SECTION("10.format-error-atomicity");

    for (const std::size_t plen : {std::size_t{8}, std::size_t{100},
                                   std::size_t{300}, std::size_t{1000},
                                   std::size_t{5000}, std::size_t{20000}}) {
        const std::string prefix(plen, 'P');
        std::string bad_arg = "nope";

        petra::capped_streambuf sink{1 << 20};
        std::ostream os{&sink};
        const auto r = petra::ostream_write_checked_via_buf_runtime(
            os, "{}{:d}", std::make_format_args(prefix, bad_arg));

        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::WriteErrorKind::Format);
        CHECK_EQ(sink.consumed(), std::size_t{0});   // no leak at any size
        CHECK_EQ(r.error().accepted, std::size_t{0});
    }
}

// ------------------------------------------------------------------
// Section 11 — the rdbuf(sb*) state-clobber trap
// ------------------------------------------------------------------
// basic_ios::rdbuf(streambuf*) calls clear() ([ios.members]/6). A naive
// scoped filter therefore (a) erases whatever error state the stream
// already carried when it installs, and (b) erases the badbit the short
// write just produced when it restores. Both are silent. The guard in
// Part 2 saves and re-applies rdstate to close the hole.

void section_11_rdbuf_clobbers_state() {
    SECTION("11.rdbuf-clobbers-rdstate");

    {   // the raw behaviour, unguarded
        petra::capped_streambuf sink{3};
        std::ostringstream oss;
        std::ios& as_ios = oss;   // see the note below on name hiding
        oss.setstate(std::ios::badbit);
        CHECK(oss.bad());

        std::streambuf* previous = as_ios.rdbuf(&sink);
        CHECK(!oss.bad());        // *** install cleared it ***

        oss.write("abcdefgh", 8);
        CHECK(oss.bad());         // the short write set it again

        (void)as_ios.rdbuf(previous);
        CHECK(!oss.bad());        // *** restore cleared it again ***
        CHECK_EQ(sink.str(), std::string{"abc"});
    }
    {   // the guard preserves pre-existing state across the splice
        std::ostringstream oss;
        oss.setstate(std::ios::badbit);
        {
            petra::scoped_counting_filter guard{oss};
            CHECK_EQ(guard.offered(), std::size_t{0});
        }
        CHECK(oss.bad());         // survived the round trip
    }
    {   // and it preserves state PRODUCED while installed
        petra::capped_streambuf sink{3};
        std::ostream os{&sink};
        CHECK(os.good());
        {
            petra::scoped_counting_filter guard{os};
            os.write("abcdefgh", 8);
            CHECK_EQ(guard.offered(), std::size_t{8});
            CHECK_EQ(guard.accepted(), std::size_t{3});
            CHECK(guard.truncated());
        }
        CHECK(os.bad());          // badbit survived the restore
        CHECK_EQ(sink.str(), std::string{"abc"});
    }
    // Name hiding, for the record: std::ostringstream::rdbuf() and
    // std::ofstream::rdbuf() are zero-argument members that HIDE the
    // inherited one-argument basic_ios::rdbuf(streambuf*). Writing
    //     oss.rdbuf(&filter);
    // is a compile error on libc++ 21:
    //     error: too many arguments to function call, expected 0, have 1
    //     note: 'rdbuf' declared here
    //         basic_stringbuf<...>* rdbuf() const
    // The fix is to go through a base reference, as `as_ios` does above.
    // scoped_counting_filter's constructor takes std::ostream&, so the
    // conversion happens at the call site and users never hit this.
    CHECK(true);
}

// ------------------------------------------------------------------
// Section 12 — exceptions(badbit) turns truncation into a throw
// ------------------------------------------------------------------
// If the caller has armed the stream's exception mask, the write path's
// setstate(badbit) becomes an std::ios_base::failure. The checked
// adapter does NOT swallow it — the mask is the caller's policy. This
// section documents which path honours the mask (write) and which
// bypasses it entirely (streambuf iterator), which is the same
// asymmetry section 4 found, seen from the other side.

void section_12_exception_mask() {
    SECTION("12.exception-mask");

    {   // write path honours the mask
        petra::capped_streambuf sink{2};
        std::ostream os{&sink};
        os.exceptions(std::ios::badbit);
        bool threw = false;
        try {
            os.write("abcdef", 6);
        } catch (const std::ios_base::failure&) {
            threw = true;
        }
        CHECK(threw);
        CHECK_EQ(sink.str(), std::string{"ab"});
    }
    {   // streambuf-iterator path bypasses the mask: no throw, no state
        petra::capped_streambuf sink{2};
        std::ostream os{&sink};
        os.exceptions(std::ios::badbit);
        bool threw = false;
        try {
            petra::ostream_print_via_buf_runtime(
                os, "abcdef", std::make_format_args());
        } catch (const std::ios_base::failure&) {
            threw = true;
        }
        CHECK(!threw);            // *** silent again ***
        CHECK(!os.bad());
        CHECK_EQ(sink.str(), std::string{"ab"});
    }
    {   // the checked adapter reports it either way, mask or no mask
        petra::capped_streambuf sink{2};
        std::ostream os{&sink};
        const auto r = petra::ostream_write_checked_via_buf(os, "abc{}", "def");
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::WriteErrorKind::PartialWrite);
        CHECK_EQ(r.error().accepted, std::size_t{2});
    }
}

// ------------------------------------------------------------------
// Section 13 — the iterator bypasses stream state in BOTH directions
// ------------------------------------------------------------------
// Section 4 showed the iterator not SETTING state. It also does not
// READ it: a stream already in badbit still gets bytes written through
// its streambuf. std::ostream::write, by contrast, short-circuits in
// the sentry and writes nothing.

void section_13_iterator_ignores_existing_state() {
    SECTION("13.iterator-ignores-state");

    {   // write path: sentry refuses, sink untouched
        petra::capped_streambuf sink{100};
        std::ostream os{&sink};
        os.setstate(std::ios::badbit);
        os.write("xyz", 3);
        CHECK_EQ(sink.consumed(), std::size_t{0});
    }
    {   // streambuf path: writes anyway
        petra::capped_streambuf sink{100};
        std::ostream os{&sink};
        os.setstate(std::ios::badbit);
        petra::ostream_print_via_buf_runtime(os, "xyz", std::make_format_args());
        CHECK_EQ(sink.consumed(), std::size_t{3});   // *** bytes on a bad stream ***
        CHECK_EQ(sink.str(), std::string{"xyz"});
    }
}

// ------------------------------------------------------------------
// Section 14 — UTF-8 truncation splits mid-codepoint
// ------------------------------------------------------------------
// Byte-oriented truncation is not codepoint-aware. A cap landing inside
// a multi-byte sequence produces invalid UTF-8 in the sink. The checked
// adapter reports the byte split; making the split codepoint-safe is a
// SINK responsibility (or a follow-on lesson), not the adapter's.

void section_14_utf8_mid_codepoint() {
    SECTION("14.utf8-mid-codepoint");

    const std::string payload = "ok \xF0\x9F\x9A\x80!";   // "ok " + U+1F680 + "!"
    CHECK_EQ(payload.size(), std::size_t{8});

    {   // whole payload fits: bytes round-trip exactly
        petra::capped_streambuf sink{64};
        std::ostream os{&sink};
        const auto r = petra::ostream_write_checked(os, "{}", payload);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{8});
        CHECK_EQ(sink.str(), payload);
    }
    {   // cap 7 splits after the emoji's last byte, dropping '!'
        petra::capped_streambuf sink{7};
        std::ostream os{&sink};
        const auto r = petra::ostream_write_checked(os, "{}", payload);
        CHECK(!r.has_value());
        CHECK_EQ(r.error().accepted, std::size_t{7});
        CHECK_EQ(sink.str(), std::string("ok \xF0\x9F\x9A\x80"));
    }
    {   // cap 5 splits INSIDE the 4-byte sequence: invalid UTF-8 in sink
        petra::capped_streambuf sink{5};
        std::ostream os{&sink};
        const auto r = petra::ostream_write_checked(os, "{}", payload);
        CHECK(!r.has_value());
        CHECK_EQ(r.error().accepted, std::size_t{5});
        CHECK_EQ(sink.str(), std::string("ok \xF0\x9F"));
        // A lead byte 0xF0 announces 4 bytes; only 2 arrived.
        CHECK_EQ(static_cast<unsigned char>(sink.str()[3]), 0xF0u);
        CHECK_EQ(sink.consumed(), std::size_t{5});
    }
    {   // embedded NUL survives the byte pipeline unharmed
        const std::string with_nul = std::string("a\0b", 3);
        CHECK_EQ(with_nul.size(), std::size_t{3});
        petra::capped_streambuf sink{64};
        std::ostream os{&sink};
        const auto r = petra::ostream_write_checked(os, "{}", with_nul);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{3});
        CHECK_EQ(sink.str(), with_nul);
        CHECK_EQ(sink.str()[1], '\0');
    }
}

// ------------------------------------------------------------------
// Section 15 — a real std::ofstream sink
// ------------------------------------------------------------------
// The filter is sink-agnostic. Splicing it over a filebuf works
// unchanged, and the file on disk matches the accepted count.

void section_15_ofstream_sink() {
    SECTION("15.ofstream-sink");

    const char* path = "/tmp/petra-2026-08-30-ostream-partial-write.txt";
    {
        std::ofstream out{path, std::ios::binary | std::ios::trunc};
        CHECK(out.is_open());
        const auto r = petra::ostream_write_checked(out, "line {} of {}\n", 1, 3);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{12});   // "line 1 of 3\n"
        const auto r2 = petra::ostream_write_checked_via_buf(out, "line {} of {}\n", 2, 3);
        CHECK(r2.has_value());
        CHECK_EQ(r2.value(), std::size_t{12});
        CHECK(out.good());
    }
    {
        std::ifstream in{path, std::ios::binary};
        CHECK(in.is_open());
        const std::string content{std::istreambuf_iterator<char>{in},
                                  std::istreambuf_iterator<char>{}};
        CHECK_EQ(content, std::string{"line 1 of 3\nline 2 of 3\n"});
        CHECK_EQ(content.size(), std::size_t{24});
    }
}

// ------------------------------------------------------------------
// Section 16 — determinism stress, both paths, 50 rounds
// ------------------------------------------------------------------
// Alternates the write path and the streambuf path across a sweep of
// sink capacities, asserting the checked adapter's verdict matches an
// independently computed expectation every time. The expected text is
// stored as an OWNED std::string, not a std::string_view into a
// per-iteration buffer — the stack-use-after-scope shape the Aug 25 and
// Aug 26 lessons both had to fix.

void section_16_determinism_stress() {
    SECTION("16.determinism-stress");

    for (int round = 0; round < 50; ++round) {
        const std::size_t cap = static_cast<std::size_t>(round) % 25u;
        const std::string expected_text = std::format("r{} payload", round);
        const bool use_write_path = (round % 2 == 0);

        petra::capped_streambuf sink{cap};
        std::ostream os{&sink};

        const auto r = use_write_path
            ? petra::ostream_write_checked(os, "r{} payload", round)
            : petra::ostream_write_checked_via_buf(os, "r{} payload", round);

        const bool should_fit = (cap >= expected_text.size());
        CHECK_EQ(r.has_value(), should_fit);

        if (should_fit) {
            CHECK_EQ(r.value(), expected_text.size());
            CHECK_EQ(sink.str(), expected_text);
        } else {
            CHECK_EQ(r.error().kind, petra::WriteErrorKind::PartialWrite);
            CHECK_EQ(r.error().accepted, cap);
            CHECK_EQ(sink.consumed(), cap);
            // whatever landed is a prefix of the full text
            CHECK_EQ(sink.str(), expected_text.substr(0, cap));
        }
    }
}

int main() {
    section_1_probes();
    section_2_healthy_sink_baseline();
    section_3_capped_write_path_sets_badbit();
    section_4_capped_streambuf_path_is_silent();
    section_5_failed_flag_is_on_the_returned_iterator();
    section_6_exact_fit_boundary();
    section_7_checked_success();
    section_8_checked_partial_write();
    section_9_checked_format_error();
    section_10_format_error_atomicity_by_size();
    section_11_rdbuf_clobbers_state();
    section_12_exception_mask();
    section_13_iterator_ignores_existing_state();
    section_14_utf8_mid_codepoint();
    section_15_ofstream_sink();
    section_16_determinism_stress();

    std::fprintf(stderr, "\n%d/%d PASS\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
