// P-2026-07-12 — std::expected<T, E> (C++23) — error handling without exceptions
//
// A tour of std::expected covering:
//   1. Basic construction (T and unexpected<E>)
//   2. Predicates: has_value(), operator bool()
//   3. Access: .value() (throws bad_expected_access), .value_or(), operator*, operator->
//   4. Comparison operators (==, !=, <, etc.) with mixed T/E types
//   5. Monadic ops: and_then, or_else, transform, transform_error
//   6. Composition through chains
//   7. Comparison with alternative designs (throwing, pair<bool,T>, std::variant)
//   8. A small realistic use-case: a span-based int parser that returns expected
//
// Build: g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g -o P-2026-07-12-std-expected-cpp23 P-2026-07-12-std-expected-cpp23.cpp
// Build (ASan): g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g -fsanitize=address,undefined -o P-2026-07-12-std-expected-cpp23-asan P-2026-07-12-std-expected-cpp23.cpp

#include <cassert>
#include <cstdint>
#include <expected>
#include <iostream>
#include <print>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

// ============================================================================
// Section 1 — Construction
// ============================================================================
//
// std::expected<T, E> holds either a T ("success") or an E ("failure").
// The default-constructed T or E must be well-formed.

enum class ParseError {
    Empty,
    NotANumber,
    OutOfRange,
    LeadingSign,
};

std::ostream& operator<<(std::ostream& os, ParseError e) {
    switch (e) {
        case ParseError::Empty:        return os << "Empty";
        case ParseError::NotANumber:   return os << "NotANumber";
        case ParseError::OutOfRange:   return os << "OutOfRange";
        case ParseError::LeadingSign:  return os << "LeadingSign";
    }
    return os << "?";
}

// std::print uses std::format, which uses std::formatter<T>. We specialize
// formatter<ParseError> so std::println("{}", parse_err) works.
template <>
struct std::formatter<ParseError> : std::formatter<std::string_view> {
    auto format(ParseError e, format_context& ctx) const {
        std::string_view name = "?";
        switch (e) {
            case ParseError::Empty:       name = "Empty";       break;
            case ParseError::NotANumber:  name = "NotANumber";  break;
            case ParseError::OutOfRange:  name = "OutOfRange";  break;
            case ParseError::LeadingSign: name = "LeadingSign"; break;
        }
        return std::formatter<std::string_view>::format(name, ctx);
    }
};

void section1_construction() {
    std::println("== Section 1: construction ==");

    // (a) success: implicit from T (or std::in_place, std::in_place_type<T>).
    std::expected<int, ParseError> ok1 = 42;
    std::expected<int, ParseError> ok2{std::in_place, 7 * 6};

    // (b) failure: wrap E in std::unexpected.
    std::expected<int, ParseError> bad1 = std::unexpected{ParseError::Empty};

    // (c) failure from a value (no implicit conversion; must go through unexpected).
    auto bad2 = std::expected<int, ParseError>{std::unexpect, ParseError::OutOfRange};

    // (d) Default-construct: only when T is default-constructible and the
    //     failure type is not. expected<T, E> default-constructs to T.
    std::expected<int, ParseError> def{};  // holds 0
    std::println("def = {} (default-constructed T; .has_value() = {})", *def, def.has_value());

    std::println("ok1 = {}  ok2 = {}", *ok1, *ok2);
    std::println("bad1 has_value = {}  bad2 has_value = {}", bad1.has_value(), bad2.has_value());

    // (e) error() accesses the failure payload.
    if (!bad1) std::println("bad1.error() = {}", bad1.error());
    if (!bad2) std::println("bad2.error() = {}", bad2.error());
}

// ============================================================================
// Section 2 — Predicates and access
// ============================================================================

