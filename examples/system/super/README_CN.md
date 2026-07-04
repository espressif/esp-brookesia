# System Super 示例

[English Version](./README.md)

本示例演示如何在 ESP-Brookesia 中启动一个完整的 System Super 产品壳。示例集成 HAL、Display/Audio/Wi-Fi/HTTP/Storage/SNTP/Video/Device 等服务、GUI LVGL 后端、JavaScript 运行时，以及内置 Settings、App Store 和 Files 应用，用于验证应用生命周期、资源打包、系统 overlay 和 launcher 流程。

## 📑 目录

- [System Super 示例](#system-super-示例)
  - [📑 目录](#-目录)
  - [✨ 功能特性](#-功能特性)
  - [🚩 快速入门](#-快速入门)
    - [硬件要求](#硬件要求)
    - [开发环境](#开发环境)
  - [🔨 如何使用](#-如何使用)
  - [🚀 快速体验](#-快速体验)
  - [🔍 故障排除](#-故障排除)
  - [💬 技术支持与反馈](#-技术支持与反馈)

## ✨ 功能特性

- 🧭 **系统壳体验**：启动 System Super Shell，展示桌面背景、状态栏、App Launcher 和系统 overlay
- 📦 **内置应用集成**：预装 Settings、App Store 和 Files，验证 native app 安装、启动和恢复流程
- 🧩 **框架联动**：组合 Service Manager、GUI LVGL、Runtime Manager、System Core/Super 和 HAL 板级资源
- 🗂️ **资源打包**：通过构建流程 stage System Super 资源、字体、图片和 LittleFS 分区

## 🚩 快速入门

### 硬件要求

本示例当前仅支持以下开发板：

- `esp32_p4x_function_ev`
- `esp32_s31_korvo1`

硬件资源通过 [brookesia_hal_boards](https://components.espressif.com/components/espressif/brookesia_hal_boards) 组件管理。

> [!TIP]
> 示例支持使用 SD Card 作为外部存储卷，请在上电前将 SD Card 插入开发板。
> 使用 SD Card 后，"应用市场" 等应用会默认将其作为存储或特定文件的扫描目录，因此推荐使用其来扩展系统存储空间。

### 开发环境

请参考以下文档：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 开发环境搭建](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-dev-environment)

## 🔨 如何使用

<a href="https://espressif.github.io/esp-brookesia/index.html">
  <img alt="Try it with ESP Launchpad" src="https://dl.espressif.com/AE/esp-dev-kits/new_launchpad.png" width="400">
</a>

请参考 [ESP-Brookesia 编程指南 - 如何使用示例工程](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-example-projects)。

## 🚀 快速体验

固件烧录成功后，设备会初始化通用服务、启动显示后端并进入 System Super Shell。正常启动后可以看到 App Launcher 和系统状态栏；点击内置应用图标可启动对应 app，普通 app 前台时可通过底部上滑手势返回 launcher。

串口日志出现以下内容表示系统初始化和示例 smoke 流程完成：

```text
=== System Example Completed ===
```

## 🔍 故障排除

**启动后界面空白**

确认 LittleFS 分区已烧录，并检查构建产物中是否生成 `littlefs_data.bin`。System Super 依赖构建流程 stage `system/super`、`system/fonts` 和 `apps` 资源。

**Shell 或内置应用启动失败**

确认 `brookesia_system_super`、`brookesia_system_core`、`brookesia_gui_lvgl`、相关服务组件和内置 app 组件均使用同一发布版本线。

**无法通过手势退出普通应用**

确认 Display service 已正常启动并上报触摸手势，且手势从屏幕底部边缘开始向上滑动。

## 💬 技术支持与反馈

请通过以下渠道进行反馈：

- 有关技术问题，请访问 [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) 论坛
- 有关功能请求或错误报告，请创建新的 [GitHub 问题](https://github.com/espressif/esp-brookesia/issues)
