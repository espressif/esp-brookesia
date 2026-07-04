.. _service-manager-index-sec-00:

服务管理器
==============

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_service_manager <https://components.espressif.com/components/espressif/brookesia_service_manager>`_
- 公共头文件： ``#include "brookesia/service_manager.hpp"``

.. _service-manager-index-sec-01:

概述
----

`brookesia_service_manager` 是 ESP-Brookesia 的服务管理框架核心组件，统一提供服务生命周期管理、函数注册与事件分发、本地调用和事件订阅等能力。

.. _service-manager-index-sec-02:

特性
----

- 生命周期管理：集中式管理服务初始化、启动、停止与反初始化流程。
- 本地调用：在进程内直接调用服务函数，不依赖网络传输。
- 统一函数与事件模型：提供函数定义、注册、分发与事件订阅机制。
- 线程安全调度：可通过本地运行器与任务调度机制实现并发安全执行。
- 解耦集成：应用代码通过统一 API 调用服务，不依赖具体服务实现细节。

.. _service-manager-index-sec-03:

通信架构
--------

应用通过 `ServiceManager` 绑定服务后，直接通过 `ServiceBase` 和服务 helper 访问函数与事件注册表，调用链路短、性能高，并且不需要网络通信。

.. only:: html

   .. raw:: html
      :file: ../../../../_static/mermaid/zh_CN/service/manager/index/diagram_local.html

.. only:: latex

   .. image:: ../../../../_static/mermaid/zh_CN/service/manager/index/diagram_local.png
      :width: 100%

.. _service-manager-index-sec-04:

模块介绍
--------

.. _service-manager-index-sec-05:

服务运行层
~~~~~~~~~~

服务运行层负责服务生命周期管理、调用与事件执行调度和依赖绑定，核心由 ``ServiceBase``、``ServiceManager`` 和 ``LocalTestRunner`` 构成。

:doc:`API 参考 </service/framework/manager/service>`

.. _service-manager-index-sec-06:

函数系统
~~~~~~~~

函数系统用于定义服务可调用接口、校验调用参数并分派到具体处理函数，核心包含函数定义模型和函数注册表。

:doc:`API 参考 </service/framework/manager/function>`

.. _service-manager-index-sec-07:

事件系统
~~~~~~~~

事件系统用于描述事件结构、校验事件数据并将事件分发到本地订阅者。

:doc:`API 参考 </service/framework/manager/event>`

.. _service-manager-index-sec-08:

通用
~~~~

通用模块定义服务管理器中复用的通用类型和宏，为其他模块提供公共基础能力。

:doc:`API 参考 </service/framework/manager/common>`

.. toctree::
   :hidden:

   服务运行层 <service>
   函数系统 <function>
   事件系统 <event>
   通用 <common>
