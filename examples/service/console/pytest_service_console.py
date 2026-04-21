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

3. Build the example (replace `esp32s3` with your target chip: `esp32s3` or `esp32p4`):

   **Build with default configuration:**
   ```bash
   python .gitlab/tools/build_apps.py examples/service/console -t esp32s3 --config "=defaults"
   ```

   **Build for a specific board:**
   ```bash
   # ESP32-S3 example: esp_vocat_board_v1_2
   python .gitlab/tools/build_apps.py examples/service/console -t esp32s3 --config "sdkconfig.ci.board.esp_vocat_board_v1_2=esp_vocat_board_v1_2"

   # ESP32-P4 example: esp32_p4_function_ev
   python .gitlab/tools/build_apps.py examples/service/console -t esp32p4 --config "sdkconfig.ci.board.esp32_p4_function_ev=esp32_p4_function_ev"
   ```

   **Build for all CI boards (recommended for CI):**
   ```bash
   python .gitlab/tools/build_apps.py examples/service/console -t esp32s3 --config "sdkconfig.ci.board.*="
   ```

## Test

1. Install pytest dependencies:
   ```bash
   ${IDF_PATH}/install.sh --enable-pytest
   ${IDF_PATH}/install.sh --enable-test-specific
   ```

2. (Optional) Setup WiFi credentials:
   ```bash
   export CI_TEST_WIFI_2_4G_AP1_SSID="your_wifi_ssid"
   export CI_TEST_WIFI_2_4G_AP1_PSW="your_wifi_password"
   ```

2. Run pytest with appropriate target and environment:

   **ESP32-S3 examples:**
   ```bash
   # Generic environment
   pytest examples/service/console --target esp32s3 --env generic

   # Specific board: esp_vocat_board_v1_2
   pytest examples/service/console --target esp32s3 --env "esp_vocat"
   ```

   **ESP32-P4 examples:**
   ```bash
   # Multiple environments: generic, eco4, esp32p4_function_ev_board
   pytest examples/service/console --target esp32p4 --env "generic,eco4,esp32p4_function_ev_board"
   ```
