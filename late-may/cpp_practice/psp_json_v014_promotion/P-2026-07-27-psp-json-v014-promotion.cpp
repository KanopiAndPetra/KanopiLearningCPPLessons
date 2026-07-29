// P-2026-07-27-psp-json-v014-promotion.cpp
//
// Companion to P-2026-07-25-psp-json-negative-numbers.cpp. The Jul 25
// consumer proved the v0.14.0 design end-to-end using LOCAL COPIES of
// the four numeric parsers in a `v014` namespace, exercising the
// v0.13.0 library. TODAY we use the LIBRARY PROPER (psp_span_lib
// v0.14.0) and verify that the four parsers now accept a leading
// '+' or '-', that parse_int returns std::int64_t (widened from int),
// and that the JSON pipeline + JSON Patch engine handle negative +
// INT64_MAX-shaped values cleanly through the LIBRARY surface (no
// shadow dispatcher).
//
// Layout:
//   Section 1 — v0.14.0 sign acceptance through the library proper
//                (parse_int / parse_double / parse_int_at /
//                parse_double_at / parse_value_at).
//   Section 2 — parse_int now returns std::int64_t; verifies the
//                widened range (INT64_MAX / INT64_MIN+1) parses
//                cleanly.
//   Section 3 — round-trip parse_value_at -> json_to_string ->
//                parse_value_at -> == for negative + INT64_MAX
//                values through the LIBRARY (no shadow dispatcher).
//   Section 4 — through psp::json_patch::patch (TestOp equality):
//                a TestOp with INT64_MAX matches a hand-built
//                INT64_MAX target; a TestOp with -42 matches a
//                hand-built -42 target; failure case (BIG-1)
//                correctly reports TestValueMismatch.
//   Section 5 — wire-format round-trip via the library's
//                psp::json_patch::parse_patch_document +
//                psp::json_patch::serialise_patch_document
//                (the Jul 23 wire-format parser + the Jul 24
//                consumer writer, both now exercised by the
//                library proper with negative + INT64_MAX values).
//
// Section totals: 5 sections, 54 cases, all passing.
//
// Build requires psp_span_lib v0.14.0 installed.

#include <psp_span/json.h>
#include <psp_span/json_ext.h>

#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

int g_pass = 0;
int g_fail = 0;

void check(bool cond, std::string_view label) {
    if (cond) {
        ++g_pass;
        std::println("  PASS: {}", label);
    } else {
        ++g_fail;
        std::println("  FAIL: {}", label);
    }
}

void header(std::string_view title) {
    std::println("");
    std::println("== {} ==", title);
}

// ===========================================================================
// build helpers
// ===========================================================================
psp::JsonValue make_int(std::int64_t v) {
    psp::JsonValue j;
    j.value = v;
    return j;
}

psp::JsonValue make_str(std::string s) {
    psp::JsonValue j;
    j.value = std::move(s);
    return j;
}

psp::JsonValue make_obj() {
    psp::JsonValue j;
    j.value.emplace<std::map<std::string, psp::JsonValue>>();
    return j;
}

void obj_set(psp::JsonValue& j, std::string key, psp::JsonValue v) {
    auto& m = std::get<std::map<std::string, psp::JsonValue>>(j.value);
    m.emplace(std::move(key), std::move(v));
}

psp::JsonValue make_arr() {
    psp::JsonValue j;
    j.value.emplace<std::vector<psp::JsonValue>>();
    return j;
}

void arr_push(psp::JsonValue& j, psp::JsonValue v) {
    auto& a = std::get<std::vector<psp::JsonValue>>(j.value);
    a.push_back(std::move(v));
}

// make_double helper was unused under strict warnings (the consumer
// doesn't construct doubles directly — they only flow through
// parse_value_at / parse_double / parse_double_at for round-trip
// testing). Removed; reintroduce if a future test section needs
// to construct JsonValue{double} inline.

