# ChangeLog

## v0.7.0 - 2026-03-24

### Initial Release

- break(repo): rename component from `brookesia_hal` to `brookesia_hal_interface` and move it from `utils/` to `hal/`
- break(hal): rename legacy interface types and methods to the current `*Iface` and snake_case style
- break(hal): split monolithic interface declarations into dedicated headers under `include/brookesia/hal_interface/interfaces/`
- feat(hal): add `get_interfaces<T>()` and `get_first_interface<T>()` helpers for interface discovery
- feat(hal): add `DisplayTouchIface`, `AudioPlayerIface`, and `AudioRecorderIface`
- feat(test): add `test_apps` coverage for registry initialization, multi-interface queries, and FNV-1a hashing
