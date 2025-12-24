# ESP-Brookesia Audio Service

* [English Version](./README.md)

## 概述

`brookesia_service_audio` 是为 ESP-Brookesia 生态系统提供的音频服务，提供：

- **音频播放**：支持从 URL 播放音频文件，支持暂停、恢复、停止等播放控制。
- **音频编解码**：支持多种音频编解码格式（PCM、OPUS、G711A），支持编码和解码功能。
- **音量控制**：支持音量设置和查询，提供完整的音量管理能力。
- **播放状态管理**：实时跟踪播放状态（空闲、播放中、暂停），并通过事件通知状态变化。
- **编码器管理**：支持编码器的启动、停止和配置，可设置编码器读取数据大小。
- **解码器管理**：支持解码器的启动、停止和数据输入，支持流式解码。
- **持久化存储**：可选搭配 `brookesia_service_nvs` 服务持久化保存音量等信息

## 目录

- [ESP-Brookesia Audio Service](#esp-brookesia-audio-service)
  - [概述](#概述)
  - [目录](#目录)
  - [功能特性](#功能特性)
    - [音频编解码格式](#音频编解码格式)
    - [播放控制](#播放控制)
    - [编码器配置](#编码器配置)
    - [解码器配置](#解码器配置)
    - [事件通知](#事件通知)
  - [开发环境要求](#开发环境要求)
  - [添加到工程](#添加到工程)

## 功能特性

### 音频编解码格式

Audio Service 支持以下音频编解码格式：

| 格式 | 编码 | 解码 | 说明 |
|------|------|------|------|
| **PCM** | ✅ | ✅ | 无损音频格式 |
| **OPUS** | ✅ | ✅ | 支持 VBR 和固定比特率配置 |
| **G711A** | ✅ | ✅ | 电话音质音频格式 |

### 播放控制

支持以下播放控制操作：

- **播放**：从指定 URL 播放音频文件
- **暂停**：暂停当前播放
- **恢复**：从暂停状态恢复播放
- **停止**：停止当前播放

### 编码器配置

编码器支持灵活的配置选项：

- **编解码格式**：PCM、OPUS、G711A
- **通道数**：1-4 通道
- **采样位数**：8、16、24、32 位
- **采样率**：8000、16000、24000、32000、44100、48000 Hz
- **帧时长**：可配置的帧时长（毫秒）
- **OPUS 扩展配置**：VBR 开关、比特率设置

### 解码器配置

解码器支持以下配置：

- **编解码格式**：PCM、OPUS、G711A
- **通道数**：1-4 通道
- **采样位数**：8、16、24、32 位
- **采样率**：8000、16000、24000、32000、44100、48000 Hz
- **帧时长**：可配置的帧时长（毫秒）

### 事件通知

Audio Service 提供以下事件通知：

- **播放状态变化**：播放状态改变时触发（Idle、Playing、Paused）
- **编码器事件**：编码器事件发生时触发
- **编码数据就绪**：编码数据准备好时触发

## 开发环境要求

使用本库前，请确保已安装以下 SDK 开发环境：

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> SDK 的安装方法请参阅 [ESP-IDF 编程指南 - 安装](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

## 添加到工程

`brookesia_service_audio` 已上传到 [Espressif 组件库](https://components.espressif.com/)，您可以通过以下方式将其添加到工程中：

1. **使用命令行**

   在工程目录下运行以下命令：

   ```bash
   idf.py add-dependency "espressif/brookesia_service_audio"
   ```

2. **修改配置文件**

   在工程目录下创建或修改 *idf_component.yml* 文件：

   ```yaml
   dependencies:
     espressif/brookesia_service_audio: "*"
   ```

详细说明请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。
