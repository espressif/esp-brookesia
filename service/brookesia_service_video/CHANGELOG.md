# ChangeLog

## v0.7.0 - 2026-04-10

### Initial Release

- feat(repo): add `brookesia_service_video` component providing `VideoEncoder` and `VideoDecoder` service wrappers for the ESP-Brookesia service framework
- feat(encoder): add `VideoEncoder` service class with `Open` (multi-sink config), `Close`, `Start`, `Stop`, and `FetchFrame` functions
- feat(encoder): support sink output formats: `H264`, `MJPEG`, `RGB565`, `RGB888`, `BGR888`, `YUV420`, `YUV422`, `O_UYY_E_VYY`
- feat(encoder): add `EncoderSinkInfo` (format, width, height, fps) and `EncoderConfig` (sinks list, stream mode) types
- feat(encoder): each `VideoEncoder` instance is bound to a device path and an integer slot id
- feat(decoder): add `VideoDecoder` service class with `Open`, `Close`, `Start`, `Stop`, and `FeedFrame` functions
- feat(decoder): support compressed input formats: `H264`, `MJPEG`
- feat(decoder): support pixel output formats: `RGB565_LE/BE`, `RGB888`, `BGR888`, `YUV420P`, `YUV422P`, `YUV422`, `UYVY422`, `O_UYY_E_VYY`
- feat(helper): add `Video`, `VideoEncoder<N>`, `VideoDecoder<N>` helper definitions to `service_helper/video.hpp`
- feat(deps): depend on `jason-mao/av_processor 0.6.*` (private) and `espressif/brookesia_service_helper 0.7.*` (public)
