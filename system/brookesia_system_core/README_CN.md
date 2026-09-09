# ESP-Brookesia System Core

* [English Version](./README.md)

## 概述

`brookesia_system_core` 是面向产品的应用、GUI、运行时、存储、定时器和服务桥接框架。

更多信息请参考 [ESP-Brookesia 编程指南](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/system/core/index.html)。

## USB 主机桥接

在支持的 ESP32-P4 目标上，System Core 可以绑定 USB Serial/JTAG 服务，为主机提供独占控制通道。该通道支持服务调用、配置上传根目录下的受保护文件上传，以及 BPK 运行时应用安装。
不支持的 manifest 字段会打印 warning，但不会阻止软件包安装。

配置和使用方式请参考 [USB service 文档](../../service/system/brookesia_service_usb/README_CN.md)
以及 [主机 CLI 文档](../../tools/brookesia_usb/README_CN.md)。

## 如何使用

### 开发环境要求

请参考以下文档：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 开发环境搭建](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-dev-environment)

### 添加到工程

请参考 [ESP-Brookesia 编程指南 - 如何获取和使用组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-component-usage)。
