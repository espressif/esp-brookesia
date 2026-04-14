.. _hal-index-sec-00:

硬件抽象组件
============

:link_to_translation:`en:[English]`

ESP-Brookesia HAL 框架由三个组件分层协作，共同完成从开发板硬件到上层业务的抽象：

.. only:: html

   .. mermaid::

      flowchart TD
          A["上层业务 / 服务 / 示例"]
          B["**brookesia_hal_interface**<br/>· 设备 / 接口抽象基类<br/>· 音频、显示、存储等 HAL 接口定义<br/>· 全局接口注册表"]
          C["**brookesia_hal_adaptor**<br/>· AudioDevice / DisplayDevice / StorageDevice 板级实现<br/>· 通过 esp_board_manager 初始化外设<br/>· 将接口实例注册到全局表"]
          D["**brookesia_hal_boards**<br/>· 各开发板的外设拓扑（引脚、总线）<br/>· 逻辑设备配置（音频编解码、LCD、触摸等）<br/>· 板级 Kconfig 默认值与驱动工厂回调"]

          A -->|"按接口名称发现 & 调用"| B
          B -->|"实现抽象接口"| C
          C -->|"提供 YAML 配置"| D

- ``brookesia_hal_interface``： **定义抽象接口**，上层业务只依赖此层，与具体硬件解耦
- ``brookesia_hal_adaptor``： **实现抽象接口**，通过 ``esp_board_manager`` 读取板级配置并初始化真实外设
- ``brookesia_hal_boards``： **提供板级 YAML 配置**，描述各开发板的外设拓扑、引脚与驱动参数

.. only:: latex

   .. image:: ../../_static/hal/index_diagram_cn.png
      :width: 100%

.. note::

   自定义开发板可通过以下两种方式接入 ESP-Brookesia HAL 框架：

   - **方式一（推荐）**：在 ``brookesia_hal_boards`` 的 ``boards/<厂商>/`` 目录下，按照 ``esp_board_manager`` 规范新建开发板子目录并补充配置文件，无需修改适配层代码。详见 :ref:`添加自定义开发板 <hal-boards-sec-08>`。
   - **方式二（完全自定义）**：移除对 ``brookesia_hal_adaptor`` 和 ``brookesia_hal_boards`` 的依赖，直接基于 ``brookesia_hal_interface`` 的抽象接口编写板级初始化代码。适用于无法使用 ``esp_board_manager`` 的场景，但需自行维护与接口规范的兼容性。

.. toctree::
   :maxdepth: 1

   HAL 接口定义 <interface/index>
   HAL 接口适配 <adaptor>
   HAL 开发板支持 <boards/index>
