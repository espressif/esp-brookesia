# Source provenance

This board definition selects ESP-Mosaico V1.0 explicitly through Board Manager.
It does not read eFuse data to choose between board revisions.

The V1.0 hardware assignments and initialization behavior were adapted from the
Apache-2.0-licensed
[ESP-Mosaico BSP](https://github.com/esp-mosaico/esp-mosaico-bsp/tree/bef99672e411101489ed19c40527cca1c1dd5bb1)
at commit `bef99672e411101489ed19c40527cca1c1dd5bb1`, particularly
`components/esp-mosaico-bsp` and its onboard power, audio, display, and NAND code.

USB hardware routing was checked against the public
[ESP-Mosaico CoreBoard V1.0 schematic](https://dl.espressif.com/AE/SCH_SCH_ESP-Mosaico_CoreBoard_V1_0_2026-08-18.pdf)
and the [ESP-Mosaico user guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp-mosaico/user_guide.html).
The onboard Type-C connector uses the ESP32-S31 dedicated USB 2.0 High-Speed
PHY pins. Those package pin numbers are separate from GPIO44 and GPIO45, so USB
does not conflict with the LCD clock or the audio PA control signals.

## Board configuration and shared implementation

This directory retains revision-specific `setup_device.c`, `board_devices.cpp`,
the three board YAML files, and `sdkconfig.defaults.board`. The setup file selects
the CST9217 touch factory and passes the V1.0 LCD clock GPIO44 to the shared
panel factory. Device registration translates the
generated board configuration into explicit parameters for shared lifecycle
functions. The `audio_codec_power` registration uses
`esp_mosaico_audio_power_init/deinit` from the shared `audio_codec.c` for the
V1.0 codec power-enable GPIO.

The board selects `brookesia_hal_custom`, `esp_lcd_co5300`, and
`esp_mosaico_usb_console` from `../../../components/mosaico/`. Common lifecycle,
display, expansion, and USB implementation code is owned by those components;
it no longer resides in this revision directory. Neither Mosaico revision
includes source files from the other. See the
[shared source provenance](../../../components/mosaico/SOURCE.md) for BSP-derived
implementation details, USB behavior, and dependency workarounds.

This source-layout refactor does not change the V1.0 board defaults. Previously
recorded build and hardware results predate the refactor and do not validate
the reorganized layout.
