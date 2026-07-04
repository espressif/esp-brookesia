# Service Manager Test App

This test app supports both ESP-IDF/Unity and PC/Boost.Test builds.

## PC Build

Run from the repository root:

```bash
cmake -S service/framework/brookesia_service_manager/test_apps \
      -B /tmp/service_manager_pc \
      -DBROOKESIA_SERVICE_MANAGER_TEST_APPS_FORCE_PC=ON
cmake --build /tmp/service_manager_pc -j4
ctest --test-dir /tmp/service_manager_pc --output-on-failure
```

If system Boost.Test is unavailable, set `BROOKESIA_TEST_BOOST_ROOT=/path/to/esp-boost/src`.

Use verbose ctest or direct Boost.Test execution to inspect case boundaries:

```bash
ctest --test-dir /tmp/service_manager_pc -V
/tmp/service_manager_pc/test_brookesia_service_manager --run_test=test_apis_bind_service --log_level=test_suite
```

Each PC case prints a `[TEST] Running: ...` banner before it starts.

## ESP-IDF Build

```bash
. ${IDF_PATH}/export.sh
idf.py -C service/framework/brookesia_service_manager/test_apps set-target esp32s3
idf.py -C service/framework/brookesia_service_manager/test_apps build
```

## ESP-IDF Pytest

```bash
pytest service/framework/brookesia_service_manager/test_apps --target esp32s3 --env generic,octal-psram
```
