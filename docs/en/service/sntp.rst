.. _service-sntp-sec-00:

SNTP
====

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_service_sntp <https://components.espressif.com/components/espressif/brookesia_service_sntp>`_
- Helper header: ``#include "brookesia/service_helper/network/sntp.hpp"``
- Helper class: ``esp_brookesia::service::helper::SNTP``

.. _service-sntp-sec-01:

Overview
--------

`brookesia_service_sntp` provides SNTP (Simple Network Time Protocol)
for the ESP-Brookesia ecosystem:

- **NTP servers**: Multiple servers; picks an available one.
- **Time zone**: System time zone with offset applied.
- **Auto sync**: Starts the state machine when the service starts, waits for network availability, then syncs.
- **Status**: Query ``CheckingNetwork``, ``Syncing``, ``Synced`` or ``Stopped`` state, server list, and zone.
- **Persistence**: Uses `brookesia_service_storage` in the ``SNTP`` namespace for servers and zone.

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
- **Get state**: Read the synchronization state.
- **Check sync status**: Check whether system time is synchronized with NTP.
- **Reset data**: Reset all configuration to defaults.

.. _service-sntp-sec-06:

Auto Management
^^^^^^^^^^^^^^^

- Load from the Storage service on start.
- Save after changes in the ``SNTP`` namespace.
- Enter ``CheckingNetwork`` first, then move to ``Syncing`` when ``ProtocolSntpIface::is_network_ready()`` returns true.
- Publish ``StateChanged`` when synchronization state changes.

.. include-build-file:: contract_guides/service/sntp.inc
