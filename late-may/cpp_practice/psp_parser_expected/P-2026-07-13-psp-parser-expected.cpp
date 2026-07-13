// P-2026-07-13 — Wire std::expected into psp_span_lib's parser layer.
//
// This lesson closes the loop opened on Jul 12 (std::expected tour) and
// Jul 9 (std::span). The parser is the natural follow-on: a function
// that takes a non-owning view of bytes (psp::Span<const char>) and
// returns either the parsed int or a typed error (std::expected<int,
// ParseError>). It compiles against the psp_span_lib static archive
// that psp_span_lib/CMakeLists.txt builds.
//
// Build prerequisites:
//   1. The psp_span_lib library must already be installed (or built) at
//      /tmp/psp_install. We use that install tree as a find_package
//      consumer. (See P-2026-07-01-cmake-install-find-package for the
//      recipe: `cd psp_span_lib && cmake -S . -B build-release &&
//      cmake --build build-release && cmake --install build-release
//      --prefix /tmp/psp_install`.)
//   2. The toolchain is Apple clang 21 with __cpp_lib_expected == 202211
//      (C++23 final). The `expected` header is in <expected>.
//
// Compile (the lesson program):
//   g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g \
//       -I /tmp/psp_install/include \
//       P-2026-07-13-psp-parser-expected.cpp \
//       -L /tmp/psp_install/lib -lpsp_span_lib \
//       -o P-2026-07-13-psp-parser-expected
//
// Run:
//   ./P-2026-07-13-psp-parser-expected
//
// ASan build:
//   g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g \
//       -fsanitize=address,undefined \
//       -I /tmp/psp_install/include \
//       P-2026-07-13-psp-parser-expected.cpp \
//       -L /tmp/psp_install/lib -lpsp_span_lib \
//       -o P-2026-07-13-psp-parser-expected-asan

#include <psp_span/span.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

// ---------------------------------------------------------------------------
// ParseError — the failure-payload type for our parser.
//
// A plain enum class (no fixed underlying type) has sizeof == sizeof(int)
// == 4 on this toolchain (Apple clang). The std::expected<int, ParseError>
// return value is also 8 bytes, because libc++ packs the discriminator
// (the "has_value" bool) into the unused bytes of the larger of the two
// payloads via empty-base optimization. Section 4 of the Jul 12 lesson
// demonstrated the sizeof math; it composes the same way here.
//
// NOTE: this enum and the formatter specialization are declared at file
// scope (not inside the anonymous namespace below). User specializations
// of std::formatter MUST live in namespace std; if the enum lived in an
// anonymous namespace, the formatter specialization would also need to
// be inside that anonymous namespace, which is not allowed by the
// standard.
// ---------------------------------------------------------------------------
enum class ParseError {
    Empty,        // zero-length input
    LeadingSign,  // starts with '+' or '-'
    NotADigit,    // contains a non-digit character
    Overflow,     // would exceed the value range
};

// std::formatter specialization so std::println / std::format can print
// ParseError values without an ad-hoc ostream operator<<. Inherits the
// string_view formatter for the parse step (handles width, alignment,
// fill); only the `format` member is overridden to look up the name.
template <>
struct std::formatter<ParseError> : std::formatter<std::string_view> {
    auto format(ParseError e, std::format_context& ctx) const {
        std::string_view name = "?";
        switch (e) {
            case ParseError::Empty:       name = "Empty";       break;
            case ParseError::LeadingSign: name = "LeadingSign"; break;
            case ParseError::NotADigit:   name = "NotADigit";   break;
            case ParseError::Overflow:    name = "Overflow";    break;
        }
        return std::formatter<std::string_view>::format(name, ctx);
    }
};

