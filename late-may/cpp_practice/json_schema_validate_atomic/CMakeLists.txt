# CMakeLists.txt — consumer of psp_span_lib v0.15.0 that adds the
# schema-validated atomic update pattern as today's P-2026-08-13
# consumer-side lesson.
#
#   psp::json_schema::validate_atomic(
#       JsonValue& root,
#       std::span<const JsonPatchOp> ops,
#       const JsonValue& schema)
#       -> std::expected<void, SchemaValidatedPatchError>
#
# Where this fits in the arc
# --------------------------
#   - Jul 21: psp::json_pointer::split / resolve /  Pointer (RFC 6901)
#             resolve_mut
#   - Jul 22: psp::json_patch::patch +             Patch engine (RFC 6902)
#             parse_patch_document                 v0.13.0 wire-format
#                                                   parser
#   - Aug  3: psp::json_patch::patch_atomic +     DEEP-CLONE transactional
#             patch_dry_run                        wrapper (in-memory)
#   - Aug  5: psp::json_patch::patch_journaled    INVERSE-JOURNAL
#                                                   transactional wrapper
#                                                   (in-memory)
#   - Aug  6: psp::json_patch::patch_self_move_   SELF-MOVE FIX wrapper
#             safe                                 (in-memory)
#   - Aug  9: psp::json_patch::patch_journaled_   JOURNAL-AWARE
#             self_move_safe                       SELF-MOVE SAFE
#                                                   wrapper (in-memory)
#   - Aug 10: parse_and_apply_atomic_streaming    INVERSE-JOURNAL
#                                                   STREAMING wrapper
#   - Aug 11: parse_and_apply_atomic_streaming_   DEEP-CLONE STREAMING
#             deep_clone                           wrapper
#   - Aug 12: psp::json_schema::validate          JSON SCHEMA VALIDATION
#                                                   (Draft 2020-12, focused
#                                                   subset; 22 enumerators;
#                                                   schema_path +
#                                                   instance_path RFC 6901)
#   - Aug 13: psp::json_schema::validate_atomic   SCHEMA-VALIDATED ATOMIC
#     TODAY  (four-gate composition)               UPDATE (closes the JSON
#                                                   Schema item on the
#                                                   Aug 12 "Where we go next"
#                                                   forward-on list; the
#                                                   Aug 12 "The natural
#                                                   next step is the
#                                                   validate_atomic
#                                                   composition" + Section 8
#                                                   four-gate pattern, wired
#                                                   end-to-end as a single
#                                                   one-shot call; library
#                                                   version unchanged at
#                                                   v0.15.0; future v0.16.0
#                                                   promotion is mechanical;
#                                                   SchemaValidatedPatchError
#                                                   enum Gate {PreValidate,
#                                                   DryRun, AtomicApply,
#                                                   PostValidate} + Kind
#                                                   {Schema, Engine} + an
#                                                   optional<SchemaErrorContext>
#                                                   + an optional<JsonPatchError>;
#                                                   same observable contract
#                                                   as patch_atomic on the
#                                                   success path + the
#                                                   engine-failure rollback
#                                                   path; additionally
#                                                   guarantees that the
#                                                   post-state schema check
#                                                   rolls back to the
#                                                   captured pre-state, an
#                                                   invariant patch_atomic
#                                                   alone cannot provide
#                                                   because the engine
#                                                   itself doesn't know
#                                                   about schemas;
#                                                   18 cases across 9 new
#                                                   sections (11-19) added
#                                                   on top of the Aug 12
#                                                   baseline 137 cases)
#
# The composition validates against the schema at gate 1 (pre-state),
# dry-runs the patch on a private clone at gate 2, applies atomically
# with deep-snapshot rollback at gate 3, and re-validates at gate 4
# (post-state, with rollback to the up-front-captured pre_state on
# failure). The four-gate pattern gives callers a single function
# call that combines pointer + patch + schema validation into one
# transaction.
#
# Strict-warning + ASan/UBSan coverage
# ------------------------------------
# Three builds; the third is the ASan+UBSan build for memory and UB
# correctness.
#
# Build (assumes psp_span_lib v0.15.0 is installed at /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-08-13-json-schema-validate-atomic
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install \
#       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-13-json-schema-validate-atomic
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-13-json-schema-validate-atomic

cmake_minimum_required(VERSION 3.16)
project(json_schema_validate_atomic_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.15.0 that adds today's psp::json_schema::validate_atomic four-gate schema-validated atomic update composition as the P-2026-08-13 lesson. Closes the Aug 12 (P-2026-08-12) 'natural next step is the validate_atomic composition' item by wiring validate() + patch_dry_run() + patch_atomic() + validate() into a single one-shot call with named gate failures."
    LANGUAGES CXX
)

find_package(psp_span_lib 0.15 REQUIRED)

add_executable(P-2026-08-13-json-schema-validate-atomic
    P-2026-08-13-json-schema-validate-atomic.cpp
)

target_link_libraries(P-2026-08-13-json-schema-validate-atomic
    PRIVATE psp_span_lib::psp_span_lib
)

# Require C++23 for std::expected, std::format, etc.
set_target_properties(P-2026-08-13-json-schema-validate-atomic PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-13-json-schema-validate-atomic PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-13-json-schema-validate-atomic PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
