#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Insert explicit English .. _anchor: labels before RST section titles."""

from __future__ import annotations

import os
import re
import sys

UNDERLINE_CHARS = frozenset('=-~^"+#*._:`')

# Explicit hyperlink target: .. _label:
ANCHOR_DEF_RE = re.compile(r"^\.\.\s+_([^:]+):\s*$")


def is_underline(line: str) -> bool:
    s = line.rstrip("\n")
    if len(s) < 3:
        return False
    ch = s[0]
    if ch not in UNDERLINE_CHARS:
        return False
    return len(set(s)) == 1


def file_base(rel: str) -> str:
    """service/usage.rst -> service-usage"""
    p = rel.replace("\\", "/")
    if p.endswith(".rst"):
        p = p[:-4]
    return p.replace("/", "-")


def should_skip_title(line: str) -> bool:
    t = line.strip()
    if not t:
        return True
    if t.startswith(".."):
        return True
    if t.startswith("|"):
        return True
    return False


def prev_is_explicit_anchor(lines: list[str], idx: int) -> bool:
    """True if a non-empty line before ``idx`` is already ``.. _label:`` (skips blank lines)."""
    j = idx - 1
    while j >= 0 and lines[j].strip() == "":
        j -= 1
    if j < 0:
        return False
    return bool(ANCHOR_DEF_RE.match(lines[j].strip()))


def collect_explicit_anchor_labels(lines: list[str]) -> set[str]:
    """Collect all explicit hyperlink target names from ``.. _label:`` lines in a file."""
    out: set[str] = set()
    for line in lines:
        m = ANCHOR_DEF_RE.match(line.strip())
        if m:
            out.add(m.group(1))
    return out


def collect_sections(lines: list[str]) -> list[int]:
    """Line indices (0-based) of section title lines."""
    out: list[int] = []
    i = 0
    while i < len(lines) - 1:
        line = lines[i].rstrip("\n")
        u = lines[i + 1].rstrip("\n") if i + 1 < len(lines) else ""
        if (
            is_underline(u)
            and len(u) >= len(line)
            and not should_skip_title(line)
        ):
            out.append(i)
            i += 2
            continue
        i += 1
    return out


def default_anchor(base: str, sec_index: int) -> str:
    return f"{base}-sec-{sec_index:02d}"


# Semantic anchors for getting_started.rst (same index in zh_CN and en)
GETTING_STARTED_SEMANTIC = {
    0: "getting-started-guide",
    1: "getting-started-versioning",
    2: "getting-started-dev-environment",
    3: "getting-started-hardware",
    4: "getting-started-component-usage",
    5: "getting-started-example-projects",
}


def anchor_for(
    rel: str, base: str, sec_index: int, lang: str
) -> str:
    if rel == "getting_started.rst" and sec_index in GETTING_STARTED_SEMANTIC:
        return GETTING_STARTED_SEMANTIC[sec_index]
    return default_anchor(base, sec_index)


def unique_label(desired: str, used: set[str]) -> str:
    """Return a label not in ``used``; keep ``desired`` if free, else ``{desired}-dupN``."""
    if desired not in used:
        return desired
    n = 2
    while True:
        cand = f"{desired}-dup{n}"
        if cand not in used:
            return cand
        n += 1


def discover_rst_paths(root: str) -> list[str]:
    paths: list[str] = []
    for dirpath, _, files in os.walk(root):
        for fn in sorted(files):
            if not fn.endswith(".rst"):
                continue
            paths.append(os.path.join(dirpath, fn))
    paths.sort()
    return paths


def process_file(path: str, rel: str, lang: str, dry_run: bool, used: set[str]) -> bool:
    with open(path, encoding="utf-8") as f:
        lines = f.readlines()

    base = file_base(rel)
    title_lines = collect_sections(lines)
    changed = False
    # Insert from bottom to top so line indices stay valid
    for k, title_i in sorted(enumerate(title_lines), key=lambda x: -x[1]):
        if prev_is_explicit_anchor(lines, title_i):
            continue
        desired = anchor_for(rel, base, k, lang)
        label = unique_label(desired, used)
        used.add(label)
        insert = f".. _{label}:\n\n"
        lines.insert(title_i, insert)
        changed = True

    if changed and not dry_run:
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.writelines(lines)
    return changed


def main() -> int:
    dry_run = "--dry-run" in sys.argv
    docs = os.path.dirname(os.path.abspath(__file__))
    for lang in ("zh_CN", "en"):
        root = os.path.join(docs, lang)
        if not os.path.isdir(root):
            continue
        paths = discover_rst_paths(root)
        # One pass: all existing anchors in this language tree (unified doc for that build)
        used: set[str] = set()
        for path in paths:
            with open(path, encoding="utf-8") as f:
                used |= collect_explicit_anchor_labels(f.readlines())
        for path in paths:
            rel = os.path.relpath(path, root)
            process_file(path, rel, lang, dry_run, used)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
