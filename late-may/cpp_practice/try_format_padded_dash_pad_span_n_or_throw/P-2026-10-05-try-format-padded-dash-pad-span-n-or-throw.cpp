// P-2026-10-05 — try_format_padded_dash_pad_span_n_or_throw:
//
// Closes Oct 4's "Where we go next" item (1) verbatim — the
// PETRA_PAD = 1 × PETRA_DASH_PAD = 1 cells on the STD::SPAN<CHAR>
// × BOUNDED-BUFFER axis (the SPAN<CHAR> MIRROR of Sep 30's PAD ×
// SPACE_PAD composition lesson, but for the DASH axis).  TODAY
// extends the SEVENTH axis (PETRA_DASH_PAD) to the NUL-padded
// sibling (FIFTH axis) on the std::span<char> side.
//
// The new compile-time flag pair is (PETRA_PAD, PETRA_DASH_PAD) =
// (1, 1).  When BOTH are set, the natural composition rule is:
// "the LATER fill overwrites the EARLIER fill" — so the DASH fill
// (axis 7) overwrites the NUL fill (axis 5), and bytes [written,
// span.size()) end up as '-' on success.
//
// The compile-time flag 4-tuple (PETRA_THROW_ON_OVERFLOW,
// PETRA_PAD, PETRA_SPACE_PAD, PETRA_DASH_PAD) controls the
// dispatch via #if PETRA_THROW_ON_OVERFLOW × #if PETRA_PAD × #if
// PETRA_SPACE_PAD × #if PETRA_DASH_PAD.  The (0, 1, 0, 1) and
// (1, 1, 0, 1) cells REUSE the (0, 0, 0, 1) and (1, 0, 0, 1)
// DASH-padded span<char> sibling wrappers verbatim (DASH
// overwrites NUL when both PAD and DASH_PAD are set).  The
// (0, 1, 1, 1) and (1, 1, 1, 1) cells also REUSE the (0, 0, 0, 1)
// and (1, 0, 0, 1) DASH-padded span<char> sibling wrappers
// verbatim (DASH overwrites SPACE overwrites NUL when all three
// are set, by the transitive composition rule).
//
// The 16 cells of the 4-tuple are (showing the dispatch wrapper
// each cell resolves to):
//
//   - (0, 0, 0, 0): expected+untouched sibling (returns
//     expected<size_t, FormatError>; bytes [written, span.size())
//     UNTOUCHED on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_span (Sep 10)
//      RUNTIME:   petra::try_format_runtime_n(span, ...) (Sep 19)
//
//   - (0, 1, 0, 0): expected+NUL-padded sibling (returns
//     expected<size_t, FormatError>; bytes [written, span.size())
//     zero-filled to '\0' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_padded_span
//                (Sep 27)
//      RUNTIME:   petra::try_format_runtime_n_padded_span (Sep 27)
//
//   - (0, 0, 1, 0): expected+SPACE-padded sibling (returns
//     expected<size_t, FormatError>; bytes [written, span.size())
//     filled with ' ' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_space_pad_span
//                (Sep 28)
//      RUNTIME:   petra::try_format_runtime_n_space_pad_span (Sep 28)
//
//   - (0, 0, 0, 1): expected+DASH-padded sibling (returns
//     expected<size_t, FormatError>; bytes [written, span.size())
//     filled with '-' on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_dash_pad_span
//                (Oct 4)
//      RUNTIME:   petra::try_format_runtime_n_dash_pad_span (Oct 4)
//
//   - (0, 1, 0, 1): expected+NUL+DASH-padded sibling (NEW today;
//     returns expected<size_t, FormatError>; bytes [written,
//     span.size()) filled with '-' on success — the DASH fill
//     OVERWRITES the NUL fill):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_dash_pad_span
//                (Oct 4, REUSED verbatim; the (0, 0, 0, 1) and
//                (0, 1, 0, 1) cells share the SAME sibling wrapper
//                because DASH overwrites NUL when both are set).
//      RUNTIME:   petra::try_format_runtime_n_dash_pad_span (Oct 4,
//                REUSED verbatim).
//
//   - (0, 1, 1, 0): expected+NUL+SPACE-padded sibling (REUSES
//     Sep 28's (0, 0, 1, 0) SPACE-padded wrapper verbatim per
//     the composition rule "SPACE overwrites NUL when both are
//     set"):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_space_pad_span
//                (Sep 28, REUSED verbatim).
//      RUNTIME:   petra::try_format_runtime_n_space_pad_span (Sep
//                28, REUSED verbatim).
//
//   - (0, 0, 1, 1): expected+SPACE+DASH-padded sibling (REUSES
//     Oct 4's (0, 0, 0, 1) DASH-padded wrapper verbatim per
//     the composition rule "DASH overwrites SPACE when both are
//     set"):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_dash_pad_span
//                (Oct 4, REUSED verbatim).
//      RUNTIME:   petra::try_format_runtime_n_dash_pad_span (Oct 4,
//                REUSED verbatim).
//
//   - (0, 1, 1, 1): expected+NUL+SPACE+DASH-padded sibling
//     (NEW today; returns expected<size_t, FormatError>; bytes
//     [written, span.size()) filled with '-' on success — the
//     DASH fill OVERWRITES the SPACE fill which OVERWROTE the
//     NUL fill):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_dash_pad_span
//                (Oct 4, REUSED verbatim; the (0, 0, 0, 1),
//                (0, 0, 1, 1), (0, 1, 0, 1), and (0, 1, 1, 1)
//                cells share the SAME sibling wrapper per the
//                transitive composition rule).
//      RUNTIME:   petra::try_format_runtime_n_dash_pad_span (Oct
//                4, REUSED verbatim).
//
//   - (1, 0, 0, 0): throwing+untouched sibling (returns
//     std::size_t on success, throws std::runtime_error on
//     overflow; bytes [written, span.size()) UNTOUCHED on
//     success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_span
//                (Sep 21)
//      RUNTIME:   petra::try_format_runtime_n_throwing(span, ...)
//                (Sep 21)
//
//   - (1, 1, 0, 0): throwing+NUL-padded sibling (returns
//     std::size_t on success, throws std::runtime_error on
//     overflow; bytes [written, span.size()) zero-filled to '\0'
//     on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_padded_span
//                (Sep 27)
//      RUNTIME:   petra::try_format_runtime_n_throwing_padded_span
//                (Sep 27)
//
//   - (1, 0, 1, 0): throwing+SPACE-padded sibling (Sep 28;
//     returns std::size_t on success, throws std::runtime_error
//     on overflow; bytes [written, span.size()) filled with ' '
//     on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_space_pad_span
//                (Sep 28)
//      RUNTIME:   petra::try_format_runtime_n_throwing_space_pad_span
//                (Sep 28)
//
//   - (1, 0, 0, 1): throwing+DASH-padded sibling (Oct 4;
//     returns std::size_t on success, throws std::runtime_error
//     on overflow; bytes [written, span.size()) filled with '-'
//     on success):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_dash_pad_span
//                (Oct 4)
//      RUNTIME:   petra::try_format_runtime_n_throwing_dash_pad_span
//                (Oct 4)
//
//   - (1, 1, 0, 1): throwing+NUL+DASH-padded sibling (NEW today;
//     returns std::size_t on success, throws std::runtime_error
//     on overflow; bytes [written, span.size()) filled with '-'
//     on success — the DASH fill OVERWRITES the NUL fill):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_dash_pad_span
//                (Oct 4, REUSED verbatim).
//      RUNTIME:   petra::try_format_runtime_n_throwing_dash_pad_span
//                (Oct 4, REUSED verbatim).
//
//   - (1, 1, 1, 0): throwing+NUL+SPACE-padded sibling (REUSES
//     Sep 28's (1, 0, 1, 0) SPACE-padded wrapper verbatim):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_space_pad_span
//                (Sep 28, REUSED verbatim).
//      RUNTIME:   petra::try_format_runtime_n_throwing_space_pad_span
//                (Sep 28, REUSED verbatim).
//
//   - (1, 0, 1, 1): throwing+SPACE+DASH-padded sibling (REUSES
//     Oct 4's (1, 0, 0, 1) DASH-padded wrapper verbatim):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_dash_pad_span
//                (Oct 4, REUSED verbatim).
//      RUNTIME:   petra::try_format_runtime_n_throwing_dash_pad_span
//                (Oct 4, REUSED verbatim).
//
//   - (1, 1, 1, 1): throwing+NUL+SPACE+DASH-padded sibling
//     (NEW today; returns std::size_t on success, throws
//     std::runtime_error on overflow; bytes [written,
//     span.size()) filled with '-' on success — the DASH fill
//     OVERWRITES the SPACE fill which OVERWROTE the NUL fill):
//      CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_dash_pad_span
//                (Oct 4, REUSED verbatim; the (1, 0, 0, 1),
//                (1, 0, 1, 1), (1, 1, 0, 1), and (1, 1, 1, 1)
//                cells share the SAME sibling wrapper per the
//                transitive composition rule).
//      RUNTIME:   petra::try_format_runtime_n_throwing_dash_pad_span
//                (Oct 4, REUSED verbatim).
//
// Defaults: PETRA_THROW_ON_OVERFLOW = 0, PETRA_PAD = 0,
// PETRA_SPACE_PAD = 0, PETRA_DASH_PAD = 0 (expected+untouched —
// the prior contract across Sep 6 through Oct 4).
//
// KEY DESIGN DECISION — the (0, 1, 0, 1), (0, 1, 1, 1), (1, 1, 0,
// 1), and (1, 1, 1, 1) cells REUSE the (0, 0, 0, 1) and (1, 0, 0,
// 1) DASH-padded span<char> sibling wrappers verbatim because
// DASH overwrites NUL when both PAD and DASH_PAD are set (and
// transitively DASH overwrites SPACE overwrites NUL when all
// three are set).  No NEW wrappers are needed for these
// composition cells; they just dispatch to Oct 4's DASH-padded
// siblings.  Likewise, the (0, 1, 1, 0) and (1, 1, 1, 0) cells
// REUSE the (0, 0, 1, 0) and (1, 0, 1, 0) SPACE-padded span<char>
// sibling wrappers verbatim (Oct 1's composition rule).
//
// SCOPE NOTE: This lesson is FOCUSED on the PAD × DASH_PAD
// composition ONLY on the std::span<char> side.  Oct 4's
// PETRA_PAD = 0 branches require Oct 4's NEW DASH-pad wrappers
// (which ARE included in this file verbatim).  When PETRA_PAD = 0
// AND PETRA_DASH_PAD = 0, the lesson's dispatcher works exactly
// like Oct 4's default (untouched) sibling.  When PETRA_PAD = 0
// AND PETRA_DASH_PAD = 1, the lesson's dispatcher works exactly
// like Oct 4's (PAD=0, DASH_PAD=1) sibling.  When PETRA_PAD = 1
// AND PETRA_DASH_PAD = 0, the lesson's dispatcher works exactly
// like Sep 27's (PAD=1, DASH_PAD=0) sibling on the std::span<char>
// side.

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <expected>
#include <format>
#include <iterator>
#include <print>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

