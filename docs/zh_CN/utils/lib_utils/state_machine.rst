.. _utils-lib_utils-state_machine-sec-00:

状态机
============

:link_to_translation:`en:[English]`

公共头文件： ``#include "brookesia/lib_utils/state_machine.hpp"``

.. _utils-lib_utils-state_machine-sec-01:

概述
----

`state_machine` 提供状态机实现，支持状态注册、动作触发、转换回调与运行时控制，
适合管理复杂流程与多状态切换场景。

.. _utils-lib_utils-state_machine-sec-02:

特性
----

- 支持添加状态与状态转换规则
- 支持指定初始状态与强制切换
- 支持转换完成回调与运行状态查询
- 可与 `TaskScheduler` 联动进行异步调度

.. _utils-lib_utils-state_machine-sec-03:

API 参考
------------

.. include-build-file:: inc/utils/brookesia_lib_utils/include/brookesia/lib_utils/state_machine.inc
