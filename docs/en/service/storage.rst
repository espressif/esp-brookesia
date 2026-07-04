.. _service-storage-sec-00:

Storage
*******

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_service_storage <https://components.espressif.com/components/espressif/brookesia_service_storage>`_
- Helper header: ``#include "brookesia/service_helper/system/storage.hpp"``
- Helper class: ``esp_brookesia::service::helper::Storage``

.. _service-storage-sec-01:

Overview
--------

`brookesia_service_storage` provides namespace-based key-value storage for the ESP-Brookesia ecosystem:

- **Namespaces**: Key–value storage partitioned by namespace.
- **Types**: Boolean, 32-bit signed integer, and UTF-8 string.
- **Persistence**: Data survives power cycles through the underlying storage backend.
- **Thread safety**: Optional `TaskScheduler`–based async execution.
- **Queries**: List keys in a namespace or read specific keys.

.. _service-storage-sec-02:

Features
--------

.. _service-storage-sec-03:

Namespaces
^^^^^^^^^^

- **Default**: If omitted, namespace ``"storage"`` is used.
- **Multiple**: Create several namespaces for different data.
- **Isolation**: Namespaces are independent.

.. _service-storage-sec-04:

Supported Types
^^^^^^^^^^^^^^^

- **Bool**: ``true`` / ``false``.
- **Int**: 32-bit signed integer.
- **String**: UTF-8 string.

.. note::

   Beyond these primitives, Storage Helper templates :cpp:func:`esp_brookesia::service::helper::Storage::save_key_value` and :cpp:func:`esp_brookesia::service::helper::Storage::get_key_value` can store and retrieve arbitrary types.

.. _service-storage-sec-05:

Core Capabilities
^^^^^^^^^^^^^^^^^

- List key metadata in a namespace.
- Set multiple key–value pairs in batch.
- Read one key or all pairs in a namespace.
- Delete a key or clear a namespace.

.. include-build-file:: contract_guides/service/storage.inc