// ============================================================================
// Part 1 — petra::FormatError (verbatim from Sep 6 / Sep 7 / Sep 8 / Sep 9 /
// Sep 10 / Sep 11 / Sep 12 / Sep 13 / Sep 14 / Sep 15 / Sep 16 / Sep 19 /
// Sep 20 / Sep 21 / Sep 22 / Sep 23 / Sep 25 / Sep 26 / Sep 27 / Sep 28 /
// Sep 29 / Sep 30 / Oct 1 / Oct 3 / Oct 4)
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
// Part 2 — petra::counting_output_iterator (verbatim from Sep 6 onwards;
// callback-based per Sep 30 / Oct 1 / Oct 3 / Oct 4 fix)
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

    std::size_t count() const noexcept { return count_; }

private:
    std::size_t count_{0};
    callback_type cb_;
    void* data_;
};

static_assert(std::indirectly_writable<counting_output_iterator, const char&>,
              "petra::counting_output_iterator must satisfy "
              "std::indirectly_writable for const char&");
static_assert(std::output_iterator<counting_output_iterator, const char&>,
              "petra::counting_output_iterator must satisfy "
              "std::output_iterator for const char&");

}  // namespace petra

// ============================================================================
// Part 3 — petra::detail::bounded_char_writer (verbatim from Sep 8 onwards;
// callback-based per Sep 30 / Oct 1 / Oct 3 / Oct 4 fix)
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

static_assert(std::indirectly_writable<bounded_char_writer, const char&>,
              "petra::bounded_char_writer must satisfy "
              "std::indirectly_writable for const char&");
static_assert(std::output_iterator<bounded_char_writer, const char&>,
              "petra::bounded_char_writer must satisfy "
              "std::output_iterator for const char&");

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 4 — petra::detail::try_format_runtime_n_impl (Sep 8 verbatim) +
// petra::detail::try_format_bounded_runtime_n_impl (Sep 9 verbatim).
// ============================================================================

namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_impl(char* out, std::size_t cap,
                           std::string_view fmt,
                           const Args&... args) {
    // Pass 1: measure the would-be byte count.
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

    // needed > cap: TooLarge.
    if (needed > cap) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"formatted output would be "}
                            + std::to_string(needed)
                            + " bytes; cap is " + std::to_string(cap),
                        needed});
    }

    // Pass 2: write into out[0, cap).
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
                                   std::format_string<Args...> fmt,
                                   const Args&... args) {
    // Same shape as the RUNTIME impl above but takes std::format_string.
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };
    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt.get(),
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
        (void)std::vformat_to(std::move(sink), fmt.get(),
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
// Part 5 — petra::try_format_runtime_n (Sep 8 verbatim char*+cap) +
// petra::try_format_bounded_runtime_n (Sep 9 verbatim char*+cap) +
// petra::try_format_runtime_n(span<char>) (Sep 19 verbatim) +
// petra::try_format_bounded_runtime_n_span (Sep 10 verbatim).
//
// These are the expected+untouched siblings on the CHAR*+CAP axis AND
// the STD::SPAN<CHAR> axis.  TODAY's NEW (composition) wrappers
// REUSE the OCT 4 DASH-padded siblings verbatim; OCT 4's DASH-
// padded siblings in turn compose on top of these untouched
// siblings.
// ============================================================================

namespace petra {

// Sep 8 verbatim — char*+cap RUNTIME bounded-buffer wrapper.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(char* out, std::size_t cap, std::string_view fmt,
                     const Args&... args) {
    return detail::try_format_runtime_n_impl(out, cap, fmt, args...);
}

// Sep 9 verbatim — char*+cap CONSTEVAL bounded-buffer wrapper.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n(char* out, std::size_t cap,
                              std::format_string<Args...> fmt,
                              const Args&... args) {
    return detail::try_format_bounded_runtime_n_impl(out, cap, fmt, args...);
}

// Sep 19 verbatim — span<char> RUNTIME bounded-buffer wrapper.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n(std::span<char> out, std::string_view fmt,
                     const Args&... args) {
    return try_format_runtime_n(out.data(), out.size(), fmt, args...);
}

// Sep 10 verbatim — span<char> CONSTEVAL bounded-buffer wrapper.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_span(std::span<char> out,
                                   std::format_string<Args...> fmt,
                                   const Args&... args) {
    return try_format_bounded_runtime_n(out.data(), out.size(), fmt,
                                         args...);
}

}  // namespace petra

// ============================================================================
// Part 6 — petra::try_format_runtime_n_throwing(span<char>) (Sep 21
// verbatim) + petra::try_format_bounded_runtime_n_throwing_span (Sep 21
// verbatim).
//
// These are the throwing+untouched siblings on the STD::SPAN<CHAR>
// axis.  SEP 27's NEW throwing+padded span<char> wrappers compose on
// top of THESE wrappers; SEP 28's NEW throwing+SPACE-padded span<char>
// wrappers also compose on top of THESE wrappers; OCT 4's NEW
// throwing+DASH-padded span<char> wrappers also compose on top of
// SEP 28's; TODAY's (1, 1, 0, 1) and (1, 1, 1, 1) dispatch cells
// reuse OCT 4's DASH-padded siblings.
// ============================================================================

namespace petra {

// Sep 21 verbatim — span<char> RUNTIME throwing wrapper.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing(std::span<char> out, std::string_view fmt,
                              const Args&... args) {
    auto r = try_format_runtime_n(out, fmt, args...);
    if (r.has_value()) {
        return r.value();
    }
    throw std::runtime_error(r.error().message);
}

// Sep 21 verbatim — span<char> CONSTEVAL throwing wrapper.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_span(
    std::span<char> out, std::format_string<Args...> fmt,
    const Args&... args) {
    auto r = try_format_bounded_runtime_n_span(out, fmt, args...);
    if (r.has_value()) {
        return r.value();
    }
    throw std::runtime_error(r.error().message);
}

}  // namespace petra

// ============================================================================
// Part 7 — petra::try_format_bounded_runtime_n_padded_span /
// petra::try_format_runtime_n_padded_span /
// petra::try_format_bounded_runtime_n_throwing_padded_span /
// petra::try_format_runtime_n_throwing_padded_span — SEP 27 verbatim.
// These are the NUL-padded span<char> siblings.  TODAY's (0, 1, 0,
// 0) and (1, 1, 0, 0) cells use these; TODAY's (0, 1, 0, 1) and
// (1, 1, 0, 1) cells REUSE OCT 4's DASH-padded siblings per the
// composition rule.
// ============================================================================

namespace petra {

// Sep 27 verbatim — RUNTIME surface of the BOUNDED-BUFFER × EXPECTED ×
// PADDED sibling on the span<char> axis.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_padded_span(std::span<char> out, std::string_view fmt,
                                 const Args&... args) {
    auto r = try_format_runtime_n(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '\0';
        }
    }
    return r;
}

