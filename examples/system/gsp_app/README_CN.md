# System GSP App

[English](./README.md)

这个示例跑在 ESP32-P4 Function EV Board 的 1024×600 屏幕上。开机后是一个启动页，可以从这里打开设置，并调节屏幕亮度。

界面在 menuconfig 里选择：

- **GSP**，默认。启动页、设置首页和亮度页。
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

Wi-Fi、声音、语言和主题还在页面上，旁边写着 **Unavailable**。列表下面有一行字，说明这一版还没有这些功能。

选择 LVGL 时，板子先显示启动画面，然后进入桌面。从桌面打开设置。亮度走同一个显示服务。

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

画面以屏幕为准。串口日志记录的是服务和内存数字。
