.. _gui-interface-json-ui-interaction-events-sec-00:

事件
============================

:link_to_translation:`en:[English]`

.. _gui-interface-json_ui-interaction-events-sec-01:

概览
--------------------

``events`` 是可选数组。每个事件项描述一个 backend 事件到 runtime action 的映射。

.. _gui-interface-json_ui-interaction-events-sec-02:

相关文档
--------------------

- :doc:`index`
- :doc:`../index`
- :doc:`animations`
- :doc:`../runtime`

.. _gui-interface-json_ui-interaction-events-sec-03:

事件项字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - 是否必填
     - 说明
   * - ``type``
     - event type enum
     - ``clicked``
     - 否
     - backend 事件类型
   * - ``action``
     - string
     - ``""``
     - 否
     - runtime 分发 action
   * - ``effects``
     - object[]
     - ``[]``
     - 否
     - 本地事件效果，按声明顺序执行

.. _gui-interface-json_ui-interaction-events-sec-04:

事件类型
--------------------

.. list-table::
   :header-rows: 1

   * - JSON 值
     - 说明
   * - ``pressed``
     - 按下
   * - ``pressing``
     - 按住过程中
   * - ``pressLost``
     - 按住后滑出控件，当前 press sequence 视为无效点击
   * - ``released``
     - 释放
   * - ``clicked``
     - 点击
   * - ``longPressed``
     - 长按
   * - ``longPressedRepeat``
     - 长按重复
   * - ``focused``
     - 获得焦点
   * - ``defocused``
     - 失去焦点
   * - ``valueChanged``
     - 值变化
   * - ``ready``
     - 输入完成
   * - ``cancel``
     - 取消
   * - ``scroll``
     - 滚动
   * - ``gesture``
     - 手势

.. _gui-interface-json_ui-interaction-events-sec-05:

Payload
--------------------

Runtime 事件统一使用 ``boost::json::object payload`` 承载附加数据。

.. list-table::
   :header-rows: 1

   * - 来源
     - 常见 event type
     - payload
   * - ``textInput``
     - ``valueChanged``
     - ``{ "text": "..." }``
   * - ``textInput``
     - ``ready``
     - ``{}``（``ready`` 不携带 ``text``）
   * - ``slider`` / ``progressBar`` / ``arc``
     - ``valueChanged``
     - ``{ "value": 45 }``
   * - ``switch`` / ``checkbox``
     - ``valueChanged``
     - ``{ "checked": true }``
   * - ``dropdown``
     - ``valueChanged``
     - ``{ "selectedIndex": 1 }``
   * - 支持手势的节点
     - ``gesture``
     - ``{ "direction": "left" }``，取值 ``left`` / ``right`` / ``up`` / ``down``
   * - 其它无附加数据事件
     - ``clicked`` 等
     - ``{}``

.. _gui-interface-json_ui-interaction-events-sec-06:

订阅入口
--------------------

- ``Runtime::subscribe_event_action(document_id, action, ...)``：按 ``document_id + action`` 精确匹配。
- ``Runtime::subscribe_event_action_with_id(document_id, action, ...)``：在相同路由语义上返回稳定的 ``subscription_id``。
- ``Runtime::unsubscribe_subscription(subscription_id)``：按 id 主动断开通过 ``with_id`` 获取到的事件订阅。
- ``View::on_event(...)``：拿到具体实例后按 ``EventType`` 订阅，不使用 ``action`` 过滤。

.. _gui-interface-json_ui-interaction-events-sec-07:

事件效果（effects）
--------------------------

``effects`` 在 Runtime 内执行，不需要 app action 回调。单个 effect 失败会记录 warning 并继续执行后续 effect；
非法 schema 会在解析阶段报错。

> 提示：如果节点需要在按住后滑出范围时收到 ``pressLost``，请在该节点设置 ``commonProps.pressLock=false``。
> 默认 ``pressLock=true`` 会在手指滑出节点范围时尽量保持 pressed target。

