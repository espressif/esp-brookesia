# ESP-Brookesia Service Helper

## 概述

`brookesia_service_helper` 是 ESP-Brookesia 服务开发辅助库，为服务开发者和使用者提供类型安全的定义、模式和工具，用于构建和使用符合 ESP-Brookesia 服务框架规范的服务。

本库主要提供：

- **类型安全定义**：提供枚举、结构体等类型定义，确保类型安全
- **函数模式定义**：提供标准化的函数定义（FunctionSchema），包括函数名、参数、描述等
- **事件模式定义**：提供标准化的事件定义（EventSchema），包括事件名、数据格式等
- **常量定义**：提供服务名称、默认值等常量
- **索引枚举**：提供函数索引、事件索引等枚举，便于在服务实现中使用

## 功能特性

### 辅助类

`brookesia_service_helper` 目前提供以下辅助类：

- [NVS](./include/brookesia/service_helper/nvs.hpp)
- [Wifi](./include/brookesia/service_helper/wifi.hpp)

### 类型安全

所有定义都使用强类型枚举和结构体，确保：

- **编译时类型检查**：避免字符串拼写错误
- **IDE 自动补全**：提供更好的开发体验
- **代码可维护性**：统一的接口定义，便于维护和扩展

### 标准化接口

通过提供标准化的函数和事件定义，确保：

- **接口一致性**：所有服务遵循相同的接口规范
- **文档自动生成**：函数和事件的描述信息可用于自动生成文档
- **参数验证**：统一的参数类型和验证规则

## 开发环境要求

使用本库前，请确保已安装以下 SDK 开发环境：

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> SDK 的安装方法请参阅 [ESP-IDF 编程指南 - 安装](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

`brookesia_service_helper` 具有以下依赖组件：

| **依赖组件** | **版本要求** |
| ------------ | ------------ |
| [brookesia_service_manager](https://components.espressif.com/components/espressif/brookesia_service_manager) | 0.7.* |
| [brookesia_lib_utils](https://components.espressif.com/components/espressif/brookesia_lib_utils) | 0.7.* |

## 添加到工程

`brookesia_service_helper` 已上传到 [Espressif 组件库](https://components.espressif.com/)，您可以通过以下方式将其添加到工程中：

1. **使用命令行**

   在工程目录下运行以下命令：

   ```bash
   idf.py add-dependency "espressif/brookesia_service_helper"
   ```

2. **修改配置文件**

   在工程目录下创建或修改 *idf_component.yml* 文件：

   ```yaml
   dependencies:
     espressif/brookesia_service_helper: "*"
   ```

详细说明请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。
