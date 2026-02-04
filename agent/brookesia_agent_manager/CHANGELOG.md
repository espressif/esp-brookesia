# ChangeLog

## v0.7.2 - 2026-02-02

### Breaking Changes:

- break(base): `Base` class now inherits from `service::ServiceBase`
- break(repo): migrate agent helper from `service::helper::AgentManager` to `agent::helper::Manager`
- break(repo): remove `gmf_ai_audio` dependency
- break(repo): change `brookesia_service_sntp` and `brookesia_service_audio` dependencies to private
- break(base): rename lifecycle methods: `on_start` -> `on_startup`, `on_stop` -> `on_shutdown`
- break(base): refactor `AudioConfig` to use `EncoderDynamicConfig` and `DecoderDynamicConfig`
- break(state_machine): add new states to `GeneralStateFlagBit`: `TimeSyncing`, `Ready`, `Activating`, `Activated`, `Error`

### Enhancements:

- feat(Kconfig): add `BROOKESIA_AGENT_MANAGER_ENABLE_AUTO_REGISTER` option for automatic plugin registration
- feat(Kconfig): add `BROOKESIA_AGENT_MANAGER_ENABLE_AFE_EVENT_PROCESSING` option for AFE event processing
- feat(base): add `ServiceConfig` struct for configuring service dependencies
- feat(base): add `ChatMode` type support
- feat(base): add `on_manual_start_listening()` and `on_manual_stop_listening()` methods
- feat(base): add task group methods: `get_call_task_group()`, `get_event_task_group()`, `get_request_task_group()`, `get_state_task_group()`
- feat(repo): add `brookesia_agent_helper` dependency
- feat(Kconfig): rename default worker name from `SrvAmWorker` to `AgentMgrWorker`
- feat(Kconfig): change default worker poll interval from 5ms to 10ms

## v0.7.1~1 - 2026-01-14

### Enhancements:

- feat(CMake): use linker option `-u` to register plugin instead of `WHOLE_ARCHIVE`

### Bug Fixes

- fix(agent): call `try_load_data()` in `set_info()`

## v0.7.1 - 2025-12-25

### Bug Fixes

- fix(yml): update `brookesia_service_audio` dependency version to `0.7.*`

## v0.7.0 - 2025-12-24

### Initial Release

- feat(repo): add agent management framework for ESP-Brookesia ecosystem
- feat(repo): implement unified agent lifecycle management (init, activate, start, stop, sleep, wakeup) through plugin mechanism
- feat(repo): implement state machine for automatic agent state transitions (TimeSyncing, TimeSynced, Starting, Started, Sleeping, Slept, WakingUp, Stopping)
- feat(repo): support agent activation/deactivation, suspend/resume, interrupt speaking, and state query operations
- feat(repo): implement event-driven architecture with support for general actions, state changes, text interactions (agent/user speaking text), and emote events
- feat(repo): support plugin mechanism for complete decoupling between project code and agent implementations
- feat(repo): provide agent attributes configuration with support for function calling, text processing, interrupt speaking, and emote features
- feat(repo): integrate with Audio service and SNTP service through service manager framework
- feat(repo): support NVS-based data persistence for active agent state
- feat(repo): implement plugin registry for dynamic agent discovery and management
