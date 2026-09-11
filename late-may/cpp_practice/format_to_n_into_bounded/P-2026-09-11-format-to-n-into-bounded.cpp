// P-2026-09-11 — format_to_n_into_bounded: the NUL-TERMINATING BOUNDED
// sibling of Sep 9 / Sep 10's try_format_bounded_runtime_n[_span].  Closes
// Sep 10's "Where we go next" item #3 — a `petra::format_to_n_into_bounded`
// adapter that combines Aug 25's NUL-terminating behavior with today's
// `cap` parameter (Aug 25's format_to_n_into does NOT have a cap parameter
// — it writes a complete std::string and returns it).
//
// The shape:
//
//   petra::format_to_n_into_bounded<Args...>(char* out, std::size_t cap,
//                                            std::format_string<Args...> fmt,
//                                            Args&&... args)
//       -> std::expected<std::size_t, petra::FormatError>
//
//   petra::format_to_n_into_bounded<Args...>(std::span<char> out,
//                                            std::format_string<Args...> fmt,
//                                            Args&&... args)
//       -> std::expected<std::size_t, petra::FormatError>
//
// The wrapper's contract is the COMBINATION of Sep 9's bounded-buffer
// contract and Aug 25's NUL-termination contract:
//
//   On success:
//     - out[0, r.value()) has the formatted bytes (the would-be size).
//     - out[r.value()] (or out[cap-1] when r.value() > cap) is '\0',
//       so out[0, cap) is ALWAYS a valid C-string on the success path.
//
//   On TooLarge:
//     - The caller's buffer is UNTOUCHED (pass-1 measure-then-cap-fails
//       short-circuits before pass-2 writes anything).
//     - Returns unexpected{FormatError{TooLarge, ..., needed_bytes=N}}.
//
//   On FormatError (from std::make_format_args):
//     - The caller's buffer is UNTOUCHED.
//     - Returns unexpected{FormatError{Format, e.what()}}.
//
// Today's wrapper is the FIRST wrapper in the chain to combine the
// two-pass measure-then-write pair (Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10)
// with NUL-termination (Aug 25).  The previous wrappers in the chain
// returned either:
//   - std::string (Sep 6, Sep 7) — no caller-owned buffer, so no NUL placement.
//   - std::size_t bytes-written (Sep 8, Sep 9, Sep 10) — caller-owned
//     buffer but no NUL-termination (caller owns NUL-termination).
// Today returns the would-be size but NUL-terminates for "I want a
// C-string in buf" use cases — the bounded sibling of Aug 25's
// format_to_n_into.
//
// What gets reused (verbatim from Sep 6 / Sep 7 / Sep 8 / Sep 9 / Sep 10):
//   - petra::FormatError{kind, message, needed_bytes} — verbatim.
//   - petra::FormatErrorKind {FormatParse, FormatArgument, Format, TooLarge}
//     — verbatim.
//   - petra::counting_output_iterator — verbatim (still hand-rolled;
//     std::counting_output_iterator is NOT shipped by libc++ 21).
//   - petra::detail::bounded_char_writer — verbatim (Sep 8 / Sep 9 / Sep 10).
//
// What is NEW today:
//   - petra::detail::try_format_to_n_into_bounded_impl — a NEW two-pass
//     impl that RESERVES ONE SLOT for the NUL.  Differs from Sep 9's
//     try_format_bounded_runtime_n_impl: Sep 9 reserves NO slots;
//     today's impl uses effective_cap = cap - 1 for pass-2 writing.
//     Reusing Sep 9's impl would not work because Sep 9 does not reserve
//     a NUL slot, so the success path would overwrite the formatted
//     text with the NUL on the r.value() == cap edge.
//   - petra::format_to_n_into_bounded<Args...>(char* out, std::size_t cap,
//     std::format_string<Args...>, Args&&...) — the NUL-terminating
//     bounded sibling of Sep 9's wrapper.  Lives in the same `petra`
//     namespace; no new types.
//   - petra::format_to_n_into_bounded<Args...>(std::span<char> out, ...)
//     — the SPAN overload of today's primary surface.
//   - NUL placement is the Aug 25 formula:
//       nul_idx = (written < cap) ? written : (cap - 1)
//     On the success path, out[nul_idx] = '\0' is written LAST, after
//     pass-2 has populated out[0, written).
//
// C++23, std::format, std::vformat_to, std::format_string<Args...>
// (P2216R3), std::make_format_args, std::format_error, std::expected
// (P0323R12), std::println (P2093R14), std::span<char> (P0122R7).
// Requires -fexperimental-library on Apple Clang 21.0.0 / libc++ 21
// for std::println.

