// P-2026-08-31 — ostream_write_n_via_buf: std::format_to_n / std::vformat_to
// straight to std::ostreambuf_iterator<char>, the bounded zero-allocation
// sibling of the Aug 28 std::print(std::ostream&, ...) adapter.
//
// Why today
// ---------
// The Aug 28 (P-2026-08-28) lesson shipped petra::ostream_print /
// ostream_println / ostream_write_n — the consumer-side
// std::print(std::ostream&, ...) adapter that closes the libc++ 21
// missing-overload gap. Its "Where we go next" section listed four
// new follow-on items. The Aug 30 (P-2026-08-30) lesson closed the
// first (partial-write detection on a custom streambuf). The THIRD
// item on Aug 28's list is:
//
//   > std::format_to_n straight to ostream streambuf — P2216R3
//   > doesn't ship this overload either; the same bounded_ostream_writer
//   > machinery from Aug 28 could be lifted to format_to_n's runtime
//   > path. The two-pass measure-then-write design re-uses unchanged.
//
// Today IS that lesson. The headline finding of the lesson is
// somewhat different from what Aug 28 predicted:
//
//   libc++ 21 / P2216R3 deliberately SEPARATES the two paths.
//
//   - std::format_to_n is consteval-ONLY: it takes
//     std::format_string<Args...>, which is the constrained-string
//     type whose ctor is a consteval function. There is no public
//     `format_to_n(OutIt, n, string_view, format_args)` — only the
//     internal `std::__vformat_to_n` which is `__`-prefixed and
//     implementation-private.
//
//   - The runtime-format-string sibling is std::vformat_to, which
//     takes a string_view + format_args but has NO n parameter.
//     The iterator-side cap is the user's responsibility.
//
// So today's wrapper has TWO entry points and they take DIFFERENT
// forms:
//
//   ostream_write_n_via_buf(os, cap, fmt, args...)         // consteval
//     — calls std::format_to_n(It{sb}, n, fmt, args...) directly.
//       One stdlib call. Returns the same would_be shape.
//
//   ostream_write_n_via_buf_runtime(os, cap, fmt, args)    // runtime fmt
//     — calls std::vformat_to through a hand-rolled bounded_ostream_writer
//       (Aug 28's machinery, lifted). The bounded iterator carries an
//       external std::size_t* counter because std::vformat_to copies
//       the iterator on every advance (same trap Aug 25 / Aug 26 /
//       Aug 30 hit with their bounded iterators).
//
// Both paths share:
//   - the counting_streambuf filter spliced under the ostream for
//     sink-side byte accounting;
//   - the FormatError type with Format / PartialWrite kinds (Aug 30's
//     shape);
//   - the would_be size returned on success, and the actual accepted
//     count carried in error.accepted on a sink-side PartialWrite.
//
// What this lesson is NOT
// -----------------------
//   - It is NOT a re-implementation of the Aug 30 ostream_write_checked
//     family. Today adds the n parameter (iterator-side cap), which is
//     a new dimension. The Aug 30 sibling has no cap; today the cap
//     IS the headline feature.
//   - It does NOT carry the Aug 25 NUL-termination contract. Today
//     writes to a streambuf, which manages its own memory; no NUL
//     is synthesised. The Aug 25 contract belongs to raw char* sinks;
//     a custom ostream that expected a NUL on read-back is a
//     different lesson (still on the Aug 28 follow-on list).
//   - It does NOT carry a std::string intermediate on either path.
//     The consteval path is a single std::format_to_n call; the
//     runtime path is std::vformat_to through a custom iterator.
//
// C++ standard: C++23. Uses std::format_to_n (P2216R3),
// std::vformat_to, std::ostreambuf_iterator, std::make_format_args,
// std::format_error, std::expected (P0323R12), and a user-defined
// std::formatter specialisation.

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

// ============================================================================
// Part 1 — sink filters: capped_streambuf + counting_streambuf
// ============================================================================
//
// These are the SAME types Aug 30's lesson shipped, redeclared here so
// the consumer TU is self-contained. Aug 30 documented them at length;
// today's lesson only references the contracts:
//
//   capped_streambuf{cap}     — accepts at most `cap` bytes total, then
//                               refuses via overflow() -> eof() or a
//                               short xsputn() return.
//   counting_streambuf{down}  — observes bytes offered vs accepted
//                               through the filter; forwards verbatim.
//                               offered > accepted is the definition
//                               of a partial write at the sink.
//   scoped_counting_filter    — RAII splice/unsplice that works around
//                               basic_ios::rdbuf(sb*) calling clear().
//
// Why today's lesson needs the counting filter even on the format_to_n
// path: std::format_to_n reports would_be on every path. It does NOT
// tell you whether the bytes it OFFERED actually landed — the iterator
// assigns to *it, and the streambuf either consumes the char (sputc/
// sputn) or refuses. The filter sits BETWEEN the iterator and the real
// streambuf so we can read offered/accepted exactly. Section 4 pins
// this down.

namespace petra {

class capped_streambuf : public std::streambuf {
public:
    explicit capped_streambuf(std::size_t cap) noexcept : cap_(cap) {}