namespace {

// ---------------------------------------------------------------------------
// parse_int — the keystone demo. Takes a psp::Span<const char>, returns
// std::expected<int, ParseError>.
//
// Design choices:
//   - Non-owning input: the caller still owns the buffer; we just borrow.
//     That's the whole point of using psp::Span<const char> here — same
//     span type the rest of the library uses for everything else.
//   - No allocations, no exceptions on the failure path. The error is a
//     tagged enum carried inside the expected return value.
//   - The implementation is `noexcept` because every failure becomes an
//     expected-unexpected, never an exception. The function doesn't
//     allocate, so std::move_if_noexcept (the Jun 12 topic) would never
//     prefer a copy here either way.
//
// This is exactly the shape the psp_span_lib release pipeline wants.
// ---------------------------------------------------------------------------
std::expected<int, ParseError> parse_int(psp::Span<const char> s) noexcept {
    // Case 1: empty input.
    if (s.empty()) {
        return std::unexpected{ParseError::Empty};
    }

    // Case 2: leading sign. The lesson deliberately doesn't accept '+' or
    // '-' so the parser can be used as a building block for an unsigned
    // DSL without a "what about signed?" footgun. (Easy to extend later.)
    if (s.front() == '+' || s.front() == '-') {
        return std::unexpected{ParseError::LeadingSign};
    }

    // Case 3 / 4: digit-by-digit accumulation with overflow guard.
    //
    // std::int64_t is the accumulator type — wider than int — so a single
    // overflow check at the top of each iteration catches the moment we'd
    // overflow int. (For larger ranges, a __builtin_mul_overflow loop
    // would be the right call; for ints, this guard is enough.)
    std::int64_t acc = 0;
    for (char c : s) {
        if (c < '0' || c > '9') {
            return std::unexpected{ParseError::NotADigit};
        }
        acc = acc * 10 + (c - '0');
        if (acc > static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
            return std::unexpected{ParseError::Overflow};
        }
    }
    return static_cast<int>(acc);
}

// ---------------------------------------------------------------------------
// validate_positive — a small predicate step that lifts the parser into
// a domain validator. Returned as std::expected<int, ParseError> so it
// chains via .and_then() with the parser.
//
// This is the function-as-pipeline-step pattern. parse_int and
// validate_positive are independent units; a third function could compose
// them via:
//   auto r = parse_int(s).and_then(validate_positive);
// with no extra plumbing.
// ---------------------------------------------------------------------------
std::expected<int, ParseError> validate_positive(int n) noexcept {
    if (n <= 0) {
        return std::unexpected{ParseError::Overflow};  // reuse: 'non-positive'
    }
    return n;
}

// ---------------------------------------------------------------------------
// double_if_positive — composes parse_int + validate_positive + transform.
//
// Returns std::expected<int, ParseError>. The chain demonstrates:
//   - .and_then(...) short-circuits if parse_int fails
//   - .and_then(...) short-circuits if validate_positive fails
//   - .transform(...) applies a T->T mapping on the success branch
// ---------------------------------------------------------------------------
std::expected<int, ParseError>
double_if_positive(psp::Span<const char> s) noexcept {
    return parse_int(s)
        .and_then(validate_positive)
        .transform([](int n) { return n * 2; });
}

// ---------------------------------------------------------------------------
// Helper: build a psp::Span<const char> from a std::string.
//
// psp::Span doesn't have a std::string ctor (the library was designed
// for C-array / std::array / std::vector / raw (ptr,len) pairs — see
// span.h). To wrap a runtime-sized buffer we use the (ptr, len) ctor,
// which is the most general form. This is the standard pattern for
// bridging std::string -> psp::Span in a parser pipeline.
// ---------------------------------------------------------------------------
psp::Span<const char> as_span(const std::string& s) noexcept {
    return psp::Span<const char>{s.data(), s.size()};
}

// ---------------------------------------------------------------------------
// Section 1: basic parse_int demo
// ---------------------------------------------------------------------------
void section_1_basic_parse() {
    std::cout << "== Section 1: parse_int on a psp::Span<const char> ==\n";

    // Each test case is a separate string. as_span() wraps it in a
    // non-owning view without copying.
    const std::string cases[] = {
        "12345",      // ok: 12345
        "0",          // ok: 0
        "",           // empty
        "+9",         // leading sign
        "12a3",       // not a digit
        "9999999999", // overflow (10 digits > INT_MAX = 2147483647)
    };

    for (const auto& src : cases) {
        auto r = parse_int(as_span(src));
        if (r.has_value()) {
            std::cout << "  parse(\"" << src << "\") = " << *r << '\n';
        } else {
            std::cout << "  parse(\"" << src << "\") error: "
                      << std::format("{}", r.error()) << '\n';
        }
    }
}

// ---------------------------------------------------------------------------
// Section 2: monadic composition with validate_positive + double_if_positive
// ---------------------------------------------------------------------------
void section_2_monadic_compose() {
    std::cout << "\n== Section 2: monadic composition (.and_then / .transform) ==\n";

    const std::string cases[] = {
        "50",    // ok: doubled to 100
        "-1",    // LeadingSign (rejected before validate_positive sees it)
        "abc",   // NotADigit
        "0",     // Overflow (validate_positive rejects n<=0)
        "100",   // ok: doubled to 200
    };

    for (const auto& src : cases) {
        auto r = double_if_positive(as_span(src));
        if (r.has_value()) {
            std::cout << "  double(\"" << src << "\") = " << *r << '\n';
        } else {
            std::cout << "  double(\"" << src << "\") error: "
                      << std::format("{}", r.error()) << '\n';
        }
    }
}

// ---------------------------------------------------------------------------
// Section 3: error formatting + std::format integration
// ---------------------------------------------------------------------------
void section_3_error_format() {
    std::cout << "\n== Section 3: error formatting ==\n";

    // std::println goes through std::format, which dispatches via
    // std::formatter<T>. The ParseError formatter (declared at the top
    // of this file) is what makes the {} placeholder work.
    auto r = parse_int(as_span(std::string{"12x34"}));
    if (!r.has_value()) {
        std::cout << "  formatted error: " << std::format("{}", r.error())
                  << '\n';
    }

    // Compose with .and_then so we can see how a chain reports the first
    // failing step. The failure here is in parse_int (LeadingSign),
    // not in validate_positive — the chain short-circuits before
    // validate_positive ever runs.
    auto chained = parse_int(as_span(std::string{"-1"}))
                       .and_then(validate_positive);
    if (!chained.has_value()) {
        std::cout << "  chained error: "
                  << std::format("{}", chained.error()) << '\n';
    }
}

// ---------------------------------------------------------------------------
// Section 4: comparison operators on expected
//
// std::expected<T, E> has == and != (the libc++ I'm on does NOT provide
// < or <=>). The interesting comparison cases are:
//   - expected<T, E> == T          (matches when in success state with that T)
//   - expected<T, E> == unexpected<E>  (matches when in failure state with that E)
//   - expected<T, E> == expected<T, E>  (state + payload match)
// Cross-comparisons (expected == T on a failure state) are deleted by
// design.
// ---------------------------------------------------------------------------
void section_4_comparisons() {
    std::cout << "\n== Section 4: comparison operators ==\n";

    auto ok  = parse_int(as_span(std::string{"42"}));
    auto bad = parse_int(as_span(std::string{""}));

    // ok == 42:    true (success state, value 42)
    // ok != 99:    true
    // bad == unexpected{Empty}: true
    // bad == unexpected{Overflow}: false
    std::cout << "  ok == 42: " << (ok == 42) << '\n';
    std::cout << "  ok != 99: " << (ok != 99) << '\n';
    std::cout << "  bad == unexpected<Empty>: "
              << (bad == std::unexpected{ParseError::Empty}) << '\n';
    std::cout << "  bad == unexpected<Overflow>: "
              << (bad == std::unexpected{ParseError::Overflow}) << '\n';

    // Two expecteds equal each other iff both state and payload match.
    auto also_ok = parse_int(as_span(std::string{"42"}));
    std::cout << "  ok == also_ok: " << (ok == also_ok) << '\n';

    auto also_bad = parse_int(as_span(std::string{"+9"}));
    std::cout << "  bad == also_bad (different errors): "
              << (bad == also_bad) << '\n';
}

// ---------------------------------------------------------------------------
// Section 5: the psp_span_lib integration check
//
// We verify two things:
//   1. The psp::Span<const char> we built at runtime actually points at
//      the same buffer as the std::string source (no copy).
//   2. The std::expected<int, ParseError> return value has the expected
//      sizeof (8 bytes on this toolchain: int(4) + ParseError(4) with
//      the bool discriminator packed into alignment slack; matches the
//      Jul 12 measurement).
// ---------------------------------------------------------------------------
void section_5_library_integration() {
    std::cout << "\n== Section 5: psp_span_lib integration ==\n";

    const std::string src = "hello 12345 world";
    psp::Span<const char> view{src.data(), src.size()};

    // Subview: borrow the "12345" middle. .subspan(6, 5) starts at byte 6
    // and takes 5 bytes — non-owning, no allocation. Pointers go into the
    // std::string buffer directly.
    auto digits = view.subspan(6, 5);
    auto r = parse_int(digits);

    // Address check: digits.data() must equal src.data() + 6.
    std::cout << "  digits.data() == src.data()+6: "
              << (digits.data() == src.data() + 6) << '\n';
    std::cout << "  digits.size_bytes() = " << digits.size_bytes()
              << " (expected 5)\n";

    if (r.has_value()) {
        std::cout << "  parse(digits) = " << *r << '\n';
    }

    // Sizeof probes — the runtime cost of the return type.
    std::cout << "  sizeof(expected<int, ParseError>) = "
              << sizeof(std::expected<int, ParseError>) << '\n';
    std::cout << "  sizeof(ParseError) = "
              << sizeof(ParseError) << '\n';

    // Toolchain check: __cpp_lib_expected == 202211 means C++23 final.
    std::cout << "  __cpp_lib_expected = "
#ifdef __cpp_lib_expected
              << __cpp_lib_expected
#else
              << "undefined"
#endif
              << '\n';
}

// ---------------------------------------------------------------------------
// Section 6: composing with std::optional (a forward-looking note)
//
// std::expected doesn't implicitly convert to std::optional, but the
// .transform_error() method can collapse the failure payload into a
// sentinel:
//   expected<int, E>.transform_error([](E){return -1;}) -> expected<int, int>
//
// For a "give me the value or a sentinel" pipeline, this is the cleanest
// way to bridge the two types without a manual if/else at every call site.
// ---------------------------------------------------------------------------
void section_6_optional_bridge() {
    std::cout << "\n== Section 6: bridging to std::optional / sentinel ==\n";

    auto r = parse_int(as_span(std::string{""}));

    // .transform_error replaces the failure payload. Here we collapse
    // every error into the int sentinel -1.
    auto with_sentinel = r.transform_error(
        [](ParseError) -> int { return -1; });
    std::cout << "  with_sentinel = " << *with_sentinel
              << " (sentinel for any error)\n";

    // std::optional<int> view of the same expected.
    std::optional<int> opt = r.has_value() ? std::optional<int>{*r}
                                           : std::nullopt;
    std::cout << "  std::optional view has_value: "
              << opt.has_value() << '\n';
}

}  // namespace

int main() {
    section_1_basic_parse();
    section_2_monadic_compose();
    section_3_error_format();
    section_4_comparisons();
    section_5_library_integration();
    section_6_optional_bridge();
    return 0;
}
