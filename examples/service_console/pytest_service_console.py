# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-4.0

'''
Steps to run these cases (Take `esp32s3` as an example):

- Build
  - . ${IDF_PATH}/export.sh
  - export IDF_CI_BUILD=y
  - pip install idf_build_apps
  - idf-build-apps build -t esp32s3 --manifest-files=".build-rules.yml" --path='./examples/service_console' --recursive --build-dir="@v/build_@t_@w"

- Test
  - ${IDF_PATH}/install.sh --enable-pytest
  - ${IDF_PATH}/install.sh --enable-test-specific
  - pytest examples/service_console --target esp32s3 --env generic
'''

import pytest
from pytest_embedded import Dut
import time
import os
import json
from typing import List, Tuple, Dict, Optional

PROBE_DELAY_S = 5
TIMEOUT_S = 60
DEFAULT_RESPONSE = b'Success!'

WIFI_CONNECT_REPEAT_COUNT = 1


def get_prompt(target: str = 'esp32s3') -> bytes:
    """
    Get the console prompt based on target.

    Args:
        target: Target chip name (e.g., 'esp32s3', 'esp32p4')

    Returns:
        Prompt bytes string
    """
    return f'{target}>'.encode()

# Get WiFi credentials from environment variables
CI_TEST_WIFI_SSID_2_4G = os.getenv('CI_TEST_WIFI_SSID_2_4G', '')
CI_TEST_WIFI_PSW_2_4G = os.getenv('CI_TEST_WIFI_PSW_2_4G', '')

# Log WiFi credentials status (without exposing password)
if CI_TEST_WIFI_SSID_2_4G and CI_TEST_WIFI_PSW_2_4G:
    print(f"✓ WiFi credentials loaded from environment: SSID='{CI_TEST_WIFI_SSID_2_4G}'")
else:
    print("⚠ WiFi credentials not found in environment variables (CI_TEST_WIFI_SSID_2_4G, CI_TEST_WIFI_PSW_2_4G)")
    print("  WiFi connect tests will be skipped")


# ============================================================================
# Helper Functions for Command Generation
# ============================================================================

def get_wifi_connect_ap_command() -> Optional[str]:
    """
    Generate WiFi connect AP command using environment variables.

    Returns:
        Command string if credentials are available, None otherwise
    """
    if CI_TEST_WIFI_SSID_2_4G and CI_TEST_WIFI_PSW_2_4G:
        # Build JSON object with proper escaping and compact format (no spaces)
        # to avoid command line parsing issues
        wifi_config = {
            "SSID": CI_TEST_WIFI_SSID_2_4G,
            "Password": CI_TEST_WIFI_PSW_2_4G
        }
        # Use separators=(',', ':') to ensure compact JSON without spaces
        wifi_config_json = json.dumps(wifi_config, separators=(',', ':'))
        return f'svc_call Wifi SetConnectAp {wifi_config_json}'
    return None


# ============================================================================
# Command Test Groups
# ============================================================================

# Basic service manager commands
# Format: (command, expected_response_list, description, delay_seconds)
BASIC_COMMANDS = [
    ('svc_list', [b'NVS', b'Wifi', b'SNTP'], 'List all services', 4.0),
    ('svc_funcs NVS', [b'List', b'Set', b'Get', b'Erase'], 'List NVS functions', 4.0),
    ('svc_events Wifi', [b'GeneralActionTriggered'], 'List WiFi events', 4.0),
]

# Debug commands
DEBUG_COMMANDS = [
    ('debug_time_report', [b'Performance Tree Report'], 'Print time profiler report', 4.0),
    ('debug_time_clear', [b'All time profiling data has been cleared'], 'Clear time profiler data', 4.0),
    ('debug_mem', [b'Memory Profiler'], 'Print memory profiler info', 4.0),
    ('debug_thread', [b'Thread Profiler', b'ipc1'], 'Print thread profiler info', 4.0),
    ('debug_thread -p core -s cpu -d 1000', [b'Thread Profiler', b'ipc1'], 'Print thread profiler with options', 4.0),
]

