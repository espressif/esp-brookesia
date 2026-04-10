.. _service-video-sec-00:

Video
=====

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_service_video <https://components.espressif.com/components/espressif/brookesia_service_video>`_
- Helper header: ``#include "brookesia/service_helper/video.hpp"``
- Helper class: ``esp_brookesia::service::helper::Video``

.. _service-video-sec-01:

Overview
--------

`brookesia_service_video` provides:

- **Encoding**: Capture to compressed or raw formats (resolution, FPS, format per stream).
- **Decoding**: H.264, MJPEG, and others to RGB/YUV for display or processing.

.. _service-video-sec-02:

Features
--------

.. _service-video-sec-03:

Encoder
^^^^^^^

Input comes from a **local video device** (default ``/dev/video0``; Kconfig can set path prefix and count).

Each output stream can be one of (per-stream resolution and FPS):

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - Type
     - Description
   * - **H.264**
     - Common network/storage codec
   * - **MJPEG**
     - Frame-wise JPEG-like compression
   * - **RGB565 / RGB888 / BGR888**
     - RGB formats
   * - **YUV420 / YUV422 / O_UYY_E_VYY**
     - YUV formats

.. warning::

   Multiple simultaneous outputs are not supported yet.

.. _service-video-sec-04:

Decoder
^^^^^^^

Compressed input and pixel output; configure per use case.

.. _service-video-sec-05:

Input (compressed)
~~~~~~~~~~~~~~~~~~

.. list-table::
   :widths: 28 72
   :header-rows: 1

   * - Format
     - Description
   * - **H.264**
     - Common video codec
   * - **MJPEG**
     - Frame-wise JPEG stream

.. _service-video-sec-06:

Output (pixel)
~~~~~~~~~~~~~~

.. list-table::
   :widths: 36 64
   :header-rows: 1

   * - Format
     - Description
   * - **RGB565 (BE/LE)**
     - 16-bit RGB for some panels/framebuffers
   * - **RGB888 / BGR888**
     - 24-bit RGB/BGR
   * - **YUV420P / YUV422P**
     - Planar YUV for pipelines
   * - **YUV422 / UYVY422**
     - Packed YUV for capture/display
   * - **O_UYY_E_VYY**
     - Packed layout (hardware/pipeline dependent)

.. include-build-file:: contract_guides/service/video.inc