// Sep 27 verbatim — CONSTEVAL surface of the BOUNDED-BUFFER × EXPECTED ×
// PADDED sibling on the span<char> axis.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_padded_span(std::span<char> out,
                                         std::format_string<Args...> fmt,
                                         const Args&... args) {
    auto r = try_format_bounded_runtime_n_span(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '\0';
        }
    }
    return r;
}

// Sep 27 verbatim — RUNTIME surface of the BOUNDED-BUFFER × THROWING ×
// PADDED sibling on the span<char> axis.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_padded_span(std::span<char> out,
                                          std::string_view fmt,
                                          const Args&... args) {
    try {
        const std::size_t n =
            try_format_runtime_n_throwing(out, fmt, args...);
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '\0';
        }
        return n;
    } catch (...) {
        throw;
    }
}

// Sep 27 verbatim — CONSTEVAL surface of the BOUNDED-BUFFER × THROWING ×
// PADDED sibling on the span<char> axis.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_padded_span(
    std::span<char> out, std::format_string<Args...> fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_bounded_runtime_n_throwing_span(out, fmt, args...);
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '\0';
        }
        return n;
    } catch (...) {
        throw;
    }
}

}  // namespace petra

// ============================================================================
// Part 8 — petra::try_format_bounded_runtime_n_space_pad_span /
// petra::try_format_runtime_n_space_pad_span /
// petra::try_format_bounded_runtime_n_throwing_space_pad_span /
// petra::try_format_runtime_n_throwing_space_pad_span — SEP 28 verbatim.
//
// KEY INSIGHT: the (0, 1, 1, 0) and (1, 1, 1, 0) cells REUSE these
// wrappers verbatim because SPACE overwrites NUL when both PAD and
// SPACE_PAD are set (Oct 1's composition rule).  See Section 1's
// "PAD × SPACE_PAD composition rule" pinning below.
// ============================================================================

namespace petra {

// Sep 28 verbatim — RUNTIME surface of the BOUNDED-BUFFER × EXPECTED ×
// SPACE-PAD sibling on the span<char> axis.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_space_pad_span(std::span<char> out, std::string_view fmt,
                                    const Args&... args) {
    auto r = try_format_runtime_n(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = ' ';
        }
    }
    return r;
}

// Sep 28 verbatim — CONSTEVAL surface of the BOUNDED-BUFFER × EXPECTED ×
// SPACE-PAD sibling on the span<char> axis.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_space_pad_span(
    std::span<char> out, std::format_string<Args...> fmt,
    const Args&... args) {
    auto r = try_format_bounded_runtime_n_span(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = ' ';
        }
    }
    return r;
}

// Sep 28 verbatim — RUNTIME surface of the BOUNDED-BUFFER × THROWING ×
// SPACE-PAD sibling on the span<char> axis.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_space_pad_span(
    std::span<char> out, std::string_view fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_runtime_n_throwing(out, fmt, args...);
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = ' ';
        }
        return n;
    } catch (...) {
        throw;
    }
}

// Sep 28 verbatim — CONSTEVAL surface of the BOUNDED-BUFFER × THROWING ×
// SPACE-PAD sibling on the span<char> axis.
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_space_pad_span(
    std::span<char> out, std::format_string<Args...> fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_bounded_runtime_n_throwing_span(out, fmt, args...);
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = ' ';
        }
        return n;
    } catch (...) {
        throw;
    }
}

}  // namespace petra

// ============================================================================
// Part 9 — petra::try_format_bounded_runtime_n_dash_pad_span /
// petra::try_format_runtime_n_dash_pad_span /
// petra::try_format_bounded_runtime_n_throwing_dash_pad_span /
// petra::try_format_runtime_n_throwing_dash_pad_span — OCT 4 verbatim.
//
// KEY INSIGHT (TODAY): the (0, 1, 0, 1), (0, 1, 1, 1), (1, 1, 0, 1),
// and (1, 1, 1, 1) cells REUSE these wrappers verbatim because
// DASH overwrites NUL when both PAD and DASH_PAD are set (and
// transitively DASH overwrites SPACE overwrites NUL when all
// three are set).  See Section 1's "PAD × DASH_PAD composition
// rule" pinning below.
// ============================================================================

namespace petra {

// Oct 4 verbatim — RUNTIME surface of the BOUNDED-BUFFER × EXPECTED ×
// DASH-PAD sibling on the span<char> axis.  Wraps Sep 28's
// try_format_runtime_n_space_pad_span verbatim and dash-fills
// [written, span.size()) with '-' on success ONLY.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_dash_pad_span(std::span<char> out, std::string_view fmt,
                                    const Args&... args) {
    auto r = try_format_runtime_n_space_pad_span(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '-';
        }
    }
    return r;
}

// Oct 4 verbatim — CONSTEVAL surface of the BOUNDED-BUFFER × EXPECTED ×
// DASH-PAD sibling on the span<char> axis.  Wraps Sep 28's
// try_format_bounded_runtime_n_space_pad_span verbatim and
// dash-fills [written, span.size()) with '-' on success ONLY.
template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_bounded_runtime_n_dash_pad_span(
    std::span<char> out, std::format_string<Args...> fmt,
    const Args&... args) {
    auto r = try_format_bounded_runtime_n_space_pad_span(out, fmt, args...);
    if (r.has_value()) {
        const std::size_t written = r.value();
        const std::size_t cap = out.size();
        for (std::size_t i = written; i < cap; ++i) {
            out[i] = '-';
        }
    }
    return r;
}

// Oct 4 verbatim — RUNTIME surface of the BOUNDED-BUFFER × THROWING ×
// DASH-PAD sibling on the span<char> axis.  Wraps Sep 28's
// try_format_runtime_n_throwing_space_pad_span verbatim and
// dash-fills [written, span.size()) with '-' on success ONLY
// (control does NOT reach the dash-fill loop on error — the catch
// block re-raises the std::runtime_error untouched).
template <typename... Args>
[[nodiscard]] std::size_t
try_format_runtime_n_throwing_dash_pad_span(
    std::span<char> out, std::string_view fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_runtime_n_throwing_space_pad_span(out, fmt, args...);
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '-';
        }
        return n;
    } catch (...) {
        throw;
    }
}

// Oct 4 verbatim — CONSTEVAL surface of the BOUNDED-BUFFER × THROWING ×
// DASH-PAD sibling on the span<char> axis.  Wraps Sep 28's
// try_format_bounded_runtime_n_throwing_space_pad_span verbatim
// and dash-fills [written, span.size()) with '-' on success ONLY
// (control does NOT reach the dash-fill loop on error — the catch
// block re-raises the std::runtime_error untouched).
template <typename... Args>
[[nodiscard]] std::size_t
try_format_bounded_runtime_n_throwing_dash_pad_span(
    std::span<char> out, std::format_string<Args...> fmt,
    const Args&... args) {
    try {
        const std::size_t n =
            try_format_bounded_runtime_n_throwing_space_pad_span(
                out, fmt, args...);
        const std::size_t cap = out.size();
        for (std::size_t i = n; i < cap; ++i) {
            out[i] = '-';
        }
        return n;
    } catch (...) {
        throw;
    }
}

}  // namespace petra

