.. _gui-interface-json-ui-assets-interaction-template-sec-00:

interactionTemplate
===================

:link_to_translation:`en:[English]`

.. _gui-interface-json_ui-assets-interaction_template-sec-01:

概览
--------------------

``interactionTemplate`` 复用事件、动画、状态样式和部分 common props，只在 parser 阶段展开，不生成 runtime view。节点通过 ``interactionRefs`` 引用后，模板里的 ``commonProps``、``stateStyles``、``events`` 和 ``animations`` 会合并到节点上。

.. _gui-interface-json_ui-assets-interaction_template-sec-02:

相关文档
--------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../view/index`

.. _gui-interface-json_ui-assets-interaction_template-sec-03:

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
     - 固定为 ``"interactionTemplate"``
   * - ``id``
     - string
     - 无
     - 是
     - 模板 id
   * - ``commonProps``
     - object
     - ``{}``
     - 否
     - 默认 common props；节点本地字段优先
   * - ``stateStyles``
     - object
     - ``{}``
     - 否
     - 默认状态样式；节点本地字段优先
   * - ``events``
     - object[]
     - ``[]``
     - 否
     - 追加到节点事件前面
   * - ``animations``
     - object[]
     - ``[]``
     - 否
     - 追加到节点动画前面

.. code-block:: json

   {
       "type": "interactionTemplate",
       "id": "press.scale",
       "commonProps": {"pressLock": false, "pivotX": "50%", "pivotY": "50%"},
       "events": [
           {"type": "pressed", "effects": [{"type": "startAnimation", "animationId": "press_down"}]},
           {"type": "released", "effects": [{"type": "startAnimation", "animationId": "press_up"}]}
       ],
       "animations": [
           {"id": "press_down", "trigger": "manual", "property": "scale", "fromMode": "current", "to": 236},
           {"id": "press_up", "trigger": "manual", "property": "scale", "fromMode": "current", "to": 256}
       ]
   }
