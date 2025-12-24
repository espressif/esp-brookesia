# ChangeLog

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
