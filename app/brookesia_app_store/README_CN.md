# ESP-Brookesia 应用商店应用

* [English Version](./README.md)

## 概述

`brookesia_app_store` 是用于运行时应用包发现与安装的内置原生应用商店。

HTTP 事件缓冲最多跟踪 16 个 App Store 请求，保留 33 条待处理事件，并限制响应负载预算为 768 KiB。无关请求会被过滤，进度事件会合并。响应超过预算时，相关请求会被取消并报告失败，调用方可以重试。

结果丢失按请求 ID 隔离。事件突发超出丢失记录容量时，仍有进度的请求会继续执行；结果持续不可用、超过包含重试的等待时间后，相关请求才会被报告为失败。

更多信息请参考 [ESP-Brookesia 编程指南](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/app/store.html)。

## 如何使用

### 开发环境要求

请参考以下文档：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 开发环境搭建](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-dev-environment)

### 添加到工程

请参考 [ESP-Brookesia 编程指南 - 如何获取和使用组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-component-usage)。
