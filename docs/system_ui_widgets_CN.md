# 系统 UI 控件

* [English Version](./system_ui_widgets.md)

## Status Bar

[Status Bar](../src/widgets/status_bar/) 用于显示时间、电量、WiFi 状态以及 app 指定图标，运行效果如下图所示，具有以下特点：

<div align="center">
    <img src="./_static/readme/status_bar_demo.png" alt="status_bar_demo" width="600">
</div>

- **位置**：屏幕顶部。
- **状态图标**：每个图标支持最多 6 张图片，并且支持自适应缩放，允许使用不同于样式表大小的图片。
- **时间信息**：允许设置系统时间，12h 格式为 `HH:MM AM/PM`, 24h 格式为 `HH:MM`。
- **电量信息**：允许设置电量状态，包含百分比和状态图标。
- **WiFi 状态**：允许设置 WiFi 连接状态，包含状态图标。

## App Launcher

[App Launcher](../src/widgets/app_launcher/) 用于显示所有已安装 app 的图标，运行效果如下图所示，具有以下特点：

<div align="center">
    <img src="./_static/readme/app_launcher_demo.png" alt="app_launcher_demo" width="600">
</div>

- **位置**：屏幕中间。
- **App 图标**：每个 app 图标支持最多一张图片，并且支持自适应缩放，允许使用不同于样式表大小的图片。
- **多页显示**：通过左右滑动切换页面。
- **页面指示器**：位于控件的底部，指示当前页面的位置。

## Navigation Bar

[Navigation Bar](../src/widgets/navigation_bar/) 提供导航按键，运行效果如下图所示，具有以下特点：

<div align="center">
    <img src="./_static/readme/navigation_bar_demo.png" alt="navigation_bar_demo" width="600">
</div>

- **位置**：屏幕底部。
- **导航按键**：提供 "后退"、"主页"、"概览屏幕" 三种按键，通过点击控制界面切换。
- **动态调整**：支持通过样式表参数调整按键位置顺序。

## Recents Screen

[Recents Screen](../src/widgets/recents_screen/) 用于显示正在运行中的 App，运行效果如下图所示，具有以下特点：

<div align="center">
    <img src="./_static/readme/recents_screen_demo.png" alt="recents_screen_demo" width="600">
</div>

- **位置**：屏幕中间。
- **系统存储信息**：显示剩余和总存储空间大小。
- **后台 App**：显示当前后台 app 的 GUI 截图，并且支持自适应缩放。
- **手势控制**：通过左右滑动切换 App，上下滑动清理后台 App。
- **一键清理**：支持一键清理所有后台 App。

## Gesture

[Gesture](../src/widgets/gesture/) 用于获取输入设备的手势信息，具有以下特点：

- **指示条**

    - **位置**：位于屏幕 `左边缘`、`右边缘` 和 `下边缘`。
    - **显示**：默认隐藏，当检测到边缘位置触发手势动作时显示，并且随着手势在特定方向上移动而变化

- **手势信息**

    - **类型**：仅支持起始点和终止点之间的直线手势。
    - **方向**：包含 `上`、`下`、`左`、`右` 四个方向。
    - **位置**：包含 `中间`、`上边缘`、`下边缘`、`左边缘`、`右边缘` 五个位置。
    - **坐标**：包含起始点和终止点的坐标。
    - **时间**：包含手势的起始与终止的时间间隔。
    - **速度**：包含手势的速度。
    - **角度**：包含手势的角度。
