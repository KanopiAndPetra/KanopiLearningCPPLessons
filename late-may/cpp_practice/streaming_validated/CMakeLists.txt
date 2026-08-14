# CMakeLists.txt — consumer of psp_span_lib v0.15.0 that adds today's
# P-2026-08-14 lesson:
#
#   psp::json_schema::parse_and_apply_atomic_streaming_validated(
#       JsonValue& root,
#       Span<const char>& doc,
#       const JsonValue& schema)
#       -> std::expected<std::size_t, SchemaValidatedStreamingPatchError>
#
# Where this fits in the arc
# --------------------------
#   - Jul 21: psp::json_pointer::split / resolve /  Pointer (RFC 6901)
#             resolve_mut
#   - Jul 22: psp::json_patch::patch +             Patch engine (RFC 6902)
#             parse_patch_document
#   - Aug  3: psp::json_patch::patch_atomic +     DEEP-CLONE transactional
#             patch_dry_run                        wrapper (in-memory)
#   - Aug  9: psp::json_patch::patch_journaled_   JOURNAL-AWARE
#             self_move_safe                       SELF-MOVE SAFE
#                                                   wrapper (in-memory)
#   - Aug 10: parse_and_apply_atomic_streaming    INVERSE-JOURNAL
#                                                   STREAMING wrapper
#   - Aug 11: parse_and_apply_atomic_streaming_   DEEP-CLONE STREAMING
#             deep_clone                           wrapper
#   - Aug 12: psp::json_schema::validate          JSON SCHEMA VALIDATION
#                                                   (Draft 2020-12, focused
#                                                   subset)
#   - Aug 13: psp::json_schema::validate_atomic   SCHEMA-VALIDATED ATOMIC
#                                                   UPDATE (four-gate
#                                                   composition; closes
#                                                   the JSON Schema arc)
#   - Aug 14: parse_and_apply_atomic_streaming_   STREAMING + SCHEMA-
#     TODAY  validated                              VALIDATED wire-format
#                                                   atomic update (closes
#                                                   the entire RFC 6902 +
#                                                   RFC 6901 + Draft
#                                                   2020-12 wire-format
#                                                   arc end-to-end; the
#                                                   first forward-on item
#                                                   the Aug 13 lesson's
#                                                   "Where we go next"
#                                                   section explicitly
#                                                   named; composes Aug
#                                                   11 (streaming
#                                                   deep-clone) with Aug
#                                                   13 (validate_atomic)
#                                                   into a single one-shot
#                                                   call: pre-validate,
#                                                   stream + apply each
#                                                   op with deep-clone
#                                                   rollback on failure,
#                                                   post-validate with
#                                                   rollback to the
#                                                   up-front-captured
#                                                   pre_state on failure;
#                                                   library version
#                                                   unchanged at v0.15.0;
#                                                   future v0.16.0
#                                                   promotion is
#                                                   mechanical — lift
#                                                   SchemaValidatedStream
#                                                   ingPatchError +
#                                                   ValidatedStreamingGate
#                                                   + parse_and_apply_
#                                                   atomic_streaming_
#                                                   validated + the
#                                                   gate_name + the
#                                                   schema + streaming
#                                                   helpers into
#                                                   <psp_span/json_schema.h>
#                                                   and <psp_span/json_ext.h>;
#                                                   bump the version; no
#                                                   new error enumerators;
#                                                   reuses Aug 13
#                                                   SchemaErrorContext +
#                                                   JsonSchemaError + Aug
#                                                   11 deep_clone +
#                                                   parse_and_apply_
#                                                   atomic_streaming_
#                                                   deep_clone rollback
#                                                   pattern; the wire-
#                                                   format arc is now
#                                                   complete end-to-end)
#
# Build (assumes psp_span_lib v0.15.0 is installed at /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-08-14-streaming-validated
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install \
#       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-14-streaming-validated
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-14-streaming-validated

cmake_minimum_required(VERSION 3.16)
project(streaming_validated_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.15.0 that adds today's psp::json_schema::parse_and_apply_atomic_streaming_validated composition as the P-2026-08-14 lesson. Closes the first forward-on item on the Aug 13 'Where we go next' list ('parse_and_apply_atomic_streaming_validated — composes Aug 10/11 streaming wrappers with today's validate_atomic for a wire-format patch that validates end-to-end') by wiring Aug 11's deep-clone streaming wrapper + Aug 13's validate() + validate_atomic into a single one-shot wire-format call."
    LANGUAGES CXX
)

find_package(psp_span_lib 0.15 REQUIRED)

add_executable(P-2026-08-14-streaming-validated
    P-2026-08-14-streaming-validated.cpp
)

target_link_libraries(P-2026-08-14-streaming-validated
    PRIVATE psp_span_lib::psp_span_lib
)

# Require C++23 for std::expected, std::format, std::span.
set_target_properties(P-2026-08-14-streaming-validated PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-14-streaming-validated PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-14-streaming-validated PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