// ===========================================================================
// op_writer — RFC 6902 §3 wire-format writer (mirror of the Jul 24
// consumer writer, re-inlined here so today's TU is self-contained).
// ===========================================================================
struct op_writer {
    static psp::JsonValue serialise_one_op(JsonPatchOp op) {
        psp::JsonValue out = make_obj();
        std::string op_name;
        std::visit([&](auto&& o) {
            using T = std::decay_t<decltype(o)>;
            if constexpr (std::is_same_v<T, AddOp>) {
                op_name = "add";
                obj_set(out, "path",  make_str(std::string(o.path)));
                obj_set(out, "value", o.value);
            } else if constexpr (std::is_same_v<T, RemoveOp>) {
                op_name = "remove";
                obj_set(out, "path", make_str(std::string(o.path)));
            } else if constexpr (std::is_same_v<T, ReplaceOp>) {
                op_name = "replace";
                obj_set(out, "path",  make_str(std::string(o.path)));
                obj_set(out, "value", o.value);
            } else if constexpr (std::is_same_v<T, MoveOp>) {
                op_name = "move";
                obj_set(out, "path", make_str(std::string(o.path)));
                obj_set(out, "from", make_str(std::string(o.from)));
            } else if constexpr (std::is_same_v<T, CopyOp>) {
                op_name = "copy";
                obj_set(out, "path", make_str(std::string(o.path)));
                obj_set(out, "from", make_str(std::string(o.from)));
            } else if constexpr (std::is_same_v<T, TestOp>) {
                op_name = "test";
                obj_set(out, "path",  make_str(std::string(o.path)));
                obj_set(out, "value", o.value);
            }
        }, op.data);
        obj_set(out, "op", make_str(op_name));
        return out;
    }

    static std::string serialise_patch_document(const std::vector<JsonPatchOp>& ops) {
        psp::JsonValue arr = make_arr();
        for (const auto& op : ops) arr_push(arr, serialise_one_op(op));
        return psp::json_to_string(arr);
    }
};

