# CMakeLists.txt — consumer of psp_span_lib v0.15.0 that revives
# the ABANDONED Aug 7 JSON Schema validation (Draft 2020-12,
# focused subset) attempt as the P-2026-08-12 consumer-side
# lesson:
#
#   psp::json_schema::validate(JsonValue const& instance,
#                              JsonValue const& schema)
#       -> std::expected<void, JsonSchemaError>
#
# Where this fits in the arc
# --------------------------
#   - Jul 21: psp::json_pointer::split / resolve /  Pointer (RFC 6901)
#             resolve_mut
#   - Jul 22: psp::json_patch::patch +             Patch engine (RFC 6902)
#             parse_patch_document                 v0.13.0 wire-format
#                                                   parser
#   - Aug  1: ...                                 (intermediate lessons)
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
#             deep_clone                           wrapper (closes the
#                                                   streaming-atom arc)
#   - Aug 12: psp::json_schema::validate          JSON SCHEMA VALIDATION
#     TODAY  (Draft 2020-12, focused subset)       (consumer-side;
#                                                   closes the JSON
#                                                   Schema item on the
#                                                   Aug 11 'Where we go
#                                                   next' forward-on list;
#                                                   design lifted from the
#                                                   abandoned Aug 7 attempt
#                                                   at late-may/cpp_practice/
#                                                   json_schema_validation/;
#                                                   same code, same
#                                                   contract, same
#                                                   observable behaviour;
#                                                   today's lesson is the
#                                                   verification)
#
# Why consumer-side today
# -----------------------
# Same shape as the Aug 3 / Aug 5 / Aug 6 / Aug 9 / Aug 10 / Aug 11
# lessons: a proven-in-consumer capability that exercises the design
# end-to-end. Library version unchanged at v0.15.0. Future v0.16.0
# promotion is mechanical: lift json_schema namespace
# (SchemaKind, JsonSchemaError, validate, validate_with_meta,
# detail::check_type, detail::check_keyword, detail::validate_value,
# detail::validate_object, detail::validate_array,
# detail::validate_composition) into <psp_span/json_schema.h>;
# bump the version.
#
# What the consumer exercises
# ----------------------------
#   Section 1 — symbol-presence + compile-time feature probes
#               (JsonSchemaError has the right enumerators,
#               validate has the right signature, etc.).
#   Section 2 — type validator (true / "object" / "array" /
#               "string" / "number" / "integer" / "boolean" / "null").
#   Section 3 — primitive constraints (enum / const / minimum /
#               maximum / exclusiveMinimum / exclusiveMaximum /
#               minLength / maxLength).
#   Section 4 — object validator (required / properties /
#               additionalProperties:false / minProperties /
#               maxProperties).
#   Section 5 — array validator (items as a single schema /
#               minItems / maxItems / uniqueItems).
#   Section 6 — composition (allOf / anyOf / oneOf / not).
#   Section 7 — boolean schemas (true accepts, false rejects).
#   Section 8 — interop with patch_atomic + patch_self_move_safe
#               (schema-validated atomic updates; back-compat with
#               the Aug 3 + Aug 6 wrappers, mirrored in this TU).
#   Section 9 — validation surface walks a real RFC 6901/6902
#               schema+document (a small registry entry shape
#               that the transactional layer would gate).
#   Section 10 — error formatting via std::format (the
#               std::formatter<JsonSchemaError> specialisation).
#
#   137 cases across 10 sections, all expected to pass.
#
# Strict-warning + ASan/UBSan coverage
# ------------------------------------
# Three builds; the third is the ASan+UBSan build for memory
# and UB correctness.
#
# Build (assumes psp_span_lib v0.15.0 is installed at /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-08-12-json-schema-validation
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install \
#       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-12-json-schema-validation
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-12-json-schema-validation

cmake_minimum_required(VERSION 3.16)
project(json_schema_validation_v2_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.15.0 that revives the ABANDONED Aug 7 JSON Schema validation (Draft 2020-12, focused subset) attempt as the P-2026-08-12 consumer-side lesson. Adopts the abandoned source as today's proven-in-consumer capability, verifies it on default + strict-warning + ASan/UBSan builds, and closes the JSON Schema validation item on the Aug 11 (P-2026-08-11) 'Where we go next' forward-on list. Library version unchanged at v0.15.0; future v0.16.0 promotion is mechanical."
    LANGUAGES CXX
)

# Require >= 0.15: today uses the v0.10.0 JsonValue tree, the
# v0.11.0 pointer, the v0.12.0 patch engine, the v0.13.0 patch
# parser, and the v0.15.0 patch writer. The validate() function
# itself only needs the v0.10.0 tree, but pinning 0.15 keeps
# the consumer consistent with the Aug 3/5/6/9/10/11 lesson arc.
find_package(psp_span_lib 0.15 REQUIRED)

add_executable(P-2026-08-12-json-schema-validation
    P-2026-08-12-json-schema-validation.cpp
)

target_link_libraries(P-2026-08-12-json-schema-validation
    PRIVATE psp_span_lib::psp_span_lib
)

# Require C++23 for std::expected, std::format, etc.
set_target_properties(P-2026-08-12-json-schema-validation PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-12-json-schema-validation PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-12-json-schema-validation PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()
