# Shared Mosaico source provenance

This directory owns the components shared by the explicit ESP-Mosaico V1.0 and
V1.2 board definitions:

- `brookesia_hal_custom`: Mosaico device lifecycle, display, and expansion glue.
- `esp_lcd_co5300`: the vendored CO5300 panel driver; see its
  [source record](esp_lcd_co5300/SOURCE.md) and retained license.
- `esp_mosaico_usb_console`: the board USB CDC console and download integration.

The shared code was extracted from the Mosaico integration recorded in
ESP-Brookesia commit `d79a7ac0`. Each board directory retains its configuration,
setup factories, and device registration. Registration passes explicit
parameters through `brookesia_hal_custom/include/brookesia/hal_custom/board/`
to the implementations in `brookesia_hal_custom/src/board/`; shared code does
not include another board's generated configuration or source files.

The shared `audio_codec.c` exposes `esp_mosaico_audio_power_init/deinit` for a
dedicated codec GPIO and `esp_mosaico_audio_ready_init/deinit` for a codec powered
by the board rail. V1.0 and V1.2 register the respective APIs in their own
`board_devices.cpp`. Their `setup_device.c` files select the touch factory and
pass the LCD clock GPIO to the shared panel factory; the common CO5300 sequence
and drive-strength setup reside in `brookesia_hal_custom/src/board/display.c`.

## BSP-derived implementation

Board power, codec power, the CO5300 initialization sequence and drive strength,
and SPI NAND initialization were adapted from the Apache-2.0-licensed
[ESP-Mosaico BSP](https://github.com/esp-mosaico/esp-mosaico-bsp/tree/bef99672e411101489ed19c40527cca1c1dd5bb1)
at commit `bef99672e411101489ed19c40527cca1c1dd5bb1`, under
`components/esp-mosaico-bsp`. The integration reorganizes this behavior around
Board Manager device dependencies, reference-counted peripheral handles, and
reverse-order failure cleanup.

The expansion-module EEPROM descriptor and discovery behavior, together with
the OV3640 slot mapping and initialization parameters, were adapted from the
following areas of the same BSP revision:

- `components/mosaico_module_mgr`
- `components/mosaico_module_camera`
- `components/esp-mosaico-bsp/onboard/subboard.c`

Only the Mosaico provider uses this BSP-derived hardware knowledge. The generic
ESP-Brookesia expansion contract and runtime contain no Mosaico pin, EEPROM, or
camera assumptions. The BQ27220 code is a new read-only Board Manager bridge
using the standard BatteryStatus and Voltage registers; it does not migrate
the BSP fuel-gauge configuration or sealing logic.

Revision-specific pin assignments and later BSP checks are recorded in the
[V1.0 source record](../../boards/espressif/esp_mosaico_v1_0/SOURCE.md) and
[V1.2 source record](../../boards/espressif/esp_mosaico_v1_2/SOURCE.md).
All required implementation code is kept in this repository; configuration and
build do not download or link against the ESP-Mosaico BSP repository.

## USB console

`esp_mosaico_usb_console` uses the public ESP TinyUSB CDC APIs available in the
selected ESP-IDF environment. Its DTR/RTS restart behavior and 50 ms restart
delay follow the Apache-2.0-licensed ESP-IDF ROM CDC console implementation.
ESP32-S31 does not currently expose the equivalent ROM CDC console or a public
restart-to-download API, so the S31 force-download bit is isolated inside this
board component. Automatic-download policy remains revision-specific.

The console explicitly restores line buffering for stdout/stderr after
redirecting or restoring them. Picolibc's `freopen()` otherwise switches these
streams to full buffering, which can retain short log messages. Reserved UART
output streams receive the same configuration before use as fallbacks. This
behavior applies to both Mosaico revisions.

## Dependency workarounds and validation

The integration keeps Board Manager 0.5.15 and the existing resolved media
versions. Six dependency patches are shipped by `brookesia_hal_adaptor/tools`:

- Board Manager peripheral cleanup isolates unknown release results in private
  runtime nodes; its historical `periph_deinit_retry` filename no longer means
  reference restoration or driver retries.
- Board Manager DVP cleanup reports errors and retains staged ownership.
- `esp_video` DVP teardown checks open descriptors before destroying resources.
- `av_processor` stops its frame-mode capture pipeline.
- `esp_capture` negotiates UYVY as well as YUYV.
- `media_lib_sal` accommodates ESP-IDF 6 TLS fields.

Mosaico registers its NAND, fuel-gauge, and expansion cleanup callbacks through
the HAL Board Manager cleanup registration interface at startup. The common
manager does not name those devices or link weak Mosaico cleanup symbols;
retained-resource ownership and retry decisions remain in their implementations.

The shared lifecycle lock, actual video-node detection, asynchronous expansion
notifications, and pending board cleanup owners remain Brookesia code. The
former Board Manager lock and DVP detection patches, along with probe logging
patches, are removed. Discovery never changes a global log level. Unknown
release results isolate affected hardware until manual restart. Known-live
child-device cleanup may be retried before a later open. See the
[HAL adaptor source record](../../../brookesia_hal_adaptor/SOURCE.md) and the
bilingual board guide in `docs/{en,zh_CN}/hal/boards/espressif.rst` for details.

This layout change adds no dependency patch or device capability. Previously
recorded hardware observations predate the refactor; validation of the
reorganized layout must be recorded separately.