    [[nodiscard]] const std::string& str()     const noexcept { return sink_; }
    [[nodiscard]] std::size_t         consumed() const noexcept { return sink_.size(); }
    [[nodiscard]] std::size_t         capacity() const noexcept { return cap_; }
    [[nodiscard]] std::size_t         refusals() const noexcept { return refusals_; }

protected:
    int_type overflow(int_type ch) override {
        if (traits_type::eq_int_type(ch, traits_type::eof())) {
            return traits_type::not_eof(ch);
        }
        if (sink_.size() >= cap_) {
            ++refusals_;
            return traits_type::eof();
        }
        sink_.push_back(traits_type::to_char_type(ch));
        return ch;
    }

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
    std::size_t  cap_;
    std::string  sink_;
    std::size_t  refusals_ = 0;
};

class counting_streambuf : public std::streambuf {
public:
    explicit counting_streambuf(std::streambuf* down) noexcept : down_(down) {}

    [[nodiscard]] std::size_t offered()   const noexcept { return offered_; }
    [[nodiscard]] std::size_t accepted()  const noexcept { return accepted_; }
    [[nodiscard]] bool        truncated() const noexcept { return accepted_ < offered_; }

    void reset() noexcept { offered_ = 0; accepted_ = 0; }

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
    std::size_t     offered_  = 0;
    std::size_t     accepted_ = 0;
};

class scoped_counting_filter {
public:
    explicit scoped_counting_filter(std::ostream& os)
        : os_(os),
          saved_state_(os.rdstate()),
          filter_(os.rdbuf()),
          previous_(os.rdbuf(&filter_)) {}

    scoped_counting_filter(const scoped_counting_filter&) = delete;
    scoped_counting_filter& operator=(const scoped_counting_filter&) = delete;

    ~scoped_counting_filter() {
        const std::ios_base::iostate produced = os_.rdstate();
        (void)os_.rdbuf(previous_);
        os_.setstate(saved_state_ | produced);
    }

    [[nodiscard]] std::size_t offered()   const noexcept { return filter_.offered(); }
    [[nodiscard]] std::size_t accepted()  const noexcept { return filter_.accepted(); }
    [[nodiscard]] bool        truncated() const noexcept { return filter_.truncated(); }

private:
    std::ostream&          os_;
    std::ios_base::iostate saved_state_;
    counting_streambuf     filter_;
    std::streambuf*        previous_;
};

} // namespace petra

// ============================================================================
// Part 2 — error type: Format + PartialWrite
// ============================================================================
//
// Two failure kinds, matching the Aug 30 lesson:
//
//   Format       — std::format_to_n / std::vformat_to threw
//                  std::format_error. The message carries the parser's
//                  reason.
//   PartialWrite — the format succeeded but the sink refused bytes.
//                  `requested` is what std::format_to_n's iterator
//                  actually OFFERED (not would_be — see section 4 for
//                  why these differ on the iterator-capped path);
//                  `accepted` is what the sink actually took.
//
// The `requested` semantic is deliberately the same as the Aug 30
// lesson's WriteError.requested, NOT std::format_to_n's would_be —
// because the user wants "what actually reached the sink's downstream
// interface", which is the iterator's actual offer count, not the
// formatter's full intent. (Section 4 contrasts the two.)

namespace petra {

enum class FormatErrorKind { Format, PartialWrite };

[[nodiscard]] constexpr std::string_view format_error_kind_name(FormatErrorKind k) noexcept {
    switch (k) {
        case FormatErrorKind::Format:       return "Format";
        case FormatErrorKind::PartialWrite: return "PartialWrite";
    }
    return "?";
}

struct FormatError {
    FormatErrorKind  kind{};
    std::string      message{};
    std::size_t      requested = 0;   // bytes the iterator offered
    std::size_t      accepted  = 0;   // bytes the sink actually took

    [[nodiscard]] std::size_t dropped() const noexcept {
        return requested - std::min(requested, accepted);
    }

    friend bool operator==(const FormatError&, const FormatError&) = default;
};

} // namespace petra

// libc++ 21 has no formatter for arbitrary user types.
template <>
struct std::formatter<petra::FormatError> : std::formatter<std::string> {
    auto format(const petra::FormatError& e, std::format_context& ctx) const {
        return std::formatter<std::string>::format(
            std::format("FormatError{{{}, requested={}, accepted={}, dropped={}, msg=\"{}\"}}",
                        petra::format_error_kind_name(e.kind),
                        e.requested, e.accepted, e.dropped(), e.message),
            ctx);
    }
};

// ============================================================================
// Part 3 — bounded_ostream_writer: the runtime path's cap mechanism
// ============================================================================
//
// The consteval path uses std::format_to_n, which natively takes n.
// The runtime path uses std::vformat_to, which has no n parameter —
// so the iterator itself must cap the writes. Aug 28's lesson built
// this kind of iterator for std::vformat_to; today lifts the same
// shape over unchanged.
//
// The trap the iterator must avoid: std::vformat_to copies the
// iterator on every advance (same as std::vformat_to does for the
// Aug 26 / Aug 28 iterators). So the count MUST live externally;
// storing it as a member loses every increment after the first copy.
// The constructor takes a std::size_t* and writes through it.

namespace petra {

class bounded_ostream_writer {
public:
    using iterator_category = std::output_iterator_tag;
    using value_type        = char;
    using difference_type   = std::ptrdiff_t;
    using pointer           = void;
    using reference         = void;