// ===========================================================================
// Section 1 — v0.14.0 sign acceptance through the library proper
// ===========================================================================
void section_1_sign_acceptance() {
    header("Section 1: v0.14.0 sign acceptance through the library proper");

    // parse_int: negative ints (was LeadingSign in v0.13.0).
    // NB: build a std::string then take a (data, size) Span so the
    // span doesn't include the trailing '\0'. The whole-span
    // parser correctly rejects non-digit trailing data, so passing
    // a c-string literal would falsely report NotADigit.
    {
        std::string buf = "-42";
        auto r = psp::parse_int(psp::Span<const char>{buf.data(), buf.size()});
        check(r.has_value() && *r == -42,
              "1a parse_int(\"-42\") = -42 (was LeadingSign in v0.13.0)");
    }
    {
        std::string buf = "-2000000000";
        auto r = psp::parse_int(psp::Span<const char>{buf.data(), buf.size()});
        check(r.has_value() && *r == -2000000000LL,
              "1b parse_int(\"-2000000000\") = -2000000000");
    }
    {
        std::string buf = "-9223372036854775807";
        auto r = psp::parse_int(psp::Span<const char>{buf.data(), buf.size()});
        check(r.has_value() && *r == -9223372036854775807LL,
              "1c parse_int(\"-9223372036854775807\") = INT64_MIN+1");
    }

    // parse_int: positive with explicit '+'.
    {
        std::string buf = "+100";
        auto r = psp::parse_int(psp::Span<const char>{buf.data(), buf.size()});
        check(r.has_value() && *r == 100, "1d parse_int(\"+100\") = 100");
    }
    {
        std::string buf = "+0";
        auto r = psp::parse_int(psp::Span<const char>{buf.data(), buf.size()});
        check(r.has_value() && *r == 0, "1e parse_int(\"+0\") = 0");
    }

    // parse_int: bare sign is NotADigit (was LeadingSign).
    {
        std::string buf = "+";
        auto r = psp::parse_int(psp::Span<const char>{buf.data(), buf.size()});
        check(!r && r.error() == ParseError::NotADigit,
              "1f parse_int(\"+\") -> NotADigit");
    }
    {
        std::string buf = "-";
        auto r = psp::parse_int(psp::Span<const char>{buf.data(), buf.size()});
        check(!r && r.error() == ParseError::NotADigit,
              "1g parse_int(\"-\") -> NotADigit");
    }

    // parse_double: negative doubles (was LeadingSign in v0.13.0).
    {
        std::string buf = "-3.14";
        auto r = psp::parse_double(psp::Span<const char>{buf.data(), buf.size()});
        check(r.has_value() && std::abs(*r - (-3.14)) < 1e-9,
              "1h parse_double(\"-3.14\") = -3.14");
    }
    {
        std::string buf = "-2.5e-10";
        auto r = psp::parse_double(psp::Span<const char>{buf.data(), buf.size()});
        check(r.has_value() && std::abs(*r - (-2.5e-10)) < 1e-15,
              "1i parse_double(\"-2.5e-10\") = -2.5e-10");
    }
    {
        std::string buf = "-0";
        auto r = psp::parse_double(psp::Span<const char>{buf.data(), buf.size()});
        check(r.has_value() && *r == 0.0,
              "1j parse_double(\"-0\") = 0 (magnitude)");
    }

    // parse_double: positive with explicit '+'.
    {
        std::string buf = "+3.14";
        auto r = psp::parse_double(psp::Span<const char>{buf.data(), buf.size()});
        check(r.has_value() && std::abs(*r - 3.14) < 1e-9,
              "1k parse_double(\"+3.14\") = 3.14");
    }

    // parse_int_at: cursor variant accepts sign too. Build the
    // cursor over a std::string so the span does NOT include the
    // trailing '\0' (the literal would push size up by 1).
    {
        std::string buf = "-42,99";
        psp::Span<const char> s{buf.data(), buf.size()};
        auto r = psp::parse_int_at(s);
        check(r.has_value() && *r == -42,
              "1l parse_int_at(\"-42,99\") = -42");
        check(s.size() == 3 && s[0] == ',',
              "  1l span advanced past \"-42\"");
    }
    {
        std::string buf = "+100 rest";
        psp::Span<const char> s{buf.data(), buf.size()};
        auto r = psp::parse_int_at(s);
        check(r.has_value() && *r == 100,
              "1m parse_int_at(\"+100 rest\") = 100");
        check(s.size() == 5 && s[0] == ' ',
              "  1m span advanced past \"+100\"");
    }

    // parse_double_at: cursor variant accepts sign too.
    {
        std::string buf = "-3.14;";
        psp::Span<const char> s{buf.data(), buf.size()};
        auto r = psp::parse_double_at(s);
        check(r.has_value() && std::abs(*r - (-3.14)) < 1e-9,
              "1n parse_double_at(\"-3.14;\") = -3.14");
        check(s.size() == 1 && s[0] == ';',
              "  1n span advanced past \"-3.14\"");
    }
}

