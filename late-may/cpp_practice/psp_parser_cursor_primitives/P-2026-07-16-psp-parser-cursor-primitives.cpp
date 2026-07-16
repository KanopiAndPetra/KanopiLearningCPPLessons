// P-2026-07-16 — non-numeric cursor primitives in <psp_span/parser.h>
//
// The Jul 15 lesson (P-2026-07-15-psp-parser-streaming-cursor.cpp)
// shipped the streaming numeric parsers (parse_int_at, parse_uint_at,
// parse_double_at). Those take Span<const char>& and shrink the
// span past the consumed digit run. But two cursor-walking chores
// were left to the caller:
//   (1) skipping leading whitespace between tokens;
//   (2) verifying (and consuming) an expected delimiter.
//
// Today (v0.8.0) the parser header gains two small primitives for
// those chores:
//
//   psp::expect_char_at   (Span<const char>&, char)
//                         -> std::expected<bool, ParseError>
//   psp::skip_whitespace_at (Span<const char>&)
//                         -> std::expected<bool, ParseError>
//
// Both follow the SAME two-rule contract as the v0.7.0 numeric
// streaming parsers:
//   - On success: s shrinks past the consumed run.
//   - On failure: s is unchanged.
//
// This lesson is structured as six numbered sections:
//
//   1. expect_char_at     — peek-and-consume single char (match, mismatch, empty)
//   2. skip_whitespace_at — runs of ASCII whitespace (space, tab, newline, CR)
//   3. CSV walker rewrite — the Jul 15 lesson's Section 4 inline loop,
//                            redone against expect_char_at + skip_whitespace_at
//                            (about 5 lines instead of 7, and the delimiter
//                            is now strict)
//   4. JSON-ish walker    — the headline demo: walk a buffer like
//                            '{"x": 42, "y": 3.14}' step by step using
//                            skip_whitespace_at + expect_char_at +
//                            parse_int_at / parse_double_at. This is the
//                            forward-on to a real JSON parser that the
//                            Jul 15 lesson flagged.
//   5. Failure recovery   — when expect_char_at fails with UnexpectedChar,
//                            s is unchanged, so we can recover and try
//                            a different rule (the "composable" payoff).
//   6. sizeof / feature probes
//
// Build (assumes psp_span_lib v0.8.0 is installed at /tmp/psp_install):
//
//   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
//   cmake --build build
//   ./build/P-2026-07-16-psp-parser-cursor-primitives
//
// ASan build:
//
//   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
//   cmake --build build-asan
//   ./build-asan/P-2026-07-16-psp-parser-cursor-primitives

#include <psp_span/parser.h>
#include <psp_span/span.h>

#include <cstddef>
#include <cstdio>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers — same shape as the Jul 15 consumer's helpers.
// ---------------------------------------------------------------------------
static psp::Span<const char> as_span(const std::string& s) noexcept {
    return psp::Span<const char>(s.data(), s.size());
}

static std::string span_to_string(psp::Span<const char> s) {
    return std::string(s.data(), s.size());
}

static void print_section(const char* title) {
    std::printf("\n== %s ==\n", title);
}

