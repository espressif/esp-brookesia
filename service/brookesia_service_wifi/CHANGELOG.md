# ChangeLog

## v0.7.4 - 2026-02-25

### Breaking Changes:

* break(state_machine): rename initial state from `Deinited` to `Idle`
* break(hal): restructure HAL header files - `hal.hpp` is now split into multiple files in `hal/` subdirectory

### Enhancements:

* feat(hal): add SoftAP functionality with parameter configuration, optimal channel selection, and provisioning support
* feat(hal): add SoftAP provisioning functionality with HTTP server and HTML configuration page
* feat(state_machine): add transient states (Initing, Deiniting, Starting, Stopping, Connecting, Disconnecting) to state machine
* feat(Kconfig): reorganize configuration structure with hierarchical menus (Hal/General, Hal/Station, Hal/SoftAP, State Machine, Service)
* feat(Kconfig): add SoftAP configuration options (channel range, setup interval/timeout, event delay)
* feat(Kconfig): add SoftAP Provision configuration options (scan interval/timeout, success close delay, DNS thread settings)
* feat(Kconfig): add Station configuration options (connect retries max, auto connect delay)
* feat(Kconfig): add State Machine configuration options (general state update interval, wait finished timeout)
* feat(Kconfig): add Service configuration options (deinit finished timeout, general action queue dispatch interval, NVS save/erase data timeout)
* feat(Kconfig): add HAL event timeout configurations for Inited and Deinited events
* feat(CMake): move dependencies to PRIV_REQUIRES and add `esp_event` and `esp_http_server` dependencies
* feat(CMake): use `GLOB_RECURSE` to recursively find source files for better subdirectory support
* feat(CMake): embed HTML provisioning page as text file resource
* feat(repo): update description to include SoftAP functionality

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
