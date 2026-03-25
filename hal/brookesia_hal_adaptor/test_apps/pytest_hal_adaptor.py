# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

# '''
# Steps to run these test cases:
#
# - Build
#   - . ${IDF_PATH}/export.sh
#   - export IDF_CI_BUILD=y
#   - pip install idf_build_apps
#   - python .gitlab/tools/build_apps.py hal/brookesia_hal_adaptor/test_apps -t esp32s3 --config "sdkconfig.ci.board.esp_vocat_board_v1_2=esp_vocat_board_v1_2"
#   - python .gitlab/tools/build_apps.py hal/brookesia_hal_adaptor/test_apps -t esp32p4 --config "sdkconfig.ci.board.esp32_p4_function_ev=esp32_p4_function_ev"
#
# - Test
#   - ${IDF_PATH}/install.sh --enable-pytest
#   - ${IDF_PATH}/install.sh --enable-test-specific
#   - pytest hal/brookesia_hal_adaptor/test_apps --target esp32s3 --env esp_vocat
#   - pytest hal/brookesia_hal_adaptor/test_apps --target esp32p4 --env generic,eco4,esp32p4_function_ev_board
# '''

import json
import os
import re
import time
from pathlib import Path

import pytest
from pytest_embedded import Dut

SUCCESS_RESPONSE = b'0 Failures'
FAILURE_RESPONSE = re.compile(rb'([1-9]\d*)\s+Failures|\bFAIL\b')
LEAK_MEMORY_RESPONSE = b'The test leaked too much memory'
ENTER_RESPONSE_LIST = [
    re.compile(rb'Enter test for running\.?'),
    re.compile(rb'Press ENTER to see (?:the )?list of tests\.?'),
]
MENU_RESPONSE = b"Enter next test, or 'enter' to see menu"
REBOOT_RESPONSE = b'Rebooting...'
RESPONSE_TIMEOUT_S = 60
SINGLE_TIMEOUT_S = 10 * 60
TOTAL_TIMEOUT_S = 30 * 60
RETRY_LIMIT = 5
MENU_RETRY_LIMIT = 3


def _has_required_flash_artifacts(build_dir: Path) -> bool:
    flasher_args = build_dir / 'flasher_args.json'
    if not flasher_args.is_file():
        return False

    try:
        with flasher_args.open('r', encoding='utf-8') as f:
            args = json.load(f)
    except (OSError, ValueError, json.JSONDecodeError):
        return False

    flash_files = args.get('flash_files', {})
    if not flash_files:
        return False

    return all((build_dir / relative_path).is_file() for relative_path in flash_files.values())


def _is_matching_local_build(build_dir: Path, target: str) -> bool:
    flasher_args = build_dir / 'flasher_args.json'
    config_env = build_dir / 'config.env'
    if not flasher_args.is_file() or not config_env.is_file():
        return False

    try:
        with config_env.open('r', encoding='utf-8') as f:
            config = json.load(f)
    except (OSError, ValueError, json.JSONDecodeError):
        return False

    return config.get('IDF_TARGET') == target and _has_required_flash_artifacts(build_dir)


@pytest.fixture
def app_path(test_file_path: str, target: str) -> str:
    app_dir = Path(os.path.dirname(test_file_path))
    local_build_dir = app_dir / 'build'
    if _is_matching_local_build(local_build_dir, target):
        return str(local_build_dir)

    return str(app_dir)


def get_index_and_name_list(response: bytes):
    index_pattern = rb'\((\d+)\)'
    name_pattern = rb'"([^"]+)"'

    indices = re.findall(index_pattern, response)
    names = re.findall(name_pattern, response)

    result = []
    for i in range(min(len(indices), len(names))):
        result.append((int(indices[i]), names[i].decode('utf-8').strip()))

    return result


