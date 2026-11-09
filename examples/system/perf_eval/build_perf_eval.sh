#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
#
# Reproducible build/flash wrapper for the perf_eval app.
#
# The build itself is delegated to .gitlab/tools/build_apps.py. Its pre-build hook is
# what runs gen-bmgr-config, and a tree configured without that hook ends up with a
# board-manager component whose include dirs miss the board device headers
# (dev_display_lcd.h, dev_audio_codec.h), so it cannot link. Keeping a second build
# path here is how the two drifted apart before; this wrapper only picks the
# configuration and guards flashing.
#
# The board is selected through sdkconfig.ci.board.<alias>, which also fixes the build
# directory: build_apps names it <idf-version>/build_<target>_board.<alias>. flash and
# monitor reuse exactly that directory, so they can never pick up a build left behind
# by another board or by a hand-run idf.py.
set -euo pipefail

IDF_PATH="${IDF_PATH:-$HOME/sdk/test/esp-idf}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$HERE/../../.." && pwd)"

# The P4X board is the baseline board (spec v1.8 section 1.1), and its USB-Serial-JTAG
# console lets the [BENCH] records share the download port. The P4 EV board still
# builds with BOARD=espressif/esp32_p4_function_ev.
BOARD="${BOARD:-espressif/esp32_p4x_function_ev}"
TARGET="${TARGET:-esp32p4}"

# sdkconfig.ci.board.* is keyed by the board alias without its vendor prefix. The
# explicit =NAME is what makes build_apps put the output in build_<target>_board.<alias>
# instead of a shared build_<target> directory.
BOARD_ALIAS="${BOARD##*/}"
CONFIG_FILE="sdkconfig.ci.board.$BOARD_ALIAS"
CONFIG_SPEC="$CONFIG_FILE=board.$BOARD_ALIAS"

# No argument means "build": that is the documented entry point, and reading $1
# directly made the bare invocation die on `unbound variable`.
action="${1:-build}"
if [ "$#" -gt 0 ]; then
    shift
fi

# Flashing this image reformats the flash region that examples/system/super uses for
# its LittleFS volume, so a plain `flash` would silently erase that data.
# The wildcard branch forwards "$action" plus every extra argument to idf.py, so
# checking only $action let `size erase-flash` bypass the gate. Inspect the whole
# command line, while leaving genuinely read-only actions alone.
needs_ack=0
for arg in "$action" "$@"; do
    case "$arg" in
        read-flash|partition-table|size|size-components|size-files|monitor) ;;
        *flash*|*erase*|*partition*) needs_ack=1 ;;
    esac
done
if [ "$needs_ack" = "1" ] && [ "${PERF_EVAL_ACK_ERASE:-0}" != "1" ]; then
    echo "refusing to flash: this image erases the shared LittleFS region." >&2
    echo "on a board dedicated to benchmarking, re-run with PERF_EVAL_ACK_ERASE=1." >&2
    exit 1
fi

# shellcheck disable=SC1091
. "$IDF_PATH/export.sh" >/dev/null 2>&1

