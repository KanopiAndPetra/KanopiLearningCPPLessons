# CMakeLists.txt — consumer of psp_span_lib v0.15.0 that designs
# a STREAMING variant of psp::json_patch::parse_patch_document:
#
#   psp::json_patch::parse_patch_document_at(Span<const char>& s)
#       -> std::expected<JsonPatchOp, JsonPatchError>
#
# It applies the cursor-primitive contract from the v0.7.0/v0.8.0
# streaming parsers (parse_int_at, parse_double_at, expect_char_at,
# skip_whitespace_at) to the RFC 6902 §3 wire-format parser:
# shrink `s` past the consumed op on success; leave `s` unchanged
# on failure.
#
# Where this fits in the arc
# --------------------------
# The Aug 3 lesson (P-2026-08-03-transactional-patch.md) closed
# the transactional engine arc — the consumer-side wrapper around
# psp::json_patch::patch that implements "all-or-nothing" rollback
# via deep-snapshot. Today's lesson is the next v0.15.0 candidate
# from the Aug 3 "v0.15.0 candidates" forward-on list:
#
#   - Streaming patch parser — the v0.13.0 parse_patch_document
#     reads a full string_view; a streaming variant over
#     Span<const char> would close the cursor-primitive gap in
#     the RFC 6902 layer.
#
# The cursor-primitive gap was first surfaced in the Jul 15 lesson
# (P-2026-07-15-psp-parser-streaming-cursor.cpp), which shipped
# parse_int_at / parse_uint_at / parse_double_at and proved the
# "shrink on success / unchanged on failure" contract on top of
# the numeric parsers. Today we apply that same contract to the
# RFC 6902 §3 wire-format parser.
#
# What's in this consumer
# -----------------------
#   Section 1 — symbol presence + the cursor contract on the
#               streaming parser.
#   Section 2 — happy path: walk a 3-op document one op at a time,
#               shrinking the span. After the third op the span
#               is empty (with optional trailing whitespace).
#   Section 3 — error path: an unparseable doc leaves the span
#               BYTE-IDENTICAL to pre-state; a partial-trailing
#               doc returns BadDocument at the op that failed
#               and leaves the span at the START of that op.
#   Section 4 — round-trip: parse a wire-format patch via the
#               streaming parser, apply via the engine, serialise
#               via the v0.15.0 writer. The round-trip is
#               deterministic.
#   Section 5 — generator-style usage: the streaming parser
#               composes naturally with the v0.7.0 cursor
#               primitives (expect_char_at + skip_whitespace_at)
#               to walk a multi-op document without ever
#               allocating the full std::vector<JsonPatchOp>.
#   Section 6 — interop with transactional patch: walk the
#               streaming parser op-by-op, apply each via the
#               existing patch (not patch_atomic — the
#               transactional layer would commit even on
#               partial-failure mid-stream, which is the wrong
#               semantics for streaming).
#   Section 7 — sizeof / feature probes.
#
# Design notes
# ------------
# The streaming variant mirrors the v0.13.0 driver's structure
# (parse_patch_document + detail::build_one_op): the OUTER
# function is the cursor walker (skip whitespace + expect '[' +
# walk commas + expect ']'), the INNER function is the per-op
# builder (called once per op, returns one JsonPatchOp or an
# error). The cursor contract — shrink on success / unchanged
# on failure — falls out of using psp::skip_whitespace_at and
# psp::expect_char_at as the outer steps (both honour the
# contract today), and of the inner per-op builder leaving
# `s` unchanged on its typed error path (via the same copy-
# before-shrink trick the v0.13.0 driver uses).
#
# Why consumer-side and not library-side today
# ---------------------------------------------
# Same shape as the Jul 24 writer / Jul 27 op_writer / Aug 3
# patch_atomic lessons: a proven-in-consumer capability that
# today exercises the design end-to-end. The library version
# is unchanged at v0.15.0. A future v0.16.0 promotion is
# mechanical (lift the two consumer functions into
# <psp_span/json_ext.h> alongside the v0.13.0 driver; bump the
# version).
#
# Usage (assumes psp_span_lib v0.15.0 is installed at
# /tmp/psp_install):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/psp_install
#   cmake --build build
#   ./build/P-2026-08-04-streaming-patch-parser
#
# Strict-warning build:
#
#   cmake -S . -B build-strict -DCMAKE_PREFIX_PATH=/tmp/psp_install \
#       -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion"
#   cmake --build build-strict
#   ./build-strict/P-2026-08-04-streaming-patch-parser
#
# ASan + UBSan build:
#
#   cmake -S . -B build-asan -DCMAKE_PREFIX_PATH=/tmp/psp_install -DENABLE_ASAN=ON
#   cmake --build build-asan
#   ./build-asan/P-2026-08-04-streaming-patch-parser

cmake_minimum_required(VERSION 3.16)
project(streaming_patch_parser_consumer
    VERSION 0.1.0
    DESCRIPTION "Consumer of psp_span_lib v0.15.0 that designs a STREAMING JSON Patch parser (psp::json_patch::parse_patch_document_at as a consumer-side function): the cursor-primitive variant of the v0.13.0 parse_patch_document. Takes psp::Span<const char>&, shrinks past each consumed op on success, leaves the span unchanged on failure. Library version is unchanged at v0.15.0; future v0.16.0 promotion is mechanical."
    LANGUAGES CXX
)

# Require >= 0.15: today uses the v0.15.0 RFC 6902 §3 wire-format
# writer (psp::json_patch::serialise_patch_document) for the
# round-trip section, the v0.13.0 v0.13.0 RFC 6902 §3 wire-format
# parser for the side-by-side comparison, and the v0.12.0
# engine + v0.11.0 pointer. The streaming parser itself only
# needs the v0.8.0 cursor primitives (skip_whitespace_at,
# expect_char_at) plus the v0.10.0 JSON tree, but pinning 0.15
# keeps the consumer consistent with the Aug 2/3 lesson arc.
find_package(psp_span_lib 0.15 REQUIRED)

add_executable(P-2026-08-04-streaming-patch-parser
    P-2026-08-04-streaming-patch-parser.cpp
)

target_link_libraries(P-2026-08-04-streaming-patch-parser
    PRIVATE psp_span_lib::psp_span_lib
)

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan for the consumer" OFF)
if(ENABLE_ASAN)
    target_compile_options(P-2026-08-04-streaming-patch-parser PRIVATE
        -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -O1
    )
    target_link_options(P-2026-08-04-streaming-patch-parser PRIVATE
        -fsanitize=address -fsanitize=undefined
    )
endif()