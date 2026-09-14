# Source provenance

The files under `tools/` are local compatibility patches applied to resolved
ESP-IDF Component Manager dependencies during CMake configuration. They do not
bundle the complete upstream components and do not add a runtime network
dependency.

| Patch | Validated upstream source | Upstream license |
| --- | --- | --- |
| `media_lib_sal_esp_tls_idf6.patch` | [`espressif/media_lib_sal` 0.9.3](https://components.espressif.com/components/espressif/media_lib_sal/versions/0.9.3), repository commit `8603e88bf29cab414a1a461ded5e07bcba05bf58`, `media_lib_sal/port/media_lib_tls_default.c` | `LicenseRef-Espressif-Modified-MIT` |
| `av_processor_frame_mode_stop.patch` | [`jason-mao/av_processor` 0.6.6](https://components.espressif.com/components/jason-mao/av_processor/versions/0.6.6), ESP-ADF `components/av_processor/src/video_processor.c` | `LicenseRef-Espressif-Modified-MIT` |
| `esp_board_manager_device_lifecycle_lock.patch`, `esp_board_manager_periph_deinit_retry.patch`, `esp_board_manager_dvp_camera_deinit.patch` | [`espressif/esp_board_manager` 0.5.15](https://components.espressif.com/components/espressif/esp_board_manager/versions/0.5.15), repository commit `cbee842b9eb75cf86b731f1933f71adee9e41d7f` | `LicenseRef-Espressif-Modified-MIT` |
| `esp_video_dvp_deinit_order.patch`, `esp_video_dvp_detect_required.patch` | [`espressif/esp_video` 2.4.1](https://components.espressif.com/components/espressif/esp_video/versions/2.4.1), repository commit `67a555a517b7aa4753836432453659abfaca393a` | ESPRESSIF MIT |
| `esp_capture_v4l2_uyvy_support.patch` | [`espressif/esp_capture` 0.8.4](https://components.espressif.com/components/espressif/esp_capture/versions/0.8.4), `impl/capture_video_src/capture_video_v4l2_src.c` | `LicenseRef-Espressif-Modified-MIT` |

The patches retain the licensing terms of their respective upstream source.
In particular, the Espressif Modified MIT and ESPRESSIF MIT components are
licensed for use with Espressif products. See each resolved component's
`LICENSE` file for the complete terms.
