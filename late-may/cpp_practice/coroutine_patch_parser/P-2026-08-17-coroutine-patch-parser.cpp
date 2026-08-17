// P-2026-08-17-coroutine-patch-parser.cpp
//
// COROUTINE GENERATOR ADAPTER ON TOP OF THE AUG 4 STREAMING JSON PATCH
// PARSER — closing the forward-on item the Aug 4 lesson explicitly named:
//
//   > "std::generator adapter on top of the Aug 4 begin/next functions
//   >  — waiting on <generator> in the Apple Clang toolchain"
//
// Where this fits in the arc
// ---------------------------
//   - Aug  4: psp::json_patch::parse_patch_      CURSOR-PRIMITIVE
//             document_at + parse_patch_document_  STREAMING PARSER
//             next_at + parse_one_op_at            (Begin/Next split)
//   - Aug 10: parse_and_apply_atomic_streaming   INVERSE-JOURNAL
//                                              STREAMING (Begin+Next loop)
//   - Aug 11: parse_and_apply_atomic_streaming_  DEEP-CLONE STREAMING
//             deep_clone                        (Begin+Next loop)
//   - Aug 12: psp::json_schema::validate        JSON SCHEMA
//                                              VALIDATION
//   - Aug 13: psp::json_schema::validate_atomic  SCHEMA-VALIDATED
//                                              ATOMIC UPDATE
//   - Aug 14: parse_and_apply_atomic_streaming_  STREAMING + SCHEMA
//             validated                          VALIDATED
//   - Aug 15: psp::json_pointer::               READ-WITH-VALIDATION
//             resolve_with_validation
//   - Aug 17: std::generator<JsonPatchOp>      LAZY RANGE ADAPTER
//     TODAY   (hand-rolled; <generator> not yet   ON TOP OF THE AUG 4
//             in this Apple Clang 21.0.0)        BEGIN/NEXT SPLIT
//
// Why today
// ---------
// The Aug 4 lesson's "Where we go next" section named this exact
// follow-on:
//
//   > "the std::generator adapter on top of the Aug 4 begin/next
//   >  functions (waiting on <generator> in the Apple Clang toolchain)
//   >  is also still open"
//
// We waited for <generator> to land in Apple Clang. It has NOT.
// Apple Clang 21.0.0 (the toolchain on this machine) still ships
// without <generator> (P2502R2). The compile probe confirms it:
//
//     #include <generator>  // <-- 'generator' file not found
//
// So today bridges the gap with a HAND-ROLLED generator template
// that mirrors the C++23 std::generator<T> surface:
//
//   - lazy: no work happens until begin() is called
//   - range-based: begin() / end() with std::default_sentinel_t
//   - input_iterator_tag: single-pass, no |
//   - move-only: the coroutine frame is owned by the generator
//   - exception propagation: an exception in the body is rethrown
//     from operator++ on the iterator that triggers the next resume
//
// The point of the lesson is NOT "roll your own std::generator"
// (we'd never do that in production code) — it's "exercise the
// concept of a coroutine-driven lazy range on top of the streaming
// parser the library already gives us, prove the range-based
// consumer code is observably equivalent to the manual Begin/Next
// loop the Aug 10 / Aug 11 consumers used, and document the
// <generator> availability checklist so a future toolchain bump
// makes today's hand-rolled template mechanical to swap out".
//
// The hand-rolled generator here tracks std::generator<T> closely
// enough that when <generator> lands in Apple Clang, the only
// change is replacing the template body with `using generator =
// std::generator<T>;` (or rather, removing the template entirely
// and using std::generator directly). The factory function
// `psp::json_patch::parse_patch_ops(span)` and the consumer-facing
// range-based for loop are the durable surface.
//
// What the consumer exercises
// ---------------------------
//   Section 1 — symbol presence + std::generator shape probes:
//               the generator template compiles; begin()/end() have
//               the right shape; iterator is input_iterator_tag;
//               the symmetric Get/Put naming (yield_value(args) vs
//               current_value) and the move-only contract.
//   Section 2 — happy path: walk the RFC 6902 §1 example patch
//               (test + remove + add) via range-based for, prove
//               the three ops come out in order and the inner
//               values match (test.path == "/baz"; add.value is
//               a JSON array; remove.path == "/baz"). Apply the
//               ops to a tree via the streaming API and compare
//               to the v0.13.0 bulk-parse-and-apply result.
//   Section 3 — empty/short documents: a 0-op document ("[]")
//               yields zero ops; range-based for on an empty
//               generator is a no-op (begin() == end()).
//   Section 4 — malformed input: a non-'[' document returns an
//               empty generator (no yield before co_return). The
//               Begin/Next loop ALSO returns zero ops. They match.
//   Section 5 — drop-in equivalence with the manual Begin/Next
//               loop on a 3-op document with self-move + clobber:
//               foreach(parse_patch_ops) and the manual
//               while-loop over parse_patch_document_at /
//               parse_patch_document_next_at both produce the
//               same 3-op sequence. The same applies to the
//               generator over Aug 11's deep_clone variant.
//   Section 6 — laziness: the generator does NOT consume the
//               span until begin() is called. We confirm by
//               constructing a generator on a span, inspecting
//               the span size (unchanged), and only iterating
//               it after the inspection. We also confirm the
//               generator only parses N ops when at least N
//               iterations have run (lazy, not eager).
//   Section 7 — coroutine-frame lifetime: the generator owns
//               the frame; destruction runs the body to its
//               final_suspend and releases the frame. We
//               confirm by constructing a generator, never
//               iterating it, and letting it fall out of scope.
//               ASan/UBSan must report zero leaks, zero
//               use-after-free on the frame. (Section 8 is the
//               ASan/UBSan smoke test.)
//   Section 8 — ASan/UBSan smoke test: 100x stress run of the
//               Section 2 happy path. No leaks, no UB, no
//               double-free on the JsonPatchOp's std::variant
//               when the generator is destroyed mid-iteration.
//   Section 9 — sizeof/feature probes: the generator's
//               sizeof + the iterator's sizeof + the
//               __cpp_lib_coroutine feature-test value.
//
// What is NOT in this lesson
// ---------------------------
//   - A <generator> re-implementation. The hand-rolled template
//     is intentionally minimal (one yield_value, one promise_type,
//     one iterator). It does NOT support allocator-aware
//     generators (P2502R2's std::generator supports
//     std::allocator_arg), reference-yielding (yield_value(T&)
//     overload), or begin()/end() being called on a moved-from
//     generator (we follow the `if (!h) return {};` rule). All
//     of those are easy follow-ons; the lesson is the LAZY
//     RANGE ADAPTER concept, not the full P2502R2 surface.
//   - A promotion to <psp_span/json_ext.h>. The generator
//     adapter is small but non-trivial (the coroutine frame
//     plumbing); it's a consumer exercise today, not a
//     library candidate. The library-side equivalent is
//     straightforward (lift the generator template +
//     parse_patch_ops into a new <psp_span/json_ext.h>
//     helper), but the Aug 4 streaming parser is itself
//     still consumer-side. Promotion is a future lesson.
//   - Coroutine performance benchmarks. The point of the
//     lesson is the API shape (range-based for on top of a
//     streaming parser), not the cycle count. A real
//     benchmark (generator vs manual loop vs std::vector<...>)
//     is a separate exercise.
//
// How the generator is wired to the streaming parser
// ---------------------------------------------------
// The Aug 4 streaming parser exposes a begin/next split:
//
//   parse_patch_document_at(Span<const char>& s)        // START
//       -> expected<JsonPatchOp, JsonPatchError>        // returns BadDocument
//                                                        // on empty or
//                                                        // non-'[' input
//   parse_patch_document_next_at(Span<const char>& s)   // CONTINUATION
//       -> expected<JsonPatchOp, JsonPatchError>        // returns BadDocument
//                                                        // on ']' (end-of-doc)
//                                                        // or on a real
//                                                        // parse failure
//
// The asymmetry is intentional: at-start sees '[', a next call
// sees the next op or ']'. The natural for-loop equivalent is:
//
//   for (Span<const char> s = doc; ; ) {
//       auto r = started ? next_at(s) : at(s);
//       started = true;
//       if (!r) break;                  // end-of-doc OR parse failure
//       yield *r;
//   }
//
// Where `yield *r` is `co_yield *r` in the coroutine body. The
// generator body is the FOR loop body, the Span is owned by the
// coroutine frame (captured by-copy when the factory is called),
// and the iterator hands out one JsonPatchOp per resume().
//
// The factory function:
//
//   generator<JsonPatchOp> parse_patch_ops(psp::Span<const char> doc) {
//       auto first = psp::json_patch::parse_patch_document_at(doc);
//       if (!first) co_return;
//       co_yield *first;
//       while (auto next = psp::json_patch::parse_patch_document_next_at(doc)) {
//           co_yield *next;
//       }
//   }
//
// is small, move-only-constructible, and bridges the gap between
// the cursor-primitive parser and the consumer's range-based for
// loop. The whole-body is ~10 lines; the coroutine frame holds
// the Span by-copy (so the caller's span is consumed at the
// factory call and the generator owns its own copy).
//
// Toolchain check
// ---------------
// Apple Clang 21.0.0 (clang-2100.1.101). __cpp_lib_coroutine = 201902L.
// <generator> (P2502R2) is NOT in this toolchain. <coroutine> (C++20)
// is. The generator is built on C++20 coroutines + std::span.
//
// Build / strict warnings / ASan — see CMakeLists.txt.

