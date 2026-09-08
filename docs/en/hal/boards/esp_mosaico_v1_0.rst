.. _hal-boards-esp-mosaico-v1-0-sec-00:

ESP-Mosaico V1.0
=================

:link_to_translation:`zh_CN:[中文]`

Overview
--------

The ``esp_mosaico_v1_0`` configuration supports the ESP-Mosaico V1.0 core board and its optional expansion-module slots. Core-board support includes system and network facilities, Wi-Fi, the 480 x 480 display and touch panel, audio playback and recording, battery voltage, internal LittleFS/NVS, and external NAND FATFS.

Expansion modules are discovered separately from core-board devices. A missing or unsupported module does not prevent the core board or System Super from starting.

Expansion Module Configuration
------------------------------

Expansion-module support is controlled by ``CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES`` under ``Component config > ESP-Brookesia: Hal Adaptor Configurations > Expansion Modules``. The generic default is disabled because many boards do not provide expansion slots; the ESP-Mosaico board default enables it.

An existing project's ``sdkconfig`` takes precedence over board defaults. It can keep Expansion, Video, Camera, DVP, or camera sensor support disabled after the board package is updated. Before camera validation, prefer a clean default ``sdkconfig`` and confirm these options in ``menuconfig``. Board Manager 0.5.x does not automatically inject ``board_manager.defaults`` into a non-default ``SDKCONFIG`` path; such builds must explicitly include ``components/gen_bmgr_codes/board_manager.defaults`` in ``SDKCONFIG_DEFAULTS``.

Use the local ESP-IDF environment and generate the board configuration before building:

.. code-block:: console

   idf.py --preview gen-bmgr-config -b esp_mosaico_v1_0
   idf.py --preview set-target esp32s31
   idf.py --preview menuconfig
   idf.py --preview build

Detection and Lifecycle
-----------------------

When expansion support is enabled, the Mosaico provider reads the identification EEPROM in each safe-to-scan slot every 300 ms. A result must remain unchanged for three consecutive scans, so an insertion, removal, or identity change is normally published after about 0.9 seconds.

The public ``hal::expansion::ModuleManagerIface`` is registered as ``Expansion:ModuleManager:0``. ``get_module_infos()`` returns the stable slot snapshots, while ``add_event_listener()`` and ``remove_event_listener()`` manage change notifications. The Device Service exposes the same information through ``GetExpansionModuleInfos`` and publishes ``ExpansionModuleChanged`` events.

Each slot reports one of the following states:

.. list-table::
   :widths: 24 76
   :header-rows: 1

   * - State
     - Meaning
   * - ``Unknown``
     - The slot has not produced a stable scan result yet.
   * - ``Empty``
     - No module EEPROM responds in the slot.
   * - ``Invalid``
     - An EEPROM responds, but its descriptor is blank, malformed, or has an invalid CRC. The provider does not guess the module type.
   * - ``Unsupported``
     - The descriptor is valid, but the module type or selected slot has no supported driver.
   * - ``Ready``
     - A supported module is present and available to be opened.
   * - ``Active``
     - A client has claimed the module and its hardware resources are initialized.
   * - ``Error``
     - A runtime error occurred while scanning, claiming, or releasing hardware.

Scanning identifies modules but does not initialize their functional hardware. A module is claimed and initialized only when a matching HAL consumer opens it, and its resources are returned when the last client closes it.

Supported Expansion Modules
---------------------------

.. list-table::
   :widths: 24 14 20 42
   :header-rows: 1

   * - Module
     - Slot
     - Result
     - Current Support
   * - Camera module
     - Left
     - ``Ready`` / ``Active``
     - Camera HAL and Video capture. The sensor is detected at runtime: OV3640 delivers native RGB565 at 640 x 480 and 7 fps, SC101IOT delivers YUV422 at 640 x 480 and 15 fps.
   * - Camera module
     - Right
     - ``Unsupported``
     - The camera driver is not initialized because the right slot does not provide the required mapping.
   * - Other valid module descriptors
     - Either
     - ``Unsupported``
     - Detection and reporting only; additional module drivers will be added separately.

Hot-Plug Behavior and Limitations
---------------------------------

- With the camera inserted but unopened, both slots continue to be scanned.
- Opening the camera pauses scanning of both slots because a camera data signal shares the Mosaico EEPROM address-selection pin.
- Closing the camera restores the shared pin and triggers an immediate scan; normal periodic scanning then resumes.
- Removing the camera while it is streaming is not supported. Close the camera before unplugging it.
- Full-screen 480 x 480 crop, rotation, scaling, and a System Super camera-preview experience are not part of the current support.

Implementation Boundary
-----------------------

The generic adaptor layer owns the optional expansion capability, stable module states, query and event contracts, driver-provider registration, and claim lifetime. It has no knowledge of Mosaico EEPROM addresses, slot pins, or camera sensor setup.

The Mosaico board implementation owns its two-slot EEPROM discovery, descriptor validation, resource arbitration, slot compatibility, and the camera slot mapping. The camera then uses the existing generic Camera HAL and Video path through a claimed module lease.

The Mosaico implementation is adapted from the Apache-2.0-licensed `ESP-Mosaico BSP <https://github.com/esp-mosaico/esp-mosaico-bsp>`_ at commit ``bef99672e411101489ed19c40527cca1c1dd5bb1``. Required code is kept locally, so building this board does not depend on or download that repository.
