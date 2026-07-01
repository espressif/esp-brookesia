# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut

from unity_menu_runner import run_unity_menu


@pytest.mark.target('esp32s3')
@pytest.mark.env('generic,octal-psram')
@pytest.mark.parametrize('target, config', [('esp32s3', 'defaults'), ('esp32s3', 'esp32s3_psram_xip')])
@pytest.mark.timeout(30 * 60)
def test_esp32s3(dut: Dut) -> None:
    run_unity_menu(dut)


@pytest.mark.target('esp32p4')
@pytest.mark.env('jtag,esp32p4_rev3')
@pytest.mark.parametrize('target, config', [('esp32p4', 'defaults')])
@pytest.mark.timeout(30 * 60)
def test_esp32p4(dut: Dut) -> None:
    run_unity_menu(dut)
