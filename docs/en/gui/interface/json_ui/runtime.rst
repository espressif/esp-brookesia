.. _gui-interface-json-ui-runtime-sec-00:

Runtime
==============================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-runtime-sec-01:

Overview
--------------------

This page covers:

- the runtime resource model
- the ``document_id + absolute_path`` API mapping
- font/image resource queries
- template instances

This page does not cover JSON field tables; for field details, see the per-module pages.

.. _gui-interface-json_ui-runtime-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`assets/index`
- :doc:`view/index`
- :doc:`styling/placement`
- :doc:`interaction/screen_flow`

.. _gui-interface-json_ui-runtime-sec-03:

Runtime Resource Model
--------------------------------------------

The parser ultimately resolves resources into a runtime ``Document``:

- a top-level ``screen`` -> ``Document.screens``
- a top-level non-``screen`` ``view`` asset -> ``Document.templates``

The Runtime also maintains three kinds of non-document global resources:

- global ``font``
- global ``image``
- global ``theme``

Then ``Runtime`` handles:

- document registration
- eager screen pre-creation
- dynamic screen lazy creation
- template instantiation
- path indexing
- events and bindings

Current bindings rules:

- a ``bindings`` key uses a public ``camelCase`` dotted path
- a ``bindings`` value is the local key in the current node path scope, without a prefix
- the runtime actually locates a binding by ``document_id + absolute_path + local_key``
- prefer reading/writing via ``Runtime::set_binding_value(document_id, absolute_path, key, value)``, ``Runtime::set_binding_values(document_id, updates)``, or ``View::set_binding_value(key, value)``

.. _gui-interface-json_ui-runtime-sec-04:

Current Public API Mapping of Protocols
------------------------------------------------------------------------------

.. _gui-interface-json_ui-runtime-sec-05:

Document Related
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``load(root_path, document, environment)``: load an already-parsed ``Document`` object
- ``load_json(root_path, json, base_dir, environment)``: load from a JSON string
- ``load_file(file_path, environment)``
- ``unload(document_id)``
- ``update(document_id, file_path, environment)``
- ``get_constant_value(document_id, path)``: read a value from the document's merged constant tree
- ``load_theme(...)``
- ``load_theme_json(...)``
- ``load_theme_file(...)``
- ``list_supported_themes()``
- ``set_theme(theme_id)``
- ``set_theme(theme_id, reapply_loaded_documents)``
- ``get_theme()``
- ``register_font(...)``
- ``register_font_json(...)``
- ``register_font_file(...)``
- ``list_supported_fonts(language = "")``
- ``list_supported_languages()``
- ``set_language(language)``
- ``set_language(language, reapply_loaded_documents)``
- ``get_language()``
- ``set_default_font_for_language(language, font_id)``
- ``get_default_font_for_language(language)``
- ``register_image(...)``
- ``unregister_image(...)``
- ``register_image_json(...)``
- ``register_image_file(...)``
- ``list_font_resources(document_id)``
- ``list_image_resources(document_id)``
- ``set_view_debug_enabled(enabled)``
- ``is_view_debug_enabled()``
- ``reapply_styles(document_id)``
- ``enable_live_preview(document_id, options = {})``
- ``disable_live_preview(document_id)``
- ``poll_live_preview()``

Notes:

- view debug is a Runtime-level debugging capability
- when enabled, a debug outline (only for observing boundaries and hierarchy) is overlaid on all views in the current runtime
- toggling view debug does not require ``update(...)``
- ``theme`` is a Runtime global resource, switched via ``set_theme(...)``
- ``set_theme(theme_id)`` immediately reapplies style to all loaded documents; if a file-backed document uses a
  ``Theme`` variant, its root is re-parsed
- ``set_theme(theme_id, false)`` only updates the Runtime's current theme and marks loaded documents' style or
  environment dirty inside the Runtime
