#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
#
# Reproducible build for the perf_eval app.
#
# The config chain deliberately starts from examples/system/super's two files.
# perf_eval needs the same LVGL/storage/GUI bring-up that super already proves
# out, and duplicating ~40 keys by hand is how the two drift. super's files are
# only read here; nothing in super is modified, and super keeps building from
# its own chain unchanged.
#
# sdkconfig.defaults.board is copied from the board definition
# (hal/brookesia_hal_boards/boards/espressif/<board>/) and carries what only the
# board knows: SELECTS_REV_LESS_V3, the 16 MB partition layout, and SPIRAM.
# Without it the build reaches a bootloader stamped for rev>=3.1, falls back to
# the 1 MB single-app partition table, and reports PSRAM as uninitialised.
#
# perf_eval's own two files come last, so they win.
set -euo pipefail

IDF_PATH="${IDF_PATH:-$HOME/sdk/test/esp-idf}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SUPER="$(cd "$HERE/../super" && pwd)"

BOARD="${BOARD:-espressif/esp32_p4_function_ev}"
TARGET="${TARGET:-esp32p4}"

# sdkconfig.ci.board.* is keyed by the board alias without its vendor prefix,
# while -DBOARD wants the qualified "vendor/alias" id.
BOARD_ALIAS="${BOARD##*/}"

# shellcheck disable=SC1091
. "$IDF_PATH/export.sh" >/dev/null 2>&1

cd "$HERE"
idf.py \
    -DBOARD="$BOARD" \
    -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.$TARGET;sdkconfig.ci.board.$BOARD_ALIAS;sdkconfig.defaults.board" \
    "$@"