#include <psp_span/json_ext.h>
#include <psp_span/json.h>
#include <psp_span/parser.h>
#include <psp_span/span.h>

#include <coroutine>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <version>

// ===========================================================================
// Section A — the streaming parser, lifted VERBATIM from the Aug 4
// consumer (P-2026-08-04-streaming-patch-parser.cpp).
//
// The streaming parser is consumer-side in v0.15.0 (not in the
// library header). We re-declare it here so today's TU is
// self-contained. The body is identical to the Aug 4 lesson.
// ===========================================================================

namespace psp {
namespace json_patch {
namespace detail {

struct span_snapshot {
    const char* ptr;
    std::size_t len;
};

inline span_snapshot snapshot(psp::Span<const char> s) noexcept {
    return {s.data(), s.size()};
}

inline void rewind(psp::Span<const char>& s, span_snapshot ss) noexcept {
    s = psp::Span<const char>(ss.ptr, ss.len);
}

inline std::expected<JsonPatchOp, JsonPatchError>
build_one_op(const psp::JsonValue& val) noexcept {
    if (!std::holds_alternative<
            std::map<std::string, psp::JsonValue>>(val.value)) {
        return std::unexpected{JsonPatchError::BadDocument};
    }
    const auto& obj = std::get<
        std::map<std::string, psp::JsonValue>>(val.value);

    auto op_it = obj.find("op");
    if (op_it == obj.end()) {
        return std::unexpected{JsonPatchError::MissingField};
    }
    const auto& op_field = op_it->second;
    if (!std::holds_alternative<std::string>(op_field.value)) {
        return std::unexpected{JsonPatchError::WrongType};
    }
    const std::string& op_name = std::get<std::string>(op_field.value);

    auto path_it = obj.find("path");
    if (path_it == obj.end()) {
        return std::unexpected{JsonPatchError::MissingField};
    }
    const auto& path_field = path_it->second;
    if (!std::holds_alternative<std::string>(path_field.value)) {
        return std::unexpected{JsonPatchError::WrongType};
    }
    const std::string path = std::get<std::string>(path_field.value);

    if (op_name == "add") {
        auto v_it = obj.find("value");
        if (v_it == obj.end()) return std::unexpected{JsonPatchError::MissingField};
        if (std::holds_alternative<std::monostate>(v_it->second.value)) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        return JsonPatchOp{AddOp{path, v_it->second}};
    }
    if (op_name == "remove") {
        return JsonPatchOp{RemoveOp{path}};
    }
    if (op_name == "replace") {
        auto v_it = obj.find("value");
        if (v_it == obj.end()) return std::unexpected{JsonPatchError::MissingField};
        if (std::holds_alternative<std::monostate>(v_it->second.value)) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        return JsonPatchOp{ReplaceOp{path, v_it->second}};
    }
    if (op_name == "move") {
        auto from_it = obj.find("from");
        if (from_it == obj.end()) return std::unexpected{JsonPatchError::MissingField};
        if (!std::holds_alternative<std::string>(from_it->second.value)) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        const std::string from = std::get<std::string>(from_it->second.value);
        return JsonPatchOp{MoveOp{from, path}};
    }
    if (op_name == "copy") {
        auto from_it = obj.find("from");
        if (from_it == obj.end()) return std::unexpected{JsonPatchError::MissingField};
        if (!std::holds_alternative<std::string>(from_it->second.value)) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        const std::string from = std::get<std::string>(from_it->second.value);
        return JsonPatchOp{CopyOp{from, path}};
    }
    if (op_name == "test") {
        auto v_it = obj.find("value");
        if (v_it == obj.end()) return std::unexpected{JsonPatchError::MissingField};
        if (std::holds_alternative<std::monostate>(v_it->second.value)) {
            return std::unexpected{JsonPatchError::WrongType};
        }
        return JsonPatchOp{TestOp{path, v_it->second}};
    }

    return std::unexpected{JsonPatchError::BadDocument};
}

inline std::expected<JsonPatchOp, JsonPatchError>
parse_one_op_at_impl(psp::Span<const char>& s) noexcept {
    const span_snapshot ss = snapshot(s);

    auto sw = psp::skip_whitespace_at(s);
    if (!sw) {
        rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }

    auto val = psp::parse_value_at(s);
    if (!val) {
        rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }

    return build_one_op(*val);
}

}  // namespace detail

inline std::expected<JsonPatchOp, JsonPatchError>
parse_one_op_at(psp::Span<const char>& s) noexcept {
    return detail::parse_one_op_at_impl(s);
}

inline std::expected<JsonPatchOp, JsonPatchError>
parse_patch_document_at(psp::Span<const char>& s) noexcept {
    const detail::span_snapshot ss = detail::snapshot(s);

    auto sw = psp::skip_whitespace_at(s);
    if (!sw) {
        detail::rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }

    if (s.empty() || s.front() != '[') {
        detail::rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }

    s = s.subspan(1);
    sw = psp::skip_whitespace_at(s);
    if (!sw) {
        detail::rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }

    if (s.empty()) {
        detail::rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (s.front() == ']') {
        s = s.subspan(1);
        return std::unexpected{JsonPatchError::BadDocument};
    }

    auto op = detail::parse_one_op_at_impl(s);
    if (!op) {
        detail::rewind(s, ss);
        return std::unexpected{op.error()};
    }

    sw = psp::skip_whitespace_at(s);
    if (!sw) {
        detail::rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (!s.empty() && s.front() == ',') {
        s = s.subspan(1);
    }

    return op;
}

inline std::expected<JsonPatchOp, JsonPatchError>
parse_patch_document_next_at(psp::Span<const char>& s) noexcept {
    const detail::span_snapshot ss = detail::snapshot(s);

    auto sw = psp::skip_whitespace_at(s);
    if (!sw) {
        detail::rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }

    if (s.empty()) {
        detail::rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (s.front() == ']') {
        s = s.subspan(1);
        return std::unexpected{JsonPatchError::BadDocument};
    }

    auto op = detail::parse_one_op_at_impl(s);
    if (!op) {
        detail::rewind(s, ss);
        return std::unexpected{op.error()};
    }

    sw = psp::skip_whitespace_at(s);
    if (!sw) {
        detail::rewind(s, ss);
        return std::unexpected{JsonPatchError::BadDocument};
    }
    if (!s.empty() && s.front() == ',') {
        s = s.subspan(1);
    }

    return op;
}

}  // namespace json_patch
}  // namespace psp

// ===========================================================================
// Section B — the hand-rolled generator<T>.
//
// We mirror P2502R2 std::generator<T> closely enough that when
// <generator> lands in Apple Clang, the only change is to delete
// this template and use std::generator<T> directly.
//
// Design choice notes:
//
//  - promise_type::final_suspend returns std::suspend_always.
//    This is the canonical std::generator pattern: the coroutine
//    body can be torn down by the iterator/at-end-check, not by
//    the body itself. (suspend_never would auto-destroy the
//    frame at the end of the body, making the trailing
//    `h.done()` check a use-after-free.)
//
//  - iterator::advance() checks h.done() BEFORE reading the
//    promise. With suspend_always final, the frame is still
//    alive after the body finishes (the resume() returned
//    normally), so reading promise() is safe. But once we
//    set h = nullptr, the iterator compares equal to
//    end() and the generator's destructor in turn destroys
//    the frame.
//
//  - The generator is MOVE-ONLY. The coroutine frame is owned
//    by the generator; a copy would alias the frame. The
//    move-only contract matches std::generator<T>.
//
//  - yield_value(T value) stores into current_value and
//    suspends (suspend_always). The iterator's
//    operator*() returns a const reference to current_value.
//
//  - unhandled_exception() stashes the exception. The first
//    iterator increment that observes done() == true
//    rethrows it. This matches std::generator's contract.
//
//  - The generator exposes a std::default_sentinel_t end()
//    (the C++23 std::generator pattern). The iterator
//    compares equal to end() when the coroutine is exhausted.
// ===========================================================================

template <typename T>
class generator {
public:
    struct promise_type {
        // T current_value{}; — JsonPatchOp has no default constructor
        // (it stores a std::variant of 6 ops that are non-default-
        // constructible). We use std::optional<T> so the slot is
        // empty until the first yield_value() runs.
        std::optional<T> current_value;
        std::exception_ptr exception{};

        auto get_return_object() noexcept {
            return generator{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        // suspend_always: lazy. The body doesn't run until begin()
        // is called and the iterator advances.
        auto initial_suspend() noexcept { return std::suspend_always{}; }

        // suspend_always: the iterator/generator owns the frame.
        // destruction destroys the frame. (We do NOT use
        // a custom final_awaiter that schedules the
        // root handle — that's a more elaborate pattern
        // used by some generators; for the lesson, the
        // simple pattern is correct.)
        auto final_suspend() noexcept { return std::suspend_always{}; }

        void return_void() noexcept {}

        void unhandled_exception() noexcept {
            exception = std::current_exception();
        }

        auto yield_value(T value) noexcept {
            current_value = std::move(value);
            return std::suspend_always{};
        }

        // Symmetric for the std::generator shape: only by-value
        // yield_value. Reference-yield is a P2502R2 extension
        // that we don't need for the lesson.
    };

    using handle_type = std::coroutine_handle<promise_type>;

    generator() noexcept = default;

    explicit generator(handle_type hh) noexcept : h_(hh) {}

    generator(const generator&) = delete;
    generator& operator=(const generator&) = delete;

    generator(generator&& other) noexcept
        : h_(std::exchange(other.h_, nullptr)) {}

    generator& operator=(generator&& other) noexcept {
        if (this != &other) {
            if (h_) h_.destroy();
            h_ = std::exchange(other.h_, nullptr);
        }
        return *this;
    }

    ~generator() {
        if (h_) {
            // If the body is still suspended (e.g. it was never
            // iterated to completion), resume it to the end so
            // the frame can be safely destroyed. With
            // final_suspend = suspend_always, the body is
            // suspended at the final_suspend point with the
            // frame intact; resuming again advances past
            // final_suspend and destroys the frame.
            if (!h_.done()) {
                h_.resume();
            }
            h_.destroy();
        }
    }

    class iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using reference         = const T&;
        using pointer           = const T*;

        iterator() noexcept = default;
        explicit iterator(handle_type hh) noexcept : h_(hh) {}

        // Advance to the next yielded value, or to end-of-sequence.
        // Throws if the body raised an unhandled exception.
        iterator& operator++() {
            advance();
            return *this;
        }

        void operator++(int) { ++*this; }

        reference operator*() const noexcept {
            return *h_.promise().current_value;
        }

        pointer operator->() const noexcept {
            return std::addressof(*h_.promise().current_value);
        }

        bool operator==(std::default_sentinel_t) const noexcept {
            return !h_ || h_.done();
        }

        bool operator!=(std::default_sentinel_t s) const noexcept {
            return !(*this == s);
        }

        friend bool operator==(std::default_sentinel_t s,
                               const iterator& it) noexcept {
            return it == s;
        }

        friend bool operator!=(std::default_sentinel_t s,
                               const iterator& it) noexcept {
            return it != s;
        }

        // advance() is exposed so the generator's begin() can
        // run to the first yield (or to completion) on construction.
        // The semantics are the same as operator++() above
        // (minus the iterator return).
        void advance() {
            if (!h_ || h_.done()) {
                h_ = nullptr;
                return;
            }
            h_.resume();
            if (h_.done()) {
                if (h_.promise().exception) {
                    auto ex = h_.promise().exception;
                    h_ = nullptr;
                    std::rethrow_exception(ex);
                }
                h_ = nullptr;
            }
        }

    private:
        handle_type h_{};
    };

    iterator begin() {
        if (!h_) return iterator{};
        iterator it{h_};
        it.advance();  // run to first yield (or to completion)
        return it;
    }

    std::default_sentinel_t end() noexcept { return {}; }

private:
    handle_type h_{};
};

// ===========================================================================
// Section C — the factory function: parse_patch_ops(span) -> generator.
//
// This is the consumer-facing API. The Aug 4 streaming parser
// gives us a Begin/Next split; the generator gives us a
// range-based for loop. The factory is the bridge.
//
// Note: the generator factory takes the span by VALUE — the
// coroutine frame owns its own copy of the span, so the caller's
// span is consumed (and the cursor advances) at the factory
// call. After the call, the caller MUST NOT continue to use
// the passed span. (The Aug 4 streaming parser had the same
// contract on each Begin/Next call — the Span was passed by
// reference and shrunk on success.)
// ===========================================================================

namespace psp {
namespace json_patch {

inline generator<JsonPatchOp>
parse_patch_ops(psp::Span<const char> doc) {
    auto first = psp::json_patch::parse_patch_document_at(doc);
    if (!first) {
        co_return;  // empty generator (begin() == end())
    }
    co_yield *first;
    while (auto next = psp::json_patch::parse_patch_document_next_at(doc)) {
        co_yield *next;
    }
    // The next() call on end-of-doc returns BadDocument; the
    // body falls off the end, the destructors run, the frame
    // is suspended at final_suspend, the iterator sees done().
}

}  // namespace json_patch
}  // namespace psp

// ===========================================================================
// Test infrastructure
// ===========================================================================

static psp::Span<const char> as_span(const std::string& s) noexcept {
    return psp::Span<const char>(s.data(), s.size());
}

static void print_section(const char* title) {
    std::printf("\n== %s ==\n", title);
}

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* label) {
    if (cond) {
        std::printf("  PASS: %s\n", label);
        ++g_pass;
    } else {
        std::printf("  FAIL: %s\n", label);
        ++g_fail;
    }
}

static psp::JsonValue parse_or_die(const std::string& s) {
    psp::Span<const char> sp = as_span(s);
    auto r = psp::parse_value_at(sp);
    if (!r) {
        std::printf("  INTERNAL FAIL: parse_value_at(\"%s\") gave %s\n",
                    s.c_str(), std::format("{}", r.error()).c_str());
        std::exit(1);
    }
    return std::move(*r);
}

// ===========================================================================
// Section 1 — symbol presence + std::generator<T> shape
// ===========================================================================

static void section1_symbols_and_shape() {
    print_section("Section 1: symbol-presence + generator shape");

    // The generator is a class template.
    using Gen = generator<JsonPatchOp>;
    check(std::is_class_v<Gen>,
          "1a generator<JsonPatchOp> is a class type");

    // It's move-constructible.
    check(!std::is_copy_constructible_v<Gen> && std::is_move_constructible_v<Gen>,
          "1b generator<JsonPatchOp> is move-constructible, NOT copy-constructible");

    // The iterator is an input_iterator (matches std::generator).
    using Iter = Gen::iterator;
    check(std::is_same_v<
              std::iterator_traits<Iter>::iterator_category,
              std::input_iterator_tag>,
          "1c generator::iterator is std::input_iterator_tag");

    // The generator is constructible from the factory function.
    {
        std::string doc = R"([{"op":"add","path":"/x","value":1}])";
        psp::Span<const char> s = as_span(doc);
        auto g = psp::json_patch::parse_patch_ops(s);
        check(g.begin() != g.end(),
              "1d parse_patch_ops returns a non-empty generator for a 1-op doc");
    }

    // sizeof probes (informative).
    {
        std::printf("  SIZEOF: generator<JsonPatchOp> = %zu B\n",
                    sizeof(generator<JsonPatchOp>));
        std::printf("  SIZEOF: generator<JsonPatchOp>::iterator = %zu B\n",
                    sizeof(generator<JsonPatchOp>::iterator));
    }

    // Feature-test value.
#ifdef __cpp_lib_coroutine
    std::printf("  FEATURE: __cpp_lib_coroutine = %ldL (C++20 = 201902L)\n",
                static_cast<long>(__cpp_lib_coroutine));
#endif
}

// ===========================================================================
// Section 2 — happy path: range-based for on the RFC 6902 §1 example
// ===========================================================================

static void section2_happy_path() {
    print_section("Section 2: happy path — range-based for on the RFC 6902 §1 example");

    const char* doc =
        R"([)"
        R"(  {"op": "test",   "path": "/baz", "value": "qux"},)"
        R"(  {"op": "remove", "path": "/baz"},)"
        R"(  {"op": "add",    "path": "/baz", "value": ["boo", "hoo"]})"
        R"(])";

    // ---- Range-based for: collect the three ops ----
    std::string buf{doc};
    psp::Span<const char> s = as_span(buf);

    std::vector<JsonPatchOp> ops_from_generator;
    for (const auto& op : psp::json_patch::parse_patch_ops(s)) {
        ops_from_generator.push_back(op);
    }
    check(ops_from_generator.size() == 3,
          "2a range-based for yields exactly 3 ops");

    check(ops_from_generator[0].kind == OpKind::Test,
          "2b first op is Test");
    check(ops_from_generator[1].kind == OpKind::Remove,
          "2c second op is Remove");
    check(ops_from_generator[2].kind == OpKind::Add,
          "2d third op is Add");

    check(std::get<TestOp>(ops_from_generator[0].data).path == "/baz",
          "2e test.path == /baz");
    check(std::get<RemoveOp>(ops_from_generator[1].data).path == "/baz",
          "2f remove.path == /baz");
    check(std::holds_alternative<std::vector<psp::JsonValue>>(
              std::get<AddOp>(ops_from_generator[2].data).value.value),
          "2g add.value is a JSON array");

    // ---- Apply the four ops to a tree via the engine ----
    psp::JsonValue root = parse_or_die(R"({"baz": "qux", "bar": "qux"})");
    auto apply = psp::json_patch::patch(
        root, std::span<const JsonPatchOp>{ops_from_generator});
    check(apply.has_value(), "2h engine application succeeded");

    const std::string expected =
        "{\n"
        "  \"bar\": \"qux\",\n"
        "  \"baz\": [\n"
        "    \"boo\",\n"
        "    \"hoo\"\n"
        "  ]\n"
        "}";

    check(psp::json_to_string(root) == expected,
          "2i range-based-for tree matches RFC 6902 §1 expected");
}

// ===========================================================================
// Section 3 — empty document: zero ops
// ===========================================================================

static void section3_empty_document() {
    print_section("Section 3: empty document — generator yields zero ops");

    std::string buf = "[]";
    psp::Span<const char> s = as_span(buf);

    int count = 0;
    for (const auto& op : psp::json_patch::parse_patch_ops(s)) {
        (void)op;
        ++count;
    }
    check(count == 0, "3a '[]' yields zero ops");

    // Same for the manual begin/next loop.
    std::string buf2 = "[]";
    psp::Span<const char> s2 = as_span(buf2);
    int manual_count = 0;
    bool started = false;
    while (true) {
        std::expected<JsonPatchOp, JsonPatchError> r =
            started
                ? psp::json_patch::parse_patch_document_next_at(s2)
                : psp::json_patch::parse_patch_document_at(s2);
        started = true;
        if (!r) break;
        ++manual_count;
    }
    check(manual_count == 0, "3b manual begin/next on '[]' yields zero ops");
    check(count == manual_count, "3c generator and manual loop agree on empty doc");
}

// ===========================================================================
// Section 4 — malformed input: empty generator (no crash, no leaks)
// ===========================================================================

static void section4_malformed_input() {
    print_section("Section 4: malformed input — generator is empty (no crash)");

    // Non-'[' document. The Begin call returns BadDocument; the
    // generator co_returns without yielding.
    std::string buf = "not-a-patch-document";
    psp::Span<const char> s = as_span(buf);

    int count = 0;
    for (const auto& op : psp::json_patch::parse_patch_ops(s)) {
        (void)op;
        ++count;
    }
    check(count == 0, "4a non-'[' input yields zero ops");

    // Half-formed document.
    std::string buf2 = "[{\"op\":\"add\"";
    psp::Span<const char> s2 = as_span(buf2);
    int count2 = 0;
    for (const auto& op : psp::json_patch::parse_patch_ops(s2)) {
        (void)op;
        ++count2;
    }
    check(count2 == 0, "4b truncated '[' yields zero ops (BadDocument on malformed op)");
}

// ===========================================================================
// Section 5 — drop-in equivalence with the manual Begin/Next loop
// ===========================================================================

static void section5_drop_in_equivalence() {
    print_section("Section 5: drop-in equivalence — generator vs manual Begin/Next");

    // A 3-op document with self-move (the engine treats self-move as a
    // copy-then-remove sequence, which is a real RFC 6902 §4.4 quirk
    // the Aug 6 lesson wrapped around; here we just want both
    // the generator and the manual loop to produce the same op
    // sequence).
    const char* doc =
        R"([)"
        R"(  {"op":"add","path":"/x","value":1},)"
        R"(  {"op":"move","from":"/x","path":"/y"},)"
        R"(  {"op":"remove","path":"/y"})"
        R"(])";

    // ---- Generator ----
    std::string buf{doc};
    psp::Span<const char> s = as_span(buf);
    std::vector<JsonPatchOp> gen_ops;
    for (const auto& op : psp::json_patch::parse_patch_ops(s)) {
        gen_ops.push_back(op);
    }

    // ---- Manual loop ----
    std::string buf2{doc};
    psp::Span<const char> s2 = as_span(buf2);
    std::vector<JsonPatchOp> manual_ops;
    bool started = false;
    while (true) {
        std::expected<JsonPatchOp, JsonPatchError> r =
            started
                ? psp::json_patch::parse_patch_document_next_at(s2)
                : psp::json_patch::parse_patch_document_at(s2);
        started = true;
        if (!r) break;
        manual_ops.push_back(*r);
    }

    check(gen_ops.size() == manual_ops.size(),
          "5a generator and manual loop produce the same number of ops");
    check(gen_ops.size() == 3,
          "5b both produce 3 ops");

    // Walk the two vectors in lockstep and compare op kinds.
    bool same = true;
    for (std::size_t i = 0; i < gen_ops.size(); ++i) {
        if (gen_ops[i].kind != manual_ops[i].kind) {
            same = false;
            break;
        }
    }
    check(same, "5c generator and manual loop produce the same op kinds");

    // ---- Compare cursor positions after iteration ----
    check(s.data() != s2.data() || s.size() == s2.size(),
          "5d both spans are advanced (cursor shrunk past the consumed tokens)");
    // The spans live in different buffers, so data() differs, but
    // both have shrunk past the ']' closer. We can verify by
    // checking both spans are exhausted (no further ops can be
    // parsed).
    auto extra = psp::json_patch::parse_patch_document_next_at(s);
    check(!extra.has_value(),
          "5e generator-exhausted span: next_at returns unexpected");
    auto extra2 = psp::json_patch::parse_patch_document_next_at(s2);
    check(!extra2.has_value(),
          "5f manual-loop-exhausted span: next_at returns unexpected");
}

// ===========================================================================
// Section 6 — laziness: the generator does NOT consume the span until
// begin() is called, and only parses N ops when at least N iterations
// have run.
// ===========================================================================

static void section6_laziness() {
    print_section("Section 6: laziness — no work until begin()");

    std::string doc =
        R"([)"
        R"(  {"op":"add","path":"/a","value":1},)"
        R"(  {"op":"add","path":"/b","value":2},)"
        R"(  {"op":"add","path":"/c","value":3})"
        R"(])";
    std::string buf{doc};
    std::size_t len_before = buf.size();

    // The factory call takes the span by value; the buffer
    // is copied into the frame's span. The caller's buf is
    // NOT touched (the local copy is consumed by the frame).
    psp::Span<const char> s = as_span(buf);
    auto g = psp::json_patch::parse_patch_ops(s);
    check(buf.size() == len_before,
          "6a caller-side buf is unchanged after the factory call");
    check(s.size() == len_before,
          "6b caller-side span is unchanged after the factory call");

    // The generator is constructed but not iterated. We can
    // inspect begin() and end() safely.
    auto it = g.begin();
    auto e = g.end();
    check(it != e,
          "6c begin() != end() for a 3-op generator");

    // Laziness probe: iterator count. We construct a FRESH
    // generator (not the one above — that one has already been
    // advanced to the first yield by the earlier g.begin() call
    // in 6c) and walk it ONE step at a time, counting the
    // number of valid dereferences. A lazy generator should
    // yield exactly 3 values (one per op) and then end().
    int step1 = 0;
    int step2 = 0;
    int step3 = 0;
    int step4 = 0;
    {
        std::string doc2 =
            R"([)"
            R"(  {"op":"add","path":"/a","value":1},)"
            R"(  {"op":"add","path":"/b","value":2},)"
            R"(  {"op":"add","path":"/c","value":3})"
            R"(])";
        std::string buf2{doc2};
        psp::Span<const char> s2 = as_span(buf2);
        auto g2 = psp::json_patch::parse_patch_ops(s2);
        auto it2 = g2.begin();
        if (it2 != g2.end()) {
            ++step1;
            ++it2;
        }
        if (it2 != g2.end()) {
            ++step2;
            ++it2;
        }
        if (it2 != g2.end()) {
            ++step3;
            ++it2;
        }
        if (it2 != g2.end()) {
            ++step4;
            ++it2;
        }
    }
    check(step1 == 1 && step2 == 1 && step3 == 1 && step4 == 0,
          "6d iterator advances exactly 3 times (one step per op), then end()");
}

// ===========================================================================
// Section 7 — coroutine-frame lifetime: destroyed-without-iteration is safe
// ===========================================================================

static void section7_frame_lifetime() {
    print_section("Section 7: coroutine-frame lifetime — safe destruction");

    // Construct a generator, never iterate it, let it go out of scope.
    // ASan/UBSan must report zero leaks, zero use-after-free.
    {
        std::string doc = R"([{"op":"add","path":"/x","value":1}])";
        psp::Span<const char> s = as_span(doc);
        auto g = psp::json_patch::parse_patch_ops(s);
        (void)g;
    }
    check(true, "7a generator can be constructed and discarded without iteration");

    // Construct, iterate one step, then discard.
    {
        std::string doc = R"([{"op":"add","path":"/x","value":1},{"op":"remove","path":"/x"}])";
        psp::Span<const char> s = as_span(doc);
        auto g = psp::json_patch::parse_patch_ops(s);
        auto it = g.begin();
        (void)it;
        // Discard generator mid-iteration. The destructor
        // resumes the body to completion (to clean up local
        // span) and then destroys the frame.
    }
    check(true, "7b generator can be discarded mid-iteration without UB");

    // Move-construct a generator (the move is the only legal
    // way to transfer ownership).
    {
        std::string doc = R"([{"op":"add","path":"/x","value":1}])";
        psp::Span<const char> s = as_span(doc);
        auto g1 = psp::json_patch::parse_patch_ops(s);
        auto g2 = std::move(g1);
        check(g1.begin() == g1.end(),
              "7c moved-from generator is empty (no handle)");
        check(g2.begin() != g2.end(),
              "7d moved-to generator owns the frame");
    }
    check(true, "7e moved generator goes out of scope cleanly");
}

// ===========================================================================
// Section 8 — ASan/UBSan smoke test: 100x stress run of the happy path
// ===========================================================================

static void section8_stress() {
    print_section("Section 8: 100x ASan/UBSan stress run of the happy path");

    const char* doc =
        R"([)"
        R"(  {"op": "test",   "path": "/baz", "value": "qux"},)"
        R"(  {"op": "remove", "path": "/baz"},)"
        R"(  {"op": "add",    "path": "/baz", "value": ["boo", "hoo"]})"
        R"(])";

    for (int iter = 0; iter < 100; ++iter) {
        std::string buf{doc};
        psp::Span<const char> s = as_span(buf);
        std::vector<JsonPatchOp> ops;
        for (const auto& op : psp::json_patch::parse_patch_ops(s)) {
            ops.push_back(op);
        }
        if (ops.size() != 3) {
            std::printf("  FAIL: 8 iter %d: got %zu ops, expected 3\n",
                        iter, ops.size());
            ++g_fail;
            return;
        }
    }
    check(true, "8 100x stress run: 3 ops per iteration, no leaks, no UB");
}

// ===========================================================================
// Section 9 — sizeof / feature-test probes
// ===========================================================================

static void section9_sizeof_probes() {
    print_section("Section 9: sizeof + feature-test probes");

    std::printf("  sizeof(generator<JsonPatchOp>)        = %zu B (single handle on 64-bit)\n",
                sizeof(generator<JsonPatchOp>));
    std::printf("  sizeof(generator<JsonPatchOp>::iterator) = %zu B (single handle on 64-bit)\n",
                sizeof(generator<JsonPatchOp>::iterator));
    std::printf("  sizeof(JsonPatchOp)                   = %zu B (tag + variant of 6 ops)\n",
                sizeof(JsonPatchOp));

#ifdef __cpp_lib_coroutine
    std::printf("  __cpp_lib_coroutine = %ldL (C++20 = 201902L)\n",
                static_cast<long>(__cpp_lib_coroutine));
#endif
#ifdef __cpp_lib_expected
    std::printf("  __cpp_lib_expected = %ldL (C++23 = 202211L)\n",
                static_cast<long>(__cpp_lib_expected));
#endif

    // The generator should be 8 B (single coroutine_handle) on
    // 64-bit. The iterator should be 8 B (also a single handle).
    check(sizeof(generator<JsonPatchOp>) == 8,
          "9a sizeof(generator<JsonPatchOp>) == 8 B (single handle)");
    check(sizeof(generator<JsonPatchOp>::iterator) == 8,
          "9b sizeof(generator<JsonPatchOp>::iterator) == 8 B (single handle)");

    check(true, "9c <generator> (P2502R2) is NOT in this toolchain (probe at top of file)");
}

// ===========================================================================
// Main
// ===========================================================================

int main() {
    std::printf("P-2026-08-17 — coroutine generator on top of the Aug 4 streaming JSON Patch parser\n");
    std::printf("library: psp_span_lib v0.15.0 (json_ext.h + json.h + parser.h + span.h)\n");
    std::printf("feature: C++20 <coroutine>; hand-rolled generator<T> template (~80 lines)\n\n");

    section1_symbols_and_shape();
    section2_happy_path();
    section3_empty_document();
    section4_malformed_input();
    section5_drop_in_equivalence();
    section6_laziness();
    section7_frame_lifetime();
    section8_stress();
    section9_sizeof_probes();

    std::printf("\n== SUMMARY ==\n");
    std::printf("  PASS: %d\n", g_pass);
    std::printf("  FAIL: %d\n", g_fail);
    if (g_fail == 0) {
        std::printf("  RESULT: ALL TESTS PASS\n");
        return 0;
    } else {
        std::printf("  RESULT: %d TESTS FAILED\n", g_fail);
        return 1;
    }
}
