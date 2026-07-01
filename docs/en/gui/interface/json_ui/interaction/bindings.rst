.. _gui-interface-json-ui-interaction-bindings-sec-00:

Bindings
================================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-interaction-bindings-sec-01:

Overview
--------------------

``bindings`` is an optional object that applies the string values from Runtime bindings to the current node's leaf fields.

.. _gui-interface-json_ui-interaction-bindings-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../styling/props/index`
- :doc:`../runtime`

.. _gui-interface-json_ui-interaction-bindings-sec-03:

Field Table
----------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Description
   * - ``<fieldPath>``
     - string
     - none
     - public JSON ``camelCase`` dotted path; only leaf fields allowed
   * - value
     - string
     - none
     - the local store key in the current node path scope, without a prefix

.. _gui-interface-json_ui-interaction-bindings-sec-04:

Rule
--------------------

- the ``bindings`` key must be a public JSON ``camelCase`` dotted path and can only point to a leaf field.
- the ``bindings`` value is the local store key in the current node path scope, without a prefix.
- the same local key can be safely reused on different nodes; the real scope is ``document_id + absolute_path + local_key``.
- whether a props-field binding is available depends on the current control type.

.. _gui-interface-json_ui-interaction-bindings-sec-05:

Why the Public Interface Needs Absolute Path
------------------------------------------------------------------------------------------------------

- under the same document, multiple static nodes can reuse the same local key.
- multiple instances created from the same template definition also reuse a local key with the same name.
- ``document_id`` can locate only the document, not uniquely a specific node in a template instance.
- ``absolute_path`` is what uniquely identifies:
  - which static node
  - which node in which template instance
- therefore the formal public locator for bindings is ``document_id + absolute_path + local_key``, not a document-level unique key.

.. _gui-interface-json_ui-interaction-bindings-sec-06:

Common Paths
------------------------

.. list-table::
   :header-rows: 1

   * - Path
     - Applicable control types
   * - ``commonProps.hidden`` / ``disabled`` / ``clickable`` / ``scrollable`` / ``pressLock`` / ``angle`` / ``zoom`` / ``pivotX`` / ``pivotY``
     - all
   * - ``labelProps.text``
     - ``label`` / ``checkbox``
   * - ``imageProps.src`` / ``recolor`` / ``recolorOpacity`` / ``angle`` / ``offsetX`` / ``offsetY`` / ``zoom`` / ``pivotX`` / ``pivotY``
     - ``image``
   * - ``textInputProps.text`` / ``placeholder`` / ``password`` / ``multiline`` / ``maxLength``
     - ``textInput``
   * - ``rangeProps.value`` / ``min`` / ``max`` / ``step``
     - ``slider`` / ``progressBar`` / ``arc``
   * - ``toggleProps.checked``
     - ``switch`` / ``checkbox``
   * - ``dropdownProps.options`` / ``selectedIndex``
     - ``dropdown``
   * - ``tableProps.rows`` / ``columns`` / ``cells``
     - ``table``
   * - ``lineProps.points``
     - ``line``
   * - ``keyboardProps.mode`` / ``popovers`` / ``allowedModes`` / ``iconSize``
     - ``keyboard``
   * - ``canvasProps.commands``
     - ``canvas``
   * - ``style.imageRecolor`` / ``style.imageRecolorOpacity``
     - all; produces a visual effect on ``image`` nodes
   * - ``style.*``
     - all
   * - ``stateStyles.<state>.*``
     - all; ``state`` supports ``pressed`` / ``checked`` / ``focused`` / ``focusKey`` / ``edited`` / ``hovered`` / ``scrolled`` / ``disabled`` / ``user1..user4``
   * - ``partStyles.<part>.*``
     - controls with internal parts; ``part`` currently supports ``indicator`` / ``knob``
   * - ``partStyles.<part>.stateStyles.<state>.*``
     - controls with internal parts; for part state styles such as pressed / disabled
   * - ``layout.*``
     - all
   * - ``placement.*``
     - all

For the complete props fields, see :doc:`../styling/props/index`; for style/layout/placement fields, see their pages.

``stateStyles.<state>.*`` uses the same leaf fields as ``style.*``, for example
``stateStyles.pressed.bgColor``, ``stateStyles.disabled.opacity``, ``stateStyles.checked.borderWidth``.
Not supported: ``stateStyles.<state>.font``, ``stateStyles.<state>.fontSize``, and
``stateStyles.<state>.imageFontSize``.

``partStyles`` uses the same leaf fields as ``style.*``, for example
``partStyles.indicator.bgGradientColor``, ``partStyles.indicator.bgGradientDirection``,
``partStyles.knob.bgColor``, ``partStyles.knob.stateStyles.pressed.bgColor``. Not supported:
``partStyles.<part>.font``, ``partStyles.<part>.fontSize``, and ``partStyles.<part>.imageFontSize``.

.. _gui-interface-json_ui-interaction-bindings-sec-07:

Public API
--------------------

- ``Runtime::set_binding_value(document_id, absolute_path, key, value)`` writes a single binding store; nodes that have declared the binding reapply the corresponding field through their registered listener.
- ``Runtime::set_binding_values(document_id, updates)`` writes multiple bindings in batch, explicitly merging each node's dirty mask before a single apply, reducing backend applies during high-frequency updates.
- ``Runtime::get_binding_value(document_id, absolute_path, key)`` reads the current scoped binding value.
- ``Runtime::subscribe_binding_value(document_id, absolute_path, key, listener)`` listens for writes to a scoped binding; the listener signature is ``(absolute_path, key, value)``.
- ``Runtime::subscribe_binding_value_with_id(document_id, absolute_path, key, listener)`` returns a stable ``subscription_id``.
- ``Runtime::unsubscribe_subscription(subscription_id)`` actively disconnects, by id, a subscription obtained via ``with_id``.
- ``View::set_binding_value(key, value)`` / ``View::get_binding_value(key)`` are convenient forms scoped to the current instance path.
- ``subscribe_binding_value(...)`` returns a ``ScopedConnection``; the connection disconnects automatically on destruction, with no manual ``unsubscribe(...)``.
- ``subscribe_binding_value_with_id(...)`` returns a ``SubscriptionId``; suitable when you need logging, tracing, or explicit disconnection.
- if the caller needs to disconnect explicitly, use ``Runtime::unsubscribe_subscription(subscription_id)``; this does not change the rule that ``absolute_path`` is the unique locating dimension for bindings.
- ``IDataStore`` is an internal Runtime implementation detail, not a public entry point for bindings.
- ``bindings`` is a node-level scoped binding capability, not a sub-capability of ``events``.

.. _gui-interface-json_ui-interaction-bindings-sec-08:

Incremental Update Behavior
------------------------------------------------------

- a binding write maps to a specific leaf field in props/style/layout/placement.
- on first node creation the backend still fully applies props, layout, placement, and style.
- later binding updates reapply only the mask of the changed field; for example, ``imageProps.offsetX`` does not replay the full image props, and ``placement.x`` does not re-set width/height.
- the binding value of ``placement.x`` / ``placement.y`` can use a percentage offset such as ``"50%"``.
- the binding value of ``placement.width`` / ``placement.height`` can use a percentage such as ``"75%"``; ``placement.aspectRatio``
  can use ``"16:9"`` or a numeric string.
- in a batch write, multiple fields of the same node merge their masks first, then apply once.
- runtime animation acts directly on the backend node and does not update the binding store; if the app layer keeps its own binding cache, invalidate or force-write the key after the animation ends.

.. _gui-interface-json_ui-interaction-bindings-sec-09:

Example
--------------------

.. code-block:: json

   "bindings": {
       "labelProps.text": "status",
       "keyboardProps.iconSize": "keyboardIconSize",
       "style.fontSize": "fontSize",
       "style.imageFontSize": "iconFontSize",
       "stateStyles.pressed.bgColor": "pressedBg",
       "partStyles.indicator.bgGradientColor": "progressEndColor",
       "partStyles.knob.stateStyles.pressed.bgColor": "pressedKnobBg",
       "layout.gap": "gap",
       "placement.width": "width",
       "placement.aspectRatio": "ratio"
   }
