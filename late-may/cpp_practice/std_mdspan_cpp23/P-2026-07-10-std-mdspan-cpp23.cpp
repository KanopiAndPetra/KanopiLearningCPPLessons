// P-2026-07-10 — std::mdspan (C++23): multi-dimensional view over a contiguous buffer.
//
// std::span   = 1-D non-owning view          (C++20)
// std::mdspan = N-D non-owning view          (C++23)
//
// A mdspan is a (pointer, mapping) pair. The mapping is what tells you
// how to translate a multi-index (i, j, k, ...) into a single offset into
// the contiguous buffer. The mapping also owns the extents (shape).
//
// Sections:
//   0. Toolchain check
//   1. The 2-D minimum viable mdspan (extents, data ptr)
//   2. Indexing — mdspan[i,j] and the rank-only operator[] story
//   3. Extents — rank, rank_dynamic, static-vs-dynamic
//   4. Layout policies — row-major vs column-major vs stride (sizeof + offset probe)
//   5. Manual slicing — what std::submdspan would do, expressed via a new mdspan
//   6. std::span compatibility — mdspan.data_handle() + std::as_bytes() bridge
//   7. Function parameters — mdspan<const T, dextents<...>> as a typed multi-D argument
//   8. Comparison vs psp::Span — where mdspan lives in the same shape family
//
// Build:
//   g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g \
//       -o P-2026-07-10-std-mdspan-cpp23 P-2026-07-10-std-mdspan-cpp23.cpp
//
// ASan build (mirrors the per-session convention from late May):
//   g++ -Wall -Wextra -Wpedantic -std=c++23 -O0 -g -fsanitize=address \
//       -o P-2026-07-10-std-mdspan-cpp23-asan P-2026-07-10-std-mdspan-cpp23.cpp

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <mdspan>
#include <span>
#include <type_traits>
#include <vector>

namespace {

// ----- helpers --------------------------------------------------------------

// Section labels printed as we go, so the run reads as a log.
int g_section = 0;
void section(const char* title) {
    ++g_section;
    std::cout << "\n== Section " << g_section << ": " << title << " ==\n";
}

// A small matrix printer that works for any mdspan with integral value_type.
template <class MDS>
void print_matrix(const MDS& m, const char* tag) {
    using ext_t = std::remove_cvref_t<decltype(m.extents())>;
    static_assert(ext_t::rank() == 2, "print_matrix assumes rank-2");
    std::cout << "[" << tag << "] "
              << m.extent(0) << "x" << m.extent(1)
              << "  (rank=" << m.extent(0) << "*" << m.extent(1)
              << " = " << (m.extent(0) * m.extent(1)) << ")\n";
    for (std::size_t i = 0; i < m.extent(0); ++i) {
        std::cout << "  ";
        for (std::size_t j = 0; j < m.extent(1); ++j) {
            std::cout << m[i, j];
            if (j + 1 < m.extent(1)) std::cout << " ";
        }
        std::cout << "\n";
    }
}

}  // namespace

