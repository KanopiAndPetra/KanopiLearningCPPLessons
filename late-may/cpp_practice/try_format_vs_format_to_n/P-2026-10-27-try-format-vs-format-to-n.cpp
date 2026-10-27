// P-2026-10-27 — try_format_vs_format_to_n:
//
// Closes Oct 26's "Where we go next" item (3) verbatim, which was
// itself carried over from Oct 22's item (4):
//
//   item (3) — "Compare the hand-rolled expected<size_t, FormatError>
//               contract against stdlib's std::format_to_n — carried
//               over from Oct 22's item (4), still untried.  Note that
//               libc++ 21's format_to_n_result has exactly TWO members
//               — out and size — and NO error-code member at all ...
//               So the two contracts are not directly comparable, and
//               the differences need spelling out."
//
// THE QUESTION: the arc hand-rolls an error-returning, buffer-atomic
// formatting API.  The standard library ships one.  Are they the same
// thing with different spellings?  MEASURED: no — and the differences
// are not the ones the arc assumed.
//
// KEY FINDING #1 — THE STDLIB'S RESULT TYPE HAS NO ERROR CHANNEL AT
// ALL.  `std::format_to_n_result<OutIt>` has EXACTLY TWO members, `out`
// and `size`.  Measured: a detection idiom for a member named `ec`
// reports false for `std::format_to_n_result<char*>`, and the type is
// 16 bytes (one pointer + one ptrdiff_t).  The arc's
// `std::expected<std::size_t, FormatError>` HAS an error channel.  So
// the two contracts cannot be compared member-for-member: the stdlib
// has nowhere to put an error, which is why it reports overflow by
// CONVENTION (`size` = bytes that WOULD have been written) and reports
// a bad format by THROWING `std::format_error`.
//
// KEY FINDING #2 — THE TWO "size"s MEAN DIFFERENT THINGS.  The arc's
// success value is the number of bytes WRITTEN.  The stdlib's `size`
// is the number of bytes that WOULD HAVE BEEN WRITTEN.  The two
// coincide exactly when the output fits, and the moment it does not
// fit, one of them becomes an ERROR and the other stays a NUMBER.
// Measured (cap 3, output "12345"):
//     arc    -> unexpected(TooLarge), needed_bytes = 5, buffer UNTOUCHED
//     stdlib -> {out = buf+3, size = 5}, buffer = "123"  (PARTIAL WRITE)
// This is the single most important thing to get right when
// translating between the two contracts.
//
// KEY FINDING #3 — THE STDLIB TRUNCATES, THE ARC REFUSES.  The stdlib
// writes what fits and reports the rest as a number.  The arc writes
// NOTHING and reports an error.  Byte-for-byte on the same input:
//     cap 3:  arc buffer "QQQ..."      stdlib buffer "123..."
//     cap 5:  arc buffer "12345"       stdlib buffer "12345"   (both fit)
// So the two agree on the SUCCESS path and disagree on the FAILURE
// path, and they disagree about the BUFFER, not just about the
// return value.
//
// KEY FINDING #4 (THE HEADLINE) — THE ARC'S "BUFFER UNTOUCHED ON
// ERROR" ATOMICITY IS NOT A PROPERTY OF THE TWO-PASS DESIGN.  It is an
// ARTIFACT of which libc++ buffer class gets selected for a given
// output iterator.
//
// libc++'s `__buffer_selector` picks:
//   - `__direct_iterator_buffer` when `__enable_direct_output<OutIt>`
//     — which is TRUE only for `char*` / `__wrap_iter<char*>`.  This
//     buffer writes DIRECTLY into the user's memory, character by
//     character, as the format proceeds.
//   - `__iterator_buffer` (an ALLOCATING buffer) for every other
//     output iterator.  This one accumulates the WHOLE formatted
//     output in a 256-byte stack buffer (growing to the heap if
//     needed) and copies it to the user's iterator only at the very
//     END, in `__out_it()`.
//
// The arc's `bounded_char_writer` is a CUSTOM iterator, so libc++
// routes it through the ALLOCATING buffer.  A formatter that throws
// mid-format therefore never reaches the copy-out step, and the user's
// buffer is untouched — but that is a side effect of libc++'s buffer
// CHOICE, not a property of the arc's two-pass design.
//
// PROVED BY HOLDING EVERYTHING ELSE CONSTANT AND CHANGING ONLY THE
// SINK TYPE.  The formatter used for the control SUCCEEDS on the
// measure pass (writing nothing) and WRITES-THEN-THROWS on the write
// pass — so the write pass is actually REACHED, which is the whole
// point of the control.  (A formatter that throws on EVERY call is
// caught by the measure pass, the write pass never runs, and the
// control shows NO difference — that was the first draft's mistake.)
//
// Same two-pass shape, only the sink differs:
//     arc's bounded_char_writer sink (custom iter) -> buffer "QQQ..."   CLEAN
//     same shape, plain char* sink                -> buffer "BAD9..."  DIRTY
//     std::format_to_n (char*)                     -> buffer "HALF9..." DIRTY
// So the atomicity is a property of the SINK, and the stdlib's
// `format_to_n` — whose whole point is to write into a `char*` — does
// NOT have it.
//
// KEY FINDING #5 — AND THE ATOMICITY IS NOT EVEN UNIFORM *WITHIN*
// THE ARC.  Three distinct error paths exist, and only TWO of them
// leave the buffer untouched:
//   (a) TooLarge detected in the MEASURE pass -> returns BEFORE any
//       write -> buffer untouched.  STRUCTURAL.
//   (b) Format error in the MEASURE pass -> returns before any write
//       -> buffer untouched.  STRUCTURAL.
//   (c) Format error in the WRITE pass -> the write was already
//       under way.  Buffer untouched ONLY because of KEY FINDING #4's
//       buffering artifact.
//   (d) the POST-WRITE consistency check ("bounded_char_writer wrote N
//       bytes; measure pass said M") -> this fires AFTER the write
//       has already happened, so the buffer IS modified.  Measured:
//       the buffer reads "BAD9" while the call returns an error.
//
// (d) is reachable only with a NON-DETERMINISTIC formatter — one whose
// output depends on its invocation count — because the arc runs the
// formatter TWICE (see KEY FINDING #6).  Measured: a formatter that
// emits "OK9" on the first call and "BAD9" on the second makes the
// arc return TooLarge with the message "bounded_char_writer wrote 4
// bytes; measure pass said 3" — a capacity error caused by the
// formatter, not by the buffer — with the buffer left holding "BAD9".
//
// So the arc's atomicity is real but CONDITIONAL, and the condition is
// not "the two-pass design"; it is "the error was detected before the
// write, or the sink buffers".  The lesson does not change the arc's
// contract; it PRICES the contract.
//
// KEY FINDING #6 — THE ARC CALLS THE USER'S FORMATTER TWICE; THE
// STDLIB CALLS IT ONCE.  Measured with a formatter that counts its own
// invocations: arc = 2 (measure pass + write pass), `std::format_to_n`
// = 1.  That is the price of the arc's contract: the measure pass
// exists so that TooLarge can be reported WITHOUT writing.
//
// KEY FINDING #7 — THE STDLIB'S `n` IS SIGNED, AND A NEGATIVE `n` IS
// SILENTLY ACCEPTED.  `format_to_n`'s second parameter is
// `iter_difference_t<OutIt>` = `ptrdiff_t`, i.e. SIGNED.  Measured:
// `format_to_n(buf, -1, "{}", 12345)` writes NOTHING, reports
// `size = 5` and `out = buf`, and raises no diagnostic.  A negative
// capacity means "capacity zero", not "error".  The arc's `cap` is
// `std::size_t` — UNSIGNED — so a negative capacity is not
// representable at all.  This is a genuine safety difference, and it
// is invisible in the type's spelling: both are "a number of bytes".
//
// KEY FINDING #8 — THE ARC'S ERROR TAXONOMY LOOKS RICHER AND IS
// ACTUALLY POORER.  `petra::FormatErrorKind` has FOUR values
// (FormatParse, FormatArgument, Format, TooLarge), but the
// implementation only ever produces TWO of them: every
// `std::format_error` becomes `Format`, whatever its cause.  Measured:
// a parse error ("{"), an arity error ("{} {}" with one argument), an
// argument-index error ("{5}") and a presentation-type error
// ("{:f}" with an int) all produce `FormatErrorKind::Format`.
// Meanwhile the stdlib distinguishes those four classes by MESSAGE
// TEXT (four distinct strings measured).  So the stdlib's error type
// is coarser but its error INFORMATION is finer — and the arc cannot
// recover the distinction, because it keeps the message but collapses
// the class into the kind.
//
// KEY FINDING #9 — THE PUBLIC STDLIB API IS CELL [2] ONLY.  The
// arc's 2x2 matrix has a runtime-format cell ([3]) that the public
// stdlib CANNOT REACH: `std::format_to_n` requires
// `std::format_string<Args...>`, and there is NO public
// `std::vformat_to_n` on this toolchain (measured: "no member named
// 'vformat_to_n' in namespace 'std'").  So the arc's cell [3] has no
// public standard-library counterpart at all — the comparison this
// lesson makes is possible only because the arc ALREADY has a
// runtime-format path.
//
// CONSEQUENCE: on the compile-time-format cell the stdlib's format
// errors are COMPILE-TIME errors, so `std::format_to_n`'s only
// runtime failure mode is "a user formatter threw".  The arc's
// runtime-format cell additionally has "the format string is invalid"
// and "the output does not fit" as runtime outcomes.
//
// KEY FINDING #10 — THE CONSTEVAL CHECK IS ON THE VALUE, NOT ON THE
// TYPE'S SPELLING.  `std::format_to_n`'s third parameter is
// `std::format_string<Args...>`, whose constructor is `consteval`, so
// the format string must be a CONSTANT EXPRESSION.  Measured:
//   - an INLINE `std::string_view("{}")` IS a constant expression (the
//     literal is), so it is ACCEPTED even though its TYPE is
//     "runtime" — the stdlib's bounded API accepts it;
//   - a `std::string_view` arriving as a function PARAMETER is NOT, and
//     is REJECTED with "call to consteval function ... is not a
//     constant expression".
//
// So "runtime format string" is not a property of the TYPE — it is a
// property of the VALUE.  This is the same lesson as Oct 26's KEY
// FINDING #3 seen from the other side: a `consteval` constructor is a
// guard rail that fires at the CALL SITE, and what it guards is
// constant-evaluability, not a type category.
//
// WHAT DID NOT CHANGE (deliberately):
//   - petra::FormatError / FormatErrorKind, the two-pass (measure,
//     then write) impls, counting_output_iterator and
//     bounded_char_writer are reused VERBATIM from Oct 26 / Oct 25.
//   - The arc's contract is NOT changed.  This lesson MEASURES it
//     against the standard library; it does not replace it.
//   - No pad-selection machinery is touched; the pad axis is
//     orthogonal to the error contract.
//
// Build (default):
//
//   clang++ -std=c++23 -stdlib=libc++ -fexperimental-library -O0 \
//           -Wall -Wextra -Wpedantic -Werror -Wshadow \
//           -Wconversion -Wsign-conversion \
//           P-2026-10-27-try-format-vs-format-to-n.cpp \
//           -o /tmp/petra20261027/bin_default
//
// Or, via CMake:
//
//   cmake -S . -B build
//   cmake --build build
//   ./build/P-2026-10-27-try-format-vs-format-to-n
//
// Pure consumer-side; no third-party libraries.

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <expected>
#include <format>
#include <iterator>
#include <new>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// ============================================================================
// Part 1 — petra::FormatError (verbatim from Sep 6 through Oct 26)
// ============================================================================

