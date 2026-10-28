# ChangeLog

## v0.8.0 - 2026-07-21

### Initial Release

- Add the high-level `BtSpeaker` service and helper with explicit configuration and lifecycle control.
- Forward A2DP/AVRCP state and controls through the generic `Bt` service.
- Stop optional local AudioPlayback asynchronously after a Bluetooth speaker connection.
- Add ESP, PC, WASM-compatible component wiring and deterministic service tests.
