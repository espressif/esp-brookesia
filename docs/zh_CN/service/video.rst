.. _service-video-sec-00:

视频
====

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_service_video <https://components.espressif.com/components/espressif/brookesia_service_video>`_
- 辅助头文件： ``#include "brookesia/service_helper/video.hpp"``
- 辅助类： ``esp_brookesia::service::helper::Video``

.. _service-video-sec-01:

概述
----

`brookesia_service_video` 是为 ESP-Brookesia 生态系统提供的视频服务，主要包括：

- **视频编码**：将摄像头等采集源编码为多种压缩或原始格式（分辨率、帧率、格式可分别配置）。
- **视频解码**：将 H.264、MJPEG 等压缩码流解码为常用 RGB、YUV 等显示或后处理格式。

.. _service-video-sec-02:

功能特性
--------

.. _service-video-sec-03:

编码器
^^^^^^

视频编码服务的输入来自 **本地视频设备**，默认设备路径为 ``/dev/video0``，可在 Kconfig 中配置默认路径前缀与数量。

视频编码服务可将一路输出配置为下列类型之一（每路可单独指定分辨率与帧率）：

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - 类型
     - 说明
   * - **H.264**
     - 常用网络与存储压缩格式
   * - **MJPEG**
     - 逐帧 JPEG 类压缩
   * - **RGB565 / RGB888 / BGR888**
     - RGB 类格式
   * - **YUV420 / YUV422 / O_UYY_E_VYY**
     - YUV 类格式

.. warning::

   目前暂不支持多路输出。

.. _service-video-sec-04:

解码器
^^^^^^

解码服务的输入为压缩码流格式，输出为像素格式，可按业务分别配置。常见取值如下：

.. _service-video-sec-05:

输入（压缩格式）
~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :widths: 28 72
   :header-rows: 1

   * - 格式
     - 说明
   * - **H.264**
     - 常用网络与存储视频压缩格式
   * - **MJPEG**
     - 逐帧 JPEG 类压缩码流

.. _service-video-sec-06:

输出（像素格式）
~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :widths: 36 64
   :header-rows: 1

   * - 格式
     - 说明
   * - **RGB565（大端 / 小端）**
     - 16 位 RGB，适合部分屏驱与显存布局
   * - **RGB888 / BGR888**
     - 24 位 RGB / BGR，常用于显示与图像处理
   * - **YUV420P / YUV422P**
     - 平面 YUV，便于视频处理管线
   * - **YUV422 / UYVY422**
     - 打包 YUV，常见于采集与显示链路
   * - **O_UYY_E_VYY**
     - 特定打包 YUV 布局（视硬件与管线而定）

.. include-build-file:: contract_guides/service/video.inc
