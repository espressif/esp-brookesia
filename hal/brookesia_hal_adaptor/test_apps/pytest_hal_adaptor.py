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

   **Build for a specific board:** (replace `esp32p4` and `esp32_p4_function_ev` with your target chip and board name)
   ```bash
   python .gitlab/tools/build_apps.py hal/brookesia_hal_adaptor/test_apps -t esp32p4 --config "sdkconfig.ci.board.esp32_p4_function_ev=esp32_p4_function_ev"
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
   pytest hal/brookesia_hal_adaptor/test_apps --target esp32p4 --env generic,eco4,esp32p4_function_ev_board
   ```
'''

import pytest
from pytest_embedded import Dut
import time

SUCCESS_RESPONSE = b'0 Failures'
FAILURE_RESPONSE = b'1 Failures'
LEAK_MEMORY_RESPONSE = b'The test leaked too much memory'
ENTER_RESPONSE_LIST = [
    b'Enter test for running',
    b'Press ENTER to see the list of tests',
]
REBOOT_RESPONSE = b'Rebooting...'
RESPONSE_TIMEOUT_S = 30
SINGLE_TIMEOUT_S = 2 * 60
TOTAL_TIMEOUT_S = 10 * 60
RETRY_LIMIT = 5  # Retry once before recording failure


def get_index_and_name_list(response: bytes):
    import re
    # Find all numbers in parentheses like (1), (2), etc.
    index_pattern = rb'\((\d+)\)'
    # Extract the test name in quotes
    name_pattern = rb'"([^"]+)"'

    indices = re.findall(index_pattern, response)
    names = re.findall(name_pattern, response)

    result = []
    for i in range(min(len(indices), len(names))):
        result.append((int(indices[i]), names[i].decode('utf-8').strip()))

    return result


def run_test(dut: Dut)-> None:
    dut.expect(ENTER_RESPONSE_LIST, timeout=RESPONSE_TIMEOUT_S)

    dut.write('\n\n')

    response = dut.expect(ENTER_RESPONSE_LIST, return_what_before_match=True, timeout=RESPONSE_TIMEOUT_S)
    index_and_name_list = get_index_and_name_list(response)
    print(f"index_and_name_list: {index_and_name_list}")

    for num, name in index_and_name_list:
        retries = 0
        success = False
        leaked_memory = False

        while not success and retries < RETRY_LIMIT:
            print(f"[{num}] [{name}] Writing: {num}")
            dut.write(f"{num}\n")
            start_time = time.time()

            while True:
                try:
                    response = dut.expect([SUCCESS_RESPONSE, LEAK_MEMORY_RESPONSE, FAILURE_RESPONSE, REBOOT_RESPONSE], timeout=RESPONSE_TIMEOUT_S).group(0)
                    print(f"[{num}] [{name}] Received: {response}")
                    time.sleep(0.1)

                    if response == SUCCESS_RESPONSE:
                        leaked_memory = False
                        success = True
                        break
                    elif response == LEAK_MEMORY_RESPONSE:
                        retries += 1
                        leaked_memory = True
                        print(f"[{num}] [{name}] Leaked memory, retrying...")
                        break
                    elif response == FAILURE_RESPONSE:
                        if leaked_memory:
                            leaked_memory = False
                            print(f"Skip leaked memory test")
                            continue
                        pytest.fail(f"[{num}] [{name}] Failed")
                        break
                    elif response == REBOOT_RESPONSE:
                        pytest.fail(f"[{num}] [{name}] Device rebooted unexpectedly.")
                        break
                except Exception:
                    if time.time() - start_time > SINGLE_TIMEOUT_S:
                        pytest.fail(f"[{num}] [{name}] Timeout exceeded {SINGLE_TIMEOUT_S}s.")
                        break

        if retries == RETRY_LIMIT:
            pytest.fail(f"[{num}] [{name}] Failed after {RETRY_LIMIT} retries.")


@pytest.mark.target('esp32s3')
@pytest.mark.env('esp_vocat')
@pytest.mark.parametrize(
    'target, config',
    [
        ('esp32s3', 'esp_vocat_board_v1_2'),
    ],
)
@pytest.mark.timeout(TOTAL_TIMEOUT_S)
def test_esp_vocat_board_v1_0(dut: Dut)-> None:
    run_test(dut)


# esp32p4_function_ev_board is temporarily disabled due to lack of display
# @pytest.mark.target('esp32p4')
# @pytest.mark.env('generic,eco4,esp32p4_function_ev_board')
# @pytest.mark.parametrize(
#     'target, config',
#     [
#         ('esp32p4', 'esp32_p4_function_ev'),
#     ],
# )
# @pytest.mark.timeout(TOTAL_TIMEOUT_S)
# def test_esp32_p4_function_ev(dut: Dut)-> None:
#     run_test(dut)
