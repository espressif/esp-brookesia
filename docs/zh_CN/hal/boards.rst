.. _hal-boards-sec-00:

HAL 开发板支持
==============

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_hal_boards <https://components.espressif.com/components/espressif/brookesia_hal_boards>`_

.. _hal-boards-sec-01:

概述
----

``brookesia_hal_boards`` 是 ESP-Brookesia 的开发板配置集合，基于 ``esp_board_manager`` 组件以 YAML 文件描述各开发板的外设拓扑与设备参数，供 :ref:`hal-adaptor-sec-00` 在运行时无需硬编码即可完成硬件初始化。

.. _hal-boards-sec-02:

支持的开发板
------------

.. list-table::
   :widths: 35 15 50
   :header-rows: 1

   * - 板级名称
     - 芯片
     - 描述
   * - ``esp_vocat_board_v1_0``
     - ESP32-S3
     - ESP VoCat Board V1.0 — AI 宠物伴侣开发板
   * - ``esp_vocat_board_v1_2``
     - ESP32-S3
     - ESP VoCat Board V1.2 — AI 宠物伴侣开发板
   * - ``esp_box_3``
     - ESP32-S3
     - ESP-BOX-3 开发板
   * - ``esp32_s3_korvo2_v3``
     - ESP32-S3
     - ESP32-S3-Korvo-2 V3 开发板
   * - ``esp32_p4_function_ev``
     - ESP32-P4
     - ESP32-P4-Function-EV-Board
   * - ``esp_sensair_shuttle``
     - ESP32-C5
     - ESP Sensair Shuttle 模块

.. _hal-boards-sec-03:

目录结构
--------

每块开发板在 ``boards/<板级名称>/`` 目录下包含以下文件：

.. code-block:: text

   boards/
   └── <board>/
       ├── board_info.yaml          # 开发板元信息（名称、芯片、版本、制造商等）
       ├── board_devices.yaml       # 逻辑设备配置（音频编解码、LCD 显示、触摸、存储等）
       ├── board_peripherals.yaml   # 底层外设配置（I2C/I2S/SPI 总线、GPIO、LEDC 等）
       ├── sdkconfig.defaults.board # 开发板专用 Kconfig 默认值（Flash、PSRAM 等）
       └── setup_device.c           # 板级设备工厂回调（用于需要自定义驱动初始化的场景）

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

在工程根目录执行以下命令，指定目标开发板：

.. code-block:: bash

   idf.py gen-bmgr-config -b <board>

``<board>`` 的可选值见 :ref:`hal-boards-sec-02`。若 ``brookesia_hal_boards`` 以本地路径依赖引入，需通过 ``-c`` 参数指定 ``boards/`` 目录路径：

.. code-block:: bash

   idf.py gen-bmgr-config -b <board> -c path/to/brookesia_hal_boards/boards

.. note::

   在使用了 ``idf_ext.py`` 的示例工程中，``-c`` 参数会在构建时自动注入，无需手动添加。

.. _hal-boards-sec-08:

添加自定义开发板
^^^^^^^^^^^^^^^^

在 ``boards/`` 目录（或任意自定义目录）下创建新的开发板子目录，按以下顺序添加文件：

1. **board_info.yaml**：填写开发板名称、芯片型号、版本号和描述
2. **board_peripherals.yaml**：按实际引脚和总线配置填写外设参数
3. **board_devices.yaml**：按实际板载外设填写设备类型和配置
4. **sdkconfig.defaults.board**：添加与该板相关的 Kconfig 默认值
5. **setup_device.c** （可选）：如驱动需要额外初始化步骤则实现对应工厂函数

完成后执行 ``idf.py gen-bmgr-config -b <new_board>`` 即可使用。

.. note::

   关于 ``esp_board_manager`` 配置格式的完整说明，请参考 `esp_board_manager 组件文档 <https://github.com/espressif/esp-gmf/blob/main/packages/esp_board_manager/README_CN.md>`_。
