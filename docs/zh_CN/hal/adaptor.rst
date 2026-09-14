.. _hal-adaptor-sec-00:

ESP 设备板级适配
================

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_hal_adaptor <https://components.espressif.com/components/espressif/brookesia_hal_adaptor>`_
- 公共头文件： ``#include "brookesia/hal_adaptor.hpp"``

.. _hal-adaptor-sec-01:

概述
----

``brookesia_hal_adaptor`` 是 ESP-Brookesia 的板级 HAL 适配实现，基于 :ref:`HAL 接口 <hal-interface-index-sec-00>` 的设备/接口模型，通过 ``esp_board_manager`` 与 ESP-IDF 驱动初始化真实外设，并将 **系统**、 **网络**、 **音频**、 **显示**、 **存储**、 **电源**、 **视频**、 **扩展模块**、 **Wi-Fi** 等能力注册到全局 HAL 表，供上层按名称发现和使用。

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
   * - ``ExpansionDevice`` (``"Expansion"``)
     - ``expansion::ModuleManagerIface`` (``"ExpansionModuleManager"``)
     - 可选发布稳定的扩展插槽状态、查询与变更事件；具体模块由板级 provider 识别和占用。
   * - ``WifiDevice`` (``"WiFi"``)
     - ``BasicIface`` (``BASIC_IMPL_NAME``)、``StationIface`` (``STA_IMPL_NAME``)、``SoftApIface`` (``SOFTAP_IMPL_NAME``)
     - 基于 ESP-IDF 的 Wi-Fi 后端，负责单次生命周期、STA、扫描、SoftAP 与配网动作。重试、fallback 与自动连接策略由 ``brookesia_service_wifi`` 负责。

.. _hal-adaptor-sec-04:

配置与参数
^^^^^^^^^^

各设备与子接口可在 ``menuconfig`` 的 **ESP-Brookesia: Hal Adaptor Configurations** 中独立开启或关闭；默认能力参数（音量范围、录音格式、背光亮度范围、电池低电量阈值、ADC 电压换算参数等）也可在 ``menuconfig`` 中调整，由 ``macro_configs.h`` 映射为编译宏供实现使用。

若需在初始化前覆盖默认能力参数，可在对应设备单例上调用 ``set_codec_player_info``、``set_codec_recorder_info`` 或 ``set_ledc_backlight_info``。初始化完成后再调用通常不会生效。

.. _hal-adaptor-expansion-modules:

扩展模块支持
^^^^^^^^^^^^^^^^

``CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES`` 控制通用扩展框架。该选项默认关闭，因此没有扩展硬件的开发板不会创建扩展设备、provider、扫描器或后台任务。具备扩展能力的开发板可以通过板级默认配置开启。

开启后，``hal::expansion::ModuleManagerIface`` 以 ``Expansion:ModuleManager:0`` 名称提供。``get_module_infos()`` 返回 ``ModuleInfo`` 快照，内容包含 provider、插槽、类型、板级身份、generation 和 ``ModuleState``。``add_event_listener()`` 与 ``remove_event_listener()`` 用于订阅稳定变化；回调会收到更新后的完整快照。

模块状态如下：

.. list-table::
   :widths: 24 76
   :header-rows: 1

   * - 状态
     - 含义
   * - ``Unknown``
     - 尚未得到稳定的扫描结果。
   * - ``Empty``
     - 插槽中未检测到模块。
   * - ``Invalid``
     - 模块描述信息无效。
   * - ``Unsupported``
     - 模块描述信息有效，但模块类型或插槽不受支持。
   * - ``Ready``
     - 模块已就绪，可以打开。
   * - ``Active``
     - 模块已被占用。
   * - ``Error``
     - 检测或资源操作发生错误。

扫描仅识别模块，功能硬件由对应 HAL 接口打开时初始化。板级实现负责检测协议和资源占用，通用层提供状态查询与事件接口。

使用事件接口时，请注意：

- 事件按顺序异步派发，占用和释放操作不会同步执行用户回调。
- 回调应尽快返回，不应等待其他事件回调。
- 移除监听器会取消尚未开始的回调，并等待正在执行的回调完成；监听器也可在自身回调中移除。
- 先订阅事件，再读取状态快照，并使用各插槽的 ``generation`` 忽略较旧的事件。

Device Service 通过 ``GetExpansionModuleInfos`` 和 ``ExpansionModuleChanged`` 为不应直接访问 HAL 的应用提供能力。各开发板的具体支持范围和热插拔限制记录在对应板级文档中；参见 :ref:`Brookesia 适配说明 <hal-boards-espressif-mosaico>`。

.. _hal-adaptor-lifecycle:

生命周期与错误处理
------------------

Brookesia 管理的 Board Manager 调用会串行执行。直接调用 Board Manager 的第三方代码不在此保证范围内，应避免与 HAL 操作并发访问同一设备。

- **摄像头**：打开时检查视频设备是否可用，失败时尝试释放本次获取的资源。清理失败会记录错误日志；接口销毁后，未完成的清理仍由 HAL 管理。
- **帧回调**：允许查询 Encoder 状态；``stop()`` 和 ``close()`` 应在其他任务中调用。停止或关闭期间，``open()`` 和 ``start()`` 返回失败。
- **可恢复错误**：资源仍有效时，可在后续打开操作中继续清理。
- **释放状态不确定**：隔离受影响的外设并保留原始错误，禁止再次获取或释放其句柄；需要手动重启后恢复。
- **SPI 绘制**：同步绘制失败或超时后，仍需等待已提交的 DMA 传输结束，因此实际返回时间可能超过设置的超时时间。无法安全结束传输时，调用保持等待，需要手动重启恢复。

.. _hal-adaptor-dependency-patches:

依赖补丁
--------

组件根据启用的功能和依赖组件应用 ``hal/brookesia_hal_adaptor/tools`` 中的补丁。Mosaico 适配使用 Board Manager 0.5.15；其他依赖版本由组件清单和工程的依赖锁定文件管理。

.. list-table::
   :widths: 50 50
   :header-rows: 1

   * - 补丁
     - 用途
   * - ``esp_board_manager_periph_deinit_retry``
     - 外设释放状态不确定时隔离句柄，防止重复释放或复用。
   * - ``esp_board_manager_dvp_camera_deinit``
     - 返回摄像头关闭错误，按顺序释放视频和 I2C 资源。
   * - ``esp_video_dvp_deinit_order``
     - 销毁传感器前检查视频设备是否仍被打开。
   * - ``av_processor_frame_mode_stop``
     - 支持停止帧模式下的采集管线。
   * - ``esp_capture_v4l2_uyvy_support``
     - 支持 UYVY/YUYV 格式协商及协商失败后的清理。
   * - ``media_lib_sal_esp_tls_idf6``
     - 适配 ESP-IDF 6 的 TLS 接口。

.. note::

   必要补丁无法应用或检测到不兼容的旧补丁时，构建配置会报错。请核对依赖版本及本地修改；需要恢复依赖时，应使用依赖锁定文件中的版本。

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

.. include-build-file:: inc/module_manager.inc

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/expansion/device.inc

.. include-build-file:: inc/module_provider.inc

.. include-build-file:: inc/hal/brookesia_hal_adaptor/include/brookesia/hal_adaptor/wifi/device.inc
