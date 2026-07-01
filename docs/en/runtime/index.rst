.. _runtime-index-sec-00:

Runtime Components
==================

:link_to_translation:`zh_CN:[中文]`

This section documents the runtime manager and supported runtime backends for dynamic app packages.

.. only:: html

   .. raw:: html
      :file: ../../_static/mermaid/en/runtime/index/diagram.html

.. only:: latex

   .. image:: ../../_static/mermaid/en/runtime/index/diagram.png
      :width: 100%

.. rubric:: Component Responsibilities

- Runtime Manager provides backend registration and app context ownership.
- ELF, JS, Lua, and WASM backends execute package code behind one lifecycle contract.

.. toctree::
   :maxdepth: 1

   Runtime Manager <manager>
   ELF Runtime <elf>
   JS Runtime <js>
   Lua Runtime <lua>
   WASM Runtime <wasm>