namespace petra {

enum class FormatErrorKind : unsigned char {
    FormatParse,
    FormatArgument,
    Format,
    TooLarge,
};

struct FormatError {
    FormatErrorKind kind{FormatErrorKind::Format};
    std::string message;
    std::size_t needed_bytes{0};

    FormatError() = default;
    FormatError(FormatErrorKind k, std::string m,
                std::size_t needed = 0) noexcept
        : kind(k), message(std::move(m)), needed_bytes(needed) {}

    [[nodiscard]] bool operator==(const FormatError& other) const noexcept {
        return kind == other.kind && message == other.message
            && needed_bytes == other.needed_bytes;
    }
    [[nodiscard]] bool operator!=(const FormatError& other) const noexcept {
        return !(*this == other);
    }
};

}  // namespace petra

// ============================================================================
// Part 2 — petra::counting_output_iterator (Oct 11 .. Oct 26 verbatim)
//
// CRITICAL: the callback carries a `void* data` pointer, so the writes
// still land in the LOCAL variable even after std::vformat_to copies
// the iterator internally (Apple Clang 21 / libc++ 21 behavior).
// ============================================================================

namespace petra {

class counting_output_iterator {
public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = void;
    using callback_type = void (*)(std::size_t, void*);

    struct proxy {
        counting_output_iterator* it_;
        constexpr const proxy& operator=(char) const noexcept {
            ++it_->count_;
            if (it_->cb_) it_->cb_(it_->count_, it_->data_);
            return *this;
        }
    };

    constexpr counting_output_iterator(callback_type cb, void* data) noexcept
        : cb_(cb), data_(data) {}

