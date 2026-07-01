.. _gui-interface-json-ui-interaction-index-sec-00:

交互
======================================

:link_to_translation:`en:[English]`

概览
--------------------

本分组负责 view 的交互与动态行为：``bindings`` 处理 store 数据绑定与订阅，``events`` 处理交互事件与 action 路由，``animations`` 处理声明式动画，``screenFlow`` 处理同一 document 内一组 screen 的状态机切换。

相关文档
--------------------

- :doc:`../index`
- :doc:`../view/index`
- :doc:`../runtime`

本组文档
--------------------

.. list-table::
   :header-rows: 1

   * - 文档
     - 负责内容
   * - :doc:`bindings`
     - ``bindings``、store 路径作用域、subscribe
   * - :doc:`events`
     - ``events``、payload、action 路由
   * - :doc:`animations`
     - 基础声明式动画（mount/show/hide 等）
   * - :doc:`screen_flow`
     - ``screenFlow``、state、transition 与 Runtime screen flow API

.. toctree::
   :maxdepth: 1

   bindings
   events
   animations
   screen_flow
