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

# When the ESP console fails to recognize a command it prints this literal line.
# Most often this happens because the serial input dropped the first character
# (e.g. "svc_call ..." arrives as "vc_call ..."), which is not a real test
# failure. We detect this pattern and retry the exact same command.
UNRECOGNIZED_COMMAND_MARKER = b'Unrecognized command'
MAX_COMMAND_RETRIES = 2  # total attempts = 1 + MAX_COMMAND_RETRIES
RETRY_SETTLE_DELAY_S = 0.3  # quiet period before re-sending the command

# Number of iterations to run the whole test flow. Useful for stress-testing
# startup-time issues (e.g. flaky recorder_open). Override via env var:
#   CI_TEST_LOOP_COUNT=10 pytest examples/service/console ...
# Between iterations the board is hard-reset so each loop exercises a fresh boot.
try:
    LOOP_COUNT = max(1, int(os.getenv('CI_TEST_LOOP_COUNT', '1')))
except ValueError:
    LOOP_COUNT = 1


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
    ('debug_thread', [b'Thread Profiler', b'IDLE0'], 'Print thread profiler info', 4000, 0),
    ('debug_thread -p core -s cpu -d 1000', [b'Thread Profiler', b'IDLE0'], 'Print thread profiler with options', 4000, 0),
]

# RPC service commands following docs/cmd_rpc_cn.md (English: docs/cmd_rpc.md).
# The flow mirrors the "Full Example" section (Device A = server,
# Device B = client), collapsed onto a single device via the lwIP loopback
# interface (127.0.0.1), so it does not require a real WiFi/Ethernet
# connection.
RPC_SERVER_COMMANDS = [
    # --- Server side (Device A) ---
    # Doc: "Step 3. Start the RPC server on port 65500" (default port 65500)
    ('svc_rpc_server start',
     [b'RPC server started successfully'],
     'Start RPC server on default port (65500)', 4000, 0),
    # Doc: "Step 4. Connect all services to the RPC server" -> connect all,
    # then also demo the "-s" selector variant.
    ('svc_rpc_server connect',
     [b'Services connected successfully'],
     'Connect all services to RPC server', 4000, 0),
    ('svc_rpc_server connect -s Wifi,NVS',
     [b'Services connected successfully'],
     'Connect specific services (-s Wifi,NVS)', 4000, 0),

    # --- Client side over loopback (Device B -> 127.0.0.1) ---
    # Doc: "Step 2. Subscribe to a remote service event"
    # -- svc_rpc_subscribe <host> Wifi ScanApInfosUpdated
    ('svc_rpc_subscribe 127.0.0.1 Wifi ScanApInfosUpdated',
     [b'Successfully subscribed!'],
     'Loopback: subscribe Wifi.ScanApInfosUpdated', 8000, 0),
    # Doc: "Step 3. Call a remote service function"
    # -- svc_rpc_call <host> Wifi TriggerScanStart
    ('svc_rpc_call 127.0.0.1 Wifi TriggerScanStart',
     [b'Success!'],
     'Loopback: call Wifi.TriggerScanStart', 10000, 0),
    # Doc: "Call on local host" -- svc_rpc_call 127.0.0.1 NVS List
    ('svc_rpc_call 127.0.0.1 NVS Set {"KeyValuePairs":{"key1":"value1"}}',
     [b'Success!'],
     'Loopback: call NVS.Set via 127.0.0.1', 8000, 0),
    # Doc: "Call with custom port and timeout (10 s)"
    # -- demo the -p / -t flags
    ('svc_rpc_call 127.0.0.1 NVS List -p 65500 -t 10000',
     [b'Success!'],
     'Loopback: call NVS.List with custom -p/-t', 12000, 0),
    # Doc: "Step 4. Unsubscribe from a remote service event"
    ('svc_rpc_unsubscribe 127.0.0.1 Wifi ScanApInfosUpdated',
     [b'Successfully unsubscribed!'],
     'Loopback: unsubscribe Wifi.ScanApInfosUpdated', 8000, 0),

    # --- Server side cleanup ---
    # Doc: "Disconnect specific services" then "Disconnect all services"
    ('svc_rpc_server disconnect -s Wifi,NVS',
     [b'Services disconnected successfully'],
     'Disconnect specific services (-s Wifi,NVS)', 5000, 0),
    ('svc_rpc_server disconnect',
     [b'Services disconnected successfully'],
     'Disconnect all services', 5000, 0),
    # Doc: "Stop the RPC server"
    ('svc_rpc_server stop',
     [b'RPC server stopped successfully'],
     'Stop RPC server', 5000, 0),
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
     [b"Agent do 'Stop' finished"], 'Ops: Stop agent', 10000, 5000),
    ('svc_call AgentManager GetGeneralState',
     [b'Value: Ready'], 'Ops: Get agent general state', 4000, 0),
])