// ===========================================================================
// Section 2 — parse_int returns std::int64_t; widened range verified
// ===========================================================================
void section_2_widened_return_type() {
    header("Section 2: parse_int returns std::int64_t (widened from int in v0.13.0)");

    // Runtime check: values larger than INT_MAX parse cleanly.
    {
        std::string buf = "5000000000";
        auto r = psp::parse_int(psp::Span<const char>{buf.data(), buf.size()});
        check(r.has_value() && *r == 5000000000LL,
              "2a parse_int(\"5000000000\") past INT_MAX (was Overflow in v0.13.0)");
    }
    {
        std::string buf = "9223372036854775807";
        auto r = psp::parse_int(psp::Span<const char>{buf.data(), buf.size()});
        check(r.has_value() && *r == std::numeric_limits<std::int64_t>::max(),
              "2b parse_int(\"9223372036854775807\") = INT64_MAX");
    }
    {
        std::string buf = "-9223372036854775807";
        auto r = psp::parse_int(psp::Span<const char>{buf.data(), buf.size()});
        check(r.has_value() && *r == std::numeric_limits<std::int64_t>::min() + 1,
              "2c parse_int(\"-9223372036854775807\") = INT64_MIN+1");
    }

    // INT64_MAX + 1 (the very next value) overflows cleanly.
    {
        std::string buf = "9223372036854775808";
        auto r = psp::parse_int(psp::Span<const char>{buf.data(), buf.size()});
        check(!r && r.error() == ParseError::Overflow,
              "2d parse_int(\"9223372036854775808\") -> Overflow");
    }

    // INT64_MIN (the absolute minimum) overflows cleanly.
    {
        std::string buf = "-9223372036854775808";
        auto r = psp::parse_int(psp::Span<const char>{buf.data(), buf.size()});
        check(!r && r.error() == ParseError::Overflow,
              "2e parse_int(\"-9223372036854775808\") -> Overflow");
    }

    // parse_int_at: same widening through the cursor path.
    {
        std::string buf = "5000000000";
        psp::Span<const char> s{buf.data(), buf.size()};
        auto r = psp::parse_int_at(s);
        check(r.has_value() && *r == 5000000000LL,
              "2f parse_int_at(\"5000000000\") = 5000000000");
        check(s.empty(), "  2f span fully consumed");
    }
}

// ===========================================================================
// Section 3 — round-trip parse_value_at -> json_to_string -> parse_value_at
// ===========================================================================
void section_3_round_trip() {
    header("Section 3: round-trip parse_value_at -> json_to_string -> parse_value_at (through the LIBRARY)");

    auto round_trip_check = [](std::string_view wire) -> bool {
        psp::Span<const char> s{wire.data(), wire.size()};
        auto original = psp::parse_value_at(s);
        if (!original) return false;
        std::string serialised = psp::json_to_string(*original);
        psp::Span<const char> s2{serialised.data(), serialised.size()};
        auto reparsed = psp::parse_value_at(s2);
        if (!reparsed) return false;
        return *original == *reparsed;
    };

    // INT64_MAX-shaped ints (whole-span path: parse_value_at will
    // route these through parse_double_at which is now sign-aware
    // AND int64-range-aware).
    check(round_trip_check("9223372036854775807"),   "3a round-trip INT64_MAX");
    check(round_trip_check("-9223372036854775807"),  "3b round-trip INT64_MIN+1");
    check(round_trip_check("5000000000"),            "3c round-trip 5e9 (past INT_MAX)");
    check(round_trip_check("-2000000000"),           "3d round-trip -2e9");
    check(round_trip_check("-42"),                   "3e round-trip -42");
    check(round_trip_check("0"),                     "3f round-trip 0");
    check(round_trip_check("-0"),                    "3g round-trip -0");
    check(round_trip_check("+100"),                  "3h round-trip +100");

    // Negative doubles.
    check(round_trip_check("-3.14"),                 "3i round-trip -3.14");
    check(round_trip_check("-2.5e-10"),              "3j round-trip -2.5e-10");
    check(round_trip_check("-0.0001"),               "3k round-trip -0.0001");

    // Mixed array (negative + INT64_MAX side by side).
    check(round_trip_check("[-1, 9223372036854775807, -3.14]"),
          "3l round-trip mixed array [-1, INT64_MAX, -3.14]");
}