#include <cstddef>
#include <cstring>
#include <expected>
#include <format>
#include <iterator>
#include <memory>
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace petra {

// ----- FormatError (reused verbatim from Sep 6..10) -----------------------

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

// ----- counting_output_iterator (reused verbatim from Sep 6..10) ----------

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

// ----- bounded_char_writer (reused verbatim from Sep 8..10) ---------------

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

// ----- try_format_to_n_into_bounded_impl (NEW today) ----------------------
//
// Two-pass measure-then-write pair, RESERVING ONE SLOT for the NUL.
// This is the key difference from Sep 9's try_format_bounded_runtime_n_impl:
// Sep 9's impl treats `cap` as the full writable byte count (no NUL
// reservation, so success means needed <= cap exactly); today's impl
// treats `cap - 1` as the writable byte count (one slot is reserved
// for the NUL on the success path), and the truncation edge is
// handled as a partial-write SUCCESS — NOT a TooLarge error.
//
// Returns:
//   - expected<size_t> = r.value() == FORMATTED bytes in out[0, r.value())
//     (NOT counting the NUL).  Three success cases:
//       needed <  cap: r.value() == needed (full output, NUL at needed).
//       needed == cap: r.value() == cap - 1 (truncation, NUL at cap - 1).
//       needed == 0 and cap == 0: r.value() == 0 (no bytes, no NUL).
//   - The NUL is at out[r.value()] for r.value() < cap, or at
//     out[cap - 1] for the truncation edge.
//
// On TooLarge (needed > cap): caller's buffer is UNTOUCHED.  No NUL
// is written on this path.
//
// On format_error: caller's buffer is UNTOUCHED.
//
// This impl is DIFFERENT from Sep 9's.  Reusing Sep 9's impl would
// NOT work: Sep 9 returns TooLarge whenever written != needed, which
// means Sep 9 cannot express the truncation-success path (Aug 25's
// core contract).  Today needs its own impl that handles the
// truncation-success path as a real success.

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_to_n_into_bounded_impl(char* out, std::size_t cap,
                                  std::format_string<Args...> fmt,
                                  const Args&... args) {
    // Pass 1: measure the would-be byte count.
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

    // cap == 0: no room for anything.  needed == 0 is success (no NUL);
    // needed > 0 is TooLarge.
    if (cap == 0) {
        if (needed > 0) {
            return std::unexpected(
                FormatError{FormatErrorKind::TooLarge,
                            std::string{"formatted output would be "}
                                + std::to_string(needed)
                                + " bytes; cap is 0",
                            needed});
        }
        return std::size_t{0};
    }

    // cap >= 1: reserve one slot for the NUL.  Effective writable is
    // cap - 1.
    if (needed > cap) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"formatted output would be "}
                            + std::to_string(needed)
                            + " bytes; cap is " + std::to_string(cap)
                            + " (effective writable is "
                            + std::to_string(cap - 1) + ")",
                        needed});
    }

    const std::size_t effective_cap = cap - 1;

    // Truncation edge: needed == cap.  Aug 25's contract writes
    // cap - 1 bytes (capped by the bounded writer at effective_cap)
    // and puts the NUL at cap - 1, overwriting nothing (the cap - 1
    // byte is the last writable slot).
    if (needed == cap) {
        // Pass 2: write into out[0, cap - 1).
        std::size_t written = 0;
        try {
            bounded_char_writer sink{out, effective_cap, cb, &written};
            (void)std::vformat_to(std::move(sink), fmt.get(),
                                  std::make_format_args(args...));
        } catch (const std::format_error& e) {
            return std::unexpected(
                FormatError{FormatErrorKind::Format, e.what()});
        }
        // Invariant: written == effective_cap (the bounded writer
        // stops at effective_cap when there are more chars to write).
        if (written != effective_cap) {
            return std::unexpected(
                FormatError{FormatErrorKind::TooLarge,
                            std::string{"bounded_char_writer wrote "}
                                + std::to_string(written)
                                + " bytes; expected "
                                + std::to_string(effective_cap)
                                + " on truncation edge",
                            needed});
        }
        out[cap - 1] = '\0';  // NUL at cap - 1 (Aug 25 formula).
        return effective_cap;
    }

    // needed < cap: full success path.  Write all `needed` bytes into
    // out[0, needed), then NUL at out[needed].
    std::size_t written = 0;
    try {
        bounded_char_writer sink{out, effective_cap, cb, &written};
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
    out[needed] = '\0';
    return written;
}

}  // namespace detail

