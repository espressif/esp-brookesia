# Device Service Test App

This test app supports both ESP-IDF/Unity and PC/Boost.Test builds.

## PC Build

The PC path uses `brookesia_hal_linux`, `brookesia_service_storage`, and the shared storage test launcher so KV data and
transparent `/littlefs`, `/spiffs`, `/fatfs`, and `/sdcard` paths stay isolated under the build directory.

Install dependencies first:

```bash
hal/brookesia_hal_linux/scripts/install_linux_deps.sh --full
```

Build and run from the repository root:

```bash
cmake -S service/system/brookesia_service_device/test_apps \
      -B /tmp/service_device_pc \
      -DBROOKESIA_SERVICE_DEVICE_TEST_APPS_FORCE_PC=ON
cmake --build /tmp/service_device_pc -j4
ctest --test-dir /tmp/service_device_pc --output-on-failure
```

If system Boost.Test is unavailable, set `BROOKESIA_TEST_BOOST_ROOT=/path/to/esp-boost/src`.

Use verbose ctest or the generated launcher to see case names:

```bash
ctest --test-dir /tmp/service_device_pc -V
bash /tmp/service_device_pc/run_test_brookesia_service_device.sh \
     --run_test=test_servicedevice_board_capabilities_and_reset_are_available \
     --log_level=test_suite
```

The launcher requires `proot`; running the bare binary does not guarantee transparent absolute storage paths.

## ESP-IDF Build

```bash
. ${IDF_PATH}/export.sh
idf.py -C service/system/brookesia_service_device/test_apps set-target esp32s3
idf.py -C service/system/brookesia_service_device/test_apps build
```

## ESP-IDF Pytest

```bash
pytest service/system/brookesia_service_device/test_apps --target esp32s3 --env generic,octal-psram
```