.. _gui-interface-json_ui-interaction-events-sec-08:

emitAction
^^^^^^^^^^^^^^^^^^^^

派发 runtime action。``requireValidPress=true`` 时，如果同一次按压序列中已经收到 ``pressLost``，则跳过派发。

.. code-block:: json

   {
       "type": "emitAction",
       "action": "launcher.open",
       "requireValidPress": true
   }

.. _gui-interface-json_ui-interaction-events-sec-09:

setProperty
^^^^^^^^^^^^^^^^^^^^^^

修改目标 view 的单个字段。``field`` 复用 binding target 语法，例如 ``commonProps.hidden``、
``labelProps.text``、``imageProps.src``、``imageProps.recolor``、``imageProps.recolorOpacity``、``style.opacity``、
``stateStyles.pressed.bgColor``、
``style.imageFontSize``、``partStyles.indicator.bgGradientColor``、``partStyles.knob.stateStyles.pressed.bgColor``、
``layout.gap``、``placement.x``。
``placement.x`` / ``placement.y`` 的 event value 可使用 ``"50%"`` 百分比 offset，规则同 placement 文档。

.. code-block:: json

   {
       "type": "setProperty",
       "target": "self",
       "field": "style.opacity",
       "value": 180
   }

.. _gui-interface-json_ui-interaction-events-sec-10:

setProperties
^^^^^^^^^^^^^^^^^^^^^^^^^^

批量修改多个字段。每个 ``updates[]`` entry 包含 ``target``、``field``、``value``。

.. code-block:: json

   {
       "type": "setProperties",
       "updates": [
           {"target": "self", "field": "commonProps.zoom", "value": 236},
           {"target": "./label", "field": "style.opacity", "value": 220}
       ]
   }

.. _gui-interface-json_ui-interaction-events-sec-11:

startAnimation / stopAnimation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

启动或停止动画。``startAnimation`` 可通过 ``animationId`` 引用目标 view 的 ``animations[]`` 中的
``trigger=manual`` 动画，也可以内嵌 ``animation`` object。``stopAnimation`` 按 ``target + animationId``
停止由 event effect 启动并记录的动画。

.. code-block:: json

   {
       "type": "startAnimation",
       "target": "self",
       "animationId": "press_down"
   }

.. _gui-interface-json_ui-interaction-events-sec-12:

目标（target）
--------------------

.. list-table::
   :header-rows: 1

   * - 写法
     - 含义
   * - ``self`` 或缺省
     - 当前事件源 view
   * - ``./child/path``
     - 相对当前事件源 view
   * - ``/absolute/path``
     - 当前 document 内绝对路径

补充说明：

- 同一个 document 内，静态声明的非空 ``action`` 必须全局唯一。
- 同一个模板定义生成的多个实例可以共用同一个 ``action``。
- 模板实例共用 ``action`` 时，建议优先使用 ``event.path`` 区分具体实例，``event.node_id`` 用于区分模板内部触发节点。
- ``subscribe_event_action(...)`` 返回 ``ScopedConnection``；connection 析构后自动断开。
- ``subscribe_event_action_with_id(...)`` 返回 ``SubscriptionId``；它只是订阅身份，不影响 ``document_id + action`` 路由模型。

.. _gui-interface-json_ui-interaction-events-sec-13:

示例
--------------------

.. code-block:: json

   "events": [
       {
           "type": "valueChanged",
           "action": "preferences.language"
       }
   ]

按压动画示例：

.. code-block:: json

   "events": [
       {"type": "pressed", "effects": [{"type": "startAnimation", "animationId": "press_down"}]},
       {"type": "pressLost", "effects": [{"type": "startAnimation", "animationId": "press_up"}]},
       {"type": "clicked", "effects": [{"type": "emitAction", "action": "launcher.open", "requireValidPress": true}]}
   ]