void section2_predicates_and_access() {
    std::println("\n== Section 2: predicates and access ==");

    std::expected<int, ParseError> ok = 100;
    std::expected<int, ParseError> bad = std::unexpected{ParseError::NotANumber};

    // has_value() and operator bool() are equivalent.
    std::println("ok.has_value() = {}  ok.operator bool() = {}",
                 ok.has_value(), static_cast<bool>(ok));
    std::println("bad.has_value() = {}  bad.operator bool() = {}",
                 bad.has_value(), static_cast<bool>(bad));

    // operator* and operator-> give direct access. Precondition: has_value() is true.
    std::expected<std::string, ParseError> s = std::string{"hello"};
    std::println("s->size() = {}", s->size());

    // .value() throws std::bad_expected_access<E> on failure.
    try {
        int v = ok.value();
        std::println("ok.value() = {}", v);
        // The next line throws:
        (void) bad.value();
    } catch (const std::bad_expected_access<ParseError>& e) {
        std::println("caught bad_expected_access<ParseError>: error() = {}", e.error());
    }

    // .value_or(default) returns the T on success, default on failure. Never throws.
    std::println("ok.value_or(-1)  = {}", ok.value_or(-1));
    std::println("bad.value_or(-1) = {}", bad.value_or(-1));

    // .error() returns the E on failure, UB on success. Never throws.
    if (!bad) std::println("bad.error() = {}", bad.error());
}

// ============================================================================
// Section 3 — Monadic operations
// ============================================================================
//
// and_then(f): T -> expected<U, E>     // chains fallible transformations
// or_else(f):  E -> expected<T, F>     // chains fallible error handlers
// transform(f):   T -> U               // chains infallible transformations
// transform_error(f): E -> F           // chains infallible error mappers
//
// These take expected<T,E> and return expected<U,F> (or expected<T,F>).
// They don't run the function unless the expected is in the right state.

void section3_monadic() {
    std::println("\n== Section 3: monadic operations ==");

    // transform: T -> U (infallible)
    std::expected<int, ParseError> ok = 10;
    auto t1 = ok.transform([](int n) { return n * 2; });
    std::println("ok.transform(x2) = {} (type: int)", *t1);

    // and_then: T -> expected<U, E> (fallible; same E)
    std::expected<int, ParseError> bad = std::unexpected{ParseError::Empty};
    auto t2 = ok.and_then([](int n) -> std::expected<int, ParseError> {
        if (n > 5) return n * 10;
        return std::unexpected{ParseError::OutOfRange};
    });
    auto t3 = bad.and_then([](int n) -> std::expected<int, ParseError> {
        std::println("  and_then ran (should NOT happen)");
        return n;
    });
    std::println("ok.and_then(>5)  = {}", *t2);
    std::println("bad.and_then(...) = {} (and_then was skipped)", t3.error());

    // or_else: E -> expected<T, F> (recover or rewrap)
    auto t4 = bad.or_else([](ParseError e) -> std::expected<int, std::string> {
        if (e == ParseError::Empty) return -1;
        return std::unexpected{std::string{"rejected: "} + "?"};
    });
    std::println("bad.or_else(Empty->-1) = {}", *t4);

    // transform_error: E -> F (re-wrap the error in a richer type)
    auto t5 = bad.transform_error([](ParseError e) {
        return std::string{"parse failure: "} + (e == ParseError::Empty ? "empty" : "other");
    });
    std::println("bad.transform_error -> string = {}", t5.error());

    // The expected from a fallible T->expected<U, E> function still chains
    // through and_then. Note: when an early step fails, the chain short-
    // circuits; calling *result on a failure-state expected is UB, so we
    // always check has_value() first.
    std::expected<int, std::string> numbered = 20;  // even
    auto chained = numbered
        .and_then([](int n) -> std::expected<int, std::string> {
            if (n % 2 != 0) return std::unexpected{std::string{"odd input"}};
            return n * 2;
        })
        .and_then([](int n) -> std::expected<int, std::string> {
            return n + 1;  // any int is fine
        });
    if (chained) std::println("chained and_then = {} (20 -> *2 -> +1)", *chained);
    else         std::println("chained and_then error = {}", chained.error());

    // 21 is odd, so the first and_then fails and short-circuits.
    std::expected<int, std::string> odd = 21;
    auto odd_chained = odd
        .and_then([](int n) -> std::expected<int, std::string> {
            if (n % 2 != 0) return std::unexpected{std::string{"odd input"}};
            return n * 2;
        })
        .and_then([](int n) -> std::expected<int, std::string> {
            std::println("  this and_then should NOT run");
            return n + 1;
        });
    if (odd_chained) std::println("odd_chained (unexpectedly) = {}", *odd_chained);
    else             std::println("odd_chained error = {} (short-circuit on first step)",
                                   odd_chained.error());

    // The same chain on a bad starting expected is short-circuited.
    std::expected<int, std::string> numbered_bad = std::unexpected{"upstream"};
    auto chained_bad = numbered_bad
        .and_then([](int) -> std::expected<int, std::string> {
            std::println("  this and_then should NOT run (upstream failed)");
            return 0;
        });
    if (chained_bad) std::println("chained_bad (unexpectedly) = {}", *chained_bad);
    else             std::println("chained_bad (short-circuits) error = {}",
                                   chained_bad.error());
}