// ============================================================================
// Part 10 — TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW /
//          TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW
// (Mirrors Oct 4's shape verbatim; now extended to cover ALL 16 cells
//  on the 4-bit flag tuple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
//  PETRA_SPACE_PAD, PETRA_DASH_PAD).)
//
// The compile-time flag 4-tuple controls the dispatch via
//   #if PETRA_THROW_ON_OVERFLOW × #if PETRA_PAD × #if PETRA_SPACE_PAD
//   × #if PETRA_DASH_PAD:
//
//   (0, 0, 0, 0): expected+untouched sibling (returns expected<size_t,
//            FormatError>; bytes [written, span.size()) UNTOUCHED on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_span (Sep 10)
//             RUNTIME:   petra::try_format_runtime_n(span, ...) (Sep 19)
//
//   (0, 1, 0, 0): expected+NUL-padded sibling (returns expected<size_t,
//            FormatError>; bytes [written, span.size()) zero-filled to '\0'
//            on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_padded_span
//                       (Sep 27)
//             RUNTIME:   petra::try_format_runtime_n_padded_span (Sep 27)
//
//   (0, 0, 1, 0): expected+SPACE-padded sibling (returns expected<size_t,
//            FormatError>; bytes [written, span.size()) filled with ' '
//            on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_space_pad_span
//                       (Sep 28)
//             RUNTIME:   petra::try_format_runtime_n_space_pad_span (Sep 28)
//
//   (0, 0, 0, 1): expected+DASH-padded sibling (returns expected<size_t,
//            FormatError>; bytes [written, span.size()) filled with '-'
//            on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_dash_pad_span
//                       (Oct 4)
//             RUNTIME:   petra::try_format_runtime_n_dash_pad_span (Oct 4)
//
//   (0, 1, 0, 1): expected+NUL+DASH-padded sibling (NEW today — REUSES
//            the (0, 0, 0, 1) DASH-padded sibling wrappers verbatim
//            because DASH overwrites NUL when both are set; bytes
//            [written, span.size()) end up as '-' on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_dash_pad_span
//                       (Oct 4 — REUSED verbatim; the (0, 0, 0, 1) and
//                       (0, 1, 0, 1) cells share the SAME sibling wrapper)
//             RUNTIME:   petra::try_format_runtime_n_dash_pad_span (Oct 4
//                       — REUSED verbatim)
//
//   (0, 1, 1, 0): expected+NUL+SPACE-padded sibling (REUSES the
//            (0, 0, 1, 0) SPACE-padded sibling wrappers verbatim
//            because SPACE overwrites NUL when both are set per
//            Oct 1's composition rule):
//             CONSTEVAL: petra::try_format_bounded_runtime_n_space_pad_span
//                       (Sep 28 — REUSED verbatim; the (0, 0, 1, 0) and
//                       (0, 1, 1, 0) cells share the SAME sibling wrapper)
//             RUNTIME:   petra::try_format_runtime_n_space_pad_span (Sep
//                       28 — REUSED verbatim)
//
//   (0, 0, 1, 1): expected+SPACE+DASH-padded sibling (REUSES the
//            (0, 0, 0, 1) DASH-padded sibling wrappers verbatim
//            because DASH overwrites SPACE when both are set):
//             CONSTEVAL: petra::try_format_bounded_runtime_n_dash_pad_span
//                       (Oct 4 — REUSED verbatim)
//             RUNTIME:   petra::try_format_runtime_n_dash_pad_span (Oct 4
//                       — REUSED verbatim)
//
//   (0, 1, 1, 1): expected+NUL+SPACE+DASH-padded sibling (NEW today
//            — REUSES the (0, 0, 0, 1) DASH-padded sibling wrappers
//            verbatim because DASH overwrites SPACE overwrites NUL
//            when all three are set, per the transitive composition
//            rule):
//             CONSTEVAL: petra::try_format_bounded_runtime_n_dash_pad_span
//                       (Oct 4 — REUSED verbatim; the (0, 0, 0, 1),
//                       (0, 0, 1, 1), (0, 1, 0, 1), and (0, 1, 1, 1)
//                       cells share the SAME sibling wrapper)
//             RUNTIME:   petra::try_format_runtime_n_dash_pad_span (Oct 4
//                       — REUSED verbatim)
//
//   (1, 0, 0, 0): throwing+untouched sibling (returns std::size_t on
//            success, throws std::runtime_error on overflow; bytes
//            [written, span.size()) UNTOUCHED on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_span
//                       (Sep 21)
//             RUNTIME:   petra::try_format_runtime_n_throwing(span, ...)
//                       (Sep 21)
//
//   (1, 1, 0, 0): throwing+NUL-padded sibling (returns std::size_t on
//            success, throws std::runtime_error on overflow; bytes
//            [written, span.size()) zero-filled to '\0' on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_padded_span
//                       (Sep 27)
//             RUNTIME:   petra::try_format_runtime_n_throwing_padded_span
//                       (Sep 27)
//
//   (1, 0, 1, 0): throwing+SPACE-padded sibling (returns std::size_t on
//            success, throws std::runtime_error on overflow; bytes
//            [written, span.size()) filled with ' ' on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_space_pad_span
//                       (Sep 28)
//             RUNTIME:   petra::try_format_runtime_n_throwing_space_pad_span
//                       (Sep 28)
//
//   (1, 0, 0, 1): throwing+DASH-padded sibling (returns std::size_t on
//            success, throws std::runtime_error on overflow; bytes
//            [written, span.size()) filled with '-' on success)
//             CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_dash_pad_span
//                       (Oct 4)
//             RUNTIME:   petra::try_format_runtime_n_throwing_dash_pad_span
//                       (Oct 4)
//
//   (1, 1, 0, 1): throwing+NUL+DASH-padded sibling (NEW today —
//            REUSES the (1, 0, 0, 1) DASH-padded sibling wrappers
//            verbatim because DASH overwrites NUL when both are
//            set):
//             CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_dash_pad_span
//                       (Oct 4 — REUSED verbatim)
//             RUNTIME:   petra::try_format_runtime_n_throwing_dash_pad_span
//                       (Oct 4 — REUSED verbatim)
//
//   (1, 1, 1, 0): throwing+NUL+SPACE-padded sibling (REUSES the
//            (1, 0, 1, 0) SPACE-padded sibling wrappers verbatim):
//             CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_space_pad_span
//                       (Sep 28 — REUSED verbatim)
//             RUNTIME:   petra::try_format_runtime_n_throwing_space_pad_span
//                       (Sep 28 — REUSED verbatim)
//
//   (1, 0, 1, 1): throwing+SPACE+DASH-padded sibling (REUSES the
//            (1, 0, 0, 1) DASH-padded sibling wrappers verbatim):
//             CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_dash_pad_span
//                       (Oct 4 — REUSED verbatim)
//             RUNTIME:   petra::try_format_runtime_n_throwing_dash_pad_span
//                       (Oct 4 — REUSED verbatim)
//
//   (1, 1, 1, 1): throwing+NUL+SPACE+DASH-padded sibling (NEW today
//            — REUSES the (1, 0, 0, 1) DASH-padded sibling wrappers
//            verbatim):
//             CONSTEVAL: petra::try_format_bounded_runtime_n_throwing_dash_pad_span
//                       (Oct 4 — REUSED verbatim; the (1, 0, 0, 1),
//                       (1, 0, 1, 1), (1, 1, 0, 1), and (1, 1, 1, 1)
//                       cells share the SAME sibling wrapper)
//             RUNTIME:   petra::try_format_runtime_n_throwing_dash_pad_span
//                       (Oct 4 — REUSED verbatim)
//
// Defaults: PETRA_THROW_ON_OVERFLOW = 0, PETRA_PAD = 0,
// PETRA_SPACE_PAD = 0, PETRA_DASH_PAD = 0 (expected+untouched —
// the prior contract across Sep 6 through Oct 4).
//
// Each macro is a parenthesized expression — NOT do { } while (0)
// block — so the caller can write
//   'auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(...)'
// and capture the result.
//
// Each macro uses __VA_OPT__(,) to handle the no-args case.
// ============================================================================

// PETRA_THROW_ON_OVERFLOW defaults to 0 (expected sibling).
#ifndef PETRA_THROW_ON_OVERFLOW
#define PETRA_THROW_ON_OVERFLOW 0
#endif

// Sanity check: PETRA_THROW_ON_OVERFLOW must be 0 or 1.
#if PETRA_THROW_ON_OVERFLOW != 0 && PETRA_THROW_ON_OVERFLOW != 1
#error "PETRA_THROW_ON_OVERFLOW must be 0 (expected) or 1 (throwing)"
#endif

// PETRA_PAD defaults to 0 (untouched sibling).
#ifndef PETRA_PAD
#define PETRA_PAD 0
#endif

// Sanity check: PETRA_PAD must be 0 or 1.
#if PETRA_PAD != 0 && PETRA_PAD != 1
#error "PETRA_PAD must be 0 (untouched) or 1 (NUL-padded)"
#endif

// PETRA_SPACE_PAD defaults to 0 (no space-pad).
#ifndef PETRA_SPACE_PAD
#define PETRA_SPACE_PAD 0
#endif

// Sanity check: PETRA_SPACE_PAD must be 0 or 1.
#if PETRA_SPACE_PAD != 0 && PETRA_SPACE_PAD != 1
#error "PETRA_SPACE_PAD must be 0 (no space-pad) or 1 (space-padded)"
#endif

// PETRA_DASH_PAD defaults to 0 (no dash-pad).
#ifndef PETRA_DASH_PAD
#define PETRA_DASH_PAD 0
#endif

// Sanity check: PETRA_DASH_PAD must be 0 or 1.
#if PETRA_DASH_PAD != 0 && PETRA_DASH_PAD != 1
#error "PETRA_DASH_PAD must be 0 (no dash-pad) or 1 (dash-padded)"
#endif

// --------------------------------------------------------------------------
// Dispatch table — 4-bit tuple (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
// PETRA_SPACE_PAD, PETRA_DASH_PAD) → sibling wrapper.
//
// HEADLINE FINDING: the (0, 1, 0, 1), (0, 1, 1, 1), (1, 1, 0, 1),
// and (1, 1, 1, 1) cells reuse the (0, 0, 0, 1) and (1, 0, 0, 1)
// DASH-padded span<char> sibling wrappers verbatim because the
// composition rule is "the LATER fill overwrites the EARLIER fill"
// — so when both PAD=1 and DASH_PAD=1, the DASH fill overwrites
// the NUL fill, and bytes [written, span.size()) end up as '-'.
// This is the CLEANEST design: no NEW wrappers are needed for the
// (PAD=1, DASH_PAD=1) cells; they just dispatch to the (PAD=0,
// DASH_PAD=1) DASH-padded wrappers.  Likewise, the (0, 1, 1, 0)
// and (1, 1, 1, 0) cells REUSE the (0, 0, 1, 0) and (1, 0, 1, 0)
// SPACE-padded span<char> sibling wrappers verbatim per Oct 1's
// composition rule.
// --------------------------------------------------------------------------

