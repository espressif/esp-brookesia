#!/usr/bin/env bash
#
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
#
# Run the repository gates locally. This is the source of truth for the gate
# commands; the GitLab CI jobs are expected to call this same script so there is
# no "only verified in CI" gap. No GitLab account or runner is needed.
#
#   ./.gitlab/tools/run_local_gates.sh            # all gates
#   GATES=host_mem ./.gitlab/tools/run_local_gates.sh
#
# GATES: comma-separated subset of host_mem,p4_guard,size (default: all).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

GATES="${GATES:-host_mem,p4_guard,size}"
want() { case ",${GATES}," in *",$1,"*) return 0 ;; *) return 1 ;; esac; }

pass() { printf '  \033[32mPASS\033[0m %s\n' "$1"; }
fail() { printf '  \033[31mFAIL\033[0m %s\n' "$1" >&2; }

# --- G2 host memory gate: build and run the gui_interface PC tests -----------
gate_host_mem() {
    echo "== [host_mem] G2 host memory gate (gui_interface PC tests)"
    local app="gui/brookesia_gui_interface/test_apps"
    cmake -S "$app" -B "$app/build_pc" -DBROOKESIA_GUI_INTERFACE_TEST_APPS_FORCE_PC=ON
    cmake --build "$app/build_pc" -j"$(nproc)"
    ctest --test-dir "$app/build_pc" --output-on-failure
    pass "host_mem"
}

# --- P4 SPIRAM guard: same rule the pre-commit hook enforces ----------------
gate_p4_guard() {
    echo "== [p4_guard] ALWAYSINTERNAL=0 and no explicit PSRAM alloc in gui_interface"
    local files
    files="$(git ls-files | grep -E '(^|/)sdkconfig[^/]*$|^gui/brookesia_gui_interface/(src|include|examples|test_apps)/.+\.(c|cc|cpp|cxx|h|hh|hpp)$' || true)"
    if [ -z "$files" ]; then
        pass "p4_guard (no files to check)"
        return
    fi
    # shellcheck disable=SC2086
    python ./.gitlab/tools/check_p4_spiram_guard.py -- $files
    pass "p4_guard"
}

# --- G4 size gate: measure a build, compare when a baseline exists ----------
gate_size() {
    echo "== [size] G4 size measurement"
    local build_dir
    build_dir="$(find examples/system/perf_eval/build examples/system/super/build* -maxdepth 1 -type d 2>/dev/null | head -1 || true)"
    if [ -z "${build_dir:-}" ]; then
        echo "  skip: no build directory found (build an app first)"
        return
    fi
    local args=(measure --build-dir "$build_dir" --repo-root . --board-id esp32_p4_function_ev --profile perf -o size_record.json)
    if [ -f "${build_dir}/size.json" ]; then
        args+=(--idf-size-json "${build_dir}/size.json")
    fi
    python ./.gitlab/tools/measure_size.py "${args[@]}"
    local base="reports/baseline/esp32_p4_function_ev/size_baseline.json"
    if [ -f "$base" ]; then
        python ./.gitlab/tools/measure_size.py compare "$base" size_record.json --first-round -o size_compare.json
    else
        echo "  record-only: $base absent (no size baseline yet)"
    fi
    pass "size"
}

echo "run_local_gates: $GATES"
want host_mem && gate_host_mem
want p4_guard && gate_p4_guard
want size && gate_size
echo "all requested gates passed"
