# CMakeLists.txt — today's P-2026-11-27 lesson:
#
#   try_format_restricted_table_value — what is a restricted table FOR?
#
#   Closes Nov 25's item (26): "does ANY restricted table earn its keep?
#   what happens if they are DELETED?"
#
#   A restricted table can be deleted, and a deleted table answers
#   exactly one thing: "no code point has this property".  So the
#   deletion has a LEDGER:
#
#     the OVER-claims  (the arc says yes, libc++ says no)  are FIXED
#     the correct claims (both say yes)                  are BROKEN
#     the UNDER-claims (libc++ says yes, the arc says no) do not change
#
#   so a table earns its keep when it is right more often than it is
#   wrong, and the net worth is `agree - over`:
#
#     kWidth2Restricted        +174449      kIndicConsonant   +199
#     kExtendedPictographic      +1631      kIndicLinker         -3
#     kPrepend                      +27     is_cluster_extend  +475
#
#   FIVE of the six earn their keep.  kIndicLinker does NOT: it is
#   right about 6 code points and wrong about 9, so deleting it would
#   make the arc's rule RIGHT nine times and wrong six times.
#
#   And the PRODUCT does not read any of them: the ONE rule consults
#   libc++ 21's FULL tables behind `gb::props_full` / `gb::width_full`,
#   so deleting the arc's own six moves the product by exactly ZERO
#   answers, while deleting LIBC++'s table moves it on exactly the
#   disagreement code points:
#
#     the England flag (U+1F3F4 + "gbeng" + U+E007F, 28 bytes):
#       libc++ charges       2 columns
#       the arc's ONE rule   2 columns  (correct)
#       libc++'s table gone  8 columns  (the arc falls back to its own
#                         573-code-point table, which does not know the
#                         Tags block -- Nov 25's error, resurrected)
#
#   The lesson: the six restricted tables are EXHIBITS.  They are kept
#   so a lesson can point at one and say "this is what the arc used to
#   believe" -- not because the product consults them.
#
# Where this fits in the arc
# --------------------------
#   - Sep 6            petra::FormatError / FormatErrorKind (4 kinds).
#   - Sep 8 .. Oct 24  the try_format_* family and the two-pass
#                      (measure, then write) implementation.
#   - Oct 25           the buffer concept; the raw char*+cap axis.
#   - Oct 26           cell [4]: runtime pad + compile-time format.
#   - Oct 27           the contract comparison against std::format_to_n.
#   - Oct 28           the ERROR TAXONOMY -- a constexpr grammar scanner
#                      plus a libc++-specific message table.
#   - Oct 30           the TYPE-AWARE scanner -- the argument's category
#                      decides the spec-vs-argument class.
#   - Nov 7            the POLICY TABLE -- one mask table plus
#                      per-implementation override rows.
#   - Nov 12           the CARRIED VERDICT -- the scanner's silence is
#                      carried into the fallback as silence, and the
#                      numeric ceiling joins the policy table.
#   - Nov 13           the UNICODE FILL RULE -- the fill is decoded as a
#                      scalar value; the ill-formed fill is structural.
#   - Nov 14           the COMBINATION GRID -- the 21 PAIRS of options,
#                      and the align/zero rule that only exists where
#                      two options meet.
#   - Nov 15           the TRIPLE GRID -- the 35 TRIPLES of options, and
#                      the ONE cell where the fill's rule and the
#                      align/zero rule meet.
#   - Nov 16           the FILL BEHAVIOUR SPACE -- the fill's enforcement
#                      as ONE datum, and the first grid in the arc whose
#                      axis is the POLICY rather than the spec.
#   - Nov 17           the FIELD WIDTH -- the width's UNIT, measured
#                      against the linked stdlib: bytes agree on 1 of 28
#                      cells, code points on 11 of 28, COLUMNS on 28 of 28.
#   - Nov 18           the PRECISION -- the same unit, the same table,
#                      and a cluster-boundary cut.  Plus UAX #29's GB11,
#                      which the arc's own ZWJ rule got wrong.
#   - Nov 19           the ONE COLUMN RULE -- the two rules become one
#                      function, the defective rule is deleted.
#   - Nov 20           the RULE ORDER -- UAX #29's rules are an ORDERED
#                      list and a rule CONSUMES its context.
#   - Nov 21           the CONTROL CHARACTERS -- GB3, GB4 and GB5, plus
#                      libc++ 21's ASCII fast path.
#   - Nov 22           the WIDTH TABLE -- the arc's restricted table is
#                      not a restriction of libc++'s full one.
#   - Nov 23           the PROPERTY TABLES -- the same census over the
#                      arc's FOUR remaining restricted tables.
#   - Nov 24           the EXTEND TABLE -- the sixth restricted table,
#                      and the FIRST one whose error makes the arc
#                      charge TOO MUCH rather than too little.
#   - Nov 25           the TAGS BLOCK -- the widest gap of all, and the
#                      first battery drawn from a RUN rather than from a
#                      single code point.
#   - TODAY            the VALUE OF A RESTRICTED TABLE -- delete each
#                      of the six and measure what the deletion is
#                      worth, and what the PRODUCT loses (nothing).
#
# What today measures (see the .md for the detail):
#
#   - The LEDGER: over the whole code point space, each table's
#     agree / over / under counts, and the net `agree - over`.
#   - The DELETION BATTERY: 774 cells fixed, 289 broken, 11,399
#     unchanged -- the deletion is a net LOSS for five of the six.
#   - The PRODUCT TEST: deleting the arc's own tables moves the
#     product by ZERO answers; deleting libc++'s moves it on exactly
#     the disagreement code points.
#
# Build it
# --------
#   cmake -S . -B build -DENABLE_STRICT_WARNINGS=ON
#   cmake --build build
#   ./build/P-2026-11-27-restricted-table-value
#
#   # ASan/UBSan build (one-shot):
#   cmake -S . -B build-asan -DENABLE_STRICT_WARNINGS=ON -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-11-27-restricted-table-value

cmake_minimum_required(VERSION 3.20)
project(P-2026-11-27-restricted-table-value
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apple Clang 21 / libc++ 21 requires -fexperimental-library for
# std::println and std::expected.  It is a libc++-only flag, so it is
# added for Clang only: the lesson is BUILT AND MEASURED under
# libstdc++ 15 too, and a flag g++ does not know would stop that.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-fexperimental-library)
    add_link_options(-fexperimental-library)
endif()

# Optional strict-warning mode toggle.
option(ENABLE_STRICT_WARNINGS
       "Enable -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
       OFF)
if(ENABLE_STRICT_WARNINGS)
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror
        -Wshadow -Wconversion -Wsign-conversion)
endif()

# Optional ASan/UBSan mode toggle.
option(ENABLE_ASAN
       "Enable -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer"
       OFF)
if(ENABLE_ASAN)
    add_compile_options(
        -fsanitize=address -fsanitize=undefined
        -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address -fsanitize=undefined)
endif()

add_executable(P-2026-11-27-restricted-table-value
               P-2026-11-27-restricted-table-value.cpp)
