# ESP-Brookesia Expression Emote

* [English Version](./README.md)

## 概述

`brookesia_expression_emote` 是 ESP-Brookesia 表情管理组件，基于 ESP-Brookesia 服务框架实现，提供：

- **表情符号集管理**：支持设置和管理表情符号（emoji），用于表达不同的情感状态。
- **动画集管理**：支持设置、插入和停止动画，提供丰富的动画效果展示。
- **事件消息集管理**：支持设置事件消息，实现表情与事件的关联。
- **资源管理**：支持加载和管理表情资源源，灵活配置表情资源。
- **动画控制**：支持动画播放控制，包括等待动画帧完成、停止动画等操作。

## 目录

- [ESP-Brookesia Expression Emote](#esp-brookesia-expression-emote)
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

`brookesia_expression_emote` 已上传到 [Espressif 组件库](https://components.espressif.com/)，您可以通过以下方式将其添加到工程中：

1. **使用命令行**

   在工程目录下运行以下命令：

   ```bash
   idf.py add-dependency "espressif/brookesia_expression_emote"
   ```

2. **修改配置文件**

   在工程目录下创建或修改 *idf_component.yml* 文件：

   ```yaml
   dependencies:
     espressif/brookesia_expression_emote: "*"
   ```

详细说明请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。
