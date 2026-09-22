# System Super 示例

[English Version](./README.md)

本示例演示如何在 ESP-Brookesia 中启动完整的 System Super 产品壳。默认构建集成 HAL、Display/Audio/Wi-Fi/HTTP/Storage/SNTP/Video/Device 等服务、NES 模拟器、Coze 和 Xiaozhi agent、GUI LVGL 后端、JavaScript 运行时，以及内置 Settings、App Store 和 Files 应用。

## 📑 目录

- [System Super 示例](#system-super-示例)
  - [📑 目录](#-目录)
  - [✨ 功能特性](#-功能特性)
  - [🚩 快速入门](#-快速入门)
    - [硬件要求](#硬件要求)
    - [开发环境](#开发环境)
  - [🔨 如何使用](#-如何使用)
  - [⚡ 构建性能](#-构建性能)
  - [📊 构建分析](#-构建分析)
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

本示例提供以下开发板配置：

- `esp32_p4x_function_ev`
- `esp32_s31_korvo1`
- `esp_mosaico_v1_0`
- `esp_mosaico_v1_2`（初步适配，部分验证范围见下方指南）

硬件资源通过 [brookesia_hal_boards](https://components.espressif.com/components/espressif/brookesia_hal_boards) 组件管理。

在 ESP-Mosaico 上，扩展模块是可选功能，System Super 启动不依赖扩展模块。请按 CoreBoard 硬件版本选择配置；模块支持范围和限制请参见 [V1.0 适配说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/hal/boards/espressif.html#hal-boards-espressif-mosaico) 或 [V1.2 适配说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/hal/boards/espressif.html#hal-boards-espressif-mosaico-v1-2)。V1.2 CDC 控制台交互已验证，但当前样板触发自动下载后 USB 断开且未重新枚举。默认设置为 `CONFIG_BSP_USB_AUTO_DOWNLOAD=n` 和 `CONFIG_ESPTOOLPY_BEFORE_NORESET=y`，已有 `sdkconfig` 需同步调整，烧录前通过 BOOT 手动进入 ROM 下载模式。V1.2 启动日志已出现 `=== System Example Completed ===`，当前样板的桌面显示、基本触摸交互和亮度滑杆拖动已人工确认正常。My Device 信息和附近 Wi-Fi 热点扫描也已确认正常，运行日志记录了网络校时成功。指南另列明已验证的设备初始化、NAND 文件操作与重挂载，以及 SC101IOT 50 轮启停取帧。音质、摄像头实际帧率与图像质量及 NAND 文件数据断电持久化仍待验证。NAND FATFS 已通过 HAL/Storage Service 接入，尚不能作为可选的应用安装位置。

V1.2 默认将 HTTP/TLS 路径所用的 mbedTLS/PSA SHA/AES 配置为软件实现（`CONFIG_MBEDTLS_HARDWARE_SHA=n`、`CONFIG_MBEDTLS_HARDWARE_AES=n`），支持 LCD、NAND、摄像头和 HTTPS 共存；已有 V1.2 `sdkconfig` 也需关闭这两项。实机跟踪显示 App Store 的 HTTPS 活动持续占用剩余 AXI GDMA RX 通道，阻止 Camera 重新打开。TLS 和签名验证仍启用，CPU 工作量可能增加。直接硬件加密调用仍然存在，加密媒体共存尚未验证。修复版固件的 Camera 正常预览已由用户人工确认。实际下载安装应用后的复测未单独记录。

> [!TIP]
> 对于配有 SD Card 插槽的开发板，示例支持使用 SD Card 作为外部存储卷，请在上电前将 SD Card 插入开发板。
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

### Console 与主机控制端口配对

ESP-Mosaico 使用板级 TinyUSB CDC 控制台，板级默认配置关闭 USB Service 和 System Core USB bridge。下述 UART 与 USB Serial/JTAG profile 不提供通过 Mosaico 板载 TinyUSB CDC 端口进行的主机控制。

USB service 的主机控制（BPK 安装、文件传输）与 console 日志输出必须使用同一个物理端口，这样主机才能通过一根线既与设备通信又读取日志。这两个配置项互相独立，必须成对切换：

| 端口 | Console | USB service 传输 |
| --- | --- | --- |
| USB 转 UART 桥（UART0） | `CONFIG_ESP_CONSOLE_UART_DEFAULT=y` | `CONFIG_BROOKESIA_SERVICE_USB_TRANSPORT_UART=y`、`..._UART_PORT=0` |
| USB Serial/JTAG (USJ) | `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` | `CONFIG_BROOKESIA_SERVICE_USB_TRANSPORT_SERIAL_JTAG=y` |

`examples/system/super/sdkconfig.defaults.esp32p4` 与 `sdkconfig.defaults.esp32s31` 中同时提供两种 profile，只需启用其中一组。两目标的默认值不同：

- **ESP32-P4** 默认使用 **USB Serial/JTAG**，与 P4 function 板默认配置及 CI target test 环境一致。当主机通过板载 USB 转 UART 桥连接时，改为 USB 转 UART profile。
- **ESP32-S31**（Korvo1）默认使用 **USB 转 UART**，与板级默认的 UART0 console 一致。

ESP32-P4 同时引出两种端口，目前需在编译期选择；运行期自动识别尚未实现。

`brookesia-usb` 主机 CLI 会自动发现端口，切换 profile 后主机侧无需改动。

## ⚡ 构建性能

示例默认构建完整依赖集，同时启用 ccache，并限制第一方 Brookesia C++ target 和 `esp-boost` 的并发编译 edge 数量；这些模板密集型编译单元通常具有最高的编译器峰值内存。相同的调优机制也用于所有第一方 example 和 test app。

| CMake 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `CCACHE_ENABLE` | `ON` | 在重复构建中复用编译结果 |
| `BROOKESIA_CXX_JOBS` | `6` | 包含 C++ 的 Brookesia 和 `esp-boost` target 的 Ninja job pool 大小；设置为 `0` 可关闭限制 |
| `BROOKESIA_FAST_COMPILE` | `OFF` | 为 Brookesia C++ 源文件使用 `-g1`，减少本地开发时的调试信息生成量 |
| `BROOKESIA_COMPILE_TUNING_INCLUDE_ESP_BOOST` | `ON` | target 存在时将 `esp-boost` 加入共享 C++ job pool |

PC 内存较小时可以进一步降低 `BROOKESIA_CXX_JOBS`：

```bash
idf.py -B build -D BROOKESIA_CXX_JOBS=3 build
```

现有的 GCC IPA clone 编译选项保持不变，并与 job pool 开关独立生效。旧的 `BROOKESIA_SUPER_CXX_JOBS` 和 `BROOKESIA_SUPER_FAST_COMPILE` 名称仍作为弃用别名接受。

## 📊 构建分析

完成配置或编译后，可基于构建元数据生成报告：

```bash
python3 tools/analyze_build.py build
```

工具读取 `project_description.json`、`compile_commands.json`、`.ninja_log` 和 Ninja 依赖信息，只会在指定构建目录中写入 `brookesia_build_analysis.json` 和 `brookesia_build_analysis.md`。仅需编译单元及 Ninja 耗时统计时可添加 `--skip-deps`。

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

**USB 转 UART 桥上看不到应用日志**

Bootloader 的 ROM 阶段始终从 UART0 输出，但第二阶段 bootloader 与应用从所配置的 console 输出。若 console 设为 USB Serial/JTAG，则在 USB 转 UART 桥上只能看到 ROM banner，之后便没有输出。这通常意味着 console 与主机控制传输被配对到了不同端口：请启用 [Console 与主机控制端口配对](#console-与主机控制端口配对) 中的 USB 转 UART profile。

## 💬 技术支持与反馈

请通过以下渠道进行反馈：

- 有关技术问题，请访问 [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) 论坛
- 有关功能请求或错误报告，请创建新的 [GitHub 问题](https://github.com/espressif/esp-brookesia/issues)
