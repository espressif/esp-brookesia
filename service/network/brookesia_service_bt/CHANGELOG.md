# ChangeLog

## v0.9.0 - 2026-10-28

### Breaking Changes:

- break(deps): move to the shared Brookesia 0.9 line and use matching 0.9.x component dependencies.

### Bug Fixes:

- fix(version): report the component version generated from its manifest.

## v0.8.0 - 2026-07-20

### Initial Release

- Add the generic Bluetooth service with A2DP Sink and AVRCP control schemas.
- Route native PCM frames through the Manager AudioPlayback DataFlow operation.
- Add deterministic Linux/WASM A2DP injection backend and explicit unavailable reporting.
