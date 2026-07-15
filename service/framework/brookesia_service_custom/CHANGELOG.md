# ChangeLog

## v0.8.1 - 2026-07-13

### Enhancements:

- feat(service): register a description for Manager metadata queries.
- feat(service): register the component version for centralized Manager service queries.
- chore(scheduler): remove the Worker suffix from default thread names on ESP and PC.

### Bug Fixes:

- fix(build): use `idf_component.yml` as the single component version source.

## v0.8.0 - 2026-06-28

### Enhancements:

- feat(repo): move Custom service into the framework service category for grouped component publishing
- feat(deps): align first-party Brookesia dependencies with the v0.8 release train

### Documentation:

- docs(readme): refresh component README links for the reorganized programming guide

## v0.7.2 - 2026-05-29

### Bug Fixes:

- fix(test): set explicit whole-archive linking for the test app component
- fix(test): update ESP32-P4 test environment and PSRAM defaults for the function EV board

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
