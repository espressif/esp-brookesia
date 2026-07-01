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

2. Install dependencies:
   ```bash
   pip install idf_build_apps
   ```

3. Build the test app:

   **Build for a specific board:** (replace `esp32p4` and `esp32_p4x_function_ev` with your target chip and board name)
   ```bash
   python .gitlab/tools/build_apps.py hal/brookesia_hal_adaptor/test_apps -t esp32p4 --config "sdkconfig.ci.board.esp32_p4x_function_ev=esp32_p4x_function_ev"
   ```

## Test

1. Install pytest dependencies:
   ```bash
   ${IDF_PATH}/install.sh --enable-ci
   ${IDF_PATH}/install.sh --enable-test-specific
   ```

2. Run pytest with appropriate target and environment:

   **esp32p4x_function_ev_board examples:**
   ```bash
   pytest hal/brookesia_hal_adaptor/test_apps --target esp32p4 --env esp32p4x_function_ev_board
   ```
'''
import pytest
from pytest_embedded import Dut

from unity_menu_runner import run_unity_menu


@pytest.mark.target('esp32s3')
@pytest.mark.env('esp_vocat')
@pytest.mark.parametrize(
    'target, config',
    [
        ('esp32s3', 'esp_vocat_board_v1_2'),
    ],
)
@pytest.mark.timeout(10 * 60)
def test_esp_vocat_board_v1_2(dut: Dut)-> None:
    run_unity_menu(dut, single_timeout_s=2 * 60)


@pytest.mark.target('esp32p4')
@pytest.mark.env('esp32p4x_function_ev_board')
@pytest.mark.parametrize(
    'target, config',
    [
        ('esp32p4', 'esp32_p4x_function_ev'),
    ],
)
@pytest.mark.timeout(10 * 60)
def test_esp32_p4x_function_ev(dut: Dut)-> None:
    run_unity_menu(dut, single_timeout_s=2 * 60)
