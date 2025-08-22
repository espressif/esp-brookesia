# EchoEar（喵伴）出厂示例

[English Version](./README.md)

这个示例演示了如何在 EchoEar 开发板上运行 ESP-Brookesia Speaker，并支持通过语音唤醒与 Coze 智能体进行语音交互。

请参阅 [EchoEar（喵伴）用户指南](https://espressif.craft.me/BBkCPR3ZaoLCV8) 了解产品信息和固件使用方法。

## 入门指南

### 硬件要求

* 一个 EchoEar 开发板（已插入 SD 卡）。

### ESP-IDF 要求

- 此示例支持 IDF release/v5.5 及更高的分支。默认情况下，它在 IDF release/v5.5 上运行。
- 请按照 [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html) 设置开发环境。**我们强烈建议**您 [构建第一个项目](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#build-your-first-project)，以熟悉 ESP-IDF 并确保环境设置正确。

### 获取 esp-brookesia 仓库

要从 esp-brookesia 示例开始，请在终端运行以下命令，将仓库克隆到本地电脑：

```
git clone https://github.com/espressif/esp-brookesia.git
```

### 配置

运行 `idf.py menuconfig` > `Example Configuration` 配置 COZE 相关参数。

请参阅 [EchoEar（喵伴）用户指南 - 开始使用 - 开发者模式](https://espressif.craft.me/BBkCPR3ZaoLCV8) 了解相关配置参数。

## 如何使用示例

### 构建和烧录示例

构建项目并将其烧录到开发板，然后运行监视工具查看串行输出（将 `PORT` 替换为您的开发板串口名称）：

```c
idf.py -p PORT flash monitor
```

要退出串行监视器，请输入 ``Ctrl-]``。

完整的配置和使用 ESP-IDF 构建项目的步骤，请参见 [ESP-IDF 入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/get-started/index.html)。

### 已知问题

- 设备上电或复位后反复重启：这是丝印为 `v1.0` 版本 PCBA 已知问题，需要删除 `Baseboard` 的 `C1`、`C10` 和 `CoreBoard` 的 `C4`，三个 `10uF` 的电容，这些电容会影响板上 VCC 电源域供电能力，会导致概率触发芯片设备重启

## 烧录已有固件

您也可以直接使用 ESP Launchpad 烧录已有固件，请选择以下任一方式进行烧录：

1. **最新开源固件（无内置 Key）**：此固件为当前示例代码自动编译生成的固件，具有最新功能，但是无法使用内置 Key，需要您在烧录后自行配置 Key（参考 [EchoEar（喵伴）用户指南 - 开始使用 - 开发者模式](https://espressif.craft.me/BBkCPR3ZaoLCV8)）。

<a href="https://espressif.github.io/esp-launchpad/?flashConfigURL=https://espressif.github.io/esp-brookesia/launchpad.toml">
    <img alt="Try it with ESP Launchpad" src="https://espressif.github.io/esp-launchpad/assets/try_with_launchpad.png" width="200" height="56">
</a>

2. **出厂固件（内置 Key）**：此固件为官方手动编译生成的固件，可以使用内置 Key，您可以直接烧录使用，但可能不具有最新功能。

<a href="https://espressif.github.io/esp-launchpad/?flashConfigURL=https://lzw655.github.io/launchpad_test/launchpad.toml">
    <img alt="Try it with ESP Launchpad" src="https://espressif.github.io/esp-launchpad/assets/try_with_launchpad.png" width="200" height="56">
</a>

> [!NOTE]
> 以固件名 `speaker_0_i2_1_dev_echoear_1_2` 为例，其中，`speaker_0_i2_1_dev` 为 `工程名 + 工程版本号`，`echoear_1_2` 为 `开发板名 + 开发板版本号`。

## 技术支持和反馈

请使用以下反馈渠道：

- 有技术问题，请访问 [esp32.com](https://esp32.com/viewforum.php?f=52) 论坛。
- 如需提交功能请求或错误报告，请创建 [GitHub issue](https://github.com/espressif/esp-brookesia/issues)。

我们会尽快回复您。
