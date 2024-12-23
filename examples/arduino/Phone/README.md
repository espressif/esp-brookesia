# ESP-Brookesia Phone Example

The example demonstrates how to use the `esp-brookesia` and `ESP32_Display_Panel` libraries to display phone UI on the screen.

The example is suitable for touchscreens with a resolution of `240 x 240` or higher. Otherwise, the functionality may not work properly. The default style ensures compatibility across resolutions, but the display effect may not be optimal on many resolutions. It is recommended to use a UI stylesheet with the same resolution as the screen. If no suitable stylesheet is available, it needs to be adjusted manually.

## How to Use

To use this example, please firstly install the following dependent libraries:

- ESP32_Display_Panel (0.2.*)
- ESP32_IO_Expander (0.1.*)
- lvgl (>= v8.3.9, < v9)

Then, follow the steps below to configure the libraries and upload the example:

1. For **esp-brookesia**:

    - [optional] Follow the [steps](../../../docs/how_to_use.md#configuration-instructions-1) to configure the library.

2. For **ESP32_Display_Panel**:

    - [optional] Follow the [steps](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/How_To_Use.md#configuring-drivers) to configure drivers if needed.
    - [mandatory] If using a supported development board, follow the [steps](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/How_To_Use.md#using-supported-development-boards) to configure it.
    - [mandatory] If using a custom board, follow the [steps](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/How_To_Use.md#using-custom-development-boards) to configure it.

3. For **lvgl**:

    - [mandatory] Follow the [steps](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/How_To_Use.md#configuring-lvgl) to add *lv_conf.h* file and change the configurations.
    - [mandatory] Enable the `LV_USE_SNAPSHOT` macro in the *lv_conf.h* file.
    - [optional] Modify the macros in the [lvgl_port_v8.h](./lvgl_port_v8.h) file to configure the lvgl porting parameters.

4. Navigate to the `Tools` menu in the Arduino IDE to choose a ESP board and configure its parameters. **Please ensure that the size of APP partition in the partition table is enough (e.g. 4 MB)**. For supported boards, please refter to [Configuring Supported Development Boards](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/How_To_Use.md#configuring-supported-development-boards)
5. Verify and upload the example to the ESP board.

## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=22) forum.
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-brookesia/issues).

We will get back to you as soon as possible.
