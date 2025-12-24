# ChangeLog

## v0.7.0 - 2025-12-24

### Initial Release

- feat(repo): add agent management framework for ESP-Brookesia ecosystem
- feat(repo): implement unified agent lifecycle management (init, activate, start, stop, sleep, wakeup) through plugin mechanism
- feat(repo): implement state machine for automatic agent state transitions (TimeSyncing, TimeSynced, Starting, Started, Sleeping, Slept, WakingUp, Stopping)
- feat(repo): support agent activation/deactivation, suspend/resume, interrupt speaking, and state query operations
- feat(repo): implement event-driven architecture with support for general actions, state changes, text interactions (agent/user speaking text), and emote events
- feat(repo): support plugin mechanism for complete decoupling between project code and agent implementations
- feat(repo): provide agent attributes configuration with support for function calling, text processing, interrupt speaking, and emote features
- feat(repo): integrate with Audio service and SNTP service through service manager framework
- feat(repo): support NVS-based data persistence for active agent state
- feat(repo): implement plugin registry for dynamic agent discovery and management
