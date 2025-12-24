# ESP-Brookesia Coze Agent

* [English Version](./README.md)

## 概述

`brookesia_agent_coze` 是基于 ESP-Brookesia Agent Manager 框架实现的 Coze 智能体，提供：

- **Coze API 集成**：通过 WebSocket 与 Coze 平台进行实时通信，支持语音对话和文本交互。
- **OAuth2 认证**：支持 Coze 平台的 OAuth2 认证机制，自动获取和管理访问令牌。
- **多机器人支持**：支持配置多个机器人，可以动态切换当前激活的机器人。
- **音频编解码**：内置音频编解码支持，默认使用 G711A 格式，支持 16kHz 采样率。
- **表情支持**：支持接收和显示 Coze 平台的表情事件。
- **事件处理**：支持 Coze 平台事件（如余额不足等）的监听和处理。
- **持久化存储**：可选搭配 `brookesia_service_nvs` 服务持久化保存鉴权信息和机器人等信息

## 目录

- [ESP-Brookesia Coze Agent](#esp-brookesia-coze-agent)
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

`brookesia_agent_coze` 已上传到 [Espressif 组件库](https://components.espressif.com/)，您可以通过以下方式将其添加到工程中：

1. **使用命令行**

    在工程目录下运行以下命令：

   ```bash
   idf.py add-dependency "espressif/brookesia_agent_coze"
   ```

2. **修改配置文件**

   在工程目录下创建或修改 *idf_component.yml* 文件：

   ```yaml
   dependencies:
     espressif/brookesia_agent_coze: "*"
   ```

详细说明请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。
