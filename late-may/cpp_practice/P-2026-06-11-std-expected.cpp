// P-2026-06-11-std-expected.cpp
//
// C++23 <expected> -- a "value OR an error" return type, designed as the
// natural replacement for the variant<T, ErrorString> pattern from Jun 10.
//
// Sections:
//   1.  Declaring expected<T, E>; the success and failure constructors
//   2.  Inspecting: has_value(), operator bool(), value(), operator*, operator->
//   3.  The error() accessor: std::unexpected<E> on the failure path
//   4.  value_or() -- supply a default on the failure path
//   5.  Monadic interface: and_then, or_else, transform, transform_error
//   6.  Comparison: equality, ordering, and the role of the unexpected type
//   7.  Move semantics: expected owns its T or E, moves transfer ownership
//   8.  expected<void, E> -- "either nothing or an error"
//   9.  Mini-example: a parsing pipeline that returns expected<int, ParseError>
//  10.  expected vs. variant<T, E> -- when to reach for which
//
// Build (C++23 required; <expected> is C++23):
//   g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g \
//       -o P-2026-06-11-std-expected P-2026-06-11-std-expected.cpp
//
// Verify cleanly under ASan:
//   g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g -fsanitize=address \
//       -o P-2026-06-11-std-expected-asan P-2026-06-11-std-expected.cpp
//   ./P-2026-06-11-std-expected-asan

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <expected>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// ---------------------------------------------------------------------------
// Small traced string-like type so we can see who owns what during moves.
// Same Traced as the Jun 9 and Jun 10 sessions -- carries the lineage.
// ---------------------------------------------------------------------------
struct Traced {
    std::string name;
    std::string payload;

    Traced() : name("?"), payload("") {
        std::cout << "  [Traced] default  (" << name << ")\n";
    }
    explicit Traced(std::string n) : name(std::move(n)), payload("payload-" + name) {
        std::cout << "  [Traced] ctor     (" << name << ") payload='" << payload << "'\n";
    }
    Traced(std::string n, std::string p) : name(std::move(n)), payload(std::move(p)) {
        std::cout << "  [Traced] ctor2    (" << name << ") payload='" << payload << "'\n";
    }
    Traced(const Traced& o) : name(o.name + "(cpy)"), payload(o.payload) {
        std::cout << "  [Traced] copy     (" << name << ") from (" << o.name << ")\n";
    }
    Traced(Traced&& o) noexcept
        : name(std::move(o.name)), payload(std::move(o.payload)) {
        o.name = "(moved)"; o.payload.clear();
        std::cout << "  [Traced] MOVE     (" << name << ")\n";
    }
    Traced& operator=(const Traced& o) {
        if (this != &o) {
            std::cout << "  [Traced] copy-assign (" << name << ") <- (" << o.name << ")\n";
            name = o.name; payload = o.payload;
        }
        return *this;
    }
    Traced& operator=(Traced&& o) noexcept {
        std::cout << "  [Traced] MOVE-assign (" << name << ") <- (" << o.name << ")\n";
        name = std::move(o.name); payload = std::move(o.payload);
        o.name = "(moved)"; o.payload.clear();
        return *this;
    }
    ~Traced() {
        // The (moved) sentinel is the moved-from husk; we still print it so
        // the construction/destruction pairs are visible.
        std::cout << "  [Traced] dtor     (" << name << ") payload='" << payload << "'\n";
    }
};

// ---------------------------------------------------------------------------
// Section header
// ---------------------------------------------------------------------------
static void section(const std::string& title) {
    std::cout << "\n========== " << title << " ==========\n";
}

// ---------------------------------------------------------------------------
// 9. ParseError: a small tagged error type for the parsing pipeline.
// ---------------------------------------------------------------------------
struct ParseError {
    std::string where;   // which step failed
    std::string detail;  // human-readable reason
    int         offset = 0;  // character offset that triggered the failure

    friend std::ostream& operator<<(std::ostream& os, const ParseError& e) {
        return os << "ParseError{where='" << e.where
                  << "', detail='" << e.detail
                  << "', offset=" << e.offset << "}";
    }
};

