.. _gui-interface-json-ui-view-frame-view-sec-00:

Frameview
====================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-view-frame_view-sec-01:

Overview
--------------------

``frameView`` is an off-screen-render framebuffer control: it renders its own subtree to an independent framebuffer and can register that buffer as a named output for other consumers.

.. _gui-interface-json_ui-view-frame_view-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../styling/props/frame_view`

.. _gui-interface-json_ui-view-frame_view-sec-03:

Exclusive Props
------------------------------

- ``frameViewProps``, see details :doc:`../styling/props/frame_view`
  - ``frameViewProps.autoRegisterOutput``
  - ``frameViewProps.outputName``
  - ``frameViewProps.colorFormat``

.. _gui-interface-json_ui-view-frame_view-sec-04:

Common Events
--------------------------

- ``clicked``
