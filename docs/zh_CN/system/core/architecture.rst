.. _system-core-architecture-sec-00:

架构
====================

:link_to_translation:`en:[English]`

本文说明 ``brookesia_system_core`` 的主要对象、启动流程和内部协作关系。

入口对象
--------------------

核心入口是 ``esp_brookesia::system::core::System``，它对外提供：

- 系统生命周期：``init()``、``start()``、``stop()``、``deinit()``。
- 应用管理：``install_app()``、``install_runtime_app()``、``start_app()``、``stop_app()``、``uninstall_app()``。
- 应用自有 GUI 操作：``gui_set_text()``、``gui_trigger_screen_flow()`` 等。
- 定时器操作：``timer_start_periodic()``、``timer_start_delayed()``、``timer_stop()``。
- 运行时调用方 owner 查询：``get_current_runtime_app_owner()``。

内部状态保存在 ``System::Impl`` 中，公共头文件只暴露稳定接口。``System::Impl`` 的内部记录和共享调度 helper 位于 ``src/private/system/impl.hpp``，具体行为按 ``system/``、``app/``、``service/``、``runtime/`` 目录分模块实现。

主流程
--------------------

``System::init()`` 创建并连接基础运行环境：

1. 设置 ``system_type`` 和 ``gui::Environment``。
2. 创建并启动 ``lib_utils::TaskScheduler``，用于 system 内部 app、GUI、timer 调度。
3. 如果配置了 ``gui_backend``，创建 ``gui::Runtime``。
4. 创建 ``SystemHostBridge`` 和 ``runtime::Runtime``。
5. 初始化 ``service::ServiceManager``。
6. 注册并绑定 ``SystemCore``、``SystemGui``、``SystemTimer``。
7. 调用派生类扩展点 ``on_init()``。
8. 根据配置安装注册的原生应用和 package 应用。

``System::start()`` 调用 ``on_start()`` 后进入 started 状态；``System::stop()`` 调用 ``on_stop()`` 并清除 started 状态；``System::deinit()`` 会停止运行中的应用，清理 runtime、GUI、timer、service 和 app 记录。

``System::process_timers()`` 当前保留为兼容入口；timer 由 system 内部 scheduler 自动触发并派发到应用。

如果 ``System::Config::enable_gui_live_preview`` 打开，core 会在 file-backed GUI document 加载后注册 live preview，并在 ``SystemGui`` task group 中启动周期 poll。poll 与 document reload 仍经过 GUI runtime gate 和 backend thread guard，``deinit()`` 或 document unload 时会取消对应轮询状态。

Task 调度
--------------------

``system_core`` 使用一个公共 ``lib_utils::TaskScheduler``，并配置多个串行 task group：

.. list-table::
   :header-rows: 1

   * - Group
     - 用途
   * - ``SystemGui``
     - GUI document load/mount/unload、普通 binding flush、生命周期相关 GUI 操作
   * - ``SystemGuiInput``
     - GUI event dispatch、输入触发的 ``ExecuteBatch``、图片切换和运行时动画
   * - ``SystemApp``
     - app 生命周期、普通 timer 回调
   * - ``SystemAppInput``
     - GUI action 对应的 ``on_action()`` 回调
   * - ``SystemTimer``
     - periodic/delayed timer 触发源

``SystemGui`` 与 ``SystemGuiInput`` 共享同一个 GUI runtime gate 和 backend thread guard，避免 ``gui::Runtime`` 与 LVGL backend 并发访问；``SystemApp`` 与 ``SystemAppInput`` 共享同一个 app callback gate，避免原生应用或运行时应用被 timer 与 action 并发进入。

GUI action 不在 LVGL event callback 栈内直接执行应用逻辑。对于无 payload 的轻量 action，``gui::Runtime`` 使用 fast path 直接触发 action subscription，subscription 只负责投递到 ``SystemAppInput``；对于 ``ValueChanged`` 等需要同步 GUI runtime 状态的事件，backend event 仍先进入 ``SystemGuiInput``，再由 action subscription 投递到 ``SystemAppInput``。

Impl 记录模型
--------------------

``Impl::AppRecord`` 保存每个应用的运行状态：

- ``AppInfo info``。
- 原生应用实例和 ``AppContext``。
- 应用自有 GUI ``document_id``。
- runtime 内部 app id。
- action connection 列表。
- 已注册 image/font 资源 id。
- timer id 到 timer 名称的映射。
- 当前已挂载 screen path。
- runtime 和 GUI 是否已加载的标记。