# NVS service commands
NVS_COMMANDS = [
    ('svc_call NVS Erase', [DEFAULT_RESPONSE], 'Erase all NVS entries', 4.0),
    ('svc_call NVS List {}', [b'Error'], 'List all NVS entries in default namespace', 4.0),
    ('svc_call NVS Set {"KeyValuePairs":{"test_key1":"test_value1","test_key2":123,"test_key3":true}}', [DEFAULT_RESPONSE], 'Set NVS key-value pairs', 4.0),
    ('svc_call NVS Get {}', [b'test_key1', b'test_value1', b'test_key2', b'123', b'test_key3', b'true'], 'Get all NVS entries', 4.0),
    ('svc_call NVS Get {"Keys":["test_key1","test_key2"]}', [b'test_key1', b'test_value1', b'test_key2', b'123'], 'Get specific NVS keys', 4.0),
    ('svc_call NVS Erase {"Keys":["test_key1","test_key2","test_key3"]}', [DEFAULT_RESPONSE], 'Erase specific NVS keys', 4.0),
    ('svc_call NVS List {}', [b'Error'], 'Verify NVS entries erased', 4.0),
]

# WiFi service commands
WIFI_COMMANDS = [('svc_call Wifi TriggerGeneralAction {"Action":"Start"}', [b'WiFi Start finished'], 'Start WiFi', 10.0)]
wifi_connect_cmd = get_wifi_connect_ap_command()
if wifi_connect_cmd:
    for i in range(WIFI_CONNECT_REPEAT_COUNT):
        WIFI_COMMANDS.extend([
# Test connect to AP
            (wifi_connect_cmd, [DEFAULT_RESPONSE], 'Set WiFi connect AP (from env vars)', 4.0),
            ('svc_call Wifi TriggerGeneralAction {"Action":"Connect"}', [b'WiFi Connect finished'], 'Connect to WiFi', 10.0),
            ('svc_call Wifi GetConnectedAps', [CI_TEST_WIFI_SSID_2_4G.encode()], 'Get connected APs', 4.0),
# Test automatically reconnect to AP when start again
            ('svc_call Wifi TriggerGeneralAction {"Action":"Stop"}', [b'WiFi Stop finished'], 'Stop WiFi', 5.0),
            ('svc_call Wifi TriggerGeneralAction {"Action":"Start"}', [b'WiFi Connect finished'], 'Start WiFi and automatically connect to AP', 20.0),
            ('svc_call Wifi TriggerGeneralAction {"Action":"Disconnect"}', [b'WiFi Disconnect finished'], 'Disconnect from WiFi', 5.0),
        ])
# Test scan
WIFI_COMMANDS.extend([
    ('svc_call Wifi SetScanParams {"ApCount":30,"IntervalMs":1000,"TimeoutMs":20000}', [DEFAULT_RESPONSE], 'Set WiFi scan parameters', 4.0),
    ('svc_call Wifi TriggerScanStart {}', [b'Scanned AP count'], 'Start WiFi scan', 30.0),
    ('svc_call Wifi TriggerScanStop {}', [DEFAULT_RESPONSE], 'Stop WiFi scan', 4.0),
])
if wifi_connect_cmd:
# Test automatically reconnect to AP when scanning
    WIFI_COMMANDS.extend([
        # Set WiFi connect AP again to valid AP which is disconnected before
        (wifi_connect_cmd, [DEFAULT_RESPONSE], 'Set WiFi connect AP to valid AP which is disconnected before', 4.0),
        ('svc_call Wifi TriggerScanStart {}', [b'WiFi Connect finished'], 'Start WiFi scan and automatically connect to AP', 30.0),
        ('svc_call Wifi TriggerScanStop {}', [DEFAULT_RESPONSE], 'Stop WiFi scan', 4.0),
    ])
WIFI_COMMANDS.extend([
    ('svc_call Wifi TriggerGeneralAction {"Action":"Stop"}', [b'WiFi Stop finished'], 'Stop WiFi', 5.0),
    ('svc_call Wifi ResetData', [DEFAULT_RESPONSE], 'Reset WiFi data', 4.0),
])

