# ESP-Brookesia GUI Interface

* [中文版本](./README_CN.md)

## Overview

`brookesia_gui_interface` is the backend-neutral GUI document and runtime interface for ESP-Brookesia.

For more information, see the [ESP-Brookesia Programming Guide](https://docs.espressif.com/projects/esp-brookesia/en/latest/gui/interface/json_ui/index.html).

## C++ API Migration

The 0.9 line keeps styles sparse to reduce resident memory and fragmentation. C++ consumers moving from 0.8 must use accessors and setters instead of public optional fields:

```cpp
// Read: style.bg_color.value_or("")
const auto color = style.bg_color().value_or("");
// Write: style.bg_color = "#112233"
style.set_bg_color("#112233");
// Clear: style.bg_color.reset()
style.set_bg_color(std::nullopt);
```

Use GUI Interface and GUI LVGL from the same 0.9.x line and rebuild C++ consumers. Existing 0.8.x dependency constraints keep selecting the 0.8 API. JSON UI documents retain protocol version 0.1.1.

## How to Use

### Environment Requirements

Please refer to the following documentation:

- [ESP-Brookesia Programming Guide - Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia Programming Guide - Development Environment Setup](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-dev-environment)

### Add to Your Project

Please refer to [ESP-Brookesia Programming Guide - How to Obtain and Use Components](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-component-usage).
