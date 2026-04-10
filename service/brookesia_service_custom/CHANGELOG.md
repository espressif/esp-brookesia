# ChangeLog

## v0.7.1 - 2026-04-10

### Enhancements:

- feat(docs): add Doxygen documentation to public types and methods in `service_custom.hpp` and `macro_configs.h`
- feat(build): enable `MINIMAL_BUILD` in test apps `CMakeLists.txt` to reduce build scope

## v0.7.0 - 2026-03-05

### Initial Release

- feat(repo): Add CustomService for ESP-Brookesia ecosystem
- feat(function): Support dynamic function registration and invocation via `register_function()` / `unregister_function()`
- feat(event): Support dynamic event registration and publish/subscribe via `register_event()` / `unregister_event()` / `publish_event()`
- feat(schema): Provide `get_function_schemas()` and `get_event_schemas()` for runtime schema query