# SNTP service commands
SNTP_COMMANDS = [
    ('svc_call SNTP SetServers {"Servers":["pool.ntp.org","time.apple.com"]}', [DEFAULT_RESPONSE], 'Set SNTP servers', 4.0),
    ('svc_call SNTP SetTimezone {"Timezone":"CST-8"}', [DEFAULT_RESPONSE], 'Set timezone', 4.0),
    ('svc_call SNTP GetServers', [b'pool.ntp.org', b'time.apple.com'], 'Get SNTP servers', 4.0),
    ('svc_call SNTP GetTimezone', [b'CST-8'], 'Get timezone', 4.0),
    ('svc_call SNTP IsTimeSynced', [DEFAULT_RESPONSE], 'Check if time is synced', 4.0),
    ('svc_call SNTP ResetData', [DEFAULT_RESPONSE], 'Reset SNTP data', 4.0),
]

# Audio service commands (may not be available if board manager is disabled)
AUDIO_COMMANDS = [
    ('svc_call Audio SetVolume {"Volume":80}', [DEFAULT_RESPONSE], 'Set audio volume', 4.0),
]

# Agent service commands (may not be available if board manager is disabled)
AGENT_COMMANDS = [
    ('svc_call Agent GetAgentAttributes', [DEFAULT_RESPONSE], 'Get agent attributes', 4.0),
]

# RPC service commands (requires WiFi connection)
RPC_SERVER_COMMANDS = [
    ('svc_rpc_server start', [DEFAULT_RESPONSE], 'Start RPC server', 4.0),
    ('svc_rpc_server connect', [DEFAULT_RESPONSE], 'Connect all services to RPC server', 4.0),
    ('svc_rpc_server disconnect', [DEFAULT_RESPONSE], 'Disconnect all services', 5.0),
    ('svc_rpc_server stop', [DEFAULT_RESPONSE], 'Stop RPC server', 5.0),
]

# Command groups mapping
COMMAND_GROUPS = {
    'basic': BASIC_COMMANDS,
    'debug': DEBUG_COMMANDS,
    'nvs': NVS_COMMANDS,
    'wifi': WIFI_COMMANDS,
    'sntp': SNTP_COMMANDS,
    # 'audio': AUDIO_COMMANDS,
    # 'agent': AGENT_COMMANDS,
    # 'rpc_server': RPC_SERVER_COMMANDS,
}


# ============================================================================
# Test Helper Functions
# ============================================================================

def wait_for_prompt(dut: Dut, timeout: int = TIMEOUT_S) -> None:
    """Wait for the console prompt."""
    prompt = get_prompt(dut.target)
    dut.expect(prompt, timeout=timeout)


def execute_command(dut: Dut, command: str, expected_response: Optional[List[bytes]] = None, delay: float = 4.0, timeout: int = TIMEOUT_S) -> bool:
    """
    Execute a console command and optionally verify the response.

    Args:
        dut: The device under test
        command: The command to execute
        expected_response: Optional list of expected response substrings (all must be present)
        delay: Delay in seconds after sending command (continuously receiving output during this time)
        timeout: Timeout in seconds

    Returns:
        True if command executed successfully, False otherwise
    """
    try:
        print(f"  Executing: {command}")
        dut.write(f"{command}\n")

        # Wait for the command to complete while continuously receiving output
        start_time = time.time()
        accumulated_response = b''

        # Continuously read output during the delay period
        while (time.time() - start_time) < delay:
            try:
                # Try to read any available output with a short timeout
                chunk = dut.pexpect_proc.read_nonblocking(size=1024, timeout=0.1)
                if chunk:
                    accumulated_response += chunk
                    # Verify expected response if provided (all items in list must be present)
                    if expected_response:
                        missing_responses = []
                        for expected_item in expected_response:
                            if expected_item not in accumulated_response:
                                missing_responses.append(expected_item)

                        if missing_responses == []:
                            print(f"  ✅ All expected responses found: {expected_response}")
                            return True
            except:
                # No data available, continue waiting
                pass
            time.sleep(0.05)  # Small sleep to avoid busy waiting

        # Get the final response and wait for prompt
        prompt = get_prompt(dut.target)
        try:
            final_response = dut.expect(prompt, timeout=timeout, return_what_before_match=True)
            response = accumulated_response + final_response
        except:
            # If prompt not found, use accumulated response
            response = accumulated_response

        # Verify expected response if provided (all items in list must be present)
        if expected_response:
            missing_responses = []
            for expected_item in expected_response:
                if expected_item not in response:
                    missing_responses.append(expected_item)

            if missing_responses:
                print(f"  ❌ Expected responses not found: {missing_responses}")
                # Print first 500 bytes of response for debugging
                response_preview = response[:100] if len(response) > 100 else response
                print(f"  Response (preview): {response_preview}")
                return False
            else:
                print(f"  ✅ All expected responses found: {expected_response}")
                return True

        # Check for error indicators
        if b'Error' in response or b'error' in response or b'failed' in response or b'Failed' in response:
            print(f"  ⚠️  Command may have failed")
            # Some errors are acceptable (e.g., service not available)
            if b'Service not found' in response or b'not initialized' in response:
                print(f"  ℹ️  Service not available, skipping...")
                return True  # Skip this command gracefully
            return False

        print(f"  ✅ Success")
        return True

    except Exception as e:
        print(f"  ❌ Exception: {e}")
        return False