def _run_single_case(dut: Dut, num: int, name: str) -> bytes:
    print(f'[{num}] [{name}] Writing: {num}')
    dut.write(f'{num}\n')
    start_time = time.time()

    while True:
        try:
            match = dut.expect(
                [SUCCESS_RESPONSE, LEAK_MEMORY_RESPONSE, FAILURE_RESPONSE, REBOOT_RESPONSE],
                timeout=RESPONSE_TIMEOUT_S,
            )
            response = match.group(0)
            print(f'[{num}] [{name}] Received: {response}')
            time.sleep(0.1)
            return response
        except Exception:
            if time.time() - start_time > SINGLE_TIMEOUT_S:
                raise


def _get_menu_index_and_name_list(dut: Dut):
    # Some targets print the initial Unity prompt before pytest starts waiting on it.
    # Actively press ENTER and keep looking for either the prompt or the post-menu marker.
    for _ in range(MENU_RETRY_LIMIT):
        dut.write('\n')
        response = dut.expect(
            ENTER_RESPONSE_LIST + [MENU_RESPONSE],
            return_what_before_match=True,
            timeout=RESPONSE_TIMEOUT_S,
        )
        index_and_name_list = get_index_and_name_list(response)
        if index_and_name_list:
            return index_and_name_list

    pytest.fail('Failed to fetch Unity test menu list after pressing ENTER.')

def run_test(dut: Dut) -> None:
    index_and_name_list = _get_menu_index_and_name_list(dut)
    print(f'index_and_name_list: {index_and_name_list}')

    failed_name_and_numbers = []
    for num, name in index_and_name_list:
        retries = 0
        success = False
        leaked_memory = False

        while not success and retries < RETRY_LIMIT:
            try:
                response = _run_single_case(dut, num, name)
            except Exception:
                failed_name_and_numbers.append((name, num))
                print(f'The following numbers failed or timed out: {failed_name_and_numbers}')
                pytest.fail(f'[{num}] [{name}] Timeout exceeded {SINGLE_TIMEOUT_S}s.')
                break

            if response == REBOOT_RESPONSE:
                failed_name_and_numbers.append((name, num))
                print(f'The following numbers failed or timed out: {failed_name_and_numbers}')
                pytest.fail(f'[{num}] [{name}] Device rebooted unexpectedly.')

            if response == SUCCESS_RESPONSE:
                leaked_memory = False
                success = True
                print(f'[{num}] [{name}] Passed')
                break

            if response == LEAK_MEMORY_RESPONSE:
                retries += 1
                leaked_memory = True
                print(f'[{num}] [{name}] Leaked memory, retrying...')
                continue

            if FAILURE_RESPONSE.search(response):
                if leaked_memory:
                    leaked_memory = False
                    print(f'[{num}] [{name}] Skip failure report right after memory leak retry')
                    continue
                failed_name_and_numbers.append((name, num))
                print(f'The following numbers failed or timed out: {failed_name_and_numbers}')
                pytest.fail(f'[{num}] [{name}] Failed')

            failed_name_and_numbers.append((name, num))
            print(f'The following numbers failed or timed out: {failed_name_and_numbers}')
            pytest.fail(f'[{num}] [{name}] No valid Unity result found.')

        if retries == RETRY_LIMIT:
            failed_name_and_numbers.append((name, num))
            print(f'[{num}] [{name}] Marked as failed after {RETRY_LIMIT} attempts.')

    if failed_name_and_numbers:
        pytest.fail(f'The following numbers failed or timed out: {failed_name_and_numbers}')


@pytest.mark.target('esp32s3')
@pytest.mark.env('esp_vocat')
@pytest.mark.parametrize('config', ['esp_vocat_board_v1_2'])
@pytest.mark.timeout(TOTAL_TIMEOUT_S)
def test_esp32s3_esp_vocat(dut: Dut) -> None:
    run_test(dut)


@pytest.mark.target('esp32p4')
@pytest.mark.env('generic,eco4,esp32p4_function_ev_board')
@pytest.mark.parametrize('config', ['esp32_p4_function_ev'])
@pytest.mark.timeout(TOTAL_TIMEOUT_S)
def test_esp32p4_function_ev_board(dut: Dut) -> None:
    run_test(dut)
