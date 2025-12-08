# ChangeLog

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
