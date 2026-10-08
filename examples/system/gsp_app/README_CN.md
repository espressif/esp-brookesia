# System GSP App

[English](./README.md)

本示例面向配有 1024×600 屏幕的 ESP32-P4 Function EV Board。开机后进入启动页，可打开设置、调节亮度和声音、连接 Wi-Fi，并选择界面语言和主题。

界面在 menuconfig 里选择：

- **GSP**，默认。启动页与设置，包括亮度、Wi-Fi、声音、语言和主题。
- **LVGL**。原来的桌面，以及原来的设置界面。

工程按 ESP32-P4 revision v3.1 配置。

## 构建与烧录

```bash
idf.py gen-bmgr-config -b esp32_p4_function_ev
idf.py set-target esp32p4
idf.py build
idf.py -p <PORT> -b 921600 flash monitor
```

GSP 页面需要场景编译器。如果编译停下来并提示缺少它，可以安装工具，或把 `GSPC_EXECUTABLE` 指到 `gspc`：

```bash
python -m pip install -U esp-gsp-tools
```

## 切换界面

```bash
idf.py menuconfig
```

打开 `ESP-Brookesia: System Core Configurations`，修改 `GUI backend`，然后保存。再构建并烧录：

```bash
idf.py build
idf.py -p <PORT> -b 921600 flash
```

两种选择都使用 `partitions_16m.csv`。`sdkconfig.defaults` 里的 LVGL 选项在选中 LVGL 之后生效。

两种界面使用同一套能力配置和 AudioPlayback 服务。声音测试通过板卡共享的音频处理器播放本地 PCM/WAV 音频；高层录音接口默认关闭，原始编解码器接口仍可使用。本示例不提供语音、网络流媒体或视频功能。分区表和 GSP 字符覆盖不变。

GSP 编译场景在 Flash 中采用无损压缩。打开应用时，场景包解压到 PSRAM，关闭后释放；字体覆盖保持不变。如需使用未压缩场景，可在 **ESP-Brookesia: GUI GSP Configurations** 中关闭 **Compress embedded Brookesia GSP bundles**。

如果已有配置关闭了 **Audio Device → Audio Processor**，请在 menuconfig 的 **ESP-Brookesia: Hal Adaptor Configurations** 中启用它，关闭 **Audio Processor Recording**，并保留示例的其他能力默认值。切换 GUI 后端无需更改这些设置。

## 实际操作

内存日志结束后，屏幕上是 **Brookesia** 和一个 **Settings** 按钮。

```text
Settings
Display
拖动亮度滑块
Back
Exit
Settings
```

**Back** 回到设置首页。**Exit** 回到启动页。再次打开设置，亮度仍是刚才调的值。

设置首页可进入**声音**、**语言**和**主题**，显示页也提供主题入口。

选择 LVGL 时，板子先显示启动画面，然后进入桌面。从桌面打开设置。亮度走同一个显示服务。

## Wi-Fi

打开 **Settings → Wi-Fi**，开启 Wi-Fi 后扫描附近热点。使用 **Refresh** 重新扫描，通过 **Previous / Next** 浏览结果。页面显示各热点的安全状态、信号等级和 RSSI，以及当前连接状态。

**Back** 返回设置首页。扫描时离开页面会取消本次扫描；重新进入时读取当前服务状态，Wi-Fi 已开启则重新扫描。关闭 Wi-Fi 也会断开当前网络，开关偏好会在重启后保留。

点击热点即可连接。开放热点和已保存凭据的热点会直接连接。连接新的个人网络时，点击 **Edit password** 输入密码，确认后点击 **Connect**。键盘提供字母、数字、符号、空格和删除键，密码始终以掩码显示。点击 **Cancel** 只会取消输入，不会发起连接。

页面显示 **Connected to network** 后即可返回列表。连接失败后可重试。连接请求已提交时离开页面，Wi-Fi 服务会继续处理；重新进入页面可查看当前状态。**Disconnect** 断开当前连接，但保留已保存的凭据。成功连接的凭据由服务保存，重启后可自动重连。

暂不提供企业网络配置、中文输入和已保存网络管理，密码键盘支持可打印的 ASCII 字符。

