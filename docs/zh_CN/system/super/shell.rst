.. _system-super-shell-sec-00:

Shell
====================

:link_to_translation:`en:[English]`

Shell 是 ``system_super`` 内置的 Native app，类名为 ``ShellApp``。它是系统内置 UI 的 app 容器，内部包含 Home Dashboard、App Launcher 和 Notifications 三个 page。

概述
--------------------

status bar 和底部上滑退出 app 的 gesture indicator 也由 ShellApp 管理，但它们挂载到 ``GuiLayer::Top`` 的 stack 顶层，``z_order = 101``，不会被普通 app 的 replace screen 覆盖。

Shell 还拥有一个 background flow，从同一个 Shell document 挂载到 ``GuiLayer::Bottom``。Shell 前台时显示 ``/background`` 图片背景，普通 app 前台时显示 ``/app_background`` 纯色背景。

Manifest
--------------------

Shell manifest 当前关键字段：

.. list-table::
   :header-rows: 1

   * - 字段
     - 值
   * - ``id``
     - ``brookesia.system.super.shell``
   * - ``name``
     - ``Shell``
   * - ``kind``
     - ``AppKind::Native``
   * - ``visible``
     - ``false``
   * - ``icon_id``
     - ``super``
   * - ``resource_dir``
     - ``BROOKESIA_SYSTEM_SUPER_RESOURCE_DIR``
   * - ``get_gui_descriptor().root``
     - ``shell/root.json``
   * - ``get_gui_descriptor().screen_flows``
     - ``background -> SystemBottom``、``shell_pages -> AppDefault``、``overlay -> AppTop + Stack + z_order 101``、``keyboard -> AppTop + Stack + z_order 102``

``visible = false`` 保证 Shell 自己不会出现在 launcher app 列表中。

Pages
--------------------

Shell app 一次加载 ``shell/root.json``，该 root 聚合 2 个 background screen 和 3 个 content screen：

.. list-table::
   :header-rows: 1

   * - page
     - screen path
   * - Desktop Background
     - ``/background``
   * - App Background
     - ``/app_background``
   * - Home Dashboard
     - ``/home``
   * - App Launcher
     - ``/launcher``
   * - Notifications
     - ``/notifications``

``/background`` 和 ``/app_background`` 由 ``background`` flow 挂到 ``SystemBottom``，随前台 app 状态互斥切换。3 个 content screen 由 ``shell_pages`` flow 管理，state id 直接使用 screen id：core 会在 Shell ``on_start()`` 前自动启动该 flow 到 ``AppDefault``，导航热键触发 flow action 完成互斥切换。普通 app 作为 active app 时，其默认 ``AppDefault`` screen 会自然替换 Shell 页面；返回 Shell 时，system 会先关闭普通 app，再让常驻 Shell 触发目标 page transition。

Overlay
--------------------

Shell root 内包含 ``/overlay`` screen。``overlay`` flow 由 Shell manifest 挂到 ``AppTop``，使用 ``Stack`` mount mode 和 ``z_order = 101`` 保持在普通 app top screen 之上。ShellApp 只负责更新 binding、订阅 Display/Wi-Fi 事件和运行动画。overlay 的节点和行为详见 :doc:`overlay`。

App Launcher
--------------------

App Launcher page 启动时显示 ``/launcher`` 并调用 ``populate_launcher()`` 生成 launcher button。``populate_launcher()`` 会遍历 ``owner_.list_apps()``：

- 跳过 ``manifest.visible = false`` 的 app。
- 基于模板 ``launcher_app_button`` 在 ``/launcher/content/grid`` 下创建实例。
- 实例 id 形如 ``app_<app_id>``。
- 把实例 label 设置为 app name。
- 如果 ``manifest.icon_id`` 和 ``manifest.icon_path`` 可用，把实例 image icon 设置为以 ``manifest.id`` 注册的全局 image resource。
- 显示真实 image icon 时隐藏 fallback 首字符；无真实 image icon 时显示 app name 的首个字母或数字 fallback。
- 建立实例 id 到 ``AppId`` 的映射。
- 设置 ``/launcher/content/summary_badge/summary`` 为可启动 app 数量。

启动 action
--------------------

launcher button root 使用唯一 action ``super.launch.app``。内部 icon/label 子节点不可点击，避免抢走 root button 的 press/click event；点击 button 空白区域、icon 图片、fallback 首字符或 app 名称都会根据事件 path 解析实例 id，再调用 ``owner_.start_app(app_id)``。button root 同时通过 JSON UI event effects 播放本地 press/release 动画。若按住后触发 ``pressLost``，随后 ``clicked`` 不会派发 ``super.launch.app``，因此滑出控件后不会误启动 app。

Theme
--------------------

Shell UI 通过 Runtime theme 管理 light/dark 风格：``resource/themes/light.json``（theme id ``shell.light``）和 ``resource/themes/dark.json``（theme id ``shell.dark``）。Shell root 加载 ``constants/default.json``，并在匹配分辨率时加载 ``constants/1024x600.json``。这些 ``ui`` token 只负责布局、尺寸和少量语义色；卡片、标题、caption、导航栏、状态栏等通用视觉样式由 theme 中的命名样式提供，并通过 JSON 节点的 ``styleRefs`` 引用。

Shell 还会在启动时自动加载 ``BROOKESIA_SYSTEM_SUPER_RESOURCE_DIR/themes`` 第一层下所有 ``*.json`` Runtime theme，并设置默认 theme 为 ``shell.light``。Theme 不写入 ``shell/root.json.assets``；它通过 C++ native-only GUI API 注册到 Runtime。全局 light/dark 切换由 Settings app 的 Display 页面触发，切换时 Runtime 重新合成已加载节点的 style，Shell background 也会通过 theme variant 更新。
