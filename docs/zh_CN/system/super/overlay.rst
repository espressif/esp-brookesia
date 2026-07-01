.. _system-super-overlay-sec-00:

Overlay
====================

:link_to_translation:`en:[English]`

Overlay 是 ShellApp 管理的系统覆盖层，用于承载 status bar、底部上滑退出 app 的 gesture indicator，以及 message dialog、keyboard、app modal 等系统浮层。

概述
--------------------

Overlay 属于 Shell 的系统 UI 能力，资源文件为 ``resource/shell/screens/overlay.json``，物理上位于 Shell 主 document 的 ``/overlay`` screen 中，由 Shell manifest 中的 ``overlay`` flow 挂到 ``AppTop``，使用 ``Stack`` mount mode 和 ``z_order = 101``，映射到 GUI backend 的 ``GuiLayer::Top`` 并保持置顶。普通 app 打开时可以替换 app layer 的 replace screen，但不会覆盖该 stack overlay。

Shell 的桌面背景不属于 overlay。背景 screen ``/background`` 由 ``background`` flow 挂到 ``SystemBottom``，overlay 只负责 status、gesture indicator 和 app modal。

加载和挂载
--------------------

``ShellApp::mount_overlay()`` 执行：

1. Shell app 启动时 core 已加载 ``resource/shell/root.json``。
2. core 已根据 Shell manifest 自动启动 ``overlay`` flow 并挂载 ``/overlay``。
3. ShellApp 初始化 status、Wi-Fi 状态、时钟定时器和 Display touch gesture 订阅。

因此 overlay 不会被普通 app 的 ``GuiLayer::Default`` 或普通 ``GuiLayer::Top`` replace screen 覆盖。

Status 与 Gesture Indicator
---------------------------

当前 overlay 只有一个 screen：

.. list-table::
   :header-rows: 1

   * - 节点
     - 行为
   * - ``/overlay/status``
     - 顶部状态栏
   * - ``/overlay/status/status_right/wifi_pill``
     - Wi-Fi 已连接时显示
   * - ``/overlay/status/status_right/clock``
     - 本地时间
   * - ``/overlay/gesture_indicator``
     - 底部上滑退出 app 的进度指示
   * - ``/overlay/gesture_indicator/bar``
     - 随上滑 progress 缩短的 indicator bar

status 只保留 Wi-Fi 标志和时间。Wi-Fi service 不可用或未连接时隐藏 Wi-Fi 标志。时间在 Shell 启动时刷新一次，并通过 Shell timer 周期刷新。

Gesture indicator 默认隐藏。普通 app 前台时，如果 Display service 上报从 BottomEdge 开始的 Up 方向手势，Shell 会临时切换 active display source 到 GUI，显示 indicator bar，并根据拖拽距离缩短 bar 宽度。达到退出阈值并保持 ``gestureExitHoldMs`` 后，Shell 关闭当前 foreground app。中途松开则播放回弹动画并隐藏 indicator。

Actions
--------------------

当前 overlay 不再提供 nav/close/hide/handle action。Shell page 切换由 Shell 自身逻辑触发，普通 app 退出由 Display service ``TouchGesture`` 驱动。JSON UI 要求同一 document 内 action owner 唯一；overlay 保留 keyboard、message dialog、app modal 等子树自己的 action。

close
--------------------

当前行为：

- 没有 active app：no-op。
- active app 是 shell：no-op。
- active app 是普通 app：底部上滑达到阈值后，把 pending Shell page 设为 App Launcher，再调用 ``stop_app(active.app_id)``。

普通 app 停止后，``on_app_stopped()`` 会触发 Shell 恢复流程并显示 App Launcher。

卸载
--------------------

``ShellApp::unmount_overlay()`` 会断开 overlay action connections、断开 Display/Wi-Fi event connections、停止 system bar/gesture indicator/clock timer。Shell app stop 后由 core 停止 manifest 中的 ``overlay`` flow 并卸载 ``/overlay``，Shell 主 document 由 core 在 Shell app stop 时统一 unload。
