# ChangeLog

## v0.8.0 - 2026-11-10

### Breaking Changes:

- break(session): require owners for borrowed display, callback, and generated configuration data.

### Enhancements:

- feat(gsp): add GSP sessions and application hosting on borrowed HAL display and touch devices.
- feat(bundle): load deployable bundles and retain owned resources until session shutdown succeeds.
- feat(bundle): read into aligned bundle storage and inspect scene and component metadata without a running session.
- feat(linux): add desktop display sessions through the official ESP-GSP 1.4 simulator bridge.
- feat(linux): validate desktop bundle size, CRC, and API metadata before session startup.
- feat(build): add optional zlib bundle compression with validated PSRAM decompression on ESP.
- feat(build): share theme preparation and common font resources across consuming components.

### Documentation:

- docs(lifecycle): document borrowed-device lifetimes and cleanup after failed starts or stops.
