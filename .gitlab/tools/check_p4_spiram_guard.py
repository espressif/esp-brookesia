#!/usr/bin/env python
#
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
#
# P4 negative guardrail checks (实现文档 §七 automatable subset).
#
# Two rules, matching the P4 checklist items that are mechanically checkable:
#   1. sdkconfig files must keep CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0 or omit
#      it entirely (item: "未修改 ALWAYSINTERNAL 语义"). A non-zero value would
#      push small allocations into PSRAM and silently change hot-path latency.
#   2. GUI document-model sources must not allocate through SPIRAM explicitly
#      (item: "未把 JSON/Style 硬搬 PSRAM"). Only allocation-style calls are
#      rejected; queries such as heap_caps_get_free_size(MALLOC_CAP_SPIRAM)
#      are allowed.
#
# Usage (pre-commit passes the staged filenames after --):
#   ./.gitlab/tools/check_p4_spiram_guard.py -- sdkconfig.defaults src/foo.cpp
#
# Return code: 0 = clean, 1 = violation found.

import re
import sys
from pathlib import Path

ALWAYSINTERNAL_RE = re.compile(
    r'^CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=(\S+)\s*$', re.MULTILINE)
# Allocation-style calls only (malloc/realloc/calloc with the SPIRAM cap
# anywhere in the argument list of the same call).
SPIRAM_ALLOC_RE = re.compile(
    r'heap_caps_(?:malloc|realloc|calloc)\s*\([^;{]*MALLOC_CAP_SPIRAM', re.DOTALL)

SDKCONFIG_NAME_RE = re.compile(r'(^|/)sdkconfig[^/]*$')
GUI_INTERFACE_SRC_RE = re.compile(
    r'^gui/brookesia_gui_interface/(?:src|include|examples|test_apps)/.+\.(?:c|cc|cpp|cxx|h|hh|hpp)$')


def is_sdkconfig(path):
    return bool(SDKCONFIG_NAME_RE.search(path))


def is_gui_interface_src(path):
    return bool(GUI_INTERFACE_SRC_RE.match(path))


def check_sdkconfig(path, text):
    """Rule 1: ALWAYSINTERNAL must stay 0 or absent."""
    violations = []
    for match in ALWAYSINTERNAL_RE.finditer(text):
        value = match.group(1)
        if value != '0':
            line = text.count('\n', 0, match.start()) + 1
            violations.append(
                '%s:%d: CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=%s (must be 0 or '
                'absent; see P4 checklist / 实现文档 §七)' % (path, line, value))
    return violations


def check_gui_interface_src(path, text):
    """Rule 2: no explicit SPIRAM allocation in the document model."""
    violations = []
    for match in SPIRAM_ALLOC_RE.finditer(text):
        line = text.count('\n', 0, match.start()) + 1
        violations.append(
            '%s:%d: explicit MALLOC_CAP_SPIRAM allocation in '
            'gui/brookesia_gui_interface (P4: JSON/Style must not be moved to '
            'PSRAM; see 实现文档 §七)' % (path, line))
    return violations


def main(argv):
    if '--' in argv:
        files = argv[argv.index('--') + 1:]
    else:
        files = [a for a in argv if not a.startswith('-')]

    violations = []
    for name in files:
        path = Path(name)
        if not path.is_file():
            continue
        try:
            text = path.read_text(encoding='utf-8', errors='replace')
        except OSError as exc:
            print('check_p4_spiram_guard: cannot read %s: %s' % (name, exc))
            return 1
        if is_sdkconfig(name.replace('\\', '/')):
            violations.extend(check_sdkconfig(name, text))
        if is_gui_interface_src(name.replace('\\', '/')):
            violations.extend(check_gui_interface_src(name, text))

    for violation in violations:
        print(violation)
    if violations:
        print('check_p4_spiram_guard: %d violation(s) found' % len(violations))
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