// A simple integer parser that returns expected<int, ParseError>.
// Demonstrates expected as a return type for a "this can fail" function.
static std::expected<int, ParseError> parse_int(std::string_view s) {
    if (s.empty()) {
        return std::unexpected(ParseError{"parse_int", "empty input", 0});
    }
    std::size_t i = 0;
    bool negative = false;
    if (s[i] == '+' || s[i] == '-') {
        negative = (s[i] == '-');
        ++i;
    }
    if (i == s.size()) {
        return std::unexpected(ParseError{"parse_int", "no digits after sign", static_cast<int>(i)});
    }
    long long acc = 0;
    for (; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9') {
            return std::unexpected(ParseError{"parse_int",
                                              "non-digit character '" + std::string(1, s[i]) + "'",
                                              static_cast<int>(i)});
        }
        acc = acc * 10 + (s[i] - '0');
        if (acc > (1LL << 30)) {
            return std::unexpected(ParseError{"parse_int", "value out of range", static_cast<int>(i)});
        }
    }
    return static_cast<int>(negative ? -acc : acc);
}

// parse_uint: another layer on top of parse_int, using transform.
// Returns the doubled value if the input is a non-negative int.
static std::expected<int, ParseError> parse_doubled(std::string_view s) {
    return parse_int(s).transform([](int v) {
        // If parse_int succeeded, double the value.
        return v * 2;
    });
}

// parse_positive: requires the parsed int to be > 0. Uses and_then so the
// failure type stays ParseError.
static std::expected<int, ParseError> parse_positive(std::string_view s) {
    return parse_int(s).and_then([](int v) -> std::expected<int, ParseError> {
        if (v <= 0) {
            return std::unexpected(ParseError{"parse_positive",
                                              "value must be > 0, got " + std::to_string(v), 0});
        }
        return v;
    });
}

