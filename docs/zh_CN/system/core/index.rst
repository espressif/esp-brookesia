.. _system-core-sec-00:

System Core
======================

:link_to_translation:`en:[English]`

``brookesia_system_core`` 提供 ESP-Brookesia 产品系统所需的通用核心能力，统一编排原生应用、运行时应用、应用自有 GUI 文档、存储、定时器、服务访问和权限边界。

.. _system-core-index-sec-01:

概述
--------------------

System Core 是 Brookesia 系统框架的编排层，把 GUI Runtime、Runtime Manager、Service Manager 和 TaskScheduler 串联起来，对外提供稳定的应用接入模型。

它统一处理两类应用：原生应用通过 C++ 继承 ``IApp`` 接入，运行时应用通过解包后的 package 与脚本约定函数接入。两类应用都只操作自身的 GUI 文档，不直接接触 ``DocumentId``、图层或挂载目标。

.. _system-core-index-sec-02:

核心能力
--------------------

- 原生应用安装、启动、停止和生命周期回调。
- 运行时应用 package 扫描、加载和 ``brookesia_app.*`` 约定函数调用。
- 应用自有 GUI 文档的加载、挂载、事件订阅和清理。
- ``SystemCore``、``SystemGui``、``SystemTimer`` 三个基础服务。
- 原生应用 GUI image/font 资源的自动注册与反注册。
- 基于 ``AppKind`` 的基础权限分层。

.. _system-core-index-sec-03:

主线模型
--------------------

普通应用不直接接触 ``gui::DocumentId``、图层或挂载目标，只操作自身 GUI 文档，普通 screen 固定挂载到 ``AppDefault``。派生 ``System`` 和系统内置原生应用可通过 system-only API 使用更完整的系统能力，例如管理 shell、状态栏和 overlay。

.. _system-core-index-sec-04:

文档导航
--------------------

- :doc:`architecture`：整体运行方式、初始化流程和源码模块。
- :doc:`configuration`：构建配置、宏与运行时存储布局。
- :doc:`app_model`：应用类型、原生与运行时差异及生命周期。
- :doc:`gui`：应用自有 GUI 与 system-only GUI。
- :doc:`resources_permissions`：资源注册与权限边界。
- :doc:`services`：系统服务总览与调用边界。
- :doc:`app_package`：运行时应用包格式与扫描规则。
- :doc:`examples`：最小接入示例。
- :doc:`troubleshooting`：常见问题排查。
- :doc:`api`：公共 API 参考。

.. toctree::
   :maxdepth: 1
   :titlesonly:
   :hidden:

   架构 <architecture>
   配置 <configuration>
   应用模型 <app_model>
   GUI 管理 <gui>
   资源与权限 <resources_permissions>
   系统服务 <services>
   应用包 <app_package>
   示例 <examples>
   故障排查 <troubleshooting>
   API 参考 <api>
