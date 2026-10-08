# ESP-Brookesia 音频服务

* [English Version](./README.md)

## 概述

`brookesia_service_audio` 是用于音频播放、采集、AFE 和音量控制的媒体服务组件。

播放调用方可将 `PlayUrlConfig::only_if_idle` 与 `interrupt=false` 一同设置，使播放器忙碌时直接拒绝请求而不排队。该选项默认为 false，保留现有播放行为。

更多信息请参考 [ESP-Brookesia 编程指南](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/service/audio.html)。

## DataFlow 集成

该服务会向 Service Manager 注册由 provider 提供的音频播放和采集操作。应用可以通过类型安全的 DataFlow 合约或 `DataFlow` helper 选择可用 provider，无需直接绑定编解码器或设备实现。

## 如何使用

### 开发环境要求

请参考以下文档：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 开发环境搭建](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-dev-environment)

### 添加到工程

请参考 [ESP-Brookesia 编程指南 - 如何获取和使用组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-component-usage)。
