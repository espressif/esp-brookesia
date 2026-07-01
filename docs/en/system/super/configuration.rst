.. _system-super-configuration-sec-00:

Configuration
====================

:link_to_translation:`zh_CN:[中文]`

This page collects the macros, Kconfig options, and resource paths used by the current ``brookesia_system_super`` implementation. Super resources live in the ``system/super`` directory of the System Core internal storage and do not define a separate resource root.

Kconfig
--------------------

ESP platform configuration lives in ``system/brookesia_system_super/Kconfig``.

.. list-table::
   :header-rows: 1

   * - Option
     - Default
     - Description
   * - ``BROOKESIA_SYSTEM_SUPER_ENABLE_DEBUG_LOG``
     - ``n``
     - Enable Super debug log
   * - ``BROOKESIA_SYSTEM_SUPER_ENABLE_STARTUP_OVERLAY``
     - ``y``
     - Show the lightweight SystemTop startup overlay at boot
   * - ``BROOKESIA_SYSTEM_SUPER_STARTUP_ROOT_JSON``
     - ``startup/root.json``
     - The startup overlay root, relative to ``<system-root>/super``
   * - ``BROOKESIA_SYSTEM_SUPER_ENABLE_RESOURCE_FONT_COPY``
     - ``y``
     - Copy the built-in ``resource/fonts`` to ``<system-root>/fonts``
   * - ``BROOKESIA_SYSTEM_SUPER_ENABLE_APP_LAUNCH_TRANSITION``
     - ``n``
     - Compatibility item; the current Shell overlay inlines app launch
   * - ``BROOKESIA_SYSTEM_SUPER_APP_LAUNCH_ROOT_JSON``
     - ``app_launch/root.json``
     - Compatibility item; currently unused
   * - ``BROOKESIA_SYSTEM_SUPER_ENABLE_APP_LAUNCH_ANIMATION``
     - ``y``
     - Play a mask/icon scale-and-move animation after tapping a launcher app icon
   * - ``BROOKESIA_SYSTEM_SUPER_APP_LAUNCH_POST_COMPLETE_HOLD_MS``
     - ``0``
     - Extra hold time after the Shell app launch animation completion callback

Host Platform Config
--------------------

PC platform configuration lives in ``system/brookesia_system_super/cmake/pc_platform.cmake``.

.. list-table::
   :header-rows: 1

   * - CMake Variable
     - Default
     - Description
   * - ``BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_DEBUG_LOG``
     - ``OFF``
     - PC debug log default
   * - ``BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_STARTUP_OVERLAY``
     - ``ON``
     - PC startup overlay default switch
   * - ``BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_RESOURCE_FONT_COPY``
     - ``ON``
     - PC built-in font resource staging default switch
   * - ``BROOKESIA_SYSTEM_SUPER_PC_CONFIG_STARTUP_ROOT_JSON``
     - ``startup/root.json``
     - PC startup overlay root default
   * - ``BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_APP_LAUNCH_TRANSITION``
     - ``OFF``
     - Compatibility item; the current Shell overlay inlines app launch
   * - ``BROOKESIA_SYSTEM_SUPER_PC_CONFIG_APP_LAUNCH_ROOT_JSON``
     - ``app_launch/root.json``
     - Compatibility item; currently unused
   * - ``BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_APP_LAUNCH_ANIMATION``
     - ``ON``
     - PC launch mask/icon animation default switch
   * - ``BROOKESIA_SYSTEM_SUPER_PC_CONFIG_APP_LAUNCH_POST_COMPLETE_HOLD_MS``
     - ``0``
     - Extra hold time after the PC launch animation completes

GUI Backend
--------------------

The Super runtime only accesses the GUI backend through the ``brookesia_system_core`` and ``gui_interface`` abstractions. The caller must pass a backend instance in ``core_config.gui_backend``. Build-time image packing uses ``brookesia_gui_lvgl_pack_images()`` to convert the images under ``assets/shell/images`` into LVGL ``.bin + imageSet index.json``; this capability only generates staged resources and does not depend on a specific backend in C++ runtime code.

Macro Configs
--------------------

The public macros live in ``include/brookesia/system_super/macro_configs.h``.

.. list-table::
   :header-rows: 1

   * - Macro
     - Current Value
     - Description
   * - ``BROOKESIA_SYSTEM_SUPER_LOG_TAG``
     - ``"SysSuper"``
     - Log tag
   * - ``BROOKESIA_SYSTEM_SUPER_SYSTEM_TYPE``
     - ``"super"``
     - Super system type
   * - ``BROOKESIA_SYSTEM_SUPER_RESOURCE_DIR``
     - ``"/littlefs/system/super"``
     - Super compile-time default resource root
   * - ``BROOKESIA_SYSTEM_SUPER_SHELL_RESOURCE_DIR``
     - ``.../shell``
     - Shell resource directory compatibility macro
   * - ``BROOKESIA_SYSTEM_SUPER_ENABLE_DEBUG_LOG``
     - Kconfig/PC config
     - Debug log switch
   * - ``BROOKESIA_SYSTEM_SUPER_ENABLE_STARTUP_OVERLAY``
     - Kconfig/PC config
     - Startup overlay switch
   * - ``BROOKESIA_SYSTEM_SUPER_ENABLE_RESOURCE_FONT_COPY``
     - Kconfig/PC config
     - Built-in font resource staging switch
   * - ``BROOKESIA_SYSTEM_SUPER_ENABLE_APP_LAUNCH_ANIMATION``
     - Kconfig/PC config
     - Launch mask/icon animation switch
   * - ``BROOKESIA_SYSTEM_SUPER_APP_LAUNCH_POST_COMPLETE_HOLD_MS``
     - Kconfig/PC config
     - Extra hold time after the launch animation completion callback

Resource Paths
--------------------

ESP platform default resource root, font root, and app package root:

.. code-block:: text

   /littlefs/system/super
   /littlefs/system/fonts
   /littlefs/apps

PC platform defaults:

.. code-block:: text

   ${CMAKE_BINARY_DIR}/brookesia/system/super
   ${CMAKE_BINARY_DIR}/brookesia/system/fonts
   ${CMAKE_BINARY_DIR}/brookesia/apps

The first level of the ``littlefs`` staging root should contain only ``app/`` and ``system/``; shared fonts belong to ``system/fonts``. With built-in font resource staging off, ``brookesia_system_super`` neither deletes nor copies ``system/fonts``; the product project can provide its own ``system/fonts/index.json``, and if it is missing, Super skips font preregistration during init and continues running. The generated image resources live in ``<system-root>/super/shell/images``, and the Shell Runtime themes live in ``<system-root>/super/themes``. The product project must pack the ``littlefs`` directory into firmware through LittleFS image rules.
