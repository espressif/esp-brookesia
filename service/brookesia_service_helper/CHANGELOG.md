# ChangeLog

## v0.7.4 - 2026-02-25

### Breaking Changes:

- break(emote): remove `QRCode` object type from `EventMessageType` enum

### Enhancements:

* feat(wifi): add `GeneralState` enum with stable and transient states for WiFi state machine
* feat(wifi): add `ScanParams` struct for scan configuration
* feat(wifi): add `SoftApParams` struct and `SoftApEvent` enum for SoftAP functionality
* feat(wifi): add SoftAP-related functions: `TriggerSoftApStart`, `TriggerSoftApStop`, `TriggerSoftApProvisionStart`, `TriggerSoftApProvisionStop`, `SetSoftApParams`, `GetSoftApParams`
* feat(wifi): add `GetGeneralState` function to get current WiFi state
* feat(wifi): update `SetScanParams` function to use object parameter instead of multiple separate parameters
* feat(wifi): add `channel` field to `ApInfo` struct
* feat(wifi): change `get_signal_level()` to static method in `ApInfo`
* feat(wifi): add `SoftApEventHappened` event
* feat(wifi): add `IsUnexpected` parameter to `GeneralEventHappened` event
* feat(audio): add `PauseEncoder` and `ResumeEncoder` functions

## v0.7.3 - 2026-02-02

### Breaking Changes:

- break(repo): move agent helpers (coze, manager, openai) to `brookesia_agent_helper` component

### Enhancements:

- feat(base): add `ConvertibleToFunctionValue` concept for type-safe parameter conversion
- feat(base): add `Timeout` tag type for specifying timeout in function calls
- feat(base): add type traits for event callback parameter detection
- feat(audio): expand Audio helper with additional function and event schemas
- feat(emote): update Expression Emote helper schemas
- feat(repo): add `require_scheduler` field to `FunctionSchema` and `EventSchema`
- feat(repo): update `brookesia_service_manager` dependency to `0.7.*`

## v0.7.2 - 2026-01-14

### Enhancements:

- feat(repo): update function and event descriptions

## v0.7.1 - 2025-12-24

### Enhancements:

- feat(repo): add `Base` class
- feat(wifi): add 'reset_data' function
- feat(repo): add SNTP helper class
- feat(nvs): add `is_available`, `save_key_value`, `get_key_value`, `erase_keys` functions
- feat(repo): update `brookesia_service_manager` dependency to `>=0.7.1,<0.8.0`
- feat(repo): update README

### Bug Fixes

- fix(nvs): use object instead of array for `set` function parameter
- fix(repo): rename `definition/DEFINITION` to `schema/SCHEMA`

## v0.7.0 - 2025-12-07

### Initial Release

- feat(repo): Add service development helper library for ESP-Brookesia ecosystem
- feat(repo): Provide type-safe definitions (strongly-typed enums and structs)
- feat(repo): Provide standardized function definitions (FunctionSchema, including names, parameters, and descriptions)
- feat(repo): Provide standardized event definitions (EventSchema, including names and data formats)
- feat(repo): Provide constant definitions (service names and default values), function and event index enumerations
- feat(repo): Provide NVS helper class (ValueType enum, KeyValuePair/EntryInfo structs, and function definitions)
- feat(repo): Provide Wifi helper class (GeneralState/GeneralAction/GeneralEvent enums, ApInfo struct, and function/event definitions)
