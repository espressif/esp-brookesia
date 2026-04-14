.. _service-nvs-sec-00:

NVS
===

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_service_nvs <https://components.espressif.com/components/espressif/brookesia_service_nvs>`_
- Helper header: ``#include "brookesia/service_helper/nvs.hpp"``
- Helper class: ``esp_brookesia::service::helper::NVS``

.. _service-nvs-sec-01:

Overview
--------

`brookesia_service_nvs` provides NVS (non-volatile storage) for the ESP-Brookesia ecosystem:

- **Namespaces**: Key–value storage partitioned by namespace.
- **Types**: Boolean, 32-bit signed integer, and UTF-8 string.
- **Persistence**: Data survives power cycles in the NVS partition.
- **Thread safety**: Optional `TaskScheduler`–based async execution.
- **Queries**: List keys in a namespace or read specific keys.

.. _service-nvs-sec-02:

Features
--------

.. _service-nvs-sec-03:

Namespaces
^^^^^^^^^^

- **Default**: If omitted, namespace ``"storage"`` is used.
- **Multiple**: Create several namespaces for different data.
- **Isolation**: Namespaces are independent.

.. _service-nvs-sec-04:

Supported Types
^^^^^^^^^^^^^^^

- **Bool**: ``true`` / ``false``.
- **Int**: 32-bit signed integer.
- **String**: UTF-8 string.

.. note::

   Beyond these primitives, NVS Helper templates :cpp:func:`esp_brookesia::service::helper::NVS::save_key_value` and :cpp:func:`esp_brookesia::service::helper::NVS::get_key_value` can store and retrieve arbitrary types.

.. _service-nvs-sec-05:

Core Capabilities
^^^^^^^^^^^^^^^^^

- List key metadata in a namespace.
- Set multiple key–value pairs in batch.
- Read one key or all pairs in a namespace.
- Delete a key or clear a namespace.

.. include-build-file:: contract_guides/service/nvs.inc
