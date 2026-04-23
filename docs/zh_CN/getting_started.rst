.. _getting-started-guide:

快速入门
==================

:link_to_translation:`en:[English]`

本文档介绍如何获取和使用 ESP-Brookesia 组件，以及如何编译和运行示例工程。

.. _getting-started-versioning:

ESP-Brookesia 版本说明
--------------------------

ESP-Brookesia 自 ``v0.7`` 版本起采用组件化管理，建议项目通过组件注册表获取所需组件，约定如下：

1. 各组件独立迭代，但均具有相同的 ``major.minor`` 版本号，并且依赖相同的 ESP-IDF 版本。
2. ``release`` 分支仅维护历史大版本，``master`` 分支持续集成新特性。

不同版本说明如下，组件列表与更新说明请参考：

.. list-table:: ESP-Brookesia 版本支持情况
   :header-rows: 1
   :widths: 20 20 30 20

   * - ESP-Brookesia
     - 依赖的 ESP-IDF
     - 主要变更
     - 支持状态
   * - master (v0.7)
     - >= v5.5, < 6.0
     - 支持组件管理器
     - 新功能开发分支
   * - release/v0.6
     - >= v5.3, <= 5.5
     - 预览支持系统框架，提供 ESP-VoCat 固件工程
     - 停止维护

.. _getting-started-dev-environment:

开发环境搭建
--------------------------

ESP-IDF 是乐鑫为 ESP 系列芯片提供的物联网开发框架：

- ESP-IDF 包含一系列库及头文件，提供了基于 ESP SoC 构建软件项目所需的核心组件；
- ESP-IDF 还提供了开发和量产过程中最常用的工具及功能，例如：构建、烧录、调试和测量等。

.. note::

    - 请参考：`ESP-IDF 编程指南 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/index.html>`__ 完成 ESP-IDF 开发环境的搭建。
    - 不推荐使用 VSCode 扩展插件安装 ESP-IDF 环境，可能导致部分依赖 `esp_board_manager` 组件的示例编译失败。

.. _getting-started-hardware:

硬件准备
--------------------------

ESP 系列 SoC 支持以下功能：

- Wi-Fi（2.4 GHz/5 GHz 双频）
- 蓝牙 5.x（BLE/Mesh）
- 高性能多核处理器，主频最高可达 400 MHz
- 超低功耗协处理器和深度睡眠模式
- 丰富的外设接口：
    - 通用接口：GPIO、UART、I2C、I2S、SPI、SDIO、USB OTG 等
    - 专用接口：LCD、摄像头、以太网、CAN、Touch、LED PWM、温度传感器等
- 大容量内存：
    - 内部 RAM 最大可达 768 KB
    - 支持外部 PSRAM 扩展
    - 支持外部 Flash 存储
- 增强的安全特性：
    - 硬件加密引擎
    - 安全启动
    - Flash 加密
    - 数字签名

ESP 系列 SoC 采用先进工艺制程，提供业界领先的射频性能、低功耗特性和稳定可靠性，适用于物联网、工业控制、智能家居、可穿戴设备等多种应用场景。

.. note::

   - 各系列芯片的具体规格和功能请参考 `ESP 产品选型工具 <https://products.espressif.com/>`__。
   - 参考 `ESP-Brookesia HAL 开发板支持 <../hal/boards/index>`__ 快速开始
   - ESP-Brookesia 对内存和 Flash 占用较高，建议选用 Flash ≥ 8 MB、PSRAM ≥ 4 MB 的芯片或开发板。


.. _getting-started-component-usage:

如何获取和使用组件
--------------------------

推荐通过 `ESP Component Registry <https://components.espressif.com/>`__ 获取 ESP-Brookesia 组件。

以 ``brookesia_service_wifi`` 组件为例，添加依赖的步骤如下：

1. **使用命令行**

   在工程目录下运行以下命令：

   .. code-block:: bash

      idf.py add-dependency "espressif/brookesia_service_wifi"

2. **修改配置文件**

   在工程目录下创建或修改 *idf_component.yml* 文件：

   .. code-block:: yaml

      dependencies:
         espressif/brookesia_service_wifi: "*"

更多组件管理器的使用方法请参考 `Espressif IDF Component Manager Docs <https://docs.espressif.com/projects/idf-component-manager/en/latest/>`__。

.. _getting-started-example-projects:

如何使用示例工程
--------------------------

ESP-Brookesia 提供了多个示例工程。部分示例支持在线烧录，可通过 `ESP Launchpad <https://espressif.github.io/esp-brookesia/index.html>`__ 在浏览器中直接烧录预编译固件并查看串口输出，无需预先搭建本地开发环境。

.. only:: html

   .. raw:: html

      <div style="text-align: center;">
         <a href="https://espressif.github.io/esp-brookesia/index.html">
            <img alt="Try it with ESP Launchpad" src="https://dl.espressif.com/AE/esp-dev-kits/new_launchpad.png" width="400">
            <p>点击图片在线体验 ESP Launchpad</p>
         </a>
      </div>
      <br>

示例工程的典型编译和烧录步骤如下：

1. 选择目标芯片或开发板。具体选择取决于示例所依赖的外设，可分为以下两种情况：

   - **选择目标芯片**：

      以 ``examples/service/wifi`` 为例，该工程仅依赖芯片自身的 Wi-Fi 外设能力，因此只需选择目标芯片（如 ``esp32s3``）：

      .. code-block:: bash

         idf.py set-target <target>

   - **选择目标开发板**：

      以 ``examples/service/console`` 为例，该工程依赖开发板上的音频外设，因此需要先选择目标开发板（如 ``esp_vocat_board_v1_2``）：

      .. code-block:: bash

         idf.py gen-bmgr-config -b <board>
         idf.py set-target <target>

      .. note::

         该类工程目录中包含 ``idf_ext.py`` 脚本。与普通依赖 ``esp_board_manager`` 的示例工程相比，它额外封装了以下能力：

         - 无需手动设置 ``IDF_EXTRA_ACTIONS_PATH`` 环境变量。
         - 选择开发板时，会根据 ``idf_component.yml`` 中声明的依赖自动拉取 ``esp_board_manager`` 和 ``brookesia_hal_boards`` 组件。
         - 执行 ``idf.py gen-bmgr-config`` 命令时，会自动补充 ``-c`` 参数来指定 ``boards/`` 目录，并按以下优先级依次查找：工程目录下的 ``boards/`` 目录；然后是 ``brookesia_hal_boards`` 组件中的 ``boards/`` 目录。
         - 如果需要，也可以手动通过 ``-c`` 参数指定自定义目录。

      .. note::

         请参考 :ref:`HAL 开发板支持 <hal-boards-sec-02>` 获取所有支持的开发板列表。

2. 配置项目（可选）：

    .. code-block:: bash

        idf.py menuconfig

3. 编译并烧录到开发板：

    .. code-block:: bash

        idf.py build
        idf.py -p <PORT> flash

4. 通过串口监视输出：

    .. code-block:: bash

        idf.py -p <PORT> monitor

更多示例请见 `examples/ <https://github.com/espressif/esp-brookesia/tree/master/examples>`__ 目录，具体使用方法可参考各示例目录下的 README 文件。
