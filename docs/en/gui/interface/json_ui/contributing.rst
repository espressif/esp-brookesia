.. _gui-interface-json-ui-contributing-sec-00:

Protocol Maintenance
============================================================

:link_to_translation:`zh_CN:[中文]`

This page is for protocol and documentation maintainers. It collects the document navigation, reading route, recommended directory structure, writing conventions, and the sync checklist for protocol changes. Framework users normally do not need this page.

Document Navigation
--------------------

Documents are grouped by topic, and each group directory has an index that serves as its navigation.

.. list-table::
   :header-rows: 1

   * - Group
     - Entry
     - Responsibility
   * - entry
     - :doc:`index`
     - Protocol overview, global constraints, module navigation
   * - document
     - :doc:`document/index`
     - Document loading layer: ``root.json``, ``variants[]``, ``when``, ``Environment``, unit conversion
   * - assets
     - :doc:`assets/index`
     - Unified asset model, document assets vs. Runtime global resource boundaries
   * - view
     - :doc:`view/index`
     - ``view`` common fields and each control type
   * - styling
     - :doc:`styling/index`
     - ``layout``, ``placement``, ``style``, and each control ``props``
   * - interaction
     - :doc:`interaction/index`
     - ``bindings``, ``events``, ``animations``, ``screenFlow``
   * - runtime
     - :doc:`runtime`
     - Runtime resource model, ``document_id + absolute_path`` API

Reading Route
--------------------

When meeting this protocol for the first time, read it in the following order:

#. :doc:`document/root`
#. :doc:`assets/index`
#. :doc:`view/index`
#. :doc:`styling/layout`
#. :doc:`styling/placement`
#. :doc:`styling/style`
#. :doc:`styling/props/index`
#. :doc:`interaction/bindings`
#. :doc:`interaction/events`
#. :doc:`interaction/animations`
#. :doc:`interaction/screen_flow`
#. :doc:`runtime`

Read ``root`` / ``assets`` / ``view`` first to build the overall mental model, then ``styling`` and ``interaction`` for field details, and finally the ``runtime`` integration API.

Recommended Directory Structure
-------------------------------

The directory name does not affect the protocol itself; whether a file is a constant resource or a UI resource is decided by its in-file ``type``, not the directory name. The layout still used by the example projects is:

.. code-block:: text

   assets/
     documents/
       settings_home/
         root.json
         nodes/
           settings_home.json
           nav_item.json
     shared/
       constants/
         base.json
         theme.json
       fonts/
         default.json
       images/
         logo.json

Document Writing Conventions
----------------------------

Follow these conventions when adding or modifying documents so the whole set stays consistent.

- The body text is mainly Chinese; technical terms, field names, enum values, and APIs stay in English.
- Prefer Chinese for first-level titles; leaf control pages may use the control ``type`` name as the title (such as ``button``).
- Keep JSON field names in ``camelCase``; JSON concept names lowercase (``view`` / ``document`` / ``variant``); C++ types capitalized (``Runtime`` / ``View`` / ``Document``); the rendering backend written as ``backend``.
- Module page structure: overview -> related documents -> field table -> field notes -> examples.
- Leaf control page structure: overview -> related documents -> exclusive props -> common events -> (optional) examples.

Field table headers are fixed by purpose:

.. list-table::
   :header-rows: 1

   * - Purpose
     - Header
   * - Protocol / asset fields
     - ``Key | Type | Default | Required | Description``
   * - layout / placement / style / animations
     - ``Key | Type | Default | UI Effect | Backend Behavior/Limits``
   * - Control props
     - ``Key | Type | Default | Binding | UI Effect/Limits``

Sync Checklist for Protocol Changes
-----------------------------------

Documentation, code, and resources must stay consistent. When changing the protocol, sync-check at least the following documents and sources:

- Group documents: ``document`` / ``assets`` / ``view`` / ``styling`` / ``interaction`` / ``runtime``
- ``src/parser.cpp``, ``src/validator.cpp``
- ``include/brookesia/gui_interface/document.hpp``
- ``include/brookesia/gui_interface/widget.hpp``
- ``include/brookesia/gui_interface/runtime.hpp``
