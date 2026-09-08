.. _hal-boards-esp-mosaico-v1-0-sec-00:

ESP-Mosaico V1.0
=================

:link_to_translation:`en:[English]`

概述
----

``esp_mosaico_v1_0`` 配置支持 ESP-Mosaico V1.0 核心板及其可选扩展模块插槽。核心板支持包括系统与网络功能、Wi-Fi、480 x 480 显示与触摸、音频播放与录制、电池电压、内部 LittleFS/NVS 以及外部 NAND FATFS。

扩展模块与核心板设备分开发现。模块未插入或暂不支持都不会阻止核心板或 System Super 启动。

扩展模块配置
----------------

扩展模块功能由 ``Component config > ESP-Brookesia: Hal Adaptor Configurations > Expansion Modules`` 下的 ``CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES`` 控制。由于很多开发板没有扩展插槽，通用默认值为关闭；ESP-Mosaico 板级默认值为开启。

已有工程的 ``sdkconfig`` 优先于板级默认值。更新板级包后，旧配置仍可能保持 Expansion、Video、Camera、DVP 或摄像头传感器关闭。验证摄像头前请优先使用干净的默认 ``sdkconfig``，并在 ``menuconfig`` 中确认这些选项。Board Manager 0.5.x 不会向非默认 ``SDKCONFIG`` 路径自动注入 ``board_manager.defaults``；这类构建必须在 ``SDKCONFIG_DEFAULTS`` 中显式包含 ``components/gen_bmgr_codes/board_manager.defaults``。

使用本地 ESP-IDF 环境，并在构建前生成板级配置：

.. code-block:: console

   idf.py --preview gen-bmgr-config -b esp_mosaico_v1_0
   idf.py --preview set-target esp32s31
   idf.py --preview menuconfig
   idf.py --preview build

检测与生命周期
--------------

扩展功能开启后，Mosaico provider 会每隔 300 ms 读取当前可安全扫描插槽的身份 EEPROM。结果必须连续三次保持一致，因此插入、拔出或身份变化通常会在约 0.9 秒后发布。

公开的 ``hal::expansion::ModuleManagerIface`` 注册为 ``Expansion:ModuleManager:0``。``get_module_infos()`` 返回稳定的插槽快照，``add_event_listener()`` 和 ``remove_event_listener()`` 管理变更通知。Device Service 通过 ``GetExpansionModuleInfos`` 提供同样的信息，并发布 ``ExpansionModuleChanged`` 事件。

每个插槽会报告以下状态之一：

.. list-table::
   :widths: 24 76
   :header-rows: 1

   * - 状态
     - 含义
   * - ``Unknown``
     - 插槽尚未得到稳定的扫描结果。
   * - ``Empty``
     - 插槽中没有模块 EEPROM 应答。
   * - ``Invalid``
     - EEPROM 有应答，但描述信息为空、格式错误或 CRC 无效。Provider 不会猜测模块类型。
   * - ``Unsupported``
     - 描述信息有效，但模块类型或所选插槽没有可用驱动。
   * - ``Ready``
     - 已插入受支持的模块，可以打开。
   * - ``Active``
     - 客户端已占用模块，对应硬件资源已初始化。
   * - ``Error``
     - 扫描、占用或释放硬件时发生运行错误。

扫描只负责识别模块，不会初始化模块的功能硬件。只有匹配的 HAL 使用者打开模块时才会占用并初始化，最后一个客户端关闭后归还资源。

已支持的扩展模块
--------------------

.. list-table::
   :widths: 24 14 20 42
   :header-rows: 1

   * - 模块
     - 插槽
     - 结果
     - 当前支持范围
   * - 摄像头模块
     - 左槽
     - ``Ready`` / ``Active``
     - 通过 Camera HAL 和 Video 进行取流。传感器在运行时探测：OV3640 输出原生 RGB565、640 x 480、7 fps，SC101IOT 输出 YUV422、640 x 480、15 fps。
   * - 摄像头模块
     - 右槽
     - ``Unsupported``
     - 右槽不具备必要的映射，因此不初始化摄像头驱动。
   * - 其他有效模块描述信息
     - 任意
     - ``Unsupported``
     - 仅检测并报告；其他模块驱动将分别增加。

热插拔行为与限制
----------------

- 摄像头已插入但未打开时，两个插槽都会继续扫描。
- 打开摄像头后，由于摄像头数据信号复用了 Mosaico EEPROM 地址选择引脚，两个插槽都会暂停扫描。
- 关闭摄像头会恢复共享引脚并立即触发一次扫描，随后恢复定期扫描。
- 暂不支持取流过程中拔出摄像头。拔出前请先关闭摄像头。
- 当前不包含全屏 480 x 480 裁剪、旋转、缩放和 System Super 摄像头预览体验。

实现边界
--------

通用 adaptor 层负责可选扩展能力、稳定模块状态、查询与事件契约、驱动 provider 注册以及占用生命期，不了解 Mosaico EEPROM 地址、插槽引脚或摄像头传感器初始化。

Mosaico 板级实现负责双槽 EEPROM 发现、描述信息验证、资源仲裁、插槽兼容性和摄像头插槽映射。随后摄像头通过已占用的模块 lease 使用现有通用 Camera HAL 与 Video 通路。

Mosaico 实现参考并改造自 Apache-2.0 许可的 `ESP-Mosaico BSP <https://github.com/esp-mosaico/esp-mosaico-bsp>`_ 提交 ``bef99672e411101489ed19c40527cca1c1dd5bb1``。所需代码均保存在本地，构建该开发板时不依赖或下载该仓库。
