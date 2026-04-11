.. _utils-lib_utils-function_guard-sec-00:

函数守卫
============

:link_to_translation:`en:[English]`

公共头文件： ``#include "brookesia/lib_utils/function_guard.hpp"``

.. _utils-lib_utils-function_guard-sec-01:

概述
----

`FunctionGuard` 提供 RAII 风格的函数守卫，用于在作用域结束时自动执行清理逻辑，
降低资源释放与流程回滚的复杂度。

.. _utils-lib_utils-function_guard-sec-02:

特性
----

- 作用域退出自动执行回调，可显式 `release` 取消
- 支持移动语义，便于在函数间转移守卫
- 执行回调时捕获 `std::exception`，避免异常传播破坏清理流程

.. _utils-lib_utils-function_guard-sec-03:

API 参考
------------

.. include-build-file:: inc/function_guard.inc
