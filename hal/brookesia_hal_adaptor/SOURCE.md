# Source provenance

The files under `tools/` are local compatibility patches and compile requirements used
with resolved ESP-IDF Component Manager dependencies during CMake configuration. They do not
bundle the complete upstream components and do not add a runtime network
dependency.

| Patch | Validated upstream source | Upstream license |
| --- | --- | --- |
| `av_processor_frame_mode_stop.patch` | [`jason-mao/av_processor` 0.7.0](https://components.espressif.com/components/jason-mao/av_processor/versions/0.7.0), ESP-ADF `components/av_processor/src/video_processor.c` | `LicenseRef-Espressif-Modified-MIT` |
| `esp_board_manager_periph_deinit_retry.patch`, `esp_board_manager_dvp_camera_deinit.patch` | [`espressif/esp_board_manager` 0.5.15](https://components.espressif.com/components/espressif/esp_board_manager/versions/0.5.15), repository commit `cbee842b9eb75cf86b731f1933f71adee9e41d7f` | `LicenseRef-Espressif-Modified-MIT` |
| `esp_video_dvp_deinit_order.patch` | [`espressif/esp_video` 2.4.1](https://components.espressif.com/components/espressif/esp_video/versions/2.4.1), repository commit `67a555a517b7aa4753836432453659abfaca393a` | ESPRESSIF MIT |
| `esp_capture_v4l2_uyvy_support.patch` | [`espressif/esp_capture` 1.0.2](https://components.espressif.com/components/espressif/esp_capture/versions/1.0.2), `impl/capture_video_src/capture_video_v4l2_src.c` | `LicenseRef-Espressif-Modified-MIT` |
| `esp_board_manager_adc_idf6.patch` | [`espressif/esp_board_manager` 0.5.15](https://components.espressif.com/components/espressif/esp_board_manager/versions/0.5.15), repository commit `cbee842b9eb75cf86b731f1933f71adee9e41d7f`, `peripherals/periph_adc/periph_adc.c` | `LicenseRef-Espressif-Modified-MIT` |
| `esp_codec_dev_adc_idf6.patch` | [`espressif/esp_codec_dev` 1.5.11](https://components.espressif.com/components/espressif/esp_codec_dev/versions/1.5.11), `platform/audio_codec_data_adc.c` | Apache-2.0 |
| `esp_sr_2_5_3_compile_req.yml` | [`espressif/esp-sr` 2.5.5](https://components.espressif.com/components/espressif/esp-sr/versions/2.5.5), `tool/esp-sr-req.yml`; compile-requirement metadata backported for the unchanged SR 2.5.3 models | `LicenseRef-Espressif-Modified-MIT` |

media_lib_sal 1.0.2 contains the upstream ESP-IDF 6 TLS fix; its former local patch is retired.

The patches retain the licensing terms of their respective upstream source.
In particular, the Espressif Modified MIT and ESPRESSIF MIT components are
licensed for use with Espressif products. See each resolved component's
`LICENSE` file for the complete terms.
