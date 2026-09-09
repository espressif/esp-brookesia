.. _system-core-app_package-sec-00:

应用包
====================

:link_to_translation:`en:[English]`

本文说明运行时应用包的结构、manifest 字段、资源 descriptor 和扫描安装规则。

.. _system-core-app_package-sec-01:

概述
--------------------

App package 使用 ``manifest.json`` 描述包元信息和 runtime 入口；runtime 资源目录下的 ``profile.json`` 是 system_core resource descriptor，``root`` 指向真正的 JSON UI document，启动 flow 写在 ``screen_flows[]``。它有两种使用方式：

- ``.bpk``：ZIP 容器，运行时由 ``system::core::unpack_app_package_to()`` 解包。
- unpacked package：直接部署 ``manifest.json + app/ + res/``，由 ``System::init()`` 扫描安装。

需要运行 ``.bpk`` 时，先调用 system_core package API（可选 ``verify_app_package_release()``，再 ``unpack_app_package_to()``）得到 ``AppManifest``，再用 ``make_runtime_app_config()`` 转成 ``runtime::AppConfig``。

.. _system-core-app_package-sec-02:

最小包结构
--------------------

.. code-block:: text

   brookesia.app.demo/
     manifest.json
     app/
       app.lua
     res/
       profile.json
       root.json

``manifest.json`` 示例：

.. code-block:: json

   {
     "package": {
       "id": "brookesia.app.demo",
       "name": {"en": "Demo", "zh_CN": "演示"},
       "version": "0.1.0",
       "visible": true,
       "systems": ["core", "super"]
     },
     "runtime": {
       "type": "Lua",
       "entry": "app/app.lua",
       "resource_dir": "res",
       "arguments": []
     },
     "services": [
       {
         "name": "Storage",
         "version": "0.8.1"
       }
     ]
   }

.. _system-core-app_package-sec-03:

Manifest 字段
--------------------

``package`` 对象：

.. list-table::
   :header-rows: 1

   * - 字段
     - 说明
   * - ``id``
     - app 唯一 manifest id，必填
   * - ``name``
     - 多语言展示名对象；``AppManifest.name`` 按 ``en`` / 第一个名称 / id 回退
   * - ``version``
     - app 版本，必填
   * - ``visible``
     - 是否显示在 launcher，缺省 ``true``
   * - ``systems``
     - 支持的 system type 列表，缺省不限制

``runtime`` 对象：

.. list-table::
   :header-rows: 1

   * - 字段
     - 说明
   * - ``type``
     - runtime backend 类型，例如 ``Lua`` 或 ``JavaScript``，必填
   * - ``entry``
     - 入口脚本相对路径，必填
   * - ``resource_dir``
     - app 资源目录相对路径
   * - ``arguments``
     - runtime 启动参数字符串数组

可选的顶层 ``services`` 数组声明 runtime app 所需的 service RPC：

.. list-table::
   :header-rows: 1

   * - 字段
     - 说明
   * - ``name``
     - 稳定且区分大小写的 RPC 名，例如 ``Storage``；必须使用 service helper 的 ``get_name()`` 返回的精确值
   * - ``version``
     - 数字 ``MAJOR.MINOR.PATCH`` 格式的最低兼容 service 版本

本地 service 的 major 和 minor 必须与声明值相同，并且本地 patch 必须大于或等于声明的 patch，才视为版本兼容。例如，本地版本 ``0.8.2`` 和 ``0.8.3`` 均满足 ``0.8.1`` 的依赖，而 ``0.9.0`` 不满足。所有声明的 service 都是必要依赖。

``component`` 字段可以作为外部工具的元数据保留，但 system_core 会静默忽略它：不会解析、校验、保存、输出或用于匹配。Core 仅使用 ``name`` 和 ``version`` 匹配 service。

安装 ``.bpk`` 或启动 app 时，system_core 会检查每个已声明 service 是否已注册以及版本是否兼容。App 真正尝试使用 service 前无法发现漏声明，因此 HostBridge 还会在调用时强制检查声明。对于未声明的 service，``service_available()`` 返回 ``false``，其他按名称执行的 service 操作返回错误且不会调用该 service。

精确 RPC 名 ``SystemCore``、``SystemGui`` 和 ``SystemTimer`` 默认允许，无需声明。匹配区分大小写，``systemcore`` 等变体不会被默认允许。缺少 ``services`` 字段或数组为空，表示 runtime app 只能调用这三个默认允许的系统 RPC。

上述约束适用于通过 HostBridge 访问 service 的 runtime app。Native app 属于可信固件代码，其直接使用 C++ service 接口的行为不受 manifest 限制。

.. _system-core-app_package-sec-04:

资源 descriptor
--------------------

``<runtime.resource_dir>/profile.json`` 是 system_core resource descriptor，不是 JSON UI document：

.. code-block:: json

   {
     "icon_id": "demo",
     "root": "root.json",
     "screen_flows": [
       {"screen_flow": "main", "layer": "AppDefault", "mount_mode": "Replace", "z_order": 0}
     ]
   }

``icon_id`` 用于在包内定位 launcher 图标；``root`` 指向标准 JSON UI document；``screen_flows[]`` 指定启动 flow。普通 app 主界面应使用 ``AppDefault``，``AppTop`` 只用于 app 自己的 header、浮层或工具栏；runtime app 只能使用 ``Replace``，``z_order`` 在 ``0..100`` 内。``root.json`` 是标准 JSON UI document，应引用一个 ``screenFlow`` asset。

.. _system-core-app_package-sec-05:

扫描与安装
--------------------

``System::init()`` 在 ``install_package_apps = true`` 时扫描 storage layout 中 internal 和 external 的 ``apps`` 目录，按目录路径排序加载每个包含 ``manifest.json`` 的一级目录。如果多个 volume 下出现相同 manifest id，internal 和 preferred external 优先，后扫描的重复项会跳过并输出 warning。

安装阶段会用 runtime root ``icon_id`` 在资源目录中查找匹配的 ``imageSet`` descriptor，找到后注册为 runtime-global image resource 并填充 ``AppManifest.icon_path``，供 launcher 显示图标。

.. _system-core-app_package-sec-06:

安全约束
--------------------

- ``runtime.entry`` 和 ``runtime.resource_dir`` 必须是安全相对路径，不能是绝对路径或包含 ``..``。
- ``package.systems`` 与当前 system type 不匹配时跳过该 package。
- ``package.id``、``package.version``、``runtime.type``、``runtime.entry`` 缺失或类型错误会导致 manifest 解析失败。
- runtime GUI 启动 flow 必须写入 ``profile.json`` 的 ``screen_flows[]``。

.. _system-core-app_package-sec-07:

打包工具
--------------------

仓库提供 ``tools/app_pack.py`` 将 unpacked package 打包为 ``.bpk``，并对 ``manifest.json`` 字段做校验。修改 package 规范时，应保持 schema、工具校验、system_core parser 和示例资源一致。
