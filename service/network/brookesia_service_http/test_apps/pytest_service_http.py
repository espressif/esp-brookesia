# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

'''
Steps to run these test cases:

## Build

1. Setup ESP-IDF environment:
   ```bash
   . ${IDF_PATH}/export.sh
   export IDF_CI_BUILD=y
   ```

2. Optionally setup Wi-Fi credentials for network cases:
   ```bash
   export CI_TEST_WIFI_2_4G_AP1_SSID="your_wifi_ssid"
   export CI_TEST_WIFI_2_4G_AP1_PSW="your_wifi_password"
   ```

3. Build the test app:
   ```bash
   python .gitlab/tools/build_apps.py service/network/brookesia_service_http/test_apps -t esp32s3
   ```

## Test

```bash
pytest service/network/brookesia_service_http/test_apps --target esp32s3 --env "generic,octal-psram"
```
'''
import pytest
from pytest_embedded import Dut

from unity_menu_runner import run_unity_menu


@pytest.mark.target('esp32s3')
@pytest.mark.env('generic,octal-psram')
@pytest.mark.parametrize(
    'target, config',
    [
        ('esp32s3', 'defaults'),
        ('esp32s3', 'esp32s3_psram_xip'),
    ],
)
@pytest.mark.timeout(60 * 60)
def test_esp32s3_psram(dut: Dut)-> None:
    run_unity_menu(dut, response_timeout_s=5 * 60, single_timeout_s=20 * 60)


@pytest.mark.target('esp32p4')
@pytest.mark.env('esp32p4x_function_ev_board')
@pytest.mark.parametrize(
    'target, config',
    [
        ('esp32p4', 'defaults'),
        ('esp32p4', 'esp32p4_xip'),
    ],
)
@pytest.mark.timeout(60 * 60)
def test_esp32p4(dut: Dut)-> None:
    run_unity_menu(dut, response_timeout_s=5 * 60, single_timeout_s=20 * 60)
