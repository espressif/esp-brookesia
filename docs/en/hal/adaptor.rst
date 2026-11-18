.. _hal-adaptor-sec-00:

ESP Device Board Adaptation
===========================

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_hal_adaptor <https://components.espressif.com/components/espressif/brookesia_hal_adaptor>`_
- Public header: ``#include "brookesia/hal_adaptor.hpp"``

.. _hal-adaptor-sec-01:

Overview
--------

``brookesia_hal_adaptor`` is the board-level HAL adaptor of ESP-Brookesia. Based on the device/interface model in :ref:`HAL Interface <hal-interface-index-sec-00>`, it initialises real peripherals via ``esp_board_manager`` and ESP-IDF drivers, then registers **system**, **network**, **audio**, **display**, **storage**, **power**, **video**, **expansion module**, and **Wi-Fi** capabilities into the global HAL table for upper layers to discover by name.

.. _hal-adaptor-sec-02:

Features
--------

.. _hal-adaptor-sec-03:

Built-In Devices
^^^^^^^^^^^^^^^^

The component ships multiple board-level devices, each registered as a singleton; after initialisation they publish their interfaces into the global table:

.. list-table::
   :widths: 22 38 40
   :header-rows: 1

   * - Device class (logical name)
     - Registered interface implementations
     - Notes
   * - ``SystemDevice`` (``"System"``)
     - ``BoardInfoIface`` (``BOARD_INFO_IMPL_NAME``)
     - Reads static board metadata and publishes system-level board information.
   * - ``NetworkDevice`` (``"Network"``)
     - ``SntpClientIface`` (``SNTP_CLIENT_IFACE_NAME``), ``HttpClientIface`` (``HTTP_CLIENT_IFACE_NAME``)
     - Provides platform SNTP and HTTP/HTTPS client capabilities for services.
   * - ``AudioDevice`` (``"Audio"``)
     - ``AudioCodecPlayerIface`` (``CODEC_PLAYER_IMPL_NAME``), ``AudioCodecRecorderIface`` (``CODEC_RECORDER_IMPL_NAME``)
     - Playback via board Audio DAC; recording via Audio ADC. Each sub-implementation can be disabled in Kconfig; requires board capability ``ESP_BOARD_DEV_AUDIO_CODEC_SUPPORT``.
   * - ``DisplayDevice`` (``"Display"``)
     - ``DisplayBacklightIface`` (``LEDC_BACKLIGHT_IMPL_NAME``), ``DisplayPanelIface`` (``LCD_PANEL_IMPL_NAME``), ``DisplayTouchIface`` (``LCD_TOUCH_IMPL_NAME``)
     - LEDC backlight, LCD panel, and I2C touch can each be disabled; require ``ESP_BOARD_DEV_LEDC_CTRL_SUPPORT``, ``ESP_BOARD_DEV_DISPLAY_LCD_SUPPORT``, ``ESP_BOARD_DEV_LCD_TOUCH_I2C_SUPPORT`` respectively.
   * - ``StorageDevice`` (``"Storage"``)
     - ``FileSystemIface`` (``GENERAL_FS_IMPL_NAME``), ``KeyValueIface`` (``KV_IMPL_NAME``)
     - General filesystem implementation for LittleFS, optional SPIFFS, flash FATFS, and SD card / FATFS; KV implementation backed by ESP-IDF NVS. Filesystem backends are enabled per Kconfig, and LittleFS / flash FATFS are mounted directly by the adaptor.
   * - ``PowerDevice`` (``"Power"``)
     - ``PowerBatteryIface`` (``BATTERY_IMPL_NAME``)
     - Battery and charger capability implementation. Supports ADC voltage estimation or AXP2101 power-management-chip backends; can query level, voltage, power source, and charge state, and control charger configuration when supported by the underlying hardware.
   * - ``VideoDevice`` (``"Video"``)
     - ``CameraIface`` and video processor interfaces
     - Publishes camera and video processing interfaces when the selected board exposes them.
   * - ``ExpansionDevice`` (``"Expansion"``)
     - ``expansion::ModuleManagerIface`` (``"ExpansionModuleManager"``)
     - Optionally publishes stable expansion-slot state, queries, and change events. Board-specific providers identify and claim the actual modules.
   * - ``WifiDevice`` (``"WiFi"``)
     - ``BasicIface`` (``BASIC_IMPL_NAME``), ``StationIface`` (``STA_IMPL_NAME``), ``SoftApIface`` (``SOFTAP_IMPL_NAME``)
     - ESP-IDF Wi-Fi backend for single-shot lifecycle, STA, scan, SoftAP, and provisioning actions. Retry, fallback, and auto-connect policy are owned by ``brookesia_service_wifi``.

