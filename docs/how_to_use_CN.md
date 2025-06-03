# 如何使用

* [English Version](./how_to_use.md)

## 目录

- [如何使用](#如何使用)
  - [目录](#目录)
  - [依赖](#依赖)
  - [添加到工程](#添加到工程)
  - [配置说明](#配置说明)
  - [工程示例](#工程示例)
  - [如何开发 App](#如何开发-app)
    - [原理及功能](#原理及功能)
    - [通用示例](#通用示例)
    - [注意事项](#注意事项)

## 依赖

|    **依赖**     | **版本** |
| --------------- | -------- |
| esp-idf         | 5.1.*    |
| esp-lib-utils   | 0.2.*    |
| lvgl            | 9.2.*    |

## 添加到工程

esp-brookesia 已上传到 [Espressif 组件库](https://components.espressif.com/)，用户可以通过 `idf.py add-dependency` 命令将它们添加到用户的项目中，例如：

```bash
idf.py add-dependency "espressif/esp-brookesia"
```

或者，用户也可以创建或修改工程目录下的 `idf_component.yml` 文件，详细内容请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。

## 配置说明

在使用 esp-idf 开发时，用户可以通过修改 menuconfig 来配置 esp-brookesia：

1. 运行命令 `idf.py menuconfig`。
2. 导航到 `Component config` > `ESP-Brookesia Configurations`。

## 工程示例

以下是在 esp-idf 开发平台下使用 esp-brookesia 的示例：

- [phone_m5stace_core_s3](../examples/phone_m5stace_core_s3): 此示例演示了如何在 [M5Stack Core S3](https://docs.m5stack.com/zh_CN/core/core_s3) 开发板上运行 Phone UI。
- [phone_p4_function_ev_board](../examples/phone_p4_function_ev_board): 此示例演示了如何在 [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32p4/esp32-p4-function-ev-board/index.html) 开发板上运行 Phone UI。
- [phone_s3_box_3](../examples/phone_s3_box_3): 此示例演示了如何在 [ESP32-S3-Box-3](https://github.com/espressif/esp-box) 开发板上运行 Phone UI。
- [phone_s3_lcd_ev_board](../examples/phone_s3_lcd_ev_board): 此示例演示了如何在 [ESP32-S3-LCD-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s3/esp32-s3-lcd-ev-board/index.html) 开发板上运行 Phone UI。

## 如何开发 App

### 原理及功能

esp-brookesia 提供了通过 C++ 继承的方式来开发 app 的功能，用户可以根据目标系统 UI 继承相应的系统 app 基类（如 Phone 中的 [ESP_Brookesia_PhoneApp](../src/systems/phone/esp_brookesia_phone_app.hpp) 类），然后实现必要的纯虚函数（`run()` 和 `back()`），还可以根据需求重定义其他虚函数（如 `close()`、`init()` 和 `pause()` 等）,这些虚函数源自内核 app 基类 [ESP_Brookesia_CoreApp](../src/systems/core/esp_brookesia_core_app.hpp)。

esp-brookesia 的系统 app 基类是继承自内核 app 基类 `ESP_Brookesia_CoreApp`，它提供了以下用户可静态配置的功能：

- **名称**：通过设置 `ESP_Brookesia_CoreAppData_t` 中 `name` 字段来设置 app 的名称。
- **图标**：通过设置 `ESP_Brookesia_CoreAppData_t` 中 `launcher_icon` 字段来设置 app 的图标。
- **屏幕大小**：通过设置 `ESP_Brookesia_CoreAppData_t` 中 `screen_size` 字段来设置 app 的屏幕大小。由于当 app 屏幕大小小于实际屏幕大小时，空闲屏幕区域的显示会出现异常，因此建议用户设置的屏幕大小等于实际屏幕大小，即 `screen_size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(100, 100)`。
- **自动创建默认屏幕**：通过设置 `ESP_Brookesia_CoreAppData_t` 中 `enable_default_screen = 1` 开启,开启后系统会在 app 执行 `run()` 时自动创建并加载一个默认屏幕，用户可以通过 `lv_scr_act()` 获取当前屏幕对象。如果用户需要自行创建和加载屏幕对象，请关闭此功能。
- 自动清理资源的功能：通过设置 `ESP_Brookesia_CoreAppData_t` 中 `enable_recycle_resource = 1` 开启,开启后系统会在 app 退出时会自动清理包括 **屏幕**（`lv_obj_create(NULL)`）、 **动画**（`lv_anim_start()`） 和 **定时器**（`lv_timer_create()`）在内的资源，此功能要求用户在 `run()/resume()` 函数内或者 `startRecordResource()` 与 `stopRecordResource()` 之间完成所有资源的创建。
- **自动调整屏幕大小的功能**：通过设置 `ESP_Brookesia_CoreAppData_t` 中 `enable_resize_visual_area = 1` 开启,开启后系统会在 app 创建屏幕时自动调整屏幕的大小为可视区域的大小，此功能要求用户在 `run()/resume()` 函数内或者 `startRecordResource()` 与 `stopRecordResource()` 函数之间完成所有屏幕的创建（`lv_obj_create(NULL)`, `lv_timer_create()`, `lv_anim_start()`）。当屏幕显示悬浮 UI（如状态栏）时，如果未开启此功能，app 的屏幕将默认以全屏大小显示，但某些区域可能被遮挡。app 可以调用 `getVisualArea()` 函数来获取最终的可视区域。

除此之外，esp-brookesia 的系统 app 基类也提供了一些额外的用户可静态配置的功能，以 Phone 的 app 基类 `ESP_Brookesia_PhoneApp` 为例：

- **图标页索引**：通过设置 `ESP_Brookesia_PhoneAppData_t` 中 `app_launcher_page_index` 字段来设置 app 图标显示在主页的第几页（通常分为 3 页）。
- **状态图标区域索引**：通过设置 `ESP_Brookesia_PhoneAppData_t` 中 `status_icon_area_index` 字段来设置 app 状态图标显示在状态栏的第几个区域（通常分为 `左-中-右` 三个区域或 `左-右` 两个区域）。
- **状态图标**：通过设置 `ESP_Brookesia_PhoneAppData_t` 中 `status_icon` 字段来设置 app 的状态图标。
- **状态栏显示模式**：通过设置 `ESP_Brookesia_PhoneAppData_t` 中 `status_bar_visual_mode` 字段来设置 app 的状态栏显示模式，支持 `固定` 和 `隐藏` 两种模式。
- **导航栏显示模式**：通过设置 `ESP_Brookesia_PhoneAppData_t` 中 `navigation_bar_visual_mode` 字段来设置 app 的导航栏显示模式，支持 `固定`、`隐藏` 和 `动态` 三种模式。
- **手势导航**：通过设置 `ESP_Brookesia_PhoneAppData_t` 中 `enable_navigation_gesture` 字段来设置 app 是否支持通过手势进行导航，该功能默认仅支持从底部向上滑动打开概览屏幕，可以通过设置样式表 `ESP_Brookesia_PhoneManagerData_t` 中 `enable_gesture_navigation_back` 字段来支持侧边滑动返回上一级页面。

### 通用示例

esp-brookesia 针对以下系统 UI 提供了通用的 app 示例，用户可以根据实际需求基于适合的示例进行修改，快速实现自定义 app 的开发。

- [Phone](../src/systems/phone/app_examples/)
  - [Simple Conf](../src/systems/phone/app_examples/simple_conf/): 此示例采用简单的 app 配置方式，大部分参数为默认值，仅有少部分必要参数需要用户配置，并适用于 "手写代码"<sup>[Note 1]</sup> 的 GUI 开发方式。
  - [Complex Conf](../src/systems/phone/app_examples/complex_conf/): 此示例采用复杂的 app 配置方式，所有参数都需要用户自行配置，并适用于 "手写代码"<sup>[Note 1]</sup> 的 GUI 开发方式。
  - [Squareline](../src/systems/phone/app_examples/squareline/): 此示例采用简单的 app 配置方式，大部分参数为默认值，仅有少部分必要参数需要用户配置，并适用于 "Squareline 导出代码"<sup>[Note 2]</sup> 的 GUI 开发方式。该示例还展示了如何修改 Squareline 导出的代码，以实现在 app 退出时自动清理动画资源。

> [!NOTE]
> 1. "手写代码" 是指用户需要手动编写代码来实现 GUI 的开发，通常使用 `lv_scr_act()` 获取当前屏幕对象，并将所有的 GUI 元素添加到该屏幕对象上。
> 2. "Squareline 导出代码" 是指用户使用 [Squareline Studio](https://squareline.io/) 设计 GUI，并导出代码，然后将导出的代码添加到 app 中，即可实现 GUI 的开发。

### 注意事项

如果用户采用 "手写代码" 的方式，请开启 **自动创建默认屏幕**（`enable_default_screen`）的功能，并注意以下事项：

- **函数重名**：为了避免多个 app 之间出现变量和函数重名的问题，推荐 app 内的全局变量和函数采用 "<app_name>_" 前缀的命名方式。

如果用户采用 "Squareline 导出代码" 的方式，请关闭 **自动创建默认屏幕**（`enable_default_screen`）的功能，推荐 Squareline Stduio 的版本为 `v1.4.0` 及以上，并参考以下事项：

- **函数重名**：为了避免多个 app 之间出现变量和函数重名的问题，推荐所有屏幕的名称采用 "<app_name>_" 前缀的命名方式，并设置 Squareline Studio 中的 "Project Settings" > "Properties" > "Object Naming" 为 "[Screen_Widget]_Name"（此功能要求版本 >= `v1.4.0`）。
- **重复的 UI helpers 文件**：由于每个 Squareline 导出的代码中都会有 *ui_helpers.c* 和 *ui_helpers.h* 文件，为了避免多个 app 之间出现函数重名的问题，请删除这两个文件，并统一使用 esp-brookesia 内部提供的 *ui_helpers* 文件，删除后需要修改 *ui.h* 文件中的 `#include "ui_helpers.h"` 为 `#include "esp_brookesia.h"`。
- **重复的 UI components 文件**：当 Squareline 工程使用了 `componnets` 控件，此时导出的代码中都会有 *ui_comp.c* 和 *ui_comp.h* 文件，为了避免多个 app 之间出现函数重名的问题，请删除 *ui_comp.c* 文件，并统一使用 esp-brookesia 内部提供的 *ui_comp.c* 文件，然后需要删除 *ui_comp.h* 文件中的如下内容：

  ```c
  void get_component_child_event_cb(lv_event_t * e);
  void del_component_child_event_cb(lv_event_t * e);

  lv_obj_t * ui_comp_get_child(lv_obj_t * comp, uint32_t child_idx);
  extern uint32_t LV_EVENT_GET_COMP_CHILD;
  ```

- **不推荐使用动画**：建议用户不要在 Squareline 创建和使用动画，因为这些动画资源无法被用户或者 esp-brookesia 直接获取，从而无法被自动或者手动清理，可能会在 app 退出时导致程序崩溃或内存泄漏问题。如果一定要使用，需要对导出代码中动画资源的创建部分进行修改，具体请参考 [Squareline 示例](../src/systems/phone/app_examples/squareline/) 中的实现，该示例通过在 `lv_anim_start()` 前后添加 `startRecordResource()` 和 `stopRecordResource()` 函数来手动记录动画资源，使其能够在 app 关闭时被自动清理。
- **确保正常编译**：除此之外，使用 Squareline 导出的代码时，用户仍需根据实际情况进行一些额外的修改，如修改 `ui_init()` 函数名为 `<app_name>_ui_init()`，以确保代码能够被正常编译与运行。
