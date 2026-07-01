# Storage Service Test App

This test app supports both ESP-IDF/Unity and PC/Boost.Test builds.

## PC Build

Install dependencies first. The easiest path is:

```bash
hal/brookesia_hal_linux/scripts/install_linux_deps.sh --full
```

Run from the repository root:

```bash
cmake -S service/system/brookesia_service_storage/test_apps \
      -B /tmp/service_storage_pc \
      -DBROOKESIA_SERVICE_STORAGE_TEST_APPS_FORCE_PC=ON
cmake --build /tmp/service_storage_pc -j4
ctest --test-dir /tmp/service_storage_pc --output-on-failure
```

`BROOKESIA_SERVICE_STORAGE_TEST_APPS_FORCE_PC=ON` is recommended when `IDF_PATH` is exported, otherwise the top-level
CMake file selects the ESP-IDF build path.

If system Boost.Test is unavailable, point to an esp-boost source tree:

```bash
cmake -S service/system/brookesia_service_storage/test_apps \
      -B /tmp/service_storage_pc \
      -DBROOKESIA_SERVICE_STORAGE_TEST_APPS_FORCE_PC=ON \
      -DBROOKESIA_TEST_BOOST_ROOT=/path/to/esp-boost/src
```

The PC CMake path reuses the shared defaults exported by
`service/system/brookesia_service_storage/cmake/pc_platform.cmake`:

- `BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_SYSTEM_DIR`
- `BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_KV_DIR`
- `BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_FS_ROOT`
- `BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_ROOTFS_DIR`

This test app defaults its PC KV directory to
`BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_KV_DIR`.

## PC Run

Prefer `ctest`, which uses the generated `proot` launcher:

```bash
ctest --test-dir /tmp/service_storage_pc --output-on-failure
ctest --test-dir /tmp/service_storage_pc -V
```

For manual runs, use the generated launcher instead of invoking the binary
directly:

```bash
bash /tmp/service_storage_pc/run_test_brookesia_service_storage.sh
```

Run selected Boost.Test cases:

```bash
bash /tmp/service_storage_pc/run_test_brookesia_service_storage.sh --run_test=basic_set_and_get --log_level=test_suite
bash /tmp/service_storage_pc/run_test_brookesia_service_storage.sh --run_test=helper_string_roundtrip
```

Each PC case prints a line like:

```text
[TEST] Running: Test ServiceStorage - basic set and get (basic_set_and_get)
```

The PC path uses an isolated `BROOKESIA_HAL_LINUX_KV_DIR` under the build directory instead of the default
executable-directory fallback. The generated launcher also binds host
directories to `/spiffs`, `/littlefs`, `/fatfs`, and `/sdcard`, so transparent
absolute-path access works during the supported PC run flow. The test runner
clears the KV directory before each run so persisted key-value data does not
leak across runs.

Running the bare `test_brookesia_service_storage` binary is still possible for
ad hoc debugging, but it does not guarantee transparent `/littlefs` or
`/sdcard` absolute-path behavior.

## ESP-IDF Build

Run from the repository root:

```bash
. ${IDF_PATH}/export.sh
idf.py -C service/system/brookesia_service_storage/test_apps set-target esp32s3
idf.py -C service/system/brookesia_service_storage/test_apps build
```

## ESP-IDF Pytest

After building/flashing in a CI-compatible environment:

```bash
pytest service/system/brookesia_service_storage/test_apps --target esp32s3 --env generic,octal-psram
```
