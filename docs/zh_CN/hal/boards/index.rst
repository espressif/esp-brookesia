.. _hal-boards-sec-00:

HAL 开发板支持
==============

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_hal_boards <https://components.espressif.com/components/espressif/brookesia_hal_boards>`_

.. _hal-boards-sec-01:

概述
----

``brookesia_hal_boards`` 是 ESP-Brookesia 的开发板配置集合，基于 ``esp_board_manager`` 组件以 YAML 文件描述各开发板的外设拓扑与设备参数，供 :ref:`HAL 适配 <hal-adaptor-sec-00>` 在运行时无需硬编码即可完成硬件初始化。

.. _hal-boards-sec-02:

支持的开发板
------------

- :doc:`乐鑫官方开发板 <espressif>`
- :doc:`微雪电子开发板 <waveshare>`

.. toctree::
   :hidden:

   乐鑫官方开发板 <espressif>
   微雪电子开发板 <waveshare>

.. _hal-boards-sec-03:

目录结构
--------

每块开发板在 ``boards/<厂商>/<板级名称>/`` 目录下包含以下文件：

.. code-block:: text

   boards/
   └── <厂商>/
       └── <板级名称>/
          ├── components/...           # 特殊组件依赖（可选，用于支持非标准外设或特殊功能）
          ├── board_info.yaml          # 开发板元信息（名称、芯片、版本、制造商等）
          ├── board_devices.yaml       # 逻辑设备配置（音频编解码、LCD 显示、触摸、存储等）
          ├── board_peripherals.yaml   # 底层外设配置（I2C/I2S/SPI 总线、GPIO、LEDC 等）
          ├── sdkconfig.defaults.board # 开发板专用 Kconfig 默认值（Flash、PSRAM 等）
          └── setup_device.c           # 板级设备工厂回调（可选，用于需要自定义驱动初始化的场景）

.. note::

   关于开发板配置格式的完整说明，请参考 `esp_board_manager 组件文档 <https://github.com/espressif/esp-gmf/blob/main/packages/esp_board_manager/README_CN.md>`_。

.. _hal-boards-sec-04:

设备类型
^^^^^^^^

``board_devices.yaml`` 以设备为单位描述板上的逻辑功能模块，常见设备类型如下：

.. list-table::
   :widths: 25 75
   :header-rows: 1

   * - 设备类型
     - 说明
   * - ``audio_codec``
     - 音频编解码芯片（DAC 播放 / ADC 录音），支持 ES8311、ES7210、内置 ADC 等
   * - ``display_lcd``
     - LCD 显示屏，支持 SPI（ST77916、ILI9341）、DSI（EK79007）等接口
   * - ``lcd_touch``
     - 触摸面板，支持 CST816S、GT911 等 I2C 触控芯片
   * - ``ledc_ctrl``
     - 基于 LEDC 的 PWM 背光控制
   * - ``fs_fat`` / ``fs_spiffs``
     - 文件系统存储，支持 SD 卡（SDMMC/SPI）和 SPIFFS
   * - ``camera``
     - 摄像头（CSI 接口）
   * - ``power_ctrl``
     - GPIO 供电控制（音频电源、LCD/SD 卡电源等）
   * - ``gpio_ctrl``
     - 通用 GPIO 控制（LED、按键等）
   * - ``gpio_expander``
     - GPIO 扩展芯片（例如 TCA9554 等）
   * - ``custom``
     - 自定义设备类型，用于实现非标准外设或特殊功能

.. _hal-boards-sec-05:

外设配置
^^^^^^^^

``board_peripherals.yaml`` 描述底层硬件资源的引脚分配与参数：

- **I2C**：SDA/SCL 引脚、端口号
- **I2S**：MCLK/BCLK/WS/DOUT/DIN 引脚、采样率、位深
- **SPI**：MOSI/MISO/CLK/CS 引脚、SPI 主机编号、传输大小
- **LEDC**：背光 GPIO、PWM 频率与分辨率
- **GPIO**：供电控制、功放使能、LED 等独立引脚配置

``sdkconfig.defaults.board`` 包含与该开发板硬件强相关的 Kconfig 默认值，例如 Flash 大小、PSRAM 模式与频率、CPU 主频，以及 ``brookesia_hal_adaptor`` 的录音格式参数等。

若驱动需要自定义初始化流程（例如向 LCD 传入厂商特定的寄存器序列），则通过 ``setup_device.c`` 中的工厂回调函数实现。

.. _hal-boards-sec-06:

使用方法
--------

.. _hal-boards-sec-07:

选择开发板
^^^^^^^^^^

请参考 :ref:`快速入门 - 如何使用示例工程 <getting-started-example-projects>`。

.. _hal-boards-sec-08:

添加自定义开发板
^^^^^^^^^^^^^^^^

在 ``boards/<厂商>/`` 目录下创建新的开发板子目录，按以下顺序添加文件：

1. **board_info.yaml**：填写开发板名称、芯片型号、版本号和描述
2. **board_peripherals.yaml**：按实际引脚和总线配置填写外设参数
3. **board_devices.yaml**：按实际板载外设填写设备类型和配置
4. **sdkconfig.defaults.board**：添加与该板相关的 Kconfig 默认值
5. **setup_device.c** *(可选)*：如驱动需要额外初始化步骤则实现对应工厂函数
6. **components/...** *(可选)*：添加特殊组件依赖，用于支持非标准外设或特殊功能

完成后执行 ``idf.py gen-bmgr-config -b <new_board>`` 即可使用。
