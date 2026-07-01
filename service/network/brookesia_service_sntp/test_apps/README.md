# SNTP Service Test App

This test app supports both ESP-IDF/Unity and PC/Boost.Test builds.
The `network_time_update` case requires a reachable network and a reachable NTP server; missing Wi-Fi credentials,
blocked DNS, or blocked UDP/123 are treated as test failures.

## PC Build

The PC path uses `brookesia_hal_linux`, Storage service KV isolation, and the generated `proot` launcher. Wi-Fi and
network readiness are provided by the Linux HAL Wi-Fi stub by default, while the SNTP sync itself performs a real UDP
NTP query.

Install dependencies first:

```bash
hal/brookesia_hal_linux/scripts/install_linux_deps.sh --full
```

Run from the repository root:

```bash
cmake -S service/network/brookesia_service_sntp/test_apps \
      -B /tmp/service_sntp_pc \
      -DBROOKESIA_SERVICE_SNTP_TEST_APPS_FORCE_PC=ON
cmake --build /tmp/service_sntp_pc -j4
ctest --test-dir /tmp/service_sntp_pc --output-on-failure
```

If system Boost.Test is unavailable, set `BROOKESIA_TEST_BOOST_ROOT=/path/to/esp-boost/src`.

Use verbose ctest or the generated launcher to see case names and state transitions:

```bash
ctest --test-dir /tmp/service_sntp_pc -V
bash /tmp/service_sntp_pc/run_test_brookesia_service_sntp.sh --run_test=test_servicesntp_start_and_stop --log_level=test_suite
```

The launcher requires `proot`; running the bare binary does not guarantee transparent absolute storage paths.

Useful PC overrides:

```bash
cmake -S service/network/brookesia_service_sntp/test_apps \
      -B /tmp/service_sntp_pc \
      -DBROOKESIA_SERVICE_SNTP_TEST_APPS_FORCE_PC=ON \
      -DBROOKESIA_SERVICE_SNTP_TEST_APPS_WIFI_BACKEND=stub \
      -DBROOKESIA_SERVICE_SNTP_TEST_APPS_NTP_SERVER=time.google.com \
      -DBROOKESIA_SERVICE_SNTP_TEST_APPS_SYNC_TIMEOUT_MS=90000
```

## ESP-IDF Build

```bash
. ${IDF_PATH}/export.sh
idf.py -C service/network/brookesia_service_sntp/test_apps set-target esp32s3
idf.py -C service/network/brookesia_service_sntp/test_apps build
```

## ESP-IDF Pytest

```bash
pytest service/network/brookesia_service_sntp/test_apps --target esp32s3 --env generic,octal-psram
```

For ESP network tests, provide Wi-Fi credentials through the same CI environment variables used by other network service
tests:

```bash
export CI_TEST_WIFI_2_4G_AP1_SSID="your-ssid"
export CI_TEST_WIFI_2_4G_AP1_PSW="your-password"
export TEST_SNTP_NTP_SERVER="pool.ntp.org"
```
