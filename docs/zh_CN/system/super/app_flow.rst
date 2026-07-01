.. _system-super-app_flow-sec-00:

应用流程
====================

:link_to_translation:`en:[English]`

本文描述 Super 从启动 Shell 到打开、关闭普通 app 的流程。

.. _system-super-app_flow-sec-01:

启动 Super
--------------------

``super::System::start()`` 进入 core ``System::start()``，随后调用 Super 的 ``on_start()``：

1. ``stopping_ = false``。
2. ``restore_shell()`` 启动 Shell。
3. core 根据 Shell manifest 自动启动三个 flow：``background -> SystemBottom``、``shell_pages -> AppDefault``、``overlay -> AppTop + Stack + z_order 101``。Shell ``on_start()`` 随后加载 theme、订阅 overlay action，并生成 launcher 数据。

此时 active app 是 Shell，overlay 只显示顶部状态栏；App Launcher 负责显示应用网格和数量统计。

.. _system-super-app_flow-sec-02:

打开普通 app
--------------------

用户点击 launcher button 后：

1. JSON UI 触发 ``super.launch.app``。
2. Shell 根据事件 path 找到目标 ``AppId``。
3. Shell 播放 app launch modal 和 system UI 收起动画。
4. 动画完成后 Shell 调用 ``owner_.start_app(app_id)``。
5. core 启动目标 app，并把它设为 active app。
6. ``on_app_started()`` 通知 Shell 当前 foreground app，并通过 Shell-managed overlay 切换 app 背景与系统状态栏。

普通 app 的主 screen 默认挂载在 ``AppDefault``，会自然替换 Shell 页面。Shell background flow 仍在 ``SystemBottom``，但会从 Shell 图片背景切换到纯色 app 背景；overlay 以 ``AppTop`` stack screen 保持在该层最上方。

.. _system-super-app_flow-sec-03:

关闭 active app
--------------------

用户从屏幕底部边缘向上拖拽，触发 overlay gesture indicator 并保持到达阈值后：

1. Shell 通过 Display service ``TouchGesture`` 事件跟踪底部上滑距离。
2. indicator bar 随拖拽逐渐缩短，达到阈值并保持一小段时间后触发关闭。
3. Shell 查询 ``get_active_app()``。
4. 如果 active app 不是 shell，则把 pending Shell page 设为 App Launcher，再调用 ``stop_app(active.app_id)``。
5. core 停止 app、清理 GUI/timer/runtime/resources。
6. ``on_app_stopped()`` 调用 ``restore_shell()``，显示 App Launcher。

如果中途松开且未达到退出条件，indicator bar 会回弹并隐藏，active app 继续运行。

.. _system-super-app_flow-sec-04:

Shell 内 page 切换
--------------------

Shell 内 page 切换由 Shell 自身逻辑触发。如果 Shell 是 active app，system 直接调用 ``ShellApp::show_page()`` 在同一 app document 内切换 screen。如果普通 app 是 active app，system 先记录目标 page 并停止普通 app；``on_app_stopped()`` 随后恢复 Shell，并显示目标 page。

.. _system-super-app_flow-sec-05:

恢复 Shell
--------------------

``restore_shell()`` 当前逻辑：

1. 如果 shell id 无效，直接 no-op。
2. 如果 Shell 已经 ``Running`` 或 ``Paused``，直接切换到 pending page 并展开 overlay。
3. 如果 Shell 尚未运行，启动 Shell 后再触发 pending page。

Shell 常驻运行，因此普通 app 停止后不需要重启 Shell；App Launcher 数据在 Shell 启动时生成。

.. _system-super-app_flow-sec-06:

系统停止
--------------------

``on_stop()`` 会设置 ``stopping_ = true``。Shell stop 时 core 会停止 manifest 中的 screen flows 并卸载 Shell GUI document。``stopping_`` 用于避免系统停止过程中 app stop 又触发 restore shell。
