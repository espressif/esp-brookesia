.. _gui-interface-json-ui-assets-view-screen-sec-00:

viewScreen
==========

:link_to_translation:`en:[English]`

概览
--------------------

``viewScreen`` 是可挂载的 top-level screen，运行时可由 ``mount_screen(...)`` 或 ``screenFlow`` 挂载，挂载路径为 ``/<id>``。

相关文档
--------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../view/index`

字段
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - 是否必填
     - 说明
   * - ``type``
     - string
     - 无
     - 是
     - 固定为 ``"viewScreen"``
   * - ``id``
     - string
     - 无
     - 是
     - screen id；挂载路径为 ``/<id>``
   * - ``mountMode``
     - ``eager`` / ``dynamic``
     - ``eager``
     - 否
     - ``dynamic`` 表示首次 mount 时创建子树
   * - ``commonProps``
     - object
     - baseline
     - 否
     - 通用状态，见 :doc:`../styling/props/common`
   * - ``layout``
     - object
     - baseline
     - 否
     - 直接子节点布局，见 :doc:`../styling/layout`
   * - ``placement``
     - object
     - baseline
     - 否
     - 当前节点摆放，见 :doc:`../styling/placement`
   * - ``style``
     - object
     - theme 合并结果
     - 否
     - 视觉样式，见 :doc:`../styling/style`
   * - ``stateStyles``
     - object
     - ``{}``
     - 否
     - 状态样式覆盖，见 :ref:`State Styles <gui-interface-json-ui-styling-style-state-styles>`
   * - ``styleRefs``
     - string[]
     - ``[]``
     - 否
     - 命名样式引用，见 :doc:`../view/index`
   * - ``events``
     - array
     - ``[]``
     - 否
     - 事件声明，见 :doc:`../interaction/events`
   * - ``animations``
     - array
     - ``[]``
     - 否
     - 声明式动画，见 :doc:`../interaction/animations`
   * - ``bindings``
     - object
     - ``{}``
     - 否
     - store 绑定，见 :doc:`../interaction/bindings`
   * - ``children``
     - array
     - ``[]``
     - 否
     - 子节点或 ``templateRef``

.. code-block:: json

   {
       "type": "viewScreen",
       "id": "home",
       "children": [
           {"type": "label", "id": "title", "labelProps": {"text": "Home"}}
       ]
   }
