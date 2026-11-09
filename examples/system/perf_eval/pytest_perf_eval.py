# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
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

3. Build the carrier:
   ```bash
   python .gitlab/tools/build_apps.py examples/system/perf_eval -t esp32p4 --config "sdkconfig.ci.board.esp32_p4_function_ev"
   ```

   The board config has to be passed explicitly. Building bare would skip it, and the
   carrier then boots with a different display pipeline than the one measured.

## Test

1. Install pytest dependencies:
   ```bash
   ${IDF_PATH}/install.sh --enable-ci
   ${IDF_PATH}/install.sh --enable-test-specific
   ```

2. Run pytest:
   ```bash
   pytest examples/system/perf_eval --target esp32p4 --env esp32_p4_function_ev_board
   ```

## What is asserted

The checks below assert the section 3.1 object gate and then wait for the
completion marker, which doubles as the no-panic/reboot guard. The full bench
takes about 1740 s, so the pytest timeout covers it; the numbers themselves are
produced by brookesia-gui-perf from a capture of the whole run:

```bash
PYTHONPATH=tools/brookesia_gui_perf/src python3 -m brookesia_gui_perf.cli \
    doc-load <capture.log> -o profile.json
```
'''

import pytest
from pytest_embedded import Dut

# The full bench is ~1740 s (all scenarios incl. the 300-sample steady-state loop);
# the pytest timeout must cover it, while early boot/mount probes only need a short
# bound so a dead device fails fast instead of burning the whole budget.
EXPECT_TIMEOUT_S = 5 * 60
BENCH_TIMEOUT_S = 45 * 60

# v1.8 section 3.1: the Golden Case has to clear this on the runtime object count,
# which only exists once screens are mounted. Kept as a named threshold so a failure
# names the gate rather than just a number.
SECTION_3_1_OBJECT_GATE = 600
GOLDEN_CASE_SCREENS = 24

COMPLETION_MARKER = '"record":"completed"'


def run_test(dut: Dut) -> None:
    # The document tree is embedded, so nothing here depends on a mounted host
    # filesystem. It is staged into the internal LittleFS volume because the asset
    # layer resolves relative paths through the Storage service.
    dut.expect(r'PerfEval: materialised \d+/\d+ embedded files', timeout=EXPECT_TIMEOUT_S)

    # Every JSON asset is part of the document model, so a partial staging means the
    # Golden Case was not really reproduced (the p4x run silently skipped 144 files).
    json_line = dut.expect(r'PerfEval: materialised json (\d+)/(\d+)', timeout=EXPECT_TIMEOUT_S)
    assert json_line.group(1) == json_line.group(2), (
        'Golden Case JSON staging incomplete: '
        f'{json_line.group(1)}/{json_line.group(2)}'
    )

    # section 1.3 probes. Without these lines doc-load has nothing to segment.
    dut.expect(r'profile: .*stage\(parse_root_json\)', timeout=EXPECT_TIMEOUT_S)

    mounted = dut.expect(
        r'PerfEval: mounted (\d+) screens over the whole Golden Case \((\d+) failed\)',
        timeout=EXPECT_TIMEOUT_S,
    )
    screens = int(mounted.group(1))
    failed = int(mounted.group(2))

    # A screen id is not a file stem, so this used to be 10 of 11 with
    # /settings_header failing. A partial mount would understate the object count and
    # still look healthy, so it is a hard failure rather than a warning. The count is
    # pinned to the Golden Case (5 documents / 24 screens): a smaller number means the
    # carrier stopped enumerating documents, not that the UI shrank.
    assert failed == 0, 'screens failed to mount, object count would be understated'
    assert screens == GOLDEN_CASE_SCREENS, (
        f'expected {GOLDEN_CASE_SCREENS} screens from the Golden Case, got {screens}'
    )

    objects = dut.expect(r'runtime LVGL objects=(\d+)', timeout=EXPECT_TIMEOUT_S)
    object_count = int(objects.group(1))
    assert object_count >= SECTION_3_1_OBJECT_GATE, (
        'section 3.1 runtime object gate not met: '
        f'{object_count} < {SECTION_3_1_OBJECT_GATE}'
    )

    # Doubles as the no-panic assertion. Every pattern above is replayed on each boot,
    # so matching them proves nothing on its own: the failure this guards against was a
    # reboot loop. This marker is logged only after the load, the mount and the full
    # steady-state loop, so a device that is panicking never reaches it and the expect
    # times out instead.
    dut.expect(COMPLETION_MARKER, timeout=BENCH_TIMEOUT_S)


@pytest.mark.target('esp32p4')
@pytest.mark.env('esp32_p4_function_ev_board')
@pytest.mark.parametrize(
    'target, config',
    [
        ('esp32p4', 'esp32_p4_function_ev'),
    ],
)
@pytest.mark.timeout(BENCH_TIMEOUT_S)
def test_esp32_p4_function_ev(dut: Dut) -> None:
    run_test(dut)


@pytest.mark.target('esp32p4')
@pytest.mark.env('esp32p4x_function_ev_board')
@pytest.mark.parametrize(
    'target, config',
    [
        ('esp32p4', 'esp32_p4x_function_ev'),
    ],
)
@pytest.mark.timeout(BENCH_TIMEOUT_S)
def test_esp32_p4x_function_ev(dut: Dut) -> None:
    run_test(dut)
