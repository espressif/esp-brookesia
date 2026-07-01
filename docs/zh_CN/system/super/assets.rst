.. _system-super-assets-sec-00:

资源
====================

:link_to_translation:`en:[English]`

system super 当前主要 JSON UI 资源集中在 Shell 目录，按 ``constants + screens + flows + templates`` 组织。

.. _system-super-assets-sec-01:

概述
--------------------

Shell 资源目录结构：

- ``resource/shell/root.json``：Shell app 唯一入口，加载 constants、生成后的 ``imageSet`` descriptor、screen、flow 和模板。
- ``resource/shell/constants/*.json``：默认与分辨率覆盖 token。
- ``resource/shell/screens/*.json``：Background、Home、App Launcher、Notifications、Overlay 和 keyboard input screen。
- ``resource/shell/flows/*.json``：background、content pages 和 overlay 的 ``screenFlow`` asset。
- ``resource/shell/templates/*.json``：动态实例模板。
- ``resource/themes/*.json``：Runtime theme descriptor，由 ShellApp 启动时加载，不写入 ``root.json.assets``。
- ``resource/startup/root.json``：system core 在 Shell 加载前挂到 ``SystemTop`` 的开机界面。

图片源图位于 ``assets/shell/images/**``。构建时 CMake 先把 ``resource/`` 原样 stage 到运行资源根，再把 ``assets/`` 下图片通过 ``brookesia_gui_lvgl_pack_images()`` 转成 LVGL ``.bin + imageSet index.json``：PNG 以 ``ARGB8888`` 输出，JPG/JPEG 以 ``RGB565`` 输出，生成物不提交到源码目录。

.. _system-super-assets-sec-02:

Constants 命名空间
--------------------

Constants 使用 ``ui`` 作为 Shell 内部 token 命名空间：

.. list-table::
   :header-rows: 1

   * - 命名空间
     - 用途
   * - ``${constant.ui.color...}``
     - light 语义色，例如 surface、border、accent、状态色和 overlay 专用色
   * - ``${constant.ui.metric...}``
     - 页面标题、caption、卡片圆角等需要显式引用的共享 metric
   * - ``${constant.ui.content.metric...}``
     - content page 的 work area、launcher grid、chip、search 等尺寸
   * - ``${constant.ui.overlay.metric...}``
     - status bar、gesture indicator 和 overlay 动画尺寸

默认字体、基础文本色、透明 screen/container 背景、button/text input 的默认边框等通用外观由 ``resource/themes/*.json`` 提供；constants 保留给需要在具体节点中显式引用的语义色、字号和布局尺寸。

.. _system-super-assets-sec-03:

Runtime theme
--------------------

Theme descriptor 是 Runtime 全局资源，不是 Shell document asset。ShellApp 启动时扫描 ``<system-root>/super/themes`` 第一层下所有 ``*.json``，按文件名排序加载，并设置默认 theme 为 ``shell.light``。当前 V1 提供 ``light.json``（``shell.light``）和 ``dark.json``（``shell.dark``）。Shell screen 通过 ``styleRefs`` 引用 ``shell.card``、``shell.pageTitle``、``shell.navButton`` 等命名样式；节点显式写入的 ``style`` 优先级更高。不要把 ``resource/themes/*.json`` 写入 ``root.json`` 的 ``assets``，JSON UI parser 会拒绝 document asset 类型 ``theme``。

.. _system-super-assets-sec-04:

Shell Root
--------------------

Shell root 定义 Shell background screens、content pages、overlay 和 launcher button 模板。关键节点：

.. list-table::
   :header-rows: 1

   * - 节点或模板
     - 路径或 id
     - C++ 常量
   * - Desktop background screen
     - ``/background``
     - ``SUPER_BACKGROUND_SCREEN_PATH``
   * - App background screen
     - ``/app_background``
     - JSON screen id
   * - Home Dashboard screen
     - ``/home``
     - ``SUPER_HOME_SCREEN_PATH``
   * - App Launcher screen
     - ``/launcher``
     - ``SUPER_APP_LAUNCHER_SCREEN_PATH``
   * - Notifications screen
     - ``/notifications``
     - ``SUPER_NOTIFICATIONS_SCREEN_PATH``
   * - overlay screen
     - ``/overlay``
     - ``SUPER_OVERLAY_SCREEN_PATH``
   * - keyboard input screen
     - ``/keyboard_input``
     - ``SUPER_KEYBOARD_INPUT_SCREEN_PATH``
   * - status bar
     - ``/overlay/status``
     - ``SUPER_STATUS_BAR_PATH``
   * - status Wi-Fi
     - ``/overlay/status/status_right/wifi_pill``
     - ``SUPER_STATUS_WIFI_PATH``
   * - status clock
     - ``/overlay/status/status_right/clock``
     - ``SUPER_STATUS_CLOCK_PATH``
   * - gesture indicator
     - ``/overlay/gesture_indicator``
     - ``SUPER_GESTURE_INDICATOR_PATH``
   * - summary label
     - ``/launcher/content/summary_badge/summary``
     - ``SUPER_LAUNCHER_SUMMARY_PATH``
   * - launcher grid
     - ``/launcher/content/grid``
     - ``SUPER_LAUNCHER_GRID_PATH``
   * - launcher button template
     - ``launcher_app_button``
     - ``SUPER_LAUNCHER_BUTTON_TEMPLATE_ID``

动态实例 instance id 前缀为 ``app_``，path 前缀为 ``/launcher/content/grid/app_``。launcher button root 有效点击后通过 action ``super.launch.app``（``SUPER_ACTION_LAUNCH_APP``）启动 app。

.. _system-super-assets-sec-05:

screen flows
--------------------

Shell root 引用 3 个 flow：``flows/background.json`` 管理 ``/background``（挂到 ``SystemBottom``）；``flows/shell_pages.json`` 管理 Home / App Launcher / Notifications（挂到 ``AppDefault``）；``flows/overlay.json`` 管理 ``/overlay``（挂到 ``AppTop``，使用 ``Stack`` 和 ``z_order = 101``）。``shell_pages`` 的 state id 直接使用 top-level screen id，导航栏 action 直接作为 flow transition action：

.. list-table::
   :header-rows: 1

   * - action
     - 目标 screen
   * - ``super.nav.home``
     - ``/home``
   * - ``super.nav.open_launcher``
     - ``/launcher``
   * - ``super.nav.open_notifications``
     - ``/notifications``

Shell manifest 会自动启动 ``shell_pages`` flow，后续页面切换只触发 flow，不再手动 unmount/mount content screen。

.. _system-super-assets-sec-06:

Startup Overlay
--------------------

``resource/startup/root.json`` 是独立轻量 JSON UI root，由 ``system_core`` 在 GUI runtime 创建后、Shell 和 app 扫描前加载，并通过 transient screen 挂到 ``SystemTop``，用于避免 ESP 端文件解析、资源注册和 Shell 初始化阶段出现长时间空白画面。默认 screen path 为 ``/startup``，可通过 Super Kconfig 或 PC CMake config 修改 root 文件路径。

.. _system-super-assets-sec-07:

Background Screens
--------------------

Background flow 是 Shell root 中的底层背景状态机。``/background`` 是 Shell 桌面图片背景，``/app_background`` 是普通 app 前台时的纯色背景。运行时 Shell manifest 把 ``background`` flow 挂到 ``SystemBottom``：Shell 前台时切到 ``/background``，普通 app 前台时切到 ``/app_background``。content page 仍挂到 app layer，overlay 仍挂到 ``AppTop`` stack 顶层，因此不需要在每个 content screen 内重复添加背景图节点。

.. _system-super-assets-sec-08:

Keyboard Input
--------------------

Keyboard input 是 Shell root 中的按需 transient screen ``/keyboard_input``，不属于任何 Shell screenFlow，app 请求系统键盘时由 ShellApp 通过 transient mount 显示到 ``AppTop`` stack 顶层，并使用比 overlay 更高的 z-order。screen 结构包含全屏遮罩、单行 ``textInput`` 和 ``keyboard``；``abc`` / ``ABC`` / ``123`` / ``,.?!``、确认和取消都内嵌在 keyboard 按键布局中。键盘布局通过 ``keyboardProps.layouts`` 描述 ``text`` / ``upper`` / ``number`` / ``special`` 四种 layout，backend 映射到 LVGL keyboard map，并根据 ``allowedModes`` 禁用不允许的模式切换键。关键 action 包括 ``super.keyboard.text_changed``、``super.keyboard.submit.*``、``super.keyboard.cancel.*`` 和 ``super.keyboard.toggle_password``。

.. _system-super-assets-sec-09:

Modification Notes
--------------------

如果改动 JSON 节点 id、template id、screen path 或 action 字符串，必须同步 ``src/private/system_constants.hpp`` 中对应常量。