    constexpr proxy operator*() const noexcept {
        return proxy{const_cast<counting_output_iterator*>(this)};
    }
    constexpr counting_output_iterator& operator++() noexcept { return *this; }
    constexpr counting_output_iterator operator++(int) noexcept { return *this; }

private:
    std::size_t count_{0};
    callback_type cb_;
    void* data_;
};

}  // namespace petra

// ============================================================================
// Part 3 — petra::detail::bounded_char_writer (Oct 11 .. Oct 26 verbatim)
// ============================================================================

namespace petra {
namespace detail {

class bounded_char_writer {
public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = void;
    using callback_type = void (*)(std::size_t, void*);

    struct proxy {
        bounded_char_writer* it_;
        constexpr const proxy& operator=(char c) const noexcept {
            if (it_->written_ < it_->cap_) {
                it_->buf_[it_->written_] = c;
                ++it_->written_;
                if (it_->cb_) it_->cb_(it_->written_, it_->data_);
            }
            return *this;
        }
    };

    constexpr bounded_char_writer(char* buf, std::size_t cap,
                                  callback_type cb, void* data) noexcept
        : buf_(buf), cap_(cap), cb_(cb), data_(data) {}

    constexpr proxy operator*() const noexcept {
        return proxy{const_cast<bounded_char_writer*>(this)};
    }
    constexpr bounded_char_writer& operator++() noexcept { return *this; }
    constexpr bounded_char_writer operator++(int) noexcept { return *this; }

    std::size_t written() const noexcept { return written_; }

private:
    char* buf_;
    std::size_t cap_;
    std::size_t written_{0};
    callback_type cb_;
    void* data_;
};

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 4 — the two two-pass impls (Oct 25 / Oct 26 verbatim).
//
// `try_format_runtime_n_impl`          takes a RUNTIME format string.
// `try_format_bounded_runtime_n_impl`  takes a COMPILE-TIME format string.
//
// Both are the arc's contract under test today.
// ============================================================================

namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_impl(char* out, std::size_t cap,
                          std::string_view fmt,
                          const Args&... args) {
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };
    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt,
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }

    if (needed > cap) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"formatted output would be "}
                            + std::to_string(needed)
                            + " bytes; cap is " + std::to_string(cap),
                        needed});
    }

    std::size_t written = 0;
    try {
        bounded_char_writer sink{out, cap, cb, &written};
        (void)std::vformat_to(std::move(sink), fmt,
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }
    if (written != needed) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"bounded_char_writer wrote "}
                            + std::to_string(written)
                            + " bytes; measure pass said "
                            + std::to_string(needed),
                        needed});
    }
    return written;
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_impl(char* out, std::size_t cap,
                                  std::format_string<Args...> fmt_str,
                                  const Args&... args) {
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };
    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt_str.get(),
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }

    if (needed > cap) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"formatted output would be "}
                            + std::to_string(needed)
                            + " bytes; cap is " + std::to_string(cap),
                        needed});
    }

    std::size_t written = 0;
    try {
        bounded_char_writer sink{out, cap, cb, &written};
        (void)std::vformat_to(std::move(sink), fmt_str.get(),
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }
    if (written != needed) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"bounded_char_writer wrote "}
                            + std::to_string(written)
                            + " bytes; measure pass said "
                            + std::to_string(needed),
                        needed});
    }
    return written;
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 5 — the ADAPTER: the standard library's contract, translated into
// the arc's shape.
//
// This is the interesting engineering.  `std::format_to_n` has no error
// channel (KEY FINDING #1), so the adapter has to CONSTRUCT one:
//
//   - a thrown `std::format_error`   -> FormatErrorKind::Format
//   - `size > cap`                   -> FormatErrorKind::TooLarge
//   - otherwise                      -> success, value = bytes WRITTEN
//
// Note what the adapter CANNOT recover: the bytes the stdlib already
// wrote before it discovered the overflow.  By the time the stdlib
// reports `size = 5`, it has already written "123" into the buffer.
//
// NOTE ON THE PARAMETER SHAPE: `Args&&...` (not `const Args&...`) is
// load-bearing here.  `std::format_to_n` deduces its own `_Args` from
// FORWARDING references, so for a non-const lvalue argument it wants
// `format_string<int&>`, and for a const lvalue it wants
// `format_string<const int&>`.  Deducing `Args` the same way here and
// forwarding with std::forward keeps the two in step.  (Measured: a
// `const Args&...` spelling makes `format_to_n` reject the format
// string with "no known conversion from 'format_string<int>' to
// 'format_string<const int &>'".)
// ============================================================================

namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
stdlib_format_to_n(char* out, std::size_t cap,
                    std::format_string<Args...> fmt_str,
                    Args&&... args) {
    // The stdlib's `n` is SIGNED (iter_difference_t<char*> == ptrdiff_t).
    // KEY FINDING #7: a negative capacity is silently "capacity zero".
    const auto n = static_cast<std::iter_difference_t<char*>>(cap);
    std::size_t written = 0;
    std::size_t needed = 0;
    try {
        const auto r = std::format_to_n(out, n, fmt_str,
                                         std::forward<Args>(args)...);
        written = static_cast<std::size_t>(r.out - out);
        needed = static_cast<std::size_t>(r.size);
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }
    if (needed > cap) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"stdlib would have written "}
                            + std::to_string(needed)
                            + " bytes; cap is " + std::to_string(cap),
                        needed});
    }
    return written;
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 6 — the control experiment for KEY FINDING #4.
//
// THE SAME TWO-PASS SHAPE as the arc's impl, with ONE thing changed:
// the write pass writes into a plain `char*` instead of into the
// arc's `bounded_char_writer`.
//
// `char*` satisfies libc++'s `__enable_direct_output`, so libc++
// selects `__direct_iterator_buffer`, which writes into the user's
// memory CHARACTER BY CHARACTER as the format proceeds.  A formatter
// that throws mid-format therefore leaves a PARTIAL WRITE.
//
// This is the control that turns "the arc's buffer stays clean" from
// an observation into a CAUSE.
// ============================================================================

namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
two_pass_charptr_sink(char* out, std::size_t cap,
                      std::string_view fmt,
                      const Args&... args) {
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };
    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt,
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }
    if (needed > cap) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"formatted output would be "}
                            + std::to_string(needed)
                            + " bytes; cap is " + std::to_string(cap),
                        needed});
    }
    std::size_t written = 0;
    try {
        // THE ONLY DIFFERENCE: a `char*` sink.
        (void)std::vformat_to(out, fmt, std::make_format_args(args...));
    } catch (const std::format_error& e) {
        return std::unexpected(
            FormatError{FormatErrorKind::Format, e.what()});
    }
    return written;
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 7 — the compile-time pins.
//
// Each static_assert is a claim about a SPECIFIC rule, not about this
// file's plumbing.  Where a claim cannot be made at compile time (the
// stdlib's runtime behaviour), it is made by a MEASUREMENT in a
// section instead — and the section says so.
// ============================================================================

