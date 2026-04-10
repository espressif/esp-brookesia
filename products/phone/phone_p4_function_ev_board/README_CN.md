# ESP32-P4-Function-EV-Board 运行 ESP-Brookesia Phone 示例

[English Version](./README.md)

这个示例演示了如何在 [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32p4/esp32-p4-function-ev-board/index.html) 开发板上运行 ESP-Brookesia Phone，并使用 `1024 x 600` 分辨率的 UI 样式表。

## 入门指南

### 硬件要求

* 一个配有 `1024 x 600` 分辨率 LCD 屏幕的 ESP32-P4-Function-EV-Board 开发板。

### ESP-IDF 要求

- 此示例支持 IDF release/v5.3 及更高的分支。默认情况下，它在 IDF release/v5.3 上运行。
- 请按照 [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html) 设置开发环境。**我们强烈建议**您 [构建第一个项目](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#build-your-first-project)，以熟悉 ESP-IDF 并确保环境设置正确。

### 获取 esp-brookesia 仓库

要从 esp-brookesia 示例开始，请在终端运行以下命令，将仓库克隆到本地电脑：

```
git clone --recursive https://github.com/espressif/esp-brookesia.git
```

### 配置

  运行 ``idf.py menuconfig`` 并修改 esp-brookesia 配置。

### 测试 RainMaker 应用

若要在设备上编译并运行 RainMaker 应用，建议按顺序完成：

1. **配置自家路由器 / 热点**：运行 ``idf.py menuconfig``，在已开启 RainMaker 支持后出现 **Phone Auto Wi-Fi** 菜单，将 **Wi-Fi SSID** 与 **Wi-Fi password** 填成与您**实际使用的无线路由器（或 AP）**一致的名称与密码（与路由器后台配置的 SSID、WPA 密码相同），以便设备**上电开机后自动作为 STA 连接**该网络。一般请使用设备可接入的频段（常见为 2.4 GHz）。可在 **Phone clock** 中按需修改时区，在 **Phone Auto Wi-Fi** 中可配置 SNTP 服务器等。

烧录后，开发板会在启动时尝试连接上述 Wi‑Fi，便于 RainMaker 等需要联网/DNS 的功能直接使用，而无需再单独配网。

然后执行 ``idf.py build`` 编译并烧录即可。

具体 RainMaker APP 的说明，请参考 [README](../../../apps/brookesia_app_rainmaker/README_CN.md)。

## 如何使用示例

### 构建和烧录示例

构建项目并将其烧录到开发板，然后运行监视工具查看串行输出（将 `PORT` 替换为您的开发板串口名称）：

```c
idf.py -p PORT flash monitor
```

要退出串行监视器，请输入 ``Ctrl-]``。

完整的配置和使用 ESP-IDF 构建项目的步骤，请参见 [ESP-IDF 入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/get-started/index.html)。

## 技术支持和反馈

请使用以下反馈渠道：

- 有技术问题，请访问 [esp32.com](https://esp32.com/viewforum.php?f=35) 论坛。
- 如需提交功能请求或错误报告，请创建 [GitHub issue](https://github.com/espressif/esp-brookesia/issues)。

我们会尽快回复您。
