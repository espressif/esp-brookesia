.. _service-picodet-sec-00:

PicoDet
=======

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_service_picodet <https://components.espressif.com/components/espressif/brookesia_service_picodet>`_
- 辅助头文件： ``#include "brookesia/service_helper/media/picodet.hpp"``
- 辅助类： ``esp_brookesia::service::helper::PicoDet``

.. _service-picodet-sec-01:

概述
----

`brookesia_service_picodet` 是基于 esp-dl ESPDet-Pico 模型的端侧目标检测服务，提供：

- **模型生命周期**：通过 ``Open`` 加载一个模型，复用于后续推理，并通过 ``Close`` 释放。
- **文件推理**：通过 ``Detect`` 检测 JPEG 或 BMP 图片中的目标。
- **帧流推理**：通过 ``Attach`` 订阅调用方指定的帧事件。
- **检测框显示**：在输入帧上绘制缓存的检测框，并通过 Display 服务输出。

.. _service-picodet-sec-02:

功能特性
--------

.. _service-picodet-sec-03:

模型包
^^^^^^

模型包是一个文件系统目录，其中包含 ESP-DL 模型和 ``manifest.json`` 文件。manifest 控制以下字段：

.. list-table::
   :widths: 28 72
   :header-rows: 1

   * - 字段
     - 说明
   * - ``model_file``
     - 模型包目录下 ESP-DL 模型文件的相对路径。
   * - ``input``
     - 模型输入宽度和高度。
   * - ``preprocess``
     - 均值、标准差、通道交换和 letterbox 填充颜色。
   * - ``postprocess``
     - 置信度阈值、NMS 阈值、top-K 限制和 anchor-point strides。
   * - ``labels``
     - 按推理返回的 category 值索引的类别名称。

适配其他兼容的 ESPDet-Pico 模型时，通常只需替换模型包，无需修改服务实现。

.. _service-picodet-sec-04:

模型生命周期与文件推理
^^^^^^^^^^^^^^^^^^^^^^

文件推理的一般流程如下：

#. 使用 ``model_dir`` 调用 ``Open``；可选的 ``score_thr`` 和 ``nms_thr`` 会覆盖 manifest 配置。
#. 使用 ``.jpg``、``.jpeg`` 或 ``.bmp`` 文件路径调用 ``Detect``。
#. 读取返回的 ``{category, label, score, box}`` 对象；``box`` 格式为 ``[x_min, y_min, x_max, y_max]``。
#. 调用 ``Close`` 释放检测器和模型内存。

``GetInfo`` 返回模型是否已打开，以及模型输入尺寸和标签。服务同一时间只保持一个模型处于打开状态。

.. _service-picodet-sec-05:

帧流接入
^^^^^^^^

``Attach`` 会按名称动态订阅 ``frame_source`` 上的 ``frame_event``。默认值分别为 ``VideoEncoder0`` 和 ``StreamSinkFrameReady``。帧源服务仍由调用方管理。

被订阅事件必须包含：

.. list-table::
   :widths: 24 24 52
   :header-rows: 1

   * - 内容
     - 类型
     - 说明
   * - ``Frame``
     - ``RawBuffer``
     - 可写的 RGB 帧数据。
   * - ``SinkInfo``
     - ``Object``
     - ``{format, width, height}``；format 支持 RGB565 和 RGB888。

PicoDet 每隔 ``detect_every_n_frames`` 帧执行一次推理，缓存最新检测框，并在每次推理后发布 ``DetectionUpdated``。例如，取值为 ``5`` 表示每 5 帧推理一次，中间帧继续使用缓存的检测框。

调用 ``Detach`` 可断开帧事件并释放 Display 输出绑定。``Detach`` 支持幂等调用。

.. _service-picodet-sec-06:

显示输出
^^^^^^^^

当 ``display_output`` 非空时，PicoDet 会注册 Display source、请求指定输出、原地绘制检测框，并通过 ``Display::present_frame_sync()`` 同步提交帧。

输入帧格式必须与所选 Display 输出的像素格式一致。检测、绘制和显示共用同一个帧回调路径，因此推理耗时会影响实际帧流速率。

.. _service-picodet-sec-07:

摄像头接入
^^^^^^^^^^

使用摄像头输入时：

- 以 ``enable_stream_mode: true`` 打开 ``VideoEncoder0``，使其发布帧缓冲区。
- 当 PicoDet 负责检测框显示输出时，不要同时启用 Video 服务自身的 Display 预览。
- 单独启动和停止 Video 服务；``Attach`` 不管理摄像头服务的生命周期。
- 希望降低推理频率、避免阻塞每一帧时，可增大 ``detect_every_n_frames``。

.. _service-picodet-sec-08:

模型部署
^^^^^^^^

PicoDet 只接收文件系统路径，不负责生成、下载或安装模型包。常用部署方式如下：

- **预置模型**：将 ``model.espdl`` 和 ``manifest.json`` 添加到 LittleFS 或 SD 卡镜像。
- **运行时下载**：使用 HTTP 服务的 ``download_path`` 将两个文件写入存储，再把所在目录传给 ``Open``。

.. include-build-file:: contract_guides/service/picodet.inc
