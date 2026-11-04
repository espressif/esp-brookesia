# ChangeLog

## v0.8.9 - 2026-11-04

### Enhancements:

- perf(image): consult the document's asset bundle before reading an image file, which removes one
  open per image when the bundle carries media.

## v0.8.8 - 2026-10-29

### Enhancements:

- perf(image): read image files directly instead of through the storage service when the caller can
  touch the filesystem, cutting the image part of a screen build by 60-65% (keyboard_input
  136 -> 51 ms, settings_home 60 -> 21 ms on esp32_p4x_function_ev).

- feat(style): adapt LVGL style application for sparse Style fields from `brookesia_gui_interface`.
- feat(props): skip redundant common transform defaults when applying local LVGL transform props.
- perf(layout): skip the relative-placement scan when no record uses relative placement. Every
  property/style/layout/placement apply ends in that scan, so resident screens used to turn a
  constant-cost write into an O(records) one (-60% `flush_apply_ms` with keep-alive enabled).
- perf(layout): lay out only the screen being mounted when its layer carries no layout (opt-in), so
  keeping screens resident no longer makes every mount lay out the hidden ones too.

## v0.8.7 - 2026-10-27

### Enhancements:

- feat(display): add per-source frame / flush / dropped counters and font-load timing, compiled out behind `BROOKESIA_GUI_LVGL_ENABLE_PERF_PROBE` (default n) so shipped builds pay nothing.

### Bug Fixes:

- fix(display): count a frame once per refresh so FPS is not doubled on TEAR_AVOID_MODE_NONE.
- fix(render): avoid full-screen/layer invalidation from `process_timers()` when the ESP LVGL adapter owns timer processing.

## v0.8.6 - 2026-10-27

### Bug Fixes:

- fix(render): avoid flicker during deferred screen builds and refresh relative placement when a screen is mounted.
- fix(props): reset transforms when local transform properties are removed.
- fix(deps): require GUI Interface `>=0.8.3,<0.9`.
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
