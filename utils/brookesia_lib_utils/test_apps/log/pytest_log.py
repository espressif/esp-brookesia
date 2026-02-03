# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

# '''
# Steps to run these cases (Take `esp32s3` as an example):
#
# - Build
#   - . ${IDF_PATH}/export.sh
#   - export IDF_CI_BUILD=y
#   - pip install idf_build_apps
#   - idf-build-apps build -t esp32s3 --manifest-files=".build-rules.yml" --path='./utils/brookesia_lib_utils/test_apps/log' --recursive --build-dir="@v/build_@t_@w"
#
# - Test
#   - ${IDF_PATH}/install.sh --enable-pytest
#   - ${IDF_PATH}/install.sh --enable-test-specific
#   - pytest utils/brookesia_lib_utils/test_apps/log --target esp32s3 --env generic
# '''

import pytest
from pytest_embedded import Dut
import time
import re

SUCCESS_RESPONSE = b'0 Failures'
FAILURE_RESPONSE = b'1 Failures'
ENTER_RESPONSE_LIST = [
    b'Enter test for running',
    b'Press ENTER to see the list of tests',
]
REBOOT_RESPONSE = b'Rebooting...'
TIMEOUT_S = 300
RETRY_LIMIT = 5  # Retry once before recording failure

# Expected log messages for different log levels
LOG_LEVEL_MARKERS = {
    'trace': b'[LOG_LEVEL_VERIFY] Trace level message',
    'debug': b'[LOG_LEVEL_VERIFY] Debug level message',
    'info': b'[LOG_LEVEL_VERIFY] Info level message',
    'warning': b'[LOG_LEVEL_VERIFY] Warning level message',
    'error': b'[LOG_LEVEL_VERIFY] Error level message',
}

# Log level configuration mapping
LOG_LEVEL_CONFIG = {
    # ESP_LOGV() and ESP_LOGD() print a large amount of log output which can cause tests to take too long, so they are not tested
    # 'esp_trace': {'expected': ['trace', 'debug', 'info', 'warning', 'error'], 'not_expected': []},
    # 'esp_debug': {'expected': ['debug', 'info', 'warning', 'error'], 'not_expected': ['trace']},
    'esp_info': {'expected': ['info', 'warning', 'error'], 'not_expected': ['trace', 'debug']},
    'esp_warning': {'expected': ['warning', 'error'], 'not_expected': ['trace', 'debug', 'info']},
    'esp_error': {'expected': ['error'], 'not_expected': ['trace', 'debug', 'info', 'warning']},
    'esp_none': {'expected': [], 'not_expected': ['trace', 'debug', 'info', 'warning', 'error']},
    'std_trace': {'expected': ['trace', 'debug', 'info', 'warning', 'error'], 'not_expected': []},
    'std_debug': {'expected': ['debug', 'info', 'warning', 'error'], 'not_expected': ['trace']},
    'std_info': {'expected': ['info', 'warning', 'error'], 'not_expected': ['trace', 'debug']},
    'std_warning': {'expected': ['warning', 'error'], 'not_expected': ['trace', 'debug', 'info']},
    'std_error': {'expected': ['error'], 'not_expected': ['trace', 'debug', 'info', 'warning']},
    'std_none': {'expected': [], 'not_expected': ['trace', 'debug', 'info', 'warning', 'error']},
    'defaults': {'expected': ['info', 'warning', 'error'], 'not_expected': ['trace', 'debug']},  # Default is usually INFO
}


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


def verify_log_level_output(dut: Dut, config_name: str, captured_output: bytes) -> None:
    """Verify that log output matches the expected log level configuration."""
    if config_name not in LOG_LEVEL_CONFIG:
        print(f"Warning: Unknown config '{config_name}', skipping log level verification")
        return

    config = LOG_LEVEL_CONFIG[config_name]
    expected_levels = config['expected']
    not_expected_levels = config['not_expected']

    print(f"\n=== Verifying log level for config: {config_name} ===")
    print(f"Expected levels: {expected_levels}")
    print(f"Not expected levels: {not_expected_levels}")

    # Check for expected log messages in captured output
    found_levels = []
    missing_levels = []
    for level in expected_levels:
        marker = LOG_LEVEL_MARKERS[level]
        if marker in captured_output:
            found_levels.append(level)
            print(f"✓ Found expected log level: {level}")
        else:
            missing_levels.append(level)
            print(f"✗ Missing expected log level: {level}")

    # Check that unexpected log messages are NOT present
    unexpected_found = []
    for level in not_expected_levels:
        marker = LOG_LEVEL_MARKERS[level]
        if marker in captured_output:
            unexpected_found.append(level)
            print(f"✗ Found unexpected log level (should be filtered): {level}")

    # Verify results
    if missing_levels:
        pytest.fail(f"Missing expected log levels for config '{config_name}': {missing_levels}")

    if unexpected_found:
        pytest.fail(f"Found unexpected log levels for config '{config_name}' (should be filtered): {unexpected_found}")

    print(f"✓ Log level verification passed for config: {config_name}\n")


