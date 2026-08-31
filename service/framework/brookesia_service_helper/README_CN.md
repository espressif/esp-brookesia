# ESP-Brookesia Service Helper

* [English Version](./README.md)

## 概述

`brookesia_service_helper` 是 Brookesia 服务函数与事件的类型化 helper 契约层。

更多信息请参考 [ESP-Brookesia 编程指南](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/service/framework/helper/index.html)。

## 稳定的 RPC 名称

每个 helper 的 `get_name()` 返回值都是稳定、区分大小写的公共 RPC 标识符。应用在调用服务以及填写 `manifest.json` 的 `services[].name` 字段时，必须使用该精确值。更改现有 RPC 名属于破坏性 API 变更，必须提供明确的兼容或迁移方案；普通重构不得改名。

## DataFlow Helper

`DataFlow` helper 提供类型安全的控制面接口，用于 provider 发现、操作归属、生命周期管理和源路由。原生视频帧与音频缓冲区访问仍通过类型安全的 C++ 操作接口提供。

## 如何使用

### 开发环境要求

请参考以下文档：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 开发环境搭建](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-dev-environment)

### 添加到工程

请参考 [ESP-Brookesia 编程指南 - 如何获取和使用组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-component-usage)。