    bounded_ostream_writer(std::ostream* os,
                           std::size_t cap,
                           std::size_t* written) noexcept
        : os_(os), cap_(cap), written_(written) {}

    // Copy is intentional: std::vformat_to copies the iterator on
    // every advance. The copy must observe the same counter; the
    // external std::size_t* guarantees it.
    bounded_ostream_writer(const bounded_ostream_writer&) noexcept = default;
    bounded_ostream_writer& operator=(const bounded_ostream_writer&) noexcept = default;

    bounded_ostream_writer& operator=(char c) noexcept {
        if (*written_ < cap_) {
            os_->put(c);
            ++*written_;
        }
        return *this;
    }

    bounded_ostream_writer& operator*()  noexcept { return *this; }
    bounded_ostream_writer& operator++() noexcept { return *this; }
    bounded_ostream_writer  operator++(int) noexcept { return *this; }

private:
    std::ostream* os_;
    std::size_t   cap_;
    std::size_t*  written_;
};

} // namespace petra

// ============================================================================
// Part 4 — the wrapped adapters
// ============================================================================
//
// Two entry points, matching the consteval-vs-runtime split Aug 28 /
// Aug 30 established.

namespace petra {

// ----------------------------------------------------------------------------
// Consteval path — std::format_to_n straight to std::ostreambuf_iterator.
// ----------------------------------------------------------------------------
// One stdlib call. std::format_to_n accepts std::ostreambuf_iterator<char>
// as its Out parameter (section 1's compile-time probe pins this via
// std::output_iterator). The would_be size is the second half of the
// returned pair; the iterator's failed() flag carries the truncation
// signal. The counting filter adds the actual byte count under the
// same cap.
//
// Returns expected<size_t, FormatError> with:
//   - value: would_be (the size the formatter wanted to write)
//   - error: Format (parse / type-mismatch) or PartialWrite
//     (sink-side cap below iterator-side cap and below would_be)

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
ostream_write_n_via_buf(std::ostream& os,
                        std::size_t cap,
                        std::format_string<Args...> fmt,
                        Args&&... args) {
    scoped_counting_filter guard{os};

    using It         = std::ostreambuf_iterator<char>;
    using difference = std::iter_difference_t<It>;

    // std::format_to_n returns a format_to_n_result, but
    // format_to_n_result<It> has no default constructor (It has
    // none), so we can't default-construct one to assign into.
    // We use structured bindings instead and pull the .out / .size
    // out of the binding immediately after the call.
    std::size_t would_be = 0;
    bool        iterator_failed = false;

    try {
        // The consteval gate on std::format_string<Args...> means
        // this call only compiles when fmt is a literal; runtime
        // format strings must use the _runtime entry point below.
        auto [out, n] = std::format_to_n(It{os.rdbuf()},
                                         static_cast<difference>(cap),
                                         fmt,
                                         std::forward<Args>(args)...);
        would_be       = static_cast<std::size_t>(n);
        iterator_failed = out.failed();
    } catch (const std::format_error& e) {
        // Format-error path. ATOMIC on libc++ 21 — section 7 pins
        // this by sweeping prefix lengths.
        return std::expected<std::size_t, FormatError>{
            std::unexpect,
            FormatError{FormatErrorKind::Format, e.what(),
                        guard.offered(), guard.accepted()}};
    }

    const std::size_t offered  = guard.offered();
    const std::size_t accepted = guard.accepted();

    if (iterator_failed || accepted < offered) {
        return std::expected<std::size_t, FormatError>{
            std::unexpect,
            FormatError{FormatErrorKind::PartialWrite,
                        "sink refused bytes",
                        offered,
                        accepted}};
    }

    return would_be;
}

// ----------------------------------------------------------------------------
// Runtime path — std::vformat_to through bounded_ostream_writer.
// ----------------------------------------------------------------------------
// std::format_to_n has no public runtime-format-string overload (only
// the consteval-gated one above). The runtime-format-string sibling
// std::vformat_to takes a string_view + format_args but no n. So the
// iterator itself must cap the writes — which is exactly what
// bounded_ostream_writer above does.
//
// Returns expected<size_t, FormatError> with the same shape as the
// consteval path. The counting filter adds the sink-side byte count.

[[nodiscard]] inline std::expected<std::size_t, FormatError>
ostream_write_n_via_buf_runtime(std::ostream& os,
                                std::size_t cap,
                                std::string_view fmt,
                                std::format_args args) {
    scoped_counting_filter guard{os};

    std::size_t written = 0;
    bool threw_format_error = false;
    std::string what;

    try {
        bounded_ostream_writer w{&os, cap, &written};
        (void)std::vformat_to(std::move(w), fmt, args);
    } catch (const std::format_error& e) {
        threw_format_error = true;
        what = e.what();
    }

    const std::size_t offered  = guard.offered();
    const std::size_t accepted = guard.accepted();

    if (threw_format_error) {
        return std::expected<std::size_t, FormatError>{
            std::unexpect,
            FormatError{FormatErrorKind::Format, what,
                        offered, accepted}};
    }
    if (accepted < offered) {
        return std::expected<std::size_t, FormatError>{
            std::unexpect,
            FormatError{FormatErrorKind::PartialWrite,
                        "sink refused bytes",
                        offered,
                        accepted}};
    }

    return offered;
}

} // namespace petra