'''

import pytest
from pytest_embedded import Dut
import time
import os
import json
from typing import List, Tuple, Dict, Optional

PROBE_DELAY_S = 5
WAIT_PROMPT_TIMEOUT_S = 10
WAIT_COMMAND_TIMEOUT_S = 10
DEFAULT_RESPONSE = b'Success!'


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
CI_TEST_WIFI_2_4G_AP1_SSID = os.getenv('CI_TEST_WIFI_2_4G_AP1_SSID', '')
CI_TEST_WIFI_2_4G_AP1_PSW = os.getenv('CI_TEST_WIFI_2_4G_AP1_PSW', '')

# Log WiFi credentials status (without exposing password)
if CI_TEST_WIFI_2_4G_AP1_SSID and CI_TEST_WIFI_2_4G_AP1_PSW:
    print(f"✓ WiFi credentials loaded from environment: SSID='{CI_TEST_WIFI_2_4G_AP1_SSID}'")
else:
    pytest.fail("WiFi credentials not found in environment variables (CI_TEST_WIFI_2_4G_AP1_SSID, CI_TEST_WIFI_2_4G_AP1_PSW)")


# ============================================================================
# Helper Functions for Command Generation
# ============================================================================

def get_wifi_connect_ap_command() -> Optional[str]:
    """
    Generate WiFi connect AP command using environment variables.

    Returns:
        Command string if credentials are available, None otherwise
    """
    if CI_TEST_WIFI_2_4G_AP1_SSID and CI_TEST_WIFI_2_4G_AP1_PSW:
        # Build JSON object with proper escaping and compact format (no spaces)
        # to avoid command line parsing issues
        wifi_config = {
            "SSID": CI_TEST_WIFI_2_4G_AP1_SSID,
            "Password": CI_TEST_WIFI_2_4G_AP1_PSW
        }
        # Use separators=(',', ':') to ensure compact JSON without spaces
        wifi_config_json = json.dumps(wifi_config, separators=(',', ':'))
        return f'svc_call Wifi SetConnectAp {wifi_config_json}'
    return None


# ============================================================================
# Command Test Groups
# ============================================================================

# Basic service manager commands
# Format: (command, expected_response_list, description, timeout_ms, delay_ms)
#   - timeout_ms: max time to wait for all expected responses
#   - delay_ms:   fixed delay applied after the command succeeds (0 = no extra wait)
BASIC_COMMANDS = [
    ('svc_list', [b'=== Registered Services ==='], 'List all services', 4000, 0),
    ('svc_funcs NVS', [b'List', b'Set', b'Get', b'Erase'], 'List NVS functions', 4000, 0),
]

# Debug commands
DEBUG_COMMANDS = [
    ('debug_time_report', [b'Performance Tree Report'], 'Print time profiler report', 4000, 0),
    ('debug_time_clear', [b'All time profiling data has been cleared'], 'Clear time profiler data', 4000, 0),
    ('debug_mem', [b'Memory Profiler'], 'Print memory profiler info', 4000, 0),
    ('debug_thread', [b'Thread Profiler', b'ipc1'], 'Print thread profiler info', 4000, 0),
    ('debug_thread -p core -s cpu -d 1000', [b'Thread Profiler', b'ipc1'], 'Print thread profiler with options', 4000, 0),
]

# RPC service commands (requires WiFi connection)
RPC_SERVER_COMMANDS = [
    ('svc_rpc_server start', [b'RPC server started successfully'], 'Start RPC server', 4000, 0),
    ('svc_rpc_server connect', [b'Services connected successfully'], 'Connect all services to RPC server', 4000, 0),
    ('svc_rpc_server disconnect', [b'Services disconnected successfully'], 'Disconnect all services', 5000, 0),
    ('svc_rpc_server stop', [b'RPC server stopped successfully'], 'Stop RPC server', 5000, 0),
]

# Tutorial commands following docs/tutorial_cn.md:
#   1) Connect WiFi
#   2) Start Expression Emote (load animation assets)
#   3) Start XiaoZhi Agent (subscribe / set target / activate / start)
#   4) Common operation commands during conversation
#   5) (Optional) Play audio
TUTORIAL_COMMANDS: List[Tuple[str, Optional[List[bytes]], str, int, int]] = []

# Step 1: Connect WiFi (requires CI_TEST_WIFI_2_4G_AP1_SSID / PSW env vars)
wifi_connect_cmd = get_wifi_connect_ap_command()
if wifi_connect_cmd:
    TUTORIAL_COMMANDS.extend([
        (wifi_connect_cmd, [DEFAULT_RESPONSE],
         'Step 1.1: Set WiFi AP info (SetConnectAp)', 4000, 0),
        ('svc_call Wifi TriggerGeneralAction {"Action":"Connect"}',
         [b'Detected expected event: Connected'],
         'Step 1.2: Connect to WiFi', 15000, 0),
    ])

# Step 2: Start Expression Emote - load animation assets
TUTORIAL_COMMANDS.append(
    ('svc_call Emote LoadAssetsSource '
     '{"Source":{"source":"anim_icon","type":"PartitionLabel","flag_enable_mmap":false}}',
     [DEFAULT_RESPONSE], 'Step 2: Load Emote animation assets', 10000, 0),
)

# Step 3: Start XiaoZhi Agent
TUTORIAL_COMMANDS.extend([
    ('svc_subscribe AgentXiaoZhi ActivationCodeReceived',
     [b'Subscribed successfully'],
     'Step 3.1: Subscribe AgentXiaoZhi ActivationCodeReceived event', 4000, 0),
    ('svc_call AgentManager SetTargetAgent {"Name":"AgentXiaoZhi"}',
     [DEFAULT_RESPONSE], 'Step 3.2: Set target agent to AgentXiaoZhi', 4000, 0),
    ('svc_call AgentManager TriggerGeneralAction {"Action":"Activate"}',
     [b'No activation code or challenge found, activate successfully'],
     'Step 3.3: Activate agent', 120000, 2000),
    ('svc_call AgentManager TriggerGeneralAction {"Action":"Start"}',
     [b'Speaking status changed to'], 'Step 3.4: Start agent', 20000, 5000),
])

# Step 4: Common operation commands during conversation
TUTORIAL_COMMANDS.extend([
    ('svc_call AgentManager InterruptSpeaking',
     [DEFAULT_RESPONSE], 'Ops: Interrupt agent speaking', 4000, 2000),
    ('svc_call AgentManager TriggerGeneralAction {"Action":"Sleep"}',
     [b"Agent do 'Sleep' finished"], 'Ops: Sleep agent', 4000, 5000),
    ('svc_call AgentManager TriggerGeneralAction {"Action":"WakeUp"}',
     [b"Agent do 'WakeUp' finished"], 'Ops: Wake up agent', 4000, 5000),
    ('svc_call AgentManager TriggerGeneralAction {"Action":"Stop"}',
     [b"Agent do 'Stop' finished", b'Decoder stopped'], 'Ops: Stop agent', 10000, 5000),
    ('svc_call AgentManager GetGeneralState',
     [b'Value: Ready'], 'Ops: Get agent general state', 4000, 0),
])

# Step 5 (optional): Play audio
TUTORIAL_COMMANDS.extend([
    ('svc_subscribe Audio PlayStateChanged',
     [b'Subscribed successfully'], 'Audio: Subscribe play state changed event', 4000, 0),
    ('svc_call Audio PlayUrl {"Url":"file://spiffs/example.mp3"}',
     [b'State: Playing', b'State: Idle'], 'Audio: Play local audio', 10000, 2000),
    ('svc_call Audio PlayUrl {"Url":"https://dl.espressif.com/AE/esp-brookesia/example.mp3"}',
     [b'State: Playing', b'State: Idle'], 'Audio: Play network audio', 10000, 2000),
    ('svc_call Audio SetVolume {"Volume":80}',
     [DEFAULT_RESPONSE], 'Audio: Set volume to 80', 4000, 0),
])

# Command groups mapping
COMMAND_GROUPS = {
    'basic': BASIC_COMMANDS,
    'debug': DEBUG_COMMANDS,
    'tutorial': TUTORIAL_COMMANDS,
    'rpc_server': RPC_SERVER_COMMANDS,
}


# ============================================================================
# Test Helper Functions
# ============================================================================

def wait_for_prompt(dut: Dut, timeout: int = WAIT_PROMPT_TIMEOUT_S) -> None:
    """Wait for the console prompt."""
    prompt = get_prompt(dut.target)
    dut.expect(prompt, timeout=timeout)


def execute_command(
    dut: Dut,
    command: str,
    expected_response: Optional[List[bytes]] = None,
    timeout_ms: int = 4000,
    delay_ms: int = 0,
    prompt_timeout_s: int = WAIT_COMMAND_TIMEOUT_S,
) -> bool:
    """
    Execute a console command and optionally verify the response.

    Args:
        dut: The device under test
        command: The command to execute
        expected_response: Optional list of expected response substrings (all must be present)
        timeout_ms: Max time (ms) to wait for all expected responses while continuously receiving output
        delay_ms: Fixed delay (ms) applied after the command succeeds (0 = no extra wait)
        prompt_timeout_s: Fallback timeout (s) for waiting the console prompt after ``timeout_ms`` elapses

    Returns:
        True if command executed successfully, False otherwise
    """
    timeout_s = max(timeout_ms, 0) / 1000.0
    delay_s = max(delay_ms, 0) / 1000.0

    def _post_success_delay() -> None:
        if delay_s > 0:
            print(f"  ⏱  Post-success delay: {delay_ms} ms")
            time.sleep(delay_s)

    try:
        print(f"  Executing: {command} (timeout={timeout_ms} ms, delay={delay_ms} ms)")
        dut.write(f"{command}\n")

        # Wait for the command to complete while continuously receiving output
        start_time = time.time()
        accumulated_response = b''

        # Continuously read output during the timeout period
        while (time.time() - start_time) < timeout_s:
            try:
                # Try to read any available output with a short timeout
                chunk = dut.pexpect_proc.read_nonblocking(size=1024, timeout=0.1)
                if chunk:
                    accumulated_response += chunk
                    # Verify expected response if provided (all items in list must be present)
                    if expected_response:
                        missing_responses = [
                            item for item in expected_response if item not in accumulated_response
                        ]
                        if not missing_responses:
                            print(f"  ✅ All expected responses found: {expected_response}")
                            _post_success_delay()
                            return True
            except:
                # No data available, continue waiting
                pass
            time.sleep(0.05)  # Small sleep to avoid busy waiting

        # Get the final response and wait for prompt
        prompt = get_prompt(dut.target)
        try:
            final_response = dut.expect(prompt, timeout=prompt_timeout_s, return_what_before_match=True)
            response = accumulated_response + final_response
        except:
            # If prompt not found, use accumulated response
            response = accumulated_response

        # Verify expected response if provided (all items in list must be present)
        if expected_response:
            missing_responses = [item for item in expected_response if item not in response]
            if missing_responses:
                print(f"  ❌ Expected responses not found: {missing_responses}")
                # Print first 500 bytes of response for debugging
                response_preview = response[:100] if len(response) > 100 else response
                print(f"  Response (preview): {response_preview}")
                return False
            print(f"  ✅ All expected responses found: {expected_response}")
            _post_success_delay()
            return True

        # Check for error indicators
        if b'Error' in response or b'error' in response or b'failed' in response or b'Failed' in response:
            print(f"  ⚠️  Command may have failed")
            # Some errors are acceptable (e.g., service not available)
            if b'Service not found' in response or b'not initialized' in response:
                print(f"  ℹ️  Service not available, skipping...")
                _post_success_delay()
                return True  # Skip this command gracefully
            return False

        print(f"  ✅ Success")
        _post_success_delay()
        return True

    except Exception as e:
        print(f"  ❌ Exception: {e}")
        return False


def run_command_group(
    dut: Dut,
    group_name: str,
    commands: List[Tuple[str, Optional[List[bytes]], str, int, int]],
) -> Tuple[int, int]:
    """
    Run a group of commands.

    Args:
        dut: The device under test
        group_name: Name of the command group
        commands: List of (command, expected_response_list, description, timeout_ms, delay_ms) tuples

    Returns:
        Tuple of (passed_count, failed_count)
    """
    print(f"\n{'='*80}")
    print(f"Testing {group_name.upper()} commands ({len(commands)} commands)")
    print(f"{'='*80}")

    passed = 0
    failed = 0

    for command, expected_response, description, timeout_ms, delay_ms in commands:
        print(
            f"\n[{passed + failed + 1}/{len(commands)}] {description} "
            f"(timeout: {timeout_ms} ms, delay: {delay_ms} ms)"
        )

        if execute_command(dut, command, expected_response, timeout_ms, delay_ms):
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
@pytest.mark.env('esp_vocat')
@pytest.mark.parametrize('target, config', [('esp32s3', 'esp_vocat_board_v1_2')])
@pytest.mark.timeout(20 * 60)
def test_esp32s3_esp_vocat_board_v1_2(dut: Dut) -> None:
    """Test all command groups (incl. tutorial flow) on ESP-VoCat Board V1.2."""
    test_service_console_commands(dut)


@pytest.mark.target('esp32p4')
@pytest.mark.env('generic,eco4,esp32p4_function_ev_board')
@pytest.mark.parametrize('target, config', [('esp32p4', 'esp32_p4_function_ev')])
@pytest.mark.timeout(20 * 60)
def test_esp32p4_function_ev_board(dut: Dut) -> None:
    """Test all command groups (incl. tutorial flow) on ESP32-P4 Function EV Board."""
    test_service_console_commands(dut)