// ---------------------------------------------------------------------------
// Section 1 — expect_char_at: peek-and-consume single char.
//
// Three outcomes:
//   - Match:    s shrinks by 1; result is true.
//   - Mismatch: s unchanged; result is std::unexpected{UnexpectedChar}.
//   - Empty:    s unchanged; result is std::unexpected{Empty}.
// ---------------------------------------------------------------------------
static void section_expect_char_at() {
    print_section("Section 1: psp::expect_char_at — peek-and-consume single char");

    struct Case { std::string input; char expected; bool want_ok; const char* want_err; };
    const Case cases[] = {
        // Match — s shrinks by 1
        {",",         ',',  true,  nullptr},
        {",rest",     ',',  true,  nullptr},
        {"x",         'x',  true,  nullptr},
        {"xx",        'x',  true,  nullptr},   // only the front x is consumed
        {"\t",        '\t', true,  nullptr},   // match on a non-printable

        // Mismatch — s unchanged
        {"abc",       ',',  false, "UnexpectedChar"},
        {",",         'x',  false, "UnexpectedChar"},   // comma where 'x' wanted
        {"\t",        ' ',  false, "UnexpectedChar"},   // tab where ' ' wanted

        // Empty — nothing to compare against
        {"",          ',',  false, "Empty"},
    };

    for (const auto& c : cases) {
        psp::Span<const char> s = as_span(c.input);
        std::string orig = c.input;
        auto r = psp::expect_char_at(s, c.expected);

        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: expect_char_at(\"%s\", '%c') should have succeeded but got %s\n",
                            orig.c_str(), c.expected, std::format("{}", r.error()).c_str());
                continue;
            }
            if (!*r) {
                std::printf("  FAIL: expect_char_at(\"%s\", '%c') returned false on success\n",
                            orig.c_str(), c.expected);
                continue;
            }
            std::printf("  expect_char_at(\"%s\", '%c') = true, s now = \"%s\" (advanced 1)\n",
                        orig.c_str(), c.expected, span_to_string(s).c_str());
        } else {
            if (r) {
                std::printf("  FAIL: expect_char_at(\"%s\", '%c') should have failed but got true\n",
                            orig.c_str(), c.expected);
                continue;
            }
            std::string rem = span_to_string(s);
            const char* verdict = (rem == orig) ? "unchanged" : "REWOUND BUG!";
            std::printf("  expect_char_at(\"%s\", '%c') = error:%s, s = \"%s\" (%s)\n",
                        orig.c_str(), c.expected, std::format("{}", r.error()).c_str(),
                        rem.c_str(), verdict);
        }
    }
}

// ---------------------------------------------------------------------------
// Section 2 — skip_whitespace_at: consume a run of ASCII whitespace.
//
// "ASCII whitespace" = ' ', '\t', '\n', '\r'. We verify:
//   - all four chars are skipped;
//   - non-whitespace chars terminate the run;
//   - zero-length whitespace (front char already non-whitespace) is
//     a successful no-op, not an error;
//   - empty input is a successful no-op too.
// ---------------------------------------------------------------------------
static void section_skip_whitespace_at() {
    print_section("Section 2: psp::skip_whitespace_at — consume ASCII whitespace run");

    struct Case { std::string input; std::string want_remainder; std::size_t want_advanced; };
    const Case cases[] = {
        // All-whitespace runs — consume everything
        {" ",          "",         1},
        {"   ",        "",         3},
        {"\t",         "",         1},
        {"\n",         "",         1},
        {"\r",         "",         1},
        {"\t\n  \r\t", "",         6},

        // Mixed whitespace + non-whitespace
        {"   hello",   "hello",    3},
        {"\t\tx",      "x",        2},    // 2 tabs + x: skip 2, leave 1
        {" \n abc",    "abc",      3},
        {"\r\n42",     "42",       2},    // CRLF style

        // No leading whitespace — no-op success
        {"hello",      "hello",    0},
        {"42",         "42",       0},
        {",",          ",",        0},

        // Empty — no-op success
        {"",           "",         0},
    };

    for (const auto& c : cases) {
        psp::Span<const char> s = as_span(c.input);
        auto r = psp::skip_whitespace_at(s);
        if (!r) {
            std::printf("  FAIL: skip_whitespace_at(\"%s\") returned error %s\n",
                        c.input.c_str(), std::format("{}", r.error()).c_str());
            continue;
        }
        std::string rem = span_to_string(s);
        std::size_t advanced = c.input.size() - s.size();
        if (rem != c.want_remainder || advanced != c.want_advanced) {
            std::printf("  FAIL: skip_whitespace_at(\"%s\") -> \"%s\" (advanced %zu), want \"%s\" (advanced %zu)\n",
                        c.input.c_str(), rem.c_str(), advanced,
                        c.want_remainder.c_str(), c.want_advanced);
            continue;
        }
        std::printf("  skip_whitespace_at(\"%s\") -> \"%s\" (advanced %zu)\n",
                    c.input.c_str(), rem.c_str(), advanced);
    }
}