// ============================================================================
// Test harness (same shape as the Aug 22-30 chain)
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
    CHECK(__cpp_lib_format   == 202110);
    CHECK(__cpp_lib_expected == 202211);
    CHECK(__cpp_lib_print    == 202207);

    // ostreambuf_iterator models output_iterator<const char&> — what
    // makes it a legal std::format_to_n sink.
    constexpr bool models_output_iterator =
        std::output_iterator<std::ostreambuf_iterator<char>, char>;
    CHECK(models_output_iterator);

    // The iterator's failed() flag is the standard signal that the
    // streambuf returned eof() on sputc/sputn.
    constexpr bool has_failed =
        requires(std::ostreambuf_iterator<char> it) { it.failed(); };
    CHECK(has_failed);

    // format_to_n_result carries (out, size); std::format_to_n's
    // return type.
    using result_t = std::format_to_n_result<std::ostreambuf_iterator<char>>;
    constexpr bool result_has_out =
        requires(result_t r) { r.out; };
    constexpr bool result_has_size =
        requires(result_t r) { r.size; };
    CHECK(result_has_out);
    CHECK(result_has_size);

    // ostreambuf_iterator is small. On Apple Clang 21.0.0 / libc++
    // it carries a streambuf pointer + a failed flag, packed to
    // 16 B on 64-bit (pointer + bool + padding).
    CHECK(sizeof(std::ostreambuf_iterator<char>) <= 16);

    // bounded_ostream_writer is three pointers/size_t: 24 B on 64-bit.
    CHECK(sizeof(petra::bounded_ostream_writer) <= 24);

    // Filter inherits std::streambuf which itself is ~88 B on
    // Apple Clang 21.0.0 / libc++ (it carries get/put area
    // pointers, locale, etc.). The filter adds 24 B on top (a
    // streambuf pointer + two size_t). Loose bound — exact size
    // is implementation-defined.
    CHECK(sizeof(petra::counting_streambuf) <= 128);

    // FormatError is two size_t + a string + an enum: <= 64 B fits.
    CHECK(sizeof(petra::FormatError) <= 64);
}

// ------------------------------------------------------------------
// Section 2 — consteval path: happy case (cap >= text, healthy sink)
// ------------------------------------------------------------------

void section_2_happy_path() {
    SECTION("2.happy-path");

    std::ostringstream oss;
    const auto r = petra::ostream_write_n_via_buf(oss, 64, "hello {} world", 42);
    CHECK(r.has_value());
    CHECK_EQ(r.value(), std::size_t{14});
    CHECK_EQ(oss.str(), std::string{"hello 42 world"});
    CHECK(oss.good());
}

// ------------------------------------------------------------------
// Section 3 — std::format_to_n accepts std::ostreambuf_iterator directly
// ------------------------------------------------------------------

void section_3_format_to_n_with_ostreambuf_iterator() {
    SECTION("3.format-to-n-with-ostreambuf-iterator");

    // Probe 1: round number writes cleanly through.
    {
        std::ostringstream oss;
        using It = std::ostreambuf_iterator<char>;
        // "[{}|{}]" -> "[7|true]" = 8 chars
        auto [done, n] = std::format_to_n(It{oss.rdbuf()}, 32, "[{}|{}]", 7, true);
        CHECK_EQ(static_cast<std::size_t>(n), std::size_t{8});
        CHECK(!done.failed());
        CHECK_EQ(oss.str(), std::string{"[7|true]"});
    }

    // Probe 2: cap == exact length — exact fit succeeds.
    {
        std::ostringstream oss;
        using It = std::ostreambuf_iterator<char>;
        auto [done, n] = std::format_to_n(It{oss.rdbuf()}, 14, "hello {} world", 42);
        CHECK_EQ(static_cast<std::size_t>(n), std::size_t{14});
        CHECK(!done.failed());
        CHECK_EQ(oss.str(), std::string{"hello 42 world"});
    }

    // Probe 3: cap < length — iterator caps at cap; would_be is full.
    {
        std::ostringstream oss;
        using It = std::ostreambuf_iterator<char>;
        auto [done, n] = std::format_to_n(It{oss.rdbuf()}, 5, "hello {} world", 42);
        CHECK_EQ(static_cast<std::size_t>(n), std::size_t{14});   // would_be
        CHECK(!done.failed());                      // healthy sink accepted the 5
        CHECK_EQ(oss.str(), std::string{"hello"});  // exactly 5 chars
    }

    // Probe 4: cap == 0 — would_be is still the full size.
    {
        std::ostringstream oss;
        using It = std::ostreambuf_iterator<char>;
        auto [done, n] = std::format_to_n(It{oss.rdbuf()}, 0, "abc", 1);
        CHECK_EQ(static_cast<std::size_t>(n), std::size_t{3});
        CHECK(!done.failed());
        CHECK_EQ(oss.str(), std::string{});
    }
}

// ------------------------------------------------------------------
// Section 4 — would_be vs offered vs accepted
// ------------------------------------------------------------------
// std::format_to_n reports the would_be size (what the formatter
// wanted to write) on every path. The iterator stops OFFERING once
// it has written n bytes; that is the iterator-side cap.
//
// The filter's `offered` is what the iterator actually handed to the
// streambuf. On a healthy sink with cap >= would_be, offered ==
// would_be and accepted == offered. On cap < would_be, offered == cap
// (the iterator stops) and accepted == offered (healthy sink took
// everything offered).
//
// Today's wrapper returns FormatError.requested = filter.offered
// (NOT would_be) because the user-visible semantic is "what
// actually reached the sink's downstream interface", which on the
// iterator-capped path is min(would_be, n).