// ============================================================================
// Section 4 — Comparison operators
// ============================================================================

void section4_comparisons() {
    std::println("\n== Section 4: comparisons ==");

    std::expected<int, std::string> a = 10;
    std::expected<int, std::string> b = 20;
    std::expected<int, std::string> c = std::unexpected{"oops"};

    // expected<T, E> == expected<T, E>: T==T, E==E
    std::println("a == b : {}", a == b);
    std::println("a == 10: {}", a == 10);          // expected<T,E> == T
    // c is in error state, so it's comparable to unexpected<E>, not E:
    std::println("c == unexpected<str>: {}", c == std::unexpected{std::string{"oops"}});
    std::println("c == unexpected<str>: {}", c == std::unexpected{std::string{"nope"}});

    // Inequality between expecteds with different payload types
    std::expected<int, std::string> d = std::unexpected{"different"};
    std::println("c == d : {} (errors differ)", c == d);
    std::println("c != d : {}", c != d);

    // Ordering: std::expected provides == and !=, but does NOT define < / <=>.
    // (You can wrap with std::tie() to get lexicographic ordering if you need it.)
    // For comparison purposes, a value-state expected compares to T; an
    // error-state expected compares to unexpected<E>.
    std::expected<int, std::string> small = 1;
    std::println("small == 1                       : {}", small == 1);
    // To test "in error state with a given error payload":
    std::expected<int, std::string> small_err = std::unexpected{std::string{"nope"}};
    std::println("small_err == unexpected<nope>   : {}",
                 small_err == std::unexpected{std::string{"nope"}});
    std::println("small_err == unexpected<other>  : {}",
                 small_err == std::unexpected{std::string{"other"}});
}

// ============================================================================
// Section 5 — A small realistic use-case: span-based int parser
// ============================================================================
//
// psp_span_lib exposes a non-owning view (std::span) into the input. The
// typical consumer wants to parse a span<byte> (or span<const char>) into
// a typed value without exceptions. std::expected is the right return type.

std::expected<int, ParseError> parse_int_from_span(std::span<const char> s) {
    if (s.empty()) return std::unexpected{ParseError::Empty};
    if (s.front() == '+' || s.front() == '-') return std::unexpected{ParseError::LeadingSign};
    int v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return std::unexpected{ParseError::NotANumber};
        v = v * 10 + (c - '0');
        if (v > 1'000'000) return std::unexpected{ParseError::OutOfRange};
    }
    return v;
}

void section5_use_case() {
    std::println("\n== Section 5: span-based int parser using expected ==");

    auto try_parse = [](std::string_view literal) {
        // Construct a span<const char> from a string_view (the span doesn't own).
        std::span<const char> sp{literal.data(), literal.size()};
        auto r = parse_int_from_span(sp);
        if (r) std::println("parse(\"{}\") = {}", literal, *r);
        else   std::println("parse(\"{}\") error: {}", literal, r.error());
    };

    try_parse("12345");
    try_parse("");
    try_parse("+9");
    try_parse("12a3");
    try_parse("9999999");  // > 1,000,000

    // Composition with and_then — "double if positive"
    auto with_compose = [](std::string_view literal) {
        std::span<const char> sp{literal.data(), literal.size()};
        auto r = parse_int_from_span(sp)
                     .and_then([](int n) -> std::expected<int, ParseError> {
                         if (n < 0) return std::unexpected{ParseError::OutOfRange};
                         return n * 2;
                     });
        if (r) std::println("compose(\"{}\") = {}", literal, *r);
        else   std::println("compose(\"{}\") error: {}", literal, r.error());
    };
    with_compose("50");
    with_compose("abc");
}

