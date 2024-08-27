# Phone

* [中文版本](./system_ui_phone_CN.md)

[Phone](../src/systems/phone/) is a smartphone-like system UI. The running effects on `ESP Development Boards` and `PC` are shown below:

<div align="center"><img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_demo_esp.jpg" alt ="Running on ESP Development Boards" width="750"></div>

<p align="center">
Running on ESP Development Boards
</p>
<br>

<p align="center">
<img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_pc_1024_600_3.gif" alt ="Running on PC Simulator" width="750">
</p>

<p align="center">
Running on <a href="https://github.com/esp-arduino-libs/esp-ui-simulator_vscode">PC Simulator</a> (<a href="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_pc_1024_600_1.mp4">Click to view the video</a>)
</p>
<br>

It has the following features:

- **Hardware Requirements**: Suitable for screens with **touch**, **mouse**, or other [pointer-type input devices](https://docs.lvgl.io/master/porting/indev.html#touchpad-mouse-or-any-pointer).
- **Multi-Resolution Support**: The default style supports resolutions of `240 x 240` and above, with automatic adjustment of GUI element sizes according to the resolution.
- **Background Management**: Supports the coexistence and management of multiple Apps in the background.
- **Navigation Bar and Gesture Control**: Supports switching between interfaces within the app through the navigation bar or gesture control.

> [!NOTE]
> * The default style of Phone is designed for resolution compatibility, so the display may not be optimal at many resolutions. It is recommended to use a UI stylesheet that matches the screen's resolution. If no suitable stylesheet is available, manual adjustments may be required.
> * The "Recents Screen" in Phone, which displays screenshots of App GUIs, requires enabling the `LV_USE_SNAPSHOT` configuration in LVGL and sufficient memory space; otherwise, screenshots will be replaced with App icons.

To achieve the best display performance at fixed resolutions, Espressif also provides UI stylesheets for the following resolutions:

|  Resolution  | link |
| ------------ | ---- |
|  320 x 240   |  [Github](https://github.com/esp-arduino-libs/esp-ui-phone_320_240_stylesheet) / [Espressif](https://components.espressif.com/components/espressif/esp-ui-phone_320_240_stylesheet)  |
|  480 x 480   |  [Github](https://github.com/esp-arduino-libs/esp-ui-phone_480_480_stylesheet) / [Espressif](https://components.espressif.com/components/espressif/esp-ui-phone_480_480_stylesheet) |
|  800 x 480   |  [Github](https://github.com/esp-arduino-libs/esp-ui-phone_800_480_stylesheet) / [Espressif](https://components.espressif.com/components/espressif/esp-ui-phone_800_480_stylesheet) |
| 1024 x 600   |  [Github](https://github.com/esp-arduino-libs/esp-ui-phone_1024_600_stylesheet) / [Espressif](https://components.espressif.com/components/espressif/esp-ui-phone_1024_600_stylesheet) |

> [!NOTE]
> * If your screen resolution is not listed in the above UI stylesheets, please refer to the GitHub issue - [Phone Resolution Support](https://github.com/espressif/esp-ui/issues/5).