#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
// (0, 0, 0, 0) → expected+untouched sibling path.

// CONSTEVAL surface: dispatches to Sep 10's try_format_bounded_runtime_n_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_span((out), (fmt)               \
                                                __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 19's try_format_runtime_n(span, ...).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n((out), (fmt)                           \
                                   __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
// (0, 1, 0, 0) → expected+NUL-padded sibling path.

// CONSTEVAL surface: dispatches to Sep 27's
// try_format_bounded_runtime_n_padded_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_padded_span((out), (fmt)        \
                                                      __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 27's try_format_runtime_n_padded_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_padded_span((out), (fmt)               \
                                              __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
// (0, 0, 1, 0) → expected+SPACE-padded sibling path.

// CONSTEVAL surface: dispatches to Sep 28's
// try_format_bounded_runtime_n_space_pad_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_space_pad_span((out), (fmt)     \
                                                       __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 28's
// try_format_runtime_n_space_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_space_pad_span((out), (fmt)             \
                                               __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
// (0, 0, 0, 1) → expected+DASH-padded sibling path (Oct 4).

// CONSTEVAL surface: dispatches to Oct 4's
// try_format_bounded_runtime_n_dash_pad_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_dash_pad_span((out), (fmt)      \
                                                       __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Oct 4's
// try_format_runtime_n_dash_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_dash_pad_span((out), (fmt)             \
                                               __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
// (0, 1, 0, 1) → expected+NUL+DASH-padded sibling path (NEW today).
//
// KEY INSIGHT: this cell REUSES the (0, 0, 0, 1) DASH-padded
// span<char> sibling wrappers verbatim because DASH overwrites
// NUL when both PAD and DASH_PAD are set.  No NEW wrapper is
// needed.

// CONSTEVAL surface: dispatches to Oct 4's
// try_format_bounded_runtime_n_dash_pad_span (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_dash_pad_span((out), (fmt)      \
                                                       __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Oct 4's
// try_format_runtime_n_dash_pad_span (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_dash_pad_span((out), (fmt)             \
                                               __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
// (0, 1, 1, 0) → expected+NUL+SPACE-padded sibling path (Oct 1's
// composition rule; REUSES the (0, 0, 1, 0) SPACE-padded
// span<char> sibling wrappers verbatim because SPACE overwrites
// NUL when both PAD and SPACE_PAD are set).
//
// (Note: TODAY's lesson does not introduce a NEW wrapper for this
// cell — the dispatch just routes to the (0, 0, 1, 0) SPACE-padded
// sibling.)

// CONSTEVAL surface: dispatches to Sep 28's
// try_format_bounded_runtime_n_space_pad_span (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_space_pad_span((out), (fmt)     \
                                                       __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 28's
// try_format_runtime_n_space_pad_span (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_space_pad_span((out), (fmt)             \
                                               __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
// (0, 0, 1, 1) → expected+SPACE+DASH-padded sibling path (Oct 4).
//
// KEY INSIGHT: this cell REUSES the (0, 0, 0, 1) DASH-padded
// span<char> sibling wrappers verbatim because DASH overwrites
// SPACE when both are set.  No NEW wrapper is needed.

// CONSTEVAL surface: dispatches to Oct 4's
// try_format_bounded_runtime_n_dash_pad_span (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_dash_pad_span((out), (fmt)      \
                                                       __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Oct 4's
// try_format_runtime_n_dash_pad_span (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_dash_pad_span((out), (fmt)             \
                                               __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
// (0, 1, 1, 1) → expected+NUL+SPACE+DASH-padded sibling path (NEW
// today; the TRIPLE composition cell).
//
// KEY INSIGHT: this cell REUSES the (0, 0, 0, 1) DASH-padded
// span<char> sibling wrappers verbatim because DASH overwrites
// SPACE overwrites NUL when all three are set, by the transitive
// composition rule.  The (0, 0, 0, 1), (0, 0, 1, 1), (0, 1, 0,
// 1), and (0, 1, 1, 1) cells all share the SAME sibling
// wrapper.  No NEW wrapper is needed.

// CONSTEVAL surface: dispatches to Oct 4's
// try_format_bounded_runtime_n_dash_pad_span (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_dash_pad_span((out), (fmt)      \
                                                       __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Oct 4's
// try_format_runtime_n_dash_pad_span (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_dash_pad_span((out), (fmt)             \
                                               __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
// (1, 0, 0, 0) → throwing+untouched sibling path.

// CONSTEVAL surface: dispatches to Sep 21's
// try_format_bounded_runtime_n_throwing_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_span((out), (fmt)     \
                                                        __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 21's
// try_format_runtime_n_throwing(span, ...).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing((out), (fmt)                  \
                                            __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
// (1, 1, 0, 0) → throwing+NUL-padded sibling path.

// CONSTEVAL surface: dispatches to Sep 27's
// try_format_bounded_runtime_n_throwing_padded_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_padded_span(          \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 27's
// try_format_runtime_n_throwing_padded_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing_padded_span((out), (fmt)     \
                                                        __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
// (1, 0, 1, 0) → throwing+SPACE-padded sibling path.

// CONSTEVAL surface: dispatches to Sep 28's
// try_format_bounded_runtime_n_throwing_space_pad_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_space_pad_span(       \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 28's
// try_format_runtime_n_throwing_space_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing_space_pad_span((out), (fmt)  \
                                                          __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
// (1, 0, 0, 1) → throwing+DASH-padded sibling path (Oct 4).

