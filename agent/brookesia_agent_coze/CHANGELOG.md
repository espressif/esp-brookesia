# ChangeLog

## v0.7.1 - 2026-02-02

### Breaking Changes:

- break(repo): integrate `esp_coze` subcomponent into main component as private implementation
- break(repo): change `esp_websocket_client` dependency to private
- break(repo): migrate agent helper from `service::helper::AgentCoze` to `agent::helper::Coze`
- break(repo): refactor lifecycle methods (`on_activate`, `on_startup`, `on_shutdown`, etc.)
- break(repo): move some dependencies from `REQUIRES` to `PRIV_REQUIRES`

### Enhancements:

- feat(Kconfig): add `BROOKESIA_AGENT_COZE_ENABLE_AUTO_REGISTER` option for automatic plugin registration
- feat(repo): add `on_interrupt_speaking()` method for interrupting agent speech
- feat(repo): add `support_general_functions` and `support_general_events` in agent attributes
- feat(repo): add `require_time_sync` attribute for time synchronization requirement
- feat(repo): move `fetch_data_size` into encoder configuration
- feat(repo): update `brookesia_agent_manager` dependency to `0.7.*`

## v0.7.0~1 - 2026-01-14

### Enhancements:

- feat(CMake): use linker option `-u` to register plugin instead of `WHOLE_ARCHIVE`

### Bug Fixes

- fix(base): correct debug log

## v0.7.0 - 2025-12-24

### Initial Release

- feat(repo): add Coze agent implementation for ESP-Brookesia ecosystem
- feat(repo): implement Coze API integration with WebSocket communication for real-time voice and text interaction
- feat(repo): support OAuth2 authentication with automatic access token management
- feat(repo): support multiple robot configuration with dynamic robot switching
- feat(repo): implement audio codec support with G711A format at 16kHz sample rate
- feat(repo): support emote events from Coze platform
- feat(repo): support NVS-based data persistence for agent configuration and active robot index
- feat(repo): implement Coze platform event handling (e.g., insufficient credits balance)
- feat(repo): integrate with agent manager framework for unified lifecycle management
