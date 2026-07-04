# Custom Service Test App

This test app supports both ESP-IDF/Unity and PC/Boost.Test builds.

## PC Build

Run from the repository root:

```bash
cmake -S service/framework/brookesia_service_custom/test_apps \
      -B /tmp/service_custom_pc \
      -DBROOKESIA_SERVICE_CUSTOM_TEST_APPS_FORCE_PC=ON
cmake --build /tmp/service_custom_pc -j4
ctest --test-dir /tmp/service_custom_pc --output-on-failure
```

`BROOKESIA_SERVICE_CUSTOM_TEST_APPS_FORCE_PC=ON` is useful when `IDF_PATH` is exported and you still want the PC path.

If system Boost.Test is unavailable, provide an esp-boost source tree:

```bash
cmake -S service/framework/brookesia_service_custom/test_apps \
      -B /tmp/service_custom_pc \
      -DBROOKESIA_SERVICE_CUSTOM_TEST_APPS_FORCE_PC=ON \
      -DBROOKESIA_TEST_BOOST_ROOT=/path/to/esp-boost/src
```

Use `ctest -V` or run the binary directly to see each case banner:

```bash
ctest --test-dir /tmp/service_custom_pc -V
/tmp/service_custom_pc/test_brookesia_service_custom --run_test=test_customservice_standard_case_1 --log_level=test_suite
```

Each PC case prints a line like `[TEST] Running: ...` before it starts.

## ESP-IDF Build

```bash
. ${IDF_PATH}/export.sh
idf.py -C service/framework/brookesia_service_custom/test_apps set-target esp32s3
idf.py -C service/framework/brookesia_service_custom/test_apps build
```

## ESP-IDF Pytest

```bash
pytest service/framework/brookesia_service_custom/test_apps --target esp32s3 --env generic,octal-psram
```
