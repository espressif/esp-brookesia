.. _hal-adaptor-sec-00:

ESP Device Board Adaptation
===========================

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_hal_adaptor <https://components.espressif.com/components/espressif/brookesia_hal_adaptor>`_
- Public header: ``#include "brookesia/hal_adaptor.hpp"``

.. _hal-adaptor-sec-01:

Overview
--------

``brookesia_hal_adaptor`` is the board-level HAL adaptor of ESP-Brookesia. Based on the device/interface model in :ref:`HAL Interface <hal-interface-index-sec-00>`, it initialises real peripherals via ``esp_board_manager`` and ESP-IDF drivers, then registers **system**, **network**, **audio**, **display**, **storage**, **power**, **video**, and **Wi-Fi** capabilities into the global HAL table for upper layers to discover by name.

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
   * - ``WifiDevice`` (``"WiFi"``)
     - ``BasicIface`` (``BASIC_IMPL_NAME``), ``StationIface`` (``STA_IMPL_NAME``), ``SoftApIface`` (``SOFTAP_IMPL_NAME``)
     - ESP-IDF Wi-Fi backend for single-shot lifecycle, STA, scan, SoftAP, and provisioning actions. Retry, fallback, and auto-connect policy are owned by ``brookesia_service_wifi``.

.. _hal-adaptor-sec-04:

Configuration
^^^^^^^^^^^^^

Each device and sub-interface can be enabled or disabled individually under **ESP-Brookesia: Hal Adaptor Configurations** in ``menuconfig``; default capability parameters (volume range, recording format, backlight range, battery low-level thresholds, ADC voltage conversion parameters, etc.) are also adjustable there, and are mapped to compile-time macros by ``macro_configs.h``.

To override default capability parameters before initialisation, call ``set_codec_player_info``, ``set_codec_recorder_info``, or ``set_ledc_backlight_info`` on the corresponding device singleton. Calls after initialisation typically have no effect.

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

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/wifi/device.inc