- the Runtime always applies a built-in default theme first; ``list_supported_themes()`` does not include this built-in resource
- an external theme only overrides the built-in default theme; it does not replace the default-value baseline
- ``font`` is a Runtime global resource; ``list_supported_fonts(language)`` can filter font ids by language
- ``language`` is the Runtime's current language; ``get_language()`` returns the current value
- ``set_language(language)`` immediately reapplies style to all loaded documents; if the language also affects variants, constants, or initial binding values, the app layer must also call ``update(...)`` on the relevant document
- ``set_language(language, false)`` only updates the Runtime's current language and marks loaded documents' environment dirty inside the Runtime
- the built-in default theme does not specify a ``font``; if neither the node nor the current theme writes ``style.font`` explicitly, the Runtime prefers the "current language -> default font id" mapping
- only an explicit ``style.font: "${font.<id>}"`` bypasses the language default font; this suits a fixed brand font or special title, not body text that should follow language switching
- live preview is also a Runtime-level development aid, but applies only to documents loaded via ``load_file(...)``
- live preview watches the root file and its parsed dependencies for changes and calls ``update(...)`` automatically

.. _gui-interface-json_ui-runtime-sec-06:

Screen Related
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``list_displays()``
- ``list_layers()``
- ``mount_screen(document_id, absolute_path, target = MountTarget{})``
- ``unmount_screen(document_id, absolute_path)``
- ``push_transient_screen(document_id, absolute_path, target = MountTarget{})``: temporarily push a screen and return a ``TransientMountId``
- ``pop_transient_screen(transient_mount_id)``: pop a transient screen by id
- ``start_screen_flow(document_id, flow_id, target = MountTarget{})``
- ``trigger_screen_flow(document_id, flow_id, action)``
- ``stop_screen_flow(document_id, flow_id)``
- ``has_screen_flow(document_id, flow_id)``
- ``get_screen_flow_state(document_id, flow_id)``

``MountTarget`` contains at least:

- ``display_id``
- ``layer``
- ``mount_mode``
- ``z_order``

Defaults:

- ``display_id = ""``
  - means the backend default display
- ``layer = GuiLayer::Default``
- ``mount_mode = MountStackMode::Replace``
- ``z_order = 0``

The built-in layers are:

- ``Default``
- ``Top``
- ``System``
- ``Bottom``

Notes:

- ``Default`` is the default page layer of a given display
- ``Replace`` is the default mount semantics: another ``mount_screen(...)`` on the same ``display_id + layer`` only replaces the existing replace screen
- ``Stack`` lets multiple screens coexist on the same ``display_id + layer`` without replacing the replace screen
- within the same layer, ``z_order`` brings a screen forward; a larger value is higher; for the same ``z_order``, a later-mounted screen is on top
- a replaced screen with ``mountMode = "eager"`` is hidden
- a replaced screen with ``mountMode = "dynamic"`` is destroyed per dynamic semantics
- different ``display_id`` or different ``layer`` allows multiple mounted screens at once
- ``absolute_path`` still represents only the node path within a document, excluding display/layer

``screenFlow`` is a document-local screen flow asset; its definition is in :doc:`interaction/screen_flow`.
``start_screen_flow(document_id, flow_id, ...)`` mounts the flow's initial screen; if the flow is already running, it is idempotent and does not re-mount.
If transitions are configured, you can later use
``trigger_screen_flow(...)`` to switch screens by the current state and action; if transitions is omitted or empty, the flow only represents a
"static first screen", and the Runtime records the current state but triggering a switch returns an error. ``has_screen_flow(...)`` only checks whether the flow is **defined** in the document,
not whether the flow is running. ``stop_screen_flow(...)`` unmounts the current screen and stops the
flow. A document can run multiple flows at once; ``unload(...)`` or ``update(...)`` automatically stops flows running under that document.

.. _gui-interface-json_ui-runtime-sec-07:

Any Node Query
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``find_view(document_id, absolute_path)``
- ``View::find_view(path)``

Notes:

- ``find_view(document_id, absolute_path)`` can currently query any instantiated node
- all public paths use Unix-style absolute paths, e.g. ``/about/header/title``

.. _gui-interface-json_ui-runtime-sec-08:

