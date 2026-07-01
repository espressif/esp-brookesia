.. _gui-index-sec-00:

GUI Components
============================

:link_to_translation:`zh_CN:[中文]`

GUI components define the backend-neutral document model used by ESP-Brookesia applications and the LVGL backend that renders the resolved model.

.. only:: html

   .. raw:: html
      :file: ../../_static/mermaid/en/gui/index/diagram.html

.. only:: latex

   .. image:: ../../_static/mermaid/en/gui/index/diagram.png
      :width: 100%

.. rubric:: Component Roles

- ``brookesia_gui_interface`` defines JSON UI documents, runtime resources, bindings, actions, and backend contracts.
- ``brookesia_gui_lvgl`` maps resolved GUI documents to LVGL objects and provides image packaging helpers.
- System components normally combine both layers so applications can load UI resources without depending on LVGL details.

.. toctree::
   :maxdepth: 1
   :titlesonly:

   GUI Interface <interface/json_ui/index>
   LVGL Backend <lvgl/index>
