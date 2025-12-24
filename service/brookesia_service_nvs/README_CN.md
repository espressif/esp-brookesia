# ESP-Brookesia NVS Service

* [English Version](./README.md)

## 概述

`brookesia_service_nvs` 是为 ESP-Brookesia 生态系统提供的 NVS（Non-Volatile Storage，非易失性存储）服务，提供：

- **命名空间管理**：基于命名空间的键值对存储，支持多个独立的存储空间
- **数据类型支持**：支持 `布尔值`、`整数`、`字符串` 三种基本数据类型
- **持久化存储**：数据存储在 NVS 分区中，断电后数据不丢失
- **线程安全**：可选基于 `TaskScheduler` 实现异步任务调度，保证线程安全
- **灵活查询**：支持列出命名空间中的所有键值对，或按需获取指定键的值

## 目录

- [ESP-Brookesia NVS Service](#esp-brookesia-nvs-service)
  - [概述](#概述)
  - [目录](#目录)
  - [功能特性](#功能特性)
    - [命名空间管理](#命名空间管理)
    - [支持的数据类型](#支持的数据类型)
    - [核心功能](#核心功能)
  - [开发环境要求](#开发环境要求)
  - [添加到工程](#添加到工程)

## 功能特性

### 命名空间管理

NVS 服务使用命名空间来组织数据，每个命名空间是独立的存储空间：

- **默认命名空间**：如果不指定命名空间，将使用默认命名空间 `"storage"`
- **多命名空间**：可以创建多个命名空间来组织不同类型的数据
- **命名空间隔离**：不同命名空间的数据相互独立，互不影响

### 支持的数据类型

NVS 服务支持以下三种数据类型：

- **布尔值（Bool）**：`true` 或 `false`
- **整数（Int）**：32 位有符号整数
- **字符串（String）**：UTF-8 编码的字符串

> [!NOTE]
> 除了上述三种基本数据类型外，[brookesia_service_helper](https://components.espressif.com/components/espressif/brookesia_service_helper) 提供的 `save_key_value()` 和 `get_key_value()` 模板助手函数还支持存储和获取任意类型的数据：
> - **基本类型**：`bool`、`int32_t` 以及所有 ≤32 位的整数类型（`int8_t`、`uint8_t`、`int16_t`、`uint16_t`、`char`、`short` 等）直接存储，性能最优
> - **扩展整数类型**：>32 位的整数类型（`int64_t`、`uint64_t`、`long long` 等）通过 JSON 序列化存储
> - **浮点数类型**：`float`、`double` 等浮点数类型通过 JSON 序列化存储
> - **字符串类型**：`std::string`、`const char*` 等字符串类型通过 JSON 序列化存储
> - **复杂类型**：`std::vector`、`std::map`、自定义结构体等复杂类型通过 JSON 序列化存储

### 核心功能

- **列出键信息**：列出命名空间中所有键的信息（包括键名、类型等）
- **设置键值对**：支持批量设置多个键值对
- **获取键值对**：支持获取指定键的值，或获取命名空间中的所有键值对
- **删除键值对**：支持删除指定键，或清空整个命名空间

## 开发环境要求

使用本库前，请确保已安装以下 SDK 开发环境：

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> SDK 的安装方法请参阅 [ESP-IDF 编程指南 - 安装](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

## 添加到工程

`brookesia_service_nvs` 已上传到 [Espressif 组件库](https://components.espressif.com/)，您可以通过以下方式将其添加到工程中：

1. **使用命令行**

   在工程目录下运行以下命令：

   ```bash
   idf.py add-dependency "espressif/brookesia_service_nvs"
   ```

2. **修改配置文件**

   在工程目录下创建或修改 *idf_component.yml* 文件：

   ```yaml
   dependencies:
     espressif/brookesia_service_nvs: "*"
   ```

详细说明请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。