// ---------------------------------------------------------------------------
// Section 3 — CSV walker REWRITE.
//
// This is the Jul 15 lesson's Section 4 inline CSV walker, redone
// against the new cursor primitives. The Jul 15 version was:
//
//   while (!s.empty()) {
//       auto r = psp::parse_int_at(s);
//       if (!r) { ...bail... }
//       out.push_back(*r);
//       while (!s.empty() && (s.front() < '0' || s.front() > '9')) {
//           s = s.subspan(1, s.size() - 1);   // skip ANY non-digit delimiter
//       }
//   }
//
// The Jul 15 version was LENIENT — it accepted ANY non-digit between
// tokens (commas, spaces, tabs, anything). With v0.8.0 we can be
// STRICT — expect exactly one ',' between tokens — using
// skip_whitespace_at for incidental whitespace and expect_char_at
// for the delimiter:
//
//   while (!s.empty()) {
//       psp::skip_whitespace_at(s);            // optional inter-token ws
//       if (s.empty()) break;                  // trailing whitespace is OK
//       auto r = psp::parse_int_at(s);
//       if (!r) { ...bail... }
//       out.push_back(*r);
//       psp::skip_whitespace_at(s);            // ws after the number, before the ','
//       auto c = psp::expect_char_at(s, ',');  // STRICT: must be a comma
//       if (!c) {
//           // OK if it's just trailing whitespace then end-of-input.
//           // Otherwise it's a real error (e.g. two numbers with no comma).
//           psp::skip_whitespace_at(s);
//           if (!s.empty()) {
//               std::printf("    ERROR: expected ',' got '%c'\n", s.front());
//               break;
//           }
//       }
//   }
//
// The strict version catches malformed CSV the lenient version missed.
// ---------------------------------------------------------------------------
static void section_csv_walker_rewrite() {
    print_section("Section 3: CSV walker rewrite — strict delimiters via expect_char_at");

    const std::string rows[] = {
        "10,20,30,40,50",
        "1,2,3",
        "100,200,300,400",
        "7",                     // single value
        "10 , 20 , 30",          // whitespace around delimiters
        "  1,  2,  3  ",         // leading + trailing whitespace
        "10 20",                 // MISSING comma — strict should catch
        "10,,20",                // EMPTY between commas — strict should catch (parse_int_at -> NotADigit)
    };

    for (const auto& row : rows) {
        std::printf("  row = \"%s\" -> [", row.c_str());
        psp::Span<const char> s = as_span(row);
        std::vector<int> out;
        bool ok = true;
        bool first = true;

        // Step 1: optional leading whitespace.
        psp::skip_whitespace_at(s);

        while (ok && !s.empty()) {
            auto r = psp::parse_int_at(s);
            if (!r) {
                std::printf("ERROR(parse_int_at:%s)", std::format("{}", r.error()).c_str());
                ok = false;
                break;
            }
            if (!first) std::printf(", ");
            std::printf("%d", *r);
            out.push_back(*r);
            first = false;

            // Skip whitespace AFTER the number, before the expected comma.
            psp::skip_whitespace_at(s);

            // If s is empty, we're done (trailing whitespace already skipped).
            if (s.empty()) break;

            // Expect exactly one comma. If not a comma, error.
            auto c = psp::expect_char_at(s, ',');
            if (!c) {
                // Tolerate trailing whitespace then end-of-input.
                psp::skip_whitespace_at(s);
                if (!s.empty()) {
                    std::printf(" ERROR(expect_char_at:'%c')", s.front());
                    ok = false;
                }
                break;
            }

            // Skip whitespace AFTER the comma, before the next number.
            psp::skip_whitespace_at(s);
        }
        std::printf("]%s\n", ok ? "" : "  <-- malformed");
    }
}

