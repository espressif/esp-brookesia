.. _utils-lib_utils-state_base-sec-00:

状态基类
============

:link_to_translation:`en:[English]`

公共头文件： ``#include "brookesia/lib_utils/state_base.hpp"``

.. _utils-lib_utils-state_base-sec-01:

概述
----

`state_base` 定义状态机的基础状态抽象，提供进入、退出、更新等生命周期回调，
并支持状态超时与更新间隔配置。

.. _utils-lib_utils-state_base-sec-02:

特性
----

- 标准化状态生命周期回调：`on_enter` / `on_exit` / `on_update`
- 支持超时动作与更新间隔设置
- 便于派生自定义状态类，统一状态行为管理

.. _utils-lib_utils-state_base-sec-03:

API 参考
------------

.. include-build-file:: inc/state_base.inc
