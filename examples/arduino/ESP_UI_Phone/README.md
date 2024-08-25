# ESP-UI Phone Example

The example demonstrates how to use the `esp-ui` and `ESP32_Display_Panel` libraries to display phone UI on the screen.

The example is suitable for touchscreens with a resolution of `240 x 240` or higher. Otherwise, the functionality may not work properly. The default style ensures compatibility across resolutions, but the display effect may not be optimal on many resolutions. It is recommended to use a UI stylesheet with the same resolution as the screen. If no suitable stylesheet is available, it needs to be adjusted manually.

## How to Use

To use this example, please firstly install the following dependent libraries:

- ESP32_Display_Panel  (0.1.5)
- lvgl (>= v8.3.9, < v9)

To use the external stylesheets, please also install the following dependent libraries and set the `EXAMPLE_USE_EXTERNAL_STYLESHEET` macro to `1`:

- esp-ui-phone_480_480_stylesheet
- esp-ui-phone_800_480_stylesheet
- esp-ui-phone_1024_600_stylesheet

Then, follow the steps below to configure the libraries and upload the example:

1. For **esp-ui**:

    - [optional] Follow the [steps](../../../docs/how_to_use.md#configuration-instructions-1) to configure the library.

2. For **ESP32_Display_Panel**:

    - [optional] Follow the [steps](https://github.com/esp-arduino-libs/ESP32_Display_Panel?tab=readme-ov-file#configuring-drivers) to configure drivers.
    - [mandatory] If using a supported development board, follow the [steps](https://github.com/esp-arduino-libs/ESP32_Display_Panel?tab=readme-ov-file#using-supported-development-boards) to configure it.
    - [mandatory] If using a custom board, follow the [steps](https://github.com/esp-arduino-libs/ESP32_Display_Panel?tab=readme-ov-file#using-custom-development-boards) to configure it.

3. For **lvgl**:

    - [optional] Follow the [steps](https://github.com/esp-arduino-libs/ESP32_Display_Panel?tab=readme-ov-file#configuring-lvgl) to add *lv_conf.h* file and change the configurations.
    - [mandatory] Enable the `LV_USE_SNAPSHOT` macro in the *lv_conf.h* file.
    - [optional] Modify the macros in the [lvgl_port_v8.h](./lvgl_port_v8.h) file to configure the lvgl porting parameters.

4. Navigate to the `Tools` menu in the Arduino IDE to choose a ESP board and configure its parameters. **Please ensure that the APP partition in the partition table is at least 2 MB in size**. For supported boards, please refter to [Configuring Supported Development Boards](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/Board_Instructions.md#recommended-configurations-in-the-arduino-ide)
5. Verify and upload the example to the ESP board.

## Serial Output

```bash
...
ESP-UI Phone example start
Initialize panel device
Initialize lvgl
Create ESP-UI Phone
[WARN] [esp_ui_template.hpp:107](addStylesheet): Display is not set, use default display
[INFO] [esp_ui_core.cpp:150](beginCore): Library version: 0.1.0
[WARN] [esp_ui_phone_manager.cpp:73](begin): No touch device is set, try to use default touch device
[WARN] [esp_ui_phone_manager.cpp:77](begin): Using default touch device(@0x0x3fcede40)
ESP-UI Phone example end
  Biggest /     Free /    Total
SRAM : [  253940 /   263908 /   387916]
PSRAM : [ 7864308 /  7924256 /  8388608]
...
```

## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=22) forum.
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-ui/issues).

We will get back to you as soon as possible.
