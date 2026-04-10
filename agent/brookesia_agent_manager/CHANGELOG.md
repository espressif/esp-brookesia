# ChangeLog

## v0.7.4 - 2026-04-10

### Breaking Changes:

- break(manager): rename `DataType::ActiveAgent` to `DataType::TargetAgent`; update `BROOKESIA_DESCRIBE_ENUM`, NVS load/save, and all internal accessor paths accordingly
- break(manager): rename `function_activate_agent()` to `function_set_target_agent()`; decouple target selection from activation — `SetTargetAgent` now only stores the target, while `TriggerGeneralAction::Activate` triggers the actual activation
- break(manager): rename `function_get_attributes()` to `function_get_agent_attributes()`
- break(state_machine): `lib_utils::StateMachine` constructor no longer accepts a group name; group is now passed via `StartConfig` in `start()`
- break(state_machine): `state_machine_->start(scheduler, initial_state)` replaced by `state_machine_->start({.task_scheduler, .task_group_name, .initial_state})`
- break(state_machine): `state_machine_->add_state(name, ptr)` replaced by `state_machine_->add_state(ptr)`; state name is now set in the `StateBase` constructor
- break(manager): `set_data()` no longer calls `try_save_data()` automatically; callers are now responsible for explicit persistence

### Enhancements:

- feat(manager): add `function_set_target_agent()` to set the target agent without triggering activation
- feat(manager): add `function_get_target_agent()` to query the currently targeted agent name
- feat(manager): add `function_get_agent_names()` to retrieve all registered agent names
- feat(manager): add `get_agent_names()` internal helper method based on `Registry::get_all_instances()`
- feat(manager): initialize `TargetAgent` from the first registered agent in `on_init()`; reset to first registered agent (or empty) in `reset_data()`
- feat(base): add `wake_words_` member and `get_wake_words()` protected accessor
- feat(base): fetch AFE wake words from Audio service in `on_start()` and store in `wake_words_`
- feat(base): in `set_speaking()`, pause/resume AFE wakeup-end task when agent starts/stops speaking and wake words are configured
- feat(base): expose `is_general_action_running()` as protected method (moved from private)
- feat(manager/state_machine): include `macro_configs.h` in `manager.hpp` and `state_machine.hpp`
- feat(docs): add Doxygen documentation to all public types and methods across `base.hpp`, `manager.hpp`, `state_machine.hpp`, and `macro_configs.h`

### Bug Fixes:

- fix(manager): `function_get_active_agent()` now returns an error when no agent is active instead of an empty string
- fix(manager): `function_trigger_general_action()` with `Activate` action now activates the target agent on demand if none is active or the active agent differs from the target
- fix(manager): `function_reset_data()` no longer calls `try_erase_data()` directly; erase is now performed inside `reset_data()` for consistent cleanup
- fix(manager): `on_deinit()` no longer calls `reset_data()` to avoid side effects during teardown
- fix(manager): `try_load_data()` NVS log level reduced from INFO to DEBUG to reduce log noise

## v0.7.3 - 2026-02-25

### Breaking Changes:

- break(base): remove `get_call_task_group()`, `get_event_task_group()`, and `get_request_task_group()` override methods - now inherited from `ServiceBase`

### Enhancements:

- feat(base): add `set_manual_listening()` method and public `is_manual_listening()` accessor
- feat(base): implement `on_start()` override to configure task group parent relationships
- feat(base): use `set_manual_listening()` method instead of direct member access in `do_stop()` and `do_sleep()`
- feat(base): improve `do_manual_start_listening()` and `do_manual_stop_listening()` - change callback to non-blocking and adjust call order
- feat(base): change `get_state_task_group()` to return `get_call_task_group()` instead of separate state task group
- feat(state_machine): reduce log verbosity - remove INFO logs for init/deinit/start/stop operations
- feat(state_machine): change time sync check warnings to debug level logs

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
