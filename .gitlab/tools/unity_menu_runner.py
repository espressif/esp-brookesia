# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import os
import re
import time
from typing import Optional

import pytest

SUCCESS_RESPONSE = b'0 Failures'
FAILURE_RESPONSE = b'1 Failures'
LEAK_MEMORY_RESPONSE = b'The test leaked too much memory'
ENTER_RESPONSE_LIST = [
    b'Enter test for running',
    b'Press ENTER to see the list of tests',
]
MENU_END_PATTERN = b'Enter test for running'
RUNNING_RESPONSE = b'Running '
UNITY_TEST_COUNT_PATTERN = rb'Unity test count:\s*(\d+)'
REBOOT_RESPONSE = b'Rebooting...'
RESPONSE_TIMEOUT_S = 60
MENU_RESPONSE_TIMEOUT_S = 120
MENU_POLL_INTERVAL_S = 0.1
# Full Unity menu dumps can deadlock USB-Serial/JTAG when there are many cases.
MENU_DUMP_MAX_TESTS = 50
SINGLE_TIMEOUT_S = 10 * 60
RETRY_LIMIT = 5

# Top-level Unity menu entries: "(1)\t\"Test name\"" (sub-menu lines are indented).
_MENU_ITEM_RE = re.compile(rb'(?:^|\r?\n)\((\d+)\)\t"([^"]+)"')


def _to_bytes(buffer) -> bytes:
    if isinstance(buffer, bytes):
        return buffer
    return buffer.encode('utf-8', errors='replace')


def parse_unity_menu_items(buffer) -> list[tuple[int, str]]:
    items: list[tuple[int, str]] = []
    seen: set[int] = set()
    for match in _MENU_ITEM_RE.finditer(_to_bytes(buffer)):
        index = int(match.group(1))
        if index in seen:
            continue
        seen.add(index)
        items.append((index, match.group(2).decode('utf-8')))
    items.sort(key=lambda item: item[0])
    return items


def _read_menu_buffer(dut, extra: bytes = b'') -> bytes:
    parts = [extra, _to_bytes(dut.pexpect_proc.buffer)]
    logfile = getattr(dut, 'logfile', None)
    if logfile and os.path.isfile(logfile):
        with open(logfile, 'rb') as log_file:
            parts.append(log_file.read())
    return max(parts, key=len)


def _drain_serial(dut) -> bytes:
    serial = getattr(dut, 'serial', None)
    if serial is None:
        return b''

    with serial.disable_redirect_thread():
        return serial.proc.read_all()


def _drain_pexpect_output(dut, drain_s: float = 0.2) -> None:
    end_at = time.time() + drain_s
    while time.time() < end_at:
        try:
            chunk = dut.pexpect_proc.read_nonblocking(size=1024, timeout=0.02)
            if not chunk:
                break
        except Exception:
            break


def wait_for_unity_menu(dut, timeout: float = MENU_RESPONSE_TIMEOUT_S) -> list[tuple[int, str]]:
    """
    Request the Unity menu and parse test items while serial output is still arriving.

    Prints the number of parsed menu items in real time. Returns the full list when the
    end prompt "Enter test for running" is observed.
    """
    if hasattr(dut.pexpect_proc, 'maxread'):
        dut.pexpect_proc.maxread = 131072

    dut.write('\n\n')

    deadline = time.time() + timeout
    last_reported = 0
    last_count = 0
    stalled_for = 0.0
    extra_buffer = b''

    while time.time() < deadline:
        try:
            dut.expect([MENU_END_PATTERN], timeout=MENU_POLL_INTERVAL_S)
            break
        except Exception:
            pass

        items = parse_unity_menu_items(_read_menu_buffer(dut, extra_buffer))
        if len(items) > last_count:
            stalled_for = 0.0
            last_count = len(items)
        else:
            stalled_for += MENU_POLL_INTERVAL_S
            if stalled_for >= 2.0:
                drained = _drain_serial(dut)
                if drained:
                    extra_buffer += _to_bytes(drained)
                    stalled_for = 0.0

        buffer = _read_menu_buffer(dut, extra_buffer)
        if MENU_END_PATTERN in buffer:
            break

        items = parse_unity_menu_items(buffer)
        if len(items) > last_reported:
            latest_index, latest_name = items[-1]
            print(f'Unity menu: {len(items)} tests received (latest [{latest_index}] {latest_name})')
            last_reported = len(items)

    buffer = _read_menu_buffer(dut, extra_buffer)
    items = parse_unity_menu_items(buffer)
    if MENU_END_PATTERN not in buffer:
        max_index = items[-1][0] if items else 0
        pytest.fail(
            f'Unity menu incomplete after {timeout}s: received {len(items)} tests, '
            f'max index {max_index}, end prompt not found'
        )

    test_count = items[-1][0] if items else 0
    if test_count != len(items):
        pytest.fail(
            f'Unity menu indices are not consecutive: parsed={len(items)}, max index={test_count}'
        )

    print(f'Unity menu complete: {test_count} tests')
    return items