// ---------------------------------------------------------------------------
// Section 4 — JSON-ish walker.
//
// This is the headline demo: walk a buffer like
//   '{"x": 42, "y": 3.14}'
// step by step using skip_whitespace_at + expect_char_at + parse_int_at /
// parse_double_at. It's the forward-on to a real JSON parser that the
// Jul 15 lesson flagged as the natural next session.
//
// The walker handles:
//
//   '{' STRING ':' (INT|DOUBLE) (',' STRING ':' (INT|DOUBLE))* '}'
//
// where STRING is just a non-whitespace, non-bracket, non-comma run of
// chars (we don't parse JSON string escapes here — that's a future
// `parse_string_at(s, '"')` primitive). The walker verifies each
// structural char strictly via expect_char_at, so a malformed input
// (missing colon, missing comma, wrong brace) is detected.
//
// This is the payoff for v0.8.0's cursor primitives: the walker is
// about 20 lines of actual logic, mostly one-liners. Without them,
// the inline `while (!s.empty() && s.front() == ' ') s = s.subspan(1,...)`
// and `if (!s.empty() && s.front() == ':') s = s.subspan(1, ...)` loops
// would push that to 40+ lines.
// ---------------------------------------------------------------------------
struct KvPair { std::string key; std::string value_repr; };

static std::expected<std::vector<KvPair>, ParseError>
parse_json_object(psp::Span<const char>& s) noexcept {
    std::vector<KvPair> out;

    auto ws = psp::skip_whitespace_at(s); if (!ws) return std::unexpected{ws.error()};
    auto open = psp::expect_char_at(s, '{'); if (!open) return std::unexpected{open.error()};

    // Empty object? "{}"
    psp::skip_whitespace_at(s);
    if (!s.empty() && s.front() == '}') {
        psp::expect_char_at(s, '}');
        psp::skip_whitespace_at(s);  // consume trailing whitespace after '}'
        return out;
    }

    while (true) {
        psp::skip_whitespace_at(s);

        // STRING — we only handle the leading '"' and the trailing '"',
        // skipping everything in between. (No JSON escape support yet.)
        auto qopen = psp::expect_char_at(s, '"');
        if (!qopen) return std::unexpected{qopen.error()};

        // Read the key: scan until the closing '"' or end-of-input.
        // key_start is the offset within `s` of the closing '"'.
        std::size_t key_start = 0;
        while (key_start < s.size() && s[key_start] != '"') ++key_start;
        if (key_start >= s.size()) {
            // No closing quote — buffer ran out mid-string.
            return std::unexpected{ParseError::Empty};
        }
        // Capture the key bytes BEFORE the closing quote.
        std::string key(s.data(), key_start);
        // Advance past the key AND the closing quote (key_start is the
        // offset of the closing '"', so we want to start at key_start+1).
        s = s.subspan(key_start + 1, s.size() - (key_start + 1));

        psp::skip_whitespace_at(s);

        auto colon = psp::expect_char_at(s, ':');
        if (!colon) return std::unexpected{colon.error()};

        psp::skip_whitespace_at(s);

        // VALUE — parse as a double (covers ints too via the integer-
        // only branch in parse_double_at). For this demo we keep it
        // simple: parse as double, print with %g.
        auto val = psp::parse_double_at(s);
        if (!val) return std::unexpected{val.error()};

        // Store a printable repr of the value.
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%g", *val);
        out.push_back({std::move(key), std::string(buf)});

        psp::skip_whitespace_at(s);

        // Either ',' (more pairs coming) or '}' (end of object).
        if (!s.empty() && s.front() == ',') {
            psp::expect_char_at(s, ',');
            continue;
        }
        auto close = psp::expect_char_at(s, '}');
        if (!close) return std::unexpected{close.error()};
        psp::skip_whitespace_at(s);  // consume trailing whitespace after '}'
        return out;
    }
}

