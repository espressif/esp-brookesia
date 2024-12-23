# ESP-Brookesia 手机示例

该示例演示了如何使用 `esp-brookesia` 和 `ESP32_Display_Panel` 库在屏幕上显示 Phone UI。

该示例适用于分辨率为 `240 x 240` 或更高的触摸屏。如果分辨率较低，功能可能无法正常工作。默认样式确保了跨分辨率的兼容性，但在多种分辨率下显示效果可能不佳。建议使用与屏幕分辨率相同的用户界面样式表。如果没有合适的样式表，则需要手动调整。

## 使用方法

要使用此示例，请首先安装以下依赖库：

- ESP32_Display_Panel (0.2.1)
- ESP32_IO_Expander (0.1.0)
- lvgl (>= v8.3.9, < v9)

然后，按照以下步骤配置库并上传示例：

1. 对于 **esp-brookesia**：

    - [可选] 按照[步骤](../../../docs/how_to_use.md#configuration-instructions-1)配置库。

2. 对于 **ESP32_Display_Panel**：

    - [可选] 按照[步骤](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/How_To_Use.md#configuring-drivers)配置驱动程序。
    - [必需] 如果使用受支持的开发板，请按照[步骤](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/How_To_Use.md#using-supported-development-boards)配置它。
    - [必需] 如果使用自定义开发板，请按照[步骤](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/How_To_Use.md#using-custom-development-boards)配置它。

3. 对于 **lvgl**：

    - [必需] 按照[步骤](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/How_To_Use.md#configuring-lvgl)添加 *lv_conf.h* 文件并更改配置。
    - [必需] 在 *lv_conf.h* 文件中使能 `LV_USE_SNAPSHOT` 宏。
    - [可选] 修改 [lvgl_port_v8.h](./lvgl_port_v8.h) 文件中的宏，以配置 lvgl 端口参数。

4. 在 Arduino IDE 中导航到 `工具` 菜单，选择 ESP 板并配置其参数。**请确保分区表中 APP 分区足够大（如 4 MB）**。对于受支持的板，请参阅[配置受支持的开发板](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/How_To_Use.md#configuring-supported-development-boards)。
5. 验证并将示例上传到 ESP 板。

## 技术支持与反馈

请使用以下反馈渠道：

- 有技术问题，请访问 [esp32.com](https://esp32.com/viewforum.php?f=35) 论坛。
- 如需提交功能请求或错误报告，请创建 [GitHub issue](https://github.com/espressif/esp-brookesia/issues)。

我们会尽快回复您。
