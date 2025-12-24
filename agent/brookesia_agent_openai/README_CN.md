# ESP-Brookesia OpenAI Agent

* [English Version](./README.md)

## 概述

`brookesia_agent_openai` 是基于 ESP-Brookesia Agent Manager 框架实现的 OpenAI 智能体，提供：

- **OpenAI Realtime API 集成**：通过 WebRTC 数据通道与 OpenAI 平台进行实时通信，支持语音对话和文本交互。
- **点对点通信**：基于 `esp_peer` 实现点对点（P2P）通信，支持信号通道和数据通道。
- **音频编解码**：内置音频编解码支持，默认使用 OPUS 格式，支持 16kHz 采样率和 24kbps 比特率。
- **数据通道消息处理**：支持多种数据通道消息类型，包括音频流、文本流、函数调用、会话管理等。
- **实时交互**：支持实时音频输入输出，提供低延迟的语音交互体验。
- **持久化存储**：可选搭配 `brookesia_service_nvs` 服务持久化保存配置信息

## 目录

- [ESP-Brookesia OpenAI Agent](#esp-brookesia-openai-agent)
  - [概述](#概述)
  - [目录](#目录)
  - [如何使用](#如何使用)
    - [开发环境要求](#开发环境要求)
    - [添加到工程](#添加到工程)

## 如何使用

### 开发环境要求

使用本库前，请确保已安装以下 SDK 开发环境：

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> SDK 的安装方法请参阅 [ESP-IDF 编程指南 - 安装](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

### 添加到工程

`brookesia_agent_openai` 已上传到 [Espressif 组件库](https://components.espressif.com/)，您可以通过以下方式将其添加到工程中：

1. **使用命令行**

    在工程目录下运行以下命令：

   ```bash
   idf.py add-dependency "espressif/brookesia_agent_openai"
   ```

2. **修改配置文件**

   在工程目录下创建或修改 *idf_component.yml* 文件：

   ```yaml
   dependencies:
     espressif/brookesia_agent_openai: "*"
   ```

详细说明请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。
