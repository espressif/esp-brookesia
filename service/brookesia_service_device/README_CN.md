# ESP-Brookesia Device Service

* [English Version](./README.md)

## 概述

`brookesia_service_device` 是 ESP-Brookesia 中面向应用层的设备控制服务。它提供应用层访问 HAL 能力的统一服务接口，主要用于控制类操作和状态获取类操作。

它不是硬件驱动。HAL adaptor 和 board 组件负责提供显示背光、音频播放、存储文件系统、板卡信息、电池等底层 HAL 接口；Device Service 负责发现这些已初始化的 HAL 接口，并通过 `esp_brookesia::service::helper::Device` 暴露为 Service 函数和事件。

典型能力包括：

- 获取 HAL 能力快照和板卡信息。
- 设置/查询显示背光亮度和背光开关状态。
- 设置/查询音频播放器音量和静音状态。
- 查询已挂载的存储文件系统。
- 查询电池信息、电池状态和充电配置。
- 在底层 HAL 支持时控制电池充电。
- 搭配 `brookesia_service_nvs` 时持久化部分应用层状态，例如音量、静音和亮度。

更多信息请参考 [ESP-Brookesia 编程指南](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/service/device.html)。

## 如何使用

### 开发环境要求

请参考以下文档：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 开发环境搭建](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-dev-environment)

### 添加到工程

请参考 [ESP-Brookesia 编程指南 - 如何获取和使用组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-component-usage)。

### 应用层调用流程

1. 初始化需要使用的 HAL 设备，例如调用 `esp_brookesia::hal::init_all_devices()`。
2. 启动 `esp_brookesia::service::ServiceManager`。
3. 通过 `esp_brookesia::service::helper::Device` 调用函数或订阅事件。
4. 调用可选功能前先查询能力，因为不同板卡暴露的 HAL 接口可能不同。