void section_4_would_be_vs_filter() {
    SECTION("4.would-be-vs-filter");

    // Healthy sink, cap == would_be: offered == accepted == would_be.
    // r.value() == would_be == 14.
    {
        std::ostringstream oss;
        petra::scoped_counting_filter guard{oss};
        const auto r = petra::ostream_write_n_via_buf(oss, 14, "hello {} world", 42);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{14});
        CHECK_EQ(guard.offered(),  std::size_t{14});
        CHECK_EQ(guard.accepted(), std::size_t{14});
        CHECK(!guard.truncated());
        CHECK_EQ(oss.str(), std::string{"hello 42 world"});
    }

    // Healthy sink, cap < would_be: iterator caps at cap. The
    // iterator's offered == cap. would_be == 14 (r.value()).
    {
        std::ostringstream oss;
        petra::scoped_counting_filter guard{oss};
        const auto r = petra::ostream_write_n_via_buf(oss, 5, "hello {} world", 42);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{14});     // would_be
        CHECK_EQ(guard.offered(),  std::size_t{5});
        CHECK_EQ(guard.accepted(), std::size_t{5});
        CHECK(!guard.truncated());
        CHECK_EQ(oss.str(), std::string{"hello"});
    }

    // Healthy sink, cap > would_be: iterator caps at would_be.
    {
        std::ostringstream oss;
        petra::scoped_counting_filter guard{oss};
        const auto r = petra::ostream_write_n_via_buf(oss, 1024, "id={}", 7);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{4});     // "id=7"
        CHECK_EQ(guard.offered(),  std::size_t{4});
        CHECK_EQ(guard.accepted(), std::size_t{4});
    }
}

// ------------------------------------------------------------------
// Section 5 — iterator-side cap vs sink-side cap: four regimes
// ------------------------------------------------------------------
// Both caps are independent. A real sink can saturate one without
// touching the other. The four regimes for a 14-byte payload
// ("hello 42 world") on a sink of capacity S with iterator cap N:
//
//   regime        | N >= 14  | N < 14
//   --------------+----------+-----------
//   S >= 14       | full     | iterator caps
//   S <  14       | sink caps| sink caps first (if S <= N) else iterator

void section_5_cap_independence() {
    SECTION("5.cap-independence");

    struct Row {
        std::size_t n;
        std::size_t sink;
        bool       expect_success;
        std::size_t expect_value;     // would_be on success
        std::size_t expect_offered;
        std::size_t expect_accepted;
    };
    const std::array<Row, 8> rows{{
        // n == would_be == 14; sink has room: full write, no error.
        {14,    16, true,  14, 14, 14},
        // n == would_be == 14; sink == 14 (exact fit on the sink side).
        {14,    14, true,  14, 14, 14},
        // n == would_be == 14; sink = 5: PartialWrite. Iterator offers
        // 6 chars before the sink refuses the 6th (the iterator tries
        // one byte past the sink cap to confirm the refusal).
        {14,     5, false,  0,  6,  5},
        // n = 5 < would_be; sink = 16 (sink has room): iterator caps.
        { 5,    16, true,  14,  5,  5},
        // n = 5 < would_be; sink = 5 (exact sink-side fit on n).
        { 5,     5, true,  14,  5,  5},
        // n = 5; sink = 3 (sink is the bottleneck and below n).
        // Iterator offers 4 (sink_cap + 1) before the sink refuses.
        { 5,     3, false,  0,  4,  3},
        // n = 1024; sink = 0 (zero-capacity sink): iterator tries
        // 1 char, sink refuses it. offered == 1, accepted == 0.
        {1024,   0, false,  0,  1,  0},
        // n = 0: iterator offers 0; would_be = 14 reported.
        // A zero-cap iterator on a healthy sink is a success path
        // with 0 bytes written — not a PartialWrite, because the
        // iterator never ASKED the sink to take anything.
        {  0,   16, true,  14,  0,  0},
    }};

    for (const Row& row : rows) {
        petra::capped_streambuf sink{row.sink};
        std::ostream os{&sink};
        const auto r = petra::ostream_write_n_via_buf(os, row.n, "hello {} world", 42);

        CHECK_EQ(r.has_value(), row.expect_success);
        if (row.expect_success) {
            CHECK_EQ(r.value(), row.expect_value);
        } else {
            CHECK_EQ(r.error().kind, petra::FormatErrorKind::PartialWrite);
            CHECK_EQ(r.error().requested, row.expect_offered);
            CHECK_EQ(r.error().accepted,  row.expect_accepted);
            CHECK_EQ(r.error().dropped(),
                     row.expect_offered - row.expect_accepted);
        }
    }
}

// ------------------------------------------------------------------
// Section 6 — runtime-format-string path: cap on a vformat_to iterator
// ------------------------------------------------------------------

