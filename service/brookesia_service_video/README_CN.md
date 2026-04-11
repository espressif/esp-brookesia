# ESP-Brookesia Video Service

* [English Version](./README.md)

## 概述

`brookesia_service_video` 是为 ESP-Brookesia 生态系统提供的视频服务，主要包括：

- **视频编码**：将摄像头等采集源编码为多种压缩或原始格式（分辨率、帧率、格式可分别配置）。
- **视频解码**：将 H.264、MJPEG 等压缩码流解码为常用 RGB、YUV 等显示或后处理格式。

## 目录

- [ESP-Brookesia Video Service](#esp-brookesia-video-service)
  - [概述](#概述)
  - [目录](#目录)
  - [功能特性](#功能特性)
    - [编码器](#编码器)
    - [解码器](#解码器)
  - [开发环境要求](#开发环境要求)
  - [添加到工程](#添加到工程)

## 功能特性

### 编码器

视频编码服务的输入来自**本地视频设备**，默认设备路径为 `/dev/video0`，可在 Kconfig 中配置默认路径前缀与数量。

视频编码服务可将一路输出配置为下列类型之一（每路可单独指定分辨率与帧率）：

|               类型                |          说明          |
| --------------------------------- | ---------------------- |
| **H.264**                         | 常用网络与存储压缩格式 |
| **MJPEG**                         | 逐帧 JPEG 类压缩       |
| **RGB565 / RGB888 / BGR888**      | RGB 类格式             |
| **YUV420 / YUV422 / O_UYY_E_VYY** | YUV 类格式             |

>[!WARNING]
> 目前暂不支持多路输出。

### 解码器

解码服务的**输入**为压缩码流格式，**输出**为像素格式，可按业务分别配置。常见取值如下：

**输入（压缩格式）**

|   格式    |            说明            |
| --------- | -------------------------- |
| **H.264** | 常用网络与存储视频压缩格式 |
| **MJPEG** | 逐帧 JPEG 类压缩码流       |

**输出（像素格式）**

|           格式            |                 说明                  |
| ------------------------- | ------------------------------------- |
| **RGB565（大端 / 小端）** | 16 位 RGB，适合部分屏驱与显存布局     |
| **RGB888 / BGR888**       | 24 位 RGB / BGR，常用于显示与图像处理 |
| **YUV420P / YUV422P**     | 平面 YUV，便于视频处理管线            |
| **YUV422 / UYVY422**      | 打包 YUV，常见于采集与显示链路        |
| **O_UYY_E_VYY**           | 特定打包 YUV 布局（视硬件与管线而定） |

## 开发环境要求

使用本库前，请确保已安装以下 SDK 开发环境：

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> SDK 的安装方法请参阅 [ESP-IDF 编程指南 - 安装](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

## 添加到工程

`brookesia_service_video` 已上传到 [Espressif 组件库](https://components.espressif.com/)，您可以通过以下方式将其添加到工程中：

1. **使用命令行**

   在工程目录下运行以下命令：

   ```bash
   idf.py add-dependency "espressif/brookesia_service_video"
   ```

2. **修改配置文件**

   在工程目录下创建或修改 *idf_component.yml* 文件：

   ```yaml
   dependencies:
     espressif/brookesia_service_video: "*"
   ```

详细说明请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。