// CONSTEVAL surface: dispatches to Oct 4's
// try_format_bounded_runtime_n_throwing_dash_pad_span.
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_dash_pad_span(       \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Oct 4's
// try_format_runtime_n_throwing_dash_pad_span.
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing_dash_pad_span((out), (fmt)  \
                                                          __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
// (1, 1, 0, 1) → throwing+NUL+DASH-padded sibling path (NEW today).
//
// KEY INSIGHT: this cell REUSES the (1, 0, 0, 1) DASH-padded
// span<char> sibling wrappers verbatim because DASH overwrites
// NUL when both PAD and DASH_PAD are set.  No NEW wrapper is
// needed.

// CONSTEVAL surface: dispatches to Oct 4's
// try_format_bounded_runtime_n_throwing_dash_pad_span (REUSED
// verbatim).
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_dash_pad_span(       \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Oct 4's
// try_format_runtime_n_throwing_dash_pad_span (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing_dash_pad_span((out), (fmt)  \
                                                          __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
// (1, 1, 1, 0) → throwing+NUL+SPACE-padded sibling path (Oct 1's
// composition rule; REUSES the (1, 0, 1, 0) SPACE-padded
// span<char> sibling wrappers verbatim because SPACE overwrites
// NUL when both PAD and SPACE_PAD are set).

// CONSTEVAL surface: dispatches to Sep 28's
// try_format_bounded_runtime_n_throwing_space_pad_span (REUSED
// verbatim).
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_space_pad_span(       \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Sep 28's
// try_format_runtime_n_throwing_space_pad_span (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing_space_pad_span((out), (fmt)  \
                                                          __VA_OPT__(,) __VA_ARGS__))

#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
// (1, 0, 1, 1) → throwing+SPACE+DASH-padded sibling path (Oct 4).
//
// KEY INSIGHT: this cell REUSES the (1, 0, 0, 1) DASH-padded
// span<char> sibling wrappers verbatim because DASH overwrites
// SPACE when both are set.  No NEW wrapper is needed.

// CONSTEVAL surface: dispatches to Oct 4's
// try_format_bounded_runtime_n_throwing_dash_pad_span (REUSED
// verbatim).
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_dash_pad_span(       \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Oct 4's
// try_format_runtime_n_throwing_dash_pad_span (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing_dash_pad_span((out), (fmt)  \
                                                          __VA_OPT__(,) __VA_ARGS__))

#else  // PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 \
       //        && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
// (1, 1, 1, 1) → throwing+NUL+SPACE+DASH-padded sibling path (NEW
// today; the TRIPLE composition cell).
//
// KEY INSIGHT: this cell REUSES the (1, 0, 0, 1) DASH-padded
// span<char> sibling wrappers verbatim because DASH overwrites
// SPACE overwrites NUL when all three are set, by the transitive
// composition rule.  The (1, 0, 0, 1), (1, 0, 1, 1), (1, 1, 0,
// 1), and (1, 1, 1, 1) cells all share the SAME sibling
// wrapper.  No NEW wrapper is needed.

// CONSTEVAL surface: dispatches to Oct 4's
// try_format_bounded_runtime_n_throwing_dash_pad_span (REUSED
// verbatim).
#define TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(out, fmt, ...)              \
    (::petra::try_format_bounded_runtime_n_throwing_dash_pad_span(       \
        (out), (fmt) __VA_OPT__(,) __VA_ARGS__))

// RUNTIME surface: dispatches to Oct 4's
// try_format_runtime_n_throwing_dash_pad_span (REUSED verbatim).
#define TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(out, fmt, ...)      \
    (::petra::try_format_runtime_n_throwing_dash_pad_span((out), (fmt)  \
                                                          __VA_OPT__(,) __VA_ARGS__))

#endif  // PETRA_THROW_ON_OVERFLOW × PETRA_PAD × PETRA_SPACE_PAD × PETRA_DASH_PAD

// ============================================================================
// Part 11 — hand-rolled SECTION/CHECK framework.
// ============================================================================

namespace {

int g_pass = 0;
int g_fail = 0;
const char* g_section = "";

#define SECTION(name) do { g_section = name; } while (0)

#define CHECK(cond) do {                                                 \
    if ((cond)) {                                                         \
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
    if ((_a == _b)) {                                                    \
        ++g_pass;                                                        \
    } else {                                                             \
        ++g_fail;                                                        \
        std::fprintf(stderr,                                             \
            "FAIL: %s:%d  [%s]  CHECK_EQ(%s, %s)\n",                     \
            __FILE__, __LINE__, g_section, #a, #b);                      \
    }                                                                    \
} while (0)

}  // namespace

// ============================================================================
// Part 12 — sections
// ============================================================================

int main() {
    // ----- Section 1 — (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
    //       PETRA_SPACE_PAD, PETRA_DASH_PAD) flag 4-tuple pinned +
    //       PAD × DASH_PAD composition rule pinned.
    SECTION("Section 1 - flag 4-tuple pinned + composition rule");
    static_assert(PETRA_THROW_ON_OVERFLOW == 0
                      || PETRA_THROW_ON_OVERFLOW == 1,
                  "PETRA_THROW_ON_OVERFLOW must be 0 or 1");
    static_assert(PETRA_PAD == 0 || PETRA_PAD == 1,
                  "PETRA_PAD must be 0 or 1");
    static_assert(PETRA_SPACE_PAD == 0 || PETRA_SPACE_PAD == 1,
                  "PETRA_SPACE_PAD must be 0 or 1");
    static_assert(PETRA_DASH_PAD == 0 || PETRA_DASH_PAD == 1,
                  "PETRA_DASH_PAD must be 0 or 1");
#if PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0 (expected+untouched sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0 (expected+NUL-padded sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=0 (expected+SPACE-padded sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=1 (expected+DASH-padded sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=1 (expected+NUL+DASH-padded "
        "sibling — NEW today; DASH overwrites NUL)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=0 (expected+NUL+SPACE-padded "
        "sibling; Oct 1 composition rule)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1 (expected+SPACE+DASH-padded "
        "sibling; DASH overwrites SPACE)");
#elif PETRA_THROW_ON_OVERFLOW == 0 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=0, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1 (expected+NUL+SPACE+DASH-padded "
        "sibling — NEW today; DASH overwrites SPACE overwrites NUL)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0 (throwing+untouched sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=0 (throwing+NUL-padded sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=0 (throwing+SPACE-padded sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=1 (throwing+DASH-padded sibling)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 0 && PETRA_DASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=0, PETRA_DASH_PAD=1 (throwing+NUL+DASH-padded "
        "sibling — NEW today; DASH overwrites NUL)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 1 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=0 (throwing+NUL+SPACE-padded "
        "sibling; Oct 1 composition rule)");
#elif PETRA_THROW_ON_OVERFLOW == 1 && PETRA_PAD == 0 \
    && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 1
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=0, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1 (throwing+SPACE+DASH-padded "
        "sibling; DASH overwrites SPACE)");
#else
    std::println(
        "[Section 1] PETRA_THROW_ON_OVERFLOW=1, PETRA_PAD=1, "
        "PETRA_SPACE_PAD=1, PETRA_DASH_PAD=1 (throwing+NUL+SPACE+DASH-padded "
        "sibling — NEW today; DASH overwrites SPACE overwrites NUL)");
#endif
    CHECK(true);

    // 1b. Pin std::span<char> properties for posterity.
    static_assert(std::ranges::contiguous_range<std::span<char>>);
    static_assert(std::ranges::sized_range<std::span<char>>);
    static_assert(std::is_trivially_copyable_v<std::span<char>>);
    static_assert(std::is_standard_layout_v<std::span<char>>);
    static_assert(std::is_same_v<decltype(std::declval<std::span<char>>().data()),
                                 char*>);
    static_assert(std::is_same_v<decltype(std::declval<std::span<char>>().size()),
                                 std::size_t>);
    CHECK(true);

    // 1c. Pin std::runtime_error / std::format_error relationship.
    static_assert(std::is_base_of_v<std::exception, std::runtime_error>);
    static_assert(std::is_base_of_v<std::runtime_error, std::format_error>,
                  "std::format_error must be a std::runtime_error subclass");
    CHECK(true);

    // ----- Section 2 — Byte-state contract on success for the
    //     CURRENT (PETRA_THROW_ON_OVERFLOW, PETRA_PAD,
    //     PETRA_SPACE_PAD, PETRA_DASH_PAD) flag 4-tuple.
    //
    // For all 16 cells, the byte-state contract on success is:
    //   - (?, 0, 0, 0) → bytes [written, span.size()) are UNTOUCHED ('Q').
    //   - (?, 1, 0, 0) → bytes [written, span.size()) are '\0' (NUL).
    //   - (?, 0, 1, 0) → bytes [written, span.size()) are ' ' (SPACE).
    //   - (?, 0, 0, 1) → bytes [written, span.size()) are '-' (DASH).
    //   - (?, 1, 0, 1) → bytes [written, span.size()) are '-' (DASH overwrites NUL).
    //   - (?, 1, 1, 0) → bytes [written, span.size()) are ' ' (SPACE overwrites NUL).
    //   - (?, 0, 1, 1) → bytes [written, span.size()) are '-' (DASH overwrites SPACE).
    //   - (?, 1, 1, 1) → bytes [written, span.size()) are '-' (DASH overwrites SPACE overwrites NUL).
    SECTION("Section 2 - Byte-state contract on success");
    {
        char backing[16] = {};
        for (std::size_t i = 0; i < 16; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 16};
        std::size_t cap = buf.size();  // == 16
        std::size_t expected_written = 5;  // "ABCDE"
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "ABCDE");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), expected_written);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "ABCDE");
            CHECK_EQ(n, expected_written);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 2 - unexpected throw on success\n");
        }
#endif
        // Verify the prefix.
        CHECK_EQ(buf[0], 'A');
        CHECK_EQ(buf[1], 'B');
        CHECK_EQ(buf[2], 'C');
        CHECK_EQ(buf[3], 'D');
        CHECK_EQ(buf[4], 'E');
        // Verify the suffix per the byte-state contract.
#if PETRA_DASH_PAD == 1
        // DASH-PADDED path (any cell with DASH_PAD=1 — DASH
        // overwrites everything else per the transitive composition
        // rule): bytes [written, span.size()) are '-'.
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK_EQ(buf[i], '-');
        }
#elif PETRA_PAD == 0 && PETRA_SPACE_PAD == 0
        // UNTOUCHED path: bytes [written, span.size()) are 'Q'.
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
#elif PETRA_PAD == 1 && PETRA_SPACE_PAD == 0
        // NUL-PADDED path: bytes [written, span.size()) are '\0'.
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK_EQ(buf[i], '\0');
        }
#else
        // SPACE-PADDED path ((0,1) or (1,1) — SPACE overwrites NUL
        // when both are set): bytes [written, span.size()) are ' '.
        for (std::size_t i = expected_written; i < cap; ++i) {
            CHECK_EQ(buf[i], ' ');
        }
#endif
    }

    // 2a. Same checks but with a sub-span (the "no-bleed" case).
    //     A 6-byte sub-span at offset 5 of a 16-byte backing buffer
    //     with format 'ABCDE' (5 bytes) leaves bytes [0,5) and
    //     [11,16) as poison 'Q' regardless of the flag 4-tuple —
    //     the pad loop only touches bytes INSIDE the span.
    SECTION("Section 2a - Sub-span no-bleed on PAD x DASH_PAD path");
    {
        char backing[16] = {};
        for (std::size_t i = 0; i < 16; ++i) backing[i] = 'Q';
        std::span<char> sub{backing + 5, 6};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(sub, "ABCDE");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), 5u);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(sub, "ABCDE");
            CHECK_EQ(n, 5u);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 2a - unexpected throw on success\n");
        }
#endif
        // Prefix inside the span.
        CHECK_EQ(sub[0], 'A');
        CHECK_EQ(sub[1], 'B');
        CHECK_EQ(sub[2], 'C');
        CHECK_EQ(sub[3], 'D');
        CHECK_EQ(sub[4], 'E');
        // Suffix inside the span.
#if PETRA_DASH_PAD == 1
        // DASH-padded (any cell with DASH_PAD=1).
        CHECK_EQ(sub[5], '-');
#elif PETRA_PAD == 0 && PETRA_SPACE_PAD == 0
        CHECK_EQ(sub[5], 'Q');   // untouched
