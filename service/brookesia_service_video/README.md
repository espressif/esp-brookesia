# ESP-Brookesia Video Service

* [中文版本](./README_CN.md)

## Overview

`brookesia_service_video` is a video service for the ESP-Brookesia ecosystem. It provides:

- **Video encoding**: Encodes camera or other local capture sources into multiple compressed or raw formats, with configurable resolution, frame rate, and format.
- **Video decoding**: Decodes H.264, MJPEG, and similar compressed streams into common RGB and YUV pixel formats for display or further processing.

## Table of Contents

- [ESP-Brookesia Video Service](#esp-brookesia-video-service)
  - [Overview](#overview)
  - [Table of Contents](#table-of-contents)
  - [Features](#features)
    - [Encoder](#encoder)
    - [Decoder](#decoder)
  - [Development Environment](#development-environment)
  - [Add to Your Project](#add-to-your-project)

## Features

### Encoder

The encoder input comes from **local video devices**. The default device path is `/dev/video0`. You can configure the default path prefix and device count in Kconfig.

The encoder can be configured for one output stream with the following types (resolution, frame rate, and format can be set per stream):

| Type | Description |
|------|-------------|
| **H.264** | Common compressed format for network and storage |
| **MJPEG** | Frame-by-frame JPEG-like compression |
| **RGB565 / RGB888 / BGR888** | RGB-family formats |
| **YUV420 / YUV422 / O_UYY_E_VYY** | YUV-family formats |

> [!WARNING]
> Multi-sink (multi-stream) output is not supported yet.

### Decoder

The decoder **input** is a compressed stream format; the **output** is a pixel format. Both are configurable. Typical values:

**Input (compressed)**

| Format | Description |
|--------|-------------|
| **H.264** | Common compressed video for network and storage |
| **MJPEG** | Frame-by-frame JPEG-like stream |

**Output (pixel)**

| Format | Description |
|--------|-------------|
| **RGB565 (LE / BE)** | 16-bit RGB; suits some display controllers and memory layouts |
| **RGB888 / BGR888** | 24-bit RGB/BGR; common for display and image processing |
| **YUV420P / YUV422P** | Planar YUV; suits video pipelines |
| **YUV422 / UYVY422** | Packed YUV; common in capture and display paths |
| **O_UYY_E_VYY** | Specific packed YUV layout (depends on hardware and pipeline) |

## Development Environment

Before using this library, ensure the following SDK is installed:

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> See [ESP-IDF Get Started - Installation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf) for SDK installation.

## Add to Your Project

`brookesia_service_video` is available on the [Espressif Component Registry](https://components.espressif.com/). You can add it in either of these ways:

1. **Command line**

   From your project directory, run:

   ```bash
   idf.py add-dependency "espressif/brookesia_service_video"
   ```

2. **Configuration file**

   Create or edit *idf_component.yml* in your project directory:

   ```yaml
   dependencies:
     espressif/brookesia_service_video: "*"
   ```

For more details, see [Espressif Documentation - IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).
