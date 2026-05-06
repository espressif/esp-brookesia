# 设备服务示例

[English Version](./README.md)

本示例演示如何在 ESP-Brookesia 框架中使用设备服务（`brookesia_service_device`）。该服务作为应用层访问 HAL 设备能力的统一入口，主要覆盖开发板能力查询、设备控制、状态获取和事件验证等场景。

## 📑 目录

- [设备服务示例](#设备服务示例)
  - [📑 目录](#-目录)
  - [✨ 功能特性](#-功能特性)
  - [🚩 快速入门](#-快速入门)
    - [硬件要求](#硬件要求)
    - [开发环境](#开发环境)
  - [🔨 如何使用](#-如何使用)
  - [🚀 运行说明](#-运行说明)
    - [能力查询](#能力查询)
    - [开发板信息](#开发板信息)
    - [显示控制](#显示控制)
    - [音频控制](#音频控制)
    - [存储查询](#存储查询)
    - [电池状态与充电控制](#电池状态与充电控制)
    - [重置数据](#重置数据)
  - [🔍 故障排除](#-故障排除)
  - [💬 技术支持与反馈](#-技术支持与反馈)

## ✨ 功能特性

- 🧩 **能力自适应**：通过 `GetCapabilities` 获取当前开发板实际支持的 HAL interface，并只运行受支持的演示
- 🧾 **开发板信息**：当 `BoardInfo` 可用时读取开发板名称、芯片、版本、描述和厂商信息
- 💡 **显示控制**：当 `DisplayBacklight` 可用时演示背光亮度、开关状态读取与设置
- 🖼️ **显示面板验证**：当 `DisplayPanel` 可用时通过 HAL 直接绘制像素位彩条验证画面
- 🔊 **音频控制**：当 `AudioCodecPlayer` 可用时演示音量、静音状态读取与设置，并播放 SPIFFS 中的 PCM 文件
- 💾 **存储查询**：当 `StorageFs` 可用时查询已挂载文件系统列表
- 🔋 **电池状态**：当 `Power:Battery` 可用时查询电池信息、运行状态和充电配置
- 📡 **事件验证**：使用 `DeviceHelper::EventMonitor` 等待并检查显示、音频和电池操作触发的事件
- 🧹 **状态重置**：调用 `ResetData` 清除 device service 持久化状态，并验证恢复后的显示和音频状态

## 🚩 快速入门

### 硬件要求

本示例通过 [brookesia_hal_boards](https://components.espressif.com/components/espressif/brookesia_hal_boards) 组件管理硬件，支持符合以下条件的开发板：

- Flash >= 16MB
- 已通过 board manager 配置对应开发板
- 至少支持以下一个或多个 HAL interface：

  - `BoardInfo`
  - `DisplayBacklight`
  - `DisplayPanel`
  - `AudioCodecPlayer`
  - `StorageFs`
  - `Power:Battery`

本示例会根据实际能力自动跳过不支持的功能。请参考 [ESP-Brookesia 编程指南 - 支持的开发板](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/hal/boards/index.html#hal-boards-sec-02) 获取支持的开发板列表。

### 开发环境

请参考以下文档：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 开发环境搭建](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-dev-environment)

## 🔨 如何使用

请参考 [ESP-Brookesia 编程指南 - 如何使用示例工程](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-example-projects)。

> [!NOTE]
> 示例使用 `EXAMPLE_TEST_ACTION_DELAY_MS` 控制各测试动作之间的等待时间，默认值为 `2000` ms。需要放慢或加快演示节奏时，可在编译配置中覆盖该宏。

## 🚀 运行说明

示例烧录后，程序将初始化所有 HAL devices，启动 `ServiceManager`，绑定 `NVSHelper` 与 `DeviceHelper`，然后按顺序自动执行以下演示。所有输出通过串口打印。

### 能力查询

首先调用 `GetCapabilities` 获取当前开发板实际注册的 HAL interface，并打印能力矩阵：

```text
[Demo: Capabilities]
  Raw HAL capabilities reported by the current board
  Device helper feature matrix
```

后续演示均根据能力矩阵决定是否运行。不支持的功能会打印 `skip` 日志，而不会导致示例失败。

### 开发板信息

当 `BoardInfo` 可用时，调用 `GetBoardInfo` 并打印开发板静态信息：

```text
[Demo: Board Info]
  name / chip / version / description / manufacturer
```

### 显示控制

当 `DisplayBacklight` 可用时，演示背光亮度和开关控制：

```text
[Demo: Display Controls]
  读取当前亮度和开关状态
  若背光关闭则先打开
  若 DisplayPanel 可用则根据像素格式绘制像素位彩条验证画面
  修改亮度并通过 EventMonitor 等待 BrightnessChanged 事件
  关闭背光并等待 OnOffChanged(false) 事件
  重新打开背光并等待 OnOffChanged(true) 事件
  再次读取状态进行验证
```

### 音频控制

当 `AudioCodecPlayer` 可用时，演示音量和静音控制。示例会从 SPIFFS 分区读取 `/spiffs/audio.pcm`，并按 `16 kHz / 16-bit / mono` 打开播放器：

```text
[Demo: Audio Controls]
  读取当前音量和静音状态
  若当前静音则先取消静音
  修改音量并通过 EventMonitor 等待 VolumeChanged 事件
  播放 audio.pcm
  设置静音并等待 MuteChanged(true) 事件
  播放静音状态下的 audio.pcm
  取消静音并等待 MuteChanged(false) 事件
  再次播放 audio.pcm
```

> [!NOTE]
> `main/CMakeLists.txt` 会通过 `spiffs_create_partition_image(spiffs_data ../spiffs FLASH_IN_PROJECT)` 将 `spiffs/audio.pcm` 打包进 SPIFFS 分区。

### 存储查询

当 `StorageFs` 可用时，调用 `GetStorageFileSystems` 并打印当前已挂载的文件系统信息，包括文件系统类型、介质类型和挂载点。

### 电池状态与充电控制

当 `Power:Battery` 可用时，演示电池相关查询与事件监控：

```text
[Demo: Power Battery]
  查询 PowerBatteryInfo
  查询 PowerBatteryState
  若支持 ChargeConfig，则查询 PowerBatteryChargeConfig
  若同时支持 ChargerControl 和 ChargeConfig，则刷新充电使能状态
  通过 EventMonitor 等待电池状态或充电配置事件
```

如果当前开发板不支持充电配置或充电控制，示例会打印 skip 日志并继续执行后续演示。

### 重置数据

最后调用 `ResetData` 重置 device service 的持久化状态：

```text
[Demo: Reset Data]
  调用 ResetData
  若显示背光可用，读取并打印恢复后的亮度与开关状态
  若音频播放器可用，读取并打印恢复后的音量与静音状态
```

## 🔍 故障排除

**部分演示被跳过**

- 这是正常行为。示例会根据 `GetCapabilities` 返回的能力矩阵自动跳过当前开发板不支持的接口。
- 检查开发板配置和 HAL adaptor Kconfig，确认目标接口已启用。

**无声音输出**

- 确认 SPIFFS 分区已成功烧录（`idf.py flash` 应包含分区表和 `spiffs_data` 分区内容）。
- 确认当前开发板支持 `AudioCodecPlayer`，并且功放/扬声器硬件正常。
- 检查串口日志中是否成功读取 `/spiffs/audio.pcm`。

**显示无变化**

- 确认当前开发板支持 `DisplayBacklight` 和/或 `DisplayPanel`。
- 如果只支持背光而不支持面板，示例只会演示亮度和开关控制，不会绘制像素位彩条验证画面。

**电池信息不可用**

- 确认当前开发板支持 `Power:Battery`。
- `ChargeConfig` 和 `ChargerControl` 是可选能力，不支持时相关演示会自动跳过。

**编译失败（VSCode）**

使用命令行安装 ESP-IDF，参考 [开发环境](#开发环境)。

## 💬 技术支持与反馈

请通过以下渠道进行反馈：

- 有关技术问题，请访问 [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) 论坛
- 有关功能请求或错误报告，请创建新的 [GitHub 问题](https://github.com/espressif/esp-brookesia/issues)

我们会尽快回复。
