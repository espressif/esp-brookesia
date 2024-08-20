# How to Use

* [中文版本](./system_ui_phone_CN.md)

## Table of Contents

- [How to Use](#how-to-use)
  - [Table of Contents](#table-of-contents)
  - [Dependencies](#dependencies)
  - [Based on ESP-IDF](#based-on-esp-idf)
    - [Adding to Project](#adding-to-project)
    - [Configuration Instructions](#configuration-instructions)
    - [Project Examples](#project-examples)
  - [Based on Arduino](#based-on-arduino)
    - [Installing the Library](#installing-the-library)
    - [Configuration Instructions](#configuration-instructions-1)
    - [Project Examples](#project-examples-1)
  - [App Examples](#app-examples)

## Dependencies

| **Dependency** |   **Version**    |
| -------------- | ---------------- |
| lvgl           | >= 8.3 && < 9     |

## Based on [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)

### Adding to Project

esp-ui has been uploaded to the [Espressif Component Registry](https://components.espressif.com/), and users can add it to their project using the `idf.py add-dependency` command, for example:

```bash
idf.py add-dependency "espressif/esp-ui"
```

Alternatively, users can create or modify the `idf_component.yml` file in the project directory. For more details, please refer to the [Espressif Documentation - IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

### Configuration Instructions

When developing with esp-idf, users can configure esp-ui through the menuconfig:

1. Run the command `idf.py menuconfig`.
2. Navigate to `Component config` > `ESP-UI Configurations`.

### Project Examples

Here are examples of using esp-ui on the esp-idf development platform:

- [esp_ui_phone_s3_lcd_ev_board](../examples/esp_idf/esp_ui_phone_s3_lcd_ev_board): This example demonstrates how to run the phone UI on the [ESP32-S3-LCD-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/index.html) development board.
- [esp_ui_phone_p4_function_ev_board](../examples/esp_idf/esp_ui_phone_p4_function_ev_board): This example demonstrates how to run the phone UI on the [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html) development board.

## Based on [Arduino](https://docs.espressif.com/projects/arduino-esp32/en/latest/getting_started.html)

### Installing the Library

For online installation, navigate to `Sketch` > `Include Library` > `Manage Libraries...` in the Arduino IDE, then search for `esp-ui` and click the `Install` button.

For manual installation, download the required version of the `.zip` file from [esp-ui](https://github.com/espressif/esp-ui), then in the Arduino IDE, navigate to `Sketch` > `Include Library` > `Add .ZIP Library...`, select the downloaded `.zip` file, and click `Open` to install.

Users can also refer to the [Arduino IDE v1.x.x](https://docs.arduino.cc/software/ide-v1/tutorials/installing-libraries) or [Arduino IDE v2.x.x](https://docs.arduino.cc/software/ide-v2/tutorials/ide-v2-installing-a-library) documentation for library installation guidelines.

### Configuration Instructions

When developing with Arduino, users can configure esp-ui by modifying the first *esp_ui_conf.h* file found in the search path, with the following features:

1. esp-ui searches for configuration files in the following order: `current project directory` > `Arduino library directory` > `esp-ui directory`.
2. All example projects in esp-ui include a configuration file by default, which users can modify directly. If you want to use a configuration file from a different path, delete the one in the project.
3. For projects without a configuration file, users can copy it from the root directory of esp-ui or from example projects into their own project.
4. If multiple projects need to use the same configuration, users can place the configuration file in the Arduino library directory, allowing all projects to share the same configuration.

> [!NOTE]
> * Users can find and modify the path of the Arduino library directory by selecting `File` > `Preferences` > `Settings` > `Sketchbook location` in the Arduino IDE menu.

> [!WARNING]
> * Due to potential desynchronization between configuration file updates and esp-ui version updates, the library manages its version independently and checks whether the user's current configuration file is compatible with the library during compilation. Detailed version information and checking rules can be found at the end of the [esp_ui_conf.h](../esp_ui_conf.h) file.

Here is an example of enabling LOG debugging by modifying the *esp_ui_conf.h* file:

```c
...
/**
 * Log level. Higher levels produce less log output. Choose one of the following:
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

### Project Examples

Users can access them by navigating to `File` > `Examples` > `esp-ui` in the Arduino IDE. If the `esp-ui` option is not available, check if the library is installed correctly and ensure that an ESP board is selected.

Here are examples of using esp-ui on the Arduino development platform:

- [ESP_UI_Phone](../examples/arduino/ESP_UI_Phone): This example demonstrates how to run the Phone UI using the [ESP32_Display_Panel](https://github.com/esp-arduino-libs/ESP32_Display_Panel) library.

## App Examples

esp-ui provides general app examples for the following system UIs. Users can modify the appropriate examples based on their actual needs to quickly develop custom apps.

- [Phone](../src/app_examples/phone/)
  - [Simple Conf](../src/app_examples/phone/simple_conf/): This example uses a simple app configuration method, where most parameters have default values and only a few essential ones need to be configured by the user. It is suitable for GUI development using the "handwritten code"<sup>[Note 1]</sup> approach.
  - [Complex Conf](../src/app_examples/phone/complex_conf/): This example uses a complex app configuration method, where all parameters need to be configured by the user. It is suitable for GUI development using the "handwritten code"<sup>[Note 1]</sup> approach.
  - [Squareline](../src/app_examples/phone/squareline/): This example uses a simple app configuration method, where most parameters have default values and only a few essential ones need to be configured by the user. It is suitable for GUI development using the "Squareline exported code"<sup>[Note 2]</sup> approach. This example also demonstrates how to modify the code exported by Squareline to automatically clean up animation resources when the app exits.

> [!NOTE]
> 1. "Handwritten code" refers to manually writing code to develop the GUI, typically using `lv_scr_act()` to get the current screen object and adding all GUI elements to that screen object.
> 2. "Squareline exported code" refers to designing the GUI using [Squareline Studio](https://squareline.io/), exporting the code, and then adding the exported code to the app to develop the GUI.
> 3. The app provides an automatic default screen creation feature (`enable_default_screen`). When enabled, the system will automatically create and load a default screen when the app's `run()` is executed. Users can get the current screen object via `lv_scr_act()`. If users need to manually create and load screen objects, this feature should be disabled.
> 4. The app provides an automatic resource cleanup feature (`enable_recycle_resource`). When enabled, the system will automatically clean up resources, including **screens** (`lv_obj_create(NULL)`), **animations** (`lv_anim_start()`), and **timers** (`lv_timer_create()`), when the app exits. This feature requires users to complete the creation of all resources within the `run()/resume()` function or between `startRecordResource()` and `stopRecordResource()`.
> 5. The app provides an automatic screen size adjustment feature (`enable_resize_visual_area`). When enabled, the system will automatically resize the screen to the visual area when the screen is created. This feature requires users to complete the creation of all screens (such as `lv_obj_create(NULL)`, `lv_timer_create()`, `lv_anim_start()`) within the `run()/resume()` function or between `startRecordResource()` and `stopRecordResource()`. If this feature is not enabled, the app's screen will be displayed in full screen by default, but some areas might be obscured when displaying floating UIs (such as a status bar). The app can call the `getVisualArea()` function to retrieve the final visual area.

> [!WARNING]
> * If users adopt the "handwritten code" approach in their app, it's recommended to enable the `enable_default_screen` feature.
> * To avoid naming conflicts between variables and functions across multiple apps, it's recommended to prefix global variables and functions in the app with "<app_name>_".

> [!WARNING]
> * If users adopt the "Squareline exported code" approach in their app, it's recommended to use Squareline Studio version `v1.4.0` or above and disable the `enable_default_screen` feature.
> * To avoid naming conflicts between variables and functions across multiple apps, it's recommended to prefix the names of all screens with "<app_name>_" and set "Object Naming" in Squareline Studio's "Project Settings" > "Properties" to "[Screen_Widget]_Name" (this feature requires version >= `v1.4.0`).
> * Since each code exported by Squareline includes *ui_helpers.c* and *ui_helpers.h* files, to avoid function name conflicts across multiple apps, it's recommended to delete these two files and use the *ui_helpers* file provided by esp-ui instead. After deletion, modify the *ui.h* file by replacing `#include "ui_helpers.h"` with `#include "esp_ui.h"`.
> * It's advised not to create and use animations in Squareline, as these animation resources cannot be directly accessed by the user or esp-ui and therefore cannot be automatically or manually cleaned up, which may lead to program crashes or memory leaks when the app exits. If you must use them, you need to modify the part of the exported code where the animation resources are created. Please refer to the implementation in the [Squareline Example](../src/app_examples/phone/squareline/), which manually records animation resources by adding `startRecordResource()` and `stopRecordResource()` functions before and after `lv_anim_start()` so that they can be automatically cleaned up when the app closes.
> * Additionally, when using the code exported by Squareline, users still need to make some additional modifications based on the actual situation, such as renaming the `ui_init()` function to `<app_name>_ui_init()` to ensure the code can be compiled and run correctly.
