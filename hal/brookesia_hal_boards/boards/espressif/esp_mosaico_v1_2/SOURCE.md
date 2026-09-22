# Source provenance

This board definition selects ESP-Mosaico V1.2 explicitly through Board Manager.
It does not read eFuse data to choose between board revisions.

The V1.2 pin assignments and power dependencies were checked against the
user-provided V1.2 core-board schematic and the public
[ESP-Mosaico BSP](https://github.com/esp-mosaico/esp-mosaico-bsp/tree/4a6c22b5e4d2f66621e7124883b8dc3644180ce7)
at commit `4a6c22b5e4d2f66621e7124883b8dc3644180ce7`, particularly:

- `components/esp-mosaico-bsp/include/bsp/esp_mosaico.h`
- `components/esp-mosaico-bsp/onboard/esp_mosaico.c`
- `components/esp-mosaico-bsp/onboard/display.c`
- `components/esp-mosaico-bsp/onboard/audio.c`
- `components/esp-mosaico-bsp/onboard/subboard.c`
- `components/mosaico_module_camera/mosaico_camera.c`

The supplied schematic is not distributed with this component.

## Revision-specific configuration

The onboard codec, touch controller and fuel gauge use I2C0 on GPIO56/GPIO3.
The expansion EEPROM scanner and camera SCCB use I2C1 on GPIO0/GPIO1.
The LCD clock and reset use GPIO42 and GPIO44, respectively. The old GPIO56
codec-power device is omitted. The audio codecs depend on `audio_codec_ready`,
which holds a board-power dependency and retains the BSP's 10 ms settling delay
without configuring GPIO56 or allocating a driver handle.
The `audio_codec_ready` registration in `board_devices.cpp` selects
`esp_mosaico_audio_ready_init/deinit` from the shared `audio_codec.c`; V1.0 uses
that implementation file's separate power-enable API.

The CST9220 touch factory uses the published
[`espressif/esp_lcd_touch_cst9220` 0.1.0](https://components.espressif.com/components/espressif/esp_lcd_touch_cst9220/versions/0.1.0/readme)
driver. Its panel-IO configuration matches the driver's public configuration
macro, including 8-bit commands and parameters and a disabled control phase.
Board Manager receives the 8-bit address `0xB4` and passes the 7-bit address
`0x5A` to the I2C driver. Touch protocol detection and coordinate normalization
remain in the driver.

`setup_device.c` selects the CST9220 touch factory and passes the V1.2 LCD clock
GPIO42 to the shared panel factory. The shared
`brookesia_hal_custom/src/board/display.c` retains the CO5300 initialization
sequence and drive-strength behavior from the earlier Mosaico integration;
the original sequence provenance remains documented in that source file.

## Shared implementation

This directory retains revision-specific `setup_device.c`, `board_devices.cpp`,
the three board YAML files, and `sdkconfig.defaults.board`. Device registration
translates this board's generated configuration into explicit shared-device
parameters. The lifecycle implementations reside in
`brookesia_hal_custom/src/board/`, with interfaces under
`include/brookesia/hal_custom/board/`.

The `board_devices.yaml` dependency overrides select `esp_lcd_co5300`,
`brookesia_hal_custom`, and `esp_mosaico_usb_console` from
`../../../components/mosaico/`. Board power, the read-only BQ27220 bridge, NAND
BDL ownership, expansion discovery and lifetime handling, camera-slot claims,
display initialization and brightness, and the USB console are shared there.
Neither Mosaico revision includes source files from the other. See the
[shared source provenance](../../../components/mosaico/SOURCE.md).

V1.2 enables the CDC
console but disables automatic download by default: on the validation sample,
the host reset request disconnected USB without re-enumerating the ROM loader.
Use BOOT and a power cycle to enter download mode; the board defaults also
select `ESPTOOLPY_BEFORE_NORESET` to preserve that mode when flashing.

No source is downloaded from the ESP-Mosaico BSP during configuration or build.
Board Manager remains on the existing 0.5.x constraint. The existing HAL
lifecycle facade and six dependency patches remain responsible for the same
cleanup, capture-format and ESP-IDF compatibility behavior; this board adds no
new dependency patch and does not change those contracts.

## Scope and validation

The configuration enables the existing display/touch, audio, Wi-Fi, battery
voltage, storage, USB console and supported expansion-camera paths. NAND FATFS
access remains separate from adding NAND as an application install location.
IMU, haptics, and other modules without an existing upper-layer service are not
added by this board definition.

Hardware tracing on V1.2 confirmed that LCD and NAND each held an AXI GDMA RX
channel. After a working Camera preview closed, App Store HTTPS activity
allocated and retained the remaining RX channel through the shared hardware
SHA/AES driver; reopening Camera then failed to allocate its DVP RX channel.
The V1.2 defaults set `CONFIG_MBEDTLS_HARDWARE_SHA=n` and
`CONFIG_MBEDTLS_HARDWARE_AES=n` so the mbedTLS/PSA SHA/AES used by HTTP/TLS runs
in software. Existing V1.2 `sdkconfig` files must also disable both options.
This retains TLS and signature verification and may increase CPU work; the
overhead has not been measured. These options do not disable direct hardware
crypto calls: Media SAL AES-CBC calls `esp_aes_crypt_cbc` directly and is not
controlled by either option. Encrypted-media coexistence remains unverified.
The user manually confirmed normal Camera application preview with the fixed
firmware. This visual confirmation is separate from the HAL's 50-cycle capture
check; the final serial capture did not record this interaction. A repeat after
an actual application download and installation has not been separately
recorded. This does not extend validation to OV3640, encrypted media, or camera
performance.

Pin and API checks are static evidence. Build success does not establish
physical display/touch timing, USB enumeration, battery readings, NAND
persistence or expansion-camera operation on a V1.2 board; those require board
validation.

The hardware observations above were recorded before this source-layout
refactor. They do not establish build or hardware validation of the reorganized
layout.
