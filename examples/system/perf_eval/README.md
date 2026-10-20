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

```bash
# build (BOARD/TARGET can be overridden)
./build_perf_eval.sh

# flash: the acknowledgement is mandatory because this erases the data region
PERF_EVAL_ACK_ERASE=1 ./build_perf_eval.sh flash monitor
```

## Running

The firmware starts the benchmark automatically and prints
`perf_eval: completed` when every stage finished. `pytest_perf_eval.py` drives
it on a device and asserts the object gate, the materialised corpus and the
report contents.
