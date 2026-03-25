# ESP-Brookesia HAL Adaptor

* [English Version](./README.md)

## 概述

`brookesia_hal_adaptor` 为 `brookesia_hal_interface` 提供开箱即用的板级设备实现。它将 Brookesia HAL 接口与 `esp_board_manager` 暴露的外设连接起来，使上层可以通过类型化接口直接使用音频和显示能力。

本组件当前提供：

- **音频设备适配器**：注册 `AudioDevice`，实现 `AudioPlayerIface` 和 `AudioRecorderIface`。
- **LCD 显示设备适配器**：注册 `LcdDisplay`，实现 `DisplayPanelIface` 和 `DisplayTouchIface`。
- **板级设备集成**：封装 `esp_board_manager` 提供的 `audio_dac`、`audio_adc`、`display_lcd`、`lcd_brightness` 和 `lcd_touch`。
- **注册表统一初始化**：可配合 `Device::init_device_from_registry()` 在启动阶段统一初始化。

## 目录

- [ESP-Brookesia HAL Adaptor](#esp-brookesia-hal-adaptor)
  - [概述](#概述)
  - [目录](#目录)
  - [功能特性](#功能特性)
    - [内置设备](#内置设备)
    - [板级要求](#板级要求)
    - [典型使用方式](#典型使用方式)
  - [开发环境要求](#开发环境要求)
  - [添加到工程](#添加到工程)
  - [快速上手](#快速上手)

## 功能特性

### 内置设备

| 设备名 | 实现接口 | 对应的 board-manager 设备 | 说明 |
|--------|----------|---------------------------|------|
| `AudioDevice` | `AudioPlayerIface`、`AudioRecorderIface` | `audio_dac`、`audio_adc` | 暴露编解码句柄，并按当前板卡填充录音默认配置。 |
| `LcdDisplay` | `DisplayPanelIface`、`DisplayTouchIface` | `display_lcd`、`lcd_brightness`、`lcd_touch` | 提供显示分辨率、位图刷新、触摸句柄访问、回调注册和背光控制。 |

### 板级要求

- **音频适配器** 当前为以下板卡提供录音默认配置：
  - `esp32_s3_korvo2_v3`
  - `esp_vocat_board_v1_0`
  - `esp_vocat_board_v1_2`
  - `esp_box_3`
  - `esp32_s3_korvo2l_v1`
  - `esp32_p4_function_ev`
  - `esp_sensair_shuttle`
- **显示适配器** 要求当前板卡定义提供 `display_lcd`、`lcd_brightness` 和 `lcd_touch`。
- 如果缺少必须的板级设备，或者当前音频板卡不在支持列表中，`Device::init_device_from_registry()` 会初始化失败。

### 典型使用方式

- `service/brookesia_service_audio` 可以直接从注册表获取 `AudioPlayerIface` 和 `AudioRecorderIface`，无需直接访问板级驱动。
- 带显示功能的应用可以获取 `DisplayPanelIface` 和 `DisplayTouchIface`，访问分辨率、刷屏回调和触摸句柄。
- 应用通常只需要在启动阶段统一初始化一次所有已注册设备。

## 开发环境要求

使用本组件前，请确保已安装以下 SDK 开发环境：

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`
- `espressif/esp_board_manager`: `0.5.*`
- `espressif/brookesia_hal_interface`

> [!NOTE]
> SDK 的安装方法请参阅 [ESP-IDF 编程指南 - 安装](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

## 添加到工程

您可以通过 [Espressif 组件库](https://components.espressif.com/) 将 `brookesia_hal_adaptor` 添加到工程中：

1. **使用命令行**

   在工程目录下运行以下命令：

   ```bash
   idf.py add-dependency "espressif/brookesia_hal_adaptor"
   ```

2. **修改配置文件**

   在工程目录下创建或修改 *idf_component.yml* 文件：

   ```yaml
   dependencies:
     espressif/brookesia_hal_adaptor: "*"
   ```

详细说明请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。

## 快速上手

```cpp
#include "brookesia/hal_interface.hpp"

using namespace esp_brookesia::hal;

extern "C" void app_main(void)
{
    Device::init_device_from_registry();

    auto *player = get_interface<AudioPlayerIface>("AudioDevice");
    auto *display = get_interface<DisplayPanelIface>("LcdDisplay");

    if (player != nullptr) {
        player->set_volume(75);
    }
    if (display != nullptr) {
        display->set_backlight(100);
    }
}
```
