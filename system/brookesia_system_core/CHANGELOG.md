# ChangeLog

## v0.8.4 - 2026-08-31

### Breaking Changes:

- break(runtime): require declarations for every non-system service RPC used by runtime apps.

### Enhancements:

- feat(app): parse service dependencies and expose them through app information.
- feat(app): block runtime package installation or app startup when declared services are unavailable.
- feat(runtime): reject undeclared named service access outside the three system RPCs.

### Documentation:

- docs(app): document service declarations, version compatibility, and runtime enforcement.

## v0.8.3 - 2026-08-04

### Enhancements:

- feat(usb): add the USB Serial/JTAG host-control and runtime package bridge.
- feat(app): expose runtime package installation to system-level integrations.

### Bug Fixes:

- fix(app): warn and continue when manifests contain unsupported fields.

## v0.8.2 - 2026-07-27

### Enhancements:

- feat(build): split runtime, service, and GUI orchestration implementation units.
- chore(build): use lightweight static schema and utility APIs.

## v0.8.1 - 2026-07-13

### Breaking Changes:

- break(gui): return explicit errors when native apps cannot update GUI view debug state.

### Enhancements:

- feat(service): register a description for Manager metadata queries.
- feat(build): use `idf_component.yml` as the single component version source on PC.
- feat(logging): print the component version during system initialization.
- feat(gui): add native app image preload and release helpers.
- feat(runtime): expose SystemGui PreloadImages and ReleaseImages for runtime apps.
- feat(app): support preloading app GUI documents at install time to reduce launch latency.
- feat(gui): add batch action subscriptions, absolute scrolling, and theme-language queries.
- feat(gui): expose GUI view debug control directly through the native app GUI runtime.
- feat(service): register the component version for built-in system service version queries.
- chore(scheduler): remove the Worker suffix from default thread names on ESP and PC.

### Documentation:

- docs(gui): document image preload services and resource permission scope.

### Bug Fixes:

- fix(storage): bind Storage for the System lifecycle and route app file operations through its service APIs.

## v0.8.0 - 2026-06-28

### Initial Release:

- Initial release of `brookesia_system_core` with native and runtime app installation, lifecycle, and package scanning.
- Provides app-scoped GUI, storage, timer, and service bridge foundations.
