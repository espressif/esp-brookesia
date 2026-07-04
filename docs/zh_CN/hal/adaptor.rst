.. _hal-adaptor-sec-00:

ESP 设备板级适配
================

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_hal_adaptor <https://components.espressif.com/components/espressif/brookesia_hal_adaptor>`_
- 公共头文件： ``#include "brookesia/hal_adaptor.hpp"``

.. _hal-adaptor-sec-01:

概述
----

``brookesia_hal_adaptor`` 是 ESP-Brookesia 的板级 HAL 适配实现，基于 :ref:`HAL 接口 <hal-interface-index-sec-00>` 的设备/接口模型，通过 ``esp_board_manager`` 与 ESP-IDF 驱动初始化真实外设，并将 **系统**、 **网络**、 **音频**、 **显示**、 **存储**、 **电源**、 **视频**、 **Wi-Fi** 等能力注册到全局 HAL 表，供上层按名称发现和使用。

.. _hal-adaptor-sec-02:

功能特性
--------

.. _hal-adaptor-sec-03:

内置设备
^^^^^^^^

本组件提供多台板级设备，每台设备以单例形式注册，初始化后将对应的接口发布到全局表：

.. list-table::
   :widths: 22 38 40
   :header-rows: 1

   * - 设备类（逻辑名）
     - 注册的接口实现
     - 说明
   * - ``SystemDevice`` (``"System"``)
     - ``BoardInfoIface`` (``BOARD_INFO_IMPL_NAME``)
     - 读取静态开发板元信息，并发布系统级板级信息接口。
   * - ``NetworkDevice`` (``"Network"``)
     - ``SntpClientIface`` (``SNTP_CLIENT_IFACE_NAME``)、``HttpClientIface`` (``HTTP_CLIENT_IFACE_NAME``)
     - 为 Service 提供平台 SNTP 与 HTTP/HTTPS 客户端能力。
   * - ``AudioDevice`` (``"Audio"``)
     - ``AudioCodecPlayerIface`` (``CODEC_PLAYER_IMPL_NAME``)、``AudioCodecRecorderIface`` (``CODEC_RECORDER_IMPL_NAME``)
     - 播放经板级 Audio DAC；录音经 Audio ADC。子实现可在 Kconfig 中独立关闭，并依赖板级能力符号 ``ESP_BOARD_DEV_AUDIO_CODEC_SUPPORT``。
   * - ``DisplayDevice`` (``"Display"``)
     - ``DisplayBacklightIface`` (``LEDC_BACKLIGHT_IMPL_NAME``)、``DisplayPanelIface`` (``LCD_PANEL_IMPL_NAME``)、``DisplayTouchIface`` (``LCD_TOUCH_IMPL_NAME``)
     - LEDC 背光、LCD 面板、I2C 触摸可分别开关；分别依赖 ``ESP_BOARD_DEV_LEDC_CTRL_SUPPORT``、``ESP_BOARD_DEV_DISPLAY_LCD_SUPPORT``、``ESP_BOARD_DEV_LCD_TOUCH_I2C_SUPPORT``。
   * - ``StorageDevice`` (``"Storage"``)
     - ``FileSystemIface`` (``GENERAL_FS_IMPL_NAME``)、``KeyValueIface`` (``KV_IMPL_NAME``)
     - 通用文件系统实现支持 LittleFS、可选 SPIFFS、Flash FATFS 与 SD 卡 FATFS；KV 实现基于 ESP-IDF NVS。文件系统后端按 Kconfig 启用，其中 LittleFS 与 Flash FATFS 由 adaptor 直接挂载。
   * - ``PowerDevice`` (``"Power"``)
     - ``PowerBatteryIface`` (``BATTERY_IMPL_NAME``)
     - 电池与充电器能力实现，支持 ADC 电压估算或 AXP2101 电源管理芯片实现；可查询电量、电压、电源来源、充电状态，并在底层支持时控制充电配置。
   * - ``VideoDevice`` (``"Video"``)
     - ``CameraIface`` 与视频处理接口
     - 当所选开发板暴露对应能力时，发布摄像头与视频处理接口。
   * - ``WifiDevice`` (``"WiFi"``)
     - ``BasicIface`` (``BASIC_IMPL_NAME``)、``StationIface`` (``STA_IMPL_NAME``)、``SoftApIface`` (``SOFTAP_IMPL_NAME``)
     - 基于 ESP-IDF 的 Wi-Fi 后端，负责单次生命周期、STA、扫描、SoftAP 与配网动作。重试、fallback 与自动连接策略由 ``brookesia_service_wifi`` 负责。

.. _hal-adaptor-sec-04:

配置与参数
^^^^^^^^^^

各设备与子接口可在 ``menuconfig`` 的 **ESP-Brookesia: Hal Adaptor Configurations** 中独立开启或关闭；默认能力参数（音量范围、录音格式、背光亮度范围、电池低电量阈值、ADC 电压换算参数等）也可在 ``menuconfig`` 中调整，由 ``macro_configs.h`` 映射为编译宏供实现使用。

若需在初始化前覆盖默认能力参数，可在对应设备单例上调用 ``set_codec_player_info``、``set_codec_recorder_info`` 或 ``set_ledc_backlight_info``。初始化完成后再调用通常不会生效。

.. _hal-adaptor-sec-05:

API 参考
--------

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/system/device.inc

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/network/device.inc

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/display/device.inc

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/audio/device.inc

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/storage/device.inc

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/power/device.inc

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/video/device.inc

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/wifi/device.inc
