# ESP-Brookesia HAL Adaptor

* [English Version](./README.md)

## 概述

`brookesia_hal_adaptor` 是 ESP-Brookesia 的板级 HAL 适配实现，基于 `brookesia_hal_interface` 的设备/接口模型，通过 `esp_board_manager` 初始化真实外设，并将音频、显示、存储三类能力注册到全局 HAL 表，供上层按名称发现和使用。

更多信息请参考 [ESP-Brookesia 编程指南](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/hal/adaptor.html)。

## 如何使用

### 开发环境要求

请参考以下文档：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 开发环境搭建](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-dev-environment)

### 添加到工程

请参考 [ESP-Brookesia 编程指南 - 如何获取和使用组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-component-usage)。

## 音频处理器录制功能

启用音频处理器时，`BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_RECORDING_IMPL` 默认开启。仅需播放的固件可关闭该选项，移除高层 `EncoderIface`、`DecoderIface` 能力及其 AFE 唤醒词代码。`PlaybackIface`、`CodecPlayerIface` 和 `CodecRecorderIface` 仍可用。播放功能仍依赖 `av_processor` 组件。

## 音频处理器格式

打开共享 I2S 时钟的 codec 前，音频处理器按录音接口声明的格式统一播放采样率、位深和声道。codec 和处理器元数据均使用这一实际格式，调用者请求的播放配置保持不变。若播放需要特定的共享时钟格式，请在初始化前设置录音格式。

打开 `PlaybackIface` 后会保留一个使用 8 KiB 内部内存栈的清理任务，直到播放关闭。如果无法创建该任务，`open()` 会失败。从播放回调调用的 `close()` 会异步完成；清理完成前重新打开可能失败。

## 音视频依赖与配置

音频处理器使用 av_processor 0.7.0 及兼容的 GMF/ESP-SR 依赖组合，包含旧 P4 / IDF 6 语音库。依赖版本和补丁来源记录在 [SOURCE.md](SOURCE.md) 与 [idf_component.yml](idf_component.yml)；[依赖补丁参考](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/hal/adaptor.html#hal-adaptor-dependency-patches)列出按功能应用的补丁及失败行为。media_lib_sal 1.0.2 支持 ESP-IDF 6 TLS 接口。

仅使用音频的工程须在依赖解析前通过 `sdkconfig.defaults` 或已有 sdkconfig 设置 `CONFIG_VIDEO_PROCESSOR_ENABLE=n`，同时关闭 HAL 的摄像头、编码器和解码器实现。av_processor 0.7.0 上游默认启用视频。启用 HAL 视频编码器或解码器时，会同时启用其必需的 av_processor 视频管线。无摄像头板级配置（`esp_sensair_shuttle`、`esp_box_3`、`esp_vocat_board_v1_0`、`esp_vocat_board_v1_2`、`rymcu_bigsmart`、`esp32_s3_touch_amoled_*`）以及 Audio/Console 示例的 ESP32-C5 profile 默认设置 `CONFIG_VIDEO_PROCESSOR_ENABLE=n`；带摄像头的板级配置（`esp32_p4_function_ev`、`esp32_p4x_function_ev`、`esp32_s31_korvo1`、`esp32_s3_korvo2_v3`、`esp_mosaico_*`）保留视频能力。

### 额外 ESP-DL 模型

ESP-DL 固定为 3.3.13。HAL 使用上游 SR 2.5.5 算子需求元数据，为 SR 2.5.3 模型注册需求。登记任意需求后，会全局启用算子和内核裁剪（`DL_COMPILE_ALL=0`），因此每个额外加载 `.espdl` 模型的 native 组件都须注册自身需求，由 ESP-DL 与 SR 需求取并集。

将 `espressif/esp-dl` 声明为模型组件的依赖。在该组件的 `CMakeLists.txt` 中，先调用 `idf_component_register()`，再按解析出的 ESP-DL 组件名称注册需求文件：

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

请按各模型确定算子与内核。仅需 `Relu` 的模型可使用以下需求文件：

```yaml
kernel_abi: 1
ops: [Relu]
kernels: []
```

ESP-DL 3.3.13 会拒绝未知算子、超出其支持范围的 `kernel_abi`，以及多个需求文件间不一致的 ABI 值；未知 kernel 则会告警并被过滤。兼容性检查的运行方法见[依赖回归](test_apps/host/README_CN.md#依赖回归)。

## ADC 兼容补丁

SDK 不再定义 `SOC_ADC_CHANNEL_NUM` 时，已启用 ADC 的集成使用等价 ADC LL 通道数。Board Manager 和 codec 补丁保留既有 ADC 边界校验；必要补丁不匹配会中止配置。请参阅[补丁列表](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/hal/adaptor.html#hal-adaptor-dependency-patches)与[来源记录](SOURCE.md)。

## 扩展板级适配

自定义设备在初始化或释放失败后，可能仍持有需要清理的资源。板级适配可以注册清理回调，由 HAL 在后续释放请求中调用。使用已有板型的应用无需自行注册。

新增设备适配时，请参阅[设备清理注册接口](include/brookesia/hal_adaptor/board_manager.h)，了解注册方式和回调要求。
