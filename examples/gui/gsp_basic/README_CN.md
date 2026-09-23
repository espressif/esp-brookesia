# GSP 基础示例

[English](./README.md)

在 ESP32-P4 Function EV Board 上通过 Brookesia HAL 运行一个 ESP-GSP 场景。场景源文件是 `main/scenes/home.json`。此固件不加载 Brookesia JSON UI，也不安装 `.bpk`。

当前配置链接的是 ESP32-P4 revision v3.1 对应的 GSP 库。

## 构建与烧录

```bash
idf.py gen-bmgr-config -b esp32_p4_function_ev
idf.py set-target esp32p4
idf.py build
idf.py -p <PORT> -b 921600 flash monitor
```

端口使用已确认的 Function EV Board 串口。复位后屏幕显示 `Brookesia · GSP`、构建标识、持续变化的活动条，以及 `+1` 和 `Reset` 按钮。
