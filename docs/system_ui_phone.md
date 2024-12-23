# Phone

* [中文版本](./system_ui_phone_CN.md)

[Phone](../src/systems/phone/) is a smartphone-like system UI. The running effects on `ESP Development Boards` and `PC` are shown below:

<div align="center">
    <img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_demo_esp.jpg" alt ="Running on ESP Development Boards" width="750">
</div>

<div align="center">
    Running on ESP Development Boards
</div>
<br>

<div align="center">
    <img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_pc_1024_600_3.gif" alt ="Running on PC Simulator" width="750">
</div>

<div align="center">
    Running on PC Simulator</a> (<a href="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_pc_1024_600_1.mp4">Click to view the video</a>)
</div>
<br>

It has the following features:

- **Hardware Requirements**: Suitable for screens with **touch**, **mouse**, or other [pointer-type input devices](https://docs.lvgl.io/master/porting/indev.html#touchpad-mouse-or-any-pointer).
- **Multi-Resolution Support**: The default style supports resolutions of `240 x 240` and above, with automatic adjustment of GUI element sizes according to the resolution.
- **Background Management**: Supports the coexistence and management of multiple Apps in the background.
- **Navigation Bar and Gesture Control**: Supports switching between interfaces within the app through the navigation bar or gesture control.

> [!NOTE]
> * The default style of Phone is designed for resolution compatibility, so the display may not be optimal at many resolutions. It is recommended to use a UI stylesheet that matches the screen's resolution. If no suitable stylesheet is available, manual adjustments may be required.
> * The "Recents Screen" in Phone, which displays screenshots of App GUIs, requires enabling the `LV_USE_SNAPSHOT` configuration in LVGL and sufficient memory space; otherwise, screenshots will be replaced with App icons.

To achieve the best display performance at fixed resolutions, esp-brookesia also provides UI stylesheets for the following resolutions:

- [320 x 240](../src/systems/phone/stylesheets/320_240/)
- [320 x 480](../src/systems/phone/stylesheets/320_480/)
- [480 x 480](../src/systems/phone/stylesheets/480_480/)
- [720 x 1280](../src/systems/phone/stylesheets/720_1280/)
- [800 x 480](../src/systems/phone/stylesheets/800_480/)
- [800 x 1280](../src/systems/phone/stylesheets/800_1280/)
- [1024 x 600](../src/systems/phone/stylesheets/1024_600/)
- [1280 x 800](../src/systems/phone/stylesheets/1280_800/)

> [!NOTE]
> If your screen resolution is not listed in the above UI stylesheets, please refer to the GitHub issue - [Phone Resolution Support](https://github.com/espressif/esp-brookesia/issues/5).
