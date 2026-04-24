# ChangeLog

## v0.7.2 - 2026-04-20

### Enhancements:

- feat(storage): resolve SD card via `ESP_BOARD_DEVICE_NAME_FS_SDCARD` instead of `ESP_BOARD_DEVICE_NAME_FS_FAT`
- feat(kconfig): expose LCD panel horizontal/vertical resolution at the CMake stage

## v0.7.1 - 2026-04-10

### Breaking Changes:

- break(repo): remove monolithic flat source files `src/audio_device.cpp/hpp` and `src/lcd_display_device.cpp/hpp`; source layout reorganized into `src/audio/`, `src/display/`, and `src/storage/` subdirectories
- break(kconfig): remove `BROOKESIA_HAL_ADAPTOR_AUDIO_DEVICE_ENABLE_AUTO_REGISTER` and `BROOKESIA_HAL_ADAPTOR_DISPLAY_DEVICE_ENABLE_AUTO_REGISTER` Kconfig options; replaced by structured `ENABLE_AUDIO_DEVICE` / `ENABLE_DISPLAY_DEVICE` / `ENABLE_STORAGE_DEVICE` menu items

### Enhancements:

- feat(api): add umbrella header `include/brookesia/hal_adaptor.hpp` exposing `AudioDevice`, `DisplayDevice`, and `StorageDevice` public interfaces
- feat(audio): add public `AudioDevice` singleton class (`include/brookesia/hal_adaptor/audio/device.hpp`) with `set_codec_player_info()` and `set_codec_recorder_info()` for runtime capability override
- feat(display): add public `DisplayDevice` singleton class (`include/brookesia/hal_adaptor/display/device.hpp`) with `set_ledc_backlight_info()` for runtime capability override
- feat(storage): add new `StorageDevice` singleton class (`include/brookesia/hal_adaptor/storage/device.hpp`) backed by `GeneralFsImpl` supporting SPIFFS and SD card (FATFS)
- feat(audio): volume API now maps external [0, 100] range to hardware [volume_min, volume_max] range; add Kconfig options `CODEC_PLAYER_VOLUME_DEFAULT/MIN/MAX`
- feat(display): backlight API now maps external [0, 100] range to hardware [brightness_min, brightness_max] range; add Kconfig options `LEDC_BACKLIGHT_BRIGHTNESS_DEFAULT/MIN/MAX`
- feat(audio): add Kconfig options for recorder: `CODEC_RECORDER_BITS/CHANNELS/SAMPLE_RATE/MIC_LAYOUT/GENERAL_GAIN/CHANNEL_GAINS`
- feat(kconfig): restructure Kconfig with granular per-subsystem debug log options for `AudioDevice`, `CodecPlayer`, `CodecRecorder`, `DisplayDevice`, `LedcBacklight`, `LcdPanel`, `LcdTouch`, `StorageDevice`, `GeneralFS`
- feat(kconfig): add `ENABLE_STORAGE_DEVICE` and `STORAGE_ENABLE_GENERAL_FS_IMPL` with `ENABLE_SPIFFS` / `ENABLE_SDCARD` sub-options
- feat(macro): expand `macro_configs.h` with all per-subsystem enable/debug macros for audio, display, and storage
- feat(test): restructure test apps with per-board configuration directories for `esp32_p4_function_ev`, `esp32_s3_korvo2_v3`, `esp_box_3`, `esp_sensair_shuttle`, `esp_vocat_board_v1_0/1_2`
- feat(test): add `idf_ext.py` for dynamic board config injection in test app builds
- feat(docs): update `README.md` and `README_CN.md` to reflect new architecture and usage

### Bug Fixes:

- fix(audio): rename log tag from `"HalAdaptor"` to `"HalAdpt"` to reduce log line width

## v0.7.0 - 2026-03-24

### Initial Release

- feat(repo): add `brookesia_hal_adaptor` component under `hal/`
- feat(audio): add `AudioDevice` plugin implementing `AudioCodecPlayerImpl` and `AudioCodecRecorderImpl`
- feat(display): add `LcdDisplay` plugin implementing `DisplayPanelIface` and `DisplayTouchIface`
- feat(board): integrate with `esp_board_manager` devices `audio_dac`, `audio_adc`, `display_lcd`, `lcd_brightness`, and `lcd_touch`
