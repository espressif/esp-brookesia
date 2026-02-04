# ChangeLog

## v0.7.0 - 2026-01-16

### Initial Release

- feat(repo): add XiaoZhi agent implementation for ESP-Brookesia ecosystem
- feat(repo): implement OPUS audio codec support at 16kHz sample rate and 24kbps bitrate
- feat(repo): support agent general functions: `InterruptSpeaking`
- feat(repo): support agent general events: `SpeakingStatusChanged`, `ListeningStatusChanged`, `AgentSpeakingTextGot`, `UserSpeakingTextGot`, `EmoteGot`
- feat(repo): support manual listening control via `on_manual_start_listening()` and `on_manual_stop_listening()`
- feat(Kconfig): add `BROOKESIA_AGENT_XIAOZHI_ENABLE_AUTO_REGISTER` option for automatic plugin registration
- feat(Kconfig): add `BROOKESIA_AGENT_XIAOZHI_GET_HTTP_INFO_INTERVAL_MS` for activation check interval
- feat(Kconfig): add `BROOKESIA_AGENT_XIAOZHI_WAKE_WORD` for configurable wake word
- feat(repo): integrate with agent manager framework for unified lifecycle management
