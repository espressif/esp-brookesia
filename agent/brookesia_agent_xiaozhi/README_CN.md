# ESP-Brookesia Xiaozhi Agent

* [English Version](./README.md)

## 概述

`brookesia_agent_xiaozhi` 是为 ESP-Brookesia 生态系统提供的小智 AI 智能体实现，提供：

- **小智平台集成**：基于 `esp_xiaozhi` SDK 实现与小智 AI 平台的通信
- **实时语音交互**：支持 OPUS 音频编解码，16kHz 采样率，24kbps 比特率
- **丰富的事件支持**：支持说话/监听状态变化、智能体/用户文本、表情等事件
- **手动监听控制**：支持手动开始/停止监听，适用于 Manual 对话模式
- **中断说话**：支持中断智能体说话功能
- **统一生命周期管理**：基于 `brookesia_agent_manager` 框架的统一智能体生命周期管理

## 功能特性

### 支持的通用功能

| 功能 | 说明 |
| ---- | ---- |
| `InterruptSpeaking` | 中断智能体说话 |

### 支持的通用事件

| 事件 | 说明 |
| ---- | ---- |
| `SpeakingStatusChanged` | 说话状态变化 |
| `ListeningStatusChanged` | 监听状态变化 |
| `AgentSpeakingTextGot` | 智能体说话文本 |
| `UserSpeakingTextGot` | 用户说话文本 |
| `EmoteGot` | 表情事件 |

## 如何使用

### 开发环境要求

使用本库前，请确保已安装以下 SDK 开发环境：

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

### 添加到工程

`brookesia_agent_xiaozhi` 已上传到 [Espressif 组件库](https://components.espressif.com/)，您可以通过以下方式将其添加到工程中：

1. **使用命令行**

   在工程目录下运行以下命令：

   ```bash
   idf.py add-dependency "espressif/brookesia_agent_xiaozhi"
   ```

2. **修改配置文件**

   在工程目录下创建或修改 *idf_component.yml* 文件：

   ```yaml
   dependencies:
     espressif/brookesia_agent_xiaozhi: "*"
   ```

### Kconfig 配置

| 配置项 | 说明 | 默认值 |
| ------ | ---- | ------ |
| `BROOKESIA_AGENT_XIAOZHI_ENABLE_AUTO_REGISTER` | 启用自动插件注册 | `y` |
| `BROOKESIA_AGENT_XIAOZHI_ENABLE_DEBUG_LOG` | 启用调试日志 | `n` |
| `BROOKESIA_AGENT_XIAOZHI_GET_HTTP_INFO_INTERVAL_MS` | 激活状态检查间隔（毫秒） | `30000` |
| `BROOKESIA_AGENT_XIAOZHI_WAKE_WORD` | 唤醒词 | `"Hi,ESP"` |
