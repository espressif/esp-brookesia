.. _system-core-resources_permissions-sec-00:

资源与权限
====================

:link_to_translation:`en:[English]`

本文说明 GUI 资源的自动注册规则，以及 system core 当前基于 ``AppManifest.kind`` 的权限边界。

GUI 资源注册
--------------------

system core 支持两类 GUI 资源：原生应用在 ``start_app()`` 时通过 ``AppGuiDescriptor.resources`` 自动注册 image/font，并在 ``stop_app()``、``uninstall_app()`` 或 ``deinit()`` 时反注册；runtime package 在安装阶段会用 runtime root ``icon_id`` 自动发现并注册 launcher icon image，并在 ``uninstall_app()`` 或 ``deinit()`` 时反注册。

资源声明
--------------------

原生应用可重写 ``get_gui_descriptor()``，其中 ``resources`` 包含 ``std::vector<gui::RuntimeImageResource> images`` 和 ``std::vector<gui::RuntimeFontResource> fonts``。这些资源会注册到 ``gui::Runtime`` 的全局 runtime resource 表中，用于 JSON UI 中的 ``${image.xxx}`` 和 ``${font.xxx}`` 引用。

注册时机
--------------------

``start_app()`` 顺序是：先注册 app GUI resources，再加载 GUI document，最后调用 app ``on_start()``。这样可以保证 JSON UI 在解析 document 时已经能解析到 app 提供的 runtime image/font id。

若 image ``primary_src`` 指向 LVGL ``.bin`` 文件，``gui::Runtime`` 会在加载 GUI document 时通知 backend 预加载该文件。预加载发生在节点创建前，因此 ``${image.xxx}`` 缺文件或 header 无效会让 app start 失败并进入错误路径，而不是在第一次显示或 binding 刷新时才暴露。

反注册时机
--------------------

``stop_app()`` 顺序是：先调用 app ``on_stop()``，再 unload GUI document，最后 unregister app GUI resources。``uninstall_app()`` 和 ``deinit()`` 也会兜底取消 timer、卸载 GUI/runtime，并反注册资源。

反注册 image 会释放 backend 对该资源的预加载引用；如果同一路径仍被其他 document 或资源引用，LVGL backend 的路径缓存会通过引用计数保留。

冲突策略
--------------------

当前 v1 要求 app resource id 全局唯一：同一个 app 返回重复 image id 或 font id 会启动失败；另一个 app 已拥有同名 image/font id 也会启动失败；资源注册失败时，core 回滚已注册资源并把 app 状态设为 ``Error``。后续如需共享资源，需要引入引用计数或系统公共资源域；当前实现不做共享。

Runtime package 文件资源
------------------------

运行时应用优先使用 package 中的文件资源、``<runtime.resource_dir>/profile.json`` descriptor 和 ``root`` 指向的 JSON UI document。普通运行时应用不能注册 Native LVGL image/font 资源，也不能通过 service 操作 runtime 全局资源表。

Runtime package 中的 ``imageSet`` JSON 也可以引用 ``.bin`` 文件；这类文件同样在 document load 阶段预加载。

Launcher Icon
--------------------

runtime package 可在 ``<runtime.resource_dir>/profile.json`` 中声明展示图标 id ``icon_id``。``icon_id`` 只表示 image id；runtime manifest 不包含 icon descriptor 路径，system core 会在 package 资源目录中查找 ``images[]`` 内存在 ``id`` 等于 runtime root ``icon_id`` 的 ``imageSet`` descriptor JSON，例如 ``<runtime.resource_dir>/images/*.json`` 或 ``<app_path>/images/*.json``。找到后会把该 image 注册为 runtime-global image resource，并填充内部 ``AppManifest.icon_path`` 供 launcher 使用；没有匹配 descriptor 时 launcher 应使用文字或占位 fallback。

权限模型
--------------------

system core 当前使用 ``AppManifest.kind`` 区分基础权限。

原生应用
--------------------

``AppKind::Native`` 表示 C++ ``IApp`` app，当前 MVP 中视为系统内置可信 app。原生应用可以：

- 通过 ``SystemApi`` 查询系统信息。
- 通过 ``SystemApi::start_app()`` 和 ``SystemApi::stop_app()`` 启动或停止其他 app。
- 通过 ``SystemApi::set_default_install_storage()`` 更新 runtime app 默认安装目标。
- 通过 storage API 访问自身 app 目录和公共目录。
- 使用应用自有 GUI wrapper 操作自己的 document。
- 使用 native-only GUI API 加载、挂载、卸载 system-owned document。
- 使用 ``AppTimerRuntime`` 创建 periodic 或 delayed timer。

运行时应用
--------------------

``AppKind::Runtime`` 表示 Lua、JavaScript 等脚本 runtime app，被视为普通 app。运行时应用可以调用 ``SystemCore.GetSystemType``、``GetEnvironment``、``GetActiveApp``、``ListApps``，通过 ``SystemCore.RequestCloseApp`` 请求关闭自己，通过 ``SystemGui`` 操作自己的 GUI document，并通过 ``SystemTimer`` 管理自己的 periodic timer。运行时应用不能：

- 指定 GUI ``DocumentId``、layer 或 mount target。
- 调用 load/unload GUI document service。
- 通过 service 启动或停止其他 app。
- 修改默认安装目标或访问其它 app 的私有目录。
- 注册 Native image/font runtime resource。

调用方 owner 判定
--------------------

service 处理 runtime 调用时，会通过 ``runtime::RuntimeFunctionBridge::get_current_app_id()`` 查询当前 runtime app id，再映射到 core 的 ``AppId``。如果找不到 owner，service 调用会失败。这保证 ``SystemGui`` 和 ``SystemTimer`` 的操作都归属到发起调用的 app。