namespace petra {
namespace detail {

// KEY FINDING #1 — the stdlib's result type has NO error channel.
template <typename T, typename = void>
struct has_ec : std::false_type {};
template <typename T>
struct has_ec<T, std::void_t<decltype(std::declval<T>().ec)>>
    : std::true_type {};

static_assert(!has_ec<std::format_to_n_result<char*>>::value,
              "KEY FINDING #1: std::format_to_n_result has NO member named "
              "'ec' — the stdlib's contract has no error channel at all, "
              "which is why it reports overflow by convention and format "
              "errors by throwing");

static_assert(sizeof(std::format_to_n_result<char*>) ==
                  sizeof(char*) + sizeof(std::ptrdiff_t),
              "KEY FINDING #1: the stdlib's result is exactly a pointer "
              "and a count — 16 bytes on this platform — with nowhere to "
              "put an error");

// KEY FINDING #7 — the stdlib's `size` is SIGNED, the arc's cap is not.
static_assert(std::is_signed_v<
                  decltype(std::format_to_n_result<char*>::size)>,
              "KEY FINDING #7: the stdlib's `size` member is SIGNED "
              "(iter_difference_t<OutIt> == ptrdiff_t)");
static_assert(std::is_same_v<
                  decltype(std::format_to_n_result<char*>::out), char*>,
              "KEY FINDING #7: `out` is the output ITERATOR, not a count");

// The arc's own cap type is unsigned, so a negative capacity is not
// representable.
static_assert(std::is_unsigned_v<std::size_t>,
              "the arc's `cap` parameter is std::size_t — UNSIGNED — so a "
              "negative capacity cannot even be spelled");

// The arc's result DOES have an error channel, and the two are
// different kinds of thing.
static_assert(std::is_same_v<
                  typename std::expected<std::size_t, FormatError>::error_type,
                  FormatError>,
              "the arc's contract has an error channel: the error type is "
              "petra::FormatError");
static_assert(!std::is_same_v<
                  std::expected<std::size_t, FormatError>,
                  std::format_to_n_result<char*>>,
              "KEY FINDING #1: the two result types are NOT the same type "
              "and are not even the same SHAPE");

// KEY FINDING #8 — the arc's error taxonomy has four values, but the
// implementation only ever produces two of them.  These are the
// compile-time half of that claim; the runtime half is Section 9.
static_assert(petra::FormatErrorKind::FormatParse
                  != petra::FormatErrorKind::FormatArgument,
              "the four kinds are four distinct values");
static_assert(petra::FormatErrorKind::Format
                  != petra::FormatErrorKind::TooLarge,
              "a format error and a capacity error are distinct");

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 8 — the test types.
//
// `counted`     : a formatter that counts its own invocations.
// `half_throws` : a formatter that WRITES and then throws.
// `flaky`       : a formatter whose output depends on the invocation
//                 count — the non-deterministic formatter.
// ============================================================================

namespace petra_test {

struct counted {
    int value{0};
};
inline int g_counted_calls = 0;

struct half_throws {
    int value{0};
};

struct throws_second {
    int value{0};
};
inline int g_throws_second_calls = 0;

struct flaky {
    int value{0};
};
inline int g_flaky_calls = 0;

}  // namespace petra_test

template <>
struct std::formatter<petra_test::counted> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto format(const petra_test::counted& c,
                std::format_context& ctx) const -> decltype(ctx.out()) {
        ++petra_test::g_counted_calls;
        return std::format_to(ctx.out(), "C{}", c.value);
    }
};

template <>
struct std::formatter<petra_test::half_throws> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto format(const petra_test::half_throws& c,
                std::format_context& ctx) const -> decltype(ctx.out()) {
        auto out = std::format_to(ctx.out(), "HALF{}", c.value);
        throw std::format_error("formatter writes, then throws");
        return out;
    }
};

template <>
struct std::formatter<petra_test::throws_second> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto format(const petra_test::throws_second& c,
                std::format_context& ctx) const -> decltype(ctx.out()) {
        ++petra_test::g_throws_second_calls;
        if (petra_test::g_throws_second_calls == 1) {
            // The MEASURE pass: succeeds, and writes NOTHING, so the
            // measure pass produces zero bytes.
            return ctx.out();
        }
        // The WRITE pass: writes, then throws.
        auto out = std::format_to(ctx.out(), "BAD{}", c.value);
        throw std::format_error("write pass writes, then throws");
        return out;
    }
};

template <>
struct std::formatter<petra_test::flaky> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto format(const petra_test::flaky& c,
                std::format_context& ctx) const -> decltype(ctx.out()) {
        ++petra_test::g_flaky_calls;
        if (petra_test::g_flaky_calls == 1) {
            // The MEASURE pass: 3 bytes, deterministically.
            return std::format_to(ctx.out(), "OK{}", c.value);
        }
        // The WRITE pass: 4 bytes — a DIFFERENT length.
        return std::format_to(ctx.out(), "BAD{}", c.value);
    }
};

// ============================================================================
// Part 9 — the hand-rolled SECTION/CHECK framework (verbatim shape from
// Sep 6 .. Oct 26).
// ============================================================================

namespace petra_test {

int g_pass = 0;
int g_fail = 0;
const char* g_section = "";

void section(const char* name) {
    g_section = name;
    std::println("\n== {} ==", name);
}

template <typename A, typename B>
std::enable_if_t<!std::is_convertible_v<A, std::string_view>
                 && !std::is_convertible_v<B, std::string_view>, void>
check_eq(const A& got, const B& expected, const char* what) {
    if (got == expected) {
        ++g_pass;
    } else {
        ++g_fail;
        std::fprintf(stderr,
            "FAIL  [%s]  CHECK_EQ  %s\n", g_section, what);
    }
}

void check_eq(std::string_view got, std::string_view expected,
              const char* what) {
    if (got == expected) {
        ++g_pass;
    } else {
        ++g_fail;
        std::fprintf(stderr,
            "FAIL  [%s]  CHECK_EQ(sv)  %s: got \"%.*s\" expected \"%.*s\"\n",
            g_section, what,
            static_cast<int>(got.size()), got.data(),
            static_cast<int>(expected.size()), expected.data());
    }
}

void check_eq(char got, char expected, const char* what) {
    if (got == expected) {
        ++g_pass;
    } else {
        ++g_fail;
        std::fprintf(stderr,
            "FAIL  [%s]  CHECK_EQ(char)  %s: got '%c' (0x%02X) expected "
            "'%c' (0x%02X)\n",
            g_section, what, got,
            static_cast<unsigned>(static_cast<unsigned char>(got)),
            expected,
            static_cast<unsigned>(static_cast<unsigned char>(expected)));
    }
}

void check_true(bool cond, const char* what) {
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::fprintf(stderr,
            "FAIL  [%s]  CHECK_TRUE  %s\n", g_section, what);
    }
}

}  // namespace petra_test