def read_unity_test_count(dut) -> Optional[int]:
    match = re.search(UNITY_TEST_COUNT_PATTERN, _read_menu_buffer(dut))
    if match is None:
        return None
    return int(match.group(1))


def collect_unity_menu(
    dut,
    timeout: float = MENU_RESPONSE_TIMEOUT_S,
    response_timeout_s: float = RESPONSE_TIMEOUT_S,
) -> list[tuple[int, str]]:
    """
    Collect Unity test cases for pytest execution.

    Reads ``Unity test count: N`` from firmware when available (same value as the menu
    would list). For large suites, skips the interactive menu dump and runs by index to
    avoid USB-Serial/JTAG stalls. Smaller suites still use streaming menu parsing so test
    names are available in logs.
    """
    dut.expect(ENTER_RESPONSE_LIST, timeout=response_timeout_s)

    test_count = read_unity_test_count(dut)
    if test_count is not None:
        print(f'Unity test count: {test_count}')

    if test_count is not None and test_count > MENU_DUMP_MAX_TESTS:
        print(
            f'Skipping menu dump ({test_count} tests > {MENU_DUMP_MAX_TESTS}), '
            'running by index'
        )
        return [(index, f'test #{index}') for index in range(1, test_count + 1)]

    if test_count is None:
        return wait_for_unity_menu(dut, timeout=timeout)

    items = wait_for_unity_menu(dut, timeout=timeout)
    if len(items) == test_count:
        return items

    # Menu stalled before the end prompt; keep parsed names and fill the rest by index.
    names_by_index = dict(items)
    print(
        f'Unity menu partial: parsed {len(items)}/{test_count} names, '
        'filling remaining entries by index'
    )
    return [(index, names_by_index.get(index, f'test #{index}')) for index in range(1, test_count + 1)]


def run_unity_menu(
    dut,
    *,
    menu_timeout: float = MENU_RESPONSE_TIMEOUT_S,
    response_timeout_s: float = RESPONSE_TIMEOUT_S,
    single_timeout_s: float = SINGLE_TIMEOUT_S,
) -> None:
    index_and_name_list = collect_unity_menu(
        dut,
        timeout=menu_timeout,
        response_timeout_s=response_timeout_s,
    )
    print(f'index_and_name_list: {len(index_and_name_list)} tests')

    for num, name in index_and_name_list:
        retries = 0
        success = False
        leaked_memory = False

        while not success and retries < RETRY_LIMIT:
            print(f'[{num}] [{name}] Writing: {num}')
            _drain_pexpect_output(dut)
            dut.write(f'{num}\n')
            start_time = time.time()
            try:
                dut.expect([RUNNING_RESPONSE], timeout=response_timeout_s)
            except Exception:
                pytest.fail(f'[{num}] [{name}] Did not start within {response_timeout_s}s.')

            while True:
                try:
                    response = dut.expect(
                        [SUCCESS_RESPONSE, LEAK_MEMORY_RESPONSE, FAILURE_RESPONSE, REBOOT_RESPONSE],
                        timeout=response_timeout_s,
                    ).group(0)
                    print(f'[{num}] [{name}] Received: {response}')
                    time.sleep(0.1)

                    if response == SUCCESS_RESPONSE:
                        leaked_memory = False
                        success = True
                        break
                    if response == LEAK_MEMORY_RESPONSE:
                        retries += 1
                        leaked_memory = True
                        print(f'[{num}] [{name}] Leaked memory, retrying...')
                        break
                    if response == FAILURE_RESPONSE:
                        if leaked_memory:
                            leaked_memory = False
                            success = True
                            print('Skip leaked memory test')
                            break
                        pytest.fail(f'[{num}] [{name}] Failed')
                    if response == REBOOT_RESPONSE:
                        pytest.fail(f'[{num}] [{name}] Device rebooted unexpectedly.')
                except Exception:
                    if time.time() - start_time > single_timeout_s:
                        pytest.fail(f'[{num}] [{name}] Timeout exceeded {single_timeout_s}s.')

        if retries == RETRY_LIMIT:
            pytest.fail(f'[{num}] [{name}] Failed after {RETRY_LIMIT} retries.')
