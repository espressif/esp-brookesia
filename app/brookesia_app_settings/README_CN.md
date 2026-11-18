# ESP-Brookesia 设置应用

* [English Version](./README.md)

## 概述

`brookesia_app_settings` 是面向 System Core 与 System Super 产品的内置原生设置应用。

## 支持的界面

Settings 在两种 GUI 后端中使用相同的应用身份和设备服务偏好设置。

| 界面 | 可用页面 |
| --- | --- |
| JSON / LVGL | Settings 页面，包括 Wi-Fi 连接设置 |
| GSP | 1024 × 600 屏幕上的首页、我的设备、亮度、Wi-Fi、声音、语言、主题和时区 |

GSP Wi-Fi 页面支持开关 Wi-Fi、扫描、刷新和浏览附近热点。每个热点显示信号强度和安全状态，页面同时展示当前连接，并处理扫描中、无结果和服务异常等状态。

热点名称支持常用简繁中文、带重音的拉丁字母、希腊字母和西里尔字母。

点击热点即可连接。需要密码的个人网络可通过 **Edit password** 输入密码，确认后点击 **Connect**。密码以掩码显示。开放热点和已保存凭据的热点会直接连接。页面根据 Wi-Fi 服务的实际结果显示状态，由服务保存成功连接的凭据并处理重连。

点击**已保存网络**，选择明确的网络名称，再点击**忘记**删除该网络的保存凭据。忘记当前网络也会断开连接。**附近网络**返回扫描结果；页面不会默认选择删除目标。

密码输入使用 System Core 公共键盘，支持可打印的 ASCII 字符，暂不提供企业网络配置和中文输入。

## 声音与外观

**声音**通过 AudioPlayback 服务控制播放音量（0–100）和静音，由服务保存设置并在启动时恢复。宿主提供测试音频时，可点击**试听提示音**，以当前音量播放，静音状态不会改变。页面会显示控制不可用或更新失败的提示。

GSP 支持**英语**和**简体中文**，覆盖 Wi-Fi 状态及键盘操作文案；**浅色**和**深色**主题覆盖全部 GSP 设置页面。更改语言或主题时，选择选项后点击**立即重启**或**稍后**。重启前保持当前界面，再次打开设置仍可看到待生效的选项。

宿主配置 GSP 显示和触摸设备后，System Core 恢复外观偏好并提供系统主页、公共键盘和消息框。**我的设备**显示 OS、开发板、硬件能力和当前网络状态；**时区**提供常用时区选项，并在页面可见时刷新 SNTP 状态。高级调试仍由 JSON/LVGL 界面提供。

包含启动页和硬件配置的完整应用，请参考 [System GSP App 示例](../../examples/system/gsp_app/README_CN.md)。

更多信息请参考 [ESP-Brookesia 编程指南](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/app/settings.html)。

## 如何使用

### 开发环境要求

请参考以下文档：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 开发环境搭建](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-dev-environment)

### 添加到工程

请参考 [ESP-Brookesia 编程指南 - 如何获取和使用组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-component-usage)。