int main() {
    // ---- Section 0: toolchain check --------------------------------------
    section("Toolchain check");
#ifdef __cpp_lib_mdspan
    std::cout << "__cpp_lib_mdspan = " << __cpp_lib_mdspan << "\n";
    static_assert(__cpp_lib_mdspan >= 202207,
                  "Petra expects C++23 mdspan (P2630 + P2614R3 etc.).");
#else
#error "this toolchain does not expose __cpp_lib_mdspan; need a C++23 stdlib"
#endif
#ifdef __cpp_lib_span
    std::cout << "__cpp_lib_span  = " << __cpp_lib_span << "  (companion 1-D view)\n";
#endif
    std::cout << "sizeof(std::mdspan<int, std::extents<std::size_t, 3, 4>>) = "
              << sizeof(std::mdspan<int, std::extents<std::size_t, 3, 4>>) << "\n";
    std::cout << "sizeof(std::mdspan<int, std::dextents<std::size_t, 2>>)  = "
              << sizeof(std::mdspan<int, std::dextents<std::size_t, 2>>) << "\n";

    // ---- Section 1: minimum viable 2-D mdspan ----------------------------
    section("2-D minimum viable mdspan");

    // The extents type encodes the shape. dextents<size_t, 2> means
    // "rank-2, both extents dynamic". Same shape as std::span<T>'s
    // dynamic_extent sentinel — these are "shape unknown at compile time".
    using ext2d = std::dextents<std::size_t, 2>;
    using md2d  = std::mdspan<int, ext2d>;

    // The buffer is a plain contiguous array — mdspan does NOT own it.
    int storage[] = {
        11, 12, 13, 14,
        21, 22, 23, 24,
        31, 32, 33, 34,
    };
    constexpr std::size_t R = 3;
    constexpr std::size_t C = 4;

    // mdspan{ptr, extents...} — point at storage, declare shape.
    md2d m{storage, R, C};

    print_matrix(m, "m");

    // ---- Section 2: indexing ---------------------------------------------
    section("Indexing — mdspan[i,j]");

    // mdspan defines operator[] with as many parameters as the rank,
    // so mdspan[i, j] is the natural access. Note that mdspan does NOT
    // expose the "m[i][j]" chained form you might expect from operator[]
    // overloads on nested containers — operator[] here is variadic and
    // requires exactly rank() arguments. This is by design: indexing a
    // multi-dimensional view should be one operation, not N.
    std::cout << "m[0,0]=" << m[0, 0] << "  m[1,2]=" << m[1, 2]
              << "  m[2,3]=" << m[2, 3] << "\n";

    // Mutation goes through the view — mdspan does not own storage, but
    // it gives full access to what it points at.
    m[1, 2] = 999;
    std::cout << "after m[1,2]=999, storage[1*C+2]=" << storage[1 * C + 2]
              << " (proves the view aliases the buffer)\n";
    storage[1 * C + 2] = 23;  // restore

    // Iteration with two nested loops is the canonical pattern.
    std::cout << "iterate m via two nested loops:\n  ";
    for (std::size_t i = 0; i < m.extent(0); ++i) {
        for (std::size_t j = 0; j < m.extent(1); ++j) {
            std::cout << m[i, j] << " ";
        }
    }
    std::cout << "\n";

    // ---- Section 3: extents ----------------------------------------------
    section("Extents — rank, rank_dynamic, static-vs-dynamic");

    // extents is a variadic; each parameter is either a non-negative
    // integer (static extent, baked into the type) or dynamic_extent
    // (dynamic extent, stored at runtime). The number of dynamic_extent
    // parameters is rank_dynamic.
    using ext_mixed = std::extents<std::size_t, 3, std::dynamic_extent>;
    using md_mixed  = std::mdspan<int, ext_mixed>;

    static_assert(ext_mixed::rank() == 2);
    static_assert(ext_mixed::rank_dynamic() == 1);
    static_assert(ext_mixed::static_extent(0) == 3);   // row count is static
    static_assert(ext_mixed::static_extent(1) == std::dynamic_extent);  // col is dynamic

    // Construction only needs to supply the dynamic ones — the static
    // ones are part of the type and don't take a constructor argument.
    int small[3 * 5] = {0};
    md_mixed sm{small, 5};   // 3 rows (static), 5 cols (dynamic)
    std::cout << "sm shape: " << sm.extent(0) << "x" << sm.extent(1)
              << "  (rank=" << sm.rank()
              << ", rank_dynamic=" << sm.extents().rank_dynamic()
              << ", static_extent(0)=" << sm.extents().static_extent(0) << ")\n";
    for (std::size_t i = 0; i < sm.extent(0); ++i) {
        for (std::size_t j = 0; j < sm.extent(1); ++j) {
            sm[i, j] = static_cast<int>(i * 10 + j);
        }
    }
    std::cout << "sm after fill:\n";
    for (std::size_t i = 0; i < sm.extent(0); ++i) {
        std::cout << "  ";
        for (std::size_t j = 0; j < sm.extent(1); ++j) {
            std::cout << sm[i, j] << " ";
        }
        std::cout << "\n";
    }

    // ---- Section 4: layout policies --------------------------------------
    section("Layout policies — row-major vs column-major vs stride");

    // mdspan's mapping is the second template parameter (defaulted to
    // layout_right, i.e. row-major). Three policies in <mdspan>:
    //
    //   layout_right : last index varies fastest   (C / row-major, A[i,j]=A[i*C+j])
    //   layout_left  : first index varies fastest  (Fortran / column-major)
    //   layout_stride: caller supplies strides explicitly
    //
    // The mapping turns a multi-index into a single offset.

    constexpr std::size_t R2 = 2, C2 = 3;
    int buf[6] = {0, 1, 2, 3, 4, 5};

    using md_rm = std::mdspan<int, std::extents<std::size_t, R2, C2>, std::layout_right>;
    using md_cm = std::mdspan<int, std::extents<std::size_t, R2, C2>, std::layout_left>;

    md_rm rm{buf};  // row-major: buf[i*C + j]
    md_cm cm{buf};  // column-major: buf[i + j*R]

    std::cout << "row-major mdspan of 2x3 over [0..5]:\n";
    print_matrix(rm, "rm");

    std::cout << "column-major mdspan of 2x3 over the SAME [0..5]:\n";
    print_matrix(cm, "cm");

    // Both views alias the same 6 ints; they just interpret the offsets
    // differently. The (0,0) element is buf[0] in both. They diverge as
    // soon as we move along the fast axis:
    std::cout << "rm[0,1]=" << rm[0, 1] << "  cm[0,1]=" << cm[0, 1]
              << "  (rm fast-axis=1, cm fast-axis=0; same buf, different index)\n";

    // layout_stride: caller owns the strides. Useful for sub-blocks,
    // transposed views, leading-stride-bigger-than-row cases. The
    // mapping is constructed from an extents object plus the strides.
    std::array<std::size_t, 2> strides_cm = {1, R2};   // column-major-ish
    std::layout_stride::mapping<std::extents<std::size_t, R2, C2>> stride_map{
        std::extents<std::size_t, R2, C2>{},
        strides_cm,
    };
    using md_strided = std::mdspan<
        int, std::extents<std::size_t, R2, C2>, std::layout_stride>;
    md_strided st{buf, stride_map};
    std::cout << "layout_stride mdspan with strides={" << strides_cm[0]
              << "," << strides_cm[1] << "} over 2x3:\n";
    print_matrix(st, "st");

    // sizeof probes: the storage cost of the mdspan itself is dominated
    // by the pointer plus whatever the mapping carries. layout_right and
    // layout_left on static extents compute strides from the type and
    // carry nothing extra; layout_stride always carries an array of
    // rank strides.
    std::cout << "sizeof(md_rm)     = " << sizeof(md_rm) << "\n";
    std::cout << "sizeof(md_cm)     = " << sizeof(md_cm) << "\n";
    std::cout << "sizeof(md_strided)= " << sizeof(md_strided)
              << "  (extra stride[] storage)\n";

    // ---- Section 5: manual slicing ---------------------------------------
    section("Manual slicing — what std::submdspan would do");

    // Note: std::submdspan and std::full_extent_t ship in P2630, which is
    // a post-C++23 proposal. The Apple libc++ here has the C++23 mdspan
    // core but not the slicing helpers. The shape of the operation is
    // "construct a new mdspan over a sub-range of the buffer". For
    // row-major data that's just (ptr + offset, new_extents).
    int img[4 * 4] = {
         0,  1,  2,  3,
        10, 11, 12, 13,
        20, 21, 22, 23,
        30, 31, 32, 33,
    };
    using md4 = std::mdspan<int, std::dextents<std::size_t, 2>>;
    md4 image{img, 4, 4};

    std::cout << "image:\n";
    print_matrix(image, "image");

    // Top-left 2x2 block: re-base the pointer to img + 0*4 + 0, shape (2,2).
    int* tl_ptr = img + 0 * 4 + 0;
    md4 tl{tl_ptr, 2, 2};
    print_matrix(tl, "TL 2x2");

    // Middle 2x3 block: re-base to img + 1*4 + 0, shape (2,3).
    int* mid_ptr = img + 1 * 4 + 0;
    md4 mid{mid_ptr, 2, 3};
    print_matrix(mid, "mid 2x3");

    // Mutation through the subview lands in the same underlying buffer.
    tl[0, 0] = 7777;
    std::cout << "after tl[0,0]=7777, image[0,0]=" << image[0, 0]
              << "  img[0]=" << img[0] << "\n";
    tl[0, 0] = 0;  // restore

    // ---- Section 6: std::span compatibility ------------------------------
    section("std::span compatibility — the manual mdspan->bytes bridge");

    // mdspan exposes the underlying pointer via .data_handle() (the name
    // comes from the policy's "handle" concept — generic over pointer
    // types). For a default mdspan it's just a T*.
    int* raw = m.data_handle();
    std::cout << "m.data_handle() == storage? "
              << std::boolalpha << (raw == storage) << "\n";

    // std::as_bytes only takes std::span, not std::mdspan, so the bridge
    // is "construct a 1-D span over the same buffer, then as_bytes that".
    // This is exactly the kind of bridge you'll write in production
    // when you need to feed mdspan data to a byte-oriented API (memcpy,
    // hashing, serialization).
    std::span<const int> flat_m{storage, R * C};
    std::span<const std::byte> bytes = std::as_bytes(flat_m);
    std::cout << "std::as_bytes(flat_m).size()    = " << bytes.size() << "\n";
    std::cout << "expected (3*4*sizeof(int))      = " << (R * C * sizeof(int)) << "\n";

    // Round-trip: re-interpret the bytes back as ints via std::span<int>.
    // This is only safe because storage is a contiguous int[] with
    // matching element count.
    std::span<const int> as_ints(reinterpret_cast<const int*>(bytes.data()),
                                 bytes.size() / sizeof(int));
    std::cout << "round-trip first 6 ints via std::span: ";
    for (std::size_t i = 0; i < 6; ++i) std::cout << as_ints[i] << " ";
    std::cout << "\n";

    // ---- Section 7: function parameters ----------------------------------
    section("Function parameters — mdspan as a typed multi-D argument");

    // Same decay story as std::span, but multi-D. The function doesn't
    // care how the caller stored the data; it just gets the shape and
    // element access for free.
    auto sum_matrix = [](std::mdspan<const int, std::dextents<std::size_t, 2>> m,
                         const char* tag) {
        long sum = 0;
        for (std::size_t i = 0; i < m.extent(0); ++i) {
            for (std::size_t j = 0; j < m.extent(1); ++j) {
                sum += m[i, j];
            }
        }
        std::cout << "[" << tag << "] " << m.extent(0) << "x" << m.extent(1)
                  << "  sum=" << sum << "\n";
    };

    int a_storage[2 * 3] = {1, 2, 3, 4, 5, 6};
    std::vector<int> v_storage = {10, 20, 30, 40};
    std::array<int, 6> arr_storage = {100, 200, 300, 400, 500, 600};

    sum_matrix(std::mdspan<const int, std::dextents<std::size_t, 2>>{a_storage, 2, 3},
               "C-array");
    sum_matrix(std::mdspan<const int, std::dextents<std::size_t, 2>>{v_storage.data(), 2, 2},
               "std::vector");
    sum_matrix(std::mdspan<const int, std::dextents<std::size_t, 2>>{arr_storage.data(), 2, 3},
               "std::array");

    // ---- Section 8: comparison vs psp::Span ------------------------------
    section("Comparison vs psp::Span — where mdspan lives in the family");

    // psp::Span (Jun 14) and std::span (Jul 9) cover rank-1.
    // std::mdspan (today) is the rank-N generalization. Same shape
    // philosophy: non-owning, views-only, mapped from a contiguous
    // buffer, layout policy in the type, zero-overhead.

    std::cout << "psp::Span          (Jun 14)  : rank-1 hand-rolled in C++17\n";
    std::cout << "std::span          (Jul  9)  : rank-1 standard, C++20\n";
    std::cout << "std::mdspan        (today)   : rank-N standard, C++23\n";
    std::cout << "  -- all three are non-owning views; the only thing that changes\n";
    std::cout << "  -- is the rank of the indexing operation and the storage of the\n";
    std::cout << "  -- mapping (mapping policy is the second template arg of mdspan)\n";

    std::cout << "\nAll sections OK; exiting.\n";
    return 0;
}