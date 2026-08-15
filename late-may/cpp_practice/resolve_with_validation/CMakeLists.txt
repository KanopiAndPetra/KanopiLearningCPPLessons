# CMakeLists.txt — consumer of psp_span_lib v0.15.0 that adds today's
# P-2026-08-15 lesson:
#
#   psp::json_pointer::resolve_with_validation(
#       pointer,
#       root,
#       schema)
#       -> std::expected<const psp::JsonValue*, SchemaValidatedResolveError>
#
# Where this fits in the arc
# --------------------------
#   - Jul 21: psp::json_pointer::split /  Pointer (RFC 6901)
#             resolve / resolve_mut
#   - Jul 22: psp::json_patch::patch +   Patch engine (RFC 6902)
#             parse_patch_document
#   - Aug  3: psp::json_patch::patch_    DEEP-CLONE transactional
#             atomic + patch_dry_run     wrapper (in-memory)
#   - Aug  9: psp::json_patch::patch_    JOURNAL-AWARE SELF-MOVE
#             journaled_self_move_safe    SAFE wrapper (in-memory)
#   - Aug 10: parse_and_apply_atomic_    INVERSE-JOURNAL STREAMING
#             streaming                   wrapper
#   - Aug 11: parse_and_apply_atomic_    DEEP-CLONE STREAMING
#             streaming_deep_clone         wrapper
#   - Aug 12: psp::json_schema::validate JSON SCHEMA VALIDATION
#                                       (Draft 2020-12, focused
#                                       subset)
#   - Aug 13: psp::json_schema::         SCHEMA-VALIDATED ATOMIC
#             validate_atomic             UPDATE (four-gate
#                                       composition; closes the
#                                       JSON Schema arc)
#   - Aug 14: parse_and_apply_atomic_    STREAMING + SCHEMA-
#             streaming_validated         VALIDATED wire-format
#                                       atomic update (closes the
#                                       entire RFC 6902 + RFC
#                                       6901 + Draft 2020-12
#                                       wire-format arc end-to-end)
#   - Aug 15: psp::json_pointer::        READ-WITH-VALIDATION:
#     TODAY  resolve_with_validation      closes the second
#                                       forward-on item on the
#                                       Aug 13 "Where we go next"
#                                       list ("resolve_with_
#                                       validation — composes the
#                                       v0.11.0 resolve function
#                                       with a per-step validate()
#                                       call to surface a "point
#                                       here only if the value
#                                       passes schema" semantics.
#                                       Useful for read-with-
#                                       validation access
#                                       patterns"). The complete
#                                       read-side arc is now
#                                       closed; library version
#                                       unchanged at v0.15.0;
#                                       future v0.16.0 promotion
#                                       is mechanical — lift
#                                       resolve_with_validation +
#                                       SchemaValidatedResolveError
#                                       + ResolveGate into
#                                       <psp_span/json_pointer.h>;
#                                       bump the version; no new
#                                       error enumerators; reuses
#                                       Aug 12 SchemaErrorContext
#                                       + JsonSchemaError + Aug
#                                       14 validate(); composes
#                                       v0.11.0 resolve with the
#                                       schema layer for a one-
#                                       shot "give me this value
#                                       only if it passes schema"
#                                       access pattern; the
#                                       complete RFC 6901 + RFC
#                                       6902 + Draft 2020-12
#                                       read/write arc is now
#                                       closed end-to-end)
#
# Build (assumes psp_span_lib v0.15.0 is installed at /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-08-15-resolve-with-validation
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install \
#       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-15-resolve-with-validation
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-15-resolve-with-validation

cmake_minimum_required(VERSION 3.16)
project(resolve_with_validation_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.15.0 that adds today's psp::json_pointer::resolve_with_validation composition as the P-2026-08-15 lesson. Closes the second forward-on item on the Aug 13 'Where we go next' list ('resolve_with_validation — composes the v0.11.0 resolve function with a per-step validate() call to surface a point-here-only-if-the-value-passes-schema semantics for read-with-validation access patterns') by wiring v0.11.0 resolve() + Aug 12 validate() into a single one-shot read-with-validation call."
    LANGUAGES CXX
)

find_package(psp_span_lib 0.15 REQUIRED)

add_executable(P-2026-08-15-resolve-with-validation
    P-2026-08-15-resolve-with-validation.cpp
)

target_link_libraries(P-2026-08-15-resolve-with-validation
    PRIVATE psp_span_lib::psp_span_lib
)

# Require C++23 for std::expected, std::format, std::span.
set_target_properties(P-2026-08-15-resolve-with-validation PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-15-resolve-with-validation PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-15-resolve-with-validation PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()