# ESP-Brookesia System Core

* [English Version](./README.md)

## 概述

`brookesia_system_core` 是面向产品的应用、GUI、运行时、存储、定时器和服务桥接框架。

更多信息请参考 [ESP-Brookesia 编程指南](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/system/core/index.html)。

## GSP 系统主页与公共输入

ESP 构建通过 Kconfig 选择 GSP 或 LVGL；目标需启用
`BROOKESIA_SYSTEM_CORE_ENABLE_GSP_BACKEND` 才能选择 GSP。LVGL 构建不编译
GSP Shell、场景资源或 renderer 依赖；应用生命周期与服务契约继续共享。

`System::Config` 配置 `gsp_panel` 和 `gsp_touch` 后，System Core 安装并启动
内置 GSP 系统主页。主页每页列出六个可见应用，安装或卸载后刷新列表，恢复保存的
语言和主题偏好，并在系统运行期间持有 SNTP 服务。

Native 和 JavaScript 应用使用现有 System Core 键盘和消息框 API。打开模态界面时
暂时停止应用的 GSP 会话，关闭后恢复；切换应用时通过请求管理器关闭待处理的模态
请求。GSP 停止失败会保留会话及显示、触摸设备引用，成功重试后才释放设备。

主页、键盘和消息框场景归本组件所有；主题编译辅助和公共 Latin 字体来自已声明的
`brookesia_gui_gsp` 依赖。使用本组件不需要 Settings 或 GSP 示例的源码文件。

Linux GSP 构建通过 `System::Config::gsp_desktop_display` 配置显示端，复用相同的
主页、应用管理和公共弹层逻辑。可选的 `app_support_check` 返回应用启动限制原因，
已安装应用仍可在主页显示并报告错误。Toolkit 入口是
`simulator/tools/brookesia_gsp_system/run.py`；对应 Linux 使用文档说明了包发现、
数据保留方式及尚未接通的硬件能力。

Linux 下，JavaScript 同步调用 `ShowKeyboard` 或 `ShowMessageDialog` 时，先返回
已接收的请求 ID，再启动渲染界面；即使尚未显示，也可以用该 ID 取消请求。消息框
与 native/system 请求共用同一 FIFO，已取消的待显示消息框不会出现。接收后显示
失败时，原关闭事件对该 ID 完成一次通知，App 保留实际错误。ESP 的同步显示路径
保持不变。

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
