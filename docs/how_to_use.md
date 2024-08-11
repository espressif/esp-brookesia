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

esp-ui provides general app examples for the following system UIs. Users can modify these examples based on their actual needs to quickly develop custom apps.

- [Phone](../src/app_examples/phone/)
  - [Simple Conf](../src/app_examples/phone/simple_conf/): This example uses a simple app configuration with most parameters set to default values, requiring only a few necessary parameters to be configured by the user. It is suitable for GUI development using "handwritten code"<sup>[Note 1]</sup>.
  - [Complex Conf](../src/app_examples/phone/complex_conf/): This example uses a complex app configuration where all parameters need to be configured by the user. It is suitable for GUI development using "handwritten code"<sup>[Note 1]</sup>.
  - [Squareline](../src/app_examples/phone/squareline/): This example uses a simple app configuration with most parameters set to default values, requiring only a few necessary parameters to be configured by the user. It is suitable for GUI development using "Squareline exported code"<sup>[Note 2]</sup>.

> [!NOTE]
> 1. "Handwritten code" refers to manually writing code to implement the GUI, usually using `lv_scr_act()` to get the current screen object and adding all GUI elements to this screen object.
> 2. "Squareline exported code" refers to using [Squareline Studio](https://squareline.io/) to design the GUI, exporting the code, and then adding the exported code to the app to implement the GUI.
> 3. The app provides an automatic resource cleanup function (`enable_recycle_resource`), which automatically cleans up resources such as **screens**, **animations**, and **timers** when the app exits. This function requires all resources to be created in the `run()` function; otherwise, memory leaks may occur. If you need to create resources outside the `run()` function, disable this function and redefine the `cleanResource()` function to implement manual cleanup.
> 4. The app provides an automatic default screen creation function (`enable_default_screen`), which automatically creates and loads a default screen when the app runs. Users can obtain the current screen object through `lv_scr_act()`. If you need to create and load the screen object manually, disable this function.

> [!WARNING]
> * When using the "handwritten code" method in the app, to avoid variable and function name conflicts between multiple apps, it is recommended that global variables and functions in the app use the "<app_name>_" prefix.

> [!WARNING]
> * When using the "Squareline exported code" method in the app, it is recommended to use Squareline Studio version `v1.4.0` or above.
> * It is strongly recommended not to use animations in Squareline, as these animation objects cannot be accessed by the user or esp-ui, and therefore cannot be cleaned up automatically or manually, potentially leading to crashes or memory leaks when the app exits.
> * To avoid variable and function name conflicts between multiple apps, it is recommended to name all screens with the "<app_name>_" prefix and set "Project Settings" > "Properties" > "Object Naming" to "[Screen_Widget]_Name" in Squareline Studio (this feature requires version >= `v1.4.0`).
> * Since each set of Squareline exported code includes *ui_helpers.c* and *ui_helpers.h* files, to avoid function name conflicts between multiple apps, delete these two files and use the *ui_helpers* files provided internally by esp-ui. After deletion, modify the `#include "ui_helpers.h"` in the *ui.h* file to `#include "esp_ui.h"`.
