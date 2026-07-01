.. _system-super-extension-sec-00:

扩展
====================

:link_to_translation:`en:[English]`

本文说明当前实现下定制 system super 的边界。

.. _system-super-extension-sec-01:

替换资源目录
--------------------

最小定制方式是替换资源目录，例如 shell 资源目录 ``BROOKESIA_SYSTEM_SUPER_SHELL_RESOURCE_DIR``。资源仍需保留源码依赖的 screen path、template id 和 action 字符串，或同步修改 ``src/private/system_constants.hpp`` 中对应常量。

.. _system-super-extension-sec-02:

扩展 launcher
--------------------

当前 launcher 逻辑写在内置 ``ShellApp`` 中：app 来源是 ``owner_.list_apps()``，只显示 ``manifest.visible = true`` 的 app，button 由 ``launcher_app_button`` 模板动态创建，点击 action 为 ``super.launch.app``。如果只是调整视觉样式，优先改 ``resource/shell/constants`` 和 ``resource/shell/screens``；如果要调整筛选、排序、分组或图标逻辑，需要修改 ``ShellApp::populate_launcher()``。

.. _system-super-extension-sec-03:

调整 overlay
--------------------

overlay 是 Shell root 内的 ``/overlay`` screen，由 Shell manifest 的 ``overlay`` flow 挂到 ``AppTop + Stack + z_order 101``。如果要增加系统级按钮或手势，可以扩展 ``resource/shell/screens/overlay.json`` 和 ``ShellApp::mount_overlay()`` 的 action 订阅。普通 app 不应该通过 runtime service 修改 overlay。

.. _system-super-extension-sec-04:

派生 System
--------------------

如需产品级系统壳，可以继续派生 ``super::System`` 或直接派生 ``core::System``：

- 仅想保留 Super 的 Shell 和 Overlay：派生 ``super::System``。
- 想完全替换 shell、navigation 或 app flow：派生 ``core::System`` 更直接。

无论哪种方式，应用自有 GUI 和运行时应用权限仍建议遵循 System Core 文档中的规则。
