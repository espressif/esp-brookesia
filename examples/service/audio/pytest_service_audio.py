# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-4.0

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

   **Build for a specific board:** (replace `esp32p4` and `esp32_p4_function_ev` with your target chip and board name)
   ```bash
   python .gitlab/tools/build_apps.py examples/service/audio -t esp32p4 --config "sdkconfig.ci.board.esp32_p4_function_ev=esp32_p4_function_ev"
   ```

## Test

1. Install pytest dependencies:
   ```bash
   ${IDF_PATH}/install.sh --enable-pytest
   ${IDF_PATH}/install.sh --enable-test-specific
   ```

2. Run pytest with appropriate target and environment:

   **esp32p4_function_ev_board examples:**
   ```bash
   pytest examples/service/audio --target esp32p4 --env generic,eco4,esp32p4_function_ev_board
   ```
'''

import pytest
from pytest_embedded import Dut
import time

SUCCESS_RESPONSE = b'=== Audio Service Example Completed ==='
TIMEOUT_S = 600


def run_test(dut: Dut)-> None:
    dut.expect(SUCCESS_RESPONSE, timeout=TIMEOUT_S)


@pytest.mark.target('esp32s3')
@pytest.mark.env('esp_vocat')
@pytest.mark.parametrize(
    'target, config',
    [
        ('esp32s3', 'esp_vocat_board_v1_2'),
    ],
)
@pytest.mark.timeout(TIMEOUT_S)
def test_esp_vocat_board_v1_2(dut: Dut)-> None:
    run_test(dut)


@pytest.mark.target('esp32p4')
@pytest.mark.env('generic,eco4,esp32p4_function_ev_board')
@pytest.mark.parametrize(
    'target, config',
    [
        ('esp32p4', 'esp32_p4_function_ev'),
    ],
)
@pytest.mark.timeout(TIMEOUT_S)
def test_esp32_p4_function_ev(dut: Dut)-> None:
    run_test(dut)
