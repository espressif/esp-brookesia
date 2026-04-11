.. _utils-lib_utils-plugin-sec-00:

插件系统
============

:link_to_translation:`en:[English]`

公共头文件： ``#include "brookesia/lib_utils/plugin.hpp"``

.. _utils-lib_utils-plugin-sec-01:

概述
----

`plugin` 提供通用插件注册表与实例管理机制，用于按名称注册、发现与创建插件实例。

.. _utils-lib_utils-plugin-sec-02:

特性
----

- 模板化插件注册与查询，按名称获取实例
- 支持延迟实例化与单例插件注册
- 提供插件枚举、释放与清理接口
- 支持宏生成注册符号，保证链接可见性

.. _utils-lib_utils-plugin-sec-03:

API 参考
------------

.. include-build-file:: inc/plugin.inc
