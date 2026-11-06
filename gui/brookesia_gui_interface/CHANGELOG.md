# ChangeLog

## v0.8.7 - 2026-11-19

### Enhancements:

- feat(asset): bundles can now carry media files, and `brookesia/gui_interface/asset_source.hpp`
  lets any component read an asset through the bundle that the loaded document registered, so the
  LVGL image loader no longer opens one file per image.

### Bug Fixes:

- fix(asset): reject a bundle whose entry count, path length, or key order cannot be searched, so a
  corrupt file falls back to per-file reads.
- fix(runtime): keep a displayed transient screen out of the resident eviction queue.
- fix(runtime): drop a document's resident-screen queue when that document is unloaded.
- fix(deps): require lib_utils `>=0.8.4,<0.9` for `can_read_local_files()`.
- fix(runtime): a backend that cannot report input-idle time returns `UINT32_MAX`, so it does not
  look like a touch in the current tick and block prewarm.
- fix(parser): live preview reloads the changed source files and drops that document's hold on
  the registered bundle after the reload parses. Each open is its own hold, so another document
  on the same root keeps its stream, and the previewed document's later image reads skip that
  bundle. Unload drops only that document's hold. `parse_document()`, `parse_document_file()`,
  and `parse_document_file_with_metadata()` release the hold before they return, because they
  do not create a document that can be unloaded.

## v0.8.6 - 2026-11-19

### Enhancements:

- feat(runtime): add a keep-alive for unmounted Dynamic screens, so a re-mount reuses the built
  subtree instead of rebuilding it. It defaults on for PSRAM builds and is bounded by
  `BROOKESIA_GUI_INTERFACE_KEEP_DYNAMIC_SCREENS_MAX`, so a document cannot retain every screen it
  ever visited. esp32_p4x_function_ev: mount 193 ms -> 15 ms, about 365 KB resident PSRAM, and
  about +0.2 ms per text-binding flush. esp32_p4_function_ev against feat/gui-perf-baseline
  (perf_eval, 11 screens): mount_ms 197.1 -> 17.8 ms and screen_cycle mount_all_screens_ms
  52.7 -> 18.8 ms, about 215 KB more resident PSRAM, flush_apply_ms 1.00 -> 2.69 ms.
- feat(runtime): serve interned `SparseResolvedStyle` objects from a fixed-capacity pool when
  `BROOKESIA_GUI_INTERFACE_STYLE_POOL_SLOTS` is set, which reduces small-block PSRAM fragmentation.
- perf(runtime): build the scoped binding store key once per update instead of once per write and
  once per notification, cutting the binding flush apply time by ~10%.
- test(runtime): add opt-in flush/apply profiling (`GUI binding flush profile`, `GUI binding reapply
  profile`, `GUI props apply profile`) behind `BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG`, so a
  binding-latency regression can be attributed to store, lookup, binding or backend work.
- feat(runtime): prewarm Dynamic screens during the idle pass (opt-in, budgeted) so the first mount
  reuses a built subtree instead of paying the build on the interaction path.
- feat(runtime): delay the prewarm pass (`..._PREWARM_DYNAMIC_SCREENS_DELAY_MS`, default 5 s) so the
  background builds do not race the boot-time mount path for the LVGL lock.
- perf(parser): read text assets in large chunks without a preceding file_size() stat, cutting
  document load by 25-39% (a 30-asset document: 1187 -> 885 ms).
- perf(parser): copy a node object only when it actually references interaction templates, cutting
  screen parsing by 34% (a 30-asset document: 885 -> 724 ms).
- perf(parser): reuse the already parsed child type, cache duplicated field lookups and pass
  string defaults as views while walking a screen into sparse nodes.
- test(parser): add an opt-in per-screen timing breakdown (template/style/self-props/children)
  behind the existing profile-log switch.
- perf(parser): match the camelCase and normalized key fallbacks without allocating, cutting the
  screen walk by 37% (parse_flows_screens 274 -> 172 ms, self time per node 550 -> 251 us).
- perf(parser): parse a shared viewTemplate in place when it has no slots and no overrides,
  instead of deep-copying the JSON just to inject the instance id.
- perf(parser): validate string and integer style fields from the value already located, dropping
  the duplicate key lookup per field.
- feat(parser): read JSON assets from a packed `assets.bundle` when
  `BROOKESIA_GUI_INTERFACE_ENABLE_ASSET_BUNDLE` is enabled, replacing one open per asset with a
  single open plus a seek. Missing or malformed bundles fall back to the per-file path.

## v0.8.5 - 2026-11-10

### Bug Fixes:

- fix(logging): omit string values and UI text from data-store, binding, and widget logs to protect credentials.

## v0.8.4 - 2026-10-27

### Enhancements:

- feat(parser): add per-flow and per-screen stage timings to the PROFILE log so document load can be attributed to individual assets.
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
