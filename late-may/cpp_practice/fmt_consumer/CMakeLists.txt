# fmt_consumer/CMakeLists.txt
#
# Petra's C++ practice 2026-07-02.
#
# This is a separate CMake project — NOT added via add_subdirectory() of
# the parent late-may/cpp_practice/CMakeLists.txt. Just like
# psp_consumer_installed/ from Jul 1, this is a *standalone* project
# that finds fmt on the system via find_package().
#
# The whole lesson is here in three lines:
#   1. find_package(fmt REQUIRED)
#   2. add_executable(... <driver>.cpp)
#   3. target_link_libraries(... PRIVATE fmt::fmt)
# Everything else is just for diagnostics and ASan.

cmake_minimum_required(VERSION 3.16)
project(fmt_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ----------------------------------------------------------------------
# Build-time toggle for AddressSanitizer + UBSan.
#   cmake -S . -B build -DENABLE_ASAN=ON
# ----------------------------------------------------------------------
option(ENABLE_ASAN "Build with -fsanitize=address,undefined" OFF)
if(ENABLE_ASAN)
    add_compile_options(
        -fsanitize=address -fsanitize=undefined
        -fno-omit-frame-pointer
        -g
    )
    add_link_options(
        -fsanitize=address -fsanitize=undefined
    )
    message(STATUS "ASan + UBSan: ENABLED")
else()
    message(STATUS "ASan + UBSan: disabled")
endif()

# ----------------------------------------------------------------------
# The lesson itself: find_package() consumes the upstream fmt package.
# ----------------------------------------------------------------------
find_package(fmt REQUIRED)

# fmt::fmt is the compiled-library target (links libfmt). fmt also ships
# fmt::fmt-header-only — same API but header-only (slower, larger TU).
# We use fmt::fmt here to actually link against the .a/.so.
message(STATUS "fmt found:        ${fmt_FOUND}")
message(STATUS "fmt version:      ${fmt_VERSION}")
message(STATUS "fmt imported targets: fmt::fmt, fmt::fmt-header-only")
message(STATUS "fmt include dirs: ${fmt_INCLUDE_DIRS}")

# The driver.
add_executable(P-2026-07-02-fmt-find-package
    P-2026-07-02-fmt-find-package.cpp
)

# PRIVATE because the driver is an executable — fmt is not propagated to
# "downstream" targets (there aren't any, but the convention matters).
target_link_libraries(P-2026-07-02-fmt-find-package PRIVATE fmt::fmt)

# Tell the compiler to be strict — same flags the parent repo uses.
target_compile_options(P-2026-07-02-fmt-find-package PRIVATE
    -Wall -Wextra -Wpedantic
)

# Make the binary land at a predictable path: build/P-2026-07-02-fmt-find-package
# (no nested Debug/ subdir).
set_target_properties(P-2026-07-02-fmt-find-package PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)