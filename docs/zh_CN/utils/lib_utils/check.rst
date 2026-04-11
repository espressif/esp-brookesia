.. _utils-lib_utils-check-sec-00:

错误检查
============

:link_to_translation:`en:[English]`

公共头文件： ``#include "brookesia/lib_utils/check.hpp"``

.. _utils-lib_utils-check-sec-01:

概述
----

`check` 提供一组统一的检查宏，用于在参数非法、表达式失败、异常抛出、
错误码异常或数值越界时执行统一处理逻辑。

.. _utils-lib_utils-check-sec-02:

特性
----

- 覆盖常见检查场景：``nullptr``、布尔条件、`std::exception`、`esp_err_t`、范围检查
- 支持多种失败处理模式：执行代码块、`return`、`exit`、`goto`
- 与日志系统联动，可输出失败原因与上下文信息
- 可通过配置宏切换失败处理策略（无动作、错误日志、断言）

.. _utils-lib_utils-check-sec-03:

API 参考
------------

.. include-build-file:: inc/check.inc
