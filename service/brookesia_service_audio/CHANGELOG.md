# ChangeLog

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
