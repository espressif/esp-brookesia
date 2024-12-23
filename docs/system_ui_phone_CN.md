# Phone

* [English Version](./system_ui_phone.md)

[Phone](../src/systems/phone/) 是一个类似于智能手机的系统 UI，在 `ESP 开发板` 和 `PC` 上的运行效果如下所示：

<div align="center">
    <img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_demo_esp.jpg" alt ="运行在 ESP 开发板" width="750">
</div>

<div align="center">
    运行在 ESP 开发板
</div>
<br>

<div align="center">
    <img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_pc_1024_600_3.gif" alt ="运行在 PC 模拟器" width="800">
</div>

<div align="center">
    运行在 PC 模拟器</a>（<a href="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_pc_1024_600_1.mp4">点击查看视频</a>）
</div>
<br>

具有以下特性：

- **硬件需求**：适用于具有 **触摸**、**鼠标** 或其他 [指针型输入设备](https://docs.lvgl.io/master/porting/indev.html#touchpad-mouse-or-any-pointer) 的屏幕。
- **多分辨率支持**：默认样式支持 `240 x 240` 及以上分辨率，并能根据分辨率自动调整 GUI 元素大小。
- **后台管理**：支持后台多 App 的共存与管理。
- **导航栏和手势控制**：支持在 App 内通过导航栏或手势控制界面的切换。

> [!NOTE]
> * Phone 的默认样式为了确保分辨率的兼容性，在许多分辨率上的显示效果可能不是最佳的，因此推荐使用与屏幕相同分辨率的 UI 样式表，如无合适样式表则需自行调整。
> * Phone 中 "Recents Screen" 显示 App GUI 截图的功能需要使能 LVGL 的 `LV_USE_SNAPSHOT` 配置，并要求提供充足的内存空间，否则截图会被替换成 App 图标。

为了使 Phone 在固定分辨率下达到最佳显示效果，esp-brookesia 目前提供了以下分辨率的 UI 样式表：

- [320 x 240](../src/systems/phone/stylesheets/320_240/)
- [320 x 480](../src/systems/phone/stylesheets/320_480/)
- [480 x 480](../src/systems/phone/stylesheets/480_480/)
- [720 x 1280](../src/systems/phone/stylesheets/720_1280/)
- [800 x 480](../src/systems/phone/stylesheets/800_480/)
- [800 x 1280](../src/systems/phone/stylesheets/800_1280/)
- [1024 x 600](../src/systems/phone/stylesheets/1024_600/)
- [1280 x 800](../src/systems/phone/stylesheets/1280_800/)

> [!NOTE]
> 如果上述 UI 样式表中没有您的屏幕分辨率，请参阅 Github issue - [Phone Resolution Support](https://github.com/espressif/esp-brookesia/issues/5)。
