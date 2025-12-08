# ChangeLog

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
