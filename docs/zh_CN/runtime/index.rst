.. _runtime-index-sec-00:

运行时组件
============

:link_to_translation:`en:[English]`

本分类说明运行时管理器以及动态应用包支持的运行时后端。

.. only:: html

   .. raw:: html
      :file: ../../_static/mermaid/zh_CN/runtime/index/diagram.html

.. only:: latex

   .. image:: ../../_static/mermaid/zh_CN/runtime/index/diagram.png
      :width: 100%

.. rubric:: 组件职责

- Runtime Manager 提供后端注册和应用上下文归属。
- ELF、JS、Lua 与 WASM 后端在统一生命周期合约后执行包代码。

.. toctree::
   :maxdepth: 1

   Runtime Manager <manager>
   ELF 运行时 <elf>
   JS 运行时 <js>
   Lua 运行时 <lua>
   WASM 运行时 <wasm>
