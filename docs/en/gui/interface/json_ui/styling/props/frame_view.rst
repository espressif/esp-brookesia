.. _gui-interface-json-ui-styling-props-frame-view-sec-00:

Frameviewprops
============================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-styling-props-frame_view-sec-01:

Overview
--------------------

``frameViewProps`` applies to ``frameView`` and configures how the off-screen frame buffer is registered as an output and its pixel format.

.. _gui-interface-json_ui-styling-props-frame_view-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../../view/frame_view`

.. _gui-interface-json_ui-styling-props-frame_view-sec-03:

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

.. _gui-interface-json_ui-styling-props-frame_view-sec-04:

Example
--------------------

.. code-block:: json

   "frameViewProps": {
       "autoRegisterOutput": true,
       "outputName": "camera_preview",
       "colorFormat": "RGB888"
   }
