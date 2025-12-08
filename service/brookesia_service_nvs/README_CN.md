# ESP-Brookesia NVS Service

## 概述

`brookesia_service_nvs` 是为 ESP-Brookesia 生态系统提供的 NVS（Non-Volatile Storage，非易失性存储）服务，提供：

- **命名空间管理**：基于命名空间的键值对存储，支持多个独立的存储空间
- **数据类型支持**：支持 `布尔值`、`整数`、`字符串` 三种基本数据类型
- **持久化存储**：数据存储在 NVS 分区中，断电后数据不丢失
- **线程安全**：可选基于 `TaskScheduler` 实现异步任务调度，保证线程安全
- **灵活查询**：支持列出命名空间中的所有键值对，或按需获取指定键的值

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

### 核心功能

- **列出键值对**：列出命名空间中所有键值对的信息（包括键名、类型等）
- **设置键值对**：支持批量设置多个键值对
- **获取键值对**：支持获取指定键的值，或获取命名空间中的所有键值对
- **删除键值对**：支持删除指定键，或清空整个命名空间

## 开发环境要求

使用本库前，请确保已安装以下 SDK 开发环境：

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> SDK 的安装方法请参阅 [ESP-IDF 编程指南 - 安装](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

`brookesia_service_nvs` 具有以下依赖组件：

| **依赖组件** | **版本要求** |
| ------------ | ------------ |
| [cmake_utilities](https://components.espressif.com/components/espressif/cmake_utilities) | 0.* |
| [brookesia_service_manager](https://components.espressif.com/components/espressif/brookesia_service_manager) | 0.7.* |
| [brookesia_service_helper](https://components.espressif.com/components/espressif/brookesia_service_helper) | 0.7.* |

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
