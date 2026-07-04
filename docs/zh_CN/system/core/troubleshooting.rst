.. _system-core-troubleshooting-sec-00:

故障排查
====================

:link_to_translation:`en:[English]`

本文列出 system core 当前实现中的常见问题和优先检查项。

.. _system-core-troubleshooting-sec-01:

运行时应用 path 不存在
----------------------

日志出现 ``Runtime app path does not exist`` 时，检查：

- ESP 平台是否已挂载 LittleFS。
- Device service 是否返回了支持目录的 storage filesystem，且 internal/external layout 初始化成功。
- 构建期 staging 是否把 package 复制到 ``<storage-root>/apps/<manifest-id>``。
- 如果 internal 和 external 都存在相同 manifest id，确认优先级更高的目录下 package 内容正确；重复项会跳过。

.. _system-core-troubleshooting-sec-02:

应用未安装
--------------------

``Expected app is not installed`` 通常是 package 没有被扫描或 manifest 解析失败。检查：

- ``<storage-root>/apps/<manifest-id>/manifest.json`` 是否存在。
- manifest 是否包含 ``package`` 和 ``runtime`` 对象。
- ``runtime.type`` 是否能映射到已编译进来的 runtime backend。

.. _system-core-troubleshooting-sec-03:

Manifest 解析失败
--------------------

常见原因：

- ``package.id``、``package.version``、``runtime.type``、``runtime.entry`` 缺失或不是字符串。
- ``runtime.arguments`` 不是字符串数组。
- runtime GUI 启动 flow 未写入 ``<runtime.resource_dir>/profile.json`` 的 ``screen_flows[]``。
- ``runtime.entry``、``runtime.resource_dir`` 是绝对路径或包含 ``..``。

.. _system-core-troubleshooting-sec-04:

GUI root 加载失败
--------------------

检查：

- 原生应用的 ``get_gui_descriptor().root_kind`` 与 ``root`` 是否正确。
- 运行时应用的 ``profile.json`` descriptor 是否存在，且 ``root`` 指向的 JSON UI document 是否存在。
- ``root`` 路径是否相对应用资源目录。
- 原生应用使用的 ``${image.xxx}`` 或 ``${font.xxx}`` 是否已通过 ``get_gui_descriptor().resources`` 提供。
- ``screen_flows[]`` 是否包含至少一个启动 flow。

.. _system-core-troubleshooting-sec-05:

View not found
--------------------

``SystemGui`` 返回 ``View not found`` 时，检查：

- app 是否已经启动，GUI document 是否已加载。
- ``TriggerScreenFlow`` 或 ``SetText`` 的 path/flow id 是否属于当前 app 的 GUI document。
- view path 是否是绝对路径。
- 动态创建 view 后，instance path 是否与 JSON UI runtime 生成路径一致。

.. _system-core-troubleshooting-sec-06:

服务权限拒绝
--------------------

运行时普通 app 调用 ``SystemCore.StartApp`` 或 ``StopApp`` 会被拒绝，普通 runtime app 只能请求关闭自己。如果是原生应用，需要通过 ``AppContext::system_service()`` 调用，而不是模拟 runtime service 调用。

.. _system-core-troubleshooting-sec-07:

资源 id 冲突
--------------------

日志中出现 image/font resource id already registered 时：

- 检查多个原生应用是否使用了同一个 resource id。
- 检查同一个原生应用的 ``get_gui_descriptor().resources`` 是否返回重复 id。
- 当前版本不支持跨 app 共享同名 runtime resource id。

.. _system-core-troubleshooting-sec-08:

Timer 没有触发
--------------------

检查：

- timer interval/delay 是否大于 ``0``。
- app 是否仍处于 ``Running`` 状态。
- ``stop_app()`` 会自动取消该 app 所有 timer。
