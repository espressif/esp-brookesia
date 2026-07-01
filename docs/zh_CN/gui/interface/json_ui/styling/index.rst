.. _gui-interface-json-ui-styling-index-sec-00:

样式
==============================

:link_to_translation:`en:[English]`

.. _gui-interface-json_ui-styling-index-sec-01:

概览
--------------------

本分组负责一个 view 节点的布局与外观：``layout`` 决定如何排布直接子节点，``placement`` 决定自身如何被父节点摆放，``style`` 决定视觉样式，``props`` 决定各控件类型的字段级内容与状态。

.. _gui-interface-json_ui-styling-index-sec-02:

相关文档
--------------------

- :doc:`../index`
- :doc:`../view/index`
- :doc:`../interaction/bindings`

.. _gui-interface-json_ui-styling-index-sec-03:

本组文档
--------------------

.. list-table::
   :header-rows: 1

   * - 文档
     - 负责内容
   * - :doc:`layout`
     - 当前节点如何布局直接子节点（``none`` / ``flex`` / ``grid``）
   * - :doc:`placement`
     - 当前节点如何被父节点摆放（尺寸、坐标、相对定位、grid cell）
   * - :doc:`style`
     - 视觉样式字段及 backend 应用语义
   * - :doc:`props/index`
     - 各控件类型的 ``props`` 字段级细节

.. toctree::
   :maxdepth: 1

   layout
   placement
   style
   props/index
