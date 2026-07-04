.. _service-http-sec-00:

HTTP Service
============

:link_to_translation:`zh_CN:[中文]`

- Helper header: ``#include "brookesia/service_helper/network/http.hpp"``
- Helper class: ``esp_brookesia::service::helper::Http``

.. rubric:: Overview

``brookesia_service_http`` provides HTTP and HTTPS client requests as Brookesia service functions and events.

.. rubric:: Core Responsibilities

- Wraps ESP-IDF HTTP client behavior behind service schemas.
- Supports request submission, cancellation, memory responses, file downloads, progress, and completion events.
- Used by apps and agents for package indexes, downloads, cloud configuration, and network resources.

.. rubric:: Integration Position

This component is an independent ESP-Brookesia release component. It can be integrated through ESP-IDF component dependencies and combined with peer framework components as needed.

.. include-build-file:: contract_guides/service/http.inc
