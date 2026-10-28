# ChangeLog

## v0.9.0 - 2026-10-28

### Breaking Changes:

- break(deps): move to the shared Brookesia 0.9 line and use matching 0.9.x component dependencies.

## v0.8.0 - 2026-07-14

### Initial Release

- Add a generic single-connection BLE Peripheral/GATT Server service.
- Add canonical 128-bit UUID and runtime GATT configuration validation.
- Add lazy host startup, explicit advertising control, restart-safe cleanup, and connection control.
- Add owned byte-array write events, subscription tracking, MTU reporting, and directed notifications.
- Support ESP, Linux, and WebAssembly HAL implementations through the shared `PeripheralIface` contract.
- Add ESP-IDF/Unity and PC/Boost.Test component test applications.
