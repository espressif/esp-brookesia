# Mosaico HAL Custom

* [English Version](README.md)

## 概述

`brookesia_hal_custom` 包含 ESP32-S31 ESP-Mosaico V1.0 与 V1.2 板定义共用的
Mosaico 专属适配代码。虽然组件名较为通用，其设备与扩展模块实现的范围仍限定于
Mosaico 硬件。

所选板的 YAML 依赖覆盖配置启用此组件。该板目录中的 `board_devices.cpp` 注册
Board Manager 设备，并将生成的配置转换为明确的共享设备参数。版本选择、引脚分配、
设备依赖、触摸工厂和 SDK 默认配置仍保留在对应板目录中。

两版均使用 `src/board/audio_codec.c`：V1.0 注册供电使能 API，V1.2 注册无需独立
codec GPIO 的就绪等待 API。各板的 `setup_device.c` 保留触摸工厂，并向
`src/board/display.c` 中的共享面板工厂传入 LCD 时钟 GPIO。

清理回调由板级适配自动注册，应用无需额外注册。

## 源码布局与边界

| 位置 | 职责 |
| --- | --- |
| `include/brookesia/hal_custom/board/` | 共享设备参数与生命周期入口，不依赖生成的板级类型 |
| `src/board/` | 板级及 codec 供电时序、CO5300 初始化、BQ27220 查询、NAND 所有权，以及扩展生命周期和摄像头插槽处理 |
| [src/board/device_cleanup.cpp](src/board/device_cleanup.cpp) | 在启动时注册设备清理回调 |
| `src/display/` | CO5300 背光与显示 HAL 的衔接 |
| `src/expansion/` | Mosaico EEPROM 发现及连接扩展运行时的 provider |

通用 HAL 契约和共享生命周期运行时仍位于 `brookesia_hal_interface` 与
`brookesia_hal_adaptor`。本组件不会通过 eFuse 选择版本，也不包含任一板目录的源码。
CO5300 面板驱动和 USB CDC 控制台仍是同级独立组件。

上游参考、依赖补丁，以及此前实机证据与重组后源码布局验证的区别，见
[共享来源说明](../SOURCE.md)。
