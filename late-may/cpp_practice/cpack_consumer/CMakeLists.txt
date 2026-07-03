# CMakeLists.txt — cpack_consumer
#
# A standalone consumer project that proves psp_span_lib can be
# installed + used from a CPack-produced .tar.gz archive, with no
# awareness of how or where the library was built.
#
# This is the structural statement of the Jul 3 lesson: a library
# that is not only find_package()-able (Jul 1) but also
# REDISTRIBUTABLE — the archive is the contract.
#
# The contract, in three lines:
#   1. psp_span_lib ships a .tar.gz with a known layout (lib/, include/, ...).
#   2. The archive's prefix path is added to CMAKE_PREFIX_PATH.
#   3. find_package(psp_span_lib REQUIRED) resolves the package from that prefix.
#
# No source-tree dependency. No /tmp/psp_install. No build dir.
# Just: unpack, set prefix, configure, build.
#
# The only thing in this CMakeLists that differs from
# psp_consumer_installed/ (Jul 1) is the introductory comment
# block. The actual find_package / link / executable lines are
# identical — that's the point. The "shape of a consumer" doesn't
# change between install-tree and archive-tree; what changes is
# how the archive got there. Consumers are oblivious.

cmake_minimum_required(VERSION 3.16)
project(cpack_consumer
    VERSION 1.0.0
    DESCRIPTION "Consumer of psp_span_lib installed from a CPack-produced .tar.gz"
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
# (P-2026-07-03-cpack-package-and-install-from-archive.cpp) does no
# build-time tricks — it just includes the public header and links
# the imported target. That's all find_package() needs to work with.
add_executable(P-2026-07-03-cpack-package-and-install-from-archive
    P-2026-07-03-cpack-package-and-install-from-archive.cpp
)

# Link via the namespaced imported target, which is the
# find_package()-style convention.
target_link_libraries(P-2026-07-03-cpack-package-and-install-from-archive
    PRIVATE psp_span_lib::psp_span_lib
)

message(STATUS "==================================================")
message(STATUS " cpack_consumer ${PROJECT_VERSION} configured.")
message(STATUS " psp_span_lib found at: ${psp_span_lib_DIR}")
message(STATUS " psp_span_lib version: ${psp_span_lib_VERSION}")
message(STATUS "==================================================")