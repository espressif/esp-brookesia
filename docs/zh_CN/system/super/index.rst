.. _system-super-sec-00:

System Super
========================

:link_to_translation:`en:[English]`

``brookesia_system_super`` 是构建在 System Core 之上的标准系统，面向具有矩形触摸屏的手持设备，提供默认 shell、launcher、overlay、状态栏、应用切换和内置资源流程。

.. _system-super-index-sec-01:

概述
--------------------

System Super 派生自 ``core::System``，通过 core 的生命周期扩展点安装内置 Shell app、挂载 overlay，并在普通应用停止后恢复 Shell。它不重新实现应用生命周期或运行时应用包加载，而是复用 System Core 的能力，专注于系统壳的页面、导航和资源。

.. note::

   Super 的命名源自 `Brookesia Superciliaris <https://en.wikipedia.org/wiki/Brookesia_superciliaris>`_ 的简化形式，它是 Brookesia 属的代表种，常被作为该属的经典基准形象，适合表达这套系统是 ESP-Brookesia 在矩形触摸设备上的标准、完整形态。

.. _system-super-index-sec-02:

与 System Core 的关系
---------------------

- 产品工程提供 GUI backend 和设备服务，传入 ``core_config.gui_backend``。
- Super 管理 Shell 与 overlay 资源，应用生命周期、GUI 文档加载和运行时应用包扫描仍由 System Core 负责。
- 定制产品应把用户应用 screen 保持在普通 app 层，把系统 UI 保持在 Shell 管理的 layer 中。

.. _system-super-index-sec-03:

主要能力
--------------------

- 内置 ``ShellApp``：Home Dashboard、App Launcher、Notifications 三个页面。
- 系统 overlay：状态栏、底部上滑退出应用的 gesture indicator，以及 keyboard、message dialog、app modal 等浮层。
- 桌面背景与应用背景的 background flow。
- light/dark Runtime theme 和按需 system keyboard。
- 开机 startup overlay 和 app launch transition 动画。

.. _system-super-index-sec-04:

文档导航
--------------------

- :doc:`architecture`：派生方式与扩展点。
- :doc:`shell`：Shell app、页面、launcher 和 theme。
- :doc:`overlay`：状态栏、gesture indicator 和系统浮层。
- :doc:`app_flow`：启动 Shell、打开和退出应用的流程。
- :doc:`assets`：Shell 资源结构、screen、flow 和模板。
- :doc:`configuration`：宏、Kconfig 和资源路径。
- :doc:`extension`：定制 Shell、overlay 和资源的边界。
- :doc:`troubleshooting`：常见问题排查。
- :doc:`api`：公共 API 参考。

.. toctree::
   :maxdepth: 1
   :titlesonly:
   :hidden:

   架构 <architecture>
   Shell <shell>
   Overlay <overlay>
   应用流程 <app_flow>
   资源 <assets>
   配置 <configuration>
   扩展 <extension>
   故障排查 <troubleshooting>
   API 参考 <api>