// ----- format_to_n_into_bounded (NEW today) --------------------------------
//
// The NUL-terminating bounded sibling of Sep 9's try_format_bounded_runtime_n.
// Two public surfaces:
//
//   1. char* + std::size_t — the primary surface; reserves one slot
//      at the end for the NUL (matches Aug 25's NUL placement contract).
//   2. std::span<char> — Sep 10's forwarding wrapper shape; extracts
//      .data() + .size() and forwards.
//
// NUL placement is identical to Aug 25's format_to_n_into:
//   nul_idx = (written < cap) ? written : (cap - 1)
//
// On the success path:
//   - written is the would-be size (NOT capped; same as Sep 9 / Aug 25).
//   - out[0, written) has the formatted bytes.
//   - out[nul_idx] = '\0', so out[0, cap) is ALWAYS a valid C-string.
//
// On any error path:
//   - The caller's buffer is UNTOUCHED.  No NUL is written on error
//     (different from Aug 25, which NUL-terminates at out[0] on the
//     error path; today's contract is "buffer untouched on error").

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
format_to_n_into_bounded(char* out, std::size_t cap,
                         std::format_string<Args...> fmt,
                         const Args&... args) {
    return detail::try_format_to_n_into_bounded_impl(out, cap, fmt,
                                                     args...);
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
format_to_n_into_bounded(std::span<char> out,
                         std::format_string<Args...> fmt,
                         const Args&... args) {
    return format_to_n_into_bounded(out.data(), out.size(),
                                    fmt, args...);
}

}  // namespace petra

// ---------------------------------------------------------------------------
// Test counter — single-threaded TU.
// ---------------------------------------------------------------------------

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

void section_1_sizeof_and_probes() {
    SECTION("1.sizeof+probes");
    CHECK(sizeof(std::size_t) == 8);
    CHECK(sizeof(std::expected<std::size_t, petra::FormatError>) >= 8);
    CHECK(__cpp_lib_format == 202110);
    CHECK(__cpp_lib_expected == 202211);
    CHECK(__cpp_lib_span == 202002);
    CHECK(std::ranges::contiguous_range<std::span<char>>);
    CHECK(std::ranges::sized_range<std::span<char>>);
    CHECK(std::is_trivially_copyable_v<std::span<char>>);
    CHECK(std::is_standard_layout_v<std::span<char>>);
}

void section_2_exact_fit_nul_at_written() {
    SECTION("2.exact-fit");
    // cap=8, format "1234567" (7 chars) — fits with room for NUL.
    char buf[8] = {};
    auto r = petra::format_to_n_into_bounded(buf, sizeof(buf), "1234567");
    CHECK(r.has_value());
    CHECK(r.value() == 7);
    CHECK(std::string_view{buf} == "1234567");
    CHECK(buf[7] == '\0');  // NUL at written == 7 == cap
}

void section_3_no_truncation_nul_at_written() {
    SECTION("3.no-truncation");
    // cap=16, format "hello world" (11 chars).
    char buf[16] = {};
    auto r = petra::format_to_n_into_bounded(buf, sizeof(buf),
                                             "hello {}", "world");
    CHECK(r.has_value());
    CHECK(r.value() == 11);
    CHECK(std::string_view{buf} == "hello world");
    CHECK(buf[11] == '\0');
    CHECK(buf[12] == '\0');  // untouched (zero-initialised)
}

