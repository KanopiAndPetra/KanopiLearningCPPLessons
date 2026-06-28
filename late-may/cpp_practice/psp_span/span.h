// psp_span/span.h — a header-only C++17 implementation of std::span<T, Extent>.
//
// This file is the entire library. There is no .cpp, no .o, no archive.
// The CMake build for this is `add_library(psp_span INTERFACE)` plus
// `target_include_directories(psp_span INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})`.
//
// The class itself was originally prototyped on 2026-06-14 in
// P-2026-06-14-std-span-by-hand.cpp. Today (2026-06-28) it is promoted
// to a real header-only library so the build-system lesson can show
// what `INTERFACE` means in CMake terms.
//
// C++17 — `std::span` is C++20, so this is hand-rolled.

#ifndef PSP_SPAN_H_INCLUDED
#define PSP_SPAN_H_INCLUDED

#include <array>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace psp {

// `dynamic_extent` is the sentinel for "size is a runtime field, not a
// compile-time constant." In std::span it's std::dynamic_extent; we name
// ours with the psp_ prefix to avoid name clashes in case both this
// header and <span> are visible.
inline constexpr std::size_t dynamic_extent = static_cast<std::size_t>(-1);

template <class T, std::size_t Extent = dynamic_extent>
class Span {
    static_assert(Extent == dynamic_extent || Extent > 0,
                  "Span extent must be dynamic or a positive static N");

    T*          data_;
    std::size_t size_;  // ignored when Extent is a static value, but
                        // stored anyway so size() / at() / iterators work
                        // uniformly across the static and dynamic cases.

public:
    // The static extent (or dynamic_extent) for introspection.
    static constexpr std::size_t extent = Extent;

    // -- ctors --------------------------------------------------------------

    // Default ctor: empty span.
    constexpr Span() noexcept
        : data_(nullptr), size_(0) {
        static_assert(Extent == dynamic_extent || Extent == 0,
                      "default-constructed Span can only be empty "
                      "when Extent == 0 (or dynamic)");
    }

    // From a (ptr, len) pair. Length is fixed at construction.
    constexpr Span(T* ptr, std::size_t len) noexcept
        : data_(ptr), size_(len) {}

    // From a C-array. The static extent is deduced into the static Span.
    template <std::size_t N,
              typename = std::enable_if_t<
                  (Extent == dynamic_extent || Extent == N) &&
                  std::is_convertible<T*, T*>::value>>
    constexpr Span(T (&arr)[N]) noexcept
        : data_(arr), size_(N) {}

    // From std::array<U, N> (non-const). Accepts any U convertible to T
    // so that Span<const int> can be built from std::array<int, N>.
    // The SFINAE rejects std::array<const T, N> (which is ill-formed).
    template <class U, std::size_t N,
              typename = std::enable_if_t<
                  (Extent == dynamic_extent || Extent == N) &&
                  std::is_convertible<U*, T*>::value &&
                  !std::is_const<U>::value>>
    constexpr Span(std::array<U, N>& arr) noexcept
        : data_(arr.data()), size_(N) {}

    // From std::array<U, N> (const). Same shape.
    template <class U, std::size_t N,
              typename = std::enable_if_t<
                  (Extent == dynamic_extent || Extent == N) &&
                  std::is_convertible<const U*, T*>::value &&
                  !std::is_const<U>::value>>
    constexpr Span(const std::array<U, N>& arr) noexcept
        : data_(arr.data()), size_(N) {}

    // From std::vector<U>. SFINAE keeps us away from std::vector<const T>,
    // which is ill-formed.
    template <class U,
              typename = std::enable_if_t<
                  Extent == dynamic_extent &&
                  std::is_convertible<U*, T*>::value &&
                  !std::is_const<U>::value>>
    Span(std::vector<U>& v) noexcept
        : data_(v.data()), size_(v.size()) {}

    // From another Span with a different (but compatible) extent.
    template <std::size_t OtherExtent,
              typename = std::enable_if_t<
                  Extent == dynamic_extent ||
                  OtherExtent == dynamic_extent ||
                  Extent == OtherExtent>>
    constexpr Span(Span<T, OtherExtent> other) noexcept
        : data_(other.data()), size_(other.size()) {}

    // -- observers ----------------------------------------------------------

    constexpr T*          data()  const noexcept { return data_; }
    constexpr std::size_t size()  const noexcept { return size_; }
    constexpr bool        empty() const noexcept { return size_ == 0; }

    constexpr std::size_t size_bytes() const noexcept {
        return size_ * sizeof(T);
    }

    // -- accessors ----------------------------------------------------------

    constexpr T& operator[](std::size_t i) const noexcept {
        return data_[i];
    }

    constexpr T& at(std::size_t i) const {
        if (i >= size_) {
            throw std::out_of_range("psp::Span::at: index out of range");
        }
        return data_[i];
    }

    constexpr T& front() const noexcept { return data_[0]; }
    constexpr T& back()  const noexcept { return data_[size_ - 1]; }

    // -- subviews -----------------------------------------------------------

    template <std::size_t Count>
    constexpr Span<T, Count> first() const noexcept {
        static_assert(Count <= Extent || Extent == dynamic_extent,
                      "Count cannot exceed the static extent");
        return Span<T, Count>(data_, Count);
    }

    constexpr Span<T, dynamic_extent> first(std::size_t count) const noexcept {
        return Span<T, dynamic_extent>(data_, count);
    }

    template <std::size_t Count>
    constexpr Span<T, Count> last() const noexcept {
        static_assert(Count <= Extent || Extent == dynamic_extent,
                      "Count cannot exceed the static extent");
        return Span<T, Count>(data_ + (size_ - Count), Count);
    }

    constexpr Span<T, dynamic_extent> last(std::size_t count) const noexcept {
        return Span<T, dynamic_extent>(data_ + (size_ - count), count);
    }

    template <std::size_t Offset, std::size_t Count = dynamic_extent>
    constexpr auto subspan() const noexcept {
        static_assert(Offset <= Extent || Extent == dynamic_extent,
                      "Offset cannot exceed the static extent");
        constexpr std::size_t NewExtent =
            (Count == dynamic_extent)
                ? (Extent == dynamic_extent ? dynamic_extent
                                            : Extent - Offset)
                : Count;
        return Span<T, NewExtent>(data_ + Offset,
            (NewExtent == dynamic_extent) ? (size_ - Offset) : NewExtent);
    }

    constexpr Span<T, dynamic_extent>
    subspan(std::size_t offset,
            std::size_t count = dynamic_extent) const noexcept {
        return Span<T, dynamic_extent>(
            data_ + offset,
            count == dynamic_extent ? (size_ - offset) : count);
    }

    // -- iterators ----------------------------------------------------------

    constexpr T* begin() const noexcept { return data_; }
    constexpr T* end()   const noexcept { return data_ + size_; }
    constexpr std::reverse_iterator<T*> rbegin() const noexcept {
        return std::reverse_iterator<T*>(end());
    }
    constexpr std::reverse_iterator<T*> rend() const noexcept {
        return std::reverse_iterator<T*>(begin());
    }
};

// -- deduction guides (C++17) -----------------------------------------------

template <class T, std::size_t N>
Span(T (&)[N]) -> Span<T, N>;

template <class T, std::size_t N>
Span(std::array<T, N>&) -> Span<T, N>;

template <class T, std::size_t N>
Span(const std::array<T, N>&) -> Span<T, N>;

}  // namespace psp

#endif  // PSP_SPAN_H_INCLUDED