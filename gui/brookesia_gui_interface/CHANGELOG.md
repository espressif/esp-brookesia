# ChangeLog

## Unreleased

### Enhancements:

- feat(json_ui): store Style sparsely and avoid parse-time tree copies to cut resident SRAM and fragmentation.
- feat(runtime): parse all screens eagerly, then build LVGL subtrees lazily (entry-first, idle warm, on-demand).
- feat(runtime): materialize deferred Eager screens from `find_view` / `create_view` / `destroy_view` for path-based APIs.
- feat(runtime): build deferred screens on a staging parent to avoid visible-layer flicker during idle warm.
- feat(parser): on Storage-worker paths, batch-read multi-file JSON assets by file count and release each batch after parse.

### Bug Fixes:

- fix(runtime): keep Dynamic screen unmount as immediate destroy (no keep-alive LRU).
- fix(runtime): rebuild via `ensure_screen_built` so language/theme refresh does not materialize empty stubs.
- fix(runtime): keep public `load*()` Eager defaults; System opts into deferred Eager builds explicitly.
- fix(parser): do not stat JSON asset sizes on the caller when FS I/O must run on the Storage worker.

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
