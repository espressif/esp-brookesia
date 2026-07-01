.. _gui-interface-json-ui-styling-props-frame-view-sec-00:

Frameviewprops
============================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

``frameViewProps`` applies to ``frameView`` and configures how the off-screen frame buffer is registered as an output and its pixel format.

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../../view/frame_view`

Field Table
----------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Binding
     - UI Effect / Limits
   * - ``autoRegisterOutput``
     - bool
     - ``true``
     - no
     - whether to automatically register the frame buffer as a named output on creation
   * - ``outputName``
     - string
     - ``""``
     - no
     - the name used when registering the output; when empty, the backend decides the default name
   * - ``colorFormat``
     - string enum, ``RGB565`` / ``RGB888``
     - ``"RGB565"``
     - no
     - frame buffer pixel format

Example
--------------------

.. code-block:: json

   "frameViewProps": {
       "autoRegisterOutput": true,
       "outputName": "camera_preview",
       "colorFormat": "RGB888"
   }
