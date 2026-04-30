.. _service-device-sec-00:

Device Control
==============

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_service_device <https://components.espressif.com/components/espressif/brookesia_service_device>`_
- Helper header: ``#include "brookesia/service_helper/device.hpp"``
- Helper class: ``esp_brookesia::service::helper::Device``

.. _service-device-sec-01:

Overview
--------

``brookesia_service_device`` is an application-facing device control service. It is not a new hardware driver. Instead, it provides a unified service interface for application code to access HAL capabilities. When the service starts, it discovers initialized HAL interfaces and exposes common control and status-query operations as Service functions and events.

Typical usage:

- Application code calls functions or subscribes to events through ``service::helper::Device``.
- HAL adaptor and board components provide low-level interfaces such as ``AudioCodecPlayerIface``, ``DisplayBacklightIface``, ``StorageFsIface``, and ``PowerBatteryIface``.
- ``brookesia_service_device`` sits in between to validate parameters, cache state, publish events, and persist selected application-level state.

Therefore, applications usually do not need to hold HAL interface pointers directly unless they need low-level or board-specific behavior.

.. _service-device-sec-02:

Features
--------

.. _service-device-sec-03:

Capability Discovery
^^^^^^^^^^^^^^^^^^^^

The service provides ``GetCapabilities`` to return a snapshot of registered HAL interfaces. Applications can query capabilities first, then decide whether to show related UI or invoke related control functions.

Common capabilities:

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - HAL Interface
     - Typical Usage
   * - ``BoardInfoIface``
     - Get static board name, chip, version, vendor, and description.
   * - ``DisplayBacklightIface``
     - Set/query backlight brightness and on/off state.
   * - ``AudioCodecPlayerIface``
     - Set/query player volume and mute state.
   * - ``StorageFsIface``
     - Get mounted file-system information.
   * - ``PowerBatteryIface``
     - Get battery information, state, and charge configuration; control charging when supported.

.. _service-device-sec-04:

Control Functions
^^^^^^^^^^^^^^^^^

Control functions update device state and are intended for UI, Agent tool calls, or remote-control workflows:

- **Display control**: set backlight brightness percentage and backlight on/off state.
- **Audio control**: set player volume percentage and mute state.
- **Power control**: set battery charge configuration or enable/disable charging when supported by HAL.
- **Data reset**: clear persisted volume, mute, and brightness state, then restore defaults.

Brightness and volume use application-level percentages in ``[0, 100]``. The service maps them to the hardware min/max ranges configured through Kconfig before calling HAL.

.. _service-device-sec-05:

Status Query Functions
^^^^^^^^^^^^^^^^^^^^^^

Status-query functions read static information or runtime state:

- **Capabilities and board information**: get the current HAL capability snapshot and static board information.
- **Display state**: get current backlight brightness percentage and on/off state.
- **Audio state**: get target player volume percentage and mute state.
- **Storage state**: get mounted file systems and mount points.
- **Battery state**: get battery capabilities, voltage, percentage, charge state, low/critical flags, and charge configuration when supported.

.. _service-device-sec-06:

Events
^^^^^^

The service publishes events when cached state changes. Applications can subscribe through ``service::helper::Device::subscribe_event``:

- ``DisplayBacklightBrightnessChanged``
- ``DisplayBacklightOnOffChanged``
- ``AudioPlayerVolumeChanged``
- ``AudioPlayerMuteChanged``
- ``PowerBatteryStateChanged``
- ``PowerBatteryChargeConfigChanged``

Battery state is polled from HAL at the configured interval, and an event is published when the snapshot changes.

.. _service-device-sec-07:

Persisted State
^^^^^^^^^^^^^^^

When ``brookesia_service_nvs`` is enabled and running, Device service attempts to save and restore the following application-level state:

- Player volume
- Player mute
- Backlight brightness

If the NVS service is unavailable, Device service still works, but these values are not persisted across reboot.

.. _service-device-sec-08:

Usage Recommendations
---------------------

- Initialize required HAL devices before starting ``ServiceManager`` and the Device service.
- Query ``GetCapabilities`` before invoking optional control functions.
- For UI and Agent tool calls, prefer Device service as the application-layer entry point to HAL instead of depending on board-specific HAL implementations directly.
- For high-throughput data paths such as audio streams or display frame drawing, use dedicated services or HAL interfaces instead of routing bulk data through Device service.

.. include-build-file:: contract_guides/service/device.inc