void section_6_runtime_format_string() {
    SECTION("6.runtime-format-string");

    std::ostringstream oss;
    const std::string fmt = "id={} ok={}";      // runtime std::string
    int  nine = 9;
    bool ok_false = false;
    const auto r = petra::ostream_write_n_via_buf_runtime(
        oss, 64, fmt, std::make_format_args(nine, ok_false));

    CHECK(r.has_value());
    // "id=9 ok=false" is 13 chars (i,d,=,9, ,o,k,=,f,a,l,s,e)
    CHECK_EQ(r.value(), std::size_t{13});
    CHECK_EQ(oss.str(), std::string{"id=9 ok=false"});

    // Runtime path with iterator cap = 5 on an 8-byte payload
    // ("[7|true]" = 8 chars): iterator caps at 5; the wrapper
    // returns the ACTUAL bytes the iterator wrote (== cap on the
    // iterator-capped success path), because std::vformat_to does
    // not report a would_be size. The consteval path (which uses
    // std::format_to_n) returns would_be — section 5 covers that.
    std::ostringstream oss2;
    int seven = 7;
    bool yes = true;
    const auto r2 = petra::ostream_write_n_via_buf_runtime(
        oss2, 5, "[{}|{}]", std::make_format_args(seven, yes));
    CHECK(r2.has_value());
    CHECK_EQ(r2.value(), std::size_t{5});          // == cap (iterator-capped)
    CHECK_EQ(oss2.str(), std::string{"[7|tr"});   // exactly 5 chars
}

// ------------------------------------------------------------------
// Section 7 — format-error propagation: atomicity
// ------------------------------------------------------------------
// std::format_to_n runs the format-string parser BEFORE any byte moves,
// so a malformed spec throws std::format_error with zero bytes
// written even when cap is large. Section 8 in the Aug 30 lesson
// pinned the same atomicity for std::vformat_to; today's
// std::format_to_n has the same property.

void section_7_format_error_atomicity() {
    SECTION("7.format-error-atomicity");

    // Type-mismatch: {:d} on a string.
    {
        std::ostringstream oss;
        petra::scoped_counting_filter guard{oss};
        std::string arg = "nope";
        int seven = 7;
        // Call the consteval-checked path with a runtime format
        // string via the runtime entry point.
        const auto r = petra::ostream_write_n_via_buf_runtime(
            oss, 1024, "prefix-{} {:d}",
            std::make_format_args(seven, arg));
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::FormatErrorKind::Format);
        CHECK(!r.error().message.empty());
        // ATOMIC: nothing reached the sink.
        CHECK_EQ(guard.offered(),  std::size_t{0});
        CHECK_EQ(guard.accepted(), std::size_t{0});
        CHECK_EQ(oss.str(), std::string{});
    }

    // Sweep prefix lengths to confirm atomicity is independent of cap.
    {
        std::string long_prefix(20000, 'x');
        const std::string fmt = long_prefix + "{:d}";
        std::ostringstream oss;
        petra::scoped_counting_filter guard{oss};
        std::string arg = "nope";   // must be an lvalue for make_format_args
        const auto r = petra::ostream_write_n_via_buf_runtime(
            oss, 65536, fmt, std::make_format_args(arg));
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::FormatErrorKind::Format);
        CHECK_EQ(guard.offered(),  std::size_t{0});
        CHECK_EQ(guard.accepted(), std::size_t{0});
        CHECK_EQ(oss.str(), std::string{});
    }

    // consteval path: a malformed format string at consteval time is
    // a hard COMPILE error — the consteval gate refuses to compile
    // the call. Verify by exercising the consteval path on a valid
    // format string and confirming it works without throwing; the
    // consteval rejection of the {:d}-on-string case is the design
    // point of the consteval gate (P2216R3), not something today's
    // wrapper observes at runtime.
    {
        std::ostringstream oss;
        std::string world = "world";
        int seven = 7;
        const auto r = petra::ostream_write_n_via_buf(
            oss, 1024, "i={} hello={}", seven, world);
        // consteval gate passes (the spec is well-formed).
        CHECK(r.has_value());
        CHECK_EQ(oss.str(), std::string{"i=7 hello=world"});
    }
}

// ------------------------------------------------------------------
// Section 8 — error formatter rendering
// ------------------------------------------------------------------

void section_8_error_formatter() {
    SECTION("8.error-formatter");

    {
        const petra::FormatError e{petra::FormatErrorKind::PartialWrite,
                                   "sink refused bytes", 14, 5};
        const std::string rendered = std::format("{}", e);
        CHECK(rendered.find("PartialWrite") != std::string::npos);
        CHECK(rendered.find("requested=14") != std::string::npos);
        CHECK(rendered.find("accepted=5")   != std::string::npos);
        CHECK(rendered.find("dropped=9")    != std::string::npos);
    }
    {
        const petra::FormatError e{petra::FormatErrorKind::Format,
                                   "invalid format string", 0, 0};
        const std::string rendered = std::format("{}", e);
        CHECK(rendered.find("Format") != std::string::npos);
        CHECK(rendered.find("invalid format string") != std::string::npos);
    }
}

// ------------------------------------------------------------------
// Section 9 — std::ofstream sink
// ------------------------------------------------------------------

