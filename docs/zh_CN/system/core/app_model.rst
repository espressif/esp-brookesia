.. _system-core-app_model-sec-00:

应用模型
====================

:link_to_translation:`en:[English]`

本文说明 system core 中应用的基础类型、原生与运行时两种接入方式，以及完整生命周期状态迁移。

.. _system-core-app_model-sec-01:

基础类型
--------------------

.. list-table::
   :header-rows: 1

   * - 类型
     - 说明
   * - ``AppId``
     - core 分配的运行时 app id，``INVALID_APP_ID`` 为 ``0``
   * - ``TimerId``
     - core 分配的 app timer id，``INVALID_TIMER_ID`` 为 ``0``
   * - ``AppKind``
     - app 来源，当前为 ``Native`` 或 ``Runtime``
   * - ``AppState``
     - app 状态，当前为 ``Installed``、``Starting``、``Running``、``Paused``、``Stopping``、``Stopped``、``Error``
   * - ``GuiRootKind``
     - GUI root 来源，当前为 ``None``、``File``、``JsonString``
   * - ``AppManifest``
     - app 安装、显示和 runtime 信息
   * - ``AppGuiDescriptor``
     - GUI root、启动 screen flows 和 GUI 资源
   * - ``AppInfo``
     - ``AppId``、``AppManifest``、状态和最近错误

.. _system-core-app_model-sec-02:

AppManifest
--------------------

.. list-table::
   :header-rows: 1

   * - 字段
     - 说明
   * - ``id``
     - app 唯一 manifest id
   * - ``name``
     - 当前默认展示名
   * - ``localized_names``
     - 多语言展示名，key 为语言，value 为名称
   * - ``version``
     - app 版本，默认 ``"0.1.0"``
   * - ``kind``
     - ``AppKind::Native`` 或 ``AppKind::Runtime``
   * - ``visible``
     - 是否显示在 launcher 等 app 列表中
   * - ``icon_id``
     - 展示图标 image id；runtime package 由 runtime root ``icon_id`` 填充
   * - ``supported_systems``
     - 支持的 system type 列表，空数组表示不限制
   * - ``icon_path``
     - app icon ``imageSet`` descriptor 路径；runtime package 安装时可由 runtime root ``icon_id`` 自动发现后填充
   * - ``runtime_type``
     - runtime backend 类型
   * - ``app_path``
     - runtime package 根目录
   * - ``entry``
     - runtime 入口文件
   * - ``resource_dir``
     - app 资源目录
   * - ``arguments``
     - runtime 启动参数

.. _system-core-app_model-sec-03:

原生应用与运行时应用
--------------------

.. list-table::
   :header-rows: 1

   * - 维度
     - 原生应用
     - 运行时应用
   * - 接入方式
     - C++ 继承 ``IApp``
     - unpacked package manifest + 脚本约定函数
   * - ``AppKind``
     - ``Native``
     - ``Runtime``
   * - 安装来源
     - ``install_app()`` 或 ``IAppProvider`` registry
     - ``install_runtime_app()`` 或 app path 扫描
   * - 生命周期
     - C++ 虚函数
     - ``brookesia_app.*`` 函数
   * - GUI 操作
     - ``AppContext::gui()``
     - ``SystemGui`` service
   * - Timer
     - ``AppContext::timer()``
     - ``SystemTimer`` service
   * - 权限
     - 当前视为系统内置可信 app
     - 普通 app，只能操作自身资源

.. _system-core-app_model-sec-04:

应用生命周期
--------------------

安装。原生应用：``install_app()`` 读取 ``IApp::get_manifest()``，core 强制 ``manifest.kind = AppKind::Native``，读取 ``get_gui_descriptor()`` 并校验启动 screen flow target，分配 ``AppId`` 创建 ``AppContext``，调用 ``IApp::on_install()``，写入 app 表并调用 ``on_app_installed()``。运行时应用：``install_runtime_app()`` 接收 ``AppManifest``，core 强制 ``manifest.kind = AppKind::Runtime``，从 ``<runtime.resource_dir>/profile.json`` 读取 resource descriptor 并校验 ``screen_flows[]``，分配 ``AppId`` 创建 ``AppContext``，写入 app 表并调用 ``on_app_installed()``。

``start_app()`` 的顺序：

1. 状态设为 ``Starting``。
2. 原生应用自动注册 GUI descriptor 中的资源。
3. 根据 GUI descriptor 加载应用自有 GUI document。
4. 如果 descriptor 配置 ``screen_flows[]``，core 自动启动对应 flow，并按逻辑 layer 挂载 ``initial``。
5. 原生应用调用 ``IApp::on_start()``。
6. 运行时应用先 ``load_app()``，再 ``start_app()``，最后调用 ``brookesia_app.on_start()``。
7. 成功后状态设为 ``Running``，更新 active app，并调用 ``on_app_started()``。

如果资源注册、GUI 加载或生命周期失败，core 会卸载已加载 GUI、反注册已注册资源，并把状态设为 ``Error``。

``stop_app()`` 的顺序：

1. 状态设为 ``Stopping``。
2. 取消该 app 所有 timer，并清理 pending timer。
3. 原生应用调用 ``IApp::on_stop()``；运行时应用调用 ``brookesia_app.on_stop()`` 后再停止 runtime app。
4. 断开 action 连接，停止应用自有 running screen flow，unload GUI document。
5. 反注册 app image/font 资源。
6. 成功后状态设为 ``Stopped``；若它是 active app 则清空 active app，并调用 ``on_app_stopped()``。

