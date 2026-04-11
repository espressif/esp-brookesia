.. _utils-lib_utils-describe_helpers-sec-00:

描述辅助
============

:link_to_translation:`en:[English]`

公共头文件： ``#include "brookesia/lib_utils/describe_helpers.hpp"``

.. _utils-lib_utils-describe_helpers-sec-01:

概述
----

`describe_helpers` 基于 Boost.Describe 和 Boost.JSON 提供“对象描述 + 序列化/反序列化”
能力，降低结构体、枚举与复杂类型的配置编解码成本。

.. _utils-lib_utils-describe_helpers-sec-02:

特性
----

- 支持枚举与结构体成员反射（描述名称、成员列表、枚举值等）
- 支持常见类型 JSON 序列化/反序列化：字符串、数值、布尔、容器、可选值等
- 覆盖 `variant`、`optional`、`map`、`vector` 等复合类型检测与处理
- 提供调试友好的描述输出能力，便于日志与配置排查

.. _utils-lib_utils-describe_helpers-sec-03:

API 参考
------------

.. include-build-file:: inc/describe_helpers.inc
