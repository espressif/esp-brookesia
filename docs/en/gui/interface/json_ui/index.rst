.. _gui-interface-json-ui-index-sec-00:

GUI Interface
============================================================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

The GUI interface layer defines the common contract between application UI and concrete rendering backends, covering the document model, resource naming, action binding, runtime resources, and backend adaptation boundaries.

JSON UI is the current public backend-neutral UI authoring protocol: applications describe UI structure, resources, styling, and interaction in JSON, the Runtime parses it into a document model, and a concrete backend renders it.

This page explains the role, mainline model, and global constraints of JSON UI from the GUI interface perspective. Full field tables, examples, and Runtime mappings live in the group documents below; documentation maintenance and protocol sync rules are in :doc:`Protocol Maintenance <contributing>`.

Key points:

- A single ``root.json`` is the document entry
- Top-level ``assets`` and ``variants[].assets`` provide resources
- In-document asset first-level types: ``constant`` / ``imageSet`` / ``viewScreen`` / ``viewTemplate`` / ``interactionTemplate`` / ``screenFlow`` / ``styleSet``
- Runtime global resources: ``theme`` / ``fontSet`` / ``imageSet``

Mainline Model
--------------------

- ``root.json`` organizes the document entry, shared resources, and variant matching
- Each asset file provides constants and view definitions; fonts, images, and themes are usually registered or loaded by the Runtime
- ``view`` arranges children through ``layout`` and positions itself through ``placement``
- The Runtime parses JSON resources into a ``Document`` and indexes nodes by ``document_id + absolute_path``

Global Constraints
--------------------

Resource Reference
^^^^^^^^^^^^^^^^^^^^

A ``${...}`` reference must carry an explicit namespace:

.. code-block:: json

   "${constant.path.to.value}"
   "${font.title}"
   "${image.logo}"
   "${color.brand.primary}"

Rules:

- ``${constant.<path>}`` resolves from the merged constant tree of the matched ``constant`` asset
- ``${env.<field>}`` reads from the current parsing environment, e.g. ``${env.widthDp}`` / ``${env.heightDp}`` / ``${env.theme}``
- ``${font.<id>}`` expands to a font resource id, not a font file path
- ``${image.<id>}`` expands to an image resource id, not an image file path
- ``${color.<path>}`` expands to a color value and is only valid for ``style`` color fields
- ``${view.<path>}`` is only valid for ``placement.relativeTo``
- Supported namespaces: ``constant`` / ``env`` / ``font`` / ``image`` / ``color`` / ``view``, plus the expression wrapper ``${expr(...)}``

Not supported: ``${metrics.titleFont}``, the plural form ``${colors.*}``, ``font: "title"``, ``src: "logo"``, and the colon form ``${font:title}`` / ``${image:logo}``.

Units
^^^^^^^^^^^^^^^^^^^^

Sizes in JSON UI are converted to backend pixel values during parsing:

- ``px``: written as a JSON number, e.g. ``24``
- ``dp``: used for layout size, spacing, corner radius, borders, and other non-text sizes, ``px = round(dp * Environment.density)``
- ``sp``: used for font size, ``px = round(sp * Environment.density * Environment.font_scale)``

Field value rules: ``layout.gap`` takes a ``dp`` string or a bare number ``px``; ``placement.x`` / ``placement.y`` take a ``dp`` string, percentage, or bare number ``px``; ``placement.width`` / ``placement.height`` also accept ``match`` / ``wrap``; ``style.fontSize`` takes an ``sp`` string or a bare number ``px``.

For more complete unit and conversion examples, see :doc:`document/root` and :doc:`styling/placement`.

Json UI Specification
-----------------------

.. toctree::
   :maxdepth: 1
   :titlesonly:

   document/index
   assets/index
   view/index
   styling/index
   interaction/index
   runtime
   Protocol Maintenance <contributing>
