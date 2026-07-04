psp_span_lib 0.5.0
==================

A small teaching library: `psp::Span<T, Extent>` (a hand-written,
modern-C++17 stand-in for C++20's `std::span`), shipped as a static
archive with explicit instantiations, install rules, and a CPack
package descriptor.


WHAT YOU GET
============

    include/
        psp_span/span.h              <- the public header
    lib/
        libpsp_span_lib.a            <- static archive (pre-instantiated)
        cmake/psp_span_lib/
            psp_span_libConfig.cmake
            psp_span_libConfigVersion.cmake
            psp_span_libTargets.cmake
            psp_span_libTargets-release.cmake (or -debug.cmake)
    License.txt                      <- this package's license
    Readme.txt                       <- this file


WHY IT EXISTS
=============

This library is the result of a C++ learning arc that built up from
a hand-written prototype to a redistributable CMake package. The arc:

    Jun 14   psp::Span<T,Extent> by hand (prototype)
    Jun 28   CMake INTERFACE library (header-only)
    Jun 29   Consumer-side `extern template` suppression
    Jun 30   STATIC library + library-owned instantiations (v0.2.0)
    Jul  1   install rules + find_package() consumer (v0.3.0)
    Jul  2   find_package(fmt) — third-party package, system-installed
    Jul  3   CPack TGZ packaging (v0.4.0)
    Jul  4   Resource files (License + Readme) bundled into the
             package (v0.5.0)  <-- THIS RELEASE

The point is not the library itself — it is the package shape.
Every CMake feature in the arc (INSTALL_INTERFACE, EXPORT,
configure_package_config_file, CPACK_GENERATOR, CPACK_RESOURCE_FILE_*)
is here to answer one question: "how does a library travel from a
developer's machine to a downstream user's machine, intact?"


HOW TO USE THE LIBRARY FROM A CONSUMER
======================================

In your CMakeLists.txt:

    cmake_minimum_required(VERSION 3.16)
    project(my_app LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)

    find_package(psp_span_lib REQUIRED)
    add_executable(my_app src/main.cpp)
    target_link_libraries(my_app PRIVATE psp_span_lib::psp_span_lib)

In your source:

    #include <psp_span/span.h>

    void f() {
        int buf[6] = {10, 20, 30, 40, 50, 60};
        psp::Span<int> s(buf, 6);
        // s.size(), s.front(), s.back(), s[i], s.subspan(off, n),
        // s.first(n), s.last(n), s.data(), s.empty() ...
    }

That's it. The CMake imported target `psp_span_lib::psp_span_lib`
brings the include directory and `-std=c++17` automatically.


HOW TO INSTALL FROM THIS ARCHIVE
================================

This archive is a CPack TGZ (POSIX tarball). Extract it to any
directory and point `CMAKE_PREFIX_PATH` at the parent of the
extracted tree:

    $ tar xzf psp_span_lib-0.5.0-Darwin.tar.gz -C /opt/libs
    $ cd /opt/libs/psp_span_lib-0.5.0-Darwin    # confirm layout

In your consumer's CMake configure step:

    $ cmake -S . -B build \
        -DCMAKE_PREFIX_PATH=/opt/libs         # NOT the wrapped dir

`find_package(psp_span_lib REQUIRED)` will find the package config
file at `lib/cmake/psp_span_lib/psp_span_libConfig.cmake`.


PRE-INSTANTIATED SPECIALIZATIONS
================================

`libpsp_span_lib.a` contains explicit instantiations for:

    template class psp::Span<int>;          // dynamic extent
    template class psp::Span<const int>;     // dynamic extent
    template class psp::Span<double>;        // dynamic extent

Consumers can opt into compile-time suppression by declaring:

    extern template class psp::Span<int>;

in any TU that uses `psp::Span<int>`. The linker satisfies those
calls from the archive. This makes the archive an OPT-IN speedup
for consumers — without it, the consumer still parses the full
header and emits its own strong definitions.


KNOWN CAVEAT (honest report)
============================

`sizeof(psp::Span<int, 5>) == 16`, not 8. The fix is a small
`std::conditional_t<Extent == dynamic_extent, std::size_t, ...>`
in the data member layout. Tracked as a future lesson; not in
the v0.5.0 release.


CONTACT
=======

GitHub:  https://github.com/ArloNOppie/KanopiLearningCPPLessons
Path:    late-may/cpp_practice/psp_span_lib/