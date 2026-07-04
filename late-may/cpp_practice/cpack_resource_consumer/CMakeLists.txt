# CMakeLists.txt — cpack_resource_consumer
#
# A standalone consumer project that proves psp_span_lib v0.5.0
# can be installed + used from a CPack-produced .tar.gz that
# bundles License.txt + Readme.txt into the install root.
#
# This is the structural statement of the Jul 4 lesson: the
# archive is now self-documenting. A consumer (or any auditor)
# who extracts the .tar.gz sees License.txt and Readme.txt at
# the top of the install tree, without having to download
# anything else or visit any URL.
#
# Compared to Jul 3's cpack_consumer/CMakeLists.txt, the only
# thing that changes is the project name + the comment block.
# find_package / link / executable lines are byte-for-byte
# identical — that's the point. The "shape of a consumer"
# doesn't change between resource-bundled and non-bundled
# archives; what changes is what the archive contains, not
# how the consumer finds it.
#
# The verification that License.txt + Readme.txt actually
# arrived is done by inspecting the archive contents with
# `tar tzf` after extraction. The driver doesn't read them
# at compile time — they're metadata for the human
# (and for downstream tooling like `reuse`, SPDX scanners,
# and Debian's licensecheck).

cmake_minimum_required(VERSION 3.16)
project(cpack_resource_consumer
    VERSION 1.0.0
    DESCRIPTION "Consumer of psp_span_lib v0.5.0 — installed from a CPack .tar.gz that bundles License.txt + Readme.txt"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The headline call. After this line, the imported target
# `psp_span_lib::psp_span_lib` exists and carries the include
# path + compile features the consumer needs.
find_package(psp_span_lib REQUIRED)

# A single executable driver. The driver source is the same
# shape as cpack_consumer/ — just a tiny program that uses the
# public header. The interesting verification (License.txt +
# Readme.txt in the archive) is done out-of-band by inspecting
# the archive layout.
add_executable(P-2026-07-04-cpack-resource-files
    P-2026-07-04-cpack-resource-files.cpp
)

target_link_libraries(P-2026-07-04-cpack-resource-files
    PRIVATE psp_span_lib::psp_span_lib
)

# Wire up the version the consumer can read at compile time.
# psp_span_lib_VERSION is set by psp_span_libConfig.cmake when
# find_package() runs. By exposing it as a compile definition,
# the driver prints the actual version it linked against —
# which is the archive's version, not a hardcoded guess.
target_compile_definitions(P-2026-07-04-cpack-resource-files
    PRIVATE PSP_SPAN_LIB_VERSION="${psp_span_lib_VERSION}"
)

message(STATUS "==================================================")
message(STATUS " cpack_resource_consumer ${PROJECT_VERSION} configured.")
message(STATUS " psp_span_lib found at: ${psp_span_lib_DIR}")
message(STATUS " psp_span_lib version: ${psp_span_lib_VERSION}")
message(STATUS "==================================================")