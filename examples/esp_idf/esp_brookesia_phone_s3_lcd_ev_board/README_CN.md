# ESP32-S3-LCD-EV-Board 运行 ESP-Brookesia Phone 示例

[English Version](./README.md)

这个示例演示了如何在 [ESP32-S3-LCD-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s3/esp32-s3-lcd-ev-board/index.html) 开发板上运行 ESP-Brookesia Phone，并使用 `480 x 480` 和 `800 x 480` 分辨率的 UI 样式表。

## 入门指南

### 硬件要求

* 一个配有 `480 x 480` 或 `800 x 480` 分辨率 LCD 屏幕的 ESP32-S3-LCD-EV-Board 开发板。

### ESP-IDF 要求

- 此示例支持 IDF release/v5.1 及更高的分支。默认情况下，它在 IDF release/v5.1 上运行。
- 请按照 [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html) 设置开发环境。**我们强烈建议**您 [构建第一个项目](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#build-your-first-project)，以熟悉 ESP-IDF 并确保环境设置正确。

### 获取 esp-brookesia 仓库

要从 esp-brookesia 示例开始，请在终端运行以下命令，将仓库克隆到本地电脑：

```
git clone --recursive https://github.com/espressif/esp-brookesia.git
```

### 配置

  运行 ``idf.py menuconfig`` 并修改 esp-brookesia 配置。

## 如何使用示例

### 构建和烧录示例

构建项目并将其烧录到开发板，然后运行监视工具查看串行输出（将 `PORT` 替换为您的开发板串口名称）：

```c
idf.py -p PORT flash monitor
```

要退出串行监视器，请输入 ``Ctrl-]``。

完整的配置和使用 ESP-IDF 构建项目的步骤，请参见 [ESP-IDF 入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/get-started/index.html)。

### 示例输出

- 以下动画展示了示例在不同开发板上运行的效果。

<p align="center">
<img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_s3_lcd_ev_board_480_480.gif" alt ="esp_brookesia_phone_s3_lcd_ev_board_480_480" width="400">
</p>

<p align="center">
ESP32-S3-LCD-EV-Board（带有 480 x 480 分辨率的 LCD 屏幕）（<a href="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_s3_lcd_ev_board_480_480.mp4">Click to view the video</a>）
</p>
<br>

<p align="center">
<img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_s3_lcd_ev_board_800_480.gif" alt ="esp_brookesia_phone_s3_lcd_ev_board_800_480" width="400">
</p>

<p align="center">
ESP32-S3-LCD-EV-Board-2（带有 800 x 480 分辨率的 LCD 屏幕）（<a href="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_s3_lcd_ev_board_800_480.mp4">Click to view the video</a>）
</p>

## 技术支持和反馈

请使用以下反馈渠道：

- 有技术问题，请访问 [esp32.com](https://esp32.com/viewforum.php?f=35) 论坛。
- 如需提交功能请求或错误报告，请创建 [GitHub issue](https://github.com/espressif/esp-brookesia/issues)。

我们会尽快回复您。
