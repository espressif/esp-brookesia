.. _service-storage-sec-00:

Storage
*******

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_service_storage <https://components.espressif.com/components/espressif/brookesia_service_storage>`_
- 辅助头文件： ``#include "brookesia/service_helper/system/storage.hpp"``
- 辅助类： ``esp_brookesia::service::helper::Storage``

.. _service-storage-sec-01:

概述
----

`brookesia_service_storage` 是为 ESP-Brookesia 生态系统提供的命名空间键值存储服务，提供：

- **命名空间管理**：基于命名空间的键值对存储，支持多个独立的存储空间。
- **数据类型支持**：支持 `布尔值`、`整数`、`字符串` 三种基本数据类型。
- **持久化存储**：数据通过底层存储后端持久化，断电后不丢失。
- **线程安全**：可选基于 `TaskScheduler` 实现异步任务调度，保证线程安全。
- **灵活查询**：支持列出命名空间中的所有键值对，或按需获取指定键的值。

.. _service-storage-sec-02:

功能特性
--------

.. _service-storage-sec-03:

命名空间管理
^^^^^^^^^^^^

- **默认命名空间**：如果不指定命名空间，将使用默认命名空间 ``"storage"``。
- **多命名空间**：可以创建多个命名空间来组织不同类型的数据。
- **命名空间隔离**：不同命名空间的数据相互独立，互不影响。

.. _service-storage-sec-04:

支持的数据类型
^^^^^^^^^^^^^^

- **布尔值（Bool）**：``true`` 或 ``false``。
- **整数（Int）**：32 位有符号整数。
- **字符串（String）**：UTF-8 编码的字符串。

.. note::

   除了上述三种基本数据类型外，Storage Helper 提供的 :cpp:func:`esp_brookesia::service::helper::Storage::save_key_value` 和 :cpp:func:`esp_brookesia::service::helper::Storage::get_key_value` 模板助手函数还支持存储和获取任意类型的数据。

.. _service-storage-sec-05:

核心功能
^^^^^^^^

- **列出键信息**：列出命名空间中所有键的信息（包括键名、类型等）。
- **设置键值对**：支持批量设置多个键值对。
- **获取键值对**：支持获取指定键的值，或获取命名空间中的所有键值对。
- **删除键值对**：支持删除指定键，或清空整个命名空间。

.. include-build-file:: contract_guides/service/storage.inc
