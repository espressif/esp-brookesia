.. _service-picodet-sec-00:

PicoDet
=======

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_service_picodet <https://components.espressif.com/components/espressif/brookesia_service_picodet>`_
- Helper header: ``#include "brookesia/service_helper/media/picodet.hpp"``
- Helper class: ``esp_brookesia::service::helper::PicoDet``

.. _service-picodet-sec-01:

Overview
--------

`brookesia_service_picodet` is an on-device object-detection service based on esp-dl ESPDet-Pico models. It provides:

- **Model lifecycle**: Load one model with ``Open``, reuse it for inference, and release it with ``Close``.
- **File inference**: Detect objects in JPEG or BMP files with ``Detect``.
- **Frame-stream inference**: Subscribe to a caller-selected frame event with ``Attach``.
- **Annotated display output**: Draw cached boxes onto incoming frames and present them through the Display service.

.. _service-picodet-sec-02:

Features
--------

.. _service-picodet-sec-03:

Model Package
^^^^^^^^^^^^^

A model package is a filesystem directory containing an ESP-DL model and a ``manifest.json`` file. The manifest controls the following fields:

.. list-table::
   :widths: 28 72
   :header-rows: 1

   * - Field
     - Description
   * - ``model_file``
     - ESP-DL model filename relative to the package directory.
   * - ``input``
     - Model input width and height.
   * - ``preprocess``
     - Mean, standard deviation, channel swap, and letterbox color.
   * - ``postprocess``
     - Score threshold, NMS threshold, top-K limit, and anchor-point strides.
   * - ``labels``
     - Class names indexed by the category values returned from inference.

Supporting another compatible ESPDet-Pico model normally requires replacing the model package rather than changing the service implementation.

.. _service-picodet-sec-04:

Model Lifecycle and File Inference
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The normal file-inference sequence is:

#. Call ``Open`` with ``model_dir``; optional ``score_thr`` and ``nms_thr`` values override the manifest.
#. Call ``Detect`` with a ``.jpg``, ``.jpeg``, or ``.bmp`` path.
#. Consume the returned ``{category, label, score, box}`` objects; ``box`` is ``[x_min, y_min, x_max, y_max]``.
#. Call ``Close`` to release the detector and model memory.

``GetInfo`` reports whether a model is open together with its input size and labels. The service keeps only one model open at a time.

.. _service-picodet-sec-05:

Frame-Stream Attachment
^^^^^^^^^^^^^^^^^^^^^^^

``Attach`` dynamically subscribes to ``frame_event`` on ``frame_source``. Defaults are ``VideoEncoder0`` and ``StreamSinkFrameReady``. The source service remains owned by the caller.

The subscribed event must contain:

.. list-table::
   :widths: 24 24 52
   :header-rows: 1

   * - Item
     - Type
     - Description
   * - ``Frame``
     - ``RawBuffer``
     - Writable RGB frame data.
   * - ``SinkInfo``
     - ``Object``
     - ``{format, width, height}``; format supports RGB565 and RGB888.

PicoDet runs inference every ``detect_every_n_frames`` frames, caches the latest boxes, and publishes ``DetectionUpdated`` after each inference. For example, a value of ``5`` means inference runs on every fifth frame while cached boxes are used on intermediate frames.

Call ``Detach`` to disconnect the frame event and release any Display output binding. ``Detach`` is idempotent.

.. _service-picodet-sec-06:

Display Output
^^^^^^^^^^^^^^

When ``display_output`` is non-empty, PicoDet registers a Display source, requests the named output, draws boxes in place, and presents frames synchronously with ``Display::present_frame_sync()``.

The incoming frame format must match the selected Display output pixel format. Because detection, drawing, and presentation share the frame callback path, inference latency affects the effective stream frame rate.

.. _service-picodet-sec-07:

Camera Integration
^^^^^^^^^^^^^^^^^^

For camera input:

- Open ``VideoEncoder0`` with ``enable_stream_mode: true`` so it publishes frame buffers.
- Do not enable the Video service's own Display preview when PicoDet owns annotated display output.
- Start and stop the Video service separately; ``Attach`` does not own the camera-service lifecycle.
- Increase ``detect_every_n_frames`` when lower inference frequency is preferable to blocking every frame.

.. _service-picodet-sec-08:

Model Deployment
^^^^^^^^^^^^^^^^

PicoDet accepts a filesystem path and does not generate, download, or install model packages. Common deployment methods are:

- **Pre-provisioned**: Add ``model.espdl`` and ``manifest.json`` to a LittleFS or SD-card image.
- **Runtime download**: Use the HTTP service's ``download_path`` support to store both files, then pass their directory to ``Open``.

.. include-build-file:: contract_guides/service/picodet.inc
