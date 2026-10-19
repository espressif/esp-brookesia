# ChangeLog

## v0.8.4 - 2026-10-27

- fix(runtime): keep Dynamic screen unmount as immediate destroy (no keep-alive LRU).
- fix(runtime): rebuild via `ensure_screen_built` so language/theme refresh does not materialize empty stubs.
- fix(runtime): keep public `load*()` Eager defaults; System opts into deferred Eager builds explicitly.
- fix(parser): do not stat JSON asset sizes on the caller when FS I/O must run on the Storage worker.
- feat(parser): include `base_dir` in the parser profile lines and add per screen/flow-item stage timing.
- feat(json_ui): store Style sparsely and avoid parse-time tree copies to cut resident SRAM and fragmentation.
- feat(runtime): parse all screens eagerly, then build LVGL subtrees lazily (entry-first, idle warm, on-demand).
- feat(runtime): materialize deferred Eager screens from `find_view` / `create_view` / `destroy_view` for path-based APIs.
- feat(runtime): build deferred screens on a staging parent to avoid visible-layer flicker during idle warm.
- feat(parser): on Storage-worker paths, batch-read multi-file JSON assets by file count and release each batch after parse.
- feat(runtime): add `DocumentLoadOptions` so callers can select deferred screen builds and idle warm when loading a document.
- test(mem_gate): add the host JSON UI dual-gate memory test.

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
