.. _gui-interface-json-ui-assets-view-screen-sec-00:

Viewscreen
==========

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

``viewScreen`` is a mountable top-level screen, mountable at runtime via ``mount_screen(...)`` or ``screenFlow`` at the mount path ``/<id>``.

Related Documents
--------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../view/index`

Fields
--------------------

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
     - Yes
     - Fixed to ``"viewScreen"``
   * - ``id``
     - string
     - none
     - Yes
     - Screen id; the mount path is ``/<id>``
   * - ``mountMode``
     - ``eager`` / ``dynamic``
     - ``eager``
     - No
     - ``dynamic`` means the subtree is created on first mount
   * - ``commonProps``
     - object
     - baseline
     - No
     - Common state, see :doc:`../styling/props/common`
   * - ``layout``
     - object
     - baseline
     - No
     - Direct child layout, see :doc:`../styling/layout`
   * - ``placement``
     - object
     - baseline
     - No
     - Placement of the current node, see :doc:`../styling/placement`
   * - ``style``
     - object
     - theme merge result
     - No
     - Visual style, see :doc:`../styling/style`
   * - ``stateStyles``
     - object
     - ``{}``
     - No
     - State style overrides, see :ref:`State Styles <gui-interface-json-ui-styling-style-state-styles>`
   * - ``styleRefs``
     - string[]
     - ``[]``
     - No
     - Named style references, see :doc:`../view/index`
   * - ``events``
     - array
     - ``[]``
     - No
     - Event declarations, see :doc:`../interaction/events`
   * - ``animations``
     - array
     - ``[]``
     - No
     - Declarative animations, see :doc:`../interaction/animations`
   * - ``bindings``
     - object
     - ``{}``
     - No
     - Store bindings, see :doc:`../interaction/bindings`
   * - ``children``
     - array
     - ``[]``
     - No
     - Child nodes or ``templateRef``

.. code-block:: json

   {
       "type": "viewScreen",
       "id": "home",
       "children": [
           {"type": "label", "id": "title", "labelProps": {"text": "Home"}}
       ]
   }
