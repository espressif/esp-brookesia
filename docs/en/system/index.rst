.. _system-index-sec-00:

System Components
==================================

:link_to_translation:`zh_CN:[中文]`

System components provide the product-level framework above services, GUI, runtimes, HAL, and built-in apps.

.. only:: html

   .. raw:: html
      :file: ../../_static/mermaid/en/system/index/diagram.html

.. only:: latex

   .. image:: ../../_static/mermaid/en/system/index/diagram.png
      :width: 100%

.. rubric:: Component Roles

- ``brookesia_system_core`` provides common core capabilities for app lifecycle, runtime apps, GUI access, storage, timers, services, and permissions.
- ``brookesia_system_super`` extends System Core into a standard system for handheld devices with rectangular touch screens.

.. toctree::
   :maxdepth: 1

   System Core <core/index>
   System Super <super/index>
