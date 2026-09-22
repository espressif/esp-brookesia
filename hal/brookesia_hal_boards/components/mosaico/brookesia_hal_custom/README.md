# Mosaico HAL Custom

* [中文版本](README_CN.md)

## Overview

`brookesia_hal_custom` contains the Mosaico-specific glue shared by the
ESP32-S31 ESP-Mosaico V1.0 and V1.2 board definitions. Despite its general
component name, its device and expansion implementations are scoped to
Mosaico hardware.

The selected board's YAML dependency overrides enable this component.
`board_devices.cpp` in that board directory registers Board Manager devices
and converts generated configuration into explicit shared-device parameters.
Revision selection, pin assignments, device dependencies, touch factories, and
SDK defaults remain in the corresponding board directory.

Both boards use `src/board/audio_codec.c`: V1.0 registers its power-enable API,
while V1.2 registers its readiness API without a dedicated codec GPIO. Each
board's `setup_device.c` retains its touch factory and passes the LCD clock GPIO
to the shared panel factory in `src/board/display.c`.

Cleanup callbacks are registered automatically by the board adaptor; applications do not need additional registration.

## Source Layout and Boundaries

| Location | Responsibility |
| --- | --- |
| `include/brookesia/hal_custom/board/` | Shared device parameters and lifecycle entry points, independent of generated board types |
| `src/board/` | Board and codec power sequencing, CO5300 initialization, BQ27220 queries, NAND ownership, and expansion lifetime and camera-slot handling |
| [src/board/device_cleanup.cpp](src/board/device_cleanup.cpp) | Startup registration of device cleanup callbacks |
| `src/display/` | CO5300 backlight integration with the display HAL |
| `src/expansion/` | Mosaico EEPROM discovery and the provider bridge to the expansion runtime |

Generic HAL contracts and the shared lifecycle runtime remain in
`brookesia_hal_interface` and `brookesia_hal_adaptor`. This component does not
select a revision through eFuse or include sources from either board directory.
The CO5300 panel driver and USB CDC console remain separate sibling components.

See the [shared source provenance](../SOURCE.md) for upstream references,
dependency workarounds, and the distinction between earlier hardware evidence
and validation of the reorganized source layout.
