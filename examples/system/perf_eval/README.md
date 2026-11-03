# perf_eval — GUI performance and memory benchmark

Measures LVGL/GUI behaviour on a real board and emits `[BENCH]` records plus JSON
reports that the paired gate (`tools/brookesia_gui_perf`) compares against a
baseline.

## ⚠️ This is a destructive, benchmark-only firmware

The benchmark needs a filesystem it fully owns, so it reformats the dedicated
`perf_eval_data` LittleFS partition on every boot.

That partition occupies the **same flash region** that
`examples/system/super` uses for its own LittleFS image (`littlefs_data`): both
layouts place a 5000K data partition directly after the same 10500K `factory`
partition. Flashing or running this image on a board that previously ran
`examples/system/super` **erases the fonts, applications and user/persistent
files stored there**, and re-flashing the super app does not bring them back
unless you restore them from your own backup.

Consequences:

- Use a **board dedicated to benchmarking**. Do not use a board that holds data
  you care about.
- Back up the super volume before switching images if you need it later, and
  restore it by re-flashing the super image plus your saved files.
- Reformatting requires `CONFIG_PERF_EVAL_ALLOW_DESTRUCTIVE_FORMAT=y`. Every
  board config bundled with this example (`sdkconfig.ci.board.*`) sets it to
  `y`, and `build_perf_eval.sh` always merges that file into
  `SDKCONFIG_DEFAULTS`, so **any firmware produced by the documented build
  formats the partition on boot**. Building without a board config leaves it at
  `n`; such a firmware refuses to start the benchmark instead of erasing
  anything.
- The `PERF_EVAL_ACK_ERASE=1` check only guards the `flash` path of
  `build_perf_eval.sh`. It does not protect a firmware deployed with `idf.py`
  or `esptool` directly, so never deploy this image to a board you care about.
A layout where super and perf_eval coexist on one 16MB board is not possible
with the current sizes (10.5MB factory + 5MB super + 5MB benchmark, before the
system partitions); that would need a smaller factory or larger flash.

## Build and flash

`espressif/esp32_p4x_function_ev` is the baseline board; the P4 EV board still builds
with `BOARD=espressif/esp32_p4_function_ev`.

```bash
# build; no argument means "build"
./build_perf_eval.sh

# flash and monitor: the acknowledgement is mandatory because this erases the data region
PERF_EVAL_ACK_ERASE=1 ./build_perf_eval.sh flash monitor
```

The wrapper derives one build directory from `BOARD` and `TARGET`
(`<idf-version>/build_<target>_board.<board-alias>`) and flashes exactly that
directory's `flash_args`, so it cannot deploy an image left behind by another board.
`PERF_EVAL_PORT` and `PERF_EVAL_BAUD` are forwarded to esptool, and
`PERF_EVAL_FLASH_FORCE=1` adds `--force` for rev 0.x silicon.

Capture from the port that carries the console: USB-Serial-JTAG on P4X
(`/dev/ttyACM*`). A capture on the other node only sees the bootloader output. The P4 EV
board has no USB-Serial-JTAG wired to the host and its board defaults still select it,
so switch the console to UART0 in `menuconfig` before capturing on `/dev/ttyUSB*`.

## Baselines

`tools/brookesia_gui_perf` compares a run against
`reports/baseline/<board_id>/Baseline.<board_id>.json`. Only a capture made from a
clean tree is authoritative: `git_sha` has to resolve to a commit, `worktree_dirty`
has to be `false`, and `worktree_diff_sha256` has to be empty. The shipped baseline is
`esp32_p4x_function_ev`; captures whose `git_sha` carries a `-dirty` suffix are
historical and are not part of the comparable set.

## Running

The firmware starts the benchmark automatically and prints
`perf_eval: completed` when every stage finished. `pytest_perf_eval.py` drives
it on a device and asserts the object gate, the materialised corpus and the
report contents.