def run_command_group(dut: Dut, group_name: str, commands: List[Tuple[str, Optional[List[bytes]], str, float]]) -> Tuple[int, int]:
    """
    Run a group of commands.

    Args:
        dut: The device under test
        group_name: Name of the command group
        commands: List of (command, expected_response_list, description, delay) tuples

    Returns:
        Tuple of (passed_count, failed_count)
    """
    print(f"\n{'='*80}")
    print(f"Testing {group_name.upper()} commands ({len(commands)} commands)")
    print(f"{'='*80}")

    passed = 0
    failed = 0

    for command, expected_response, description, delay in commands:
        print(f"\n[{passed + failed + 1}/{len(commands)}] {description} (delay: {delay}s)")

        if execute_command(dut, command, expected_response, delay):
            passed += 1
        else:
            failed += 1
            print(f"  ⚠️  Command failed but continuing...")

    print(f"\n{group_name.upper()} Results: {passed} passed, {failed} failed")
    return passed, failed


# ============================================================================
# Test Functions
# ============================================================================

def test_service_console_commands(dut: Dut, test_groups: List[str] = None) -> None:
    """
    Test service console commands.

    Args:
        dut: The device under test
        test_groups: List of test groups to run. If None, run all groups.
    """
    # Wait for the console to be ready
    prompt = get_prompt(dut.target)
    print(f"Waiting for console prompt ({prompt.decode()})...")
    wait_for_prompt(dut, timeout=30)
    print("Console ready!")

    time.sleep(PROBE_DELAY_S)

    # Determine which groups to test
    if test_groups is None:
        test_groups = list(COMMAND_GROUPS.keys())

    total_passed = 0
    total_failed = 0
    results = {}

    # Run each test group
    for group_name in test_groups:
        if group_name not in COMMAND_GROUPS:
            print(f"\n⚠️  Unknown test group: {group_name}")
            continue

        commands = COMMAND_GROUPS[group_name]
        passed, failed = run_command_group(dut, group_name, commands)

        total_passed += passed
        total_failed += failed
        results[group_name] = {'passed': passed, 'failed': failed}

    # Print summary
    print(f"\n{'='*80}")
    print(f"SUMMARY")
    print(f"{'='*80}")
    print(f"\nTest Results by Group:")
    for group_name, result in results.items():
        status = "✅ PASS" if result['failed'] == 0 else "⚠️  PARTIAL"
        print(f"  {group_name:15} {status:10} ({result['passed']} passed, {result['failed']} failed)")

    print(f"\nOverall: {total_passed} passed, {total_failed} failed")
    print(f"{'='*80}\n")

    # Fail the test if any commands failed
    if total_failed > 0:
        pytest.fail(f"Test failed: {total_failed} commands failed out of {total_passed + total_failed}")


# ============================================================================
# Pytest Test Cases
# ============================================================================

@pytest.mark.target('esp32s3')
@pytest.mark.env('generic')
@pytest.mark.parametrize('config', ['defaults'])
@pytest.mark.timeout(20 * 60)
def test_esp32s3_all(dut: Dut) -> None:
    """Test all command groups on ESP32-S3."""
    test_service_console_commands(dut)


@pytest.mark.target('esp32p4')
@pytest.mark.env('generic,eco4,esp32p4_function_ev_board')
@pytest.mark.parametrize('config', ['defaults'])
@pytest.mark.timeout(20 * 60)
def test_esp32p4_all(dut: Dut) -> None:
    """Test all command groups on ESP32-P4."""
    test_service_console_commands(dut)
