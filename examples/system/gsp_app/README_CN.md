# System GSP App

[English](./README.md)

本示例面向配有 1024×600 屏幕的 ESP32-P4 Function EV Board。开机后进入启动页，可打开设置、调节亮度和声音、连接 Wi-Fi，并选择界面语言和主题。

界面在 menuconfig 里选择：

- **GSP**，默认。启动页与设置，包括亮度、Wi-Fi、声音、语言和主题。
- **LVGL**。原来的桌面，以及原来的设置界面。

工程按 ESP32-P4 revision v3.1 配置。

开发板运行时还可以通过 USB 安装应用，它们会显示在启动页的设置按钮旁，详见[可安装应用](#可安装应用)。

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

两种选择都使用 `partitions_16m.csv`。选中 LVGL 后，`sdkconfig.defaults` 中的 LVGL 选项才会生效。

### 文件系统资源

`littlefs_data` 镜像由 `build/littlefs_stage` 生成；本示例不再使用源码树中的 `littlefs/` 目录生成该镜像。

选择 GSP 时，镜像包含声音测试 WAV、Settings 应用名称所需的翻译文件，以及配置为预装的 GSP 应用。GSP 场景及其字体由 GSP bundle 提供；镜像不包含 LVGL 桌面（`system/super`）、LVGL 公共字体（`system/fonts`）和 Settings 的 LVGL 页面。选择 LVGL 时，镜像会包含桌面资源和完整的 Settings 包。

### 切换 backend 后烧录

`idf.py app-flash` 只更新应用分区，不会更新 `littlefs_data`。要把所选 backend 对应的文件系统镜像写入开发板，请先构建，再完整烧录：

```bash
idf.py build
idf.py -p <PORT> -b 921600 flash
```

完整烧录会替换整个 LittleFS 分区，其中已安装的应用及其数据也会被删除。烧录前请备份需要保留的内容。

切换到 GSP 后，第一次构建可能因旧配置尚未解析 GSP 场景组件而停止。再运行一次 `idf.py build` 即可完成配置和构建。

两种界面使用同一套能力配置和 AudioPlayback 服务。声音测试通过板卡共享的音频处理器播放本地 PCM/WAV 音频；高层录音接口默认关闭，原始编解码器接口仍可使用。本示例不提供语音、网络流媒体或视频功能。分区表和 GSP 字符覆盖不变。

GSP 编译场景在 Flash 中采用无损压缩。打开应用时，场景包解压到 PSRAM，关闭后释放；字体覆盖保持不变。如需使用未压缩场景，可在 **ESP-Brookesia: GUI GSP Configurations** 中关闭 **Compress embedded Brookesia GSP bundles**。

如果已有配置关闭了 **Audio Device → Audio Processor**，请在 menuconfig 的 **ESP-Brookesia: Hal Adaptor Configurations** 中启用它，关闭 **Audio Processor Recording**，并保留示例的其他能力默认值。切换 GUI 后端无需更改这些设置。

## 实际操作

启动完成后，System Core 公共主页列出已安装应用，并提供 **Settings** 入口。

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

## 可安装应用

系统主页每页显示六个应用，通过 **Previous / Next** 浏览更多应用。开发板运行时即可通过 USB 安装、更新和卸载，无需重新编译固件，重启后应用依然保留。

```bash
python -m pip install brookesia-usb
brookesia-usb install <package>.bpk
```

安装包只能装到它所面向的界面上：GSP 安装包需要使用 GSP 界面。[Device Control](../../app/gsp_js_device_control/README_CN.md) 是一个现成的示例，可以读取并调节亮度。

已安装的应用保存在 LittleFS 分区。完整执行 `idf.py flash` 会重写该分区，删除这些应用及其数据；只更新固件并保留应用请使用 `idf.py app-flash`。

如需将 Device Control 直接编入固件，在 menuconfig 中开启 **Brookesia system GSP example → Preinstall the Device Control app**，再完整执行 `idf.py flash`。该选项默认关闭。

## Wi-Fi

打开 **Settings → Wi-Fi**，开启 Wi-Fi 后扫描附近热点。使用 **Refresh** 重新扫描，通过 **Previous / Next** 浏览结果。页面显示各热点的安全状态、信号等级和 RSSI，以及当前连接状态。

**Back** 返回设置首页。扫描时离开页面会取消本次扫描；重新进入时读取当前服务状态，Wi-Fi 已开启则重新扫描。关闭 Wi-Fi 也会断开当前网络，开关偏好会在重启后保留。

点击热点即可连接。开放热点和已保存凭据的热点会直接连接。连接新的个人网络时，点击 **Edit password** 输入密码，确认后点击 **Connect**。键盘提供字母、数字、符号、空格和删除键，密码始终以掩码显示。点击 **Cancel** 只会取消输入，不会发起连接。

页面显示 **Connected to network** 后即可返回列表。连接失败后可重试。连接请求已提交时离开页面，Wi-Fi 服务会继续处理；重新进入页面可查看当前状态。**Disconnect** 断开当前连接，但保留已保存的凭据。成功连接的凭据由服务保存，重启后可自动重连。

点击**已保存网络**，选择目标 SSID，再点击**忘记**删除保存凭据。删除当前网络也会断开连接。**附近网络**返回扫描结果。暂不提供企业网络配置和中文输入，公共系统键盘支持可打印的 ASCII 密码。

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

## 校时

宿主和 Super 一样,在运行期间一直持有 SNTP 服务,所以 Wi-Fi 连上后无论先打开哪个 App,系统时间都会通过网络校准。没有已保存的 Wi-Fi 网络时时间不会设置,时钟等 App 会显示 1970 年。

## 尚存的界面差异

System Core 提供 GSP 系统主页及公共键盘、消息框；本示例配置设备并启动系统。Settings 提供我的设备、带实时 SNTP 状态的时区页和按目标选择的已保存网络删除。高级调试仍由 LVGL 提供，两套界面共用系统偏好和硬件服务。

## 诊断

正常启动会直接进入启动页。下面的检查用于开发，默认关闭，可在 menuconfig 的 **Brookesia system GSP example** 下开启。

| 选项 | 作用 |
| --- | --- |
| **Open and close Settings repeatedly at startup to log heap use** | 打开并关闭设置 22 次，其中 2 次预热、20 次计量，在启动页出现前记录内存 |
| **Log heap across short start and exit cycles of the installed GSP JS app** | 对已安装的 Device Control 应用做同样的检查，共 5 轮 |
| **Let USB callers inject touches and print the screen** | 增加 `GspProbe` 服务，提供 `Tap`、`Drag`、`Component` 和 `Frame`，可通过 `brookesia-usb call` 调用。`Frame` 在 USB 命令结束后把帧缓冲输出到串口。启动流程和应用安装不变 |
| **Run Settings Wi-Fi hardware acceptance probe at startup** | 使用真实扫描和模拟触摸，覆盖刷新、取消扫描、重新进入和 Wi-Fi 开关，并记录 20 轮内存。还会显示合成的空结果、扫描失败和多语言名称 |
| **Run Settings sound and appearance checks instead of scan checks** | 需先开启 Wi-Fi 探针。检查页面导航、试听、静音恢复和内存，并提供临时串口命令 `touch X Y`、`frame`、`state` |
| **Run password and connection checks instead of scan checks** | 需先开启 Wi-Fi 探针。测试密码输入和连接网络 |
| **Run LVGL hardware acceptance probe at startup** | 面向 LVGL 界面：检查桌面导航、设置、亮度、Wi-Fi、声音、语言和主题，采集画面并记录 20 轮内存。再开启 **Run LVGL password and connection checks instead of desktop checks** 可加入连接测试 |

连接检查需要一个专用的个人测试网络。串口提示就绪后，发送一行包含 `ssid` 和 `password` 字段的 JSON。固件不会回显凭据，发送端也请关闭本地回显。成功连接后，凭据由正常的 Wi-Fi 服务保存。

如果还想在启动时从服务侧改一次亮度，把 `main/main_gsp.cpp` 里的 `RUN_BRIGHTNESS_PROBE` 设为 `true`，探针结束后会把原来的百分比写回去。

请在屏幕上确认显示效果，串口日志记录服务状态和内存数据。
