.. _gui-interface-json-ui-interaction-bindings-sec-00:

绑定
================================

:link_to_translation:`en:[English]`

概览
--------------------

``bindings`` 是可选 object，用于把 Runtime bindings 中的字符串值应用到当前节点的叶子字段。

相关文档
--------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../styling/props/index`
- :doc:`../runtime`

字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - 说明
   * - ``<fieldPath>``
     - string
     - 无
     - 公开 JSON 的 ``camelCase`` 点路径，只允许叶子字段
   * - value
     - string
     - 无
     - 当前节点路径作用域下的本地 store key，不带前缀

规则
--------------------

- ``bindings`` key 必须是公开 JSON 的 ``camelCase`` 点路径，只能指向叶子字段。
- ``bindings`` value 是当前节点路径作用域下的本地 store key，不带前缀。
- 相同 local key 可安全复用于不同节点；真实作用域是 ``document_id + absolute_path + local_key``。
- props 域绑定是否可用，取决于当前控件类型。

为什么公开接口必须带 absolute_path
------------------------------------------------

- 同一个 document 下，多个静态节点可以复用同一个 local key。
- 同一个模板定义创建出的多个实例，也会复用同名 local key。
- ``document_id`` 只能定位 document，不能唯一定位某个模板实例中的具体节点。
- ``absolute_path`` 才能唯一指定：
  - 哪个静态节点
  - 哪个模板实例中的哪个节点
- 因此 bindings 的正式公开定位是 ``document_id + absolute_path + local_key``，而不是 document 级唯一 key。

常见路径
--------------------

.. list-table::
   :header-rows: 1

   * - 路径
     - 适用控件类型
   * - ``commonProps.hidden`` / ``disabled`` / ``clickable`` / ``scrollable`` / ``pressLock`` / ``angle`` / ``zoom`` / ``pivotX`` / ``pivotY``
     - 全部
   * - ``labelProps.text``
     - ``label`` / ``checkbox``
   * - ``imageProps.src`` / ``recolor`` / ``recolorOpacity`` / ``angle`` / ``offsetX`` / ``offsetY`` / ``zoom`` / ``pivotX`` / ``pivotY``
     - ``image``
   * - ``textInputProps.text`` / ``placeholder`` / ``password`` / ``multiline`` / ``maxLength``
     - ``textInput``
   * - ``rangeProps.value`` / ``min`` / ``max`` / ``step``
     - ``slider`` / ``progressBar`` / ``arc``
   * - ``toggleProps.checked``
     - ``switch`` / ``checkbox``
   * - ``dropdownProps.options`` / ``selectedIndex``
     - ``dropdown``
   * - ``tableProps.rows`` / ``columns`` / ``cells``
     - ``table``
   * - ``lineProps.points``
     - ``line``
   * - ``keyboardProps.mode`` / ``popovers`` / ``allowedModes`` / ``iconSize``
     - ``keyboard``
   * - ``canvasProps.commands``
     - ``canvas``
   * - ``style.imageRecolor`` / ``style.imageRecolorOpacity``
     - 全部；对 ``image`` 节点产生视觉效果
   * - ``style.*``
     - 全部
   * - ``stateStyles.<state>.*``
     - 全部；``state`` 支持 ``pressed`` / ``checked`` / ``focused`` / ``focusKey`` / ``edited`` / ``hovered`` / ``scrolled`` / ``disabled`` / ``user1..user4``
   * - ``partStyles.<part>.*``
     - 支持内部 part 的控件；``part`` 当前支持 ``indicator`` / ``knob``
   * - ``partStyles.<part>.stateStyles.<state>.*``
     - 支持内部 part 的控件；用于 pressed / disabled 等 part 状态样式
   * - ``layout.*``
     - 全部
   * - ``placement.*``
     - 全部

完整 props 字段见 :doc:`../styling/props/index`，style/layout/placement 字段见对应文档。

``stateStyles.<state>.*`` 使用和 ``style.*`` 相同的叶子字段，例如
``stateStyles.pressed.bgColor``、``stateStyles.disabled.opacity``、``stateStyles.checked.borderWidth``。
不支持 ``stateStyles.<state>.font``、``stateStyles.<state>.fontSize`` 和
``stateStyles.<state>.imageFontSize``。

``partStyles`` 使用和 ``style.*`` 相同的叶子字段，例如
``partStyles.indicator.bgGradientColor``、``partStyles.indicator.bgGradientDirection``、
``partStyles.knob.bgColor``、``partStyles.knob.stateStyles.pressed.bgColor``。不支持
``partStyles.<part>.font``、``partStyles.<part>.fontSize`` 和 ``partStyles.<part>.imageFontSize``。

公开 API
--------------------

- ``Runtime::set_binding_value(document_id, absolute_path, key, value)`` 写入单个 binding store；已声明该 binding 的节点会通过其已注册的 listener 重应用对应字段。
- ``Runtime::set_binding_values(document_id, updates)`` 批量写入多个 binding，并显式按节点合并 dirty mask 后统一 apply，减少高频更新时的 backend apply 次数。
- ``Runtime::get_binding_value(document_id, absolute_path, key)`` 可读取当前 scoped binding 值。
- ``Runtime::subscribe_binding_value(document_id, absolute_path, key, listener)`` 可监听某个 scoped binding 的写入结果；listener 签名为 ``(absolute_path, key, value)``。
- ``Runtime::subscribe_binding_value_with_id(document_id, absolute_path, key, listener)`` 会返回稳定的 ``subscription_id``。
- ``Runtime::unsubscribe_subscription(subscription_id)`` 可按 id 主动断开通过 ``with_id`` 获取到的订阅。
- ``View::set_binding_value(key, value)`` / ``View::get_binding_value(key)`` 是当前实例路径下的便捷写法。
- ``subscribe_binding_value(...)`` 返回 ``ScopedConnection``；connection 析构后会自动断开，无需手动 ``unsubscribe(...)``。
- ``subscribe_binding_value_with_id(...)`` 返回 ``SubscriptionId``；适合需要日志、追踪或显式断开订阅的场景。
- 若调用方需要显式断开，可使用 ``Runtime::unsubscribe_subscription(subscription_id)``；这不会改变 ``absolute_path`` 作为 bindings 唯一定位维度的规则。
- ``IDataStore`` 是 Runtime 内部实现细节，不作为 bindings 的公开入口。
- ``bindings`` 属于节点级 scoped 绑定能力，不属于 ``events`` 子能力。

增量更新行为
--------------------

- binding 写入会映射到 props/style/layout/placement 中的具体叶子字段。
- backend 首次创建节点仍完整应用 props、layout、placement 和 style。
- 后续 binding 更新只重应用变化字段所在的 mask，例如 ``imageProps.offsetX`` 不会重放完整 image props，``placement.x`` 不会重复设置 width/height。
- ``placement.x`` / ``placement.y`` 的 binding 值可使用 ``"50%"`` 这类百分比 offset。
- ``placement.width`` / ``placement.height`` 的 binding 值可使用 ``"75%"`` 这类百分比；``placement.aspectRatio``
  可使用 ``"16:9"`` 或数字字符串。
- 批量写入时，同一个节点的多个字段会先合并 mask，再统一 apply。
- 运行时动画直接作用于 backend 节点，不会更新 binding store；如果应用层维护了自己的 binding cache，动画结束后应失效对应 key 或强制写入。

示例
--------------------

.. code-block:: json

   "bindings": {
       "labelProps.text": "status",
       "keyboardProps.iconSize": "keyboardIconSize",
       "style.fontSize": "fontSize",
       "style.imageFontSize": "iconFontSize",
       "stateStyles.pressed.bgColor": "pressedBg",
       "partStyles.indicator.bgGradientColor": "progressEndColor",
       "partStyles.knob.stateStyles.pressed.bgColor": "pressedKnobBg",
       "layout.gap": "gap",
       "placement.width": "width",
       "placement.aspectRatio": "ratio"
   }
