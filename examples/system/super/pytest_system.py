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

   **Build for esp32_p4x_function_ev:**
   ```bash
   python .gitlab/tools/build_apps.py examples/system/super -t esp32p4 --config "sdkconfig.ci.board.esp32_p4x_function_ev=esp32_p4x_function_ev"
   ```

## Test

1. Install pytest dependencies:
   ```bash
   ${IDF_PATH}/install.sh --enable-ci
   ${IDF_PATH}/install.sh --enable-test-specific
   ```

2. Run pytest with the esp32p4x_function_ev_board environment:
   ```bash
   pytest examples/system/super --target esp32p4 --env esp32p4x_function_ev_board
   ```
'''

import pytest
from pytest_embedded import Dut

SUCCESS_RESPONSE = b'=== System Example Completed ==='
TIMEOUT_S = 10 * 60


def run_test(dut: Dut) -> None:
    dut.expect(SUCCESS_RESPONSE, timeout=TIMEOUT_S)


@pytest.mark.target('esp32p4')
@pytest.mark.env('esp32p4x_function_ev_board')
@pytest.mark.parametrize(
    'target, config',
    [
        ('esp32p4', 'esp32_p4x_function_ev'),
    ],
)
@pytest.mark.timeout(TIMEOUT_S)
def test_esp32_p4x_function_ev(dut: Dut) -> None:
    run_test(dut)
