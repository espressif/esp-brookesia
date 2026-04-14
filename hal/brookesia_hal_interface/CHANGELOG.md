# ChangeLog

## v0.7.2 - 2026-04-14

### Enhancements:

- feat(api): add `get_all_devices()`, `get_all_interfaces()` helpers for device and interface discovery

## v0.7.1 - 2026-04-10

### Breaking Changes:

- break(api): remove old `interfaces/audio_player.hpp` (`AudioPlayerIface`), `interfaces/audio_recorder.hpp` (`AudioRecorderIface`), `interfaces/display_panel.hpp`, `interfaces/display_touch.hpp`, `interfaces/storage_fs.hpp`; replaced by redesigned headers under `audio/`, `display/`, and `storage/` subdirectories
- break(api): `interface.hpp` no longer acts as an aggregated convenience include; it now defines the `Interface` base class, `IsInterface` concept, and `InterfaceRegistry` alias; use `interfaces.hpp` for the full interface set
- break(api): umbrella header `hal_interface.hpp` now includes `interfaces.hpp` instead of `interface.hpp`
- break(api): `Device::query<T>()` and old FNV-1a hash helpers removed; replaced by `Device::get_interface<T>(name)`
- break(api): `Device::check_initialized()` pure virtual removed
- break(api): `Device` constructor parameter type changed from `const char *` to `std::string`
- break(api): old `AudioPlayerIface` renamed to `AudioCodecPlayerIface`; interface registry name changed from `"AudioPlayerIface"` to `"AudioCodecPlayer"`; `PlayerConfig` replaced by `Info` and `Config` structs

### Enhancements:

- feat(api): add `interfaces.hpp` as the new aggregated convenience include for all public interface type headers
- feat(audio): new `AudioCodecPlayerIface` (`audio/codec_player.hpp`) with `Info` (volume_default/min/max), `Config` (bits/channels/sample_rate), full virtual API (`open`, `close`, `set_volume`, `get_volume`, `mute`, `unmute`, `write_data`, `get_info`), and `BROOKESIA_DESCRIBE_STRUCT`
- feat(audio): new `AudioCodecRecorderIface` (`audio/codec_recorder.hpp`) with `Info` (bits, channels, sample_rate, mic_layout, general_gain, channel_gains), full virtual API (`open`, `close`, `read_data`, `set_general_gain`, `set_channel_gains`, `get_info`), and `BROOKESIA_DESCRIBE_STRUCT`
- feat(display): new `DisplayBacklightIface` (`display/backlight.hpp`) with `Info` (brightness_default/min/max), full virtual API (`set_brightness`, `get_brightness`, `get_info`)
- feat(display): new `display/panel.hpp` and `display/touch.hpp` replacing old `interfaces/` equivalents
- feat(storage): new `storage/fs.hpp` replacing old `interfaces/storage_fs.hpp`
- feat(api): `Interface` base class now provides `get_name()` accessor and `IsInterface<T>` concept for compile-time type constraints
- feat(api): `Device::get_interface<T>(name)` typed lookup using `std::shared_ptr` and `std::dynamic_pointer_cast`
- feat(test): add comprehensive test coverage for `AudioCodecPlayerIface`, `AudioCodecRecorderIface`, `DisplayBacklightIface`, `DisplayPanelIface`, `DisplayTouchIface`, `StorageFsIface` in `test_apps`
- feat(docs): update `README.md` and `README_CN.md` to reflect new API structure

## v0.7.0 - 2026-03-24

### Initial Release

- break(repo): rename component from `brookesia_hal` to `brookesia_hal_interface` and move it from `utils/` to `hal/`
- break(hal): rename legacy interface types and methods to the current `*Iface` and snake_case style
- break(hal): split monolithic interface declarations into dedicated headers under `include/brookesia/hal_interface/interfaces/`
- feat(hal): add `get_interfaces<T>()` and `get_first_interface<T>()` helpers for interface discovery
- feat(hal): add `DisplayTouchIface`, `AudioCodecPlayerIface`, and `AudioCodecRecorderIface`
- feat(test): add `test_apps` coverage for registry initialization, multi-interface queries, and FNV-1a hashing
