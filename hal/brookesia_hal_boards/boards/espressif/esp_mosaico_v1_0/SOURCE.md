# Source provenance

The hardware assignments and initialization behavior for board power
(`board_power.c`), audio codec power (`audio_codec_power.c`), the CO5300
display (`setup_device.c`), and SPI NAND (`fs_nand.c`) are adapted from the
ESP-Mosaico BSP at commit `bef99672e411101489ed19c40527cca1c1dd5bb1`:

- Repository: [https://github.com/esp-mosaico/esp-mosaico-bsp](https://github.com/esp-mosaico/esp-mosaico-bsp)
- Source area: `components/esp-mosaico-bsp`

The code was reorganized around ESP Board Manager device dependencies,
reference-counted peripheral handles, and reverse-order failure cleanup.

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