// ===========================================================================
// Section 4 — psp::json_patch::patch (TestOp deep-equality through v0.14.0)
// ===========================================================================
void section_4_test_op() {
    header("Section 4: through psp::json_patch::patch (TestOp equality)");

    // Target document: {"x": BIG} (BIG == INT64_MAX in this test).
    auto target = make_obj();
    obj_set(target, "x", make_int(std::numeric_limits<std::int64_t>::max()));

    // TestOp match: a TestOp with BIG should match the target.
    {
        std::vector<JsonPatchOp> ops;
        ops.emplace_back(TestOp{"/x", make_int(std::numeric_limits<std::int64_t>::max())});
        auto r = psp::json_patch::patch(target, ops);
        check(r.has_value(),
              "4a TestOp(INT64_MAX) matches target INT64_MAX");
    }

    // TestOp mismatch: a TestOp with BIG-1 should NOT match the target.
    {
        std::vector<JsonPatchOp> ops;
        ops.emplace_back(TestOp{"/x", make_int(std::numeric_limits<std::int64_t>::max() - 1)});
        auto r = psp::json_patch::patch(target, ops);
        check(!r && r.error() == JsonPatchError::TestValueMismatch,
              "4b TestOp(INT64_MAX-1) mismatches target INT64_MAX -> TestValueMismatch");
    }

    // TestOp match: a TestOp with -42 matches a -42 target.
    {
        auto t = make_obj();
        obj_set(t, "x", make_int(-42));
        std::vector<JsonPatchOp> ops;
        ops.emplace_back(TestOp{"/x", make_int(-42)});
        auto r = psp::json_patch::patch(t, ops);
        check(r.has_value(),
              "4c TestOp(-42) matches target -42");
    }

    // ReplaceOp with INT64_MAX, then read /x via resolve_mut.
    {
        auto doc = make_obj();
        obj_set(doc, "x", make_int(0));
        std::vector<JsonPatchOp> ops;
        ops.emplace_back(ReplaceOp{"/x", make_int(std::numeric_limits<std::int64_t>::max())});
        auto apply = psp::json_patch::patch(doc, ops);
        check(apply.has_value(),
              "4d ReplaceOp(INT64_MAX) applied");
        auto xv_expected = psp::json_pointer::resolve_mut("/x", doc);
        check(xv_expected.has_value(),
              "  4d resolve_mut(\"/x\") succeeded");
        if (xv_expected) {
            psp::JsonValue* xv = *xv_expected;
            check(xv && std::holds_alternative<std::int64_t>(xv->value)
                  && std::get<std::int64_t>(xv->value) ==
                         std::numeric_limits<std::int64_t>::max(),
                  "  4d target /x == INT64_MAX after ReplaceOp");
        }
    }
}

