# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
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

   **Build for a specific target (replace `esp32s3` with your target chip: `esp32s3` or `esp32p4`):**
   ```bash
   python .gitlab/tools/build_apps.py utils/brookesia_lib_utils/test_apps/thread_config -t esp32s3
   ```

   **Build for all CI targets (recommended for CI):**
   ```bash
   python .gitlab/tools/build_apps.py utils/brookesia_lib_utils/test_apps/thread_config -t all
   ```

## Test

1. Install pytest dependencies:
   ```bash
   ${IDF_PATH}/install.sh --enable-ci
   ${IDF_PATH}/install.sh --enable-test-specific
   ```

2. Run pytest with appropriate target and environment:

   **ESP32-S3 PSRAM examples:**
   ```bash
   # Generic Octal PSRAM environment
   pytest utils/brookesia_lib_utils/test_apps/thread_config --target esp32s3 --env "generic,octal-psram"
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
        ('esp32s3', 'defaults'), ('esp32s3', 'esp32s3_psram_xip'),
    ],
)
@pytest.mark.timeout(10 * 60)
def test_esp32s3_psram(dut: Dut)-> None:
    run_unity_menu(dut)


@pytest.mark.target('esp32p4')
@pytest.mark.env('jtag,esp32p4_rev3')
@pytest.mark.parametrize(
    'target, config',
    [
        ('esp32p4', 'defaults'), ('esp32p4', 'esp32p4_xip'),
    ],
)
@pytest.mark.timeout(10 * 60)
def test_esp32p4(dut: Dut)-> None:
    run_unity_menu(dut)
