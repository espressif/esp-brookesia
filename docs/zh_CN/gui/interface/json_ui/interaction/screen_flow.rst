.. _gui-interface-json-ui-interaction-screen-flow-sec-00:

屏幕流
======================================

:link_to_translation:`en:[English]`

.. _gui-interface-json_ui-interaction-screen_flow-sec-01:

概览
--------------------

``screenFlow`` 是 document asset，用于描述同一 document 内一组 top-level screen 的互斥切换状态机。一个
document 可以声明多个 flow，但同一个 screen 只能归属一个 flow。flow 只负责 screen 的
``mount`` / ``unmount``，不执行脚本回调，不跨 document，也不管理 app 生命周期。

.. _gui-interface-json_ui-interaction-screen_flow-sec-02:

相关文档
--------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../assets/index`
- :doc:`../runtime`

.. _gui-interface-json_ui-interaction-screen_flow-sec-03:

Asset 字段表
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
     - 固定为 ``"screenFlow"``
   * - ``id``
     - string
     - 无
     - 是
     - flow id，document 内唯一
   * - ``screens``
     - string[]
     - 无
     - 是
     - 本 flow 管理的 top-level screen id 列表，不带 ``/``
   * - ``initial``
     - string
     - 无
     - 是
     - 初始 state id，必须在 ``screens`` 中
   * - ``transitions``
     - object[]
     - ``[]``
     - 否
     - transition 列表；缺省或空数组表示仅声明静态 initial screen

.. _gui-interface-json_ui-interaction-screen_flow-sec-04:

transitions[]
--------------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - 是否必填
     - 说明
   * - ``from``
     - string[]
     - ``[]``
     - 否
     - 来源 state 列表；缺省或空数组表示任意当前 state
   * - ``action``
     - string
     - 无
     - 是
     - transition action 名
   * - ``to``
     - string
     - 无
     - 是
     - 目标 state，必须在 ``screens`` 中

规则：

- state id 直接使用 top-level screen 的 ``id``，例如 screen ``/home`` 对应 state ``home``。
- ``screens`` 中的每个 state 都必须引用当前 document 中已有 top-level screen。
- 同一 top-level screen 不能被多个 ``screenFlow`` 的 ``screens`` 包含。
- ``initial``、``to``、``from[]`` 中的 state id 必须属于当前 flow 的 ``screens``。
- 缺省 ``from`` 或 ``from: []`` 表示任意当前 state。
- 同一 ``(from_state, action)`` 只能定义一次；多个空 ``from`` 且 action 相同也会报错。
- 精确 ``from`` 的 transition 优先于空 ``from`` 的任意来源 transition。
- 触发到当前 state 且当前 screen 仍挂载在 flow target 上时视为 no-op 成功。
- 如果 ``transitions`` 缺省或为空，flow 仍合法；启动 flow 时会 mount ``initial`` screen，但触发 transition 会失败。

.. _gui-interface-json_ui-interaction-screen_flow-sec-05:

示例
--------------------

``root.json``：

.. code-block:: json

   {
       "version": "0.1.1",
       "assets": [
           "screens/home.json",
           "screens/settings.json",
           "flows/main.json"
       ]
   }

``flows/main.json``：

.. code-block:: json

   {
       "type": "screenFlow",
       "id": "main",
       "screens": ["home", "settings"],
       "initial": "home",
       "transitions": [
           {"from": [], "action": "open_home", "to": "home"},
           {"from": ["home", "settings"], "action": "open_settings", "to": "settings"}
       ]
   }

.. _gui-interface-json_ui-interaction-screen_flow-sec-06:

运行时 API
--------------------

- ``Runtime::start_screen_flow(document_id, flow_id, target)``：启动 flow 并 mount ``initial`` screen；若该 flow 已在运行，则视为幂等成功，不会重复 mount。
- ``Runtime::trigger_screen_flow(document_id, flow_id, action)``
- ``Runtime::stop_screen_flow(document_id, flow_id)``
- ``Runtime::get_screen_flow_state(document_id, flow_id)``
- ``Runtime::has_screen_flow(document_id, flow_id)``：仅判断 document 内是否**定义**了该 flow，不代表 flow 正在运行。

``screenFlow`` 不声明 layer。``target`` 由调用 ``start_screen_flow(...)`` 的宿主指定；未指定时使用默认 display 的
``GuiLayer::Default``。