#elif PETRA_PAD == 1 && PETRA_SPACE_PAD == 0
        CHECK_EQ(sub[5], '\0');  // NUL-padded
#else
        CHECK_EQ(sub[5], ' ');   // SPACE-padded (or PAD x SPACE_PAD composition)
#endif
        // Outside the span — must be UNTOUCHED on ALL paths.
        for (std::size_t i = 0; i < 5; ++i) {
            CHECK_EQ(backing[i], 'Q');
        }
        for (std::size_t i = 11; i < 16; ++i) {
            CHECK_EQ(backing[i], 'Q');
        }
    }

    // 2c. cap=0 edge case on the DASH-padded path: the 'for (i =
    //     written; i < cap; ++i)' loop has i < cap == 0 as the
    //     initial condition, so the loop body never executes.
    SECTION("Section 2c - cap=0 edge case on PAD x DASH_PAD path");
    {
        char backing[1] = {'Q'};
        std::span<char> empty_span{backing, 0};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(empty_span, "");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), 0u);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(empty_span, "");
            CHECK_EQ(n, 0u);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 2c - unexpected throw on cap=0+empty-fmt\n");
        }
#endif
        // The sentinel byte OUTSIDE the cap=0 writable region
        // remains untouched.
        CHECK_EQ(backing[0], 'Q');
    }

    // 2d. written==cap edge case: format 'ABCDEFGH' (8 bytes) into
    //     a span of size 8.  On the padded path, the 'for (i =
    //     written; i < cap; ++i)' loop has i == cap as the initial
    //     condition, so the loop body never executes (the span is
    //     exactly filled).
    SECTION("Section 2d - written==cap edge case on PAD x DASH_PAD path");
    {
        char backing[8] = {};
        for (std::size_t i = 0; i < 8; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 8};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "ABCDEFGH");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), 8u);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "ABCDEFGH");
            CHECK_EQ(n, 8u);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 2d - unexpected throw on full-buffer\n");
        }
#endif
        for (std::size_t i = 0; i < buf.size(); ++i) {
            CHECK_EQ(buf[i], "ABCDEFGH"[i]);
        }
    }

    // ----- Section 3 — Buffer atomicity on TooLarge.
    //     On all 16 cells, the buffer must be UNTOUCHED on overflow.
    SECTION("Section 3 - Buffer atomicity on TooLarge");
    {
        char backing[8] = {};
        for (std::size_t i = 0; i < 8; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 8};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
            buf, "this is way too long to fit");
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::FormatErrorKind::TooLarge);
#else
        bool caught = false;
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
                buf, "this is way too long to fit");
            (void)n;
        } catch (const std::runtime_error&) {
            caught = true;
        } catch (...) {
            // unexpected
        }
        CHECK(caught);
#endif
        // Buffer must be UNTOUCHED on overflow.
        for (std::size_t i = 0; i < buf.size(); ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
    }

    // ----- Section 4 — Byte-count contract on success.
    SECTION("Section 4 - Byte-count contract on success");
    {
        char backing[32] = {};
        for (std::size_t i = 0; i < 32; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 32};
        std::size_t expected_written = 13;  // "Hello, world!"
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
            buf, "Hello, {}!", "world");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), expected_written);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
                buf, "Hello, {}!", "world");
            CHECK_EQ(n, expected_written);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 4 - unexpected throw on success\n");
        }
#endif
        for (std::size_t i = 0; i < expected_written; ++i) {
            CHECK_EQ(buf[i], "Hello, world!"[i]);
        }
    }

    // ----- Section 5 — RUNTIME surface sanity check.
    SECTION("Section 5 - RUNTIME surface sanity check");
    {
        char backing[32] = {};
        for (std::size_t i = 0; i < 32; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 32};
        std::size_t expected_written = 11;  // "value is 42"
#if PETRA_THROW_ON_OVERFLOW == 0
        std::string_view fmt = "value is {}";
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
            buf, fmt, 42);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), expected_written);
#else
        try {
            std::string_view fmt = "value is {}";
            auto n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
                buf, fmt, 42);
            CHECK_EQ(n, expected_written);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 5 - unexpected throw on success\n");
        }
#endif
        for (std::size_t i = 0; i < expected_written; ++i) {
            CHECK_EQ(buf[i], "value is 42"[i]);
        }
    }

    // ----- Section 6 — CONSTEVAL gate pinned.
    SECTION("Section 6 - CONSTEVAL gate requires literal fmt");
    {
#if PETRA_THROW_ON_OVERFLOW == 0
        using F1 = decltype(TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using ExpectedFmt1 = std::expected<std::size_t, petra::FormatError>;
        static_assert(std::is_same_v<F1, ExpectedFmt1>,
                      "CONSTEVAL macro must return expected<size_t, "
                      "FormatError> on the expected path");
#else
        using F1 = decltype(TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        static_assert(std::is_same_v<F1, std::size_t>,
                      "CONSTEVAL macro must return std::size_t on the "
                      "throwing path");
#endif
        CHECK(true);
    }

    // ----- Section 7 — RUNTIME macro accepts std::string_view (NOT
    //     consteval-checked).
    SECTION("Section 7 - RUNTIME macro accepts std::string_view");
    {
#if PETRA_THROW_ON_OVERFLOW == 0
        using F1 = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        using ExpectedFmt1 = std::expected<std::size_t, petra::FormatError>;
        static_assert(std::is_same_v<F1, ExpectedFmt1>,
                      "RUNTIME macro must return expected<size_t, "
                      "FormatError> on the expected path");
#else
        using F1 = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
        static_assert(std::is_same_v<F1, std::size_t>,
                      "RUNTIME macro must return std::size_t on the "
                      "throwing path");
#endif
        CHECK(true);
    }

    // ----- Section 8 — Format error: atomicity preserved on the
    //     RUNTIME surface.
    SECTION("Section 8 - Format error atomicity (RUNTIME)");
    {
        char backing[16] = {};
        for (std::size_t i = 0; i < 16; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 16};
        std::string bad_fmt = "bad {";  // unterminated brace
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
            buf, std::string_view{bad_fmt});
        CHECK(!r.has_value());
        CHECK_EQ(r.error().kind, petra::FormatErrorKind::Format);
#else
        bool caught = false;
        try {
            auto n = TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
                buf, std::string_view{bad_fmt});
            (void)n;
        } catch (...) {
            caught = true;
        }
        CHECK(caught);
#endif
        // Buffer must be UNTOUCHED on Format error (no byte at any
        // index was written).
        for (std::size_t i = 0; i < buf.size(); ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
    }

    // ----- Section 9 — std::format_error IS-A std::runtime_error.
    SECTION("Section 9 - std::format_error IS-A std::runtime_error");
    {
        static_assert(
            std::is_base_of_v<std::runtime_error, std::format_error>,
            "std::format_error must derive from std::runtime_error");
        CHECK(true);
    }

    // ----- Section 10 — Concurrency: 4 threads × 25 concurrent
    //       calls.
    SECTION("Section 10 - Concurrency (4 threads x 25 calls)");
    {
        std::atomic<int> a_pass{0};
        std::atomic<int> a_fail{0};
        auto worker = [&a_pass, &a_fail]() {
            int lp = 0, lf = 0;
            char backing[32] = {};
            std::span<char> buf{backing, 32};
            for (int i = 0; i < 25; ++i) {
                for (std::size_t j = 0; j < buf.size(); ++j) buf[j] = 'Q';
#if PETRA_THROW_ON_OVERFLOW == 0
                auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
                    buf, "iteration {}", i);
                if (r.has_value() && r.value() > 0u) {
                    ++lp;
                } else {
                    ++lf;
                }
#else
                try {
                    auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
                        buf, "iteration {}", i);
                    if (n > 0u) {
                        ++lp;
                    } else {
                        ++lf;
                    }
                } catch (...) {
                    ++lf;
                }
#endif
            }
            a_pass.fetch_add(lp, std::memory_order_relaxed);
            a_fail.fetch_add(lf, std::memory_order_relaxed);
        };
        std::array<std::thread, 4> threads;
        for (auto& t : threads) {
            t = std::thread(worker);
        }
        for (auto& t : threads) {
            t.join();
        }
        CHECK_EQ(a_pass.load(), 100);
        CHECK_EQ(a_fail.load(), 0);
    }

    // ----- Section 11 — No-args case: __VA_OPT__(,) works.
    SECTION("Section 11 - No-args case via __VA_OPT__(,)");
    {
        char backing[16] = {};
        for (std::size_t i = 0; i < 16; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 16};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "no args");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), 7u);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "no args");
            CHECK_EQ(n, 7u);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 11 - unexpected throw on no-args\n");
        }
#endif
        for (std::size_t i = 0; i < 7; ++i) {
            CHECK_EQ(buf[i], "no args"[i]);
        }
    }

    // ----- Section 12 — Multiple args + value formatting.
    SECTION("Section 12 - Multiple args + value formatting");
    {
        char backing[64] = {};
        for (std::size_t i = 0; i < 64; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 64};
        std::size_t expected_written = 21;  // "x=42, y=hello, z=3.14"
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
            buf, "x={}, y={}, z={:.2f}", 42, "hello", 3.14);
        CHECK(r.has_value());
        CHECK_EQ(r.value(), expected_written);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
                buf, "x={}, y={}, z={:.2f}", 42, "hello", 3.14);
            CHECK_EQ(n, expected_written);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 12 - unexpected throw on success\n");
        }