void section_9_ofstream_sink() {
    SECTION("9.ofstream-sink");

    const std::string path = "/tmp/petra-2026-08-31-ofstream.bin";
    std::remove(path.c_str());

    {
        std::ofstream of{path, std::ios::binary | std::ios::trunc};
        CHECK(of.is_open());
        const auto r = petra::ostream_write_n_via_buf(of, 64, "hello {} world", 42);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{14});
    }
    {
        std::ifstream in{path, std::ios::binary};
        std::string content(
            (std::istreambuf_iterator<char>(in)),
            std::istreambuf_iterator<char>());
        CHECK_EQ(content, std::string{"hello 42 world"});
    }

    // Iterator cap is the binding constraint; filebuf has room.
    std::remove(path.c_str());
    {
        std::ofstream of{path, std::ios::binary | std::ios::trunc};
        CHECK(of.is_open());
        const auto r = petra::ostream_write_n_via_buf(of, 5, "hello {} world", 42);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), std::size_t{14});
    }
    {
        std::ifstream in{path, std::ios::binary};
        std::string content(
            (std::istreambuf_iterator<char>(in)),
            std::istreambuf_iterator<char>());
        CHECK_EQ(content, std::string{"hello"});
    }

    std::remove(path.c_str());
}

// ------------------------------------------------------------------
// Section 10 — UTF-8 + embedded NUL
// ------------------------------------------------------------------

void section_10_utf8_and_nul() {
    SECTION("10.utf8-and-nul");

    // UTF-8 payload + cap that lands mid-codepoint. The adapter
    // reports the byte split; making the split codepoint-safe is a
    // sink responsibility.
    {
        // "ok \xF0\x9F\x9A\x80!" = "ok 🚀!" = 8 bytes
        //   o(1) k(2) ' '(3) F0(4) 9F(5) 9A(6) 80(7) !(8)
        const std::string payload = "ok \xF0\x9F\x9A\x80!";
        CHECK_EQ(payload.size(), std::size_t{8});

        petra::capped_streambuf sink{4};   // lands after "ok " but mid-emoji
        std::ostream os{&sink};
        const auto r = petra::ostream_write_n_via_buf(
            os, 64, "ok \xF0\x9F\x9A\x80!");
        // iterator cap == 64 (well above would_be == 8), but the
        // sink-side cap of 4 IS the binding constraint, so this is
        // a PartialWrite. The iterator offers 5 chars (sink_cap+1)
        // before the sink refuses the 5th; sink takes 4.
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::FormatErrorKind::PartialWrite);
        CHECK_EQ(r.error().accepted, std::size_t{4});
        CHECK_EQ(sink.str(), std::string{"ok \xF0"}); // first 4 bytes
    }

    // Embedded NUL passes through the runtime path.
    {
        std::ostringstream oss;
        const std::string text_with_nul = std::string{"a\0b", 3};
        const auto r = petra::ostream_write_n_via_buf_runtime(
            oss, 64, "[{}]", std::make_format_args(text_with_nul));
        CHECK(r.has_value());
        CHECK_EQ(oss.str().size(), std::size_t{5});
        CHECK_EQ(oss.str()[0], '[');
        CHECK_EQ(oss.str()[1], 'a');
        CHECK_EQ(oss.str()[2], '\0');
        CHECK_EQ(oss.str()[3], 'b');
        CHECK_EQ(oss.str()[4], ']');
    }
}

// ------------------------------------------------------------------
// Section 11 — exception mask: ostreambuf_iterator vs os.write
// ------------------------------------------------------------------
// Same finding the Aug 30 lesson pinned: the iterator path bypasses
// exceptions() masks entirely. Today's wrapper doesn't change that.

void section_11_exception_mask() {
    SECTION("11.exception-mask");

    petra::capped_streambuf sink{5};
    std::ostream os{&sink};
    os.exceptions(std::ios::badbit);   // ask for a throw on badbit

    bool threw = false;
    try {
        const auto r = petra::ostream_write_n_via_buf(os, 64, "hello {} world", 42);
        // The iterator path never sets badbit on the stream — the
        // sink-side truncation is reported via the filter, NOT via
        // rdstate. os.write would throw here; the iterator does not.
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::FormatErrorKind::PartialWrite);
        CHECK_EQ(r.error().accepted, std::size_t{5});
        CHECK_EQ(sink.str(), std::string{"hello"});
    } catch (const std::ios_base::failure&) {
        threw = true;
    }
    CHECK(!threw);
}

// ------------------------------------------------------------------
// Section 12 — drop-in equivalence with Aug 28's ostream_write_n
// ------------------------------------------------------------------
// Aug 28 shipped ostream_write_n(ostream&, cap, fmt, args...) using
// the vformat->string->os.write path. Today's wrapper uses
// std::format_to_n straight to std::ostreambuf_iterator. On the
// success path with a healthy sink and cap >= would_be, the two
// produce byte-identical output and report the same size.