# Step 5 (optional): Play audio
# Timeouts need to cover: HTTP download + full playback of the MP3. The
# reference example.mp3 plays for ~9 s and network pull adds another ~6 s,
# so 15 s was exactly at the edge; bump it to give the async "State: Idle"
# event some room.
TUTORIAL_COMMANDS.extend([
    ('svc_subscribe Audio PlayStateChanged',
     [b'Subscribed successfully'], 'Audio: Subscribe play state changed event', 4000, 0),
    ('svc_call Audio PlayUrl {"Url":"file://spiffs/example.mp3"}',
     [b'State: Playing', b'State: Idle'], 'Audio: Play local audio', 20000, 2000),
    ('svc_call Audio PlayUrl {"Url":"https://dl.espressif.com/AE/esp-brookesia/example.mp3"}',
     [b'State: Playing', b'State: Idle'], 'Audio: Play network audio', 30000, 2000),
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


def _drain_pending_output(dut: Dut, drain_s: float = 0.2) -> None:
    """Consume any already-buffered console output so the next command starts clean.

    Used before a retry after "Unrecognized command" to avoid the echo of the
    previous attempt bleeding into the next one's matcher.
    """
    end_at = time.time() + drain_s
    while time.time() < end_at:
        try:
            chunk = dut.pexpect_proc.read_nonblocking(size=1024, timeout=0.05)
            if not chunk:
                break
        except Exception:
            break


def _is_unrecognized_command_failure(command: str, response: bytes) -> bool:
    """Return True iff the failure is caused by a genuine ``Unrecognized command``.

    Guardrails to avoid false-positive retries:
    * The marker must appear in ``response``.
    * The marker's position must come AFTER the first newline of the response
      (i.e. after the echoed - possibly truncated - command line). This rules
      out cases where ``Unrecognized command`` text merely exists inside the
      original command (e.g. a JSON payload) or inside stale buffer bytes
      sitting in front of our command echo.
    """
    marker_pos = response.find(UNRECOGNIZED_COMMAND_MARKER)
    if marker_pos < 0:
        return False

    # Locate the end of the echoed command line. If we can't find one, be
    # conservative and do NOT treat this as an unrecognized-command failure.
    first_newline = response.find(b'\n')
    if first_newline < 0:
        return False

    if marker_pos <= first_newline:
        return False

    # Extra safety: if the marker text itself is literally part of the command
    # string we sent (e.g. the caller tested error handling), never retry.
    if UNRECOGNIZED_COMMAND_MARKER.decode(errors='ignore') in command:
        return False

    return True


def _execute_command_once(
    dut: Dut,
    command: str,
    expected_response: Optional[List[bytes]],
    timeout_ms: int,
    prompt_timeout_s: int,
) -> Tuple[bool, bytes]:
    """Send the command once and collect the response.

    Returns:
        (success, accumulated_response_bytes). ``success`` means all expected
        responses were observed (or, when ``expected_response`` is None, no
        error indicator was found). The returned bytes always contain the full
        captured output so the caller can inspect it for retry heuristics.
    """
    timeout_s = max(timeout_ms, 0) / 1000.0

    # Drain anything left in pexpect's buffer BEFORE we send so the response
    # only contains bytes that were actually emitted as a result of this
    # command. This is critical for the "Unrecognized command" retry
    # heuristic: otherwise stale bytes from a prior failure could leak in and
    # trigger a false-positive retry.
    _drain_pending_output(dut)

    dut.write(f"{command}\n")

    # Wait for the command to complete while continuously receiving output
    start_time = time.time()
    accumulated_response = b''

    while (time.time() - start_time) < timeout_s:
        try:
            chunk = dut.pexpect_proc.read_nonblocking(size=1024, timeout=0.1)
            if chunk:
                accumulated_response += chunk
                if expected_response:
                    missing_responses = [
                        item for item in expected_response if item not in accumulated_response
                    ]
                    if not missing_responses:
                        return True, accumulated_response
                # Early-exit: if the device has already reported that it can't
                # recognize this command (almost always a dropped UART char),
                # there is no point in waiting for ``timeout_s`` to elapse -
                # especially for commands with very long timeouts (e.g. the
                # 120 s Activate step). Bail out now so the retry can fire
                # within a second instead of minutes later.
                if _is_unrecognized_command_failure(command, accumulated_response):
                    return False, accumulated_response
        except Exception:
            pass
        time.sleep(0.05)  # small sleep to avoid busy waiting

    prompt = get_prompt(dut.target)
    try:
        final_response = dut.expect(prompt, timeout=prompt_timeout_s, return_what_before_match=True)
        response = accumulated_response + final_response
    except Exception:
        # ``expect()`` failed (often because the command returns the prompt
        # synchronously and the real signal we want is a trailing async
        # event, so no NEW prompt will ever appear). Drain whatever
        # pexpect has buffered so the final check still sees late-arriving
        # lines like "State: Idle".
        tail = b''
        drain_end_at = time.time() + 1.0
        while time.time() < drain_end_at:
            try:
                chunk = dut.pexpect_proc.read_nonblocking(size=1024, timeout=0.1)
                if not chunk:
                    break
                tail += chunk
            except Exception:
                break
        response = accumulated_response + tail

    if expected_response:
        missing_responses = [item for item in expected_response if item not in response]
        if missing_responses:
            return False, response
        return True, response

    if b'Error' in response or b'error' in response or b'failed' in response or b'Failed' in response:
        # Some errors are acceptable (e.g., service not available); the caller
        # handles those cases, we just report raw success/failure here.
        if b'Service not found' in response or b'not initialized' in response:
            return True, response
        return False, response

    return True, response


def execute_command(
    dut: Dut,
    command: str,
    expected_response: Optional[List[bytes]] = None,
    timeout_ms: int = 4000,
    delay_ms: int = 0,
    prompt_timeout_s: int = WAIT_COMMAND_TIMEOUT_S,
    max_retries: int = MAX_COMMAND_RETRIES,
) -> bool:
    """
    Execute a console command and optionally verify the response.

    Automatically retries (up to ``max_retries`` extra attempts) when the
    device responds with ``Unrecognized command``, which is almost always a
    dropped first character on the UART rather than a real failure.

    Args:
        dut: The device under test
        command: The command to execute
        expected_response: Optional list of expected response substrings (all must be present)
        timeout_ms: Max time (ms) to wait for all expected responses while continuously receiving output
        delay_ms: Fixed delay (ms) applied after the command succeeds (0 = no extra wait)
        prompt_timeout_s: Fallback timeout (s) for waiting the console prompt after ``timeout_ms`` elapses
        max_retries: Extra attempts to run the command if ``Unrecognized command`` is observed

    Returns:
        True if command executed successfully, False otherwise
    """
    delay_s = max(delay_ms, 0) / 1000.0

    def _post_success_delay() -> None:
        if delay_s > 0:
            print(f"  ⏱  Post-success delay: {delay_ms} ms")
            time.sleep(delay_s)

    total_attempts = 1 + max(max_retries, 0)
    response = b''
    attempts_used = 0

    try:
        for attempt in range(1, total_attempts + 1):
            attempts_used = attempt
            if attempt == 1:
                print(f"  Executing: {command} (timeout={timeout_ms} ms, delay={delay_ms} ms)")
            else:
                print(
                    f"  🔁 Retrying (attempt {attempt}/{total_attempts}) after "
                    f"'Unrecognized command' on '{command}'"
                )
                # Drain any leftover echo/prompt from the failed attempt so
                # the new attempt's matcher sees clean output only.
                _drain_pending_output(dut)
                time.sleep(RETRY_SETTLE_DELAY_S)

            success, response = _execute_command_once(
                dut, command, expected_response, timeout_ms, prompt_timeout_s
            )

            if success:
                if expected_response:
                    print(f"  ✅ All expected responses found: {expected_response}")
                else:
                    print(f"  ✅ Success")
                _post_success_delay()
                return True

            # Only retry the "Unrecognized command" case; other failures are
            # real and should bubble up to the caller for fail-fast behavior.
            if not _is_unrecognized_command_failure(command, response):
                break

            print(
                f"  ⚠️  Detected '{UNRECOGNIZED_COMMAND_MARKER.decode()}' "
                f"(likely dropped UART char); will retry if attempts remain"
            )

        print(
            f"  ❌ Command failed after {attempts_used}/{total_attempts} attempt(s): {command}"
        )
        # Show both head and tail so we can tell whether late-arriving async
        # events (e.g. "State: Idle" after a long playback) actually reached
        # the response buffer.
        if len(response) <= 400:
            print(f"  Response: {response}")
        else:
            print(f"  Response head: {response[:200]}")
            print(f"  Response tail: {response[-200:]}")
        if expected_response:
            missing = [item for item in expected_response if item not in response]
            print(f"  Missing responses: {missing}")
        return False

    except Exception as e:
        print(f"  ❌ Exception: {e}")
        return False


class CommandFailure(Exception):
    """Raised on the first failing command so callers can fail fast."""

    def __init__(self, group_name: str, index: int, total: int, command: str, description: str):
        super().__init__(
            f"[{group_name}] command {index}/{total} failed: {description} -> {command!r}"
        )
        self.group_name = group_name
        self.index = index
        self.total = total
        self.command = command
        self.description = description


def run_command_group(
    dut: Dut,
    group_name: str,
    commands: List[Tuple[str, Optional[List[bytes]], str, int, int]],
) -> int:
    """
    Run a group of commands, aborting on the first failure.

    Args:
        dut: The device under test
        group_name: Name of the command group
        commands: List of (command, expected_response_list, description, timeout_ms, delay_ms) tuples

    Returns:
        Number of successfully executed commands.

    Raises:
        CommandFailure: If any command in the group fails.
    """
    print(f"\n{'='*80}")
    print(f"Testing {group_name.upper()} commands ({len(commands)} commands)")
    print(f"{'='*80}")

    passed = 0
    total = len(commands)

    for index, (command, expected_response, description, timeout_ms, delay_ms) in enumerate(
        commands, start=1
    ):
        print(
            f"\n[{index}/{total}] {description} "
            f"(timeout: {timeout_ms} ms, delay: {delay_ms} ms)"
        )

        if execute_command(dut, command, expected_response, timeout_ms, delay_ms):
            passed += 1
        else:
            print(f"\n❌ {group_name.upper()} command failed, aborting test (fail-fast).")
            raise CommandFailure(group_name, index, total, command, description)

    print(f"\n{group_name.upper()} Results: {passed} passed, 0 failed")
    return passed


# ============================================================================
# Test Functions
# ============================================================================

def _reset_device_for_next_iteration(dut: Dut) -> None:
    """Hard-reset the DUT and wait for the fresh console prompt."""
    print(f"\n🔄 Hard-resetting DUT for next iteration...")
    try:
        dut.serial.hard_reset()
    except Exception as e:
        # Fallback: try the console 'restart' command if hard_reset is not available
        print(f"  hard_reset() failed ({e}); falling back to 'restart' command")
        try:
            dut.write('restart\n')
        except Exception:
            pass

    prompt = get_prompt(dut.target)
    print(f"  Waiting for console prompt after reset ({prompt.decode()})...")
    wait_for_prompt(dut, timeout=30)
    print("  Console ready after reset!")
    time.sleep(PROBE_DELAY_S)


def _run_command_groups_once(
    dut: Dut,
    test_groups: List[str],
) -> Tuple[int, Dict[str, Dict[str, int]]]:
    """
    Run all configured command groups once.

    Aborts on the first failing command (fail-fast).

    Returns:
        Tuple of (total_passed, per_group_results). Only reached when every
        command succeeded.

    Raises:
        CommandFailure: propagated from :func:`run_command_group`.
    """
    total_passed = 0
    results: Dict[str, Dict[str, int]] = {}

    for group_name in test_groups:
        if group_name not in COMMAND_GROUPS:
            print(f"\n⚠️  Unknown test group: {group_name}")
            continue

        commands = COMMAND_GROUPS[group_name]
        passed = run_command_group(dut, group_name, commands)

        total_passed += passed
        results[group_name] = {'passed': passed, 'failed': 0}

    return total_passed, results


def test_service_console_commands(dut: Dut, test_groups: List[str] = None) -> None:
    """
    Test service console commands.

    Args:
        dut: The device under test
        test_groups: List of test groups to run. If None, run all groups.

    Repeats the full flow ``LOOP_COUNT`` times (default 1). When > 1 the DUT is
    hard-reset between iterations so every iteration starts from a fresh boot.
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

    if LOOP_COUNT > 1:
        print(f"\n🔁 Loop mode enabled: running full flow {LOOP_COUNT} times (fail-fast)")

    grand_total_passed = 0
    per_iteration_results: List[Tuple[int, Dict[str, Dict[str, int]]]] = []

    for iteration in range(1, LOOP_COUNT + 1):
        if LOOP_COUNT > 1:
            print(f"\n{'#' * 80}")
            print(f"# Iteration {iteration}/{LOOP_COUNT}")
            print(f"{'#' * 80}")

        try:
            iter_passed, iter_results = _run_command_groups_once(dut, test_groups)
        except CommandFailure as exc:
            print(f"\n{'='*80}")
            print(f"SUMMARY (aborted at iteration {iteration}/{LOOP_COUNT})")
            print(f"{'='*80}")
            pytest.fail(
                f"Iteration {iteration}/{LOOP_COUNT} failed: {exc}",
                pytrace=False,
            )

        grand_total_passed += iter_passed
        per_iteration_results.append((iteration, iter_results))

        # Hard-reset the board between iterations so the next run starts fresh
        if iteration < LOOP_COUNT:
            _reset_device_for_next_iteration(dut)

    # Print summary (only reached if all iterations passed)
    print(f"\n{'='*80}")
    print(f"SUMMARY")
    print(f"{'='*80}")

    for iteration, results in per_iteration_results:
        header = f"Iteration {iteration}" if LOOP_COUNT > 1 else "Test Results by Group"
        print(f"\n{header}:")
        for group_name, result in results.items():
            print(f"  {group_name:15} ✅ PASS     ({result['passed']} passed, 0 failed)")

    print(f"\nOverall across {LOOP_COUNT} iteration(s): {grand_total_passed} passed, 0 failed")
    print(f"{'='*80}\n")


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
