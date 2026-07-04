# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut

from unity_menu_runner import run_unity_menu


@pytest.mark.target('esp32s3')
@pytest.mark.env('esp_vocat')
@pytest.mark.parametrize('target, config', [('esp32s3', 'esp_vocat_board_v1_2')])
@pytest.mark.timeout(30 * 60)
def test_esp_vocat(dut: Dut) -> None:
    run_unity_menu(dut)


@pytest.mark.target('esp32p4')
@pytest.mark.env('esp32p4x_function_ev_board')
@pytest.mark.parametrize('target, config', [('esp32p4', 'esp32_p4x_function_ev')])
@pytest.mark.timeout(30 * 60)
def test_esp32_p4x_function_ev(dut: Dut) -> None:
    run_unity_menu(dut)
