.. _hal-interface-index-sec-00:

HAL Interface
=============

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_hal_interface <https://components.espressif.com/components/espressif/brookesia_hal_interface>`_
- Public header: ``#include "brookesia/hal_interface.hpp"``

.. _hal-interface-index-sec-01:

Overview
--------

``brookesia_hal_interface`` is the hardware abstraction foundation of ESP-Brookesia, providing a unified abstraction between board-level implementations and upper-level subsystems. Its main capabilities include:

- **Device and interface model**: devices aggregate hardware units; interfaces express reusable capabilities (codec playback/recording, display panel/touch/backlight, storage volumes, etc.), with clear separation of concerns
- **Plugin-based registration**: device and interface implementations are registered in a registry and resolved by name at runtime, avoiding hard-coded implementation types in application code
- **Probing and lifecycle**: each device is first probed for availability before initialisation; batch and per-name single-device init/deinit are supported symmetrically
- **Global discovery**: devices can be resolved by plugin name or device logical name; interfaces can be enumerated globally by type or retrieved by name from within a device
- **Built-in HAL declarations**: abstract definitions for common audio, display, and storage interfaces; concrete behaviour is provided by the adaptor layer

.. _hal-interface-index-sec-02:

Features
--------

.. _hal-interface-index-sec-03-dup2:

Devices and Interfaces
^^^^^^^^^^^^^^^^^^^^^^

**A device** represents an independently managed hardware unit (e.g. an audio codec path, a display subsystem, a storage subsystem). Before a device operates it must be **probed**: only when the hardware is available in the current environment does it proceed to initialisation. During initialisation the device collects the **interface instances** it will expose.

**An interface** represents a stable capability boundary (e.g. codec playback, recording, panel rendering, touch sampling, backlight control, storage medium and filesystem discovery). Multiple instances of the same abstract type can coexist, distinguished by the name used at registration time; that name uniquely identifies one instance in the global interface registry.

Device and interface implementation classes are registered through the project's plugin mechanism; the framework creates or retrieves instances by name at runtime, and upper layers depend only on the abstract types and conventions defined in this component.

.. _hal-interface-index-sec-04-dup2:

Registration and Lifecycle
^^^^^^^^^^^^^^^^^^^^^^^^^^

During batch initialisation, the framework iterates the registry: for each device it executes probe and device-side init in order; on success the framework simultaneously registers that device's declared interfaces into the global interface table. A probe or init failure for one device typically only affects that device; the rest continue to be processed.

Per **plugin registration name** initialisation and deinitialisation are also supported. During deinit the device's interfaces are first removed from the global table, then any device-defined cleanup runs, and finally the device's internal references to interfaces are released.

Typical flow:

#. Registered devices enter the **probe** phase;
#. On success, the device is **initialised** and its interfaces are registered into the global registry; on failure, that **device is skipped**.

.. _hal-interface-index-sec-05-dup2:

Discovery and Naming
^^^^^^^^^^^^^^^^^^^^

Two distinct name types are associated with devices:

.. list-table::
   :header-rows: 1
   :widths: 18 82

   * - Name type
     - Meaning
   * - Plugin name
     - Key in the plugin registry, used to look up a device instance directly by registration entry
   * - Device name
     - The logical name carried by the device object itself; may match or differ from the plugin name

Either path can be used at runtime to resolve the corresponding device.

Interface access is oriented around the **global interface registry**: all currently convertible instances of a given interface type can be listed, or the first matching instance of a type can be retrieved (returned as a name–instance pair). If starting from a device object, a specific interface can also be retrieved from that device's published interface set using the full registration name.

When multiple devices co-exist, it is recommended to add a device-distinguishing prefix or other namespace to interface registration names to avoid global conflicts.

.. _hal-interface-index-sec-06-dup2:

Built-In Capability Scope
^^^^^^^^^^^^^^^^^^^^^^^^^

The headers provide abstract definitions of common HAL interfaces, covering audio codec playback and recording, display panel and touch and backlight, and storage filesystem discovery. They describe static information, capability parameters, and virtual interface contracts; register operations, bus configuration, and timing are handled by the board-level adaptor or other components.

The following interface headers can be included at once via ``brookesia/hal_interface/interfaces.hpp``, or together with the device base class via the aggregation entry ``brookesia/hal_interface.hpp``:

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Header
     - Primary types
   * - ``audio/codec_player.hpp``
     - ``AudioCodecPlayerIface``
   * - ``audio/codec_recorder.hpp``
     - ``AudioCodecRecorderIface``
   * - ``display/backlight.hpp``
     - ``DisplayBacklightIface``
   * - ``display/panel.hpp``
     - ``DisplayPanelIface``
   * - ``display/touch.hpp``
     - ``DisplayTouchIface``
   * - ``storage/fs.hpp``
     - ``StorageFsIface``

.. _hal-interface-index-sec-03:

Device Interface Classes
~~~~~~~~~~~~~~~~~~~~~~~~

The component provides the following interface classes:

- ``AudioCodecPlayerIface``
- ``AudioCodecRecorderIface``
- ``DisplayBacklightIface``
- ``DisplayPanelIface``
- ``DisplayTouchIface``
- ``StorageFsIface``

.. _hal-interface-index-sec-04:

API Reference
-------------

.. _hal-interface-index-sec-05:

Device & Interface Base
^^^^^^^^^^^^^^^^^^^^^^^

.. toctree::
   :maxdepth: 1

   Device Base <device>
   Interface Base <interface>

.. _hal-interface-index-sec-06:

Audio Interface Classes
^^^^^^^^^^^^^^^^^^^^^^^

.. toctree::
   :maxdepth: 1

   Codec Player <audio/codec_player>
   Codec Recorder <audio/codec_recorder>

.. _hal-interface-index-sec-11:

Display Interface Classes
^^^^^^^^^^^^^^^^^^^^^^^^^

.. toctree::
   :maxdepth: 1

   Panel <display/panel>
   Touch <display/touch>
   Backlight <display/backlight>

.. _hal-interface-index-sec-12:

Storage Interface Classes
^^^^^^^^^^^^^^^^^^^^^^^^^

.. toctree::
   :maxdepth: 1

   Filesystem <storage/fs>
