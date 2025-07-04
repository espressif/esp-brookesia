# SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: CC0-1.0
import pytest
from pytest_embedded import Dut


@pytest.mark.esp32
@pytest.mark.esp32c3
@pytest.mark.esp32s3
@pytest.mark.timeout(8000)
@pytest.mark.ESP_GMF_ELEMENT
@pytest.mark.ESP_GMF_TASK
@pytest.mark.ESP_GMF_IO
@pytest.mark.ESP_GMF_RINGBUF
@pytest.mark.ESP_GMF_PBUF
@pytest.mark.ESP_GMF_BLOCK
@pytest.mark.ELEMENT_POOL
@pytest.mark.ELEMENT_PORT
@pytest.mark.ESP_GMF_METHOD
@pytest.mark.parametrize(
    'config',
    [
        'default',
    ],
    indirect=True,
)
def test_gmf_core(dut: Dut) -> None:
    dut.run_all_single_board_cases(timeout=2000)
