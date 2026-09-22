# ESP-Brookesia HAL Boards

* [English Version](./README.md)

## 概述

`brookesia_hal_boards` 是 ESP-Brookesia 的开发板配置集合，基于 YAML 描述各开发板的外设拓扑与设备参数，供 `brookesia_hal_adaptor` 在运行时无需硬编码即可完成硬件初始化。

更多信息请参考 [ESP-Brookesia 编程指南](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/hal/boards/index.html)。

### ESP-Mosaico 硬件版本

请明确选择与 CoreBoard 硬件版本一致的配置；板型选择不使用 eFuse 自动识别。

| 开发板配置 | 适配情况 |
| --- | --- |
| `esp_mosaico_v1_0` | 现有 V1.0 配置，参见 [V1.0 适配说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/hal/boards/espressif.html#hal-boards-espressif-mosaico) |
| `esp_mosaico_v1_2` | 面向现有 HAL 和服务能力的 V1.2 初步适配；**已完成部分实机验证**，已验证范围及剩余检查见 [V1.2 适配说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/hal/boards/espressif.html#hal-boards-espressif-mosaico-v1-2) |

V1.2 将板载与扩展 I2C 总线分离，调整 LCD 时钟/复位，取消独立 codec 供电 GPIO 并保留供电稳定延时，触摸使用 CST9220 驱动。保留 NAND FATFS 与可选摄像头接入，但 System Core 尚未将 NAND 作为应用安装位置。IMU 和马达服务不在本次适配范围内。两个配置均使用现有 Board Manager 0.5.15 依赖线。

V1.2 TinyUSB CDC 控制台已支持菜单交互。当前样板触发自动下载后 USB 断开且未重新枚举，因此默认关闭自动下载（`CONFIG_BSP_USB_AUTO_DOWNLOAD=n`），并通过 `CONFIG_ESPTOOLPY_BEFORE_NORESET=y` 避免手动进入 ROM 后再次复位。烧录前需通过 BOOT 手动进入 ROM 下载模式；已有 `sdkconfig` 需同步这两个设置。已检查显示/触摸初始化、亮度控制、音频初始化/释放、电池查询及 LittleFS/NVS。NAND 格式化后的文件操作和卸载/重挂载测试已通过，SC101IOT 50 轮启停与取帧也已通过。System Super 已完成启动，当前样板的桌面显示、基本触摸交互和亮度滑杆拖动已人工确认正常。My Device 信息和附近 Wi-Fi 热点扫描也已确认正常，运行日志记录了网络校时成功。音质、摄像头实际帧率与图像质量及 NAND 文件数据断电持久化仍待验证。

V1.2 将 HTTP/TLS 路径所用的 mbedTLS/PSA SHA/AES 配置为软件实现（`CONFIG_MBEDTLS_HARDWARE_SHA=n`、`CONFIG_MBEDTLS_HARDWARE_AES=n`），以支持 LCD、NAND、摄像头和 HTTPS 共存；已有 V1.2 `sdkconfig` 需同步这两个选项。实机跟踪已确认 HTTPS 活动持续占用剩余 AXI GDMA RX 通道，导致摄像头无法重新打开。这些设置保留 TLS 和签名验证，可能增加 CPU 工作量，并不禁用所有直接硬件加密调用。修复版固件的 Camera 应用正常预览已由用户人工确认。实际下载安装应用后的复测未单独记录。

### Mosaico 源码布局

`boards/espressif/` 下的每个版本目录分别保留 `setup_device.c`、`board_devices.cpp`、三份板级 YAML、`sdkconfig.defaults.board` 和 `SOURCE.md`。`setup_device.c` 选择触摸工厂，并向共享面板工厂传入该版本的 LCD 时钟 GPIO。`board_devices.cpp` 注册 Board Manager 设备，将生成的配置转换为明确的共享参数。共享音频实现提供独立的供电使能与就绪等待 API，分别由 V1.0、V1.2 注册代码选择。

两个版本均从 `components/mosaico/` 选择现有的 `brookesia_hal_custom`、`esp_lcd_co5300` 和 `esp_mosaico_usb_console` 组件。共享设备生命周期位于 `brookesia_hal_custom/src/board/`，注册接口位于 `include/brookesia/hal_custom/board/`；两版互不依赖对方的源码。参见[共享来源说明](components/mosaico/SOURCE.md)和 [Mosaico HAL Custom README](components/mosaico/brookesia_hal_custom/README_CN.md)。

上述实机结果记录于本次源码布局重构之前，不代表重组后的布局已通过构建或实机验证。

## 如何使用

### 开发环境要求

请参考以下文档：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 开发环境搭建](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-dev-environment)

### 添加到工程

请参考 [ESP-Brookesia 编程指南 - 如何获取和使用组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-component-usage)。
