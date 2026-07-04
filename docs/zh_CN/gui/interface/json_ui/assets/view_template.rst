.. _gui-interface-json-ui-assets-view-template-sec-00:

viewTemplate
============

:link_to_translation:`en:[English]`

.. _gui-interface-json_ui-assets-view_template-sec-01:

概览
--------------------

``viewTemplate`` 是可复用的模板，运行时可由 ``create_view(...)`` 动态实例化，也可在 ``children[]`` 中通过 ``templateRef`` 静态实例化。

.. _gui-interface-json_ui-assets-view_template-sec-02:

相关文档
--------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../view/index`

.. _gui-interface-json_ui-assets-view_template-sec-03:

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
     - 固定为 ``"viewTemplate"``
   * - ``id``
     - string
     - 无
     - 是
     - 模板 id
   * - ``node``
     - object
     - 无
     - 是
     - 模板根控件；不能包含 ``id``，parser 会使用模板 id 或实例 id 注入根 id

.. code-block:: json

   {
       "type": "viewTemplate",
       "id": "menu_item",
       "node": {
           "type": "button",
           "children": [
               {"type": "label", "id": "title", "labelProps": {"text": "Item"}}
           ]
       }
   }

.. _gui-interface-json_ui-assets-view_template-sec-04:

templateRef
----------------------

``templateRef`` 只能出现在 ``children[]`` 中，是 parser 指令，不是运行时控件类型。

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
     - 固定为 ``"templateRef"``
   * - ``id``
     - string
     - 无
     - 是
     - 实例根节点 id
   * - ``templateId``
     - string
     - 无
     - 是
     - 要实例化的 ``viewTemplate.id``
   * - ``slots``
     - object
     - ``{}``
     - 否
     - 按模板内部 slot id 替换 slot 内容
   * - ``overrides``
     - object
     - ``{}``
     - 否
     - 按模板内部相对路径覆盖局部字段

.. code-block:: json

   {
       "type": "templateRef",
       "id": "wifi_item",
       "templateId": "menu_item",
       "overrides": {
           "title": {
               "labelProps": {"text": "Wi-Fi"}
           }
       }
   }

模板可在 ``children[]`` 中使用 ``slot`` parser 指令暴露可替换子树：

.. code-block:: json

   {
       "type": "viewTemplate",
       "id": "settings_card",
       "node": {
           "type": "container",
           "children": [
               {"type": "slot", "id": "content", "children": []}
           ]
       }
   }

``slot.id`` 必须非空。实例通过 ``slots`` 填充对应内容。未提供某个 slot 时，使用 slot 自带的
``children`` 作为默认内容；没有默认内容时为空。``slot`` 只在 parser 阶段存在，不是运行时控件类型。
未知 slot、重复 slot id 或普通 view tree 中残留 ``slot`` 都会报错。

``overrides`` 的 key 使用模板内部相对路径，空字符串或 ``"."`` 表示模板根节点。覆盖项只能替换局部字段；
不允许覆盖 ``id``、``type``、``children``、``node``、``templateId``、``slots`` 或 ``overrides``。当 ``slots`` 和
``overrides`` 同时存在时，先填充 slots，再应用 overrides。
