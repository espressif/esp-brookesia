# ChangeLog

## v0.8.3 - 2026-10-27

### Breaking Changes:

- break(style): replace direct optional Style fields with sparse accessors; update and rebuild C++ consumers.

### Enhancements:

- feat(json_ui): store styles sparsely and reduce parser copies to lower resident memory and fragmentation.
- feat(runtime): parse every screen eagerly and optionally defer non-entry builds until idle or first use.
- feat(parser): batch Storage-worker reads and release each batch after parsing.

### Bug Fixes:

- fix(runtime): materialize deferred screens for view lookup, creation, destruction, and language or theme refresh.
- fix(runtime): destroy dynamic screens immediately when unmounted.
- fix(runtime): preserve public eager-load defaults and stage deferred builds away from the active screen.
- fix(parser): keep JSON asset size checks on the Storage worker when required.
- fix(deps): require Service Helper 0.8.6 or later for batched storage reads.

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
