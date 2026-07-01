.. _system-core-services-sec-00:

系统服务
====================

:link_to_translation:`en:[English]`

system core 当前注册三个基础 service：``SystemCore``、``SystemGui``、``SystemTimer``。运行时应用通过 system core 私有的 ``SystemHostBridge`` 调用这些 service，service 侧会根据当前 runtime caller 判定 owner app，从而把 GUI 和 timer 操作限制在调用方 app 内。

服务名
--------------------

.. list-table::
   :header-rows: 1

   * - 宏
     - service 名
   * - ``BROOKESIA_SYSTEM_CORE_SERVICE_NAME``
     - ``SystemCore``
   * - ``BROOKESIA_SYSTEM_CORE_GUI_SERVICE_NAME``
     - ``SystemGui``
   * - ``BROOKESIA_SYSTEM_CORE_TIMER_SERVICE_NAME``
     - ``SystemTimer``

权限边界
--------------------

运行时应用可以查询系统信息、只能操作自己的 GUI document、只能管理自己的 timer，且不能通过 service 启动或停止其他 app。原生应用通过 ``AppContext`` 直接调用 ``SystemApi``、``AppGuiRuntime``、``AppTimerRuntime``，当前被视为系统内置可信 app。

Core 服务
--------------------

``SystemCore`` 是基础系统 service，service 名固定为 ``SystemCore``。

.. list-table::
   :header-rows: 1

   * - 函数
     - 说明
   * - ``GetSystemType``
     - 返回当前 system type，例如 ``"core"`` 或 ``"super"``
   * - ``GetEnvironment``
     - 返回当前 ``gui::Environment`` 的 JSON 对象
   * - ``GetActiveApp``
     - 返回当前 active app 信息；没有时返回空结果
   * - ``ListApps``
     - 返回已安装 app 列表
   * - ``RequestCloseApp``
     - 请求关闭 app
   * - ``StartApp`` / ``StopApp``
     - 启动或停止指定 app
   * - ``ShowLoading`` / ``HideLoading``
     - 显示或隐藏调用方 app 的 loading overlay
   * - ``ShowKeyboard`` / ``HideKeyboard``
     - 显示或取消调用方 app 的系统键盘请求

事件包括 ``AppChanged``（app 状态变化）和 ``KeyboardClosed``（键盘输入完成或取消，包含 ``RequestId``、``AppId``、``Confirmed``、``Text``）。运行时普通 app 允许调用查询类函数，``RequestCloseApp``、``ShowLoading`` / ``HideLoading``、``ShowKeyboard`` / ``HideKeyboard`` 只作用于自己，``StartApp`` 和 ``StopApp`` 会被拒绝；原生应用建议通过 ``AppContext::system_service()`` 使用 ``SystemApi``。

运行时应用可调用 ``ShowKeyboard`` 请求系统键盘，``Options`` 支持 ``title``、``placeholder``、``initial_text``、``max_length``、``password``、``mode``（``text`` / ``upper`` / ``number`` / ``special``）和 ``allowedModes``。如果 ``mode`` 不在 ``allowedModes`` 中，Shell 自动使用 ``allowedModes`` 的第一项；未设置时默认允许全部四种布局。用户确认或取消后通过 ``KeyboardClosed`` event 返回 ``RequestId``、``AppId``、``Confirmed`` 和 ``Text``。

``GetEnvironment`` 返回 ``width_px``、``height_px``、``density``、``font_scale``、``language``、``theme_id``。``GetActiveApp`` 和 ``ListApps`` 基于 ``AppInfo``，包含 ``app_id``、``manifest``、``state``、``last_error``。

GUI 服务
--------------------

``SystemGui`` 是运行时应用操作自身 GUI document 的受限 service，service 名固定为 ``SystemGui``。

.. list-table::
   :header-rows: 1

   * - 函数
     - 参数
     - 说明
   * - ``SetBinding`` / ``SetBindings``
     - ``Path``/``Key``/``Value`` 或 ``Updates``
     - 设置或批量设置 binding 值
   * - ``GetBinding``
     - ``Path``、``Key``
     - 读取 binding 值
   * - ``GetConstant``
     - ``Path``
     - 读取调用方 app GUI document 的 constant 值
   * - ``SetText`` / ``SetValue`` / ``SetChecked``
     - ``Path`` 与对应值
     - 更新 label、数值控件、开关/勾选控件
   * - ``CreateView`` / ``DestroyView``
     - ``TemplateId``/``ParentPath``/``InstanceId`` 或 ``Path``
     - 从模板创建或销毁动态 view
   * - ``SubscribeAction``
     - ``Action``
     - 订阅 GUI action，由 core 转发到 app 生命周期
   * - ``TriggerScreenFlow`` / ``GetScreenFlowState``
     - ``ScreenFlow``、``Action``
     - 触发 screen flow transition 或查询当前 state
   * - ``GetViewFrame``
     - ``Path``
     - 查询 view 当前 frame
   * - ``SetViewSrc``
     - ``Path``、``Src``
     - 切换 image view 的图片资源 id
   * - ``StartViewAnimation`` / ``StopAnimation``
     - ``Path``/``Animation`` 或 ``AnimationId``
     - 启动或停止 view 运行时动画
   * - ``ExecuteBatch``
     - ``Commands``
     - 在同一个 SystemGui task 中顺序执行 GUI 命令

