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
   python .gitlab/tools/build_apps.py utils/brookesia_lib_utils/test_apps/log -t esp32s3
   ```

   **Build for all CI targets (recommended for CI):**
   ```bash
   python .gitlab/tools/build_apps.py utils/brookesia_lib_utils/test_apps/log -t all
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
   pytest utils/brookesia_lib_utils/test_apps/log --target esp32s3 --env generic
   ```
'''

import pytest
from pytest_embedded import Dut
import time
import re

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


def run_test(dut: Dut, config: str = 'defaults')-> None:
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

    for num, name in index_and_name_list:
        retries = 0
        success = False
        leaked_memory = False

        while not success and retries < RETRY_LIMIT:
            print(f"[{num}] [{name}] Writing: {num}")
            dut.write(f"{num}\n")
            start_time = time.time()

            # Collect output for log level verification
            captured_output = b''

            while True:
                try:
                    # Capture output before matching the response for log verification
                    captured_output = dut.expect(
                        [b'Enter next test'],
                        timeout=RESPONSE_TIMEOUT_S,
                        return_what_before_match=True
                    )

                    # If this is the log level test, verify the output
                    if num == log_level_test_num:
                        verify_log_level_output(dut, config, captured_output)

                    time.sleep(0.1)

                    if SUCCESS_RESPONSE in captured_output:
                        leaked_memory = False
                        success = True
                        break
                    elif LEAK_MEMORY_RESPONSE in captured_output:
                        retries += 1
                        leaked_memory = True
                        print(f"[{num}] [{name}] Leaked memory, retrying...")
                        break
                    elif FAILURE_RESPONSE in captured_output:
                        if leaked_memory:
                            leaked_memory = False
                            print(f"Skip leaked memory test")
                            continue
                        pytest.fail(f"[{num}] [{name}] Failed")
                        break
                    elif REBOOT_RESPONSE in captured_output:
                        pytest.fail(f"[{num}] [{name}] Device rebooted unexpectedly.")
                        break
                except Exception:
                    if time.time() - start_time > SINGLE_TIMEOUT_S:
                        pytest.fail(f"[{num}] [{name}] Timeout exceeded {SINGLE_TIMEOUT_S}s.")
                        break

        if retries == RETRY_LIMIT:
            pytest.fail(f"[{num}] [{name}] Failed after {RETRY_LIMIT} retries.")


# We only test all log configurations for ESP32-S3; for other chips, only the default configuration is tested.
@pytest.mark.target('esp32s3')
@pytest.mark.env('generic')
@pytest.mark.parametrize(
    'target, config',
    [
        ('esp32s3', 'defaults'),
        # ESP_LOGV() and ESP_LOGD() print a large amount of log output which can cause tests to take too long, so they are not tested
        # 'esp_trace', 'esp_debug',
        ('esp32s3', 'esp_info'), ('esp32s3', 'esp_warning'), ('esp32s3', 'esp_error'), ('esp32s3', 'esp_none'),
        ('esp32s3', 'std_trace'), ('esp32s3', 'std_debug'), ('esp32s3', 'std_info'), ('esp32s3', 'std_warning'),
        ('esp32s3', 'std_error'), ('esp32s3', 'std_none'),
    ],
)
@pytest.mark.timeout(TOTAL_TIMEOUT_S)
def test_esp32s3(dut: Dut, config: str)-> None:
    run_test(dut, config)


# We only test all log configurations for ESP32-S3; for other chips, only the default configuration is tested.
@pytest.mark.target('esp32s3')
@pytest.mark.env('generic')
@pytest.mark.parametrize(
    'target, config',
    [
        ('esp32s3', 'enable_thread_name'),
    ],
)
@pytest.mark.timeout(TOTAL_TIMEOUT_S)
def test_esp32s3_enable_thread_name(dut: Dut)-> None:
    run_test(dut)


@pytest.mark.target('esp32p4')
@pytest.mark.env('generic,eco4')
@pytest.mark.parametrize(
    'target, config',
    [
        ('esp32p4', 'defaults'),
    ],
)
@pytest.mark.timeout(TOTAL_TIMEOUT_S)
def test_esp32p4(dut: Dut, config: str)-> None:
    run_test(dut, config)