P4 开发板通过 ESP-Hosted 使用配套 Wi-Fi 芯片。若提示服务不可用，请检查开发板配置和配套芯片固件。扫描失败可通过 **Refresh** 重试；结果为空表示未发现有名称的热点。

## 亮度

滑块是 0 到 100 的百分比，这个百分比会被保存。

退出设置不会关掉显示服务。再次打开时读取当前百分比，不会写成一个新的固定值。

重启后会恢复上次保存的百分比，然后把背光打开。还没有保存过时，第一次启动是 90。

灯的硬件范围比 0 到 100 窄，所以硬件日志里的数字可能和滑块不同。滑块上的数字是保存的百分比。

## 声音

连接开发板扬声器后，打开**设置 → 声音**，调节音量或切换静音。**试听提示音**会按当前设置播放一段短音频，静音时保持无声。若有其他音频正在播放或暂停，请先结束播放，再试听提示音。音量与静音由播放服务保存，并在重启后恢复；设备不可用或更新失败时，页面会显示提示。

构建后请使用完整的 `idf.py flash`，使文件系统包含测试音频。音频由构建过程在本地生成。

## 语言与主题

语言提供**英语**和**简体中文**，主题提供**浅色**和**深色**。选择后点击**立即重启**应用已保存的选项，或点击**稍后**继续使用当前外观。再次选择当前选项，可替换该设置的待生效选择。

重启后，启动页和全部设置页面都会使用选定的语言与主题，包括 Wi-Fi 状态和密码键盘。选择外观不会改变已保存的亮度、声音或 Wi-Fi 设置。密码键盘仍仅支持可打印的 ASCII 字符，文案切换不会启用中文输入法。

## 尚存的界面差异

GSP 启动页提供设置入口，LVGL 使用 Super 桌面。我的设备、时区、调试和已保存网络管理仍由 LVGL 提供。两种界面共用系统偏好及硬件服务。

## 启动日志

复位后会先打印内存日志，然后才出现启动页。示例会打开并关闭设置 22 次：2 次预热，20 次计量。这不会改已保存的亮度。

如果还想在启动时从服务侧改一次亮度，编辑 `main/main_gsp.cpp`：

```cpp
constexpr bool RUN_BRIGHTNESS_PROBE = true;
```

重新构建并烧录。这次会先改亮度，再把原来的百分比写回去，然后才开始内存日志。

如需执行 Wi-Fi 验收，在 menuconfig 的 **Brookesia system GSP example** 下开启 **Run Settings Wi-Fi hardware acceptance probe at startup**。探针使用真实扫描和模拟触摸，覆盖刷新、取消扫描、重新进入及 Wi-Fi 开关，并在预热后记录 20 轮内存数据。正常使用时请关闭此选项。

探针还会注入空结果、扫描失败和多语言名称样例，检查对应页面。这些带有标记的用例是合成数据；随后的刷新和循环扫描使用附近的真实热点。

选择 LVGL 后，同一示例菜单提供 **Run LVGL hardware acceptance probe at startup**。它检查桌面导航、Settings、亮度、Wi-Fi 和硬件信息查询，采集画面并记录 20 轮内存数据。两种验收探针默认都关闭。

两种探针都可选择密码与连接测试模式。请使用专用的个人测试网络，在串口提示就绪后发送一行包含 `ssid` 和 `password` 字段的 JSON。固件不会回显凭据，发送端也应关闭本地回显。GSP 先检查未保存热点的密码输入，再检查从列表直接连接期间关闭并重新开启 Wi-Fi；如果已有保存凭据，则只运行列表直连检查。LVGL 可先通过 Settings 删除测试热点的保存记录，再重新输入密码。成功连接后，凭据由正常的 Wi-Fi 服务保存。

请在屏幕上确认显示效果。串口日志记录服务状态和内存数据。

声音和外观验收可启用 GSP 探针下的 **Sound and appearance checks**。该模式检查页面导航、试听、静音恢复和内存，还会注入文案更新失败以检查返回、退出及重新打开，并提供临时串口命令 `touch X Y`、`frame`、`state` 供画面核对。LVGL 探针也会检查声音控制，并打开语言和主题选项。普通固件不包含这些可选探针。
