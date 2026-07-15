# ChangeLog

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
