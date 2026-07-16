# ChangeLog

## v0.8.0 - 2026-07-07

### Enhancements:

- feat(picodet): initial PicoDet object-detection service wrapping esp-dl ESPDet-Pico models
- feat(picodet): config-driven model loading from the filesystem (`manifest.json` + `.espdl`), model resident from Open to Close
- feat(picodet): service functions Open/Detect/Close/GetInfo with cache-safe file IO on PSRAM-stacked workers
- feat(picodet): Attach/Detach pipeline-node wiring — consumes VideoEncoder0 frame events (no camera HAL access), runs decimated inference, optionally presents annotated frames to a display output, and publishes DetectionUpdated events
