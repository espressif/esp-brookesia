# Phone

* [中文版本](./system_ui_phone_CN.md)

## Introduction

[Phone](../systems/phone/) is a smartphone-like system UI that runs on ESP development boards with various screen sizes and resolutions as shown below:

<div align="center">
    <img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_demo_esp.jpg" alt ="Running Effect" width="750">
</div>

It has the following features:

- **Hardware Requirements**: Suitable for screens with **touch**, **mouse**, or other [pointer-type input devices](https://docs.lvgl.io/master/porting/indev.html#touchpad-mouse-or-any-pointer).
- **Multi-Resolution Support**: The default style supports resolutions of `240 x 240` and above, with automatic adjustment of GUI element sizes according to the resolution.
- **Background Management**: Supports the coexistence and management of multiple Apps in the background.
- **Navigation Bar and Gesture Control**: Supports switching between interfaces within the app through the navigation bar or gesture control.

> [!NOTE]
> * The default style of Phone is designed for resolution compatibility, so the display may not be optimal at many resolutions. It is recommended to use a UI stylesheet that matches the screen's resolution. If no suitable stylesheet is available, manual adjustments may be required.
> * The "Recents Screen" in Phone, which displays screenshots of App GUIs, requires enabling the `LV_USE_SNAPSHOT` configuration in LVGL and sufficient memory space; otherwise, screenshots will be replaced with App icons.

To achieve the best display performance at fixed resolutions, esp-brookesia also provides UI stylesheets for the following resolutions:

- [320 x 240](../systems/phone/stylesheets/320_240/)
- [320 x 480](../systems/phone/stylesheets/320_480/)
- [480 x 480](../systems/phone/stylesheets/480_480/)
- [720 x 1280](../systems/phone/stylesheets/720_1280/)
- [800 x 480](../systems/phone/stylesheets/800_480/)
- [800 x 1280](../systems/phone/stylesheets/800_1280/)
- [1024 x 600](../systems/phone/stylesheets/1024_600/)
- [1280 x 800](../systems/phone/stylesheets/1280_800/)

> [!NOTE]
> If your screen resolution is not listed in the above UI stylesheets, please refer to the GitHub issue - [Phone Resolution Support](https://github.com/espressif/esp-brookesia/issues/5).

## Built-in UI Components

### Status Bar

The [Status Bar](../systems/phone/widgets/status_bar/) displays time, battery level, WiFi status, and app-specific icons. The appearance is shown in the image below and it has the following features:

<div align="center">
    <img src="_static/readme/status_bar_demo.png" alt="status_bar_demo" width="600">
</div>

- **Position**: Located at the top of the screen.
- **Status Icons**: Each icon supports up to 6 images with adaptive scaling, allowing the use of images different from the stylesheet size.
- **Time Information**: Allows setting the system time, 12h formatted as `HH:MM AM/PM` and 24h formatted as `HH:MM`.
- **Battery Information**: Allows setting the battery status, including both percentage and status icon.
- **WiFi Status**: Allows setting the WiFi connection status, including a status icon.

### App Launcher

The [App Launcher](../systems/phone/widgets/app_launcher/) displays all installed app icons. The appearance is shown in the image below and it has the following features:

<div align="center">
    <img src="_static/readme/app_launcher_demo.png" alt="app_launcher_demo" width="600">
</div>

- **Position**: Located in the center of the screen.
- **App Icons**: Each app icon supports up to one image with adaptive scaling, allowing the use of images different from the stylesheet size.
- **Multi-Page Display**: Switch between pages by swiping left or right.
- **Page Indicator**: Located at the bottom of the widget, indicating the current page position.

### Navigation Bar

The [Navigation Bar](../systems/phone/widgets/navigation_bar/) provides navigation buttons. The appearance is shown in the image below and it has the following features:

<div align="center">
    <img src="_static/readme/navigation_bar_demo.png" alt="navigation_bar_demo" width="600">
</div>

- **Position**: Located at the bottom of the screen.
- **Navigation Buttons**: Provides three buttons: "Back," "Display," and "Recents Screen," which control interface switching by clicking.
- **Dynamic Adjustment**: Supports adjusting the button position order through stylesheet parameters.

### Recents Screen

The [Recents Screen](../systems/phone/widgets/recents_screen/) displays the currently running apps. The appearance is shown in the image below and it has the following features:

<div align="center">
    <img src="_static/readme/recents_screen_demo.png" alt="recents_screen_demo" width="600">
</div>

- **Position**: Located in the center of the screen.
- **System Storage Information**: Displays the remaining and total storage space.
- **Background Apps**: Displays the GUI screenshots of the current background apps with adaptive scaling.
- **Gesture Control**: Switch apps by swiping left or right, and clear background apps by swiping up or down.
- **One-Tap Clean**: Supports cleaning all background apps with one tap.

### Gesture

[Gesture](../systems/phone/widgets/gesture/) is used to capture gesture information from input devices, with the following features:

- **Indicator Bar**:

    - **Position**: Located at the `left edge`, `right edge`, and `bottom edge` of the screen.
    - **Display**: Hidden by default, it appears when an edge gesture is detected and changes as the gesture moves in a specific direction.

- **Gesture Information**

    - **Type**: Only supports linear gestures between the start and end points.
    - **Direction**: Includes four directions: Up, Down, Left, and Right.
    - **Position**: Includes five positions: Center, Top Edge, Bottom Edge, Left Edge, and Right Edge.
    - **Coordinates**: Includes the coordinates of the start and end points.
    - **Time**: Includes the time interval between the start and end.
    - **Speed**: Includes the speed of the gesture.
    - **Angle**: Includes the angle of the gesture.
