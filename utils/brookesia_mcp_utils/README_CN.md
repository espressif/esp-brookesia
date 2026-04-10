# ESP-Brookesia MCP 工具库

* [English Version](./README.md)

## 概述

`brookesia_mcp_utils` 在 Brookesia 服务与 [ESP MCP（Model Context Protocol）引擎](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/ai/mcp.html) 之间提供桥接，支持将已注册服务函数或自定义回调注册为 MCP 工具，并生成可供 MCP 引擎使用的工具句柄。

## 目录

- [ESP-Brookesia MCP 工具库](#esp-brookesia-mcp-工具库)
  - [概述](#概述)
  - [目录](#目录)
  - [特性](#特性)
  - [如何使用](#如何使用)
    - [开发环境要求](#开发环境要求)
    - [添加到工程](#添加到工程)

## 特性

- **服务工具 (Service Tools)**：将已注册的 Brookesia 服务中的函数暴露为 MCP 工具，调用时转发到对应服务，工具名格式为 `Service.<服务名>.<函数名>`
- **自定义工具 (Custom Tools)**：使用自定义 C++ 回调实现 MCP 工具，支持参数 schema 与可选上下文，工具名格式为 `Custom.<函数名>`
- **工具注册表 (ToolRegistry)**：统一管理上述两类工具的注册、移除，并生成 `esp_mcp_tool_t*` 数组供 MCP 引擎使用

## 如何使用

### 开发环境要求

使用本库前，请确保已安装以下 SDK 开发环境：

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> SDK 的安装方法请参阅 [ESP-IDF 编程指南 - 安装](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

### 添加到工程

`brookesia_mcp_utils` 已上传到 [Espressif 组件库](https://components.espressif.com/)，您可以通过以下方式将其添加到工程中：

1. **使用命令行**

    在工程目录下运行以下命令：

   ```bash
   idf.py add-dependency "espressif/brookesia_mcp_utils"
   ```

2. **修改配置文件**

   在工程目录下创建或修改 *idf_component.yml* 文件：

   ```yaml
   dependencies:
     espressif/brookesia_mcp_utils: "*"
   ```

详细说明请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。
