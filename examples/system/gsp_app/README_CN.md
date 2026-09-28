# System GSP App

[English](./README.md)

Brookesia System 在 ESP32-P4 Function EV Board 上运行一个本地应用。GUI 后端在 menuconfig 中选择：

- **GSP**（默认）：1024×600 的 Showcase（Command、Gallery、Lab），编译进固件。
- **LVGL**：原有的 Super 桌面，资源写入 `littlefs_data` 分区。

当前配置链接的是 ESP32-P4 revision v3.1 对应的 GSP 库。

## 构建与烧录

```bash
idf.py gen-bmgr-config -b esp32_p4_function_ev
idf.py set-target esp32p4
idf.py build
idf.py -p <PORT> -b 921600 flash monitor
```

## 切换后端

```bash
idf.py menuconfig
```

打开 `ESP-Brookesia: System Core Configurations → GUI backend`，然后保存。两种选择共用 `partitions_16m.csv`。选中 LVGL 后，`sdkconfig.defaults` 里的 LVGL 选项才会生效。

```bash
idf.py build
idf.py -p <PORT> -b 921600 flash
```

编译 GSP 场景需要场景编译器。没安装时菜单仍能打开，GSP 编译会停下来并提示安装方法：在 IDF 的 Python 环境里执行 `python -m pip install -U esp-gsp-tools`，或把 `GSPC_EXECUTABLE` 指到 `gspc`。

## 预期效果

GSP：复位后进入 Command 场景。横向滑动可进入 Gallery 和 Lab。时钟指针会走动。`ALL SYSTEMS READY` 打开快捷设置。显示滑条会改变背光，并更新亮度文字。`Exit` 会熄灭背光约 1.5 秒，然后重新进入 Command。在 Lab 中，进入场景后的第一次 `TRIGGER` 打开组件页；离开后再进入也是如此。

LVGL：复位后先显示启动画面，然后进入 Super 桌面，其中有设置应用。
