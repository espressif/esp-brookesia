# ESP-Brookesia GUI LVGL 后端

* [English Version](./README.md)

## 概述

`brookesia_gui_lvgl` 是面向 GUI Interface 文档的 LVGL 后端和图片打包辅助组件。

更多信息请参考 [ESP-Brookesia 编程指南](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/gui/lvgl/index.html)。

## 如何使用

### 运行时图片格式

LVGL 后端支持 JSON UI ``imageSet`` 引用 LVGL ``.bin``、PNG 和 JPEG 文件。
JPEG 资源使用 ``.jpg`` 或 ``.jpeg`` 扩展名，经过校验后预加载到引用计数缓存，并可从 JPEG frame
header 补全图片尺寸。

ESP 平台需要启用 ``CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER``；PC 和 WASM 平台需要在 LVGL 配置中启用
``LV_USE_TJPGD`` 与 ``LV_USE_FS_MEMFS``。解码器不可用或文件损坏时，JPEG 加载会返回包含路径的
明确错误。

### 开发环境要求

请参考以下文档：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 开发环境搭建](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-dev-environment)

### 添加到工程

请参考 [ESP-Brookesia 编程指南 - 如何获取和使用组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-component-usage)。
