# ChangeLog

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
