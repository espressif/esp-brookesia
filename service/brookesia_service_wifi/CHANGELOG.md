# ChangeLog

## v0.7.3 - 2026-02-02

### Enhancements:

- feat(Kconfig): add `BROOKESIA_SERVICE_WIFI_ENABLE_AUTO_REGISTER` option for automatic plugin registration
- feat(Kconfig): add HAL event timeout configurations (started, stopped, connected, disconnected)
- feat(Kconfig): add state machine finished timeout configuration
- feat(Kconfig): rename default worker name from `SrvWifiWorker` to `SvcWifiWorker`
- feat(repo): update `brookesia_service_helper` dependency to `0.7.*`

## v0.7.2 - 2026-01-14

### Enhancements:

- feat(CMake): use linker option `-u` to register plugin instead of `WHOLE_ARCHIVE`
- feat(repo): update `brookesia_service_helper` dependency to `>=0.7.2,<0.8.0`

### Bug Fixes

- fix(hal): initialize NVS flash in `Hal::init()` instead of `Hal::start()`
- fix(hal): ensure execution in a thread with an SRAM stack for `esp_wifi_init()` and `nvs_flash_init()`

## v0.7.1 - 2025-12-09

### Enhancements:

- feat(hal): add 'reset_data' function
- feat(service): add 'reset_data' function
- feat(test_apps): remove 'general` test_apps

## v0.7.0 - 2025-12-07

### Initial Release

- feat(repo): Add WiFi connection management service with state machine-based lifecycle management
- feat(repo): Implement state machine with 4 core states (Deinited, Inited, Started, Connected)
- feat(repo): Support automatic connection to historical APs and auto-reconnection after disconnection
- feat(repo): Support periodic WiFi scanning with configurable interval and timeout
- feat(repo): Manage target AP and connected AP lists with multiple AP history records
- feat(repo): Optional integration with `brookesia_service_nvs` for persistent connection configuration storage
- feat(repo): Provide rich event notification mechanisms for real-time WiFi state change feedback
- feat(repo): Implement asynchronous task scheduling based on `TaskScheduler` for thread safety
- feat(repo): Support both local WiFi HAL and remote WiFi HAL (for ESP32-P4)
- feat(repo): Implement functions for general actions, scan control, scan parameters, and AP connection management
