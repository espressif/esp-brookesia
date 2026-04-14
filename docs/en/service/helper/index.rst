.. _service-helper-index-sec-00:

Service Helper
==============

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_service_helper <https://components.espressif.com/components/espressif/brookesia_service_helper>`_
- Public header: ``#include "brookesia/service_helper.hpp"``

.. _service-helper-index-sec-01:

Overview
--------

`brookesia_service_helper` is the unified application-facing helper layer for Brookesia services: typed wrappers and schema entry points.

.. _service-helper-index-sec-02:

Features
--------

- **Single entry**: Helpers encapsulate each service to simplify integration.
- **Schema-friendly**: Works with function/event schemas for standardized calls and docs.
- **Decoupling**: Apps target helpers instead of concrete providers.
- **Extensible**: New services can add helpers and documentation over time.

.. _service-helper-index-sec-03:

Module Overview
###############

.. toctree::
   :maxdepth: 1

   Base <base>
   Audio <audio>
   Emote <emote>
   NVS <nvs>
   SNTP <sntp>
   Video <video>
   Wi-Fi <wifi>