void section_4_truncation_edge_nul_at_cap_minus_one() {
    SECTION("4.truncation-edge");
    // needed == cap exactly: truncation edge.  cap=5, format "ABCDE"
    // (5 chars).  Today's impl writes cap-1 = 4 bytes ("ABCD") and NUL
    // at out[cap-1] = out[4] (Aug 25's truncation-edge formula).
    char buf[5];
    for (auto& b : buf) b = 'Q';
    auto r = petra::format_to_n_into_bounded(buf, sizeof(buf), "ABCDE");
    CHECK(r.has_value());
    CHECK(r.value() == 4);  // cap - 1
    CHECK(buf[0] == 'A');
    CHECK(buf[1] == 'B');
    CHECK(buf[2] == 'C');
    CHECK(buf[3] == 'D');
    CHECK(buf[4] == '\0');  // NUL at cap - 1
}

void section_5_too_large_buffer_untouched() {
    SECTION("5.too-large");
    // cap=4, format "1234567" (7 chars) — TooLarge, buffer untouched.
    char buf[4];
    for (auto& b : buf) b = 'Q';
    auto r = petra::format_to_n_into_bounded(buf, sizeof(buf), "1234567");
    CHECK(!r.has_value());
    CHECK(r.error().kind == petra::FormatErrorKind::TooLarge);
    CHECK(r.error().needed_bytes == 7);
    CHECK(buf[0] == 'Q');
    CHECK(buf[1] == 'Q');
    CHECK(buf[2] == 'Q');
    CHECK(buf[3] == 'Q');
}

void section_6_cap_zero_empty_format() {
    SECTION("6.cap-zero-empty");
    // cap=0, format "" — success, no bytes written, no NUL written.
    char c = 'Q';
    auto r = petra::format_to_n_into_bounded(&c, 0, "");
    CHECK(r.has_value());
    CHECK(r.value() == 0);
    CHECK(c == 'Q');  // untouched
}

void section_7_cap_zero_nonempty_format() {
    SECTION("7.cap-zero-nonempty");
    // cap=0, format "x" — TooLarge, needed_bytes == 1.
    char c = 'Q';
    auto r = petra::format_to_n_into_bounded(&c, 0, "x");
    CHECK(!r.has_value());
    CHECK(r.error().kind == petra::FormatErrorKind::TooLarge);
    CHECK(r.error().needed_bytes == 1);
    CHECK(c == 'Q');  // untouched
}

void section_8_cap_one_only_nul() {
    SECTION("8.cap-one");
    // cap=1, format "x" (1 byte).  needed == cap == 1 → truncation edge.
    // Today's impl writes effective_cap=0 bytes, NUL at out[cap-1]=out[0],
    // return effective_cap=0.  The formatted byte never makes it into
    // the buffer (the NUL overwrites the only writable slot, which was
    // never written to).
    char buf[1] = {'Q'};
    auto r = petra::format_to_n_into_bounded(buf, sizeof(buf), "x");
    CHECK(r.has_value());
    CHECK(r.value() == 0);  // effective_cap == cap - 1 == 0
    CHECK(buf[0] == '\0');
}

void section_9_format_specifiers() {
    SECTION("9.format-specifiers");
    char buf[64] = {};
    auto r = petra::format_to_n_into_bounded(buf, sizeof(buf),
                                             "v={} h={:#x} pi={:.3f}",
                                             42, 0xcafe, 3.14159);
    CHECK(r.has_value());
    CHECK(std::string_view{buf} == "v=42 h=0xcafe pi=3.142");
    CHECK(buf[r.value()] == '\0');
}

