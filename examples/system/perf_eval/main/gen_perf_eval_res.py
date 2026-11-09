# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""Regenerate perf_eval_res.c from res/.

perf_eval_res.c is checked in, so the manifest it carries has to be
reproducible rather than hand-maintained: renaming or adding a corpus file
without regenerating leaves perf_eval_res.c pointing at _binary_* symbols that no
longer exist, and the failure shows up as a link error rather than as a stale
report.

Two things are measured here rather than restated by hand:

* doc_complexity, which v1.8 section 3.1 reports as
  {"nodes", "styles", "images", "bytes"}.
* the screen list. Screen ids come from the flow documents, not from file stems:
  apps/brookesia.general.settings/res/screens/header.json declares the screen id
  "settings_header", so mounting "/header" fails while the other ten succeed.

Usage: python3 main/gen_perf_eval_res.py
"""

import json
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RES_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "res"))
OUT_C = os.path.join(SCRIPT_DIR, "perf_eval_res.c")

IMAGE_EXT = (".jpg", ".jpeg", ".png", ".gif")


def staged_symbol(rel_path):
    """Flat staged filename, mirroring the rule in main/CMakeLists.txt.

    ESP-IDF derives target_add_binary_data symbols from the basename alone, so
    main/CMakeLists.txt stages every file under a name derived from its full
    path. The two rules must stay identical or the link breaks.
    """
    return rel_path.replace("/", "_").replace(".", "_").replace("-", "_")


def collect_files():
    files = []
    for dirpath, _, filenames in os.walk(RES_DIR):
        for filename in filenames:
            full = os.path.join(dirpath, filename)
            rel = os.path.relpath(full, RES_DIR).replace(os.sep, "/")
            files.append(rel)
    return sorted(files)


def load_json(rel_path):
    with open(os.path.join(RES_DIR, rel_path), encoding="utf-8") as handle:
        return json.load(handle)


def measure_complexity(files):
    """Count what section 3.1 asks for, over the whole embedded tree."""
    nodes = 0
    styles = 0

    def walk(obj):
        nonlocal nodes, styles
        if isinstance(obj, dict):
            if "type" in obj:
                nodes += 1
            if "style" in obj:
                styles += 1
            for value in obj.values():
                walk(value)
        elif isinstance(obj, list):
            for value in obj:
                walk(value)

    for rel in files:
        if rel.endswith(".json"):
            walk(load_json(rel))

    images = sum(1 for rel in files if rel.lower().endswith(IMAGE_EXT))
    total_bytes = sum(os.path.getsize(os.path.join(RES_DIR, rel)) for rel in files)
    return nodes, styles, images, total_bytes


def collect_screens(files):
    """Screen ids as declared by the flow documents.

    Returns (dir, screen_id) pairs where dir is the document set the flow lives
    in, which is also the directory relative asset paths resolve against.
    """
    screens = set()
    for rel in files:
        if not rel.endswith(".json"):
            continue
        if "/flows/" in rel:
            flow = load_json(rel)
            directory = rel[: rel.index("/flows/")]
            for screen_id in flow.get("screens", []):
                screens.add((directory, screen_id))
        elif "/screens/" in rel:
            # Screens that no flow declares (the startup document) exist only as files,
            # so the id has to come from the document itself.
            screen = load_json(rel)
            screen_id = screen.get("id")
            if screen_id:
                directory = rel[: rel.index("/screens/")]
                screens.add((directory, screen_id))
    return sorted(screens)


def main():
    if not os.path.isdir(RES_DIR):
        sys.exit("res/ not found at {}".format(RES_DIR))

    files = collect_files()
    nodes, styles, images, total_bytes = measure_complexity(files)
    screens = collect_screens(files)
    documents = sorted({rel[: rel.index("/root.json")] for rel in files
                        if rel.endswith("/root.json")})
    if len(documents) != 5:
        sys.exit("expected 5 documents, found {}: {}".format(len(documents), documents))
    if len(screens) != 24:
        sys.exit("expected 24 screens, found {}: {}".format(len(screens), screens))

    out = [
        "/*",
        " * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD",
        " *",
        " * SPDX-License-Identifier: CC0-1.0",
        " */",
        "// Generated from res/ by main/gen_perf_eval_res.py - do not edit by hand.",
        "//",
        "// Paths are mapped onto flat staged filenames because ESP-IDF derives",
        "// target_add_binary_data symbols from the basename alone, which collides for",
        "// flows/header.json and screens/header.json. See main/CMakeLists.txt.",
        "//",
        "// doc_complexity and the screen list below are measured from res/ at generation",
        "// time rather than restated by hand, so neither can drift from the corpus that",
        "// is actually on the device.",
        "",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        '#include "perf_eval_res.hpp"',
        "",
    ]
    for rel in files:
        symbol = staged_symbol(rel)
        out.append("extern const uint8_t _binary_{}_start[];".format(symbol))
        out.append("extern const uint8_t _binary_{}_end[];".format(symbol))
    out += [
        "",
        "// Measured from res/: nodes are objects carrying a \"type\", styles are objects",
        "// carrying a \"style\", bytes is the whole embedded tree (JSON + images + fonts).",
        "const uint32_t perf_eval_res_doc_nodes = {};".format(nodes),
        "const uint32_t perf_eval_res_doc_styles = {};".format(styles),
        "const uint32_t perf_eval_res_doc_images = {};".format(images),
        "const uint32_t perf_eval_res_doc_bytes = {}u;".format(total_bytes),
        "",
        "// Screen ids as declared by the flow documents. A screen id is not a file stem:",
        "// settings ships screens/header.json for the screen id \"settings_header\".",
        "const PerfEvalResScreen perf_eval_res_screens[] = {",
    ]
    for directory, screen_id in screens:
        out.append('    {{"{}", "{}"}},'.format(directory, screen_id))
    out += [
        "};",
        "",
        "const size_t perf_eval_res_screen_count =",
        "    sizeof(perf_eval_res_screens) / sizeof(perf_eval_res_screens[0]);",
        "",
        "const PerfEvalResEntry perf_eval_res_manifest[] = {",
    ]
    for rel in files:
        symbol = staged_symbol(rel)
        out.append('    {{"{}", _binary_{}_start, _binary_{}_end}},'.format(rel, symbol, symbol))
    out += [
        "};",
        "",
        "const size_t perf_eval_res_manifest_count =",
        "    sizeof(perf_eval_res_manifest) / sizeof(perf_eval_res_manifest[0]);",
        "",
        "size_t perf_eval_res_size(const PerfEvalResEntry *entry)",
        "{",
        "    if (entry == NULL) {",
        "        return 0;",
        "    }",
        "    return (size_t)(entry->end - entry->data);",
        "}",
        "",
        "const PerfEvalResEntry *perf_eval_res_find(const char *path)",
        "{",
        "    if (path == NULL) {",
        "        return NULL;",
        "    }",
        "    for (size_t i = 0; i < perf_eval_res_manifest_count; ++i) {",
        "        const char *a = perf_eval_res_manifest[i].path;",
        "        size_t j = 0;",
        "        while (a[j] != '\\0' && a[j] == path[j]) {",
        "            ++j;",
        "        }",
        "        if (a[j] == '\\0' && path[j] == '\\0') {",
        "            return &perf_eval_res_manifest[i];",
        "        }",
        "    }",
        "    return NULL;",
        "}",
        "",
    ]

    with open(OUT_C, "w", encoding="utf-8") as handle:
        handle.write("\n".join(out))

    print(
        "{}: {} files, {} symbols, {} screens; "
        "doc_complexity nodes={} styles={} images={} bytes={}".format(
            os.path.relpath(OUT_C, os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))),
            len(files),
            len(files) * 2,
            len(screens),
            nodes,
            styles,
            images,
            total_bytes,
        )
    )


if __name__ == "__main__":
    main()
