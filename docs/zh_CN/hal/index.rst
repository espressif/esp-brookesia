.. _hal-index-sec-00:

硬件组件
===========

:link_to_translation:`en:[English]`

ESP-Brookesia HAL 框架由三个组件分层协作，共同完成从开发板硬件到上层业务的抽象：

.. only:: html

   .. raw:: html
      :file: ../../_static/mermaid/zh_CN/hal/index/diagram.html

.. only:: latex

   .. image:: ../../_static/mermaid/zh_CN/hal/index/diagram.png
      :width: 100%

- ``brookesia_hal_interface``：**定义抽象接口**；上层代码只依赖这一层，从而与具体硬件解耦。
- ``brookesia_hal_adaptor``：**实现抽象接口**，通过 ``esp_board_manager`` 读取开发板配置并初始化真实外设。
- ``brookesia_hal_boards``：**提供板级 YAML 配置**，描述外设拓扑、引脚分配和各开发板的驱动参数。

.. note::

   自定义开发板可通过以下两种方式接入 ESP-Brookesia HAL 框架：

   - **方式一（推荐）**：在 ``brookesia_hal_boards`` 的 ``boards/<厂商>/`` 目录下，按照 ``esp_board_manager`` 规范新建开发板子目录并补充配置文件，无需修改适配层代码。详见 :ref:`添加自定义开发板 <hal-boards-sec-08>`。
   - **方式二（完全自定义）**：移除对 ``brookesia_hal_adaptor`` 和 ``brookesia_hal_boards`` 的依赖，直接基于 ``brookesia_hal_interface`` 的抽象接口编写板级初始化代码。适用于无法使用 ``esp_board_manager`` 的场景，但需自行维护与接口规范的兼容性。

.. toctree::
   :maxdepth: 1

   接口抽象 <interface/index>
   ESP 设备板级适配 <adaptor>
   ESP 设备板级配置 <boards/index>
   PC 仿真支持 <pc_simulation>