.. _hal-adaptor-sec-04:

Configuration
^^^^^^^^^^^^^

Each device and sub-interface can be enabled or disabled individually under **ESP-Brookesia: Hal Adaptor Configurations** in ``menuconfig``; default capability parameters (volume range, recording format, backlight range, battery low-level thresholds, ADC voltage conversion parameters, etc.) are also adjustable in ``menuconfig``, and are mapped to compile-time macros by ``macro_configs.h``.

To override default capability parameters before initialisation, call ``set_codec_player_info``, ``set_codec_recorder_info``, or ``set_ledc_backlight_info`` on the corresponding device singleton. Calls after initialisation typically have no effect.

.. _hal-adaptor-audio-format:

Audio Processor Format
^^^^^^^^^^^^^^^^^^^^^^

Before opening codecs that share an I2S clock, the audio processor aligns playback sample rate, bit depth, and channel count with the recorder's declared format. The codecs and processor metadata use that effective format; the caller's requested playback configuration remains unchanged. Set the recorder format before initialisation when playback needs a specific shared-clock format.

.. _hal-adaptor-expansion-modules:

Expansion Module Support
^^^^^^^^^^^^^^^^^^^^^^^^

``CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES`` controls the generic expansion framework. It defaults to disabled, so boards without expansion hardware do not create an expansion device, provider, scanner, or background task. A capable board may enable the option in its board defaults.

When enabled, ``hal::expansion::ModuleManagerIface`` is available as ``Expansion:ModuleManager:0``. ``get_module_infos()`` returns ``ModuleInfo`` snapshots containing the provider, slot, type, board identity, generation, and ``ModuleState``. ``add_event_listener()`` and ``remove_event_listener()`` subscribe to stable changes; callbacks receive the complete updated snapshot.

Module states are defined as follows:

.. list-table::
   :widths: 24 76
   :header-rows: 1

   * - State
     - Meaning
   * - ``Unknown``
     - No stable scan result is available yet.
   * - ``Empty``
     - No module is detected in the slot.
   * - ``Invalid``
     - The module descriptor is invalid.
   * - ``Unsupported``
     - The descriptor is valid, but the module type or slot is unsupported.
   * - ``Ready``
     - The module is ready to open.
   * - ``Active``
     - The module is claimed.
   * - ``Error``
     - Detection or a resource operation failed.

Scanning only identifies modules; functional hardware is initialized when the corresponding HAL interface is opened. The board implementation handles detection protocols and resource ownership, while the generic layer provides state queries and events.

When using the event interfaces:

- Events are delivered asynchronously in order. Claim and release operations do not invoke user callbacks inline.
- Keep callbacks short and do not wait for other event callbacks.
- Removing a listener cancels callbacks that have not started and waits for a running callback to finish; a listener may also remove itself from its callback.
- Subscribe before reading state snapshots, and use each slot's ``generation`` to ignore older events.

The Device Service exposes ``GetExpansionModuleInfos`` and ``ExpansionModuleChanged`` for applications that should not access HAL directly. Board-specific support and hot-plug limitations are documented with the board; see :ref:`Brookesia Adaptation Guide <hal-boards-espressif-mosaico>`.

.. _hal-adaptor-lifecycle:

Lifecycle and Error Handling
----------------------------

Brookesia serializes the Board Manager calls it manages. Third-party code that calls Board Manager directly is outside this guarantee and should avoid accessing the same device concurrently with HAL operations.

- **Camera**: Opening checks that the video device is available and attempts to release resources acquired by a failed attempt. Cleanup failures are logged; HAL retains responsibility for unfinished cleanup after interface destruction.
- **Frame callbacks**: Encoder state queries are allowed. Call ``stop()`` and ``close()`` from another task. During a stop or close operation, ``open()`` and ``start()`` fail.
- **Recoverable errors**: When resources remain valid, cleanup can continue during a later open operation.
- **Uncertain release state**: The affected peripheral is isolated and the original error is retained. Its handle cannot be acquired or released again until a manual restart.
- **SPI drawing**: After a synchronous draw fails or times out, it still waits for submitted DMA transfers to finish. The actual return time can therefore exceed the configured timeout. If the transfers cannot be drained safely, the call remains blocked until a manual restart.

