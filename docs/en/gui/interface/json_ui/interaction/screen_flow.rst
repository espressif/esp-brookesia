.. _gui-interface-json-ui-interaction-screen-flow-sec-00:

Screen Flow
======================================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

``screenFlow`` is a document asset that describes a mutually exclusive switching state machine for a group of top-level screens within the same document. A
document can declare multiple flows, but a given screen can belong to only one flow. A flow handles only screen
``mount`` / ``unmount``; it runs no script callbacks, does not cross documents, and does not manage the app lifecycle.

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../assets/index`
- :doc:`../runtime`

Asset Field Table
----------------------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Required
     - Description
   * - ``type``
     - string
     - none
     - yes
     - fixed to ``"screenFlow"``
   * - ``id``
     - string
     - none
     - yes
     - flow id, unique within the document
   * - ``screens``
     - string[]
     - none
     - yes
     - the list of top-level screen ids managed by this flow, without ``/``
   * - ``initial``
     - string
     - none
     - yes
     - initial state id; must be in ``screens``
   * - ``transitions``
     - object[]
     - ``[]``
     - no
     - transition list; omitted or an empty array declares only a static initial screen

Transitions[]
--------------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Required
     - Description
   * - ``from``
     - string[]
     - ``[]``
     - no
     - source state list; omitted or an empty array means any current state
   * - ``action``
     - string
     - none
     - yes
     - transition action name
   * - ``to``
     - string
     - none
     - yes
     - target state; must be in ``screens``

Rules:

- a state id directly uses the top-level screen's ``id``; for example, screen ``/home`` corresponds to state ``home``.
- every state in ``screens`` must reference an existing top-level screen in the current document.
- the same top-level screen cannot be included in the ``screens`` of multiple ``screenFlow`` assets.
- the state ids in ``initial``, ``to``, ``from[]`` must belong to the current flow's ``screens``.
- an omitted ``from`` or ``from: []`` means any current state.
- the same ``(from_state, action)`` may be defined only once; multiple empty ``from`` with the same action also raise an error.
- a transition with an exact ``from`` takes precedence over a wildcard (empty ``from``) transition.
- triggering to the current state while the current screen is still mounted on the flow target is treated as a no-op success.
- if ``transitions`` is omitted or empty, the flow is still valid; starting the flow mounts the ``initial`` screen, but triggering a transition fails.

Example
--------------------

``root.json``:

.. code-block:: json

   {
       "version": "0.1.0",
       "assets": [
           "screens/home.json",
           "screens/settings.json",
           "flows/main.json"
       ]
   }

``flows/main.json``:

.. code-block:: json

   {
       "type": "screenFlow",
       "id": "main",
       "screens": ["home", "settings"],
       "initial": "home",
       "transitions": [
           {"from": [], "action": "open_home", "to": "home"},
           {"from": ["home", "settings"], "action": "open_settings", "to": "settings"}
       ]
   }

Runtime API
----------------------

- ``Runtime::start_screen_flow(document_id, flow_id, target)``: start the flow and mount the ``initial`` screen; if the flow is already running, it is idempotent and does not re-mount.
- ``Runtime::trigger_screen_flow(document_id, flow_id, action)``
- ``Runtime::stop_screen_flow(document_id, flow_id)``
- ``Runtime::get_screen_flow_state(document_id, flow_id)``
- ``Runtime::has_screen_flow(document_id, flow_id)``: only checks whether the flow is **defined** in the document; it does not mean the flow is running.

``screenFlow`` does not declare a layer. ``target`` is specified by the host that calls ``start_screen_flow(...)``; when unspecified, the default display's
``GuiLayer::Default`` is used.
