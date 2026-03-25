# ESP-Brookesia HAL Interface

* [English Version](./README.md)

## 概述

`brookesia_hal_interface` 是 ESP-Brookesia 生态中的硬件抽象基础组件。它定义统一的 `Device` 和 `Interface` 模型，使板级实现只需注册一次，上层即可通过稳定、类型化的 API 访问硬件能力。

本组件主要提供：

- **设备生命周期模型**：通过 `Device` 统一 `probe()`、`check_initialized()`、`init()` 和 `deinit()` 流程。
- **插件式注册表**：设备可以通过 Brookesia 插件机制注册，并批量初始化。
- **类型化查找辅助函数**：支持 `get_device()`、`get_interface<T>()`、`get_interfaces<T>()` 和 `get_first_interface<T>()`。
- **可复用接口定义**：常见音频、显示、存储和状态灯接口位于 `include/brookesia/hal_interface/interfaces/`。
- **回归测试**：内置 Unity 和 pytest，用于覆盖注册表、接口查找和公开 API 行为。

## 目录

- [ESP-Brookesia HAL Interface](#esp-brookesia-hal-interface)
  - [概述](#概述)
  - [目录](#目录)
  - [功能特性](#功能特性)
    - [设备与注册表模型](#设备与注册表模型)
    - [内置接口目录](#内置接口目录)
    - [测试覆盖](#测试覆盖)
  - [开发环境要求](#开发环境要求)
  - [添加到工程](#添加到工程)
  - [快速上手](#快速上手)

## 功能特性

### 设备与注册表模型

- **`Device`** 定义统一的 HAL 生命周期和类型化查询入口。
- **`DeviceImpl<Derived>`** 提供 CRTP 辅助基类，用于把一个设备映射到一个或多个接口。
- **`Device::init_device_from_registry()`** 支持一次性初始化所有已注册设备。
- **`get_device()` / `get_interface<T>()` / `get_interfaces<T>()` / `get_first_interface<T>()`** 提供上层服务常用的查找路径。

### 内置接口目录

| 头文件 | 接口 | 说明 |
|--------|------|------|
| `status_led.hpp` | `StatusLedIface` | 状态灯闪烁控制，包含预定义闪烁类型。 |
| `storage_fs.hpp` | `StorageFsIface` | 文件系统相关能力，例如挂载路径和文件系统类型。 |
| `display_panel.hpp` | `DisplayPanelIface` | 显示分辨率、位图刷新、回调注册和背光控制。 |
| `display_touch.hpp` | `DisplayTouchIface` | 显示触摸设备句柄访问。 |
| `audio_player.hpp` | `AudioPlayerIface` | 播放句柄和播放器配置访问。 |
| `audio_recorder.hpp` | `AudioRecorderIface` | 录音句柄和录音配置访问。 |

### 测试覆盖

`test_apps` 目录当前覆盖了以下内容：

- FNV-1a 接口哈希辅助函数
- 注册表初始化和接口过滤逻辑
- `DeviceImpl` 的接口查询行为
- 状态灯、音频和显示接口访问器
- `esp32s3` 和 `esp32p4` 的 pytest 目标

## 开发环境要求

使用本组件前，请确保已安装以下 SDK 开发环境：

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> SDK 的安装方法请参阅 [ESP-IDF 编程指南 - 安装](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

## 添加到工程

`brookesia_hal_interface` 已上传到 [Espressif 组件库](https://components.espressif.com/)，您可以通过以下方式将其添加到工程中：

1. **使用命令行**

   在工程目录下运行以下命令：

   ```bash
   idf.py add-dependency "espressif/brookesia_hal_interface"
   ```

2. **修改配置文件**

   在工程目录下创建或修改 *idf_component.yml* 文件：

   ```yaml
   dependencies:
     espressif/brookesia_hal_interface: "*"
   ```

详细说明请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。

## 快速上手

```cpp
#include "brookesia/hal_interface.hpp"

using namespace esp_brookesia::hal;

class DemoLedDevice : public DeviceImpl<DemoLedDevice>, public StatusLedIface {
public:
    DemoLedDevice(): DeviceImpl<DemoLedDevice>("demo_led") {}

    bool probe() override { return true; }
    bool check_initialized() const override { return true; }
    bool init() override { return true; }
    bool deinit() override { return true; }
    void start_blink(BlinkType type) override { (void)type; }
    void stop_blink(BlinkType type) override { (void)type; }

private:
    void *query_interface(uint64_t id) override
    {
        return build_table<StatusLedIface>(id);
    }
};

BROOKESIA_PLUGIN_REGISTER(Device, DemoLedDevice, "demo_led");
```