void section_10_atomicity_on_failure() {
    SECTION("10.atomicity-failure");
    // Pre-fill buffer with valid string; issue a TooLarge call; verify
    // the pre-fill survives.  cap=8, format "hello world" (11 chars) —
    // needed > cap, TooLarge.
    char buf[8];
    std::memcpy(buf, "v=42", 5);  // 4 chars + NUL would fit, but we only set 5.
    for (auto& b : buf) b = 'Q';  // re-poison.
    buf[0] = 'v'; buf[1] = '='; buf[2] = '4'; buf[3] = '2';
    auto r = petra::format_to_n_into_bounded(buf, sizeof(buf),
                                             "{}", "hello world");
    CHECK(!r.has_value());
    CHECK(r.error().kind == petra::FormatErrorKind::TooLarge);
    CHECK(r.error().needed_bytes == 11);
    CHECK((std::string_view{buf, 4} == "v=42"));  // pre-fill survives
}

void section_11_span_overload_equivalent() {
    SECTION("11.span-equiv");
    char buf1[16] = {};
    char buf2[16] = {};
    std::span<char> sp1{buf1, sizeof(buf1)};
    std::span<char> sp2{buf2, sizeof(buf2)};

    auto r1 = petra::format_to_n_into_bounded(buf1, sizeof(buf1),
                                              "v={}", 42);
    auto r2 = petra::format_to_n_into_bounded(sp2, "v={}", 42);

    CHECK(r1.has_value() && r2.has_value());
    CHECK(r1.value() == r2.value());
    CHECK(r1.value() == 4);
    CHECK(std::string_view{buf1} == std::string_view{buf2});
    CHECK(buf1[4] == '\0');
    CHECK(buf2[4] == '\0');
}

void section_12_subspan_no_bleed() {
    SECTION("12.subspan");
    // 16-byte buffer, sub-span at offset 5 covering [5, 11).
    char buf[16];
    for (auto& b : buf) b = 'Q';
    std::span<char> sp{buf + 5, 6};
    auto r = petra::format_to_n_into_bounded(sp, "ABCDE");
    CHECK(r.has_value());
    CHECK(r.value() == 5);
    // Bytes [0, 5) untouched.
    CHECK(buf[0] == 'Q');
    CHECK(buf[4] == 'Q');
    // Bytes [5, 10) = formatted text.
    CHECK(buf[5] == 'A');
    CHECK(buf[6] == 'B');
    CHECK(buf[7] == 'C');
    CHECK(buf[8] == 'D');
    CHECK(buf[9] == 'E');
    // NUL at written == 5 within sub-span: buf[10] == '\0'.
    CHECK(buf[10] == '\0');
    // Bytes [11, 16) untouched.
    CHECK(buf[11] == 'Q');
    CHECK(buf[15] == 'Q');
}

void section_13_multi_arg_round_trip() {
    SECTION("13.multi-arg");
    char buf[64] = {};
    auto r = petra::format_to_n_into_bounded(buf, sizeof(buf),
                                             "name={} age={:#x} pi={:.5f}",
                                             "ada", 0x2a, 3.14159);
    CHECK(r.has_value());
    CHECK(std::string_view{buf} == "name=ada age=0x2a pi=3.14159");
    CHECK(buf[r.value()] == '\0');
}

void section_14_utf8_mid_codepoint() {
    SECTION("14.utf8");
    char buf[16] = {};
    auto r = petra::format_to_n_into_bounded(buf, sizeof(buf),
                                             "{}", "héllo");
    CHECK(r.has_value());
    CHECK(r.value() == 6);  // 5 UTF-8 bytes + é is 2 bytes
    CHECK(std::string_view{buf} == "héllo");
    CHECK(buf[6] == '\0');
}

void section_15_embedded_nul_in_arg() {
    SECTION("15.embedded-nul");
    // Use std::string because std::string_view stops at first NUL.
    std::string s{'a', '\0', 'b'};
    char buf[16] = {};
    auto r = petra::format_to_n_into_bounded(buf, sizeof(buf), "{}", s);
    CHECK(r.has_value());
    CHECK(r.value() == 3);
    CHECK(buf[0] == 'a');
    CHECK(buf[1] == '\0');
    CHECK(buf[2] == 'b');
    // Our NUL at buf[3] is the wrapper's terminator.
    CHECK(buf[3] == '\0');
}

