# ChangeLog

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
