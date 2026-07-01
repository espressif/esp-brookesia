.. _gui-interface-json-ui-view-frame-view-sec-00:

Frameview
====================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

``frameView`` is an off-screen-render framebuffer control: it renders its own subtree to an independent framebuffer and can register that buffer as a named output for other consumers.

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../styling/props/frame_view`

Exclusive Props
------------------------------

- ``frameViewProps``, see details :doc:`../styling/props/frame_view`
  - ``frameViewProps.autoRegisterOutput``
  - ``frameViewProps.outputName``
  - ``frameViewProps.colorFormat``

Common Events
--------------------------

- ``clicked``