#endif
        for (std::size_t i = 0; i < expected_written; ++i) {
            CHECK_EQ(buf[i], "x=42, y=hello, z=3.14"[i]);
        }
    }

    // ----- Section 13 — Macro is a parenthesized expression (can be
    //       captured as 'auto n = MACRO(...)').
    SECTION("Section 13 - Macro returns parenthesized expression");
    {
        char backing[32] = {};
        for (std::size_t i = 0; i < 32; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 32};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "5");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), 1u);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "5");
            CHECK_EQ(n, 1u);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 13 - unexpected throw on success\n");
        }
#endif
    }

    // ----- Section 14 — Perpendicular-axis claim pinned via
    //       decltype checks on all 16 paths.
    SECTION("Section 14 - Perpendicular-axis decltype checks (16 cells)");
    {
        using RA = decltype(TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::format_string<int>>(),
            std::declval<int>()));
        using RB = decltype(TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
            std::declval<std::span<char>>(),
            std::declval<std::string_view>(),
            std::declval<int>()));
#if PETRA_THROW_ON_OVERFLOW == 0
        using ExpectedT = std::expected<std::size_t, petra::FormatError>;
        static_assert(std::is_same_v<RA, ExpectedT>,
                      "expected-path CONSTEVAL must return expected<size_t, "
                      "FormatError>");
        static_assert(std::is_same_v<RB, ExpectedT>,
                      "expected-path RUNTIME must return expected<size_t, "
                      "FormatError>");
#else
        static_assert(std::is_same_v<RA, std::size_t>,
                      "throwing-path CONSTEVAL must return std::size_t");
        static_assert(std::is_same_v<RB, std::size_t>,
                      "throwing-path RUNTIME must return std::size_t");
#endif
        CHECK(true);
    }

    // ----- Section 15 — std::format_error dynamic_cast check.
    SECTION("Section 15 - std::format_error dynamic_cast check");
    {
        char backing[16] = {};
        for (std::size_t i = 0; i < 16; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 16};
#if PETRA_THROW_ON_OVERFLOW == 1
        bool caught_runtime = false;
        bool dynamic_cast_is_nullptr = false;
        try {
            (void)TRY_FORMAT_BOUNDED_RUNTIME_SPAN_N_PAD_OR_THROW(
                buf, std::string_view{"bad {"});
        } catch (const std::runtime_error& e) {
            caught_runtime = true;
            // The throwing wrapper re-raises as std::runtime_error
            // (NOT std::format_error).  dynamic_cast to
            // std::format_error* must return nullptr.
            dynamic_cast_is_nullptr =
                (dynamic_cast<const std::format_error*>(&e) == nullptr);
        } catch (...) {
            // unexpected
        }
        CHECK(caught_runtime);
        CHECK(dynamic_cast_is_nullptr);
#else
        CHECK(true);
#endif
    }

    // ----- Section 16 — On the (?, 1, 0, 1) and (?, 1, 1, 1)
    //       composition paths, explicitly verify that bytes
    //       [written, span.size()) are '-' (NOT '\0' and NOT ' '
    //       and NOT 'Q').  This is the DASH-overwrites-everything
    //       composition-rule byte-state contract check.
    SECTION("Section 16 - Composition rule: DASH-pad byte is '-'");
    {
        char backing[8] = {};
        for (std::size_t i = 0; i < 8; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 8};
        std::size_t expected_written = 4;  // "WXYZ"
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "WXYZ");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), expected_written);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "WXYZ");
            CHECK_EQ(n, expected_written);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 16 - unexpected throw on success\n");
        }
#endif
        // Verify the prefix.
        CHECK_EQ(buf[0], 'W');
        CHECK_EQ(buf[1], 'X');
        CHECK_EQ(buf[2], 'Y');
        CHECK_EQ(buf[3], 'Z');
#if PETRA_DASH_PAD == 1
        // DASH-pad path (any cell with DASH_PAD=1, including
        // composition cells): bytes [4, 8) must be '-' (NOT '\0'
        // and NOT ' ' and NOT 'Q' — the transitive composition
        // rule).
        for (std::size_t i = expected_written; i < buf.size(); ++i) {
            CHECK_EQ(buf[i], '-');
            CHECK(buf[i] != '\0');
            CHECK(buf[i] != ' ');
            CHECK(buf[i] != 'Q');
        }
#elif PETRA_PAD == 0 && PETRA_SPACE_PAD == 0
        // UNTOUCHED path: bytes [4, 8) must be 'Q'.
        for (std::size_t i = expected_written; i < buf.size(); ++i) {
            CHECK_EQ(buf[i], 'Q');
        }
#elif PETRA_PAD == 1 && PETRA_SPACE_PAD == 0
        // NUL-padded path: bytes [4, 8) must be '\0'.
        for (std::size_t i = expected_written; i < buf.size(); ++i) {
            CHECK_EQ(buf[i], '\0');
        }
#else  // (?, 1, 1, 0) — composition: SPACE overwrites NUL.
        // bytes [4, 8) must be ' ' (NOT '\0' — composition rule).
        for (std::size_t i = expected_written; i < buf.size(); ++i) {
            CHECK_EQ(buf[i], ' ');
            CHECK(buf[i] != '\0');
            CHECK(buf[i] != 'Q');
        }
#endif
    }

    // ----- Section 17 — On the (?, 1, 0, 1) and (?, 1, 1, 1) cells,
    //       verify the DASH-pad path's distinctness from the
    //       NUL-padded / SPACE-padded / untouched paths via
    //       per-byte check.
    SECTION("Section 17 - Composition rule: DASH distinct from NUL/SPACE/untouched");
    {
        char backing[8] = {};
        for (std::size_t i = 0; i < 8; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 8};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "MNOP");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), 4u);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "MNOP");
            CHECK_EQ(n, 4u);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 17 - unexpected throw on success\n");
        }
#endif
        // On the (?, 1, 0, 1) and (?, 1, 1, 1) cells, the DASH
        // fill has overwritten the NUL/SPACE fill, so the suffix
        // bytes must be '-' (NOT '\0' and NOT ' ').  On the
        // (?, 0, 0, 1) and (?, 0, 1, 1) cells, the DASH fill is
        // the only fill, so the suffix bytes must be '-'.  On all
        // 4 DASH-pad cells, the suffix bytes are '-'.
#if PETRA_DASH_PAD == 1
        for (std::size_t i = 4; i < 8; ++i) {
            CHECK(buf[i] == '-');
            CHECK(buf[i] != '\0');  // NOT NUL (DASH overwrites NUL)
            CHECK(buf[i] != ' ');   // NOT SPACE (DASH overwrites SPACE)
            CHECK(buf[i] != 'Q');   // NOT untouched
        }
#else
        CHECK(true);
#endif
    }

    // ----- Section 18 — On the (?, 1, 1, 0) composition cell
    //       (REUSES Sep 28's (?, 0, 1, 0) SPACE-padded wrapper
    //       verbatim per Oct 1's rule), verify the byte-state
    //       contract: bytes [written, span.size()) must be ' ' (NOT
    //       '\0' and NOT '-' and NOT 'Q').
    SECTION("Section 18 - PAD x SPACE_PAD composition: SPACE overwrites NUL");
    {
        char backing[8] = {};
        for (std::size_t i = 0; i < 8; ++i) backing[i] = 'Q';
        std::span<char> buf{backing, 8};
#if PETRA_THROW_ON_OVERFLOW == 0
        auto r = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "QRST");
        CHECK(r.has_value());
        CHECK_EQ(r.value(), 4u);
#else
        try {
            auto n = TRY_FORMAT_BOUNDED_SPAN_N_PAD_OR_THROW(buf, "QRST");
            CHECK_EQ(n, 4u);
        } catch (...) {
            ++g_fail;
            std::fprintf(stderr,
                "FAIL: Section 18 - unexpected throw on success\n");
        }
#endif
#if PETRA_PAD == 1 && PETRA_SPACE_PAD == 1 && PETRA_DASH_PAD == 0
        // Composition cell: bytes [4, 8) must be ' ' (NOT '\0'
        // because SPACE overwrites NUL on this path).
        for (std::size_t i = 4; i < 8; ++i) {
            CHECK(buf[i] == ' ');
            CHECK(buf[i] != '\0');
            CHECK(buf[i] != '-');
            CHECK(buf[i] != 'Q');
        }
#else
        CHECK(true);
#endif
    }

    std::println("===================");
    std::println("PASS: {}  FAIL: {}", g_pass, g_fail);
    std::println("===================");
    if (g_fail != 0) {
        std::println("OVERALL: FAIL");
        return 1;
    }
    std::println("OVERALL: PASS");
    return 0;
}
