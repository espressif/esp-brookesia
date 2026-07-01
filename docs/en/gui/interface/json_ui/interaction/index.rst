.. _gui-interface-json-ui-interaction-index-sec-00:

Interaction
======================================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

This group covers the view's interaction and dynamic behavior: ``bindings`` handles store data binding and subscription, ``events`` handles interaction events and action routing, ``animations`` handles declarative animations, and ``screenFlow`` handles state-machine switching across a group of screens within the same document.

Related Documents
----------------------------------

- :doc:`../index`
- :doc:`../view/index`
- :doc:`../runtime`

Documents for This Group
------------------------------------------------

.. list-table::
   :header-rows: 1

   * - Document
     - Responsibility
   * - :doc:`bindings`
     - ``bindings``, store path scope, subscribe
   * - :doc:`events`
     - ``events``, payload, action routing
   * - :doc:`animations`
     - basic declarative animation (mount/show/hide, etc.)
   * - :doc:`screen_flow`
     - ``screenFlow``, state, transition and Runtime screen flow API

.. toctree::
   :maxdepth: 1

   bindings
   events
   animations
   screen_flow