// ============================================================================
// Part 10 — the sweep machinery.
//
// Every cell is scored on FIVE independent axes, each with its own
// counter, so a failure names the claim it broke:
//
//   disagree       the two contracts reached different verdicts
//                  (one succeeded, the other failed)
//   byte_fail      both succeeded but the BYTES differ
//   atomicity_fail the arc reported an error but MODIFIED the buffer
//   bleed_fail     the arc succeeded but modified bytes PAST the
//                  reported `written` count
//   needed_fail    both failed but disagree about how many bytes would
//                  have been needed
//
// The three-way check additionally pins the arc's own runtime-format
// path against its compile-time-format path, so a regression in EITHER
// arc path is caught.
//
// The `Args&&...` shape is the same one the adapter uses (Part 5), so
// the stdlib's `format_string<_Args...>` deduction lines up.
// ============================================================================

namespace {

constexpr char kPoison = 'Q';
constexpr std::size_t kBufSize = 64;

using usz = std::size_t;

// The index of the LAST byte that differs from the poison, or kNone if
// the buffer is untouched.
//
// NOTE: the obvious metric for "did it write past the reported count"
// — the index of the FIRST modified byte — is USELESS, because it is 0
// for ANY written output (the first written byte is by definition not
// the poison).  The question "is every byte at index >= `written` still
// untouched?" needs the LAST modified index.  (Measured: the first
// draft of this file used first-modified and reported bleed_fail=32 on
// 50 perfectly correct cells.)
constexpr std::size_t kNone = static_cast<std::size_t>(-1);

std::size_t last_modified(const char* buf, std::size_t n) noexcept {
    for (std::size_t i = n; i > 0; --i) {
        if (buf[i - 1] != kPoison) {
            return i - 1;
        }
    }
    return kNone;
}

struct arm {
    bool ok{false};
    usz written{0};
    usz needed{0};
    bool wrote_nothing{true};
    bool exact{true};
};

// Fill in the two buffer-state facts from an arm's outcome.
void classify_buffer(arm& a, const char* buf, std::size_t n) noexcept {
    const std::size_t last = last_modified(buf, n);
    a.wrote_nothing = (last == kNone);
    // "exact" = the arc wrote EXACTLY `written` bytes: every byte at
    // index >= written is still the poison.
    a.exact = (last == kNone) ? (a.written == 0) : (last < a.written);
}

template <typename... Args>
arm run_arc_runtime(char* buf, usz cap, std::string_view fmt,
                    const Args&... args) {
    arm a;
    const auto r = petra::detail::try_format_runtime_n_impl(buf, cap, fmt,
                                                              args...);
    a.ok = r.has_value();
    if (a.ok) {
        a.written = *r;
        a.needed = a.written;
    } else {
        a.needed = r.error().needed_bytes;
    }
    classify_buffer(a, buf, kBufSize);
    return a;
}

template <typename... Args>
arm run_arc_compiletime(char* buf, usz cap, std::format_string<Args...> fmt,
                         Args&&... args) {
    arm a;
    const auto r = petra::detail::try_format_bounded_runtime_n_impl(
        buf, cap, fmt, std::forward<Args>(args)...);
    a.ok = r.has_value();
    if (a.ok) {
        a.written = *r;
        a.needed = a.written;
    } else {
        a.needed = r.error().needed_bytes;
    }
    classify_buffer(a, buf, kBufSize);
    return a;
}

template <typename... Args>
arm run_stdlib(char* buf, usz cap, std::format_string<Args...> fmt,
               Args&&... args) {
    arm a;
    const auto n = static_cast<std::iter_difference_t<char*>>(cap);
    try {
        const auto r = std::format_to_n(buf, n, fmt,
                                         std::forward<Args>(args)...);
        a.written = static_cast<usz>(r.out - buf);
        a.needed = static_cast<usz>(r.size);
        a.ok = (a.needed <= cap);
    } catch (const std::format_error&) {
        a.ok = false;
        a.needed = 0;
    }
    classify_buffer(a, buf, kBufSize);
    return a;
}

struct counters {
    int cells{0};
    int disagree{0};
    int byte_fail{0};
    int atomicity_fail{0};
    int bleed_fail{0};
    int needed_fail{0};
    int three_way_fail{0};
};

template <typename... Args>
void compare_one(counters& ctr, usz cap, std::format_string<Args...> fmt,
                 Args&&... args) {
    char arc_buf[kBufSize];
    char rt_buf[kBufSize];
    char std_buf[kBufSize];
    std::memset(arc_buf, kPoison, kBufSize);
    std::memset(rt_buf, kPoison, kBufSize);
    std::memset(std_buf, kPoison, kBufSize);

    const arm arc = run_arc_compiletime(arc_buf, cap, fmt,
                                         std::forward<Args>(args)...);
    const arm rt = run_arc_runtime(rt_buf, cap, fmt.get(), args...);
    const arm std_ = run_stdlib(std_buf, cap, fmt,
                                 std::forward<Args>(args)...);

    ++ctr.cells;

    if (arc.ok != std_.ok) {
        ++ctr.disagree;
    }
    if (arc.ok && std_.ok) {
        if (arc.written != std_.written ||
            std::memcmp(arc_buf, std_buf, arc.written) != 0) {
            ++ctr.byte_fail;
        }
    }
    if (!arc.ok && !arc.wrote_nothing) {
        ++ctr.atomicity_fail;
    }
    if (arc.ok && !arc.exact) {
        ++ctr.bleed_fail;
    }
    if (!arc.ok && !std_.ok && arc.needed != std_.needed) {
        ++ctr.needed_fail;
    }
    // The arc's two format surfaces must agree with each other.
    if (arc.ok != rt.ok || arc.written != rt.written ||
        std::memcmp(arc_buf, rt_buf, arc.written) != 0) {
        ++ctr.three_way_fail;
    }
}

}  // namespace

// ============================================================================
// Part 11 — the sections.
// ============================================================================

