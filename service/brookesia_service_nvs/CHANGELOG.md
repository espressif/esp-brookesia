# ChangeLog

## v0.7.2 - 2026-02-02

### Enhancements:

- feat(Kconfig): add `BROOKESIA_SERVICE_NVS_ENABLE_AUTO_REGISTER` option for automatic plugin registration
- feat(Kconfig): rename default worker name from `SrvNVSWorker` to `SvcNVSWorker`
- feat(Kconfig): change default worker poll interval from 5ms to 10ms
- feat(repo): update `brookesia_service_helper` dependency to `0.7.*`

## v0.7.1 - 2025-12-10

## Enhancements

- feat(test_apps): add test cases for service helper NVS functions
- feat(nvs): auto use custom task scheduler if Service Manager uses external stack
- feat(docs): update README

### Bug Fixes

- fix(nvs): use `KeyValueMap` instead of `KeyValuePair`
- fix(nvs): use object instead of array for `set` function parameter

## v0.7.0 - 2025-12-07

### Initial Release

- feat(repo): Add NVS (Non-Volatile Storage) service for ESP-Brookesia ecosystem
- feat(namespace): Support namespace-based key-value pair storage with multiple independent storage spaces
- feat(data_types): Support three basic data types (Boolean, Integer, String)
- feat(persistent_storage): Store data in NVS partition with persistence after power loss
- feat(functions): Implement list function to list all key-value pairs in a namespace with metadata
- feat(functions): Implement set function to batch set multiple key-value pairs
- feat(functions): Implement get function to retrieve values of specified keys or all keys in a namespace
- feat(functions): Implement erase function to delete specified keys or clear entire namespace
- feat(nvs_init): Automatic NVS flash initialization with partition erase and version handling
- feat(thread_safety): Optional asynchronous task scheduling based on `TaskScheduler` for thread safety
- feat(default_namespace): Support default namespace "storage" when namespace is not specified
