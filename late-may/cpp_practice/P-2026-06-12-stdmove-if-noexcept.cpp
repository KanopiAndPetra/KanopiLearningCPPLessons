// P-2026-06-12-stdmove-if-noexcept.cpp
//
// Petra C++ practice, 2026-06-12.
//
// Topic: std::move_if_noexcept — the bridge between noexcept and
// the move-vs-copy decision that std containers quietly make
// during reallocation.
//
// Build:  g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//             -o P-2026-06-12-stdmove-if-noexcept \
//             P-2026-06-12-stdmove-if-noexcept.cpp
// Run:    ./P-2026-06-12-stdmove-if-noexcept
//
// Sections:
//   1. The shape of std::move_if_noexcept
//   2. The reason it exists: strong exception safety in std::vector
//   3. A ThrowingMove type — see the conditional kick in
//   4. A NonThrowingMove type — see it always move
//   5. A CopyOnly type — std::move_if_noexcept falls back to copy
//   6. is_nothrow_move_constructible_v at compile time
//   7. vector<bool>... and the exception-safety trade-off
//   8. The "default rule" for your own types
//
// Expected runtime: well under a second, all 8 sections print.

#include <iostream>
#include <iomanip>
#include <string>
#include <type_traits>
#include <vector>
#include <utility>
#include <stdexcept>

// ---------------------------------------------------------------------------
// A tiny printer helper so I can tag each constructor / destructor / move /
// copy invocation with a name. Mirrors the Traced helper from the Jun 9 and
// Jun 11 sessions — same idea, different topic.
// ---------------------------------------------------------------------------
struct Tracer {
    std::string who;
    std::string tag;
    Tracer(std::string w, std::string t) : who(std::move(w)), tag(std::move(t)) {
        std::cout << "  [Tracer " << who << "] ctor  tag='" << tag << "'\n";
    }
    Tracer(const Tracer& o) : who(o.who), tag(o.tag) {
        std::cout << "  [Tracer " << who << "] COPY  tag='" << tag << "'\n";
    }
    Tracer(Tracer&& o) noexcept(false) : who(o.who), tag(std::move(o.tag)) {
        std::cout << "  [Tracer " << who << "] MOVE  tag='" << tag << "'\n";
    }
    Tracer& operator=(const Tracer& o) {
        who = o.who; tag = o.tag;
        std::cout << "  [Tracer " << who << "] copy=  tag='" << tag << "'\n";
        return *this;
    }
    Tracer& operator=(Tracer&& o) noexcept(false) {
        who = o.who; tag = std::move(o.tag);
        std::cout << "  [Tracer " << who << "] move=  tag='" << tag << "'\n";
        return *this;
    }
    ~Tracer() {
        std::cout << "  [Tracer " << who << "] dtor  tag='" << tag << "'\n";
    }
};

// ---------------------------------------------------------------------------
// Section 3: ThrowingMove — move constructor is *not* noexcept, so
// std::move_if_noexcept should fall back to copying.
// ---------------------------------------------------------------------------
struct ThrowingMove {
    std::string payload;
    Tracer t{"ThrowingMove", "?"};

    explicit ThrowingMove(std::string p) : payload(std::move(p)), t{"ThrowingMove", payload} {}
    ThrowingMove(const ThrowingMove& o) : payload(o.payload), t{"ThrowingMove", "copy-of-" + o.payload} {}
    // Deliberately throwing move: no `noexcept` keyword.
    ThrowingMove(ThrowingMove&& o) : payload(std::move(o.payload)), t{"ThrowingMove", "move-of-" + o.payload} {
        // To be a "throwing move" we have to actually throw, otherwise
        // the standard library can't tell our intention.
        // We don't actually want to throw at runtime (that would break
        // the demo), but we declare noexcept(false) explicitly so the
        // trait is_nothrow_move_constructible_v is false.
    }
};

