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
  - [App Development](#app-development)
    - [Principles and Features](#principles-and-features)
    - [General Examples](#general-examples)
    - [Considerations](#considerations)

## Dependencies

| **Dependency** |   **Version**    |
| -------------- | ---------------- |
| lvgl           | >= 8.3 && < 9     |

## Based on [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)

### Adding to Project

esp-brookesia has been uploaded to the [Espressif Component Registry](https://components.espressif.com/), and users can add it to their project using the `idf.py add-dependency` command, for example:

```bash
idf.py add-dependency "espressif/esp-brookesia"
```

Alternatively, users can create or modify the `idf_component.yml` file in the project directory. For more details, please refer to the [Espressif Documentation - IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

### Configuration Instructions

When developing with esp-idf, users can configure esp-brookesia through the menuconfig:

1. Run the command `idf.py menuconfig`.
2. Navigate to `Component config` > `ESP-Brookesia Configurations`.

### Project Examples

Here are examples of using esp-brookesia on the esp-idf development platform:

- [esp_brookesia_phone_s3_lcd_ev_board](../examples/esp_idf/esp_brookesia_phone_s3_lcd_ev_board): This example demonstrates how to run the phone UI on the [ESP32-S3-LCD-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/index.html) development board.
- [esp_brookesia_phone_p4_function_ev_board](../examples/esp_idf/esp_brookesia_phone_p4_function_ev_board): This example demonstrates how to run the phone UI on the [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html) development board.

## Based on [Arduino](https://docs.espressif.com/projects/arduino-esp32/en/latest/getting_started.html)

### Installing the Library

For online installation, navigate to `Sketch` > `Include Library` > `Manage Libraries...` in the Arduino IDE, then search for `esp-brookesia` and click the `Install` button.

For manual installation, download the required version of the `.zip` file from [esp-brookesia](https://github.com/espressif/esp-brookesia), then in the Arduino IDE, navigate to `Sketch` > `Include Library` > `Add .ZIP Library...`, select the downloaded `.zip` file, and click `Open` to install.

Users can also refer to the [Arduino IDE v1.x.x](https://docs.arduino.cc/software/ide-v1/tutorials/installing-libraries) or [Arduino IDE v2.x.x](https://docs.arduino.cc/software/ide-v2/tutorials/ide-v2-installing-a-library) documentation for library installation guidelines.

### Configuration Instructions

When developing with Arduino, users can configure esp-brookesia by modifying the first *esp_brookesia_conf.h* file found in the search path, with the following features:

1. esp-brookesia searches for configuration files in the following order: `current project directory` > `Arduino library directory` > `esp-brookesia directory`.
2. All example projects in esp-brookesia include a configuration file by default, which users can modify directly. If you want to use a configuration file from a different path, delete the one in the project.
3. For projects without a configuration file, users can copy it from the root directory of esp-brookesia or from example projects into their own project.
4. If multiple projects need to use the same configuration, users can place the configuration file in the Arduino library directory, allowing all projects to share the same configuration.

> [!NOTE]
> * Users can find and modify the path of the Arduino library directory by selecting `File` > `Preferences` > `Settings` > `Sketchbook location` in the Arduino IDE menu.

> [!WARNING]
> * Due to potential desynchronization between configuration file updates and esp-brookesia version updates, the library manages its version independently and checks whether the user's current configuration file is compatible with the library during compilation. Detailed version information and checking rules can be found at the end of the [esp_brookesia_conf.h](../esp_brookesia_conf.h) file.

Here is an example of enabling LOG debugging by modifying the *esp_brookesia_conf.h* file:

```c
...
/**
 * Log level. Higher levels produce less log output. Choose one of the following:
 *      - ESP_BROOKESIA_LOG_LEVEL_DEBUG: Output all logs (most verbose)
 *      - ESP_BROOKESIA_LOG_LEVEL_INFO:  Output info, warnings, and errors
 *      - ESP_BROOKESIA_LOG_LEVEL_WARN:  Output warnings and errors
 *      - ESP_BROOKESIA_LOG_LEVEL_ERROR: Output only errors
 *      - ESP_BROOKESIA_LOG_LEVEL_NONE:  No log output (least verbose)
 *
 */
#define ESP_BROOKESIA_LOG_LEVEL        (ESP_BROOKESIA_LOG_LEVEL_DEBUG)
...
```

### Project Examples

Users can access them by navigating to `File` > `Examples` > `esp-brookesia` in the Arduino IDE. If the `esp-brookesia` option is not available, check if the library is installed correctly and ensure that an ESP board is selected.

Here are examples of using esp-brookesia on the Arduino development platform:

- [Phone](../examples/arduino/Phone): This example demonstrates how to run the Phone UI using the [ESP32_Display_Panel](https://github.com/esp-arduino-libs/ESP32_Display_Panel) library.

## App Development

### Principles and Features

esp-brookesia provides the capability to develop apps through C++ inheritance, allowing users to inherit from the appropriate system app base class (e.g., the [ESP_Brookesia_PhoneApp](../src/systems/phone/esp_brookesia_phone_app.hpp) class for Phone) and implement the necessary pure virtual functions (`run()` and `back()`). Users can also redefine other virtual functions as needed (e.g., `close()`, `init()`, and `pause()`), which are derived from the core app base class [ESP_Brookesia_CoreApp](../src/core/esp_brookesia_core_app.hpp).

The system app base class in esp-brookesia inherits from the core app base class `ESP_Brookesia_CoreApp`, which provides the following user-configurable features:

- **Name**: Set the app's name by configuring the `name` field in `ESP_Brookesia_CoreAppData_t`.
- **Icon**: Set the app's icon by configuring the `launcher_icon` field in `ESP_Brookesia_CoreAppData_t`.
- **Screen Size**: Set the app's screen size by configuring the `screen_size` field in `ESP_Brookesia_CoreAppData_t`. It is recommended that the screen size equals the actual screen size (i.e., `screen_size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(100, 100)`) to avoid display anomalies in unused screen areas when the app's screen size is smaller.
- **Automatic Default Screen Creation**: Enable this feature by setting `enable_default_screen = 1` in `ESP_Brookesia_CoreAppData_t`. The system will automatically create and load a default screen when the app executes `run()`, and users can retrieve the current screen object using `lv_scr_act()`. If users wish to create and load screen objects manually, this feature should be disabled.
- **Automatic Resource Cleanup**: Enable this feature by setting `enable_recycle_resource = 1` in `ESP_Brookesia_CoreAppData_t`. The system will automatically clean up resources, including screens (`lv_obj_create(NULL)`), animations (`lv_anim_start()`), and timers (`lv_timer_create()`), when the app exits. This requires users to complete all resource creation within the `run()/resume()` function or between `startRecordResource()` and `stopRecordResource()`.
- **Automatic Screen Size Adjustment**: Enable this feature by setting `enable_resize_visual_area = 1` in `ESP_Brookesia_CoreAppData_t`. The system will automatically adjust the screen size to the visual area size when the app creates a screen. This also requires users to complete all screen creation within the `run()/resume()` function or between `startRecordResource()` and `stopRecordResource()`. If the screen displays floating UI elements (e.g., status bar) and this feature is not enabled, the app's screen will default to full-screen size, which may obscure some areas. The app can call the `getVisualArea()` function to retrieve the final visual area.

Additionally, the system app base class in esp-brookesia provides extra user-configurable features, exemplified by the Phone app base class `ESP_Brookesia_PhoneApp`:

- **Icon Page Index**: Set the app icon's page index on the home screen by configuring the `app_launcher_page_index` field in `ESP_Brookesia_PhoneAppData_t` (usually divided into three pages).
- **Status Icon Area Index**: Set the status icon's area index in the status bar by configuring the `status_icon_area_index` field in `ESP_Brookesia_PhoneAppData_t` (usually divided into `left-middle-right` or `left-right` areas).
- **Status Icon**: Set the app's status icon by configuring the `status_icon` field in `ESP_Brookesia_PhoneAppData_t`.
- **Status Bar Visual Mode**: Set the app's status bar visual mode by configuring the `status_bar_visual_mode` field in `ESP_Brookesia_PhoneAppData_t`, supporting `fixed` and `hidden` modes.
- **Navigation Bar Visual Mode**: Set the app's navigation bar visual mode by configuring the `navigation_bar_visual_mode` field in `ESP_Brookesia_PhoneAppData_t`, supporting `fixed`, `hidden`, and `flexible` modes.
- **Gesture Navigation**: Enable or disable gesture navigation for the app by configuring the `enable_navigation_gesture` field in `ESP_Brookesia_PhoneAppData_t`, default only supporting opening the recents screen by swiping up from the bottom. You can enable side-swipe gestures for navigating back to the previous page by configuring the `enable_gesture_navigation_back` field in the `ESP_Brookesia_PhoneManagerData_t` of stylesheet.

### General Examples

esp-brookesia provides general app examples for the following system UIs, allowing users to modify suitable examples based on actual needs for rapid custom app development:

- [Phone](../src/app_examples/phone/)
  - [Simple Conf](../src/app_examples/phone/simple_conf/): This example uses a simple app configuration, with most parameters set to default values and only a few necessary parameters requiring user configuration. It is suitable for the "handwritten code"<sup>[Note 1]</sup> approach to GUI development.
  - [Complex Conf](../src/app_examples/phone/complex_conf/): This example utilizes a complex app configuration where all parameters require user configuration. It is suitable for the "handwritten code"<sup>[Note 1]</sup> approach to GUI development.
  - [Squareline](../src/app_examples/phone/squareline/): This example employs a simple app configuration, with most parameters set to default values and only a few necessary parameters requiring user configuration. It is suitable for the "Squareline exported code"<sup>[Note 2]</sup> approach to GUI development. This example also demonstrates how to modify Squareline exported code to automatically clean up animation resources upon app exit.

> [!NOTE]
> 1. "Handwritten code" refers to the approach where users manually write code to implement GUI development, typically using `lv_scr_act()` to get the current screen object and adding all GUI elements to that screen object.
> 2. "Squareline exported code" refers to the method where users design GUIs using [Squareline Studio](https://squareline.io/), export the code, and then add the exported code to the app for GUI development.

### Considerations

If users adopt the "handwritten code" approach, they should enable **Automatic Default Screen Creation** (`enable_default_screen`) and consider the following:

- **Function Name Conflicts**: To avoid variable and function name conflicts between multiple apps, it is recommended that global variables and functions within the app use the "<app_name>_" prefix for naming.

If users adopt the "Squareline exported code" approach, they should disable **Automatic Default Screen Creation** (`enable_default_screen`) and use Squareline Studio version `v1.4.0` or higher, considering the following:

- **Function Name Conflicts**: To prevent variable and function name conflicts between multiple apps, it is recommended that all screen names use the "<app_name>_" prefix and set "Project Settings" > "Properties" > "Object Naming" in Squareline Studio to "[Screen_Widget]_Name" (this feature requires version >= `v1.4.0`).
- **Duplicate UI Helpers Files**: Since each Squareline exported code contains *ui_helpers.c* and *ui_helpers.h* files, users should delete these two files and use the internal *ui_helpers* file provided by esp-brookesia. After deletion, they need to modify `#include "ui_helpers.h"` in *ui.h* to `#include "esp_brookesia.h"`.
- **Duplicate UI Components Files**: When a Squareline project uses `components` controls, the exported code will contain *ui_comp.c* and *ui_comp.h* files. To avoid function name conflicts, users should delete the *ui_comp.c* file and use the internal *ui_comp.c* file provided by esp-brookesia, and then delete the following content from *ui_comp.h*:

  ```c
  void get_component_child_event_cb(lv_event_t * e);
  void del_component_child_event_cb(lv_event_t * e);

  lv_obj_t * ui_comp_get_child(lv_obj_t * comp, uint32_t child_idx);
  extern uint32_t LV_EVENT_GET_COMP_CHILD;
  ```

- **Animation Usage Not Recommended**: It is advised not to create and use animations in Squareline, as these animation resources cannot be directly accessed or cleaned up automatically or manually by users or esp-brookesia, which may lead to crashes or memory leaks when the app exits. If animations must be used, users need to modify the animation resource creation part of the exported code, as shown in the [Squareline example](../src/app_examples/phone/squareline/), which manually records animation resources using `startRecordResource()` and `stopRecordResource()` to ensure they can be cleaned up automatically when the app closes.
- **Ensure Proper Compilation**: Furthermore, when using Squareline exported code, users may need to make additional modifications based on actual circumstances, such as renaming the `ui_init()` function to `<app_name>_ui_init()` to ensure the code compiles and runs correctly.
