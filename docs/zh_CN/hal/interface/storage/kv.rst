.. _hal-interface-storage-kv-sec-00:

键值存储接口
============

:link_to_translation:`en:[English]`

公共头文件： ``#include "brookesia/hal_interface/interfaces/storage/key_value.hpp"``

类名： ``KeyValueIface``

``KeyValueIface`` 定义平台无关的键值存储契约，
供 Service 层使用。
ESP-IDF 适配实现可基于 NVS，PC 或模拟器适配实现则可使用
确定性的内存后端，而无需改变 service helper 的 schema。

.. _hal-interface-storage-kv-sec-01:

API 参考
--------

.. include-build-file:: inc/key_value.inc
