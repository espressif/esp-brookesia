# HTTP Service Test App

This test app supports both ESP-IDF/Unity and PC/Boost.Test builds.

## PC Build

Run from the repository root:

```bash
cmake -S service/network/brookesia_service_http/test_apps \
      -B /tmp/service_http_pc \
      -DBROOKESIA_SERVICE_HTTP_TEST_APPS_FORCE_PC=ON
cmake --build /tmp/service_http_pc -j4
ctest --test-dir /tmp/service_http_pc --output-on-failure
```

`BROOKESIA_SERVICE_HTTP_TEST_APPS_FORCE_PC=ON` is recommended when `IDF_PATH` is exported, otherwise the top-level CMake file selects the ESP-IDF build path.

Useful PC options:

```bash
cmake -S service/network/brookesia_service_http/test_apps \
      -B /tmp/service_http_pc \
      -DBROOKESIA_SERVICE_HTTP_TEST_APPS_FORCE_PC=ON \
      -DBROOKESIA_SERVICE_HTTP_TEST_APPS_ENABLE_NETWORK=ON \
      -DBROOKESIA_SERVICE_HTTP_TEST_APPS_ECHO_URL=https://echo.apifox.com
```

`https://echo.apifox.com` is the default HTTP echo endpoint. If it is unavailable on your network, use another
httpbin-compatible service such as `https://httpbingo.org`; the network tests require `/get`, `/post`, `/put`,
`/patch`, `/delete`, `/response-headers`, `/status/{code}`, `/bytes/{n}`, and `/delay/{n}`.

If system Boost.Test is unavailable, point to an esp-boost source tree:

```bash
cmake -S service/network/brookesia_service_http/test_apps \
      -B /tmp/service_http_pc \
      -DBROOKESIA_SERVICE_HTTP_TEST_APPS_FORCE_PC=ON \
      -DBROOKESIA_TEST_BOOST_ROOT=/path/to/esp-boost/src
```

## PC Run

Run all tests:

```bash
/tmp/service_http_pc/test_brookesia_service_http
```

Run selected Boost.Test cases:

```bash
/tmp/service_http_pc/test_brookesia_service_http --run_test=tls_verify_modes --log_level=test_suite
/tmp/service_http_pc/test_brookesia_service_http --run_test=sync_get_request_flow
/tmp/service_http_pc/test_brookesia_service_http --run_test=download_response_to_file
```

## PC Quality Checks

The generic PC quality tool lives in `utils/brookesia_lib_utils/tools`. It does not contain HTTP-specific defaults, so pass the HTTP test app CMake options explicitly. The recommended HTTP commands enable the network test branch by default:

Run static analysis, ASan/LSan, and heaptrack in one pass:

```bash
python3 utils/brookesia_lib_utils/tools/pc_quality_check.py all \
    --source-dir service/network/brookesia_service_http/test_apps \
    --target test_brookesia_service_http \
    --build-root build/service_http_test_quality \
    --scope-prefix service/network/brookesia_service_http/test_apps/main \
    --scope-prefix service/network/brookesia_service_http/src \
    --cmake-arg=-DBROOKESIA_SERVICE_HTTP_TEST_APPS_FORCE_PC=ON \
    --cmake-arg=-DBROOKESIA_SERVICE_HTTP_TEST_APPS_ENABLE_NETWORK=ON \
    --program-arg=--run_test=enum_string_roundtrip,helper_schema_and_serialization,service_lifecycle_and_availability,sync_get_request_flow,download_response_to_file \
    --program-arg=--log_level=test_suite \
    --keep-going
```

Run only static analysis:

```bash
python3 utils/brookesia_lib_utils/tools/pc_quality_check.py static \
    --source-dir service/network/brookesia_service_http/test_apps \
    --target test_brookesia_service_http \
    --build-root build/service_http_test_quality \
    --scope-prefix service/network/brookesia_service_http/test_apps/main \
    --scope-prefix service/network/brookesia_service_http/src \
    --cmake-arg=-DBROOKESIA_SERVICE_HTTP_TEST_APPS_FORCE_PC=ON \
    --cmake-arg=-DBROOKESIA_SERVICE_HTTP_TEST_APPS_ENABLE_NETWORK=ON
```

Run a short ASan/LSan smoke pass:

```bash
python3 utils/brookesia_lib_utils/tools/pc_quality_check.py asan \
    --source-dir service/network/brookesia_service_http/test_apps \
    --target test_brookesia_service_http \
    --build-root build/service_http_test_quality \
    --cmake-arg=-DBROOKESIA_SERVICE_HTTP_TEST_APPS_FORCE_PC=ON \
    --cmake-arg=-DBROOKESIA_SERVICE_HTTP_TEST_APPS_ENABLE_NETWORK=ON \
    --program-arg=--run_test=enum_string_roundtrip,helper_schema_and_serialization,service_lifecycle_and_availability,sync_get_request_flow,download_response_to_file \
    --program-arg=--log_level=test_suite
```

Run a short heaptrack pass:

```bash
python3 utils/brookesia_lib_utils/tools/pc_quality_check.py heaptrack \
    --source-dir service/network/brookesia_service_http/test_apps \
    --target test_brookesia_service_http \
    --build-root build/service_http_test_quality \
    --cmake-arg=-DBROOKESIA_SERVICE_HTTP_TEST_APPS_FORCE_PC=ON \
    --cmake-arg=-DBROOKESIA_SERVICE_HTTP_TEST_APPS_ENABLE_NETWORK=ON \
    --program-arg=--run_test=sync_get_request_flow,download_response_to_file
```

To use an esp-boost source tree for Boost.Test headers, add:

```bash
--cmake-arg=-DBROOKESIA_TEST_BOOST_ROOT=/path/to/esp-boost/src
```

To analyze or run the PC tests without the network branch, replace the network option with:

```bash
--cmake-arg=-DBROOKESIA_SERVICE_HTTP_TEST_APPS_ENABLE_NETWORK=OFF
```

## ESP-IDF Build

Run from the repository root:

```bash
. /home/lzw/esp/work/esp-idf-github/export.sh
idf.py -C service/network/brookesia_service_http/test_apps set-target esp32s3
idf.py -C service/network/brookesia_service_http/test_apps build
```

To enable real Wi-Fi network tests, set CI Wi-Fi credentials before CMake configure/build:

```bash
export CI_TEST_WIFI_2_4G_AP1_SSID="your-ssid"
export CI_TEST_WIFI_2_4G_AP1_PSW="your-password"
. /home/lzw/esp/work/esp-idf-github/export.sh
idf.py -C service/network/brookesia_service_http/test_apps reconfigure
idf.py -C service/network/brookesia_service_http/test_apps build
```

Without the CI Wi-Fi variables, network-dependent Unity cases are not enabled by the ESP build configuration.

## ESP-IDF Pytest

After building/flashing in a CI-compatible environment:

```bash
pytest service/network/brookesia_service_http/test_apps --target esp32s3 --env generic,octal-psram
```
