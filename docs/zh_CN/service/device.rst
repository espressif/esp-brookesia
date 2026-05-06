.. _service-device-sec-00:

设备控制
========

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_service_device <https://components.espressif.com/components/espressif/brookesia_service_device>`_
- 辅助头文件： ``#include "brookesia/service_helper/device.hpp"``
- 辅助类： ``esp_brookesia::service::helper::Device``

.. _service-device-sec-01:

概述
----

``brookesia_service_device`` 是面向应用层的设备控制服务。它不是新的硬件驱动，而是应用层访问 HAL 的统一服务接口：服务启动后会发现当前已初始化的 HAL 接口，并把常用的控制类和状态获取类能力封装成 Service 函数与事件。

典型使用方式是：

- 应用层只通过 ``service::helper::Device`` 调用函数或订阅事件。
- HAL adaptor/board 负责提供底层 ``AudioCodecPlayerIface``、``DisplayBacklightIface``、``StorageFsIface``、``PowerBatteryIface`` 等接口。
- ``brookesia_service_device`` 在中间完成参数校验、状态缓存、事件发布，以及部分状态持久化。

因此，应用代码通常不需要直接持有 HAL interface 指针，除非需要访问非常底层或板级私有能力。

.. _service-device-sec-02:

功能特性
--------

.. _service-device-sec-03:

能力发现
^^^^^^^^

服务提供 ``GetCapabilities`` 函数，用于返回当前系统已经注册的 HAL 接口能力。应用可先查询能力，再决定是否显示对应 UI 或调用相关控制函数。

常见能力包括：

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - HAL 接口
     - 典型用途
   * - ``BoardInfoIface``
     - 获取板卡名称、芯片、版本、厂商等静态信息
   * - ``DisplayBacklightIface``
     - 设置/查询背光亮度和背光开关状态
   * - ``AudioCodecPlayerIface``
     - 设置/查询播放音量和静音状态
   * - ``StorageFsIface``
     - 获取已挂载文件系统列表
   * - ``PowerBatteryIface``
     - 获取电池信息、状态和充电配置，支持时可控制充电

.. _service-device-sec-04:

控制类接口
^^^^^^^^^^

控制类接口用于修改设备状态，主要面向 UI、Agent 工具调用或远程控制场景：

- **显示控制**：设置背光亮度百分比，设置背光开关状态。
- **音频控制**：设置播放器音量百分比，设置播放器静音状态。
- **电源控制**：在 HAL 支持时设置电池充电配置，或启用/禁用充电。
- **数据重置**：清除服务保存的音量、静音、亮度等持久化状态并恢复默认值。

亮度和音量传入值使用应用层百分比 ``[0, 100]``，服务会根据 Kconfig 中配置的硬件最小/最大值映射到底层 HAL。

.. _service-device-sec-05:

状态获取类接口
^^^^^^^^^^^^^^

状态获取类接口用于读取设备状态或静态信息：

- **能力与板卡信息**：获取当前 HAL 能力快照，获取板卡静态信息。
- **显示状态**：读取当前背光亮度百分比和背光开关状态。
- **音频状态**：读取当前目标音量百分比和静音状态。
- **存储状态**：读取已挂载文件系统及其挂载点。
- **电池状态**：读取电池能力信息、电压、电量百分比、充电状态、低电量/严重低电量状态等。

.. _service-device-sec-06:

事件通知
^^^^^^^^

服务会在状态变化时发布事件，应用层可以通过 ``service::helper::Device::subscribe_event`` 订阅：

- ``DisplayBacklightBrightnessChanged``
- ``DisplayBacklightOnOffChanged``
- ``AudioPlayerVolumeChanged``
- ``AudioPlayerMuteChanged``
- ``PowerBatteryStateChanged``
- ``PowerBatteryChargeConfigChanged``

其中电池状态会按配置周期轮询 HAL，检测到快照变化后发布事件。

.. _service-device-sec-07:

持久化状态
^^^^^^^^^^

当系统启用并启动 ``brookesia_service_nvs`` 时，Device 服务会尝试保存和恢复以下应用层状态：

- 播放器音量
- 播放器静音
- 背光亮度

如果 NVS 服务不可用，Device 服务仍可工作，只是这些状态不会跨重启保存。

.. _service-device-sec-08:

使用建议
--------

- 在应用启动阶段先初始化所需 HAL 设备，再启动 ``ServiceManager`` 和 Device 服务。
- 调用控制接口前先通过 ``GetCapabilities`` 判断能力是否存在。
- 对 UI 和 Agent 工具调用，优先使用 Device 服务作为访问 HAL 的入口，避免业务层直接依赖具体板级 HAL 实现。
- 对实时音频流、显示刷图等高频数据通道，仍应使用专门服务或 HAL 接口，不建议通过 Device 服务承载大数据流。

.. include-build-file:: contract_guides/service/device.inc