static void section_json_walker() {
    print_section("Section 4: JSON-ish walker — skip_whitespace_at + expect_char_at + parse_double_at");

    struct Case { std::string input; bool want_ok; std::size_t want_pairs; };
    const Case cases[] = {
        // Well-formed JSON-ish objects
        {"{\"x\": 42}",                      true,  1},
        {"{\"x\": 42, \"y\": 3.14}",         true,  2},
        {"{\"a\": 1, \"b\": 2, \"c\": 3}",   true,  3},
        {"{}",                               true,  0},
        {"  {\"x\":42}  ",                   true,  1},    // whitespace padding
        {"{\n  \"x\": 1,\n  \"y\": 2\n}",    true,  2},    // multi-line

        // Malformed — should fail
        {"{\"x\" 42}",                       false, 0},    // missing colon
        {"{\"x\": 42,}",                    false, 0},    // trailing comma before }
        {"{\"x\": 42",                       false, 0},    // missing close brace
        {"",                                false, 0},    // empty input
        {"{\"x\": \"string\"}",              false, 0},    // value is a string, not a number
    };

    for (const auto& c : cases) {
        psp::Span<const char> s = as_span(c.input);
        auto r = parse_json_object(s);

        if (c.want_ok) {
            if (!r) {
                std::printf("  FAIL: parse_json_object(\"%s\") error=%s\n",
                            c.input.c_str(), std::format("{}", r.error()).c_str());
                continue;
            }
            if (r->size() != c.want_pairs) {
                std::printf("  FAIL: parse_json_object(\"%s\") got %zu pairs, want %zu\n",
                            c.input.c_str(), r->size(), c.want_pairs);
                continue;
            }
            std::printf("  parse_json_object(\"%s\") -> {", c.input.c_str());
            for (std::size_t i = 0; i < r->size(); ++i) {
                if (i > 0) std::printf(", ");
                std::printf("\"%s\": %s", (*r)[i].key.c_str(), (*r)[i].value_repr.c_str());
            }
            // After a successful parse, s should be empty (or whitespace-only,
            // which skip_whitespace_at already consumed at the top).
            std::printf("} (s remaining = \"%s\")\n", span_to_string(s).c_str());
        } else {
            if (r) {
                std::printf("  FAIL: parse_json_object(\"%s\") should have failed but got %zu pairs\n",
                            c.input.c_str(), r->size());
                continue;
            }
            std::printf("  parse_json_object(\"%s\") = error:%s (as expected)\n",
                        c.input.c_str(), std::format("{}", r.error()).c_str());
        }
    }
}

