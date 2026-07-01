.. _gui-interface-json-ui-view-screen-sec-00:

Screen
====================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

``screen`` represents a mountable page. It is only allowed at the top level and is mounted onto a ``display + layer`` via ``Runtime::mount_screen(...)``.

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../assets/view_screen`
- :doc:`../runtime`

Exclusive Semantics
--------------------------------------

- Supports ``mountMode``
- ``placement.width`` / ``placement.height`` default to ``match``
- Usually the root node of a displayable page in the document

Exclusive Props
------------------------------

``screen`` has no dedicated props; it mainly uses ``commonProps``, ``layout``, ``placement``, and ``style``.

Common Events
--------------------------

- ``scroll``
- ``gesture``
- ``focused``
- ``defocused``
