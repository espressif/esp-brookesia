# ChangeLog

## v0.8.8 - 2026-11-19

### Enhancements:

- feat(gui): add `AppGuiRuntime::with_native_view` as a restricted native-integration escape hatch.
- feat(gui): route the callback through the GUI task group with the GUI thread guard already held.

### Bug Fixes:

- fix(gui): return an error when a native view callback throws, instead of leaving the caller blocked.
- fix(deps): require GUI Interface `>=0.8.8,<0.9`.

### Documentation:

- docs(gui): document the native integration boundary, handle lifetime, and usage restrictions.

## v0.8.7 - 2026-11-10

### Breaking Changes:

- break(abi): rebuild native binary apps for the expanded app and system interfaces and GUI descriptors.

### Enhancements:

- feat(gui): select GSP or LVGL with backend-specific dependencies and resources.
- feat(gsp): host native and JavaScript apps with deployable bundles, theme start scenes, and GUI event delivery.
- feat(gsp): add JavaScript text, value, property, and scene updates for the active application.
- feat(shell): add a paginated GSP launcher, shared keyboard and dialogs, and saved language/theme restoration.
- feat(app): expose runtime app removal through the system service.
- feat(linux): run the GSP shell and app manager on desktop displays with platform support checks.

### Bug Fixes:

- fix(gsp): stop partially started app and modal sessions before restoring the foreground application.
- fix(lifecycle): stop all sessions before uninstalling apps so deinit retries cannot repeat uninstall callbacks.
- fix(app): validate recovery targets against install metadata and transaction ownership before moving directories.
- fix(app): reject runtime packages whose declared GUI backend conflicts with their descriptor or the firmware.
- fix(app): restore previous versions after failed updates and recover interrupted installations at startup.
- fix(build): isolate staged backend resources while preserving simulator app data across rebuilds.
- fix(gsp): retain borrowed resources during deferred stop and retry modal resumption or return to the launcher.
- fix(gsp): queue lifecycle callbacks without re-entering their synchronization gates.
- fix(gsp): validate and bound cached updates, and retain the previous scene when a page change fails.
- fix(app): protect the system shell and restrict service-driven removal to managed runtime apps.
- fix(app): synchronize app-record maps and retain records while tasks still reference them.

### Documentation:

- docs(gsp): describe backend selection, shared input, session lifetimes, and Linux modal request behavior.

## v0.8.6 - 2026-10-27

### Enhancements:

- feat(system): expose `gui_binding_flush_stats()` behind `BROOKESIA_SYSTEM_CORE_ENABLE_GUI_BINDING_PROBE` (default n) so the flush-apply metric costs nothing in shipped builds.
- feat(system): drive the GUI backend from `System::process_timers()` so deferred work advances on the timer path.
- feat(system): make `system_gui_load_json` / `system_gui_mount_screen` / `system_gui_unmount_screen` / `system_gui_unload` public so a firmware carrier without a derived App can drive the GUI.
- feat(system): add and expose `system_gui_load_theme_file` / `system_gui_set_theme` so a carrier can register and activate a theme before loading GUI documents.

### Bug Fixes:

- fix(system): post the GUI binding-flush re-arm as a real queue hop instead of running it inline.
- fix(build): include `esp_timer.h` only when the GUI binding probe is enabled.

## v0.8.5 - 2026-10-27

### Enhancements:

- feat(gui): build non-entry app screens on demand or during idle.
- feat(gui): preload app documents without building screens.
- feat(profiling): log lifecycle memory snapshots when profile logging is enabled.

### Bug Fixes:

- fix(deps): require GUI Interface `>=0.8.3,<0.9` for deferred screen loading.

### Documentation:

- docs(usb): update package and command names for the brookesia-usb CLI.

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
