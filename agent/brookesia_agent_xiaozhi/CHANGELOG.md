# ChangeLog

## v0.7.3 - 2026-04-21

### Enhancements:

- feat(agent): route the new half-duplex chat mode to XiaoZhi's real-time listening backend at start-listening

### Bug Fixes:

- fix(agent): increase `start` operation timeout from 5000 ms to 10000 ms

## v0.7.2 - 2026-04-10

### Breaking Changes:

- break(repo): replace the entire `esp_iot_*` backend library with the redesigned `esp_xiaozhi_*` library; all private backend sources renamed and reorganized from `src/private/` into a dedicated `xiaozhi/` subdirectory
- break(kconfig): remove `BROOKESIA_AGENT_XIAOZHI_OTA_URL` configuration option
- break(kconfig): change default value of `BROOKESIA_AGENT_XIAOZHI_WAKE_WORD` from `"Hi,ESP"` to empty string (auto-detect wake word from Audio service)
- break(build): global `-Wno-missing-field-initializers` flag removed; `xiaozhi/` C sources now use `-Wno-stringop-truncation` only

### Enhancements:

- feat(mcp): implement `AddMCP_ToolsWithServiceFunction` function to register existing service functions as MCP tools via `mcp_utils::ToolRegistry`
- feat(mcp): implement `AddMCP_ToolsWithCustomFunction` function to register custom MCP tool definitions
- feat(mcp): implement `RemoveMCP_Tools` function to unregister MCP tools by name
- feat(mcp): add `mcp_tool_registry_` member and integrate with `esp_mcp_engine` in `on_startup()`; tools removed in `on_deinit()`
- feat(vision): implement `ExplainImage` function for image captioning via the XiaoZhi vision backend (`esp_xiaozhi_vision`)
- feat(vision): add `image_explain_handle_` and vision Kconfig options: `IMAGE_EXPLAIN_URL`, `IMAGE_EXPLAIN_TOKEN`, `IMAGE_EXPLAIN_RESPONSE_BUFFER_SIZE`
- feat(vision): add corresponding macros `BROOKESIA_AGENT_XIAOZHI_IMAGE_EXPLAIN_URL/TOKEN/RESPONSE_BUFFER_SIZE` in `macro_configs.h`
- feat(transport): add `ChatInfo` struct with `has_mqtt_config` / `has_websocket_config` flags; add `set_chat_info()` / `get_chat_info()` accessors
- feat(transport): add `open_audio_channel_task_` for delayed audio channel opening; rename `http_info_task_id_` to `get_chat_info_task_`
- feat(camera): add camera capture support via `esp_xiaozhi_camera`
- feat(video): add video encoding/decoding support via `esp_xiaozhi_video`
- feat(backend): refactor NVS keystore adapter with namespace-based handle mapping for `esp_xiaozhi_keystore`
- feat(backend): add MQTT transport layer via `esp_xiaozhi_mqtt`
- feat(backend): add payload serialization/deserialization via `esp_xiaozhi_payload`
- feat(deps): add `espressif/esp_websocket_client` (`1.*`) and `espressif/brookesia_mcp_utils` (`0.7.*`) dependencies
- feat(build): source `xiaozhi/Kconfig` from the subdirectory via `orsource`
- feat(docs): add Doxygen documentation to public types and methods in `agent_xiaozhi.hpp` and `macro_configs.h`
- feat(agent): add `on_deinit()` override to clean up MCP tools and vision handle

### Bug Fixes:

- fix(agent): increase `sleep` and `wake_up` operation timeouts from 2000 ms to 5000 ms
- fix(audio): change `on_chat_data()` parameter from `uint8_t *` to `const uint8_t *`

## v0.7.1 - 2026-02-25

### Enhancements:

- feat(settings): add `settings_set_global_callbacks()` function to allow custom settings backend implementation
- feat(settings): implement settings callbacks using NVS helper service (`get_key_value`, `save_key_value`, `erase_keys`)
- feat(agent): simplify `on_interrupt_speaking()` - remove delayed task and directly call `reset_interrupted_speaking()`

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
