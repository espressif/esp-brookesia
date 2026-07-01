.. _gui-interface-json-ui-view-screen-sec-00:

screen
====================

:link_to_translation:`en:[English]`

.. _gui-interface-json_ui-view-screen-sec-01:

概览
--------------------

``screen`` 表示可挂载页面，只允许顶层使用，并通过 ``Runtime::mount_screen(...)`` 挂到某个 ``display + layer``。

.. _gui-interface-json_ui-view-screen-sec-02:

相关文档
--------------------

- :doc:`index`
- :doc:`../assets/view_screen`
- :doc:`../runtime`

.. _gui-interface-json_ui-view-screen-sec-03:

专属语义
--------------------

- 支持 ``mountMode``
- 默认 ``placement.width`` / ``placement.height`` 为 ``match``
- 通常作为 document 内的可显示页面根节点

.. _gui-interface-json_ui-view-screen-sec-04:

专属 props
--------------------

``screen`` 当前没有专属 props 域，主要使用 ``commonProps``、``layout``、``placement`` 和 ``style``。

.. _gui-interface-json_ui-view-screen-sec-05:

常见事件
--------------------

- ``scroll``
- ``gesture``
- ``focused``
- ``defocused``
