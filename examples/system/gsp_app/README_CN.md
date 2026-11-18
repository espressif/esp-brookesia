# System GSP App

[English](./README.md)

本示例面向配有 1024×600 屏幕的 ESP32-P4 Function EV Board。开机后进入启动页，可打开设置、调节亮度和声音、连接 Wi-Fi，并选择界面语言和主题。

支持两种 backend 配置：

- **GSP**，默认。启动页与设置，包括亮度、Wi-Fi、声音、语言和主题。
- **LVGL**。LVGL 桌面和设置界面。

默认 P4 配置面向芯片 revision v3.1 或更新版本。P4 v1.3 请使用下方的[旧版 P4 配置](#p4-revision-v13)。

开发板运行时还可以通过 USB 安装应用，它们会显示在启动页的设置按钮旁，详见[可安装应用](#可安装应用)。

## 构建与烧录

先激活支持的 ESP-IDF 环境（6.0 或更新版本），以下命令均在本示例目录执行。使用 GSP 时，先通过 `python -m pip install -U esp-gsp-tools` 安装编译工具，或通过 `-D GSPC_EXECUTABLE=/path/to/gspc` 指定兼容编译器，详见 [GSP 应用环境准备](../../app/gsp_js_device_control/README_CN.md#环境要求)。LVGL 不需要 GSP 编译器。

每种板型与 backend 组合都使用新的输出目录。已有 `sdkconfig` 中的选择优先于 defaults，仅更换 defaults 不能将已有输出切换为另一板型。

### P4X（revision v3.1 或更新版本）

生成 P4X 板级输入，再选择一个 backend：

```bash
idf.py gen-bmgr-config -b esp32_p4x_function_ev
idf.py -B build-gsp -D IDF_TARGET=esp32p4 -D SDKCONFIG_DEFAULTS=sdkconfig.defaults build
idf.py -B build-lvgl -D IDF_TARGET=esp32p4 \
    -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ci.lvgl' build
```

`sdkconfig.defaults.esp32p4` 和 `sdkconfig.ci.lvgl` 均选择 revision ≥3 路径，最低要求为 v3.1。CI 文件 `sdkconfig.ci.board.esp32_p4_function_ev` 虽沿用历史板名，实际也面向 v3.1，并非旧版 P4 配置。

### P4 revision v1.3

生成旧版 P4 板级输入：

```bash
idf.py gen-bmgr-config -b esp32_p4_function_ev
```

使用 GSP 时，在输出目录内创建 defaults 文件，并放在加载顺序最后，以覆盖 `sdkconfig.defaults.esp32p4` 的 v3.1 设置：

```bash
mkdir -p build-gsp-p4-v1
cat > build-gsp-p4-v1/board.defaults <<'EOF'
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_360=y
EOF
idf.py -B build-gsp-p4-v1 -D IDF_TARGET=esp32p4 \
    -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;build-gsp-p4-v1/board.defaults' build
```

使用 LVGL 时，使用另一个输出目录，并在其 defaults 文件中选择 LVGL：

```bash
mkdir -p build-lvgl-p4-v1
cat > build-lvgl-p4-v1/board.defaults <<'EOF'
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_360=y
CONFIG_BROOKESIA_GUI_BACKEND_GSP=n
CONFIG_BROOKESIA_GUI_BACKEND_LVGL=y
EOF
idf.py -B build-lvgl-p4-v1 -D IDF_TARGET=esp32p4 \
    -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;build-lvgl-p4-v1/board.defaults' build
```

烧录前核对所选输出的 `sdkconfig`：`CONFIG_ESP_BOARD_NAME="esp32_p4_function_ev"`、`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`、`CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=360`。不要将要求 revision ≥3 的固件强制烧录到 v1.3。

### 烧录与监视

沿用构建时的 `-B`。例如，完成 P4X GSP 构建后：

```bash
idf.py -B build-gsp -p <PORT> -b 921600 flash monitor
```

将 `<PORT>` 替换为开发板串口；其他配置分别使用 `build-gsp-p4-v1`、`build-lvgl` 或 `build-lvgl-p4-v1`。按 `Ctrl+]` 退出监视器。完整烧录会替换 LittleFS，包含其中已安装的应用及其数据，请先备份需要保留的内容。

### 输出目录

各输出独立保存 `sdkconfig`、`dependencies.lock`、`dependencies/managed_components`、生成头文件/bundle 和 `littlefs_stage`。未显式指定 defaults 或外部 `SDKCONFIG` 时，新输出首次复制项目已有 sdkconfig；显式 defaults 则创建新配置，并首先加载已生成的板级 defaults。原有项目配置与下载保留；各输出的独立依赖缓存会增加磁盘占用。

`components/gen_bmgr_codes` 中的板级生成结果是共享源码输入。切换板型或 ESP-IDF 版本时，需重新生成匹配的输入；重新生成前请备份自定义输入。同时开发不同板型时，请使用独立源码检出。显式指定的外部 `SDKCONFIG` 或资源 staging 路径也应各自独立。[CI 输出审计](../../../.gitlab/tools/README.md#build_appspy)检查标准的构建目录内布局。

## 切换界面

使用上面对应的输出目录；同一输出切换 backend 会被拒绝。后续 menuconfig、构建和烧录均保留 `-B build-gsp` 或 `-B build-lvgl`。修改某个 backend 内的设置：

```bash
idf.py -B build-gsp menuconfig
idf.py -B build-gsp build
```

两种配置均使用 `partitions_16m.csv`。LVGL 启动辅助代码共享于 `examples/system/common/lvgl`，两个 backend 均不读取 Super example 的源码或产物。

### 文件系统资源

`littlefs_data` 镜像由 `<所选构建目录>/littlefs_stage` 生成。

选择 GSP 时，镜像包含声音测试 WAV、Settings 应用名称所需的翻译文件，以及配置为预装的 GSP 应用。GSP 场景及其字体由 GSP bundle 提供；镜像不包含 LVGL 桌面（`system/super`）、LVGL 公共字体（`system/fonts`）和 Settings 的 LVGL 页面。选择 LVGL 时，镜像会包含桌面资源和完整的 Settings 包。

### 切换 backend 后烧录

`idf.py app-flash` 只更新应用分区，不会更新 `littlefs_data`。切换 backend 后，请构建对应输出，再使用上方的[完整烧录命令](#烧录与监视)，写入匹配的文件系统资源。

两种界面使用同一套能力配置和 AudioPlayback 服务。声音测试通过板卡共享的音频处理器播放本地 PCM/WAV 音频；高层录音接口默认关闭，原始编解码器接口仍可使用。本示例不提供语音、网络流媒体或视频功能。

GSP 编译场景在 Flash 中采用无损压缩。打开应用时，场景包解压到 PSRAM，关闭后释放。如需使用未压缩场景，可在 **ESP-Brookesia: GUI GSP Configurations** 中关闭 **Compress embedded Brookesia GSP bundles**。

如果已有配置关闭了 **Audio Device → Audio Processor**，请在 menuconfig 的 **ESP-Brookesia: Hal Adaptor Configurations** 中启用它，关闭 **Audio Processor Recording**，并保留示例的其他能力默认值。切换 GUI 后端无需更改这些设置。

## 实际操作

启动完成后，从启动页打开 **Settings**。通过**显示**调节亮度，或进入**声音**、**语言**和**主题**页面。**Back** 返回设置首页，**Exit** 返回启动页。使用 LVGL 时，从桌面打开设置。

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

亮度滑块的范围为 0% 至 100%。显示服务会保存所选值并在启动时恢复；没有保存值时默认为 90%。硬件背光范围可能与界面显示的百分比不同。

## 声音

连接开发板扬声器后，打开**设置 → 声音**，调节音量或切换静音。**试听提示音**会按当前设置播放一段短音频，静音时保持无声。若有其他音频正在播放或暂停，请先结束播放，再试听提示音。音量与静音由播放服务保存，并在重启后恢复；设备不可用或更新失败时，页面会显示提示。

构建后请使用完整的 `idf.py flash`，使文件系统包含测试音频。音频由构建过程在本地生成。

## 语言与主题

语言提供**英语**和**简体中文**，主题提供**浅色**和**深色**。选择后点击**立即重启**应用已保存的选项，或点击**稍后**继续使用当前外观。再次选择当前选项，可替换该设置的待生效选择。

重启后，启动页和全部设置页面都会使用选定的语言与主题，包括 Wi-Fi 状态和密码键盘。选择外观不会改变已保存的亮度、声音或 Wi-Fi 设置。密码键盘仍仅支持可打印的 ASCII 字符，文案切换不会启用中文输入法。

## 校时

系统运行期间会持有 SNTP 服务，并在 Wi-Fi 连接后通过网络校准时间。没有网络连接时，时间可能尚未设置，时钟等应用可能显示 1970 年。

## 界面功能

System Core 提供 GSP 系统主页及公共键盘、消息框；本示例配置设备并启动系统。Settings 提供我的设备、带实时 SNTP 状态的时区页和按目标选择的已保存网络删除。高级调试仍由 LVGL 提供，两套界面共用系统偏好和硬件服务。

## 诊断

menuconfig 的 **Brookesia system GSP example** 提供可选的堆内存、界面和连接检查，默认关闭。开启 **Let USB callers inject touches and print the screen** 后，可通过 `GspProbe` 服务调用 `Tap`、`Drag`、`Component` 和 `Frame`；`Frame` 会将帧缓冲输出到串口。

连接检查需要一个专用的个人测试网络。串口提示就绪后，发送一行包含 `ssid` 和 `password` 字段的 JSON。固件不会回显凭据，发送端也请关闭本地回显。成功连接后，凭据由正常的 Wi-Fi 服务保存。