// ============================================================================
// Section 6 — Comparison with alternative designs
// ============================================================================

void section6_alternatives() {
    std::println("\n== Section 6: alternatives to std::expected ==");

    // (a) Throwing: hides control flow, can't be ignored, slow on the failure
    //     path, and bad for low-level/library APIs. The psp_span_lib
    //     parsing example above would have to throw for the same set of
    //     conditions; the caller now has to know to wrap in try/catch.

    // (b) pair<bool, T> (or optional<T> + a separate error code):
    //     - optional<T> throws away the error payload (no way to say WHY)
    //     - pair<bool, T>: T must be default-constructible; no monadic ops;
    //       the bool is redundant with "did we set T?".
    auto from_pair = []() -> std::pair<bool, int> {
        return {false, 0};
    };
    auto [ok, val] = from_pair();
    if (!ok) std::println("pair<bool,T>: failed (no error info)");

    // (c) std::variant<T, E> with visit:
    //     - needs visit/overloaded boilerplate for every consumer
    //     - has_value() / error() / and_then() don't exist; you write them yourself
    //     - ordering rules differ (variant is tag-ordered)
    std::variant<int, ParseError> v = ParseError::Empty;
    std::visit([](auto&& x) {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, int>) std::println("variant holds int: {}", x);
        else                                  std::println("variant holds error: {}", x);
    }, v);

    // (d) C++23 std::expected: T + E, monadic, exceptions only on .value(),
    //     clear naming (value/error/has_value/transform/and_then/...).
    //     The "default-constructed T" semantic and "T<E>==T and expected==T"
    //     rules make it composable in regular expressions.
    std::println("expected picks: T + E payload, monadic ops, exceptions only on demand.");
}

// ============================================================================
// Section 7 — Interop with optional, and a few corner cases
// ============================================================================

void section7_corner_cases() {
    std::println("\n== Section 7: corner cases ==");

    // expected<T, E> when T == E: still works; the disambiguator is the
    // value category (lvalue/rvalue and presence of unexpected).
    std::expected<int, int> dual{std::in_place, 5};
    std::expected<int, int> dual_err{std::unexpect, -1};
    std::println("dual = {}  dual_err error = {}", *dual, dual_err.error());

    // expected<void, E>: no success payload. Useful for "operation that
    // either succeeded or failed with a reason".
    std::expected<void, std::string> op_result = std::unexpected{"disk full"};
    if (!op_result) std::println("op failed: {}", op_result.error());
    op_result.emplace();  // marks as success (no payload)
    std::println("op_result.has_value() after emplace = {}", op_result.has_value());

    // .error() returns a reference. Calling it on a value-state expected
    // is UB (just like optional's value()). Don't do this:
    //   std::expected<int, std::string> good = 1;
    //   std::println("{}", good.error());  // UB: good is in value state

    // Equality with unexpected: a value-state expected compares to T; an
    // error-state expected compares to unexpected<E>. They don't mix.
    std::expected<int, std::string> x = 1;
    std::expected<int, std::string> y = std::unexpected{"nope"};
    std::println("x == 1                 : {} (value == T)", x == 1);
    std::println("y == unexpected<str>   : {} (error == unexpected<E>)",
                 y == std::unexpected{std::string{"nope"}});
}

// ============================================================================
// main
// ============================================================================

int main() {
    section1_construction();
    section2_predicates_and_access();
    section3_monadic();
    section4_comparisons();
    section5_use_case();
    section6_alternatives();
    section7_corner_cases();

    std::println("\n== sizeof checks (compile-time) ==");
    std::expected<int, std::string> e1 = 1;
    std::expected<int, ParseError>  e2 = 1;
    std::expected<std::string, int>  e3 = std::string{"x"};
    std::println("sizeof(expected<int,std::string>)    = {}", sizeof(e1));
    std::println("sizeof(expected<int,ParseError>)    = {}", sizeof(e2));
    std::println("sizeof(expected<std::string,int>)    = {}", sizeof(e3));

    // Compile-time proof: __cpp_lib_expected is defined.
#ifdef __cpp_lib_expected
    std::println("__cpp_lib_expected = {}", __cpp_lib_expected);
#endif

    return 0;
}