def test(dut: Dut, config: str = 'defaults')-> None:
    dut.expect(ENTER_RESPONSE_LIST, timeout=5)

    dut.write('\n\n')

    response = dut.expect(ENTER_RESPONSE_LIST, return_what_before_match=True, timeout=5)
    index_and_name_list = get_index_and_name_list(response)
    print(f"index_and_name_list: {index_and_name_list}")

    # Find the log level test case
    log_level_test_num = None
    for num, name in index_and_name_list:
        if 'log level' in name.lower() or 'level' in name.lower():
            log_level_test_num = num
            print(f"log_level_test_num: {log_level_test_num}")
            break

    failed_name_and_numbers = []
    for num, name in index_and_name_list:
        retries = 0
        success = False

        while not success and retries < RETRY_LIMIT:
            print(f"[{num}] [{name}] Writing: {num}")
            dut.write(f"{num}\n")
            start_time = time.time()

            # Collect output for log level verification
            captured_output = b''
            test_completed = False

            while not test_completed:
                try:
                    # Capture output before matching the response for log verification
                    captured_before = dut.expect(
                        [SUCCESS_RESPONSE, FAILURE_RESPONSE, REBOOT_RESPONSE, b'Enter next test'],
                        timeout=TIMEOUT_S,
                        return_what_before_match=True
                    )
                    captured_output += captured_before

                    # Now expect the matched pattern (it should be immediately available)
                    response_match = dut.expect(
                        [SUCCESS_RESPONSE, FAILURE_RESPONSE, REBOOT_RESPONSE, b'Enter next test'],
                        timeout=2
                    )
                    response_type = response_match.group(0)
                    captured_output += response_type

                    print(f"[{num}] [{name}] Received: {response_type}")

                    # If this is the log level test, verify the output
                    if num == log_level_test_num:
                        verify_log_level_output(dut, config, captured_output)

                    time.sleep(0.1)

                    # Set flags and break out of inner loop
                    if response_type == SUCCESS_RESPONSE or response_type == b'Enter next test':
                        # Both indicate test completion
                        success = True
                        test_completed = True
                    elif response_type == FAILURE_RESPONSE:
                        retries += 1
                        print(f"[{num}] [{name}] Retrying...")
                        test_completed = True
                    elif response_type == REBOOT_RESPONSE:
                        failed_name_and_numbers.append((name, num))
                        print(f"The following numbers failed or timed out: {failed_name_and_numbers}")
                        pytest.fail(f"[{num}] [{name}] Device rebooted unexpectedly.")
                        test_completed = True
                except Exception as e:
                    if time.time() - start_time > TIMEOUT_S:
                        failed_name_and_numbers.append((name, num))
                        print(f"The following numbers failed or timed out: {failed_name_and_numbers}")
                        pytest.fail(f"[{num}] [{name}] Timeout exceeded {TIMEOUT_S}s.")
                        test_completed = True
                    else:
                        # Continue waiting if not timeout
                        print(f"[{num}] [{name}] Exception: {e}, continuing to wait...")
                        time.sleep(0.1)

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
        # ESP_LOGV() and ESP_LOGD() print a large amount of log output which can cause tests to take too long, so they are not tested
        # 'esp_trace', 'esp_debug',
        'esp_info', 'esp_warning', 'esp_error', 'esp_none',
        'std_trace', 'std_debug', 'std_info', 'std_warning', 'std_error', 'std_none',
    ],
)
@pytest.mark.timeout(30 * 60)  # 30 minutes
def test_esp32s3(dut: Dut, config: str)-> None:
    test(dut, config)


@pytest.mark.target('esp32p4')
@pytest.mark.env('generic,eco4')
@pytest.mark.parametrize(
    'config',
    [
        'defaults',
        # ESP_LOGV() and ESP_LOGD() print a large amount of log output which can cause tests to take too long, so they are not tested
        # 'esp_trace', 'esp_debug',
        'esp_info', 'esp_warning', 'esp_error', 'esp_none',
        'std_trace', 'std_debug', 'std_info', 'std_warning', 'std_error', 'std_none',
    ],
)
@pytest.mark.timeout(30 * 60)  # 30 minutes
def test_esp32p4(dut: Dut, config: str)-> None:
    test(dut, config)
