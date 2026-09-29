# System GSP App

[English](./README.md)

本示例面向配有 1024×600 屏幕的 ESP32-P4 Function EV Board。开机后进入启动页，可打开设置、调节屏幕亮度并查看附近的 Wi-Fi 热点。

界面在 menuconfig 里选择：

- **GSP**，默认。启动页、设置首页、亮度页和 Wi-Fi 热点发现。
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

声音、语言和主题仍标为 **Unavailable**。

选择 LVGL 时，板子先显示启动画面，然后进入桌面。从桌面打开设置。亮度走同一个显示服务。

## Wi-Fi

打开 **Settings → Wi-Fi**，开启 Wi-Fi 后扫描附近热点。使用 **Refresh** 重新扫描，通过 **Previous / Next** 浏览结果。页面显示各热点的安全状态、信号等级和 RSSI，以及当前连接状态。

**Back** 返回设置首页。扫描时离开页面会取消本次扫描；重新进入时读取当前服务状态，Wi-Fi 已开启则重新扫描。关闭 Wi-Fi 也会断开当前网络，开关偏好会在重启后保留。

密码输入和连接操作尚未开放。Wi-Fi 服务仍可自动重连已保存的网络，页面会显示这些连接。

P4 开发板通过 ESP-Hosted 使用配套 Wi-Fi 芯片。若提示服务不可用，请检查开发板配置和配套芯片固件。扫描失败可通过 **Refresh** 重试；结果为空表示未发现有名称的热点。

## 亮度

滑块是 0 到 100 的百分比，这个百分比会被保存。

退出设置不会关掉显示服务。再次打开时读取当前百分比，不会写成一个新的固定值。

重启后会恢复上次保存的百分比，然后把背光打开。还没有保存过时，第一次启动是 90。

灯的硬件范围比 0 到 100 窄，所以硬件日志里的数字可能和滑块不同。滑块上的数字是保存的百分比。

## 启动日志

复位后会先打印内存日志，然后才出现启动页。示例会打开并关闭设置 22 次：2 次预热，20 次计量。这不会改已保存的亮度。

如果还想在启动时从服务侧改一次亮度，编辑 `main/main_gsp.cpp`：

```cpp
constexpr bool RUN_BRIGHTNESS_PROBE = true;
```

重新构建并烧录。这次会先改亮度，再把原来的百分比写回去，然后才开始内存日志。

如需执行 Wi-Fi 验收，在 menuconfig 的 **Brookesia system GSP example** 下开启 **Run Settings Wi-Fi hardware acceptance probe at startup**。探针使用真实扫描和模拟触摸，覆盖刷新、取消扫描、重新进入及 Wi-Fi 开关，并在预热后记录 20 轮内存数据。正常使用时请关闭此选项。

探针还会注入空结果、扫描失败和多语言名称样例，检查对应页面。这些带有标记的用例是合成数据；随后的刷新和循环扫描使用附近的真实热点。

请在屏幕上确认显示效果。串口日志记录服务状态和内存数据。
