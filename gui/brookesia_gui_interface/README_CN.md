# ESP-Brookesia GUI Interface

* [English Version](./README.md)

## 概述

`brookesia_gui_interface` 是 ESP-Brookesia 中与后端无关的 GUI 文档和运行时接口组件。

更多信息请参考 [ESP-Brookesia 编程指南](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/gui/interface/json_ui/index.html)。

## C++ API 迁移

0.9 版本线保留样式的稀疏存储，以降低驻留内存和碎片化。C++ 使用者从 0.8 迁移时，需用访问器和设置函数替代公开的 optional 字段：

```cpp
// 读取：style.bg_color.value_or("")
const auto color = style.bg_color().value_or("");
// 写入：style.bg_color = "#112233"
style.set_bg_color("#112233");
// 清除：style.bg_color.reset()
style.set_bg_color(std::nullopt);
```

GUI Interface 和 GUI LVGL 应使用同一 0.9.x 版本线，并重新编译 C++ 使用者。现有 0.8.x 依赖约束继续选择 0.8 API。JSON UI 文档仍采用 0.1.1 协议。

## 如何使用

### 开发环境要求

请参考以下文档：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 开发环境搭建](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-dev-environment)

### 添加到工程

请参考 [ESP-Brookesia 编程指南 - 如何获取和使用组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-component-usage)。
