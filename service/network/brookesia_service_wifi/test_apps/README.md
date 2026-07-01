# Wi-Fi Service Test App

This test app supports both ESP-IDF/Unity and PC/Boost.Test builds.

## PC Build

The PC path defaults `BROOKESIA_HAL_LINUX_WIFI_BACKEND=stub` so tests are deterministic and do not depend on local
NetworkManager permissions. Override that CMake variable explicitly if you want to experiment with a real Linux Wi-Fi
backend.

Install dependencies first:

```bash
hal/brookesia_hal_linux/scripts/install_linux_deps.sh --full
```

Run from the repository root:

```bash
cmake -S service/network/brookesia_service_wifi/test_apps \
      -B /tmp/service_wifi_pc \
      -DBROOKESIA_SERVICE_WIFI_TEST_APPS_FORCE_PC=ON
cmake --build /tmp/service_wifi_pc -j4
ctest --test-dir /tmp/service_wifi_pc --output-on-failure
```

If system Boost.Test is unavailable, set `BROOKESIA_TEST_BOOST_ROOT=/path/to/esp-boost/src`.

Use verbose ctest or the generated launcher to see each case banner:

```bash
ctest --test-dir /tmp/service_wifi_pc -V
bash /tmp/service_wifi_pc/run_test_brookesia_service_wifi.sh --run_test=test_servicewifi_state_transitions --log_level=test_suite
```

The launcher requires `proot`; running the bare binary does not guarantee transparent absolute storage paths.

## ESP-IDF Build

Set Wi-Fi credentials as needed through the existing sdkconfig defaults or CI environment, then build:

```bash
. ${IDF_PATH}/export.sh
idf.py -C service/network/brookesia_service_wifi/test_apps set-target esp32s3
idf.py -C service/network/brookesia_service_wifi/test_apps build
```

## ESP-IDF Pytest

```bash
pytest service/network/brookesia_service_wifi/test_apps --target esp32s3 --env generic,octal-psram
```