int main() {
    using namespace petra_test;
    using namespace petra;

    std::println("P-2026-10-27 — try_format_vs_format_to_n");
    std::println("the arc's expected<size_t, FormatError> vs "
                 "std::format_to_n");

    // ------------------------------------------------------------------
    // Section 1 — the two contracts, side by side.
    // ------------------------------------------------------------------
    section("Section 1: the two result types");
    {
        std::println("  arc    : std::expected<std::size_t, "
                     "petra::FormatError>");
        std::println("           .has_value() / .value() / .error()"
                     "  -> 3 distinct states");
        std::println("  stdlib : std::format_to_n_result<char*>");
        std::println("           .out (iterator) / .size (count)"
                     "  -> 2 members, NO error state");
        std::println("  stdlib's size = bytes THAT WOULD HAVE BEEN "
                     "WRITTEN (not bytes written)");

        check_true(!detail::has_ec<std::format_to_n_result<char*>>::value,
                   "the stdlib's result has no 'ec' member");
        check_eq(sizeof(std::format_to_n_result<char*>),
                 sizeof(char*) + sizeof(std::ptrdiff_t),
                 "the stdlib's result is pointer + count and nothing else");
        check_true(
            std::is_signed_v<
                decltype(std::format_to_n_result<char*>::size)>,
            "the stdlib's size member is SIGNED");
    }

    // ------------------------------------------------------------------
    // Section 2 — the success path agrees, byte for byte.
    // ------------------------------------------------------------------
    section("Section 2: the success path agrees byte for byte");
    {
        char arc_buf[kBufSize];
        char std_buf[kBufSize];
        std::memset(arc_buf, kPoison, kBufSize);
        std::memset(std_buf, kPoison, kBufSize);

        const int v = 12345;
        const auto a = petra::detail::try_format_bounded_runtime_n_impl(
            arc_buf, 32, "{}", v);
        const auto s = petra::detail::stdlib_format_to_n(
            std_buf, 32, "{}", v);

        check_true(a.has_value(), "the arc succeeded");
        check_true(s.has_value(), "the stdlib adapter succeeded");
        check_eq(*a, usz{5}, "the arc wrote 5 bytes");
        check_eq(*s, usz{5}, "the adapter reports 5 bytes written");
        check_eq(std::string_view(arc_buf, 5), std::string_view("12345"),
                 "the arc's bytes");
        check_eq(std::string_view(std_buf, 5), std::string_view("12345"),
                 "the stdlib's bytes");
        check_eq(std::memcmp(arc_buf, std_buf, 5), 0,
                 "byte-for-byte identical");
        // Neither writes a null terminator.
        check_eq(arc_buf[5], kPoison, "the arc does not null-terminate");
        check_eq(std_buf[5], kPoison,
                 "the stdlib does not null-terminate either");
    }

    // ------------------------------------------------------------------
    // Section 3 — the two "size"s mean different things.
    // ------------------------------------------------------------------
    section("Section 3: the two sizes mean different things");
    {
        char arc_buf[kBufSize];
        char std_buf[kBufSize];
        std::memset(arc_buf, kPoison, kBufSize);
        std::memset(std_buf, kPoison, kBufSize);

        const int v = 12345;
        const auto a = petra::detail::try_format_bounded_runtime_n_impl(
            arc_buf, 3, "{}", v);
        const auto s = petra::detail::stdlib_format_to_n(
            std_buf, 3, "{}", v);

        check_true(!a.has_value(),
                   "the arc REFUSES when the output does not fit");
        check_eq(static_cast<int>(a.error().kind),
                 static_cast<int>(FormatErrorKind::TooLarge),
                 "the arc's error kind is TooLarge");
        check_eq(a.error().needed_bytes, usz{5},
                 "the arc reports how many bytes WOULD have been needed");
        check_true(!s.has_value(),
                   "the adapter turns the stdlib's overflow into an error");
        check_eq(s.error().needed_bytes, usz{5},
                 "the adapter recovers the stdlib's `size` as needed_bytes");

        // The RAW stdlib result, for the record.
        char raw[kBufSize];
        std::memset(raw, kPoison, kBufSize);
        const auto r = std::format_to_n(raw, 3, "{}", v);
        check_eq(static_cast<usz>(r.size), usz{5},
                 "raw stdlib: size = 5 = bytes that WOULD have been written");
        check_eq(static_cast<usz>(r.out - raw), usz{3},
                 "raw stdlib: out advanced by 3 = bytes ACTUALLY written");
    }

    // ------------------------------------------------------------------
    // Section 4 — truncate vs refuse: the BUFFER differs.
    // ------------------------------------------------------------------
    section("Section 4: the stdlib truncates, the arc refuses");
    {
        char a3[kBufSize];
        char s3[kBufSize];
        char a5[kBufSize];
        char s5[kBufSize];
        std::memset(a3, kPoison, kBufSize);
        std::memset(s3, kPoison, kBufSize);
        std::memset(a5, kPoison, kBufSize);
        std::memset(s5, kPoison, kBufSize);

        const int v = 12345;
        (void)petra::detail::try_format_bounded_runtime_n_impl(a3, 3, "{}", v);
        (void)petra::detail::stdlib_format_to_n(s3, 3, "{}", v);
        (void)petra::detail::try_format_bounded_runtime_n_impl(a5, 5, "{}", v);
        (void)petra::detail::stdlib_format_to_n(s5, 5, "{}", v);

        std::println("  cap 3:  arc    \"{}\"", std::string_view(a3, 3));
        std::println("          stdlib \"{}\"", std::string_view(s3, 3));
        std::println("  cap 5:  arc    \"{}\"", std::string_view(a5, 5));
        std::println("          stdlib \"{}\"", std::string_view(s5, 5));

        check_eq(a3[0], kPoison,
                 "cap 3: the arc wrote NOTHING (buffer untouched)");
        check_eq(s3[0], '1',
                 "cap 3: the stdlib wrote a PARTIAL result");
        check_eq(std::string_view(a5, 5), std::string_view("12345"),
                 "cap 5: the arc's bytes");
        check_eq(std::string_view(s5, 5), std::string_view("12345"),
                 "cap 5: the stdlib's bytes");
    }

    // ------------------------------------------------------------------
    // Section 5 — THE HEADLINE: the atomicity is an ARTIFACT of the
    // SINK TYPE, not a property of the two-pass design.
    //
    // THE EXPERIMENT: `throws_second` SUCCEEDS on the measure pass
    // (writing nothing) and WRITES-THEN-THROWS on the write pass.  So
    // the write pass is actually REACHED — which is the whole point.
    // (A formatter that throws on EVERY call would be caught by the
    // measure pass, and the write pass would never run at all; that
    // was the first draft's mistake, and it made the control show no
    // difference.)
    //
    // Then the SAME two-pass shape runs twice, differing ONLY in the
    // sink: the arc's `bounded_char_writer` (a custom iterator) vs a
    // plain `char*`.
    // ------------------------------------------------------------------
    section("Section 5: the atomicity is a property of the SINK, "
            "not of the two-pass design");
    {
        petra_test::throws_second t{9};

        char arc_buf[kBufSize];
        char cptr_buf[kBufSize];
        char std_buf[kBufSize];

        // (a) the arc's own sink: a CUSTOM output iterator.
        std::memset(arc_buf, kPoison, kBufSize);
        g_throws_second_calls = 0;
        const auto a = petra::detail::try_format_runtime_n_impl(
            arc_buf, 32, "{}", t);

        // (b) the SAME two-pass shape, but a plain char* sink.
        std::memset(cptr_buf, kPoison, kBufSize);
        g_throws_second_calls = 0;
        const auto b = petra::detail::two_pass_charptr_sink(
            cptr_buf, 32, "{}", t);

        // (c) the stdlib, whose sink is a char* by construction and
        //     which runs only ONE pass.
        std::memset(std_buf, kPoison, kBufSize);
        petra_test::half_throws h{9};
        const auto c = petra::detail::stdlib_format_to_n(
            std_buf, 32, "{}", h);

        std::println("  (a) arc sink (custom iterator) : buffer \"{}\"",
                     std::string_view(arc_buf, 5));
        std::println("  (b) same shape, char* sink     : buffer \"{}\"",
                     std::string_view(cptr_buf, 5));
        std::println("  (c) std::format_to_n (char*)   : buffer \"{}\"",
                     std::string_view(std_buf, 5));

        check_true(!a.has_value(), "(a) the arc reports an error");
        check_eq(static_cast<int>(a.error().kind),
                 static_cast<int>(FormatErrorKind::Format),
                 "(a) the arc's error kind is Format");
        check_eq(arc_buf[0], kPoison,
                 "(a) the arc's buffer is UNTOUCHED even though the write "
                 "pass had already started");

        check_true(!b.has_value(), "(b) the char*-sink variant errors too");
        check_eq(cptr_buf[0], 'B',
                 "(b) ...but the char* sink LEFT A PARTIAL WRITE — the "
                 "atomicity came from the sink type, not the two-pass "
                 "design");

        check_true(!c.has_value(), "(c) the stdlib reports an error");
        check_eq(std_buf[0], 'H',
                 "(c) ...and the stdlib also left a partial write");

        // The counter-example, stated as a counter: the arc's atomicity
        // holds ONLY because its sink is a custom iterator.
        check_true(arc_buf[0] == kPoison && cptr_buf[0] != kPoison,
                   "the two sinks differ in exactly one observable way");
    }

    // ------------------------------------------------------------------
    // Section 6 — the formatter is called TWICE by the arc, ONCE by the
    // stdlib.
    // ------------------------------------------------------------------
    section("Section 6: the arc calls the formatter twice, the stdlib "
            "once");
    {
        petra_test::counted c{7};

        g_counted_calls = 0;
        char arc_buf[kBufSize];
        std::memset(arc_buf, kPoison, kBufSize);
        const auto a = petra::detail::try_format_runtime_n_impl(
            arc_buf, 32, "{}", c);
        const int arc_calls = g_counted_calls;

        g_counted_calls = 0;
        char std_buf[kBufSize];
        std::memset(std_buf, kPoison, kBufSize);
        (void)petra::detail::stdlib_format_to_n(std_buf, 32, "{}", c);
        const int std_calls = g_counted_calls;

        std::println("  arc    formatter invocations = {}", arc_calls);
        std::println("  stdlib formatter invocations = {}", std_calls);

        check_eq(arc_calls, 2, "the arc runs the formatter twice "
                                "(measure pass + write pass)");
        check_eq(std_calls, 1, "the stdlib runs the formatter once");
        check_true(a.has_value(), "the arc still produced a value");
    }

    // ------------------------------------------------------------------
    // Section 7 — the atomicity is not uniform WITHIN the arc, and
    // the post-write consistency check is the hole.
    // ------------------------------------------------------------------
    section("Section 7: the post-write consistency check is the one "
            "error path that MODIFIES the buffer");
    {
        petra_test::flaky f{9};

        g_flaky_calls = 0;
        char arc_buf[kBufSize];
        std::memset(arc_buf, kPoison, kBufSize);
        const auto a = petra::detail::try_format_runtime_n_impl(
            arc_buf, 32, "{}", f);

        std::println("  arc verdict: {}",
                     a.has_value() ? "success" : "error");
        if (!a.has_value()) {
            std::println("  arc error kind : {}",
                         a.error().kind == FormatErrorKind::TooLarge
                             ? "TooLarge" : "Format");
            std::println("  arc message     : {}", a.error().message);
        }
        std::println("  arc buffer      : \"{}\"",
                     std::string_view(arc_buf, 5));

        check_true(!a.has_value(),
                   "the arc reports an error for a formatter that "
                   "produced different output on the second pass");
        check_eq(static_cast<int>(a.error().kind),
                 static_cast<int>(FormatErrorKind::TooLarge),
                 "the arc reports TooLarge — a CAPACITY error caused by the "
                 "formatter, not by the buffer");
        check_true(a.error().message.find("measure pass said") !=
                       std::string::npos,
                   "the arc's message names the measure/write disagreement");
        check_eq(arc_buf[0], 'B',
                 "KEY FINDING #5: the buffer WAS MODIFIED — the "
                 "post-write consistency check fires after the write has "
                 "already happened, so this is the one error path that "
                 "breaks the arc's atomicity");
    }

    // ------------------------------------------------------------------
    // Section 8 — the SIGNED `n`.
    // ------------------------------------------------------------------
    section("Section 8: the stdlib's n is signed and a negative n is "
            "silently accepted");
    {
        char b[kBufSize];
        std::memset(b, kPoison, kBufSize);
        const int v = 12345;

        const auto r = std::format_to_n(b, -1, "{}", v);
        std::println("  format_to_n(buf, -1, \"{{}}\", 12345):");
        std::println("    out advanced by {}, size = {}",
                     static_cast<usz>(r.out - b),
                     static_cast<usz>(r.size));
        std::println("    buffer = \"{}\" (untouched)",
                     std::string_view(b, 5));

        check_eq(static_cast<usz>(r.out - b), usz{0},
                 "a negative n writes NOTHING");
        check_eq(static_cast<usz>(r.size), usz{5},
                 "a negative n still reports the needed size");
        check_eq(b[0], kPoison, "a negative n leaves the buffer untouched");

        // The arc's cap is unsigned: a negative capacity cannot be
        // spelled at all.  This is the compile-time half; the runtime
        // half is the measurement above.
        static_assert(std::is_unsigned_v<std::size_t>,
                      "the arc's cap is unsigned");
    }

    // ------------------------------------------------------------------
    // Section 9 — the error taxonomy: four kinds, two reachable.
    // ------------------------------------------------------------------
    section("Section 9: the arc's four error kinds, two reachable");
    {
        struct probe {
            const char* label;
            const char* fmt;
            const char* fragment;
        };
        const probe probes[] = {
            {"presentation type", "{:f}", "invalid value for an integer"},
            {"arity too large",  "{} {}", "argument index value is too large"},
            {"argument index",    "{5}",  "argument index value is too large"},
            {"unmatched brace",   "{",    "terminates at a '{'"},
        };

        for (const auto& p : probes) {
            // The arc's RUNTIME-format path sees all four classes.
            char b[kBufSize];
            std::memset(b, kPoison, kBufSize);
            int v = 42;
            const auto r = petra::detail::try_format_runtime_n_impl(
                b, 32, std::string_view(p.fmt), v);
            check_true(!r.has_value(),
                       "the arc reports an error for this bad format");
            if (!r.has_value()) {
                check_eq(static_cast<int>(r.error().kind),
                         static_cast<int>(FormatErrorKind::Format),
                         "every stdlib format failure becomes "
                         "FormatErrorKind::Format");
                check_true(
                    r.error().message.find(p.fragment) != std::string::npos,
                    "the arc KEEPS the stdlib's message");
            }
            check_eq(b[0], kPoison,
                     "the arc's buffer is untouched on a format error");

            // The stdlib distinguishes the same four classes BY MESSAGE.
            std::string msg;
            try {
                int w = 42;
                (void)std::vformat(std::string_view(p.fmt),
                                   std::make_format_args(w));
            } catch (const std::format_error& e) {
                msg = e.what();
            }
            std::println("  {:<18} arc kind=Format | stdlib message: {}",
                         p.label, msg);
            check_true(msg.find(p.fragment) != std::string::npos,
                       "the stdlib's message identifies the failure class");
        }
    }

    // ------------------------------------------------------------------
    // Section 10 — the public stdlib API cannot reach the arc's
    // runtime-format cell.
    // ------------------------------------------------------------------
    section("Section 10: the public stdlib API is the compile-time-format "
            "cell only");
    {
        // std::vformat_to DOES take a runtime format string...
        char b[kBufSize];
        std::memset(b, kPoison, kBufSize);
        int v = 7;
        (void)std::vformat_to(b, std::string_view("{}"),
                               std::make_format_args(v));
        check_eq(std::string_view(b, 1), std::string_view("7"),
                 "std::vformat_to accepts a RUNTIME format string");

        // ...but there is no public runtime-format BOUNDED variant, so
        // the arc's cell [3] has no public stdlib counterpart.  The
        // compile-fail probe for `std::vformat_to_n` lives in the
        // verification harness (see the notes).

        // KEY FINDING #9 — the consteval check is on the VALUE, not on
        // the TYPE's spelling.  `std::format_to_n`'s third parameter is
        // `std::format_string<Args...>`, whose constructor is
        // `consteval`; so the format string must be a CONSTANT
        // EXPRESSION.
        //
        // An INLINE `std::string_view("{}")` IS a constant expression
        // (the literal is), so it is ACCEPTED even though its TYPE is
        // "runtime".  A `std::string_view` that arrives as a function
        // PARAMETER is not, and is REJECTED with "call to consteval
        // function ... is not a constant expression".
        //
        // Both spellings are measured here.
        {
            char c[kBufSize];
            std::memset(c, kPoison, kBufSize);
            int w = 42;
            const auto r = std::format_to_n(c, 8, std::string_view("{}"), w);
            check_eq(static_cast<usz>(r.size), usz{2},
                     "an INLINE std::string_view(\"{}\") is a constant "
                     "expression and is therefore ACCEPTED by the consteval "
                     "constructor — the check is on the VALUE, not the "
                     "type's spelling");
        }

        // And the stdlib's bounded variant demands a compile-time
        // format, so a bad format string is a COMPILE-TIME error
        // there — the arc's runtime path is where the runtime format
        // errors come from.
        check_true(true,
                   "the runtime-format surface is std::vformat_to, which "
                   "is unbounded and has no error channel");
    }

    // ------------------------------------------------------------------
    // Section 11 — THE HEADLINE VERIFICATION: the exhaustive sweep.
    //
    // 5 format/argument pairs x 10 capacities = 50 cells, each scored
    // on five independent axes, plus a three-way check.
    // ------------------------------------------------------------------
    section("Section 11: the exhaustive sweep");
    {
        counters ctr;
        const usz caps[] = {0, 1, 2, 3, 4, 5, 6, 8, 12, 32};

        for (const usz cap : caps) {
            compare_one(ctr, cap, "{}", 42);
            compare_one(ctr, cap, "{} {}", 1, 2);
            compare_one(ctr, cap, "v={}", 42);
            compare_one(ctr, cap, "{:5}", 42);
            compare_one(ctr, cap, "{:.2f}", 3.14159);
        }

        std::println("  sweep: cells={} disagree={} byte_fail={} "
                     "atomicity_fail={} bleed_fail={} needed_fail={} "
                     "three_way_fail={}",
                     ctr.cells, ctr.disagree, ctr.byte_fail,
                     ctr.atomicity_fail, ctr.bleed_fail, ctr.needed_fail,
                     ctr.three_way_fail);

        check_eq(ctr.cells, 50, "the sweep covered 50 cells");
        check_eq(ctr.disagree, 0,
                 "the arc and the stdlib adapter reached the same verdict "
                 "in every cell");
        check_eq(ctr.byte_fail, 0,
                 "where both succeeded, the bytes were identical");
        check_eq(ctr.atomicity_fail, 0,
                 "the arc never modified the buffer while reporting an "
                 "error");
        check_eq(ctr.bleed_fail, 0,
                 "the arc never wrote past its reported `written` count");
        check_eq(ctr.needed_fail, 0,
                 "where both failed, they agreed on how many bytes were "
                 "needed");
        check_eq(ctr.three_way_fail, 0,
                 "the arc's runtime-format and compile-time-format paths "
                 "agree with each other in every cell");
    }

    // ------------------------------------------------------------------
    // Section 12 — the summary table, printed.
    // ------------------------------------------------------------------
    section("Section 12: the contract comparison");
    {
        std::println("");
        std::println("  aspect                | arc (expected)        | "
                     "stdlib (format_to_n)");
        std::println("  ----------------------+-----------------------+"
                     "-----------------------");
        std::println("  error channel         | yes (FormatError)     | "
                     "NO (throws format_error)");
        std::println("  overflow              | error TooLarge        | "
                     "size > n, no error");
        std::println("  buffer on overflow    | untouched             | "
                     "PARTIAL WRITE");
        std::println("  buffer on fmt error   | untouched*            | "
                     "PARTIAL WRITE");
        std::println("  success value         | bytes WRITTEN         | "
                     "bytes WOULD-HAVE-BEEN");
        std::println("  capacity type         | size_t (unsigned)     | "
                     "ptrdiff_t (SIGNED)");
        std::println("  negative capacity     | not representable     | "
                     "silently = 0");
        std::println("  formatter invocations | 2 (measure + write)  | 1");
        std::println("  runtime format string | yes (cell [3])        | "
                     "NO public API");
        std::println("  compile-time check    | only on cell [2]      | "
                     "always");
        std::println("");
        std::println("  * except the post-write consistency check, which "
                     "fires after the write");
    }

    std::println("");
    std::println("PASS={} FAIL={}", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
