# ChangeLog

## v0.7.7 - 2026-04-20

### Bug Fixes:

- fix(memory_profiler): compare `*FreePercent` thresholds by byte ratio to avoid integer-truncation false positives

## v0.7.6 - 2026-04-10

### Breaking Changes:

- break(state_machine): `StateMachine::add_state()` signature changed — removed the separate `name` parameter; the state name is now derived from the `StatePtr` itself (was `add_state(name, state)`, now `add_state(state)`)
- break(state_machine): `StateMachine::start()` signature changed — now accepts a single `Config` struct instead of separate `task_scheduler` and `initial_state` parameters
- break(state_machine): `StateMachine` constructor changed from `explicit StateMachine(group_name)` to default `StateMachine()` (configuration moved to `Config`)
- break(task_scheduler): `PreExecuteCallback` signature extended with a leading `const Group &` parameter (was `void(TaskId, TaskType)`, now `void(const Group &, TaskId, TaskType)`)
- break(task_scheduler): `PostExecuteCallback` signature extended with a leading `const Group &` parameter (was `void(TaskId, TaskType, bool)`, now `void(const Group &, TaskId, TaskType, bool)`)

### Enhancements:

- feat(state_machine): add `StateMachine::Config` struct encapsulating `task_scheduler`, `task_group_name`, `task_group_config`, and `initial_state`
- feat(task_scheduler): add `pre_execute_callback` and `post_execute_callback` fields to `StartConfig` and `GroupConfig` structs
- feat(task_scheduler): change Boost includes from broad umbrella headers (`<boost/asio.hpp>`, `<boost/thread.hpp>`) to fine-grained per-component headers for faster build times
- feat(deps): upgrade `esp-boost` dependency from `0.4.*` to `0.5.*`
- feat(docs): add comprehensive Doxygen documentation to all public types and methods across `task_scheduler.hpp`, `state_machine.hpp`, `check.hpp`, `describe_helpers.hpp`, `function_guard.hpp`, `log.hpp`, `memory_profiler.hpp`, `plugin.hpp`, `thread_config.hpp`, `thread_profiler.hpp`, `time_profiler.hpp`, and umbrella header `lib_utils.hpp`
- feat(build): enable `MINIMAL_BUILD` in all test app `CMakeLists.txt` files

## v0.7.5 - 2026-02-25

#### Enhancements:

* feat(describe_helpers): add support for 'std::queue' and 'std::list' serialization/deserialization
* feat(task_scheduler): add 'parent_group' support in 'GroupConfig' to allow child groups to use parent group's strand
* feat(task_scheduler): improve log format consistency (use 'Task[%1%]' format) and adjust log levels

#### Bug Fixes:

* fix(task_scheduler): fix race condition in promise fulfillment by removing 'promise_fulfilled' atomic flag and protecting promise operations under mutex
* fix(state_machine): remove unnecessary 'is_running()' check in 'force_transition_to()' method

## v0.7.4 - 2026-01-18

#### Breaking Changes:

* break(plugin): change the order of parameters in `BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL()`
* break(state_machine): rename `is_updating()` to `has_transition_running()` and `has_state_updating()`

#### Enhancements:

* feat(describe_helpers): add support for types derived from 'std::variant'
* feat(describe_helpers): add support for 'std::span'
* feat(describe_helpers): add flexible JSON key matching (PascalCase/camelCase to snake_case)
* feat(state_machine): optimize locking using `shared_mutex` for read operations
* feat(task_scheduler): add `restart_timer()` method for resetting delayed/periodic task timers
* feat(thread_config): modify default name to 'Thread'
* feat(repo): update 'esp-boost' dependency to '0.4.*'

### Bug Fixes:

* fix(task_scheduler): check if the task scheduler is running before performing any operations

## v0.7.3~1 - 2026-01-15

#### Enhancements:

* feat(plugin): add support for custom symbol name
* feat(time_profiler): add support for statistics reporting

## v0.7.3 - 2025-12-29

#### Enhancements:

* feat(plugin): add support for linker -u option for plugin registration

#### Bug Fixes:

* fix(log): fix 'BROOKESIA_LOG_TRACE_GUARD()' macro not working correctly

## v0.7.2 - 2025-12-24

#### Enhancements:

* feat(task_scheduler): add 'get_executor()' and 'get_task_count()' methods
* fix(task_scheduler): rename 'enable_post_execute_in_order' to 'enable_serial_execution' in 'GroupConfig'
* fix(task_scheduler): avoid any type of task from being executed in parallel when the group is structured with 'enable_serial_execution'
* feat(state_machine): add 'is_updating()' method
* feat(describe_helpers): optimize pointer serialization and deserialization support
* feat(describe_helpers): add support for 'std::monostate'
* feat(repo): update 'esp-boost' dependency to '>=0.4.2,<0.5.0'
* feat(repo): add README

#### Bug Fixes:

* fix(task_scheduler): avoid periodic task from being executed repeatedly
* fix(function_guard): add exception handling in destructor
* fix(log): fix garbled display issue for 'uint8_t/int8_t *'

## v0.7.1 - 2025-12-07

#### Enhancements:

* feat(describe_helpers): Add describe helpers for converting between any type and string (JSON format), JSON and any type
* feat(describe_helpers): Add support for 'std::variant', 'std::function'
* feat(state_machine): Add 'wait_all_transitions()', 'force_transition_to()' methods
* feat(task_scheduler): Add 'dispatch()' method

#### Bug Fixes:

* fix(log): Fix garbled display issue for 'int8_t'

## v0.7.0 - 2025-11-28

### Initial Release

- feat(repo): Add task scheduler (based on Boost.Asio) supporting multi-threaded task management, including immediate, delayed, and periodic tasks
- feat(repo): Support thread configuration (name, priority, stack size, stack location, CPU binding configuration)
- feat(repo): Add performance analysis tools (memory, thread, and time profilers)
- feat(repo): Support logging system (ESP_LOG and standard printf, boost::format)
- feat(repo): Support state base class and state machine for managing complex state transitions
- feat(repo): Add plugin mechanism to support modular capability extension
- feat(repo): Add helper utilities (false/nullptr/exception/value_range checking, function guard, object description)
