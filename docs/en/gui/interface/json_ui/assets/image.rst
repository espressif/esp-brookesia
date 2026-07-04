.. _gui-interface-json-ui-assets-image-sec-00:

Imageset
====================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-assets-image-sec-01:

Overview
--------------------

The ``imageSet`` descriptor describes a set of image resources. A document can reference an ``imageSet`` in ``root.json.assets``, and the Runtime can also
``register_image_file/json(...)`` to register all of its images globally so that ``${image.<id>}`` can be used in ``imageProps.src``.

Image resources are always organized into ``imageSet.images[]``, even for a single image.

.. _gui-interface-json_ui-assets-image-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../runtime`

.. _gui-interface-json_ui-assets-image-sec-03:

Field Table
----------------------

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
     - Fixed to ``"imageSet"``
   * - ``images``
     - object[]
     - none
     - Yes
     - Image descriptor list; cannot be empty
   * - ``images[].id``
     - string
     - none
     - Yes
     - Image resource id
   * - ``images[].src``
     - string
     - ``""``
     - No
     - Image file path; resolved relative to the ``imageSet`` file directory
   * - ``images[].width``
     - integer px
     - ``0``
     - No
     - Source image width; used for image sizing
   * - ``images[].height``
     - integer px
     - ``0``
     - No
     - Source image height; used for image sizing

An ``images[]`` entry does not contain ``type``. Duplicate ids cannot appear within one ``imageSet``.

.. _gui-interface-json_ui-assets-image-sec-04:

Runtime API
----------------------

- ``register_image(...)``
- ``unregister_image(...)``
- ``register_image_json(...)``
- ``register_image_file(...)``
- ``list_image_resources(document_id)``

``register_image_json/file(...)`` registers all images in an ``imageSet``; if any one fails to register, the images registered this time are rolled back.

Document image resources take priority over Runtime global images; Runtime global images supplement image ids a document does not declare.

.. _gui-interface-json_ui-assets-image-sec-05:

Source File Format (Backend Related)
------------------------------------------------------------------------

The concrete file format of ``src`` / ``RuntimeImageResource.primary_src`` is decided by the current backend. ``gui_interface`` only stores the image id, path, and size, and calls the backend's resource-completion and preload hooks during the document load phase.

If a backend supports reading dimensions from a proprietary image format, ``register_image(...)`` can complete ``width`` / ``height`` through a backend hook when they are missing. If they cannot be completed, ``width`` / ``height`` must be explicitly positive.

.. _gui-interface-json_ui-assets-image-sec-06:

Example
--------------------

.. code-block:: json

   {
       "type": "imageSet",
       "images": [
           {
               "id": "logo",
               "src": "../../../../../docs/_static/brookesia_logo.png",
               "width": 3409,
               "height": 834
           }
       ]
   }
