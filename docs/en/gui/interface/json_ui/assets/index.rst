.. _gui-interface-json-ui-assets-index-sec-00:

Assets
============================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-assets-index-sec-01:

Overview
--------------------

This page is the entry description for assets and Runtime resources. Public JSON uses ``camelCase``.

It does not cover ``view`` internal field details, nor the specifics of ``layout`` / ``placement`` / ``style`` / ``props``. For those, see :doc:`../view/index`, :doc:`../styling/layout`, :doc:`../styling/placement`, :doc:`../styling/style`, :doc:`../styling/props/index`.

.. _gui-interface-json_ui-assets-index-sec-02:

Related Documents
----------------------------------

- :doc:`../index`
- :doc:`../document/root`
- :doc:`../view/index`
- :doc:`../runtime`

.. _gui-interface-json_ui-assets-index-sec-03:

Asset Types
--------------------

Entries in ``root.json.assets`` and ``variants[].assets`` may be path strings or inline asset objects. Paths resolve relative to the directory of the current ``root.json``, and relative paths inside an inline object resolve against that same directory.

Document assets compose the current document:

.. list-table::
   :header-rows: 1

   * - type
     - document
     - Description
   * - ``constant``
     - :doc:`constant`
     - Constant tree, referenced by ``${constant.<path>}``
   * - ``imageSet``
     - :doc:`image`
     - Image resources of the current document, referenced by ``imageProps.src``
   * - ``viewScreen``
     - :doc:`view_screen`
     - Mountable page root node
   * - ``viewTemplate``
     - :doc:`view_template`
     - Reusable template root node
   * - ``interactionTemplate``
     - :doc:`interaction_template`
     - Reusable interaction events, animations, and state styles
   * - ``styleSet``
     - :doc:`theme`
     - Named style collection within the document, referenced by ``styleRefs``
   * - ``screenFlow``
     - :doc:`../interaction/screen_flow`
     - Switching state machine for a group of screens in one document

Runtime global resources are registered or loaded through the Runtime API:

.. list-table::
   :header-rows: 1

   * - type
     - document
     - Description
   * - ``fontSet``
     - :doc:`font`
     - Global font resource collection, referenced by ``style.font``
   * - ``imageSet``
     - :doc:`image`
     - Global image resource collection, referenced by ``imageProps.src``
   * - ``theme``
     - :doc:`theme`
     - Global theme overlay, selected by ``set_theme(...)``

.. _gui-interface-json_ui-assets-index-sec-04:

Resource Boundaries
--------------------------------------

- ``constant``, ``imageSet``, ``viewScreen``, ``viewTemplate``, ``interactionTemplate``, ``styleSet``, ``screenFlow`` are the document asset types.
- An ordinary view node uses ``type`` for its control type.
- Image and font resources always use ``imageSet`` / ``fontSet``, even for a single resource.
- ``fontSet`` and ``theme`` are not document ``assets`` types; register or load them through the Runtime global API.

.. _gui-interface-json_ui-assets-index-sec-05:

Subdocuments
----------------------

- :doc:`constant`
- :doc:`view_screen`
- :doc:`view_template`
- :doc:`interaction_template`
- :doc:`font`
- :doc:`image`
- :doc:`theme`

.. toctree::
   :maxdepth: 1

   constant
   font
   image
   theme
   view_screen
   view_template
   interaction_template
