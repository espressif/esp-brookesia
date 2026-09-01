#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""Apply the ESP32-S31 native-USB post-flash reset workaround."""

import functools
import sys


LP_SYSTEM_SYS_CTRL_REG = 0x20700008
LP_SYSTEM_FORCE_DOWNLOAD_BOOT_MASK = 1 << 2


def _patch_esp32s31_hard_reset() -> None:
    from esptool.targets.esp32s31 import ESP32S31ROM

    original_hard_reset = ESP32S31ROM.hard_reset

    @functools.wraps(original_hard_reset)
    def hard_reset_with_force_download_clear(self) -> None:
        if self.uses_usb_otg():
            try:
                self.write_reg(
                    LP_SYSTEM_SYS_CTRL_REG,
                    0,
                    LP_SYSTEM_FORCE_DOWNLOAD_BOOT_MASK,
                )
            except Exception:
                # The port can disappear while the target is resetting. Keep
                # esptool's original reset behavior in that case.
                pass
        original_hard_reset(self)

    ESP32S31ROM.hard_reset = hard_reset_with_force_download_clear


def main() -> None:
    if len(sys.argv) < 2 or sys.argv[1] != "esptool":
        raise SystemExit("expected 'esptool' as the first argument")

    del sys.argv[1]

    import esptool

    _patch_esp32s31_hard_reset()
    esptool._main()


if __name__ == "__main__":
    main()
