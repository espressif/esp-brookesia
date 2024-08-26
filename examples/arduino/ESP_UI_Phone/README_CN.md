# ESP-UI 手机示例

该示例演示了如何使用 `esp-ui` 和 `ESP32_Display_Panel` 库在屏幕上显示 Phone UI。

该示例适用于分辨率为 `240 x 240` 或更高的触摸屏。如果分辨率较低，功能可能无法正常工作。默认样式确保了跨分辨率的兼容性，但在多种分辨率下显示效果可能不佳。建议使用与屏幕分辨率相同的用户界面样式表。如果没有合适的样式表，则需要手动调整。

## 使用方法

要使用此示例，请首先安装以下依赖库：

- ESP32_Display_Panel (0.1.5)
- lvgl (>= v8.3.9, < v9)

要使用外部样式表，还需安装以下依赖库，并将 `EXAMPLE_USE_EXTERNAL_STYLESHEET` 宏设置为 `1`：

- esp-ui-phone_480_480_stylesheet
- esp-ui-phone_800_480_stylesheet
- esp-ui-phone_1024_600_stylesheet

然后，按照以下步骤配置库并上传示例：

1. 对于 **esp-ui**：

    - [可选] 按照[步骤](../../../docs/how_to_use.md#configuration-instructions-1)配置库。

2. 对于 **ESP32_Display_Panel**：

    - [可选] 按照[步骤](https://github.com/esp-arduino-libs/ESP32_Display_Panel?tab=readme-ov-file#configuring-drivers)配置驱动程序。
    - [必需] 如果使用受支持的开发板，请按照[步骤](https://github.com/esp-arduino-libs/ESP32_Display_Panel?tab=readme-ov-file#using-supported-development-boards)配置它。
    - [必需] 如果使用自定义开发板，请按照[步骤](https://github.com/esp-arduino-libs/ESP32_Display_Panel?tab=readme-ov-file#using-custom-development-boards)配置它。

3. 对于 **lvgl**：

    - [可选] 按照[步骤](https://github.com/esp-arduino-libs/ESP32_Display_Panel?tab=readme-ov-file#configuring-lvgl)添加 *lv_conf.h* 文件并更改配置。
    - [必需] 在 *lv_conf.h* 文件中使能 `LV_USE_SNAPSHOT` 宏。
    - [可选] 修改 [lvgl_port_v8.h](./lvgl_port_v8.h) 文件中的宏，以配置 lvgl 端口参数。

4. 在 Arduino IDE 中导航到 `工具` 菜单，选择 ESP 板并配置其参数。**请确保分区表中 APP 分区至少有 2 MB 大小**。对于受支持的板，请参阅[配置受支持的开发板](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/Board_Instructions.md#recommended-configurations-in-the-arduino-ide)。
5. 验证并将示例上传到 ESP 板。

## 串口输出

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

## 技术支持与反馈

请使用以下反馈渠道：

- 有技术问题，请访问 [esp32.com](https://esp32.com/viewforum.php?f=35) 论坛。
- 如需提交功能请求或错误报告，请创建 [GitHub issue](https://github.com/espressif/esp-ui/issues)。

我们会尽快回复您。