void section_16_byte_count_invariant() {
    SECTION("16.byte-count-invariant");
    // For ("hello {}", "world") — 11 bytes total — verify the wrapper
    // reports r.value() == 11 AND an independent counting probe reports 11.
    char buf[16] = {};
    auto r = petra::format_to_n_into_bounded(buf, sizeof(buf),
                                             "hello {}", "world");
    CHECK(r.has_value());
    CHECK(r.value() == 11);

    // Independent count via the same pass-1 measurement.
    std::size_t measured = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };
    petra::counting_output_iterator counter{cb, &measured};
    try {
        (void)std::vformat_to(std::move(counter),
                              std::format_string<std::string_view>{"hello {}"}.get(),
                              std::make_format_args("world"));
    } catch (...) {}
    CHECK(measured == 11);
}

void section_17_too_large_boundary() {
    SECTION("17.too-large-boundary");
    // For format "v=42" (4 bytes): cap=3 TooLarge, cap=4 truncation
    // edge (r.value() == 3, NUL at cap-1), cap=5 full success
    // (r.value() == 4, NUL at written == 4 < cap == 5).

    char buf3[16];
    for (auto& b : buf3) b = 'Q';
    auto r3 = petra::format_to_n_into_bounded(buf3, 3, "v=42");
    CHECK(!r3.has_value());
    CHECK(r3.error().kind == petra::FormatErrorKind::TooLarge);
    CHECK(r3.error().needed_bytes == 4);

    char buf4[16];
    for (auto& b : buf4) b = 'Q';
    auto r4 = petra::format_to_n_into_bounded(buf4, 4, "v=42");
    CHECK(r4.has_value());
    CHECK(r4.value() == 3);  // cap - 1
    CHECK(buf4[0] == 'v');
    CHECK(buf4[1] == '=');
    CHECK(buf4[2] == '4');
    CHECK(buf4[3] == '\0');  // NUL at cap - 1

    char buf5[16];
    for (auto& b : buf5) b = 'Q';
    auto r5 = petra::format_to_n_into_bounded(buf5, 5, "v=42");
    CHECK(r5.has_value());
    CHECK(r5.value() == 4);
    CHECK(buf5[0] == 'v');
    CHECK(buf5[3] == '2');
    CHECK(buf5[4] == '\0');  // NUL at written == 4 < cap == 5
    CHECK(buf5[5] == 'Q');   // untouched
}

void section_18_4_threads_concurrent() {
    SECTION("18.4-threads");
    std::atomic<int> mismatches{0};
    auto worker = [&](int tid) {
        for (int i = 0; i < 25; ++i) {
            char buf[32] = {};
            auto r = petra::format_to_n_into_bounded(
                buf, sizeof(buf), "t={} i={} v={}",
                tid, i, i * 7);
            if (!r.has_value()) {
                ++mismatches;
                continue;
            }
            std::string expected = "t=" + std::to_string(tid)
                                 + " i=" + std::to_string(i)
                                 + " v=" + std::to_string(i * 7);
            if (std::string_view{buf, r.value()} != expected) {
                ++mismatches;
            }
            if (buf[r.value()] != '\0') {
                ++mismatches;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) threads.emplace_back(worker, t);
    for (auto& th : threads) th.join();
    CHECK(mismatches.load() == 0);
}

}  // namespace

int main() {
    std::println("P-2026-09-11 — format_to_n_into_bounded");
    std::println("(NUL-terminating bounded sibling of Sep 9 / Sep 10)");
    std::println("");

    section_1_sizeof_and_probes();
    section_2_exact_fit_nul_at_written();
    section_3_no_truncation_nul_at_written();
    section_4_truncation_edge_nul_at_cap_minus_one();
    section_5_too_large_buffer_untouched();
    section_6_cap_zero_empty_format();
    section_7_cap_zero_nonempty_format();
    section_8_cap_one_only_nul();
    section_9_format_specifiers();
    section_10_atomicity_on_failure();
    section_11_span_overload_equivalent();
    section_12_subspan_no_bleed();
    section_13_multi_arg_round_trip();
    section_14_utf8_mid_codepoint();
    section_15_embedded_nul_in_arg();
    section_16_byte_count_invariant();
    section_17_too_large_boundary();
    section_18_4_threads_concurrent();

    std::println("");
    std::println("RESULT: {} pass, {} fail", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