``uninstall_app()`` 会先停止 ``Running`` 或 ``Paused`` app，然后调用 ``on_uninstall()`` 或 ``brookesia_app.on_uninstall()``，取消 timer、卸载 runtime、卸载 GUI document、反注册 GUI 资源，并从 app 表和 manifest id 索引中移除。

``pause_app()`` 调用 ``on_pause()`` 后状态变为 ``Paused``；``resume_app()`` 调用 ``on_resume()`` 后状态变回 ``Running`` 并更新 active app。当前实现不会在 pause 时自动 unmount GUI 或停止 timer，应用如有特殊需要应在回调中自行处理。

.. _system-core-app_model-sec-05:

原生应用接入
--------------------

原生应用通过 C++ 继承 ``core::IApp`` 接入，通常 include 兼容入口 ``brookesia/system_core/app.hpp``。``IApp`` 当前接口：

.. list-table::
   :header-rows: 1

   * - 函数
     - 说明
   * - ``get_manifest()``
     - 必须实现，返回 app manifest
   * - ``get_gui_descriptor()``
     - 可选，返回 GUI root、启动 screen flows 和 image/font 资源
   * - ``on_install()`` / ``on_uninstall()``
     - 安装/卸载回调，默认成功
   * - ``on_start()`` / ``on_stop()``
     - 启动/停止回调，默认成功
   * - ``on_pause()`` / ``on_resume()``
     - 暂停/恢复回调，默认成功
   * - ``on_action()``
     - GUI action 派发时调用，默认成功
   * - ``on_timer()``
     - app timer 触发时调用，默认成功

``start_app()`` 会在 ``on_start()`` 前自动注册 ``get_gui_descriptor()`` 返回的资源并加载 GUI document；如果配置了 ``screen_flows[]``，还会自动挂载对应 flow 的 ``initial`` screen。``on_start()`` 默认是 no-op 成功；可见 UI app 通常需要覆盖它并初始化文本、binding、action、timer 等业务状态。

``AppContext`` 提供 ``app_id()``、``system_service()``、``timer()``、``gui()``。``system_service()`` 返回 ``SystemApi``；原生应用当前被视为系统内置可信 app，可启动/停止任意 app，读取 storage layout，访问自身 ``cache/data/files`` 和公共目录，并调整 runtime app 默认安装目标。

``IAppProvider`` 用于编译期链接进工程的 app 注册：``get_manifest()`` 返回 provider 对应 manifest，``create_app()`` 返回原生应用实例（返回空时按 runtime manifest 安装）。注册宏：

.. code-block:: cpp

   BROOKESIA_SYSTEM_CORE_APP_PROVIDER_REGISTER_WITH_SYMBOL(ProviderType, name, symbol_name)

``System::install_registered_apps()`` 会遍历 ``AppProviderRegistry`` 并安装 provider 提供的 app。

.. _system-core-app_model-sec-06:

运行时应用接入
--------------------

运行时应用通过 unpacked package 和脚本约定函数接入，不需要 include C++ 头文件。``System::init()`` 在 ``install_package_apps = true`` 时扫描 storage layout 中 internal 和 external 的 ``apps`` 目录：

.. code-block:: text

   <storage-root>/apps/
     brookesia.app.calculator/
       manifest.json
       app/
         app.lua
       res/
         profile.json
         root.json

core 扫描每个 app root 下包含 ``manifest.json`` 的一级目录，并按目录路径排序后加载。``manifest.json`` 由 ``read_unpacked_app_manifest()`` 解析，映射关系：

.. list-table::
   :header-rows: 1

   * - package 字段
     - 映射到
   * - ``package.id``
     - ``AppManifest.id``
   * - ``package.name``
     - ``AppManifest.localized_names``；``AppManifest.name`` 使用 ``en`` / 第一个名称 / id 回退
   * - ``package.version``
     - ``AppManifest.version``
   * - ``package.visible``
     - ``AppManifest.visible``，缺省 ``true``
   * - ``package.systems``
     - ``AppManifest.supported_systems``，缺省不限制 system type
   * - ``runtime.type``
     - ``AppManifest.runtime_type``
   * - ``runtime.entry``
     - ``AppManifest.entry``
   * - ``runtime.resource_dir``
     - ``AppManifest.resource_dir``
   * - ``runtime.arguments``
     - ``AppManifest.arguments``

``runtime.entry`` 和 ``runtime.resource_dir`` 必须是安全相对路径，不能是绝对路径或包含 ``..``；``package.systems`` 与当前 system type 不匹配时会跳过该 package。脚本入口需要创建全局表 ``brookesia_app``，core 通过 runtime 调用约定函数：

- ``brookesia_app.on_install()`` / ``brookesia_app.on_uninstall()``
- ``brookesia_app.on_start()`` / ``brookesia_app.on_stop()``
- ``brookesia_app.on_pause()`` / ``brookesia_app.on_resume()``
- ``brookesia_app.on_action(action)``
- ``brookesia_app.on_timer(timer_id, name)``

函数缺失按 no-op 处理；函数存在但返回 ``false`` 或抛出错误时对应生命周期失败，app 可能进入 ``Error``。运行时应用的 GUI document 来自 ``<runtime.resource_dir>/profile.json`` 的 ``root``，启动 flow 写在 ``screen_flows[]``，详见 :doc:`app_package`。

.. _system-core-app_model-sec-07:

相关文档
--------------------

- :doc:`gui`
- :doc:`services`
- :doc:`resources_permissions`
- :doc:`app_package`
