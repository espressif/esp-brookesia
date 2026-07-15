.. _gui-lvgl-index-sec-00:

LVGL Backend
==============================================

:link_to_translation:`zh_CN:[中文]`

``brookesia_gui_lvgl`` is the LVGL backend implementation of ``brookesia_gui_interface``. It maps the resolved JSON UI document model to LVGL objects and provides build-time image packaging.

.. _gui-lvgl-index-sec-01:

Scope
--------------------

- Project integration of the LVGL backend
- Build-time packaging of PNG/JPEG into LVGL image ``.bin``
- Runtime loading, metadata completion, preloading, and caching of ``.bin``, PNG, and JPEG images

For the JSON UI protocol itself, see :doc:`../interface/json_ui/index`.

.. _gui-lvgl-index-sec-02:

Documents in This Group
-----------------------

.. list-table::
   :header-rows: 1

   * - Document
     - Content
     - When to use
   * - :doc:`backend`
     - Mapping from the resolved JSON UI model to the LVGL API, image loading and caching, backend pump
     - Troubleshooting LVGL backend behavior
   * - :doc:`image_pack`
     - ``brookesia_gui_lvgl_pack_images()``, LVGLImage.py parsing, Python dependencies
     - Packaging PNG/JPEG resources into package / SPIFFS

.. toctree::
   :maxdepth: 1
   :titlesonly:

   backend
   image_pack

.. note::

   Maintainer sync check: when editing ``cmake/image_pack.cmake``, sync :doc:`image_pack`; when changing image
   loading, preloading, caching, or JSON UI field mapping, sync :doc:`backend`.