// ---------------------------------------------------------------------------
// Section 5 — Failure recovery (the "composable" payoff).
//
// Both new primitives follow the failure-leaves-s-unchanged contract.
// This means the caller can ATTEMPT an expect_char_at, and if the
// char isn't there, fall back to a different rule without losing
// the cursor position. This is what makes the primitives
// composable: try `,`, fail with UnexpectedChar, try `;`, fail with
// UnexpectedChar, give up — all without consuming any input.
//
// Without expect_char_at, this kind of "peek-and-decide" requires
// hand-rolling `if (s.front() == ',' || s.front() == ';')` chains
// that get hairy fast.
// ---------------------------------------------------------------------------
static void section_failure_recovery() {
    print_section("Section 5: failure recovery — s unchanged on mismatch, can try again");

    struct Case { std::string input; std::vector<char> try_order; std::size_t want_advanced; };
    const Case cases[] = {
        {",abc",   {',', ';'},     1},    // ',' matches, advance 1
        {";abc",   {',', ';'},     1},    // ',' fails (s unchanged), ';' matches
        {":abc",   {',', ';'},     0},    // neither matches, s unchanged
        {"x",      {'x', 'y'},     1},    // 'x' matches, advance 1 (then s = "")
        {"",       {','},          0},    // empty -> Empty error, s unchanged
    };

    for (const auto& c : cases) {
        psp::Span<const char> s = as_span(c.input);
        std::string orig = c.input;
        std::string outcome_label = "?";

        for (char ch : c.try_order) {
            auto r = psp::expect_char_at(s, ch);
            if (r) {
                outcome_label = "matched";
                break;
            }
            // r.error() tells us why — could be UnexpectedChar or Empty.
            // Either way, s is unchanged. Try the next char.
            outcome_label = std::format("error:{}", r.error());
        }

        std::size_t real_advanced = orig.size() - s.size();
        const char* verdict = (real_advanced == c.want_advanced) ? "OK" : "WRONG";
        std::printf("  input=\"%s\" try={", orig.c_str());
        for (std::size_t i = 0; i < c.try_order.size(); ++i) {
            if (i > 0) std::printf(", ");
            std::printf("'%c'", c.try_order[i]);
        }
        std::printf("} -> %s, s=\"%s\" (advanced %zu, want %zu)  [%s]\n",
                    outcome_label.c_str(), span_to_string(s).c_str(),
                    real_advanced, c.want_advanced, verdict);
    }
}

// ---------------------------------------------------------------------------
// Section 6 — sizeof / alignment / feature-test probes.
//
// The new primitives return std::expected<bool, ParseError>. On this
// toolchain:
//   sizeof(bool) == 1, sizeof(ParseError) == 4 (4-byte enum class),
//   so the success payload is 1 byte + 1 byte discriminator + 2 bytes
//   alignment slack = 8 bytes total (matches std::expected with EBO).
// ---------------------------------------------------------------------------
static void section_probes() {
    print_section("Section 6: sizeof / feature probes");

    std::printf("  sizeof(bool)                                       = %zu\n", sizeof(bool));
    std::printf("  sizeof(ParseError)                                 = %zu\n", sizeof(ParseError));
    std::printf("  sizeof(psp::Span<const char>)                      = %zu\n", sizeof(psp::Span<const char>));
    std::printf("  sizeof(std::expected<bool, ParseError>)            = %zu\n",
                sizeof(std::expected<bool, ParseError>));
    std::printf("  sizeof(std::expected<int, ParseError>)             = %zu\n",
                sizeof(std::expected<int, ParseError>));
    std::printf("  sizeof(std::expected<unsigned, ParseError>)       = %zu\n",
                sizeof(std::expected<unsigned, ParseError>));
    std::printf("  sizeof(std::expected<double, ParseError>)          = %zu\n",
                sizeof(std::expected<double, ParseError>));

    // Print the error name for each ParseError variant to prove the
    // formatter covers UnexpectedChar (added today) too.
    std::printf("  ParseError names:\n");
    const ParseError all_errors[] = {
        ParseError::Empty, ParseError::LeadingSign, ParseError::NotADigit,
        ParseError::Overflow, ParseError::BadExponent,
        ParseError::MissingFraction, ParseError::UnexpectedChar,
    };
    for (ParseError e : all_errors) {
        std::printf("    -> \"%s\"\n", std::format("{}", e).c_str());
    }
#if defined(__cpp_lib_expected)
    std::printf("  __cpp_lib_expected                                 = %ld\n",
                static_cast<long>(__cpp_lib_expected));
#endif
#if defined(__cpp_lib_span)
    std::printf("  __cpp_lib_span                                     = %ld\n",
                static_cast<long>(__cpp_lib_span));
#endif
}

int main() {
    section_expect_char_at();
    section_skip_whitespace_at();
    section_csv_walker_rewrite();
    section_json_walker();
    section_failure_recovery();
    section_probes();
    std::printf("\n[psp_parser_cursor_primitives_consumer: all 6 sections complete]\n");
    return 0;
}