Node Status and Runtime Operations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``get_view_state(view, kind)`` / ``get_view_state(document_id, absolute_path, kind)``: read a node's runtime state (``ViewStateKind``)
- ``scroll_view_to_visible(document_id, absolute_path, animated = true)`` / ``scroll_view_to_visible(view, animated = true)``: scroll the target node into the visible area
- ``set_view_src(document_id, absolute_path, src)``: directly update an image node's ``src``
- ``process_backend()``: drive one backend processing pass (events, animations, refresh, etc.); usually called periodically in the host main loop

.. _gui-interface-json_ui-runtime-sec-09:

Template Instance
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``create_view(document_id, template_id, parent_absolute_path, instance_id)``
- ``destroy_view(document_id, absolute_path)``

Where:

- ``template_id`` is the top-level template id
- ``parent_absolute_path`` is the parent's absolute path
- ``instance_id`` is explicitly provided by the caller
- ``destroy_view(...)`` can only delete a non-``screen`` subtree

.. _gui-interface-json_ui-runtime-sec-10:

Event Subscription
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``subscribe_event_action(document_id, action, handler)``
- ``subscribe_event_action_with_id(document_id, action, handler)``
- ``unsubscribe_subscription(subscription_id)``
- ``View::on_event(type, handler)``
- ``Button::on_click(handler)``

Notes:

- ``subscribe_event_action(...)`` uses exact ``document_id + action`` matching
- ``subscribe_event_action_with_id(...)`` uses the same ``document_id + action`` routing and returns a ``SubscriptionId``
- within the same document, a statically declared non-empty ``action`` must be globally unique
- multiple instances from a template definition can share the same ``action``; tell sources apart via ``event.path`` / ``event.node_id``
- the new interface allows pre-registration: you can register before the target template instance has been created
- ``subscribe_event_action(...)`` returns a ``ScopedConnection``; the connection disconnects automatically on destruction
- ``subscribe_event_action_with_id(...)`` returns a ``SubscriptionId``; suitable when you need to record the subscription identity or disconnect explicitly later
- ``unsubscribe_subscription(subscription_id)`` can explicitly disconnect a subscription obtained via ``with_id``; it returns failure for an invalid or already-disconnected id
- ``View::on_event(...)`` relies on a concrete, already-queryable instance and does no ``action`` string filtering

.. _gui-interface-json_ui-runtime-sec-11:

Runtime Animation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``start_view_animation(document_id, absolute_path, animation, completed_handler = {})``
- ``start_view_animation_with_id(document_id, absolute_path, animation, completed_handler = {})``
- ``start_view_animation_with_result(document_id, absolute_path, animation, completed_handler = {})``
- ``start_view_animation(view, animation, completed_handler = {})``
- ``start_view_animation_with_id(view, animation, completed_handler = {})``
- ``start_view_animation_with_result(view, animation, completed_handler = {})``

Notes:

- this set of APIs starts a view animation directly at runtime, without relying on ``animations`` pre-declared in the node JSON.
- currently recommended for:
  - ``placement.x``
  - ``placement.y``
  - ``placement.width``
  - ``placement.height``
  - ``imageProps.angle``
  - ``imageProps.offsetX``
  - ``imageProps.offsetY``
- ``animation.property`` reuses ``AnimationProperty``; currently supports at least:
  - ``Opacity``
  - ``X``
  - ``Y``
  - ``Width``
  - ``Height``
  - ``Scale``
  - ``Angle``
  - ``OffsetX``
  - ``OffsetY``
- ``animation.easing`` reuses ``AnimationEasing``; currently ``linear`` and ``easeIn`` cover common runtime scenarios.
- ``start_view_animation(...)`` returns a ``ScopedConnection``; the animation is canceled on destruction.
- ``start_view_animation_with_id(...)`` returns a ``SubscriptionId``; pair it with ``unsubscribe_subscription(subscription_id)`` to cancel actively.
- ``start_view_animation_with_result(...)`` returns a ``RuntimeAnimationStartResult``, bringing back both a ``SubscriptionId`` and start-result info, useful when the start fails or completes immediately.
- a new animation of the same animation property on the same view overrides the previous one.
- ``completed_handler`` fires only when the animation completes naturally; if the animation is canceled, the completion callback does not fire.

