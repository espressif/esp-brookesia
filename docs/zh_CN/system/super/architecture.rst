.. _system-super-architecture-sec-00:

架构
====================

:link_to_translation:`en:[English]`

``esp_brookesia::system::super::System`` 继承 ``esp_brookesia::system::core::System``，在 core 的基础上提供 Super 风格系统壳。

初始化
--------------------

``super::System::init()`` 最终调用 ``init(Config{})``，``init(Config config)`` 会：

1. 将 ``config.core_config.system_type`` 设置为 ``BROOKESIA_SYSTEM_SUPER_SYSTEM_TYPE``。
2. 检查调用方是否通过 ``config.core_config.gui_backend`` 提供 GUI backend。
3. 调用 ``core::System::init()``。

Super 不直接依赖具体 GUI backend，产品工程负责选择 backend（例如 LVGL backend）并传入 ``core_config.gui_backend``。

system type
--------------------

Super 使用 ``BROOKESIA_SYSTEM_SUPER_SYSTEM_TYPE``，当前值为 ``"super"``。``on_init()`` 中也会再次调用 ``set_system_type("super")``，保证 service 查询得到 Super 类型。

扩展点
--------------------

Super 覆盖的 core 扩展点：

.. list-table::
   :header-rows: 1

   * - 函数
     - Super 行为
   * - ``on_init()``
     - 设置 system type，安装内置 shell app
   * - ``on_start()``
     - 启动 Shell app；core 按 Shell manifest 启动 background/content/overlay flows
   * - ``on_stop()``
     - 标记 stopping，停止 Shell 管理的 UI
   * - ``on_deinit()``
     - 清理 shell app id
   * - ``on_app_installed()``
     - 记录 launcher app 可用日志
   * - ``on_app_started()``
     - 记录 foreground app 日志，并根据 active app 切换 Shell background / app background
   * - ``on_app_stopped()``
     - 普通 app 停止后恢复 shell

源码模块
--------------------

``system_super`` 的实现按职责拆分：

.. list-table::
   :header-rows: 1

   * - 文件
     - 职责
   * - ``src/system.cpp``
     - ``System::init()`` 薄入口
   * - ``src/system_lifecycle.cpp``
     - core 扩展点和 Shell app 安装/恢复
   * - ``src/shell_overlay.cpp``
     - ``ShellApp`` 的 overlay binding、status、Wi-Fi、clock 和底部手势退出
   * - ``src/system_navigation.cpp``
     - Shell page 恢复和 active app 关闭
   * - ``src/shell_app.cpp``
     - ``ShellApp``、Shell 多 page 和动态 App Launcher
   * - ``src/private/system_constants.hpp``
     - 内部路径、action 和动画常量
   * - ``src/private/shell_app.hpp``
     - 内部 ``ShellApp`` 声明

内置组成
--------------------

Super 当前内置三个 GUI 部分：

- Shell app：一个 Native ``IApp``，manifest id 为 ``brookesia.system.super.shell``，承载 Home、App Launcher、Notifications 三个 content page。
- background：Shell 主 document 内的桌面背景 screen，由 ``background`` flow 挂到 ``SystemBottom``。
- overlay：Shell 主 document 内的系统覆盖 screen，由 ``overlay`` flow 挂到 ``AppTop + Stack + z_order 101``，负责 top status bar、gesture indicator 和 app modal 等系统浮层。
