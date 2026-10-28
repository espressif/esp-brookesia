# ChangeLog

## v0.9.0 - 2026-10-28

### Breaking Changes:

- break(deps): move to the shared Brookesia 0.9 line and use matching 0.9.x component dependencies.

### Enhancements:

- feat(picodet): present annotated frames via visual DataFlow instead of directly linking the Display service.

### Bug Fixes:

- fix(pc): derive component version metadata from the manifest.
- fix(build): suppress the esp-dl 3.3.6 anonymous typedef warning for PicoDet sources.
- fix(deps): declare the Service Manager dependency required for visual DataFlow.

## v0.8.0 - 2026-07-07

### Enhancements:

- feat(picodet): initial PicoDet object-detection service wrapping esp-dl ESPDet-Pico models
- feat(picodet): config-driven model loading from the filesystem (`manifest.json` + `.espdl`), model resident from Open to Close
- feat(picodet): service functions Open/Detect/Close/GetInfo with cache-safe file IO on PSRAM-stacked workers
- feat(picodet): Attach/Detach pipeline-node wiring — consumes VideoEncoder0 frame events (no camera HAL access), runs decimated inference, optionally presents annotated frames through the visual DataFlow interface, and publishes DetectionUpdated events
