# ChangeLog

## v0.7.5 - 2026-04-20

### Enhancements:

- feat(afe): dispatch `audio_recorder_open`/`close` to an internal-SRAM thread when caller runs on external-RAM stack
- feat(afe): give the wake-words helper thread a dedicated 5 KB stack

## v0.7.4 - 2026-04-10

### Breaking Changes:

- break(api): remove `SetPeripheralConfig` service function and corresponding `AudioPeripheralConfig` type alias
- break(api): remove internal `try_save_data()`, `get_data<T>()`, and `set_data<T>()` template helpers; NVS save is now deferred

### Enhancements:

- feat(api): add `SetMute` function to mute/unmute audio playback through `AudioCodecPlayerIface`
- feat(api): add `GetAFE_WakeWords` function to retrieve configured AFE wake-word list
- feat(api): add `PauseAFE_WakeupEnd` and `ResumeAFE_WakeupEnd` functions for AFE wakeup-end task lifecycle control
- feat(api): add `ResetData` function to erase all persisted service data (NVS)
- feat(playlist): replace single-URL loop state (`LoopPlaybackState`) with unified `PlaylistState` supporting multi-URL playlists; `PlayUrl` with an array of URLs is now handled natively
- feat(playlist): add `start_playlist()`, `play_playlist_url_at_index()`, `advance_playlist()` for sequential multi-URL playback with loop support
- feat(afe): add AFE wakeup-end task management (`start_afe_wakeup_end_task`, `stop_afe_wakeup_end_task`, `pause_afe_wakeup_end_task`, `update_afe_wakeup_end_task`)
- feat(hal): add `player_iface_` and `recorder_iface_` (`std::shared_ptr<hal::AudioCodecPlayerIface/RecorderIface>`) to hold HAL interface references during service lifetime
- feat(deps): upgrade `av_processor` dependency from `0.5.*` to `0.6.*`
- feat(docs): add Doxygen documentation to public types and methods in `service_audio.hpp` and `macro_configs.h`

### Bug Fixes:

- fix(data): change `data_player_volume_` type from `int` to `std::optional<uint8_t>` to correctly represent uninitialized state

## v0.7.3 - 2026-03-24

### Enhancements:

- feat(hal): use `AudioPlayerIface` and `AudioRecorderIface` for player and recorder access during service startup and shutdown
- feat(hal): route player volume control through `AudioPlayerIface` and remove direct `esp_codec` calls from the service layer
- feat(repo): keep audio peripheral configuration aligned with HAL-provided recorder defaults and handles

## v0.7.2 - 2026-02-25

### Enhancements:

- feat(repo): add `PauseEncoder` and `ResumeEncoder` functions

## v0.7.1 - 2026-02-02

### Breaking Changes:

- break(repo): remove `SetEncoderReadDataSize` function
- break(repo): change `av_processor` dependency to private (no longer publicly exposed)
- break(repo): refactor peripheral configuration methods to JSON-based config functions

### Enhancements:

- feat(repo): add `SetPeripheralConfig` function for peripheral configuration
- feat(repo): add `SetPlaybackConfig` function for playback configuration
- feat(repo): add `SetEncoderStaticConfig` function for encoder static configuration
- feat(repo): add `SetDecoderStaticConfig` function for decoder static configuration
- feat(repo): add `SetAFE_Config` function for AFE (Audio Front-End) configuration with VAD and WakeNet support
- feat(repo): add `PlayUrls` function for multiple URL playback
- feat(repo): add config parameter to `PlayUrl` function for playback configuration
- feat(repo): add loop playback support
- feat(repo): add type_converter module for internal type conversion
- feat(Kconfig): add `BROOKESIA_SERVICE_AUDIO_ENABLE_AUTO_REGISTER` option for automatic plugin registration
- feat(repo): update `brookesia_service_helper` dependency to `0.7.*`

## v0.7.0 - 2025-12-24

### Initial Release

- feat(repo): add audio service for ESP-Brookesia ecosystem
- feat(repo): implement audio playback with URL support and play control (pause, resume, stop)
- feat(repo): implement audio encoding with support for PCM, OPUS, and G711A codecs
- feat(repo): implement audio decoding with support for PCM, OPUS, and G711A codecs
- feat(repo): support volume control with set and get volume functions
- feat(repo): implement playback state management with state change events (Idle, Playing, Paused)
- feat(repo): support encoder configuration with flexible codec settings (channels, sample rate, bit depth, frame duration)
- feat(repo): support decoder configuration and stream decoding with data feed capability
- feat(repo): implement encoder data ready events for real-time audio processing
- feat(repo): support OPUS encoder extra configuration (VBR, bitrate)
- feat(repo): integrate with av_processor for complete audio processing pipeline
- feat(repo): support peripheral configuration (player, recorder, feeder) with gain control
