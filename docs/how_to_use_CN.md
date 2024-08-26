# 如何使用

* [English Version](./how_to_use.md)

## 目录

- [如何使用](#如何使用)
  - [目录](#目录)
  - [依赖](#依赖)
  - [基于 ESP-IDF](#基于-esp-idf)
    - [添加到工程](#添加到工程)
    - [配置说明](#配置说明)
    - [工程示例](#工程示例)
  - [基于 Arduino](#基于-arduino)
    - [安装库](#安装库)
    - [配置说明](#配置说明-1)
    - [工程示例](#工程示例-1)
  - [App 示例](#app-示例)

## 依赖

| **依赖** |   **版本**    |
| -------- | ------------- |
| lvgl     | >= 8.3 && < 9 |

## 基于 [ESP-IDF](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)

### 添加到工程

esp-ui 已上传到 [Espressif 组件库](https://components.espressif.com/)，用户可以通过 `idf.py add-dependency` 命令将它们添加到用户的项目中，例如：

```bash
idf.py add-dependency "espressif/esp-ui"
```

或者，用户也可以创建或修改工程目录下的 `idf_component.yml` 文件，详细内容请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。

### 配置说明

在使用 esp-idf 开发时，用户可以通过修改 menuconfig 来配置 esp-ui：

1. 运行命令 `idf.py menuconfig`。
2. 导航到 `Component config` > `ESP-UI Configurations`。

### 工程示例

以下是在 esp-idf 开发平台下使用 esp-ui 的示例：

- [esp_ui_phone_s3_lcd_ev_board](../examples/esp_idf/esp_ui_phone_s3_lcd_ev_board): 此示例演示了如何在 [ESP32-S3-LCD-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s3/esp32-s3-lcd-ev-board/index.html) 开发板上运行 phone UI。
- [esp_ui_phone_p4_function_ev_board](../examples/esp_idf/esp_ui_phone_p4_function_ev_board): 此示例演示了如何在 [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32p4/esp32-p4-function-ev-board/index.html) 开发板上运行 phone UI。

## 基于 [Arduino](https://docs.espressif.com/projects/arduino-esp32/en/latest/getting_started.html)

### 安装库

如果用户想要在线安装，可以在 Arduino IDE 中导航到 `Sketch` > `Include Library` > `Manage Libraries...`，然后搜索 `esp-ui`，点击 `Install` 按钮进行安装。

如果用户想要手动安装，可以从 [esp-ui](https://github.com/espressif/esp-ui) 下载所需版本的 `.zip` 文件，然后在 Arduino IDE 中导航到 `Sketch` > `Include Library` > `Add .ZIP Library...`，选择下载的 `.zip` 文件并点击 `Open` 按钮进行安装。

用户还可以查阅 [Arduino IDE v1.x.x](https://docs.arduino.cc/software/ide-v1/tutorials/installing-libraries) 或者 [Arduino IDE v2.x.x](https://docs.arduino.cc/software/ide-v2/tutorials/ide-v2-installing-a-library) 文档中关于安装库的指南。

### 配置说明

在使用 Arduino 开发时，用户可以通过修改搜索路径中第一个出现的 [esp_ui_conf.h](../esp_ui_conf.h) 文件来配置 esp-ui，具有如下的特点：

- esp-ui 搜索配置文件的路径顺序为：`当前工程目录` > `Arduino 库目录` > `esp-ui 目录`。
- esp-ui 中所有的示例工程都默认包含了配置文件，用户可以直接进行修改，如果想要使用其他路径下的配置文件，请删除工程中的配置文件。
- 对于没有配置文件的工程，用户可以将其从 esp-ui 的根目录或者示例工程中复制到自己的工程中。
- 如果有多个工程需要使用相同的配置，用户可以将配置文件放在 Arduino 库目录中，这样所有的工程都可以共享相同的配置。

> [!NOTE]
> * 用户可以在 Arduino IDE 的菜单栏中选择 `File` > `Preferences` > `Settings` > `Sketchbook location` 来查找和修改 Arduino 库的目录路径。

> [!WARNING]
> * 由于配置文件的更新可能会与 esp-ui 的版本更新不同步，为了保证程序的兼容性，库对它进行了独立的版本管理，并在编译时检查用户当前使用的配置文件与库是否兼容。详细的版本信息以及检查规则可以在 [esp_ui_conf.h](../esp_ui_conf.h) 文件的末尾处找到。

以开启 LOG 调试功能为例，下面是修改后的 *esp_ui_conf.h* 文件的部分内容：

```c
...
/**
 * Log level. Higher levels produce less log output. choose one of the following:
 *      - ESP_UI_LOG_LEVEL_DEBUG: Output all logs (most verbose)
 *      - ESP_UI_LOG_LEVEL_INFO:  Output info, warnings, and errors
 *      - ESP_UI_LOG_LEVEL_WARN:  Output warnings and errors
 *      - ESP_UI_LOG_LEVEL_ERROR: Output only errors
 *      - ESP_UI_LOG_LEVEL_NONE:  No log output (least verbose)
 *
 */
#define ESP_UI_LOG_LEVEL        (ESP_UI_LOG_LEVEL_DEBUG)
...
```

### 工程示例

用户可以在 Arduino IDE 中导航到 `File` > `Examples` > `esp-ui` 来访问它们。如果找不到 `esp-ui` 选项，请检查库是否已正确安装，并确认选择了一个 ESP 开发板。

以下是在 Arduino 开发平台下使用 esp-ui 的示例：

- [ESP_UI_Phone](../examples/arduino/ESP_UI_Phone): 此示例演示了如何使用 [ESP32_Display_Panel](https://github.com/esp-arduino-libs/ESP32_Display_Panel) 库运行 Phone UI。

## App 示例

esp-ui 针对以下系统 UI 提供了通用的 app 示例，用户可以根据实际需求基于适合的示例进行修改，快速实现自定义 app 的开发。

- [Phone](../src/app_examples/phone/)
  - [Simple Conf](../src/app_examples/phone/simple_conf/): 此示例采用简单的 app 配置方式，大部分参数为默认值，仅有少部分必要参数需要用户配置，并适用于 "手写代码"<sup>[Note 1]</sup> 的 GUI 开发方式。
  - [Complex Conf](../src/app_examples/phone/complex_conf/): 此示例采用复杂的 app 配置方式，所有参数都需要用户自行配置，并适用于 "手写代码"<sup>[Note 1]</sup> 的 GUI 开发方式。
  - [Squareline](../src/app_examples/phone/squareline/): 此示例采用简单的 app 配置方式，大部分参数为默认值，仅有少部分必要参数需要用户配置，并适用于 "Squareline 导出代码"<sup>[Note 2]</sup> 的 GUI 开发方式。该示例还展示了如何修改 Squareline 导出的代码，以实现在 app 退出时自动清理动画资源。

> [!NOTE]
> 1. "手写代码" 是指用户需要手动编写代码来实现 GUI 的开发，通常使用 `lv_scr_act()` 获取当前屏幕对象，并将所有的 GUI 元素添加到该屏幕对象上。
> 2. "Squareline 导出代码" 是指用户使用 [Squareline Studio](https://squareline.io/) 设计 GUI，并导出代码，然后将导出的代码添加到 app 中，即可实现 GUI 的开发。
> 3. app 提供了自动创建默认屏幕的功能（`enable_default_screen`），开启后系统会在 app 执行 `run()` 时自动创建并加载一个默认屏幕，用户可以通过 `lv_scr_act()` 获取当前屏幕对象。如果用户需要自行创建和加载屏幕对象，请关闭此功能。
> 4. app 提供了自动清理资源的功能（`enable_recycle_resource`），开启后系统会在 app 退出时会自动清理包括 **屏幕**（`lv_obj_create(NULL)`）、 **动画**（`lv_anim_start()`） 和 **定时器**（`lv_timer_create()`）在内的资源，此功能要求用户在 `run()/resume()` 函数内或者 `startRecordResource()` 与 `stopRecordResource()` 之间完成所有资源的创建。
> 5. app 提供了自动调整屏幕大小的功能（`enable_resize_visual_area`），开启后系统会在 app 创建屏幕时自动调整屏幕的大小为可视区域的大小，此功能要求用户在 `run()/resume()` 函数内或者 `startRecordResource()` 与 `stopRecordResource()` 函数之间完成所有屏幕的创建（`lv_obj_create(NULL)`, `lv_timer_create()`, `lv_anim_start()`）。当屏幕显示悬浮 UI（如状态栏）时，如果未开启此功能，app 的屏幕将默认以全屏显示，但某些区域可能被遮挡。app 可以调用 `getVisualArea()` 函数来获取最终的可视区域。

> [!WARNING]
> * 如果用户在 app 中采用 "手写代码" 的方式，请开启 `enable_default_screen` 功能。
> * 为了避免多个 app 之间出现变量和函数重名的问题，推荐 app 内的全局变量和函数采用 "<app_name>_" 前缀的命名方式。

> [!WARNING]
> * 如果用户在 app 中采用 "Squareline 导出代码" 的方式，推荐 Squareline Stduio 的版本为 `v1.4.0` 及以上，并关闭 `enable_default_screen` 功能。
> * 为了避免多个 app 之间出现变量和函数重名的问题，推荐所有屏幕的名称采用 "<app_name>_" 前缀的命名方式，并设置 Squareline Studio 中的 "Project Settings" > "Properties" > "Object Naming" 为 "[Screen_Widget]_Name"（此功能要求版本 >= `v1.4.0`）。
> * 由于每个 Squareline 导出的代码中都会有 *ui_helpers.c* 和 *ui_helpers.h* 文件，为了避免多个 app 之间出现函数重名的问题，请删除这两个文件，并统一使用 esp-ui 内部提供的 *ui_helpers* 文件，删除后需要修改 *ui.h* 文件中的 `#include "ui_helpers.h"` 为 `#include "esp_ui.h"`。
> * 建议用户不要在 Squareline 创建和使用动画，因为这些动画资源无法被用户或者 esp-ui 直接获取，从而无法被自动或者手动清理，可能会在 app 退出时导致程序崩溃或内存泄漏问题。如果一定要使用，需要对导出代码中动画资源的创建部分进行修改，具体请参考 [Squareline 示例](../src/app_examples/phone/squareline/) 中的实现，该示例通过在 `lv_anim_start()` 前后添加 `startRecordResource()` 和 `stopRecordResource()` 函数来手动记录动画资源，使其能够在 app 关闭时被自动清理。
> * 除此之外，使用 Squareline 导出的代码时，用户仍需根据实际情况进行一些额外的修改,如修改 `ui_init()` 函数名为 `<app_name>_ui_init()`，以确保代码能够被正常编译与运行。
