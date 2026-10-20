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

# Flashing this image reformats the flash region that examples/system/super uses
# for its LittleFS volume, so a plain `flash` would silently erase that data.
for arg in "$@"; do
    case "$arg" in
        flash|app-flash|flash-only)
            if [ "${PERF_EVAL_ACK_ERASE:-0}" != "1" ]; then
                echo "refusing to flash: this image erases the shared LittleFS region." >&2
                echo "on a board dedicated to benchmarking, re-run with PERF_EVAL_ACK_ERASE=1." >&2
                exit 1
            fi
            ;;
    esac
done

cd "$HERE"

# The board-manager code generator is only wired into build_apps.py: gen-bmgr-config
# on its own emits a component whose include dirs miss the board device headers
# (dev_display_lcd.h, dev_audio_codec.h), so a tree that was never built by build_apps
# cannot link. Delegate the build to it and keep this wrapper for the flags/ack checks.
if [ "$1" = "build" ] || [ "$1" = "all" ]; then
    echo "perf_eval: building through build_apps.py (BOARD=$BOARD, target=$TARGET)"
    cd "$HERE/../../.."
    exec python .gitlab/tools/build_apps.py examples/system/perf_eval -t "$TARGET" -vv
fi

idf.py \
    -DBOARD="$BOARD" \
    -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.$TARGET;sdkconfig.ci.board.$BOARD_ALIAS;sdkconfig.defaults.board" \
    "$@"
