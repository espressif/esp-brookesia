# ESP-Brookesia HAL Adaptor

* [English Version](./README.md)

## 概述

`brookesia_hal_adaptor` 是 ESP-Brookesia 的板级 HAL 适配实现，基于 `brookesia_hal_interface` 的设备/接口模型，通过 `esp_board_manager` 初始化真实外设，并将音频、显示、存储三类能力注册到全局 HAL 表，供上层按名称发现和使用。

更多信息请参考 [ESP-Brookesia 编程指南](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/hal/adaptor.html)。

## 如何使用

### 开发环境要求

请参考以下文档：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 开发环境搭建](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-dev-environment)

### 添加到工程

请参考 [ESP-Brookesia 编程指南 - 如何获取和使用组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-component-usage)。

## 扩展板级适配

自定义设备在初始化或释放失败后，可能仍持有需要清理的资源。板级适配可以注册清理回调，由 HAL 在后续释放请求中调用。使用已有板型的应用无需自行注册。

新增设备适配时，请参阅[设备清理注册接口](include/brookesia/hal_adaptor/board_manager.h)，了解注册方式和回调要求。
