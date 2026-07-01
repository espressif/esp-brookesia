.. _gui-lvgl-image-pack-sec-00:

LVGL Image Packaging
========================================

:link_to_translation:`zh_CN:[中文]`

``brookesia_gui_lvgl`` provides a CMake helper that converts the app's PNG/JPG/JPEG images into LVGL v9 ``.bin`` and generates an ``index.json``-form ``imageSet`` descriptor in the output directory.

.. _gui-lvgl-image_pack-sec-01:

Cmake Commands
----------------------------

.. code-block:: cmake

   brookesia_gui_lvgl_pack_images(
       INPUT_DIR <image_dir>
       OUTPUT_DIR <image_asset_dir>
       [CF <LVGLImage.py --cf value>]
       [COMPRESS <NONE|RLE|LZ4>]
       [ALIGN <n>]
       [BACKGROUND <hex>]
       [PREMULTIPLY]
       [RGB565_DITHER]
   )

Behavior:

- recursively scans ``.png`` / ``.PNG`` / ``.jpg`` / ``.JPG`` / ``.jpeg`` / ``.JPEG`` under ``INPUT_DIR``.
- JPG/JPEG is first converted with Pillow into a temporary PNG in the build directory and then handed to LVGLImage.py; the output image id still uses the original file-name stem.
- uses the file-name stem as the image id.
- generates ``<id>.bin`` for each image.
- the output directory's ``index.json`` uses ``type: "imageSet"``, where each ``images[]`` entry's ``src`` points to ``<id>.bin`` in the same directory, and ``width`` / ``height`` are written from the ``.bin`` header.
- if several image files share the same stem, configuration fails.
- a ``.bin`` generated with ``CF RGB565`` can be loaded directly by the LVGL backend, suitable for large alpha-free background images to reduce image size.
- icons or sprites that need a transparent channel should still use ``CF ARGB8888``.

.. _gui-lvgl-image_pack-sec-02:

LVGL Image Python Source
--------------------------------------

The helper does not keep a local copy of ``LVGLImage.py``; it resolves the script from the LVGL component the current project depends on:

- ESP-IDF: looks up ``lvgl`` first in the build components, then ``lvgl__lvgl``, and uses ``${COMPONENT_DIR}/scripts/LVGLImage.py``.
- PC: reads ``scripts/LVGLImage.py`` from the ``SOURCE_DIR`` of the ``lvgl`` target.
- special projects can override the script path explicitly through the cache variable ``BROOKESIA_GUI_LVGL_IMAGE_TOOL``.

.. _gui-lvgl-image_pack-sec-03:

Python Environment
------------------------------------

The helper needs a Python that can import ``png``, ``lz4.block``, and ``PIL.Image``:

- ``BROOKESIA_GUI_LVGL_IMAGE_TOOL_PYTHON`` can specify the Python to use.
- by default it tries candidates such as the project Python, ``BROOKESIA_ROOT_DIR/.venv-docs/bin/python``, and ``BROOKESIA_ROOT_DIR/.venv/bin/python``.
- ``BROOKESIA_GUI_LVGL_IMAGE_TOOL_AUTO_INSTALL_PIP_DEPS`` defaults to ``ON``; if a candidate Python lacks the dependencies, a build-local venv is created at ``${CMAKE_BINARY_DIR}/brookesia_gui_lvgl_image_tool_venv`` and ``pypng``, ``lz4``, ``Pillow`` are installed.

.. _gui-lvgl-image_pack-sec-04:

Runtime Behavior
--------------------------------

The generated ``index.json`` can be used as a JSON UI ``imageSet`` asset:

.. code-block:: json

   {
       "type": "imageSet",
       "images": [
           {
               "id": "bird",
               "src": "bird.bin",
               "width": 57,
               "height": 41
           }
       ]
   }

The LVGL backend preloads ``.bin`` files during document load and caches them by path. Later ``imageProps.src`` bindings, ``set_view_src(...)``, or incremental props apply only reuse the preloaded descriptor and do not read files on the dynamic refresh path.
The backend reads the color format, size, and stride from the ``.bin`` header; therefore LVGL v9 bin formats supported by LVGLImage.py such as ``ARGB8888``, ``RGB565``
need no extra runtime conversion.