core 通过 ``AppRecord`` 统一处理原生应用与运行时应用的差异。

源码模块
--------------------

``system_core`` 的实现按功能拆分：

.. list-table::
   :header-rows: 1

   * - 源文件
     - 负责内容
   * - ``system/core.cpp``
     - ``System`` 构造/析构和默认 virtual callbacks
   * - ``system/lifecycle.cpp``
     - ``init/start/stop/deinit/process_timers`` 主流程
   * - ``system/task.cpp``
     - ``TaskScheduler`` group、GUI/app gate、同步/异步 task helper
   * - ``system/gui.cpp``
     - 应用自有 GUI、system-only GUI、action subscription、binding 合并
   * - ``system/gui_access.cpp``
     - 派生 ``System`` 使用的 system-only GUI facade
   * - ``system/resources.cpp``
     - 原生应用 image/font 资源自动注册和反注册
   * - ``system/timer.cpp``
     - timer 创建、停止、取消、pending 合并和 ``on_timer`` 派发
   * - ``app/app.cpp``
     - app-facing runtime wrapper 的基础实现
   * - ``app/manager.cpp``
     - app 安装、启动、停止、暂停、恢复和查询
   * - ``app/package.cpp``
     - ``.bpk`` 解包、package manifest 解析、``AppManifest`` / ``AppConfig`` 映射
   * - ``app/package_scan.cpp``
     - internal/external storage 的 ``apps`` 目录扫描与重复 manifest id 过滤
   * - ``system/storage.cpp``
     - internal/external storage layout、app/private/public 目录和受限文件操作
   * - ``runtime/runtime.cpp``
     - runtime 初始化、load/unload、``brookesia_app.*`` 调用和 owner 映射
   * - ``runtime/host_bridge.cpp``
     - ``SystemHostBridge`` native module 和 runtime app service bridge
   * - ``service/*.cpp``
     - ``SystemCore``、``SystemGui``、``SystemTimer`` schema、handler 和 service helper

Public Include 模块
--------------------

``include/brookesia/system_core`` 保留顶层兼容头，同时提供按模块拆分的细粒度头：

.. list-table::
   :header-rows: 1

   * - 入口
     - 负责内容
   * - ``app.hpp``
     - 原生应用作者常用 umbrella，聚合 app 类型、context、runtime wrapper 和 ``IApp``
   * - ``system.hpp``
     - 派生 ``System`` 常用 umbrella，聚合 batch、system-only GUI facade 和 ``System``
   * - ``service_api.hpp``
     - service helper 和 service 类 umbrella
   * - ``package.hpp``
     - package 解包、manifest 读取和 ``runtime::AppConfig`` 映射 umbrella
   * - ``app/*.hpp``
     - app 类型、``AppGuiRuntime``、``AppTimerRuntime``、``SystemApi``、``AppContext``、``IApp``、package API
   * - ``system/*.hpp``
     - ``GuiBatch*``、``SystemGuiAccess``、storage layout、``System``
   * - ``service/*.hpp``
     - ``SystemCore``、``SystemGui``、``SystemTimer`` helper 和 service 类

仓库内实现文件优先 include 细粒度头；外部代码可继续 include 顶层兼容头。

模块协作
--------------------

.. list-table::
   :header-rows: 1

   * - 模块
     - 在 system core 中的职责
   * - ``gui::Runtime``
     - 加载 JSON UI document、注册 runtime image/font、挂载 screen、查找和更新 view
   * - ``runtime::Runtime``
     - 加载脚本应用，调用 ``brookesia_app.*`` 生命周期函数
   * - ``SystemHostBridge``
     - system_core 私有 host bridge，让托管的运行时应用通过 native module 调用系统服务
   * - ``service::ServiceManager``
     - 承载 ``SystemCore``、``SystemGui``、``SystemTimer``
   * - ``lib_utils::TaskScheduler``
     - 实现 system 内部 GUI、app、timer 调度
   * - ``IAppProvider`` registry
     - 提供编译期链接进工程的原生应用 provider

扩展点
--------------------

派生系统可以覆盖：

- ``on_init()``
- ``on_start()``
- ``on_stop()``
- ``on_deinit()``
- ``on_app_installed()``
- ``on_app_started()``
- ``on_app_stopped()``

``system_super`` 就通过这些扩展点安装 shell app、挂载 overlay，并在普通应用停止后恢复 shell。
