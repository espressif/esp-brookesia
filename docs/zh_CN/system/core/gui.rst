.. _system-core-gui-sec-00:

GUI 管理
====================

:link_to_translation:`en:[English]`

system core 将 app GUI 分为应用自有 GUI 和 system-only GUI 两类。

.. _system-core-gui-sec-01:

应用自有 GUI
--------------------

每个 app 最多拥有一个由 core 托管的 GUI document。document 来源由 ``AppManifest`` 指定：原生应用由 ``IApp::get_gui_descriptor()`` 指定，可为 ``None``、``File`` 或 ``JsonString``；运行时应用固定读取 ``<runtime.resource_dir>/profile.json`` descriptor，再加载 ``root`` 指向的 JSON UI document。

``start_app()`` 会在 app ``on_start()`` 前加载 document，并按 descriptor 或 runtime root 的 ``screen_flows[]`` 自动启动对应 flow，把 ``initial`` screen 挂到指定逻辑 layer。因此 app 不暴露 ``DocumentId``、layer 或 mount target。

.. _system-core-gui-sec-02:

AppGuiRuntime
--------------------

原生应用通过 ``AppContext::gui()`` 获取 ``AppGuiRuntime``。应用自有 wrapper：

- ``set_binding_value(path, key, value)``
- ``set_binding_values(updates)``
- ``get_binding_value(path, key)``
- ``get_constant_value(path)``
- ``set_text(path, text)``
- ``set_value(path, value)``
- ``set_checked(path, checked)``
- ``create_view(template_id, parent_path, instance_id)``
- ``destroy_view(path)``
- ``subscribe_action(action)``
- ``trigger_screen_flow(screen_flow, action)``
- ``get_screen_flow_state(screen_flow)``
- ``get_view_frame(path)``
- ``set_view_src(path, src)``
- ``start_view_animation_with_id(path, animation, completed_handler)``
- ``start_view_animation_with_result(path, animation, completed_handler)``
- ``stop_animation(subscription_id)``
- ``set_view_debug_enabled(enabled)`` / ``is_view_debug_enabled()``
- ``enable_live_preview(options)`` / ``disable_live_preview()``
- ``load_theme_file(resource_dir, path)``
- ``set_theme(theme_id, reapply_loaded_documents)``
- ``get_theme()``

这些接口都隐式作用于当前 app 的 document。``set_binding_values(updates)`` 是批量 binding 写入接口，适合游戏循环或脚本 app 高频刷新多个 binding 的场景；Runtime 会按节点合并 dirty mask 后再触发 backend apply。

``get_constant_value(path)`` 读取当前 app GUI document 中 parser 合并后的 constant 值，``path`` 是 ``${constant.<path>}`` 里的点路径部分，不包含 ``${}`` 或 ``constant.`` 前缀，返回值保持原始 JSON 类型。

如果配置了启动 screen flows，core 会在 ``start_app()`` 时自动启动它们；随后 app 可通过 ``trigger_screen_flow(screen_flow, action)`` 做页面切换，并通过 ``get_screen_flow_state(screen_flow)`` 读取当前 state。运行时应用只允许 ``AppDefault`` / ``AppTop`` 逻辑 layer，``layer`` 必填，只能使用 ``Replace`` mount 语义，``z_order`` 必须在 ``0..100`` 内；可信原生应用可用 ``Stack`` 和更高 ``z_order`` 构建 overlay。

``start_view_animation_with_result()`` 在启动动画后返回 ``RuntimeAnimationStartResult``，包含 ``subscription_id``、``resolved_from``、``resolved_to``。当 animation 使用 ``from_mode = Current`` 或 ``to_mode = Relative`` 时，原生应用可用这些 resolved 值同步逻辑状态，避免额外 ``get_view_frame()`` 回读。

.. _system-core-gui-sec-03:

运行时应用 GUI service
----------------------

运行时应用通过 ``SystemGui`` service 操作自己的 GUI document，接口见 :doc:`services`。``SystemGui`` 不提供：