.. _hal-adaptor-dependency-patches:

Dependency Patches
------------------

The component applies patches from ``hal/brookesia_hal_adaptor/tools`` according to the enabled features and dependency components. The Mosaico integration uses Board Manager 0.5.15; other dependency versions are managed by component manifests and the project's dependency lock file.

.. list-table::
   :widths: 50 50
   :header-rows: 1

   * - Patch
     - Purpose
   * - ``esp_board_manager_periph_deinit_retry``
     - Isolate handles after an uncertain peripheral release to prevent double release or reuse.
   * - ``esp_board_manager_dvp_camera_deinit``
     - Report camera close errors and release video and I2C resources in order.
   * - ``esp_video_dvp_deinit_order``
     - Check whether the video device is still open before destroying its sensor.
   * - ``av_processor_frame_mode_stop``
     - Stop the capture pipeline in frame mode.
   * - ``esp_capture_v4l2_uyvy_support``
     - Support UYVY/YUYV format negotiation and cleanup after negotiation failure.
   * - ``esp_board_manager_adc_idf6``
     - Use equivalent ADC LL channel counts when the old SOC macro is absent, retaining board ADC validation.
   * - ``esp_codec_dev_adc_idf6``
     - Use equivalent ADC LL channel counts when the old SOC macro is absent, retaining ADC capture and bounds checks.

.. note::

   Configuration fails if a required patch cannot be applied or an incompatible older patch is detected. Check dependency versions and local changes; when restoring a dependency, use the version recorded in the lock file.

.. _hal-adaptor-media-dependencies:

Audio/Video Dependencies and Models
-----------------------------------

av_processor 0.7.0 uses GMF AI Audio 1.0.2, ESP-SR 2.5.3, and ESP-DL 3.3.13, retaining the old-P4 / IDF 6 speech library. media_lib_sal 1.0.2 includes the ESP-IDF 6 TLS fix; the old TLS patch is retired. Non-video board profiles and ESP32-C5 audio/console profiles set ``CONFIG_VIDEO_PROCESSOR_ENABLE=n`` so audio-only targets do not pull in ``esp_capture`` or ``esp_video``.

HAL registers the SR 2.5.3 models using the upstream SR 2.5.5 compile requirements. Registering requirements enables global operator/kernel pruning (``DL_COMPILE_ALL=0``). Every native component loading additional ``.espdl`` models must register its own requirements; ESP-DL combines them with the SR requirements.

Declare ``espressif/esp-dl`` as a dependency of the model component. In its ``CMakeLists.txt``, call ``idf_component_register()`` first, then register the requirement file using the resolved component name:

.. code-block:: cmake

    idf_build_get_property(build_components BUILD_COMPONENTS)
    if("espressif__esp-dl" IN_LIST build_components)
        idf_component_get_property(
            dl_dir espressif__esp-dl COMPONENT_DIR)
    else()
        idf_component_get_property(
            dl_dir esp-dl COMPONENT_DIR)
    endif()
    include("${dl_dir}/cmake/compile_register.cmake")
    esp_dl_compile_add_yml(
        "${CMAKE_CURRENT_LIST_DIR}/model.compile_req.yml")

Derive the operators and kernels from each model. A model requiring only ``Relu`` can use this ``model.compile_req.yml``:

.. code-block:: yaml

    kernel_abi: 1
    ops: [Relu]
    kernels: []

ESP-DL 3.3.13 rejects unknown operators, a ``kernel_abi`` newer than its supported ABI, or conflicting ABI values across requirement files. Unknown kernels produce a warning and are filtered out.

.. _hal-adaptor-sec-05:

API Reference
-------------

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/system/device.inc

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/network/device.inc

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/display/device.inc

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/audio/device.inc

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/storage/device.inc

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/power/device.inc

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/video/device.inc

.. include-build-file:: inc/module_manager.inc

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/expansion/device.inc

.. include-build-file:: inc/module_provider.inc

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/wifi/device.inc