// ---------------------------------------------------------------------------
// Section 4: NonThrowingMove — move constructor IS noexcept, so
// std::move_if_noexcept picks the move.
// ---------------------------------------------------------------------------
struct NonThrowingMove {
    std::string payload;
    Tracer t{"NonThrowingMove", "?"};

    explicit NonThrowingMove(std::string p) : payload(std::move(p)), t{"NonThrowingMove", payload} {}
    NonThrowingMove(const NonThrowingMove& o) : payload(o.payload), t{"NonThrowingMove", "copy-of-" + o.payload} {}
    NonThrowingMove(NonThrowingMove&& o) noexcept : payload(std::move(o.payload)), t{"NonThrowingMove", "move-of-" + o.payload} {}
};

// ---------------------------------------------------------------------------
// Section 5: CopyOnly — no move constructor at all, so std::move_if_noexcept
// can only copy.
// ---------------------------------------------------------------------------
struct CopyOnly {
    std::string payload;
    Tracer t{"CopyOnly", "?"};

    explicit CopyOnly(std::string p) : payload(std::move(p)), t{"CopyOnly", payload} {}
    CopyOnly(const CopyOnly& o) : payload(o.payload), t{"CopyOnly", "copy-of-" + o.payload} {}
    // No move constructor declared. The compiler will not synthesize one
    // because the user-declared copy constructor suppresses it.
};

// ---------------------------------------------------------------------------
// Section 8: A "correctly noexcept" user type — the kind you actually write.
// ---------------------------------------------------------------------------
struct Good {
    std::string payload;
    // Implicit move ctor and move assignment are noexcept for any type
    // whose members' move ops are noexcept. std::string is noexcept-movable,
    // so Good's implicit move is noexcept.
};

// ---------------------------------------------------------------------------

