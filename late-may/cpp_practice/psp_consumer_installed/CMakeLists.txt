# CMakeLists.txt — psp_consumer_installed
#
# A standalone consumer project that does NOT add_subdirectory()
# the psp_span_lib source tree. Instead it relies entirely on
# `find_package(psp_span_lib REQUIRED)`, just like a downstream
# user would after `cmake --install`.
#
# This is the structural statement of the Jul 1 lesson: a library
# with install rules is a *package* — it can be consumed by a
# project that has no awareness of how or where it was built.
#
# The only requirement is that psp_span_lib was installed somewhere
# on CMAKE_PREFIX_PATH (e.g. /tmp/psp_install, /usr/local, etc.).
# We point CMAKE_PREFIX_PATH at /tmp/psp_install in the run script.

cmake_minimum_required(VERSION 3.16)
project(psp_consumer_installed
    VERSION 1.0.0
    DESCRIPTION "Consumer of installed psp_span_lib via find_package()"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The headline call. After this line, the imported target
# `psp_span_lib::psp_span_lib` exists and carries the include
# path + compile features the consumer needs.
find_package(psp_span_lib REQUIRED)

# A single executable driver. The driver source
# (P-2026-07-01-cmake-install-find-package.cpp) does no build-time
# tricks — it just includes the public header and links the
# imported target. That's all find_package() needs to work with.
add_executable(P-2026-07-01-cmake-install-find-package
    P-2026-07-01-cmake-install-find-package.cpp
)

# Link via the namespaced imported target, which is the
# find_package()-style convention (vs the bare `psp_span_lib`
# we'd use if we were add_subdirectory()ing the source tree).
target_link_libraries(P-2026-07-01-cmake-install-find-package
    PRIVATE psp_span_lib::psp_span_lib
)

message(STATUS "==================================================")
message(STATUS " psp_consumer_installed ${PROJECT_VERSION} configured.")
message(STATUS " psp_span_lib found at: ${psp_span_lib_DIR}")
message(STATUS " psp_span_lib version: ${psp_span_lib_VERSION}")
message(STATUS "==================================================")