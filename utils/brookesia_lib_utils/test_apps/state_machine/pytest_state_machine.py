# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

# '''
# Steps to run these cases (Take `esp32s3` as an example):
#
# - Build
#   - . ${IDF_PATH}/export.sh
#   - export IDF_CI_BUILD=y
#   - pip install idf_build_apps
#   - idf-build-apps build -t esp32s3 --manifest-files=".build-rules.yml" --path='./utils/brookesia_lib_utils/test_apps/state_machine' --recursive --build-dir="@v/build_@t_@w"
#
# - Test
#   - ${IDF_PATH}/install.sh --enable-pytest
#   - ${IDF_PATH}/install.sh --enable-test-specific
#   - pytest utils/brookesia_lib_utils/test_apps/state_machine --target esp32s3 --env generic
# '''

import pytest
from pytest_embedded import Dut
import time

SUCCESS_RESPONSE = b'0 Failures'
FAILURE_RESPONSE = b'1 Failures'
ENTER_RESPONSE_LIST = [
    b'Enter test for running',
    b'Press ENTER to see the list of tests',
]
REBOOT_RESPONSE = b'Rebooting...'
TIMEOUT_S = 300
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


def test(dut: Dut)-> None:
    dut.expect(ENTER_RESPONSE_LIST, timeout=5)

    dut.write('\n\n')

    response = dut.expect(ENTER_RESPONSE_LIST, return_what_before_match=True, timeout=5)
    index_and_name_list = get_index_and_name_list(response)
    print(f"index_and_name_list: {index_and_name_list}")

    failed_name_and_numbers = []
    for num, name in index_and_name_list:
        retries = 0
        success = False

        while not success and retries < RETRY_LIMIT:
            print(f"[{num}] [{name}] Writing: {num}")
            dut.write(f"{num}\n")
            start_time = time.time()

            while True:
                try:
                    response = dut.expect([SUCCESS_RESPONSE, FAILURE_RESPONSE, REBOOT_RESPONSE], timeout=TIMEOUT_S).group(0)
                    print(f"[{num}] [{name}] Received: {response}")
                    time.sleep(0.1)

                    if response == SUCCESS_RESPONSE:
                        success = True
                        break
                    elif response == FAILURE_RESPONSE:
                        retries += 1
                        print(f"[{num}] [{name}] Retrying...")
                        break
                    elif response == REBOOT_RESPONSE:
                        failed_name_and_numbers.append((name, num))
                        print(f"The following numbers failed or timed out: {failed_name_and_numbers}")
                        pytest.fail(f"[{num}] [{name}] Device rebooted unexpectedly.")
                        break
                except Exception:
                    if time.time() - start_time > TIMEOUT_S:
                        failed_name_and_numbers.append((name, num))
                        print(f"The following numbers failed or timed out: {failed_name_and_numbers}")
                        pytest.fail(f"[{num}] [{name}] Timeout exceeded {TIMEOUT_S}s.")
                        break

        if retries == RETRY_LIMIT:
            failed_name_and_numbers.append((name, num))
            print(f"[{num}] [{name}] Marked as failed after {RETRY_LIMIT} attempts.")

    if len(failed_name_and_numbers) > 0:
        pytest.fail(f"The following numbers failed or timed out: {failed_name_and_numbers}")


@pytest.mark.target('esp32s3')
@pytest.mark.env('generic')
@pytest.mark.parametrize(
    'config',
    [
        'defaults',
    ],
)
@pytest.mark.timeout(30 * 60)  # 30 minutes
def test_esp32s3(dut: Dut)-> None:
    test(dut)


@pytest.mark.target('esp32p4')
@pytest.mark.env('generic,eco4')
@pytest.mark.parametrize(
    'config',
    [
        'defaults',
    ],
)
@pytest.mark.timeout(30 * 60)  # 30 minutes
def test_esp32p4(dut: Dut)-> None:
    test(dut)