.. _gui-interface-json_ui-runtime-sec-12:

Resource Query
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``list_font_resources(document_id)``
- ``list_image_resources(document_id)``

Notes:

- a font query returns the Runtime's currently globally visible font resource set
- ``list_supported_fonts(language = "")`` returns font ids, not full descriptors
- an image query still returns, by ``document_id``, the document's currently visible image resources, including the document ``imageSet`` and Runtime global images
- a Runtime global image can be registered before ``load_file()``; a ``${image.<id>}`` in the document resolves to it during load validation
- when size is not given explicitly, ``register_image(...)`` asks the backend to complete ``width`` / ``height`` from image metadata
- during document load, only backend-required resources and document/global images with ``preload: true`` are preloaded; preload failures fail ``load_file()`` / ``load_json()`` rather than deferring failure to first display
- ``preload_image(s)`` can manually preload resources by image id visible to the current document; ``release_preloaded_image(s)`` releases only manual references, not automatic references held by ``preload: true``
- ``unregister_image(id)`` removes the resource from the Runtime global image table and releases the backend's preload reference to that image
- for image resources with the same name, the document ``imageSet`` is preferred, then the Runtime global image

.. _gui-interface-json_ui-runtime-sec-13:

Binding Batch Update and Incremental Apply
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``Runtime::set_binding_values(document_id, updates)`` can write multiple ``BindingValueUpdate`` groups at once.
- the Runtime merges, per node, the props/style/layout/placement apply masks triggered by the batch; a node usually re-applies a given domain only once in a batch.
- initial creation, document reload, and resource refresh still use a full apply; the high-frequency binding update path uses precise masks.
- the animation API modifies backend node attributes directly and does not write back to the binding store. If the app layer caches binding values itself, invalidate the cache after the animation ends or is canceled, or force-write to overwrite the post-animation real state.

.. _gui-interface-json_ui-runtime-sec-14:

Delayed Update Policy for Languages and Themes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The Runtime's default API keeps immediate-update semantics:

- ``set_language(language)`` validates the language, updates the Runtime's current language, and immediately reapplies style to all loaded documents
- ``set_theme(theme_id)`` validates the theme, updates the Runtime's current theme, and immediately reapplies style to all loaded documents; a file-backed document using a
  ``Theme`` variant re-parses its root
- ``update(document_id, file_path, environment)`` re-parses a single document; use it to let a new ``Environment`` affect variants, constant text, and initial binding values

If the app caches several loaded documents at once, a language or theme switch may not want to refresh all loaded pages synchronously on the display task. The caller can explicitly use Runtime lazy updates:

- ``set_language(language, false)`` updates the Runtime's current language and the loaded tree's environment record, and marks ``environment_dirty`` inside the Runtime
- ``set_theme(theme_id, false)`` updates the Runtime's current theme; an ordinary document is marked ``styles_dirty``, while a file-backed document using a ``Theme``
  variant is marked ``environment_dirty``
- before accessing a target document in ``mount_screen(...)``, ``find_view(...)``, ``create_view(...)``, ``subscribe_event_action(...)``, etc., the Runtime calls ``refresh_document_if_dirty(...)`` to honor dirty updates
- ``refresh_document_if_dirty(...)`` **handles only ``environment_dirty``**: when ``environment_dirty`` is set, it runs an internal ``update(...)`` on a document loaded via ``load_file(...)`` and clears environment dirty on success
- ``styles_dirty`` is not handled by the unified dirty pass: it is honored at specific access points; e.g. ``mount_screen(...)`` reapplies style only to the mounted subtree; it can also be honored explicitly by ``set_theme(theme_id)`` / ``reapply_styles(document_id)``
- a non-file-backed document has no re-parsable root file and cannot fully honor lazy environment updates; such documents need the default immediate API or an explicit ``update(...)`` by the caller
- an unloaded document needs no dirty mark; the first ``load_file(...)`` directly uses the caller's current ``Environment`` and the Runtime theme
