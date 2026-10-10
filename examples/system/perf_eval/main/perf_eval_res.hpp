// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stddef.h>
#include <stdint.h>

struct PerfEvalResEntry {
    const char *path;
    const uint8_t *data;
    const uint8_t *end;
};

// perf_eval_res.c is compiled as C, where a bare PerfEvalResEntry is not a type
// name; only C++ injects the struct keyword. Without this the .c file fails with
// "unknown type name 'PerfEvalResEntry'" even though the struct is defined
// directly above.
typedef struct PerfEvalResEntry PerfEvalResEntry;

// A screen as declared by a flow document. dir is the document set the flow lives
// in, which is also the directory that flow's relative asset paths resolve against.
struct PerfEvalResScreen {
    const char *dir;
    const char *screen_id;
};
typedef struct PerfEvalResScreen PerfEvalResScreen;

#ifdef __cplusplus
extern "C" {
#endif

extern const PerfEvalResEntry perf_eval_res_manifest[];
extern const size_t perf_eval_res_manifest_count;

// Measured from res/ when perf_eval_res.c was generated, per v1.8 section 3.1.
// nodes: objects carrying a "type"; styles: objects carrying a "style";
// images: image assets; bytes: whole embedded tree (JSON + images + fonts).
extern const uint32_t perf_eval_res_doc_nodes;
extern const uint32_t perf_eval_res_doc_styles;
extern const uint32_t perf_eval_res_doc_images;
extern const uint32_t perf_eval_res_doc_bytes;
extern const PerfEvalResScreen perf_eval_res_screens[];
extern const size_t perf_eval_res_screen_count;

// Byte count for an entry. Kept as a call because (end - data) is not a
// constant expression in C and so cannot be a static initializer.
size_t perf_eval_res_size(const PerfEvalResEntry *entry);

// Returns NULL when the path is not part of the embedded tree.
const PerfEvalResEntry *perf_eval_res_find(const char *path);

#ifdef __cplusplus
}
#endif
