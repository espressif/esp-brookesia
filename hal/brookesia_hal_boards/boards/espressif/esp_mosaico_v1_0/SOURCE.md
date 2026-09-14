# Source provenance

The hardware assignments and initialization behavior for board power
(`board_power.c`), audio codec power (`audio_codec_power.c`), the CO5300
display (`setup_device.c`), and SPI NAND (`fs_nand.c`) are adapted from the
ESP-Mosaico BSP at commit `bef99672e411101489ed19c40527cca1c1dd5bb1`:

- Repository: [https://github.com/esp-mosaico/esp-mosaico-bsp](https://github.com/esp-mosaico/esp-mosaico-bsp)
- Source area: `components/esp-mosaico-bsp`

The code was reorganized around ESP Board Manager device dependencies,
reference-counted peripheral handles, and reverse-order failure cleanup.

The Mosaico expansion-module EEPROM descriptor and discovery behavior, along
with the OV3640 camera slot mapping and initialization parameters, are adapted
from the following areas of the same Apache-2.0-licensed BSP revision:

- `components/mosaico_module_mgr`
- `components/mosaico_module_camera`
- `components/esp-mosaico-bsp/onboard/subboard.c`

Only the Mosaico provider uses this BSP-derived hardware knowledge. The generic
ESP-Brookesia expansion-module contract and runtime are new code and contain no
Mosaico pin, EEPROM, or camera assumptions. All required implementation code
is kept in this repository; neither configuration nor build downloads or links
against the ESP-Mosaico BSP repository.

The BQ27220 implementation is a new read-only Board Manager bridge using only
the standard BatteryStatus and Voltage registers; it does not migrate the BSP
fuel-gauge configuration or sealing logic.

The local `esp_mosaico_usb_console` component uses the public ESP TinyUSB CDC
APIs available in the selected ESP-IDF environment. Its DTR/RTS restart
behavior and 50 ms restart delay follow the Apache-2.0-licensed ESP-IDF ROM CDC
console implementation. ESP32-S31 does not currently expose the equivalent ROM
CDC console or a public restart-to-download API, so the S31 force-download bit
is isolated inside this board-only component.

USB hardware routing was checked against the public
[ESP-Mosaico CoreBoard V1.0 schematic](https://dl.espressif.com/AE/SCH_SCH_ESP-Mosaico_CoreBoard_V1_0_2026-08-18.pdf)
and the [ESP-Mosaico user guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp-mosaico/user_guide.html).
The onboard Type-C connector uses the ESP32-S31 dedicated USB 2.0 High-Speed
PHY pins. Those package pin numbers are separate from GPIO44 and GPIO45, so USB
does not conflict with the LCD clock or the audio PA control signals.

## Dependency workarounds

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

The shared lifecycle lock, actual video-node detection, asynchronous expansion
notifications, and pending board cleanup owners are Brookesia code. The former
Board Manager lock and DVP detection patches are removed. Probe logging patches
are also removed; discovery never changes a global log level. Unknown release
results isolate affected hardware until manual restart. Known-live child-device
cleanup may be retried before a later open. See the bilingual adaptation guide
in `docs/{en,zh_CN}/hal/boards/espressif.rst` for event and recovery contracts.
