# ChangeLog

## v0.8.5 - 2026-09-17

### Bug Fixes:

- fix(deps): pin LVGL 9.5.0 and build its FreeType port with internal headers.

## v0.8.4 - 2026-08-25

### Bug Fixes:

- fix(threading): use the recursive LVGL adapter lock instead of C++ thread-local nesting.

## v0.8.3 - 2026-08-05

### Enhancements:

- feat(event): expose pointer coordinates and released slider values through event payloads.

## v0.8.2 - 2026-07-27

### Enhancements:

- feat(build): split LVGL style and property implementations to reduce flash-sensitive code.

## v0.8.1 - 2026-07-13

### Enhancements:

- feat(build): use `idf_component.yml` as the single component version source on PC.
- feat(image): support explicit PNG decode preload and cached descriptor reuse.
- feat(image): resolve PNG dimensions through the LVGL decoder when needed.
- feat(image): validate, size, preload, and cache JPEG resources with platform decoders.
- feat(scroll): add absolute LVGL view scrolling with optional animation.
- feat(display): expose display-source task, timing, buffer, and stack defaults through configuration.

### Bug Fixes:

- fix(image): avoid draw buffer validation errors for predecoded PNG descriptors.
- fix(font): invalidate failed font lookups when font resources or mounts change.
- fix(keyboard): detach shared LVGL keyboard state before destroying keyboard nodes.

## v0.8.0 - 2026-06-28

### Initial Release:

- Initial release of `brookesia_gui_lvgl` with LVGL object mapping for resolved GUI documents.
- Provides build-time image packaging and runtime image preload support.
