# ChangeLog

## v0.7.1 - 2026-04-14

### Enhancements:

- feat(boards): add manufacturer-specific board folder to support multiple manufacturers

## v0.7.0 - 2026-04-10

### Initial Release

- feat(repo): add `brookesia_hal_boards` component providing board configurations for `esp_board_manager`
- feat(boards): add `esp32_p4_function_ev` board with EK79007 DSI LCD, GT911 touch, ES8311 audio codec, SPIFFS/SD card storage
- feat(boards): add `esp32_s3_korvo2_v3` board with audio codec, SPIFFS storage support
- feat(boards): add `esp_box_3` board with LCD panel, LCD touch, ES8311 audio codec, SPIFFS storage
- feat(boards): add `esp_sensair_shuttle` board with LCD panel, LCD touch, audio codec, SPIFFS/SD card storage
- feat(boards): add `esp_vocat_board_v1_0` board with audio codec, SPIFFS storage support
- feat(boards): add `esp_vocat_board_v1_2` board with audio codec, SPIFFS/SD card storage support
- feat(boards): each board includes `board_info.yaml`, `board_devices.yaml`, `board_peripherals.yaml`, `sdkconfig.defaults.board`, and optional `setup_device.c`
