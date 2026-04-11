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

3. Build the example:

   **Build with specific target:**
   ```bash
   python .gitlab/tools/build_apps.py examples/service/wifi -t esp32s3
   ```

   **Build for all targets:**
   ```bash
   python .gitlab/tools/build_apps.py examples/service/wifi -t all
   ```

## Test

1. Install pytest dependencies:
   ```bash
   ${IDF_PATH}/install.sh --enable-pytest
   ${IDF_PATH}/install.sh --enable-test-specific
   ```

2. Run pytest with appropriate target and environment:

   **ESP32-S3 examples:**
   ```bash
   # Generic environment
   pytest examples/service/wifi --target esp32s3 --env generic
'''

import pytest
from pytest_embedded import Dut
import time

SUCCESS_RESPONSE = b'=== Wifi Service Example Completed ==='
TIMEOUT_S = 10 * 60


def run_test(dut: Dut)-> None:
    dut.expect(SUCCESS_RESPONSE, timeout=TIMEOUT_S)


@pytest.mark.target('esp32s3')
@pytest.mark.env('generic')
@pytest.mark.parametrize(
    'target, config',
    [
        ('esp32s3', 'defaults'),
    ],
)
@pytest.mark.timeout(TIMEOUT_S)
def test_esp32s3(dut: Dut)-> None:
    run_test(dut)


@pytest.mark.target('esp32s3')
@pytest.mark.env('generic,octal-psram')
@pytest.mark.parametrize(
    'target, config',
    [
        ('esp32s3', 'esp32s3_octal_psram'), ('esp32s3', 'esp32s3_octal_psram_xip'),
    ],
)
@pytest.mark.timeout(TIMEOUT_S)
def test_esp32s3_octal_psram(dut: Dut)-> None:
    run_test(dut)


@pytest.mark.target('esp32p4')
@pytest.mark.env('generic,eco4,esp32p4_function_ev_board')
@pytest.mark.parametrize(
    'target, config',
    [
        ('esp32p4', 'defaults'), ('esp32p4', 'esp32p4_xip'),
    ],
)
@pytest.mark.timeout(TIMEOUT_S)
def test_esp32p4(dut: Dut)-> None:
    run_test(dut)