int main() {
    std::cout << std::boolalpha;

    // =====================================================================
    std::cout << "========== 1. The shape of std::move_if_noexcept ==========\n";
    // ---------------------------------------------------------------------
    // std::move_if_noexcept<T>(x) is declared roughly as:
    //
    //   template<class T>
    //   constexpr T&& move_if_noexcept(T& x) noexcept {
    //       return std::is_nothrow_move_constructible_v<T>
    //                  ? std::move(x)
    //                  : static_cast<T&&>(x);  // forces copy
    //   }
    //
    // It returns an rvalue reference iff T's move constructor is
    // nothrow. Otherwise it returns an lvalue reference, which forces
    // the caller to fall back to a copy.
    //
    // The point isn't to be called by user code; it's to be called by
    // standard library containers (vector, deque, string) when they
    // reallocate. It's an implementation hook, but the *behavior* is
    // observable and well worth understanding.

    std::cout << "Declaration (cppreference simplified):\n"
              << "  template<class T>\n"
              << "  constexpr T&& move_if_noexcept(T& x) noexcept {\n"
              << "    return std::is_nothrow_move_constructible_v<T>\n"
              << "        ? std::move(x) : static_cast<T&&>(x);\n"
              << "  }\n\n"
              << "Return type: T&& when T's move ctor is noexcept,\n"
              << "             T&  (lvalue) otherwise.\n";

    // =====================================================================
    std::cout << "\n========== 2. Why it exists: strong exception safety ==========\n";
    // ---------------------------------------------------------------------
    // The classic case is std::vector<T>::push_back when the vector needs
    // to grow. The implementation is roughly:
    //
    //   1. allocate a bigger buffer
    //   2. move-construct each existing element into the new buffer
    //   3. destroy the old elements
    //   4. deallocate the old buffer
    //   5. install the new buffer
    //
    // If step 2 throws halfway through, the vector is in a weird state:
    // some elements have been moved to the new buffer, others are still
    // in the old buffer (which is about to be deallocated), and the
    // element that was being moved was modified by the failing move.
    // That's not strong exception safety.
    //
    // The fix: if T's move constructor is noexcept, a failed move leaves
    // the source unchanged (because the source's destructor will run
    // normally on the moved-from object), so the vector can roll back.
    // If T's move constructor is *potentially throwing*, std::move_if_noexcept
    // routes the operation through a copy constructor instead — copies
    // are required to leave the source unchanged on failure, so the
    // vector can always roll back.
    //
    // The trade-off: in the non-noexcept case, you pay for a copy
    // instead of a move. In the noexcept case, you get the move and
    // still have strong exception safety.

    std::cout << "When std::vector grows, it must relocate existing elements.\n"
              << "If a move constructor throws mid-relocation, the vector is\n"
              << "corrupted (some elements moved, some not, old buffer freed).\n"
              << "std::move_if_noexcept is the standard library's tool to\n"
              << "guarantee strong exception safety:\n"
              << "  - noexcept move -> use the move, can roll back safely\n"
              << "  - throwing move -> use a copy, slower but safe\n";

    // =====================================================================
    std::cout << "\n========== 3. ThrowingMove: move_if_noexcept falls back to copy ==========\n";
    // ---------------------------------------------------------------------
    {
        ThrowingMove src{"hello"};
        std::cout << "  -- creating a destination via std::move_if_noexcept(src) --\n";
        // std::move_if_noexcept returns an lvalue reference for ThrowingMove,
        // because is_nothrow_move_constructible_v<ThrowingMove> is false.
        ThrowingMove dst = std::move_if_noexcept(src);
        std::cout << "  -- dst created --\n";
        std::cout << "  src.payload = '" << src.payload << "'\n";
        std::cout << "  dst.payload = '" << dst.payload << "'\n";
        // Note: src is unchanged because we copied, not moved.
    }

    // =====================================================================
    std::cout << "\n========== 4. NonThrowingMove: move_if_noexcept picks the move ==========\n";
    // ---------------------------------------------------------------------
    {
        NonThrowingMove src{"world"};
        std::cout << "  -- creating a destination via std::move_if_noexcept(src) --\n";
        // std::move_if_noexcept returns an rvalue reference, so this is a move.
        NonThrowingMove dst = std::move_if_noexcept(src);
        std::cout << "  -- dst created --\n";
        std::cout << "  src.payload = '" << src.payload << "' (moved-from, empty)\n";
        std::cout << "  dst.payload = '" << dst.payload << "'\n";
    }

    // =====================================================================
    std::cout << "\n========== 5. CopyOnly: move_if_noexcept copies (no move exists) ==========\n";
    // ---------------------------------------------------------------------
    {
        CopyOnly src{"only"};
        std::cout << "  -- creating a destination via std::move_if_noexcept(src) --\n";
        CopyOnly dst = std::move_if_noexcept(src);
        std::cout << "  -- dst created --\n";
        std::cout << "  src.payload = '" << src.payload << "'\n";
        std::cout << "  dst.payload = '" << dst.payload << "'\n";
    }

    // =====================================================================
    std::cout << "\n========== 6. is_nothrow_move_constructible_v at compile time ==========\n";
    // ---------------------------------------------------------------------
    std::cout << "is_nothrow_move_constructible_v<ThrowingMove>   = "
              << std::is_nothrow_move_constructible_v<ThrowingMove>   << "\n";
    std::cout << "is_nothrow_move_constructible_v<NonThrowingMove> = "
              << std::is_nothrow_move_constructible_v<NonThrowingMove> << "\n";
    std::cout << "is_nothrow_move_constructible_v<CopyOnly>        = "
              << std::is_nothrow_move_constructible_v<CopyOnly>        << "\n";
    std::cout << "is_nothrow_move_constructible_v<Good>            = "
              << std::is_nothrow_move_constructible_v<Good>            << "\n";
    std::cout << "is_nothrow_move_constructible_v<std::string>     = "
              << std::is_nothrow_move_constructible_v<std::string>     << "\n";
    std::cout << "is_nothrow_move_constructible_v<int>             = "
              << std::is_nothrow_move_constructible_v<int>             << "\n";

    // =====================================================================
    std::cout << "\n========== 7. vector<bool>... and the trade-off ==========\n";
    // ---------------------------------------------------------------------
    // std::vector<bool> is a famous special case. Its "reference" is a
    // proxy type, and moving a proxy can require swapping bits in a
    // packed representation, which the standard library can't make
    // noexcept. So vector<bool>::push_back uses copy (via proxy) rather
    // than move on reallocation. That's why vector<bool> is sometimes
    // slower than expected.
    //
    // For normal types, this trade-off is exactly what std::move_if_noexcept
    // is built for: a class author who marks their move ctor noexcept
    // gets the move; one who doesn't pays for the copy. The standard
    // library does the right thing in both cases.

    std::cout << "vector<bool>'s element proxy has a throwing move (bit swap),\n"
              << "so push_back copies proxies on reallocation rather than moves.\n"
              << "This is exactly the trade-off std::move_if_noexcept encodes.\n";

    // Demonstration: vector of ThrowingMove vs. vector of NonThrowingMove
    // during a growth event. We use reserve to force a reallocation.
    {
        std::cout << "\n  -- vector<ThrowingMove> growing via push_back --\n";
        std::vector<ThrowingMove> v;
        v.reserve(2);  // 2 slots, both empty
        std::cout << "  -- pushing 4 elements, each push_back will grow & relocate --\n";
        v.emplace_back("a");
        v.emplace_back("b");
        v.emplace_back("c");  // growth happens here
        v.emplace_back("d");  // and here
        std::cout << "  -- final size: " << v.size() << ", capacity: " << v.capacity() << " --\n";
    }
    {
        std::cout << "\n  -- vector<NonThrowingMove> growing via push_back --\n";
        std::vector<NonThrowingMove> v;
        v.reserve(2);
        std::cout << "  -- pushing 4 elements --\n";
        v.emplace_back("a");
        v.emplace_back("b");
        v.emplace_back("c");  // growth: should use moves
        v.emplace_back("d");
        std::cout << "  -- final size: " << v.size() << ", capacity: " << v.capacity() << " --\n";
    }
    // The Tracer output above is the key. Compare the number of MOVE
    // lines (NonThrowingMove) vs. COPY lines (ThrowingMove) during the
    // reallocations: NonThrowingMove should show moves, ThrowingMove
    // should show copies.

    // =====================================================================
    std::cout << "\n========== 8. The default rule for your own types ==========\n";
    // ---------------------------------------------------------------------
    // The rule of thumb is:
    //   1. If your type manages a resource (owns memory, a file handle,
    //      a mutex, etc.), your move constructor should be noexcept.
    //      Throwing after stealing a resource from the source leaves
    //      the source in a corrupted state, which is exactly what
    //      std::move_if_noexcept is designed to avoid.
    //   2. If your type is a simple value (POD-like), it doesn't really
    //      matter — the implicit move ctor is noexcept when all the
    //      members' move ctors are noexcept, and you usually get this
    //      for free.
    //   3. If you write a move ctor manually, *add the noexcept keyword*.
    //      It's the difference between std::vector moving your type
    //      and copying it.
    //
    // In code: it's two characters. `MyType(MyType&&) noexcept { ... }`.
    // In benchmarks: it can be the difference between a 2x speedup
    // and 1x during any vector/deque reallocation.

    std::cout << "Rule of thumb:\n"
              << "  1. Resource-managing types: mark move ctor noexcept.\n"
              << "  2. Simple value types: implicit move ctor is already noexcept.\n"
              << "  3. Manual move ctor: add `noexcept`. Always.\n\n"
              << "Two characters in code; can double your throughput under\n"
              << "any std::vector reallocation. Don't skip it.\n";

    std::cout << "\nDone.\n";
    return 0;
}
