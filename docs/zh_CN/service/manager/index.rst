.. _service-manager-index-sec-00:

服务管理器
==============

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_service_manager <https://components.espressif.com/components/espressif/brookesia_service_manager>`_
- 公共头文件： ``#include "brookesia/service_manager.hpp"``

.. _service-manager-index-sec-01:

概述
----

`brookesia_service_manager` 是 ESP-Brookesia 的服务管理框架核心组件，统一提供服务生命周期管理、函数注册与事件分发、本地调用与远程 RPC 调用等能力。

.. _service-manager-index-sec-02:

特性
----

- 生命周期管理：集中式管理服务初始化、启动、停止与反初始化流程。
- 双通信模式：同时支持本地高性能调用和基于 TCP/JSON 的远程 RPC。
- 统一函数与事件模型：提供函数定义、注册、分发与事件订阅机制。
- 线程安全调度：可通过本地运行器与任务调度机制实现并发安全执行。
- 解耦集成：应用代码通过统一 API 调用服务，不依赖具体服务实现细节。

.. _service-manager-index-sec-03:

通信架构
--------

`brookesia_service_manager` 支持本地模式和远程 RPC 两种通信方式，前者适合同设备内的高频调用，后者适合跨设备或跨语言场景。

.. _service-manager-index-sec-04:

本地模式
~~~~~~~~

本地模式下，应用通过 `ServiceManager` 绑定服务后，直接通过 `ServiceBase` 访问函数和事件注册表，调用链路短、性能高，并且不需要网络通信。

.. only:: html


   .. mermaid::

      flowchart TB
          App["App/User Code"]
          SM["ServiceManager"]
          Binding["ServiceBinding"]
          Base["ServiceBase"]
          Registry["FunctionRegistry & EventRegistry"]

          App -->|"bind()"| SM
          SM -->|"returns"| Binding
          Binding -->|"get_service()"| Base
          Base -->|"call_function_sync()<br/>call_function_async()<br/>subscribe_event()"| Registry

          style App fill:#e1f5ff
          style SM fill:#fff4e1
          style Binding fill:#f0e1ff
          style Base fill:#e1ffe1
          style Registry fill:#ffe1e1

.. only:: latex

   .. image:: ../../../_static/service/manager_local_diagram.svg
      :width: 100%

.. _service-manager-index-sec-05:

远程 RPC 模式
~~~~~~~~~~~~~

远程 RPC 模式下，客户端通过 TCP Socket 和 JSON 协议访问远端服务，适合将服务能力暴露给其他设备或其他语言环境。

.. only:: html


   .. mermaid::

      flowchart LR
          subgraph Client_Side["Client Device"]
              App["App/User Code"]
              Client["RPC Client"]
              Dispatcher["EventDispatcher"]
          end

          subgraph Network["Network Layer"]
              DataLink["DataLink<br/>(TCP Socket)"]
          end

          subgraph Server_Side["Server Device"]
              Server["RPC Server"]
              Base["ServiceBase"]
              Registry["EventRegistry &<br/>FunctionRegistry"]
          end

          App --->|"call_function / <br/>subscribe_event"| Client
          Client --->|"Request"| DataLink
          DataLink --->|"Response"| Client
          DataLink <-->|"Forward"| Server
          Server <-->|"invoke"| Base
          Base <-->|"access"| Registry
          App <---|"event callback"| Dispatcher
          DataLink -.->|"Notify"| Dispatcher

          style App fill:#e1f5ff
          style Client fill:#f0e1ff
          style Dispatcher fill:#ffe1f0
          style DataLink fill:#fff4e1
          style Server fill:#f0e1ff
          style Base fill:#e1ffe1
          style Registry fill:#ffe1e1

.. only:: latex

   .. image:: ../../../_static/service/manager_rpc_diagram.svg
      :width: 100%

.. _service-manager-index-sec-06:

本地调用 vs 远程 RPC
~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :widths: 18 41 41
   :header-rows: 1

   * - 对比项
     - 本地调用（ServiceBase）
     - 远程 RPC（rpc::Client）
   * - 部署位置
     - 同一设备内
     - 跨设备通信
   * - 通信方式
     - 直接函数调用
     - TCP Socket + JSON
   * - 延迟
     - 毫秒级，较低
     - 受网络影响，通常更高
   * - 性能
     - 无序列化开销，性能更高
     - 需要序列化与反序列化
   * - 适用频率
     - 高频调用
     - 中低频调用
   * - 线程安全
     - 具备异步调度与保护机制
     - 依赖网络隔离
   * - 语言支持
     - C++
     - 语言无关
   * - 网络依赖
     - 不需要网络
     - 需要同一局域网或可达网络
   * - 典型场景
     - 设备内服务协作
     - 跨设备或跨语言服务调用

.. _service-manager-index-sec-07:

模块介绍
--------

.. _service-manager-index-sec-08:

服务运行层
~~~~~~~~~~

服务运行层负责服务生命周期管理、调用与事件执行调度、依赖绑定和与 RPC 对接，核心由 ``ServiceBase``、``ServiceManager`` 和 ``LocalTestRunner`` 构成。

:doc:`API 参考 </service/manager/service>`

.. _service-manager-index-sec-09:

函数系统
~~~~~~~~

函数系统用于定义服务可调用接口、校验调用参数并分派到具体处理函数，核心包含函数定义模型和函数注册表。

:doc:`API 参考 </service/manager/function>`

.. _service-manager-index-sec-10:

事件系统
~~~~~~~~

事件系统用于描述事件结构、校验事件数据并将事件分发到本地订阅者或远程 RPC 订阅者，核心包含事件定义、注册表和调度器。

:doc:`API 参考 </service/manager/event>`

.. _service-manager-index-sec-11:

RPC 通信
~~~~~~~~

RPC 子系统通过 TCP + JSON 对外暴露服务函数与事件能力，提供协议层、数据链路层、连接桥接层及客户端/服务端实现。

:doc:`API 参考 </service/manager/rpc>`

.. _service-manager-index-sec-12:

通用
~~~~

通用模块定义服务管理器中复用的通用类型和宏，为其他模块提供公共基础能力。

:doc:`API 参考 </service/manager/common>`

.. toctree::
   :hidden:

   服务运行层 <service>
   函数系统 <function>
   事件系统 <event>
   RPC 通信 <rpc>
   通用 <common>
