.. _gui-interface-json-ui-view-screen-sec-00:

screen
====================

:link_to_translation:`en:[English]`

概览
--------------------

``screen`` 表示可挂载页面，只允许顶层使用，并通过 ``Runtime::mount_screen(...)`` 挂到某个 ``display + layer``。

相关文档
--------------------

- :doc:`index`
- :doc:`../assets/view_screen`
- :doc:`../runtime`

专属语义
--------------------

- 支持 ``mountMode``
- 默认 ``placement.width`` / ``placement.height`` 为 ``match``
- 通常作为 document 内的可显示页面根节点

专属 props
--------------------

``screen`` 当前没有专属 props 域，主要使用 ``commonProps``、``layout``、``placement`` 和 ``style``。

常见事件
--------------------

- ``scroll``
- ``gesture``
- ``focused``
- ``defocused``
