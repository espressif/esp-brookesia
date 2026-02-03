# ChangeLog

## v0.7.2 - 2026-02-02

### Breaking Changes:

- break(repo): migrate agent helper from `service::helper::AgentOpenai` to `agent::helper::Openai`
- break(repo): change `esp_peer` dependency to private
- break(base): rename lifecycle methods: `on_start` -> `on_startup`, `on_stop` -> `on_shutdown`
- break(base): change `on_wakeup()` return type to `bool`
- break(base): move `encoder_feed_data_size` to `encoder.fetch_data_size` in `AudioConfig`
- break(base): replace `general_event_wait_timeout_ms` with `operation_timeout` in `AgentAttributes`

### Enhancements:

- feat(Kconfig): add `BROOKESIA_AGENT_OPENAI_ENABLE_AUTO_REGISTER` option for automatic plugin registration
- feat(Kconfig): select `ESP_WS_CLIENT_SEPARATE_TX_LOCK` for WebSocket client
- feat(base): inherit from `service::ServiceBase` with `get_function_schemas()`, `get_event_schemas()`, `get_function_handlers()` implementations

## v0.7.1~1 - 2026-01-14

### Enhancements:

- feat(CMake): use linker option `-u` to register plugin instead of `WHOLE_ARCHIVE`

### Bug Fixes

- fix(agent): call `try_load_data()` in `set_info()`

## v0.7.1 - 2025-12-25

### Bug Fixes

- fix(repo): add missing return statement in `on_init()`

## v0.7.0 - 2025-12-24

### Initial Release

- feat(repo): add OpenAI agent implementation for ESP-Brookesia ecosystem
- feat(repo): implement OpenAI Realtime API integration with WebRTC data channel for real-time voice and text interaction
- feat(repo): support peer-to-peer communication using esp_peer with signaling and data channels
- feat(repo): implement audio codec support with OPUS format at 16kHz sample rate and 24kbps bitrate
- feat(repo): support multiple data channel message types (audio streams, text streams, function calls, session management)
- feat(repo): support NVS-based data persistence for agent configuration
- feat(repo): implement real-time audio input/output with low-latency voice interaction
- feat(repo): integrate with agent manager framework for unified lifecycle management