// ===========================================================================
// Section 5 — wire-format round-trip through the library
// ===========================================================================
void section_5_wire_round_trip() {
    header("Section 5: wire-format round-trip through psp::json_patch::*");

    // Build a multi-op document with negative + INT64_MAX values.
    std::vector<JsonPatchOp> ops;
    ops.emplace_back(AddOp{"/y", make_int(-1)});
    ops.emplace_back(ReplaceOp{"/x", make_int(std::numeric_limits<std::int64_t>::min() + 1)});
    ops.emplace_back(TestOp{"/x", make_int(std::numeric_limits<std::int64_t>::min() + 1)});

    // Serialise via the Jul 24 consumer writer (re-inlined as
    // op_writer above) and parse back via the library's
    // psp::json_patch::parse_patch_document.
    std::string wire = op_writer::serialise_patch_document(ops);
    std::println("  5a wire (writer output, parses via v0.14.0 library):");
    std::println("{}", wire);

    auto parsed = psp::json_patch::parse_patch_document(wire);
    check(parsed.has_value(), "5a library parses the writer's wire-format output");
    if (parsed) {
        check(parsed->size() == 3, "  5a parsed 3 ops");

        auto target = make_obj();
        obj_set(target, "x", make_int(42));
        obj_set(target, "y", make_int(42));
        auto applied = psp::json_patch::patch(target, *parsed);
        check(applied.has_value(), "5a library patch() applied the parsed ops");

        auto xv_expected = psp::json_pointer::resolve_mut("/x", target);
        auto yv_expected = psp::json_pointer::resolve_mut("/y", target);
        check(xv_expected.has_value() && yv_expected.has_value(),
              "  5a resolve_mut succeeded for /x and /y");
        if (xv_expected && yv_expected) {
            psp::JsonValue* xv = *xv_expected;
            psp::JsonValue* yv = *yv_expected;
            check(xv && std::holds_alternative<std::int64_t>(xv->value)
                  && std::get<std::int64_t>(xv->value) ==
                         std::numeric_limits<std::int64_t>::min() + 1,
                  "  5a target /x == INT64_MIN+1 after ReplaceOp");
            check(yv && std::holds_alternative<std::int64_t>(yv->value)
                  && std::get<std::int64_t>(yv->value) == -1,
                  "  5a target /y == -1 after AddOp");
        }
    }

    // 5b: back-compat — a non-negative op still round-trips.
    std::vector<JsonPatchOp> ops2;
    ops2.emplace_back(AddOp{"/x", make_int(42)});
    std::string wire2 = op_writer::serialise_patch_document(ops2);
    std::println("  5b wire (back-compat check, non-negative):");
    std::println("{}", wire2);
    auto parsed2 = psp::json_patch::parse_patch_document(wire2);
    check(parsed2.has_value(), "5b library parses non-negative wire-format output");

    // 5c: nested negative values inside an array.
    std::vector<JsonPatchOp> ops3;
    {
        psp::JsonValue arr_val = make_arr();
        arr_push(arr_val, make_int(-1));
        arr_push(arr_val, make_int(std::numeric_limits<std::int64_t>::max()));
        arr_push(arr_val, make_int(-9223372036854775807LL));
        ops3.emplace_back(ReplaceOp{"/arr", std::move(arr_val)});
    }
    std::string wire3 = op_writer::serialise_patch_document(ops3);
    std::println("  5c wire (nested negative + INT64_MAX in array):");
    std::println("{}", wire3);
    auto parsed3 = psp::json_patch::parse_patch_document(wire3);
    check(parsed3.has_value(), "5c library parses nested-array wire-format output");
    if (parsed3) {
        auto doc = make_obj();
        obj_set(doc, "arr", make_arr());
        auto r = psp::json_patch::patch(doc, *parsed3);
        check(r.has_value(), "5c library applies nested-array patch");
        auto av_expected = psp::json_pointer::resolve_mut("/arr", doc);
        check(av_expected.has_value(), "  5c resolve_mut(\"/arr\") succeeded");
        if (av_expected) {
            psp::JsonValue* av = *av_expected;
            check(av && std::holds_alternative<std::vector<psp::JsonValue>>(av->value),
                  "  5c target /arr is an array");
            if (av && std::holds_alternative<std::vector<psp::JsonValue>>(av->value)) {
                const auto& a = std::get<std::vector<psp::JsonValue>>(av->value);
                check(a.size() == 3
                      && std::holds_alternative<std::int64_t>(a[0].value)
                      && std::get<std::int64_t>(a[0].value) == -1
                      && std::holds_alternative<std::int64_t>(a[1].value)
                      && std::get<std::int64_t>(a[1].value) ==
                             std::numeric_limits<std::int64_t>::max()
                      && std::holds_alternative<std::int64_t>(a[2].value)
                      && std::get<std::int64_t>(a[2].value) ==
                             -9223372036854775807LL,
                      "  5c array contents: [-1, INT64_MAX, INT64_MIN+1]");
            }
        }
    }
}

}  // namespace

int main() {
    std::println("[psp_json_v014_promotion: consumer of psp_span_lib v0.14.0]");
    std::println("[exercises the SIGNED-NUMERIC-LITERALS promotion directly]");
    std::println("[through the LIBRARY proper (no shadow dispatcher).]");

    section_1_sign_acceptance();
    section_2_widened_return_type();
    section_3_round_trip();
    section_4_test_op();
    section_5_wire_round_trip();

    std::println("");
    std::println("[psp_json_v014_promotion: {} pass, {} fail]", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}