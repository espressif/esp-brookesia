.. _service-sntp-sec-00:

SNTP
====

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_service_sntp <https://components.espressif.com/components/espressif/brookesia_service_sntp>`_
- Helper header: ``#include "brookesia/service_helper/sntp.hpp"``
- Helper class: ``esp_brookesia::service::helper::SNTP``

.. _service-sntp-sec-01:

Overview
--------

`brookesia_service_sntp` provides SNTP (Simple Network Time Protocol)
for the ESP-Brookesia ecosystem:

- **NTP servers**: Multiple servers; picks an available one.
- **Time zone**: System time zone with offset applied.
- **Auto sync**: Starts when the network is up.
- **Status**: Query sync state, server list, and zone.
- **Persistence**: Optional `brookesia_service_nvs` for servers and zone.

.. _service-sntp-sec-02:

Features
--------

.. _service-sntp-sec-03:

NTP Servers
^^^^^^^^^^^

- Default: ``"pool.ntp.org"``.
- Multiple servers with automatic selection.
- Query the configured list.

.. _service-sntp-sec-04:

Time Zone
^^^^^^^^^

- Default: CST-8 (China, UTC+8).
- Standard zone strings (``UTC``, ``CST-8``, ``EST-5``, …).
- Applied to system time when set.

.. _service-sntp-sec-05:

Core Operations
^^^^^^^^^^^^^^^

- **Set NTP servers**: Configure one or more NTP servers.
- **Set time zone**: Configure the system time zone.
- **Start / stop service**: Start or stop SNTP and time synchronization.
- **Get server list**: Read the configured NTP server list.
- **Get time zone**: Read the configured time zone.
- **Check sync status**: Check whether system time is synchronized with NTP.
- **Reset data**: Reset all configuration to defaults.

.. _service-sntp-sec-06:

Auto Management
^^^^^^^^^^^^^^^

- Load from NVS on start.
- Save after changes.
- Detect network and start sync when available.
- Monitor sync status and flags.

.. include-build-file:: contract_guides/service/sntp.inc
