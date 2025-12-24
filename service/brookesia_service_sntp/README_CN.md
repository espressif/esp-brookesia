# ESP-Brookesia SNTP Service

* [English Version](./README.md)

## 概述

`brookesia_service_sntp` 是为 ESP-Brookesia 生态系统提供的 SNTP（Simple Network Time Protocol，简单网络时间协议）服务，提供：

- **NTP 服务器管理**：支持配置多个 NTP 服务器，自动从服务器列表中选择可用的服务器进行时间同步
- **时区设置**：支持设置系统时区，自动应用时区偏移
- **自动时间同步**：自动检测网络连接状态，在网络可用时自动启动时间同步
- **状态查询**：支持查询时间同步状态、当前配置的服务器列表和时区信息
- **持久化存储**：可选搭配 `brookesia_service_nvs` 服务持久化保存 NTP 服务器列表和时区信息

## 目录

- [ESP-Brookesia SNTP Service](#esp-brookesia-sntp-service)
  - [概述](#概述)
  - [目录](#目录)
  - [功能特性](#功能特性)
    - [NTP 服务器管理](#ntp-服务器管理)
    - [时区设置](#时区设置)
    - [核心功能](#核心功能)
    - [自动管理](#自动管理)
  - [开发环境要求](#开发环境要求)
  - [添加到工程](#添加到工程)

## 功能特性

### NTP 服务器管理

SNTP 服务支持配置多个 NTP 服务器：

- **默认服务器**：默认使用 `"pool.ntp.org"` 作为 NTP 服务器
- **多服务器支持**：可以配置多个 NTP 服务器，服务会自动选择可用的服务器
- **服务器列表**：支持获取当前配置的所有 NTP 服务器列表

### 时区设置

SNTP 服务支持设置系统时区：

- **默认时区**：默认时区为 `"CST-8"`（中国标准时间，UTC+8）
- **时区格式**：支持标准的时区字符串格式（如 `"UTC"`、`"CST-8"`、`"EST-5"` 等）
- **自动应用**：设置时区后会自动应用到系统时间

### 核心功能

- **设置 NTP 服务器**：支持设置一个或多个 NTP 服务器
- **设置时区**：支持设置系统时区
- **启动服务**：启动 SNTP 服务，开始时间同步
- **停止服务**：停止 SNTP 服务，停止时间同步
- **获取服务器列表**：获取当前配置的 NTP 服务器列表
- **获取时区**：获取当前配置的时区
- **检查同步状态**：检查系统时间是否已与 NTP 服务器同步
- **重置数据**：重置所有配置数据到默认值

### 自动管理

- **自动加载配置**：服务启动时自动从 NVS 加载保存的配置
- **自动保存配置**：配置更改后自动保存到 NVS
- **网络检测**：自动检测网络连接状态，在网络可用时自动启动时间同步
- **状态监控**：自动监控时间同步状态，更新同步标志

## 开发环境要求

使用本库前，请确保已安装以下 SDK 开发环境：

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> SDK 的安装方法请参阅 [ESP-IDF 编程指南 - 安装](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

## 添加到工程

`brookesia_service_sntp` 已上传到 [Espressif 组件库](https://components.espressif.com/)，您可以通过以下方式将其添加到工程中：

1. **使用命令行**

   在工程目录下运行以下命令：

   ```bash
   idf.py add-dependency "espressif/brookesia_service_sntp"
   ```

2. **修改配置文件**

   在工程目录下创建或修改 *idf_component.yml* 文件：

   ```yaml
   dependencies:
     espressif/brookesia_service_sntp: "*"
   ```

详细说明请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。
