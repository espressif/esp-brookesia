# AI 聊天机器人示例

[English Version](./README.md)

本示例演示了如何基于 ESP-Brookesia 框架构建一个完整的 AI 语音聊天机器人，支持唤醒词检测、多 AI Agent 接入与运行时切换、表情动画显示、手势导航以及通过 MCP 工具让 AI 直接控制设备硬件。

## 📑 目录

- [AI 聊天机器人示例](#ai-聊天机器人示例)
  - [📑 目录](#-目录)
  - [✨ 功能特性](#-功能特性)
  - [🚩 快速入门](#-快速入门)
    - [硬件要求](#硬件要求)
    - [开发环境](#开发环境)
  - [🔨 如何使用](#-如何使用)
  - [🔧 配置 AI Agent](#-配置-ai-agent)
  - [🚀 快速体验](#-快速体验)
    - [首次上电配网](#首次上电配网)
    - [语音交互](#语音交互)
    - [设置界面](#设置界面)
  - [🔍 故障排除](#-故障排除)
  - [💬 技术支持与反馈](#-技术支持与反馈)

## ✨ 功能特性

- 🤖 **多 AI Agent**：内置 XiaoZhi（默认）、Coze、OpenAI 三种 Agent，支持运行时切换
- 🎙️ **语音唤醒与交互**：集成 AFE（音频前端）引擎，支持 VAD 及唤醒词检测，实现免按键对话
- 😊 **表情动画**：AI 说话、思考、等待等状态实时呈现对应表情动画
- 🖐️ **手势导航**：通过触摸滑动手势在表情主界面与设置界面之间切换
- 🛠️ **MCP 硬件控制工具**：XiaoZhi Agent 可通过 MCP（Model Context Protocol）工具直接控制屏幕亮度（获取/设置/开关）及音量（获取/设置/静音）
- 📶 **WiFi 配网**：首次上电自动进入 SoftAP 配网模式；配网完成后记住凭证，后续自动重连
- 💾 **持久化设置**：亮度等用户配置通过 NVS 掉电保存
- 📊 **运行时分析**：内置内存与线程性能分析器，便于开发调试

## 🚩 快速入门

### 硬件要求

本示例通过 [brookesia_hal_boards](https://components.espressif.com/components/espressif/brookesia_hal_boards) 组件管理硬件，支持以下开发板：

- ESP-VoCat V1.0
- ESP-VoCat V1.2
- ESP32-S3-BOX-3
- ESP32-S3-Korvo-2 V3
- ESP32-P4-Function-EV-Board

### 开发环境

请参考以下文档：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 开发环境搭建](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-dev-environment)

## 🔨 如何使用

<a href="https://espressif.github.io/esp-brookesia/index.html">
  <img alt="Try it with ESP Launchpad" src="https://dl.espressif.com/AE/esp-dev-kits/new_launchpad.png" width="400">
</a>

请参考 [ESP-Brookesia 编程指南 - 如何使用示例工程](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-example-projects)。

## 🔧 配置 AI Agent

运行 `idf.py menuconfig`，在 `Example Configuration` 菜单中配置 Agent：

| 选项 | 说明 |
|------|------|
| **Enable Xiaozhi agent** | 启用小智 Agent（默认开启） |
| **Enable Coze agent** | 启用 Coze Agent，需填写 App ID、公钥、私钥及 Bot 配置 |
| **Enable OpenAI agent** | 启用 OpenAI Agent，需填写 API Key 及模型名称 |

> [!NOTE]
> 多个 Agent 可同时启用，运行时可在设置界面切换当前活跃 Agent。

## 🚀 快速体验

### 首次上电配网

设备上电后会检测是否已保存 WiFi 凭证：

- **未配置**：设备自动开启 SoftAP 热点，使用手机连接该热点后，通过配网页面输入家庭 WiFi 的 SSID 和密码完成配网。
- **已配置**：设备自动连接保存的 WiFi，连接成功后 AI Agent （默认为 XiaoZhi Agent）随即启动。

### 语音交互

AI Agent 启动后即可开始对话。AI 应答时屏幕会同步显示对应表情动画，对话结束后自动回到待机状态。

XiaoZhi Agent 在对话中可主动调用 MCP 工具执行设备操作，例如：

- "帮我把音量调大一点"
- "当前音量是多少？"
- "静音"
- "取消静音"
- "把屏幕亮度调到 50%"
- "关掉屏幕"
- "打开屏幕"

一段时间后未检测到人声，AI Agent 会自动进入睡眠状态，此时说出唤醒词（默认为 "Hi,ESP"）即可开始对话。

### 设置界面

在 **表情主界面** 从任意位置滑动手指可进入 **设置界面**，支持以下操作：

| 设置项 | 说明 |
|--------|------|
| **WiFi 状态** | 显示当前连接的 WiFi SSID 及连接状态 |
| **亮度** | 拖动滑块调节屏幕亮度（0–100%），断电后自动保存 |
| **音量** | 拖动滑块调节播放音量（0–100%） |
| **Agent 切换** | 下拉选择活跃 Agent（XiaoZhi / Coze / OpenAI） |
| **恢复出厂设置** | 清除所有已保存配置，重启后重新配网 |

在 **设置界面** 从任意边缘位置滑动手指可返回 **表情主界面**。

## 🔍 故障排除

**设备不进入配网模式**

确认 Flash 中没有残留旧配网信息，可在设置界面点击"恢复出厂设置"后重启。

**AI 无应答**

- 检查 WiFi 是否已连接（设置界面查看状态）。
- 检查 menuconfig 中对应 Agent 是否已启用，以及 API Key / Bot ID 等凭证是否正确填写。
- 查看串口日志，确认 Agent 服务是否启动成功。

**唤醒词无响应**

- 确认麦克风正常工作，音量不宜过低。
- 检查 `model` 分区是否已成功烧录唤醒词模型。

**编译失败（VSCode）**

使用命令行安装 ESP-IDF，参考 [开发环境](#开发环境)。

## 💬 技术支持与反馈

请通过以下渠道进行反馈：

- 有关技术问题，请访问 [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) 论坛
- 有关功能请求或错误报告，请创建新的 [GitHub 问题](https://github.com/espressif/esp-brookesia/issues)

我们会尽快回复。