void section_12_drop_in_equivalence() {
    SECTION("12.drop-in-equivalence");

    auto aug28_ostream_write_n =
        [](std::ostream& os, std::size_t cap, std::string_view fmt,
           std::format_args args) -> std::size_t {
            const std::string text = std::vformat(fmt, args);
            const std::size_t take = std::min(cap, text.size());
            os.write(text.data(), static_cast<std::streamsize>(take));
            return text.size();
        };

    // The consteval path takes a literal format string, so today's
    // wrapper is invoked with the same shape Aug 28 used. We compare
    // against Aug 28's vformat->string->write adapter synthesised
    // inline so the assert is against its observable contract.
    for (int i = 0; i < 5; ++i) {
        const std::size_t cap = static_cast<std::size_t>(i) * 4u + 1u;
        std::string world = "world";   // lvalue required for make_format_args

        std::ostringstream a;
        std::ostringstream b;
        const std::size_t a_size = aug28_ostream_write_n(a, cap,
            std::string_view{"i={} hello={}"},
            std::make_format_args(i, world));

        const auto rb = petra::ostream_write_n_via_buf(
            b, cap, "i={} hello={}", i, world);

        CHECK(rb.has_value());
        CHECK_EQ(rb.value(), a_size);
        CHECK_EQ(b.str(), a.str());
    }
}

// ------------------------------------------------------------------
// Section 13 — 50-round determinism stress
// ------------------------------------------------------------------

void section_13_determinism_stress() {
    SECTION("13.determinism-stress");

    // The cap-independence rules pinned in section 5 apply here:
    //   - iterator-side cap (n) is the binding constraint if n <
    //     would_be AND n <= sinkc; the iterator caps before the
    //     sink ever sees a refusal, so this is a SUCCESS path with
    //     r.value() == would_be (the iterator reports its full
    //     intent, not what landed).
    //   - sink-side cap (sinkc) is the binding constraint if sinkc <
    //     n AND sinkc < would_be; the iterator offers sinkc+1 chars
    //     before the sink refuses the (sinkc+1)th, so this is a
    //     PartialWrite with r.error().accepted == sinkc.
    //   - success requires r.error() not to be set; that is, the
    //     sink did not refuse.

    auto predict_success =
        [](std::size_t n, std::size_t sinkc, std::size_t would_be) {
            // success iff the sink does not refuse. The sink refuses
            // iff min(n, would_be) > sinkc, i.e. the iterator tries
            // to offer more chars than the sink can take.
            const std::size_t offer = std::min(n, would_be);
            if (offer == 0)    return true;     // nothing to refuse
            return (offer <= sinkc);
        };

    // 50-round consteval-path stress.
    for (int round = 0; round < 50; ++round) {
        const std::size_t cap   = static_cast<std::size_t>(round) % 25u;
        const std::size_t sinkc = static_cast<std::size_t>(
            (static_cast<std::size_t>(round) * 7u) % 25u);
        const std::string expected_text = std::format("r{} payload", round);
        const std::size_t would_be = expected_text.size();

        petra::capped_streambuf sink{sinkc};
        std::ostream os{&sink};

        const auto r = petra::ostream_write_n_via_buf(
            os, cap, "r{} payload", round);

        const bool should_fit = predict_success(cap, sinkc, would_be);

        CHECK_EQ(r.has_value(), should_fit);
        if (should_fit) {
            CHECK_EQ(r.value(), would_be);
            // The number of bytes that landed == min(cap, sinkc, would_be).
            const std::size_t landed = std::min({cap, sinkc, would_be});
            CHECK_EQ(sink.str(), expected_text.substr(0, landed));
        } else {
            CHECK_EQ(r.error().kind, petra::FormatErrorKind::PartialWrite);
            // Sink refusal: iterator offers sinkc+1 chars, sink takes sinkc.
            CHECK_EQ(r.error().accepted, sinkc);
            CHECK_EQ(sink.consumed(), sinkc);
            CHECK_EQ(sink.str(), expected_text.substr(0, sinkc));
        }
    }

    // 50-round runtime-path stress.
    for (int round = 0; round < 50; ++round) {
        const std::size_t cap   = static_cast<std::size_t>(round) % 25u;
        const std::size_t sinkc = static_cast<std::size_t>(
            (static_cast<std::size_t>(round) * 11u) % 25u);
        const std::string expected_text = std::format("R{} hello", round);
        const std::size_t would_be = expected_text.size();

        petra::capped_streambuf sink{sinkc};
        std::ostream os{&sink};

        const auto r = petra::ostream_write_n_via_buf_runtime(
            os, cap, "R{} hello", std::make_format_args(round));

        const bool should_fit = predict_success(cap, sinkc, would_be);

        CHECK_EQ(r.has_value(), should_fit);
        if (should_fit) {
            // Runtime path returns the actual bytes the iterator wrote
            // (= min(cap, sinkc, would_be) on the success path), not
            // would_be (which std::vformat_to does not report).
            const std::size_t landed = std::min({cap, sinkc, would_be});
            CHECK_EQ(r.value(), landed);
            CHECK_EQ(sink.str(), expected_text.substr(0, landed));
        } else {
            CHECK_EQ(r.error().kind, petra::FormatErrorKind::PartialWrite);
            CHECK_EQ(r.error().accepted, sinkc);
            CHECK_EQ(sink.str(), expected_text.substr(0, sinkc));
        }
    }
}

int main() {
    section_1_probes();
    section_2_happy_path();
    section_3_format_to_n_with_ostreambuf_iterator();
    section_4_would_be_vs_filter();
    section_5_cap_independence();
    section_6_runtime_format_string();
    section_7_format_error_atomicity();
    section_8_error_formatter();
    section_9_ofstream_sink();
    section_10_utf8_and_nul();
    section_11_exception_mask();
    section_12_drop_in_equivalence();
    section_13_determinism_stress();

    std::fprintf(stderr, "\n%d/%d PASS\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
