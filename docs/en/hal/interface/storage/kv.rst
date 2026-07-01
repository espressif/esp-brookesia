.. _hal-interface-storage-kv-sec-00:

Key-Value Storage Interface
===========================

:link_to_translation:`zh_CN:[中文]`

Public header: ``#include "brookesia/hal_interface/interfaces/storage/key_value.hpp"``

Class: ``KeyValueIface``

``KeyValueIface`` defines a platform-neutral key-value storage contract for service
layers. ESP-IDF adaptors can back it with NVS, while PC or simulator adaptors can
provide deterministic in-memory implementations without changing service helper
schemas.

.. _hal-interface-storage-kv-sec-01:

API Reference
-------------

.. include-build-file:: inc/key_value.inc
