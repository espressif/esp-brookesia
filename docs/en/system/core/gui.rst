.. _system-core-gui-sec-00:

GUI Management
====================

:link_to_translation:`zh_CN:[中文]`

System Core splits app GUI into app-owned GUI and system-only GUI.

.. _system-core-gui-sec-01:

App-Owned GUI
--------------------

Each app owns at most one GUI document managed by the core. The document source is specified by ``AppManifest``: a native app specifies it via ``IApp::get_gui_descriptor()``, which can be ``None``, ``File``, or ``JsonString``; a runtime app always reads the ``<runtime.resource_dir>/profile.json`` descriptor and then loads the JSON UI document referenced by ``root``.

``start_app()`` loads the document before the app ``on_start()`` and auto-starts the flow from the descriptor or runtime root ``screen_flows[]``, mounting the ``initial`` screen to the logical layer. Therefore an app never exposes ``DocumentId``, layer, or mount target.

.. _system-core-gui-sec-02:

App GUI Runtime
--------------------

A native app obtains ``AppGuiRuntime`` through ``AppContext::gui()``. The app-owned wrapper:

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

These interfaces implicitly act on the current app's document. ``set_binding_values(updates)`` is a batch binding write interface, suited to game loops or scripted apps refreshing many bindings at high frequency; the Runtime merges the dirty mask per node before triggering the backend apply.

``get_constant_value(path)`` reads the parser-merged constant value of the current app GUI document. ``path`` is the dotted path part inside ``${constant.<path>}``, without the ``${}`` or ``constant.`` prefix, and the return value keeps its original JSON type.

If startup screen flows are configured, the core auto-starts them during ``start_app()``; the app can then switch pages via ``trigger_screen_flow(screen_flow, action)`` and read the current state via ``get_screen_flow_state(screen_flow)``. A runtime app is limited to the ``AppDefault`` / ``AppTop`` logical layer, ``layer`` is required, only ``Replace`` mount semantics are allowed, and ``z_order`` must be within ``0..100``; a trusted native app can use ``Stack`` and a higher ``z_order`` to build overlays.

``start_view_animation_with_result()`` returns ``RuntimeAnimationStartResult`` after starting the animation, containing ``subscription_id``, ``resolved_from``, and ``resolved_to``. When the animation uses ``from_mode = Current`` or ``to_mode = Relative``, a native app can use these resolved values to sync logic state and avoid an extra ``get_view_frame()`` read-back.

.. _system-core-gui-sec-03:

Runtime App GUI Service
-----------------------

A runtime app operates its own GUI document through the ``SystemGui`` service; see :doc:`services` for the interface. ``SystemGui`` does not provide:

- View debug toggle
- Live preview toggle or poll
- ``LoadFile``
- ``LoadJson``
- ``Unload``
- A doc id parameter
- A layer parameter
- A mount target parameter

.. _system-core-gui-sec-04:

System-Only GUI
--------------------

A ``System`` subclass obtains the ``SystemGuiAccess`` facade through the protected ``system_gui()``. It is not a raw ``gui::Runtime`` pointer but a system-only wrapper: all calls still pass through the ``SystemGui`` / ``SystemGuiInput`` task group, the GUI runtime gate, and the backend thread guard. ``SystemGuiAccess`` provides:

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
- ``start_view_animation_with_result(document_id, path, animation, completed_handler)``
- ``stop_animation(subscription_id)``
- ``subscribe_action(document_id, action, handler)``
- ``enable_live_preview(document_id, options)`` / ``disable_live_preview(document_id)``
- ``load_theme_file(resource_dir, path)``
- ``set_theme(theme_id, reapply_loaded_documents)``
- ``get_theme()``

These interfaces let a derived ``System`` or native system app manage system-level pages such as the shell and overlay. The Shell overlay of ``system_super`` stays on top through ``AppTop + Stack + z_order`` and animates the system bar in and out.

A transient screen is a system-only overlay capability: it mounts a screen to the foreground of the target layer but does not write the regular ``mounted_screens_`` record, which suits temporary masks such as a startup page or an app launch transition and does not replace a Shell overlay already managed by a screen flow on the same layer.

.. _system-core-gui-sec-05:

Debug and Live Preview
----------------------

``System::Config::enable_gui_view_debug`` sets the initial view debug state of the GUI runtime. At runtime the C++ side can toggle it via ``System::set_gui_view_debug_enabled()`` or ``AppGuiRuntime::set_view_debug_enabled()``, affecting all loaded nodes in the current runtime.

``System::Config::enable_gui_live_preview`` is off by default. When on, the core auto-calls ``gui::Runtime::enable_live_preview()`` for a file-backed document after ``load_file()`` succeeds, and the ``SystemGui`` task periodically calls ``poll_live_preview()``. A ``load_json()`` or JSON string document has no source file to poll, so it is not auto-enabled.

Live preview is a development aid exposed only to C++ system/native apps, not to Lua/JS runtime apps through the ``SystemGui`` service.

.. _system-core-gui-sec-06:

Runtime Theme
--------------------

A native app can load and switch the Runtime theme through ``AppGuiRuntime``:

- ``load_theme_file(resource_dir, path)``: load a theme descriptor file.
- ``set_theme(theme_id, reapply_loaded_documents)``: set the current Runtime theme; the second argument controls whether to immediately reapply loaded documents.
- ``get_theme()``: return the current Runtime theme id.

These are C++ native-only capabilities and do not enter the ``SystemGui`` runtime service. A theme is global GUI Runtime state, not the private state of a single app document; an explicit node ``style`` still takes precedence over the theme default style.
