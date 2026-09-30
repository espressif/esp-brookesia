# ESP-Brookesia 设置应用

* [English Version](./README.md)

## 概述

`brookesia_app_settings` 是面向 System Core 与 System Super 产品的内置原生设置应用。

## 支持的界面

Settings 在两种 GUI 后端中使用相同的应用身份和设备服务偏好设置。

| 界面 | 可用页面 |
| --- | --- |
| JSON / LVGL | 现有 Settings 页面，包括 Wi-Fi 连接设置 |
| GSP | 1024 × 600 屏幕上的首页、显示亮度和 Wi-Fi 设置 |

GSP Wi-Fi 页面支持开关 Wi-Fi、扫描、刷新和浏览附近热点。每个热点显示信号强度和安全状态，页面同时展示当前连接，并处理扫描中、无结果和服务异常等状态。

热点名称支持常用简繁中文、带重音的拉丁字母、希腊字母和西里尔字母。

点击热点即可连接。需要密码的个人网络可通过 **Edit password** 输入密码，确认后点击 **Connect**。密码以掩码显示。开放热点和已保存凭据的热点会直接连接。页面根据 Wi-Fi 服务的实际结果显示状态，由服务保存成功连接的凭据并处理重连。

GSP 键盘支持可打印的 ASCII 字符，暂不提供企业网络配置、中文输入和已保存网络管理。声音、语言和主题设置将在后续版本提供。

包含启动页和硬件配置的完整应用，请参考 [System GSP App 示例](../../examples/system/gsp_app/README_CN.md)。

更多信息请参考 [ESP-Brookesia 编程指南](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/app/settings.html)。

## 如何使用

### 开发环境要求

请参考以下文档：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 开发环境搭建](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-dev-environment)

### 添加到工程

请参考 [ESP-Brookesia 编程指南 - 如何获取和使用组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-component-usage)。