# build_apps derives both the IDF version and the build directory name from these two
# inputs, so mirror them instead of guessing at whichever directory looks newest.
IDF_VER="$(awk -F'[ )]+' '
    /^set\(IDF_VERSION_MAJOR/ { major = $2 }
    /^set\(IDF_VERSION_MINOR/ { minor = $2 }
    END { print major "." minor }
' "$IDF_PATH/tools/cmake/version.cmake")"
BUILD_DIR="$HERE/$IDF_VER/build_${TARGET}_board.${BOARD_ALIAS}"

# An empty capture is otherwise indistinguishable from a quiet boot: if the console is
# on UART0 the USB-Serial-JTAG node only carries the bootloader output.
describe_console() {
    if [ ! -f "$BUILD_DIR/sdkconfig" ]; then
        return 0
    fi
    if grep -q '^CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y' "$BUILD_DIR/sdkconfig"; then
        echo "perf_eval: console is USB-Serial-JTAG; capture on the /dev/ttyACM* node"
    else
        echo "perf_eval: console is UART0; capture on the /dev/ttyUSB* node"
    fi
}

case "$action" in
    build|all)
        if [ ! -f "$HERE/$CONFIG_FILE" ]; then
            echo "perf_eval: no $CONFIG_FILE for board '$BOARD'" >&2
            exit 1
        fi
        echo "perf_eval: building BOARD=$BOARD target=$TARGET ($CONFIG_FILE)"
        cd "$REPO_ROOT"
        python .gitlab/tools/build_apps.py examples/system/perf_eval \
            -t "$TARGET" --config "$CONFIG_SPEC" -vv "$@"
        echo "perf_eval: build dir: $BUILD_DIR"
        describe_console
        ;;
    flash|app-flash|flash-only)
        # esptool is called directly instead of through `idf.py flash`: idf.py re-runs
        # cmake in the project directory, and this app derives the sdkconfig_hash
        # stamped into the firmware from that directory's sdkconfig. A reconfigure
        # there rewrites the file and silently changes the recorded hash, which then
        # no longer matches the baseline.
        if [ ! -f "$BUILD_DIR/flash_args" ]; then
            echo "perf_eval: $BUILD_DIR has no flash_args; run ./build_perf_eval.sh first." >&2
            exit 1
        fi
        # A directory left half-reconfigured (flash_args from one layout, sdkconfig
        # from another) would flash a partition table that does not match the image.
        args_size="$(sed -n '1s/.*--flash-size \([^ ]*\).*/\1/p' "$BUILD_DIR/flash_args")"
        cfg_size="$(sed -n 's/^CONFIG_ESPTOOLPY_FLASHSIZE="\(.*\)"/\1/p' "$BUILD_DIR/sdkconfig")"
        if [ -n "$args_size" ] && [ -n "$cfg_size" ] && [ "$args_size" != "$cfg_size" ]; then
            echo "perf_eval: $BUILD_DIR is inconsistent: flash_args=$args_size, sdkconfig=$cfg_size." >&2
            echo "perf_eval: re-run ./build_perf_eval.sh before flashing." >&2
            exit 1
        fi
        # --port and --baud are global options: esptool parses them before the
        # subcommand, and after it they are mistaken for image pairs.
        esptool_args=(--chip "$TARGET")
        if [ -n "${PERF_EVAL_PORT:-}" ]; then
            esptool_args+=(-p "$PERF_EVAL_PORT")
        fi
        if [ -n "${PERF_EVAL_BAUD:-}" ]; then
            esptool_args+=(-b "$PERF_EVAL_BAUD")
        fi
        esptool_args+=(write-flash)
        if [ "${PERF_EVAL_FLASH_FORCE:-0}" = "1" ]; then
            esptool_args+=(--force)
        fi
        echo "perf_eval: flashing $BUILD_DIR/flash_args"
        (cd "$BUILD_DIR" && python -m esptool "${esptool_args[@]}" @flash_args)

        # `flash monitor` is the documented invocation; the leftover argument is
        # consumed here because esptool, unlike idf.py, does not start a monitor.
        for extra in "$@"; do
            if [ "$extra" = "monitor" ]; then
                cd "$HERE"
                if [ -n "${PERF_EVAL_PORT:-}" ]; then
                    exec idf.py -B "$BUILD_DIR" -p "$PERF_EVAL_PORT" monitor
                fi
                exec idf.py -B "$BUILD_DIR" monitor
            fi
        done
        ;;
    *)
        if [ ! -d "$BUILD_DIR" ]; then
            echo "perf_eval: $BUILD_DIR does not exist; run ./build_perf_eval.sh first." >&2
            exit 1
        fi
        echo "perf_eval: reusing build dir: $BUILD_DIR"
        cd "$HERE"
        idf.py -B "$BUILD_DIR" "$action" "$@"
        ;;
esac
