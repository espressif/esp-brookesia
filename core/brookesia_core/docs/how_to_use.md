# How to Use

* [中文版本](./how_to_use_CN.md)

## Table of Contents

- [How to Use](#how-to-use)
  - [Table of Contents](#table-of-contents)
  - [Configuration Instructions](#configuration-instructions)
  - [How to Develop App](#how-to-develop-app)
    - [Principles and Features](#principles-and-features)
    - [Considerations](#considerations)

## Configuration Instructions

When developing with esp-idf, users can configure brookesia_core through the menuconfig:

1. Run the command `idf.py menuconfig`.
2. Navigate to `Component config` > `ESP Brookesia - Core Configurations`.

## How to Develop App

### Principles and Features

brookesia_core provides the capability to develop apps through C++ inheritance, allowing users to inherit from the appropriate system app base class (e.g., the [esp_brookesia::systems::phone::App](../systems/phone/esp_brookesia_phone_app.hpp) class for Phone) and implement the necessary pure virtual functions (`run()` and `back()`). Users can also redefine other virtual functions as needed (e.g., `close()`, `init()`, and `pause()`), which are derived from the app base class [esp_brookesia::systems::base::App](../systems/base/esp_brookesia_base_app.hpp).

The system app base class in brookesia_core inherits from the app base class `esp_brookesia::systems::base::App`, which provides the following user-configurable features:

- **Name**: Set the app's name by configuring the `name` field in `esp_brookesia::systems::base::App::Config`.
- **Icon**: Set the app's icon by configuring the `launcher_icon` field in `esp_brookesia::systems::base::App::Config`.
- **Screen Size**: Set the app's screen size by configuring the `screen_size` field in `esp_brookesia::systems::base::App::Config`. It is recommended that the screen size equals the actual screen size (i.e., `screen_size = gui::StyleSize::RECT_PERCENT(100, 100)`) to avoid display anomalies in unused screen areas when the app's screen size is smaller.
- **Automatic Default Screen Creation**: Enable this feature by setting `enable_default_screen = 1` in `esp_brookesia::systems::base::App::Config`. The system will automatically create and load a default screen when the app executes `run()`, and users can retrieve the current screen object using `lv_scr_act()`. If users wish to create and load screen objects manually, this feature should be disabled.
- **Automatic Resource Cleanup**: Enable this feature by setting `enable_recycle_resource = 1` in `esp_brookesia::systems::base::App::Config`. The system will automatically clean up resources, including screens (`lv_obj_create(NULL)`), animations (`lv_anim_start()`), and timers (`lv_timer_create()`), when the app exits. This requires users to complete all resource creation within the `run()/resume()` function or between `startRecordResource()` and `stopRecordResource()`.
- **Automatic Screen Size Adjustment**: Enable this feature by setting `enable_resize_visual_area = 1` in `esp_brookesia::systems::base::App::Config`. The system will automatically adjust the screen size to the visual area size when the app creates a screen. This also requires users to complete all screen creation within the `run()/resume()` function or between `startRecordResource()` and `stopRecordResource()`. If the screen displays floating UI elements (e.g., status bar) and this feature is not enabled, the app's screen will default to full-screen size, which may obscure some areas. The app can call the `getVisualArea()` function to retrieve the final visual area.

Additionally, the system app base class in brookesia_core provides extra user-configurable features, exemplified by the Phone app base class `App`:

- **Icon Page Index**: Set the app icon's page index on the display screen by configuring the `app_launcher_page_index` field in `App::Config` (usually divided into three pages).
- **Status Icon Area Index**: Set the status icon's area index in the status bar by configuring the `status_icon_area_index` field in `App::Config` (usually divided into `left-middle-right` or `left-right` areas).
- **Status Icon**: Set the app's status icon by configuring the `status_icon` field in `App::Config`.
- **Status Bar Visual Mode**: Set the app's status bar visual mode by configuring the `status_bar_visual_mode` field in `App::Config`, supporting `fixed` and `hidden` modes.
- **Navigation Bar Visual Mode**: Set the app's navigation bar visual mode by configuring the `navigation_bar_visual_mode` field in `App::Config`, supporting `fixed`, `hidden`, and `flexible` modes.
- **Gesture Navigation**: Enable or disable gesture navigation for the app by configuring the `enable_navigation_gesture` field in `App::Config`, default only supporting opening the recents screen by swiping up from the bottom. You can enable side-swipe gestures for navigating back to the previous page by configuring the `enable_gesture_navigation_back` field in the `Manager::Data` of stylesheet.

### Considerations

If users adopt the "handwritten code" approach, they should enable **Automatic Default Screen Creation** (`enable_default_screen`) and consider the following:

- **Function Name Conflicts**: To avoid variable and function name conflicts between multiple apps, it is recommended that global variables and functions within the app use the "<app_name>_" prefix for naming.

If users adopt the "Squareline exported code" approach, they should disable **Automatic Default Screen Creation** (`enable_default_screen`) and use Squareline Studio version `v1.4.0` or higher, considering the following:

- **Function Name Conflicts**: To prevent variable and function name conflicts between multiple apps, it is recommended that all screen names use the "<app_name>_" prefix and set "Project Settings" > "Properties" > "Object Naming" in Squareline Studio to "[Screen_Widget]_Name" (this feature requires version >= `v1.4.0`).
- **Duplicate UI Helpers Files**: Since each Squareline exported code contains *ui_helpers.c* and *ui_helpers.h* files, users should delete these two files and use the internal *ui_helpers* file provided by brookesia_core. After deletion, they need to modify `#include "ui_helpers.h"` in *ui.h* to `#include "esp_brookesia.h"`.
- **Duplicate UI Components Files**: When a Squareline project uses `components` controls, the exported code will contain *ui_comp.c* and *ui_comp.h* files. To avoid function name conflicts, users should delete the *ui_comp.c* file and use the internal *ui_comp.c* file provided by brookesia_core, and then delete the following content from *ui_comp.h*:

  ```c
  void get_component_child_event_cb(lv_event_t * e);
  void del_component_child_event_cb(lv_event_t * e);

  lv_obj_t * ui_comp_get_child(lv_obj_t * comp, uint32_t child_idx);
  extern uint32_t LV_EVENT_GET_COMP_CHILD;
  ```

- **Animation Usage Not Recommended**: It is advised not to create and use animations in Squareline, as these animation resources cannot be directly accessed or cleaned up automatically or manually by users or brookesia_core, which may lead to crashes or memory leaks when the app exits. If animations must be used, users need to modify the animation resource creation part of the exported code, as shown in the [Squareline example](https://github.com/espressif/esp-brookesia/blob/master/apps/brookesia_app_squareline_demo/esp_brookesia_app_squareline_demo.cpp), which manually records animation resources using `startRecordResource()` and `stopRecordResource()` to ensure they can be cleaned up automatically when the app closes.
- **Ensure Proper Compilation**: Furthermore, when using Squareline exported code, users may need to make additional modifications based on actual circumstances, such as renaming the `ui_init()` function to `<app_name>_ui_init()` to ensure the code compiles and runs correctly.

> [!NOTE]
> 1. "Handwritten code" refers to the approach where users manually write code to implement GUI development, typically using `lv_scr_act()` to get the current screen object and adding all GUI elements to that screen object.
> 2. "Squareline exported code" refers to the method where users design GUIs using [Squareline Studio](https://squareline.io/), export the code, and then add the exported code to the app for GUI development.
