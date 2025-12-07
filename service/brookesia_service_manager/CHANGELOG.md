# ChangeLog

## v0.7.0 - 2025-12-07

### Initial Release

- feat(repo): Add service management framework for ESP-Brookesia ecosystem
- feat(lifecycle): Implement unified service lifecycle management (init, start, stop, deinit) through plugin mechanism
- feat(local_calls): Support thread-safe, non-blocking local function calls via `ServiceBase` with high performance
- feat(remote_rpc): Support TCP-based client-server RPC communication for cross-device and cross-language scenarios
- feat(event_system): Implement event publish/subscribe mechanism for both local and remote event notifications
- feat(binding): Provide RAII-style `ServiceBinding` for automatic on-demand service state management (start/stop)
- feat(plugin): Support plugin mechanism for complete decoupling between project code and service implementations
- feat(function_registry): Implement function registry with support for synchronous and asynchronous function calls
- feat(event_registry): Implement event registry with support for event subscription and dispatch
- feat(decoupling): Enable service usage by adding component dependency without modifying project code
- feat(thread_safety): Built-in thread-safe protection with async scheduling for local calls
