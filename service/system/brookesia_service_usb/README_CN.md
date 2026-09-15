# ESP-Brookesia USB Service

* [English Version](./README.md)

## 概述

`brookesia_service_usb` 为支持的 ESP 目标提供串行传输的独占主机控制能力：USB Serial/JTAG CDC 通道，或面向只引出 USB 转串口桥的板子的 UART。

该服务提供类型化状态与传输事件、JSON 服务调用、文件上传和 BPK 运行时应用安装。主机协议与 CLI 请参考 [`brookesia_usb`](../../../tools/brookesia_usb/README_CN.md)。

## 环境要求

- 具备 USB Serial/JTAG 或已引出到主机的 UART 的 ESP 目标。
- 将所选传输保留给控制会话。
- `brookesia_service_helper` 和 `brookesia_service_manager` 依赖。

该服务依赖 ESP-IDF USB Serial/JTAG 与 UART 驱动，仅支持 ESP 平台；协议检查在主机 CLI 测试套件中执行。

## 添加到工程

将 `espressif/brookesia_service_usb` 添加到工程组件依赖并选择传输。引出 USB Serial/JTAG 的目标默认使用 Serial/JTAG；只引出 USB 转串口桥的板子请选择 UART。启用自动注册时，System Core 默认打开 USB bridge。

## 配置

主要 Kconfig 选项包括：

- `BROOKESIA_SERVICE_USB_TRANSPORT`：`SERIAL_JTAG`（支持时默认）或 `UART`。
- `BROOKESIA_SERVICE_USB_ENABLE_AUTO_REGISTER`：自动向 `ServiceManager` 注册服务。
- `BROOKESIA_SERVICE_USB_UPLOAD_ROOT`：主机上传和临时文件使用的绝对根目录，默认为 `/littlefs/usb`。
- `BROOKESIA_SERVICE_USB_MAX_TRANSFER_SIZE`：上传或 BPK 的最大大小，默认为 8 MiB。
- `BROOKESIA_SERVICE_USB_COMMAND_TIMEOUT_MS`：主机会话超时时间，默认为 30 秒。

UART 传输选项：

- `BROOKESIA_SERVICE_USB_UART_PORT`：ESP-IDF `uart_port_t`，默认为 `0`，即乐鑫开发板常见的 USB 转串口控制台端口。
- `BROOKESIA_SERVICE_USB_UART_BAUDRATE`：仅当所选端口不是控制台 UART 时使用；与控制台共用端口时沿用 `CONFIG_ESP_CONSOLE_UART_BAUDRATE`，保证控制台日志可读。
- `BROOKESIA_SERVICE_USB_UART_RX_BUFFER_SIZE`：默认为 16384，与默认帧长一致，避免 RX 泵延迟时丢帧。
- `BROOKESIA_SERVICE_USB_UART_TX_BUFFER_SIZE`：默认为 4096。

## 与控制台共用端口

没有 USB Serial/JTAG 的板子会用同一 UART 完成固件下载、控制台日志和控制会话，但三者不同时进行：

- 固件下载运行在应用启动前的 ROM 下载模式。
- 空闲时输出控制台日志；控制会话期间日志被抑制，`goodbye`、超时或断线后恢复。
- UART 传输下收到的非 JSON 输入会被静默丢弃，避免终端输入产生协议错误响应。

请勿在与控制传输相同的 UART 上启用交互式控制台 REPL（`esp_console`），二者会竞争 RX 数据流。

## 安全与所有权

USB 连接是独占的受信任控制边界。JSON 调用可以执行已注册的服务函数，因此应用不应向不受信任的物理主机暴露破坏性服务。

文件上传会拒绝绝对路径、父目录分量和符号链接逃逸。默认保护已有文件；显式传入 `overwrite` 才会替换文件。BPK 安装使用 System Core 的软件包校验与回滚流程。