``SystemGui`` 的所有函数都隐式作用于调用方 runtime app 的 GUI document，因此 API 中没有 ``DocumentId``、layer、mount target、``LoadFile`` / ``LoadJson`` / ``Unload`` 以及 view debug / live preview 等接口。``GetConstant`` 的 ``Path`` 使用 ``${constant.<path>}`` 中的点路径部分，返回值以 ``{"Value": ...}`` envelope 保留任意 JSON 类型。

为减少输入延迟，core 将 GUI 操作分成两类：

.. list-table::
   :header-rows: 1

   * - 类型
     - 函数
     - 调度
   * - 高频普通更新
     - ``SetBinding``、``SetBindings``
     - 合并到普通 binding flush，最后值胜出
   * - 输入敏感更新
     - ``ExecuteBatch``、``SetViewSrc``、``StartViewAnimation``、``StopAnimation``
     - 进入 ``SystemGuiInput``，优先于普通 binding flush

变更类函数大多是异步语义，返回成功表示请求已被接收，不代表 LVGL 已完成绘制；查询类函数会同步进入 GUI task 后返回。``ExecuteBatch`` 用于需要严格顺序和低调度开销的多步更新，例如“停止旧动画、设置 binding、再从当前坐标启动新动画”：

.. code-block:: json

   {
       "Commands": [
           {"Name": "StopAnimation", "Params": {"AnimationId": 12}},
           {"Name": "SetBindings", "Params": {"Updates": [{"Path": "/screen/bird", "Key": "angle", "Value": "-25"}]}},
           {"Name": "StartViewAnimation", "Params": {"Path": "/screen/bird", "Animation": {"property": "Y", "fromMode": "Current", "toMode": "Relative", "to": -48, "duration": 200}}}
       ]
   }

v1 支持 ``SetBindings``、``SetViewSrc``、``StopAnimation``、``StartViewAnimation`` 命令，按数组顺序执行，失败时停止后续命令，返回值在 ``Results`` 中带每条命令结果。

运行时应用必须在 ``profile.json`` 的 ``screen_flows[]`` 中声明启动 flow，core 会在 ``on_start()`` 前自动启动。``TriggerScreenFlow(ScreenFlow, Action)`` 按当前 state 和 transition action 切换 screen，精确 ``from[]`` transition 优先于任意来源 transition；如果 ``transitions`` 为空，document 仍挂载 ``initial`` 但 ``TriggerScreenFlow`` 返回错误。运行时应用只能使用 ``Replace`` mount mode，``z_order`` 在 ``0..100`` 内，不能指定 ``DocumentId``、layer 或其他 app 的 flow。

``SubscribeAction(action)`` 订阅当前 app document 中的 GUI action，触发后原生应用进入 ``IApp::on_action()``，运行时应用进入 ``brookesia_app.on_action()``。action 不在 LVGL callback 栈中直接执行：事件先进入 ``SystemGuiInput``，再投递到 ``SystemAppInput``，与普通 timer 回调共享 app callback gate。

Timer 服务
--------------------

``SystemTimer`` 提供运行时应用可用的 app-owned timer，service 名固定为 ``SystemTimer``。

.. list-table::
   :header-rows: 1

   * - 函数
     - 参数
     - 说明
   * - ``StartPeriodic``
     - ``Name``、``IntervalMs``
     - 创建周期 timer，返回 ``TimerId``
   * - ``StartDelayed``
     - ``Name``、``DelayMs``
     - 创建一次性延迟 timer，返回 ``TimerId``
   * - ``Stop``
     - ``TimerId``
     - 停止调用方 app 拥有的 timer

``IntervalMs`` / ``DelayMs`` 必须大于 ``0``。timer 触发后运行时应用会收到 ``brookesia_app.on_timer(timer_id, name)``；app 不在 ``Running`` 状态时 pending timer 不会派发。同一个 periodic timer 已在 pending 队列时，core 会合并后续 tick，避免脚本或 GUI 刷新慢于 timer 周期时无限积压；delayed timer 不做合并。

原生应用通过 ``AppContext::timer()`` 使用 ``AppTimerRuntime`` 的 ``start_periodic()``、``start_delayed()``、``stop()``。core 会在 ``stop_app()``、``uninstall_app()`` 和 ``deinit()`` 时自动取消 app timer，并从 pending 队列中移除未派发事件。

相关文档
--------------------

- :doc:`gui`
- :doc:`resources_permissions`
- :doc:`app_model`