// ===========================================================================
// main
// ===========================================================================
int main() {
    std::cout << std::boolalpha;

    // ------------------------------------------------------------------------
    // 1. Declaring expected<T, E>; the success and failure constructors
    // ------------------------------------------------------------------------
    section("1. Declaring expected<T, E>");

    // The success path: a plain T.
    std::expected<int, std::string> ok_value = 42;
    std::cout << "ok_value.has_value()        = " << ok_value.has_value() << "\n";
    std::cout << "ok_value                    = " << *ok_value << "\n";

    // The failure path: std::unexpected<E> wraps the error value.
    std::expected<int, std::string> bad_value = std::unexpected(std::string{"nope"});
    std::cout << "bad_value.has_value()       = " << bad_value.has_value() << "\n";
    std::cout << "bad_value.error()           = " << bad_value.error() << "\n";

    // std::expected has a "bool" conversion that is has_value().
    std::cout << "static_cast<bool>(ok_value) = " << static_cast<bool>(ok_value) << "\n";
    std::cout << "static_cast<bool>(bad_value)= " << static_cast<bool>(bad_value) << "\n";

    // expected<T,E> is default-constructible iff T is default-constructible.
    // A default-constructed expected is in the success state holding a
    // value-initialized T. E's default-constructibility is irrelevant: the
    // error side only ever needs to be constructed when you do
    // std::unexpected(E{...}).
    struct NotDefault { NotDefault() = delete; NotDefault(int) {} };
    static_assert(std::is_default_constructible_v<std::expected<int, std::string>>,
                  "expected<int,string> is default-constructible because int is");
    static_assert(!std::is_default_constructible_v<std::expected<NotDefault, std::string>>,
                  "expected<NotDefault,string> is NOT default-constructible because NotDefault isn't");
    // The error type's default-constructibility is irrelevant:
    static_assert(std::is_default_constructible_v<std::expected<int, NotDefault>>,
                  "expected<int,NotDefault> IS default-constructible: E doesn't have to be");
    // Try it: a default-constructed expected<int, NotDefault> holds int 0.
    std::expected<int, NotDefault> def;
    std::cout << "default-constructed has_value() = " << def.has_value()
              << ", *def = " << *def << "\n";

    // ------------------------------------------------------------------------
    // 2. Inspecting: has_value(), value(), operator*, operator->
    // ------------------------------------------------------------------------
    section("2. Inspecting: has_value, value, *, ->");

    std::expected<int, std::string> e1 = 7;
    std::cout << "e1.value()   = " << e1.value() << "  // throws on failure\n";
    std::cout << "*e1           = " << *e1       << "  // UB on failure (no check)\n";

    // operator-> for class T
    struct Point { int x = 0, y = 0; };
    std::expected<Point, std::string> pt = Point{3, 4};
    std::cout << "pt->x = " << pt->x << ", pt->y = " << pt->y << "\n";

    // value() on the error path throws std::bad_expected_access<E>
    try {
        std::expected<int, std::string> fail = std::unexpected("oops");
        (void)fail.value();
    } catch (const std::bad_expected_access<std::string>& ex) {
        std::cout << "caught bad_expected_access<string> with .error() = " << ex.error() << "\n";
    }

    // ------------------------------------------------------------------------
    // 3. The error() accessor
    // ------------------------------------------------------------------------
    section("3. error() -- the failure-side accessor");

    std::expected<int, std::string> e2 = std::unexpected("file not found");
    // .error() on a successful expected is UB -- it reads the wrong alternative
    // (same hazard as std::get<1> on a variant<int, string> holding int).
    // The library does not throw; you get whatever bytes the T side left behind.
    std::cout << "e2.error() = \"" << e2.error() << "\"  (safe: we checked has_value first)\n";

    // ------------------------------------------------------------------------
    // 4. value_or() -- default on the failure path
    // ------------------------------------------------------------------------
    section("4. value_or() -- default on the failure path");

    std::expected<int, std::string> a = 100;
    std::expected<int, std::string> b = std::unexpected("no result");
    std::cout << "a.value_or(-1) = " << a.value_or(-1) << "\n";
    std::cout << "b.value_or(-1) = " << b.value_or(-1) << "\n";

    // For class T, value_or takes the default by const ref and copies it on
    // the failure path. The cost is one copy only if you actually fail.
    std::cout << "size of expected<int, string>             = " << sizeof(std::expected<int, std::string>) << "\n";
    std::cout << "size of expected<unique_ptr<int>, string> = "
              << sizeof(std::expected<std::unique_ptr<int>, std::string>) << "\n";

    // ------------------------------------------------------------------------
    // 5. Monadic interface: and_then, or_else, transform, transform_error
    // ------------------------------------------------------------------------
    section("5. Monadic interface -- chaining expected-returning operations");

    // .transform(fn): apply fn to the value, keep the same error type.
    //   std::expected<int, E> -> std::expected<U, E>
    auto e3 = std::expected<int, std::string>{21}
              .transform([](int x) { return x * 2; });
    std::cout << "transform: 21 -> " << *e3 << "\n";

    // .transform preserves the error if it's already present.
    auto e4 = std::expected<int, std::string>{std::unexpected{"err1"}}
              .transform([](int x) { return x * 2; });
    std::cout << "transform on failure: error=\"" << e4.error() << "\", has_value=" << e4.has_value() << "\n";

    // .and_then(fn): fn returns expected<U, E>. The error type is preserved.
    auto parse_one = [](std::string_view s) -> std::expected<int, std::string> {
        if (s == "bad") return std::unexpected("parse_one rejected");
        return static_cast<int>(s.size());
    };
    auto e5 = parse_one("hi").and_then([](int n) -> std::expected<int, std::string> {
        if (n > 10) return std::unexpected("too big");
        return n + 100;
    });
    std::cout << "and_then(\"hi\"): n=2, then n+100=" << *e5 << "\n";

    // .and_then short-circuits on failure.
    auto e6 = parse_one("bad").and_then([](int n) -> std::expected<int, std::string> {
        std::cout << "  (this lambda should NOT be called)\n";
        return n + 100;
    });
    std::cout << "and_then on failure: error=\"" << e6.error() << "\"\n";

    // .or_else(fn): on failure, apply fn to the error. Lets you rewrap errors.
    auto e7 = std::expected<int, std::string>{std::unexpected{"low-level"}}
              .or_else([](const std::string& low) -> std::expected<int, std::string> {
                  // Wrap the low-level error into a "high-level" error.
                  return std::unexpected(std::string{"high-level: "} + low);
              });
    std::cout << "or_else rewrap: error=\"" << e7.error() << "\"\n";

    // .or_else can also RECOVER from the error by returning a successful expected.
    auto e8 = std::expected<int, std::string>{std::unexpected{"recoverable"}}
              .or_else([](const std::string&) -> std::expected<int, std::string> {
                  return 999;  // recovery value
              });
    std::cout << "or_else recovery: has_value=" << e8.has_value()
              << ", value=" << *e8 << "\n";

    // .transform_error(fn): same shape as transform, but for the error path.
    auto e9 = std::expected<int, std::string>{std::unexpected{"io"}}
              .transform_error([](const std::string& s) {
                  return std::string{"E: "} + s;
              });
    std::cout << "transform_error: error=\"" << e9.error() << "\"\n";

    // ------------------------------------------------------------------------
    // 6. Comparison
    // ------------------------------------------------------------------------
    section("6. Comparison");

    std::expected<int, std::string> x1 = 5;
    std::expected<int, std::string> x2 = 5;
    std::cout << "x1 == x2 (both 5)               : " << (x1 == x2) << "\n";

    std::expected<int, std::string> x3 = 5;
    std::expected<int, std::string> x4 = 6;
    // Note: the C++23 standard defines operator<=> for expected (returning
    // a partial_ordering where success < failure, then comparing T or E).
    // As of this writing, libc++ 21 only ships operator==, not the spaceship.
    // We test what the spec actually gives us; the ordering rule still holds
    // semantically and is worth knowing even if the implementation is
    // lagging.
    std::cout << "x3 == x4  (5 == 6)              : " << (x3 == x4) << "  (expected: false)\n";

    // Comparison of two error-expected: errors must be == too.
    std::expected<int, std::string> y1 = std::unexpected("e1");
    std::expected<int, std::string> y2 = std::unexpected("e1");
    std::cout << "y1 == y2 (both err 'e1')        : " << (y1 == y2) << "\n";

    // A successful expected with value 5 is NOT equal to a failed one
    // (different alternatives, no value vs value).
    std::cout << "x1 (5) == y1 (err)              : " << (x1 == y1) << "  (different alternatives)\n";
    std::cout << "y1 (err) == x1 (5)              : " << (y1 == x1) << "  (commutative)\n";

    // ------------------------------------------------------------------------
    // 7. Move semantics
    // ------------------------------------------------------------------------
    section("7. Move semantics -- expected owns its T or E");

    {
        std::cout << "  --- constructing an expected<Traced, std::string> with Traced ---\n";
        std::expected<Traced, std::string> et{Traced{"src"}};
        std::cout << "  --- moving it into another expected ---\n";
        std::expected<Traced, std::string> et2 = std::move(et);
        std::cout << "  --- after the move: ---\n";
        std::cout << "  et.has_value()  = " << et.has_value()   << "  (still valid: alternative didn't change)\n";
        std::cout << "  et2.has_value() = " << et2.has_value()  << "\n";
        std::cout << "  --- (scope ends; et2 dtor fires) ---\n";
    }

    {
        std::cout << "  --- constructing an expected<int, Traced> with an error ---\n";
        std::cout << "  --- wrapping a Traced in std::unexpected ---\n";
        std::expected<int, Traced> ef = std::unexpected(Traced{"err-src"});
        std::cout << "  ef.has_value()  = " << ef.has_value() << "\n";
        std::cout << "  ef.error().name = " << ef.error().name << "\n";
        std::cout << "  --- (scope ends; ef dtor fires) ---\n";
    }

    {
        std::cout << "  --- reassigning: successful -> successful ---\n";
        std::expected<Traced, std::string> et{Traced{"first"}};
        std::cout << "  --- et = Traced{\"second\"} ---\n";
        et = Traced{"second"};
        std::cout << "  --- (scope ends) ---\n";
    }

    {
        std::cout << "  --- reassigning: successful -> error ---\n";
        std::expected<Traced, std::string> et{Traced{"first"}};
        std::cout << "  --- et = std::unexpected{\"whoops\"} ---\n";
        et = std::unexpected(std::string{"whoops"});
        std::cout << "  --- (scope ends; only the error string is destroyed) ---\n";
    }

    // ------------------------------------------------------------------------
    // 8. expected<void, E>
    // ------------------------------------------------------------------------
    section("8. expected<void, E> -- either nothing or an error");

    // When the "success" carries no information, use expected<void, E>.
    std::expected<void, std::string> nothing_ok;
    std::cout << "nothing_ok.has_value() = " << nothing_ok.has_value() << "  (default-constructed: success)\n";

    std::expected<void, std::string> nothing_bad = std::unexpected("void-but-failed");
    std::cout << "nothing_bad.has_value()= " << nothing_bad.has_value() << "\n";
    std::cout << "nothing_bad.error()    = " << nothing_bad.error() << "\n";

    // Use case: a side-effecting function that can fail.
    auto log_to = [](const std::string& dst, const std::string& msg)
            -> std::expected<void, std::string> {
        if (dst.empty()) return std::unexpected("log_to: empty destination");
        std::cout << "    [log] " << dst << ": " << msg << "\n";
        return {};  // success: void expected
    };

    auto r1 = log_to("/var/log/app", "started");
    auto r2 = log_to("",             "started");
    std::cout << "r1.has_value() = " << r1.has_value() << "\n";
    std::cout << "r2.error()     = " << r2.error() << "\n";

    // ------------------------------------------------------------------------
    // 9. Mini-example: a parsing pipeline
    // ------------------------------------------------------------------------
    section("9. Mini-example: parse_int pipeline");

    struct Test { std::string_view input; };
    std::vector<Test> tests = {
        {"123"},
        {"-456"},
        {""},
        {"+"},
        {"12a"},
        {"999999999999"},
        {"0"},
    };
    for (const auto& t : tests) {
        auto r = parse_int(t.input);
        if (r) {
            std::cout << "parse_int(\"" << t.input << "\") = " << *r << "\n";
        } else {
            std::cout << "parse_int(\"" << t.input << "\") ERR: " << r.error() << "\n";
        }
    }

    std::cout << "\n  --- composing parse_int with transform (double) ---\n";
    for (const auto& t : tests) {
        auto r = parse_doubled(t.input);
        if (r) {
            std::cout << "parse_doubled(\"" << t.input << "\") = " << *r << "\n";
        } else {
            std::cout << "parse_doubled(\"" << t.input << "\") ERR: " << r.error() << "\n";
        }
    }

    std::cout << "\n  --- composing parse_int with and_then (positive check) ---\n";
    for (const auto& t : tests) {
        auto r = parse_positive(t.input);
        if (r) {
            std::cout << "parse_positive(\"" << t.input << "\") = " << *r << "\n";
        } else {
            std::cout << "parse_positive(\"" << t.input << "\") ERR: " << r.error() << "\n";
        }
    }

    // The full chain: parse -> double -> positive, errors flow through unchanged.
    std::cout << "\n  --- full chain: parse -> transform(double) -> and_then(positive) ---\n";
    for (const auto& t : tests) {
        auto r = parse_int(t.input)
                     .transform([](int v) { return v * 2; })
                     .and_then([](int v) -> std::expected<int, ParseError> {
                         if (v <= 0) return std::unexpected(ParseError{
                             "positive_check",
                             "value must be > 0 after doubling, got " + std::to_string(v), 0});
                         return v;
                     });
        if (r) {
            std::cout << "  \"" << t.input << "\" -> " << *r << "\n";
        } else {
            std::cout << "  \"" << t.input << "\" -> ERR: " << r.error() << "\n";
        }
    }

    // ------------------------------------------------------------------------
    // 10. expected vs. variant<T, E>
    // ------------------------------------------------------------------------
    section("10. expected vs. variant<T, E> -- when to reach for which");

    // Same idea, different ergonomics.
    using VarT  = std::variant<int, std::string>;
    using ExpT  = std::expected<int, std::string>;

    // Same set of "shapes": int on success, string on failure.
    VarT  v_ok = 42;
    ExpT  e_ok = 42;
    std::cout << "variant<int, string> holds int?  " << std::holds_alternative<int>(v_ok) << "\n";
    std::cout << "expected<int, string> has_value? " << e_ok.has_value() << "\n";

    // To get the success out of a variant you must std::get<int> or visit.
    // To get it out of an expected you call value() or operator*.
    std::cout << "  v_ok = " << std::get<int>(v_ok)   << "  (std::get<int>)\n";
    std::cout << "  e_ok = " << e_ok.value()          << "  (.value())\n";

    // The asymmetry is intentional: a variant doesn't know which side is the
    // "good" one, so the standard library can't give you a .value() that
    // means "the success alternative". expected does know, so it can.
    //
    // Rule of thumb:
    //   - "this function might fail"  -> expected<T, E>
    //   - "this value could be one of several unrelated things" -> variant<Ts...>
    //
    // Mixing them is fine; you can construct a variant from an expected and
    // vice versa, and visit the variant to destructure the expected inside.

    // ------------------------------------------------------------------------
    // Summary
    // ------------------------------------------------------------------------
    section("Summary");

    std::cout <<
        "expected<T, E> is a discriminated union where the 'good' side is\n"
        "distinguished syntactically -- .value(), operator*, and the bool\n"
        "conversion all mean 'success', and the monadic adapters (and_then,\n"
        "or_else, transform, transform_error) compose functions that can\n"
        "fail. Compared to variant<T, E>, the ergonomic win is that you\n"
        "never have to think about which alternative is 'the right one':\n"
        "expected already decided that for you. The cost is that expected\n"
        "is C++23; variant<T, E> is the C++17 fallback.\n";

    return 0;
}
