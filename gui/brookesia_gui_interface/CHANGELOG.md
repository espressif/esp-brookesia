# ChangeLog

## v0.8.3 - 2026-10-27

### Enhancements:

- feat(json_ui): reduce JSON UI memory use and fragmentation.
- feat(runtime): parse screen definitions on load and optionally defer non-entry builds until idle or first use.

### Bug Fixes:

- fix(runtime): build deferred screens when their views are looked up, created, or removed.
- fix(runtime): keep unbuilt screens deferred during language or theme refreshes.
- fix(runtime): destroy dynamic screens immediately when unmounted.
- fix(runtime): preserve the eager-load default and avoid flicker during deferred screen builds.
- fix(parser): avoid filesystem assertions when loading JSON assets from PSRAM tasks without PSRAM XIP.
- fix(deps): require Service Helper `>=0.8.6,<0.9` for batched storage reads.
- fix(style): preserve public optional `Style` fields for compatibility with 0.8.x consumers.

## v0.8.2 - 2026-07-27

### Enhancements:

- feat(build): split parser, binding, resource, screen, and runtime modules to reduce flash-sensitive code.

## v0.8.1 - 2026-07-13

### Enhancements:

- feat(build): use `idf_component.yml` as the single component version source on PC.
- feat(json_ui): add image resource preload metadata and manual preload lifecycle APIs.
- feat(runtime): support document-scoped image preload reference tracking and refresh.
- feat(scroll): add absolute view scrolling with optional animation.
- feat(font): add optional complex non-common glyph filtering for smaller dynamic fonts.

### Documentation:

- docs(json_ui): document protocol 0.1.1 and PNG preload behavior.
- docs(font): document glyph-complexity filtering and recommended subset workflows.

## v0.8.0 - 2026-06-28

### Initial Release:

- Initial release of `brookesia_gui_interface` with backend-neutral JSON UI documents, resources, bindings, and events.
- Provides runtime document APIs for GUI backends, themes, images, fonts, and actions.