- view debug 开关
- live preview 开关或 poll
- ``LoadFile``
- ``LoadJson``
- ``Unload``
- doc id 参数
- layer 参数
- mount target 参数

.. _system-core-gui-sec-04:

system-only GUI
--------------------

``System`` 派生类通过 protected ``system_gui()`` 获取 ``SystemGuiAccess`` facade。它不是裸 ``gui::Runtime`` 指针，而是 system-only wrapper：所有调用仍经过 ``SystemGui`` / ``SystemGuiInput`` task group、GUI runtime gate 和 backend thread guard。``SystemGuiAccess`` 提供：

- ``load_file(resource_dir, path)``
- ``load_json(root_path, json, resource_dir)``
- ``mount_screen(document_id, path, target)``
- ``unmount_screen(document_id, path)``
- ``push_transient_screen(document_id, path, target)``
- ``pop_transient_screen(transient_id)``
- ``start_screen_flow(document_id, screen_flow, target)``
- ``trigger_screen_flow(document_id, screen_flow, action)``
- ``stop_screen_flow(document_id, screen_flow)``
- ``get_screen_flow_state(document_id, screen_flow)``
- ``unload(document_id)``
- ``set_binding_value(document_id, path, key, value)``
- ``set_binding_values(document_id, updates)``
- ``get_constant_value(document_id, path)``
- ``set_text(document_id, path, text)``
- ``set_view_src(document_id, path, src)``
- ``preload_images(document_id, image_ids)``
- ``release_images(document_id, image_ids)``
- ``start_view_animation_with_result(document_id, path, animation, completed_handler)``
- ``stop_animation(subscription_id)``
- ``subscribe_action(document_id, action, handler)``
- ``enable_live_preview(document_id, options)`` / ``disable_live_preview(document_id)``
- ``load_theme_file(resource_dir, path)``
- ``set_theme(theme_id, reapply_loaded_documents)``
- ``get_theme()``

这些接口用于派生 ``System`` 或原生系统 app 管理 shell、overlay 等系统级页面。``system_super`` 的 Shell overlay 通过 ``AppTop + Stack + z_order`` 保持置顶，并用动画收起/展开系统栏。

Transient screen 是 system-only overlay 能力：它把 screen 挂到目标 layer 前景，但不写入普通 ``mounted_screens_`` 记录，适合启动页、app launch transition 这类临时遮罩，不会替换同 layer 上已由 screen flow 管理的 Shell overlay。

.. _system-core-gui-sec-05:

Debug 与 Live Preview
---------------------

``System::Config::enable_gui_view_debug`` 用于设置 GUI runtime 初始 view debug 状态。可信原生应用可通过
``AppGuiRuntime::set_view_debug_enabled()`` 切换，作用于当前 runtime 中已加载的所有节点；system task 或
GUI runtime 不可用时返回明确错误。

``System::Config::enable_gui_live_preview`` 默认关闭。开启后，core 会在 ``load_file()`` 成功后自动为 file-backed document 调用 ``gui::Runtime::enable_live_preview()``，并由 ``SystemGui`` task 定期调用 ``poll_live_preview()``。``load_json()`` 或 JSON string document 没有可轮询的源文件，因此不会自动开启。

live preview 是开发辅助能力，只暴露给 C++ system/native app，不通过 ``SystemGui`` service 暴露给 Lua/JS 运行时应用。

.. _system-core-gui-sec-06:

Runtime Theme
--------------------

原生应用可通过 ``AppGuiRuntime`` 加载和切换 Runtime theme：

- ``load_theme_file(resource_dir, path)``：加载一个 theme descriptor 文件。
- ``set_theme(theme_id, reapply_loaded_documents)``：设置当前 Runtime theme；第二个参数控制是否立即重应用已加载 document。
- ``get_theme()``：返回当前 Runtime theme id。

这些接口是 C++ native-only 能力，不进入 ``SystemGui`` runtime service。Theme 是 GUI Runtime 全局状态，不是单个 app document 的私有状态；节点显式 ``style`` 仍优先于 theme 默认 style。
