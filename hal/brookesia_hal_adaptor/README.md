# ESP-Brookesia HAL Adaptor

* [中文版本](./README_CN.md)

## Overview

`brookesia_hal_adaptor` is the board-level HAL adaptor for ESP-Brookesia. It builds on the device/interface model from `brookesia_hal_interface`, initializes real peripherals through `esp_board_manager`, and registers audio, display, and storage capabilities in the global HAL table so upper layers can discover and use them by name.

For more information, see the [ESP-Brookesia Programming Guide](https://docs.espressif.com/projects/esp-brookesia/en/latest/hal/adaptor.html).

## How to Use

### Environment Requirements

Please refer to the following documentation:

- [ESP-Brookesia Programming Guide - Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia Programming Guide - Development Environment Setup](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-dev-environment)

### Add to Your Project

Please refer to [ESP-Brookesia Programming Guide - How to Obtain and Use Components](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-component-usage).

## Audio Processor Recording

`BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_RECORDING_IMPL` defaults to enabled when the audio processor is enabled. Disable it for playback-only firmware to omit the high-level `EncoderIface` and `DecoderIface` capabilities and their AFE wake-word code. `PlaybackIface`, `CodecPlayerIface`, and `CodecRecorderIface` remain available. Playback still depends on the `av_processor` component.

## Audio Processor Format

Before opening codecs that share an I2S clock, the audio processor aligns playback sample rate, bit depth, and channel count with the recorder's declared format. The codec and processor metadata use that effective format; the caller's requested playback configuration remains unchanged. Set the recorder format before initialization when playback needs a specific shared-clock format.

## Audio/Video Dependencies and Configuration

The audio processor uses av_processor 0.7.0 with a compatible GMF/ESP-SR dependency set, including the old-P4 / IDF 6 speech library. Dependency versions and patch provenance are recorded in [SOURCE.md](SOURCE.md) and [idf_component.yml](idf_component.yml). The [dependency patch reference](https://docs.espressif.com/projects/esp-brookesia/en/latest/hal/adaptor.html#hal-adaptor-dependency-patches) lists the feature-specific patches and their failure behavior. media_lib_sal 1.0.2 includes the ESP-IDF 6 TLS fix, replacing the former local patch.

Audio-only projects must set `CONFIG_VIDEO_PROCESSOR_ENABLE=n` in `sdkconfig.defaults` or their existing sdkconfig before dependency resolution, and disable the HAL camera, encoder, and decoder implementations. av_processor 0.7.0 enables video by default upstream. Enabling a HAL video encoder or decoder selects its required av_processor video pipeline. The Audio example's ESP32-C5 profile leaves video disabled; full Super/P4 retains video support.

### Additional ESP-DL Models

ESP-DL is pinned to 3.3.13. HAL registers the SR 2.5.3 models using the upstream SR 2.5.5 compile requirements. Registering any requirements enables global operator/kernel pruning (`DL_COMPILE_ALL=0`), so every native component loading additional `.espdl` models must register its own requirements. ESP-DL combines them with the SR requirements.

Declare `espressif/esp-dl` as a dependency of the model component. After `idf_component_register()` in its `CMakeLists.txt`, register the requirement file using the resolved ESP-DL component name:

```cmake
idf_build_get_property(build_components BUILD_COMPONENTS)
if("espressif__esp-dl" IN_LIST build_components)
    idf_component_get_property(
        dl_dir espressif__esp-dl COMPONENT_DIR)
else()
    idf_component_get_property(
        dl_dir esp-dl COMPONENT_DIR)
endif()
include("${dl_dir}/cmake/compile_register.cmake")
esp_dl_compile_add_yml(
    "${CMAKE_CURRENT_LIST_DIR}/model.compile_req.yml")
```

Derive the operators and kernels from each model. A model requiring only `Relu` can use the following requirement file:

```yaml
kernel_abi: 1
ops: [Relu]
kernels: []
```

ESP-DL 3.3.13 rejects unknown operators, a `kernel_abi` newer than its supported ABI, or conflicting ABI values across requirement files. It warns about unknown kernels and filters them out. To run the compatibility checks, see [Dependency Regressions](test_apps/host/README.md#dependency-regressions).

## ADC Compatibility Patches

When the SDK no longer defines `SOC_ADC_CHANNEL_NUM`, feature-enabled ADC integrations use the equivalent ADC LL channel count. The Board Manager and codec patches preserve the existing ADC bounds checks; a required patch mismatch stops configuration. See the [patch list](https://docs.espressif.com/projects/esp-brookesia/en/latest/hal/adaptor.html#hal-adaptor-dependency-patches) and [source provenance](SOURCE.md).

## Extending Board Support

A custom device may still hold resources after initialization or release fails. Board adaptors can register cleanup callbacks for HAL to invoke on later release requests. Applications using an existing board do not need to register these callbacks themselves.

When adding a device, see the [cleanup registration interface](include/brookesia/hal_adaptor/board_manager.h) for registration and callback requirements.
