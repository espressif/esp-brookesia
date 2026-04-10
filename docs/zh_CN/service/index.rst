.. _service-index-sec-00:

服务组件
============

:link_to_translation:`en:[English]`

本分类包含 ESP-Brookesia 服务框架组件的说明内容。ESP-Brookesia 服务框架由服务框架层和通用服务层组成，各组件的层级关系如下：

.. only:: html

   .. mermaid::

      flowchart TD
          App["App / 用户代码"]
          Helper["**brookesia_service_helper**<br/>· CRTP 类型安全辅助基类<br/>· 函数 / 事件 Schema 定义<br/>· call_function / subscribe_event"]
          Manager["**brookesia_service_manager**<br/>· 服务插件生命周期管理<br/>· 本地线程安全调用 & TCP RPC 远程调用<br/>· 函数注册表 & 事件注册表"]
          Services["**通用服务（基于 service_helper 实现）**<br/>NVS · SNTP · Wi-Fi · 音频 · 视频 · 自定义"]

          App -->|"调用函数 / 订阅事件"| Helper
          Helper -->|"构建在"| Manager
          Services -->|"注册为插件"| Manager

.. only:: latex

   .. image:: ../../_static/service/index_diagram_cn.png
      :width: 100%

- ``brookesia_service_manager``：服务框架核心，负责服务插件注册、本地/RPC 两种通信模式下的函数路由与事件分发
- ``brookesia_service_helper``：基于 CRTP 的类型安全辅助层，简化服务的函数/事件定义与调用方式
- **通用服务**：基于 ``service_helper`` 实现的具体业务服务，注册到 ``service_manager`` 后可被上层按名称发现和调用

.. _service-index-sec-01:

服务框架
--------

.. toctree::
   :maxdepth: 1

   服务管理器 <manager/index>
   服务辅助 <helper/index>

.. _service-index-sec-02:

通用服务
--------

.. toctree::
   :maxdepth: 1

   NVS <nvs>
   SNTP <sntp>
   Wi-Fi <wifi>
   音频 <audio>
   视频 <video>
   自定义服务 <custom>

.. _service-index-sec-03:

开发指南
--------

.. toctree::
   :maxdepth: 1

   应用开发指南 <usage>
