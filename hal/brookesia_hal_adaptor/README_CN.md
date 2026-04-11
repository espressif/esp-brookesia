# ESP-Brookesia HAL Adaptor

* [English Version](./README.md)

## 概述

`brookesia_hal_adaptor` 是 ESP-Brookesia 的板级 HAL 适配实现，基于 `brookesia_hal_interface` 的设备/接口模型，通过 `esp_board_manager` 初始化真实外设，并将**音频**、**显示**、**存储**三类能力注册到全局 HAL 表，供上层按名称发现和使用。

## 目录

- [ESP-Brookesia HAL Adaptor](#esp-brookesia-hal-adaptor)
  - [概述](#概述)
  - [目录](#目录)
  - [功能特性](#功能特性)
  - [开发环境要求](#开发环境要求)
  - [添加到工程](#添加到工程)

## 功能特性

本组件提供三台板级设备，每台设备以单例形式注册，初始化后将对应的接口发布到全局表：

| 设备类（逻辑名） | 注册的接口实现 |
|------------------|----------------|
| `AudioDevice`（`"Audio"`） | Codec 播放（`CODEC_PLAYER_IMPL_NAME`）、Codec 录音（`CODEC_RECORDER_IMPL_NAME`） |
| `DisplayDevice`（`"Display"`） | LEDC 背光（`LEDC_BACKLIGHT_IMPL_NAME`）、LCD 面板（`LCD_PANEL_IMPL_NAME`）、LCD 触摸（`LCD_TOUCH_IMPL_NAME`） |
| `StorageDevice`（`"Storage"`） | 通用文件系统（`GENERAL_FS_IMPL_NAME`），支持 SPIFFS 与 SD 卡（FATFS） |

各子接口可在 `menuconfig` 的 **ESP-Brookesia: Hal Adaptor Configurations** 中独立开启或关闭，部分选项依赖板级能力（`ESP_BOARD_DEV_*`）；默认参数（音量范围、录音格式、背光亮度范围等）也可在 `menuconfig` 中调整，由 `macro_configs.h` 映射为编译宏供实现使用。

若需在初始化前覆盖默认能力参数，可在对应设备单例上调用 `set_codec_player_info`、`set_codec_recorder_info` 或 `set_ledc_backlight_info`。初始化完成后再调用通常不会生效。

> [!NOTE]
> 业务代码只需依赖 `brookesia_hal_interface` 的抽象类型，通过接口全名取回能力，无需直接包含本组件的私有头文件。

## 开发环境要求

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> SDK 的安装方法请参阅 [ESP-IDF 编程指南 - 安装](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)。

## 添加到工程

`brookesia_hal_adaptor` 已发布到 [Espressif 组件库](https://components.espressif.com/)，可通过以下方式添加：

1. **命令行**

   ```bash
   idf.py add-dependency "espressif/brookesia_hal_adaptor"
   ```

2. **idf_component.yml**

   ```yaml
   dependencies:
     espressif/brookesia_hal_adaptor: "*"
   ```

详细说明请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。
