.. _system-core-configuration-sec-00:

配置
====================

:link_to_translation:`en:[English]`

本文汇总 ``brookesia_system_core`` 当前实现使用的 Kconfig、PC CMake config、宏默认值和运行时 storage 配置。

.. _system-core-configuration-sec-01:

Kconfig
--------------------

ESP 平台配置位于 ``system/brookesia_system_core/Kconfig``。

.. list-table::
   :header-rows: 1

   * - 配置项
     - 默认值
     - 说明
   * - ``BROOKESIA_SYSTEM_CORE_RUNTIME_APP_MAX_Z_ORDER``
     - ``100``
     - runtime app manifest 可申请的最大 ``z_order``
   * - ``BROOKESIA_SYSTEM_CORE_RUNTIME_ASYNC_DEBUG_PROBE``
     - ``n``
     - 注册 runtime async timeout 调试 probe
   * - ``BROOKESIA_SYSTEM_CORE_ENABLE_DEBUG_LOG``
     - ``n``
     - 打开 core 组件 debug log 总开关
   * - ``BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG``
     - ``y``
     - 总开关打开后，启用 system 模块 debug log
   * - ``BROOKESIA_SYSTEM_CORE_SERVICE_ENABLE_DEBUG_LOG``
     - ``y``
     - 总开关打开后，启用 service 模块 debug log
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_NAME_PREFIX``
     - ``System``
     - system task scheduler worker 线程名前缀
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_PRIORITY``
     - ``10``
     - system task scheduler worker 优先级
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_STACK_SIZE``
     - ``65536``
     - system task scheduler worker 栈大小
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_STACK_IN_EXT``
     - ``y if SPIRAM``
     - 是否将 worker 栈放到外部内存
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_POLL_INTERVAL_MS``
     - ``5``
     - worker poll 间隔
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_NUM``
     - ``3``
     - worker 数量，范围 ``1..4``
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_0_CORE_ID``
     - ``0``
     - Worker 0 core affinity，``-1`` 表示不指定
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_1_CORE_ID``
     - ``1``
     - Worker 1 core affinity，``-1`` 表示不指定
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_2_CORE_ID``
     - ``-1``
     - Worker 2 core affinity，``-1`` 表示不指定
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_3_CORE_ID``
     - ``-1``
     - Worker 3 core affinity，``-1`` 表示不指定

System 不再通过 Kconfig 指定旧式资源根目录。运行时会从 Device service 枚举存储文件系统，并建立 Android-like storage layout。

.. _system-core-configuration-sec-02:

PC CMake Config
--------------------

PC 平台配置位于 ``system/brookesia_system_core/cmake/pc_platform.cmake``。

.. list-table::
   :header-rows: 1

   * - CMake 变量
     - 默认值
     - 说明
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_ENABLE_DEBUG_LOG``
     - ``OFF``
     - PC debug log 总开关
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_SYSTEM_ENABLE_DEBUG_LOG``
     - ``ON``
     - PC system debug log 默认值
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_SERVICE_ENABLE_DEBUG_LOG``
     - ``ON``
     - PC service debug log 默认值
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_RUNTIME_ASYNC_DEBUG_PROBE``
     - ``OFF``
     - PC runtime async timeout 调试 probe 默认值
   * - ``BROOKESIA_SYSTEM_CORE_PC_STAGE_INTERNAL_ROOT``
     - ``${CMAKE_BINARY_DIR}/brookesia``
     - PC staged internal storage 根目录
   * - ``BROOKESIA_SYSTEM_CORE_PC_STAGE_APP_ROOT``
     - ``.../brookesia/apps``
     - PC 构建期 app staging 目录
   * - ``BROOKESIA_SYSTEM_CORE_PC_STAGE_SYSTEM_ROOT``
     - ``.../brookesia/system``
     - PC 构建期 system resource staging 目录
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_RUNTIME_APP_MAX_Z_ORDER``
     - ``100``
     - PC runtime app manifest 可申请的最大 ``z_order``
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_NAME_PREFIX``
     - ``System``
     - PC worker 线程名前缀
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_PRIORITY``
     - ``10``
     - PC worker 优先级
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_STACK_SIZE``
     - ``32768``
     - PC worker 栈大小
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_STACK_IN_EXT``
     - ``OFF``
     - PC worker 是否使用外部内存栈
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_POLL_INTERVAL_MS``
     - ``2``
     - PC worker poll 间隔
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_NUM``
     - ``3``
     - PC worker 数量
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_0_CORE_ID``
     - ``0``
     - PC Worker 0 core affinity
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_1_CORE_ID``
     - ``1``
     - PC Worker 1 core affinity
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_2_CORE_ID``
     - ``-1``
     - PC Worker 2 core affinity
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_3_CORE_ID``
     - ``-1``
     - PC Worker 3 core affinity

``PC_STAGE_*`` 变量只用于构建期 staging。运行时 storage layout 仍由 Device service 或 ``System::Config::storage`` 覆盖确定。

.. _system-core-configuration-sec-03:

macro_configs.h
--------------------

公共宏位于 ``include/brookesia/system_core/macro_configs.h``。

.. list-table::
   :header-rows: 1

   * - 宏
     - 当前值或来源
     - 说明
   * - ``BROOKESIA_SYSTEM_CORE_LOG_TAG``
     - ``"SysCore"``
     - 日志 tag
   * - ``BROOKESIA_SYSTEM_CORE_SERVICE_NAME``
     - ``"SystemCore"``
     - 系统 service 名
   * - ``BROOKESIA_SYSTEM_CORE_GUI_SERVICE_NAME``
     - ``"SystemGui"``
     - GUI service 名
   * - ``BROOKESIA_SYSTEM_CORE_TIMER_SERVICE_NAME``
     - ``"SystemTimer"``
     - timer service 名
   * - ``BROOKESIA_SYSTEM_CORE_DEFAULT_SYSTEM_TYPE``
     - ``"core"``
     - 默认 system type
   * - ``BROOKESIA_SYSTEM_CORE_DEFAULT_SCREEN_PATH``
     - ``"/root"``
     - app 默认 screen path
   * - ``BROOKESIA_SYSTEM_CORE_RUNTIME_ASYNC_DEBUG_PROBE``
     - Kconfig/PC config/默认值
     - 是否注册 runtime async timeout 调试 probe
   * - ``BROOKESIA_SYSTEM_CORE_RUNTIME_APP_MAX_Z_ORDER``
     - Kconfig/PC config/默认值
     - runtime app manifest 可申请的最大 ``z_order``
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_*``
     - Kconfig/PC config/默认值
     - system task scheduler worker 配置

.. _system-core-configuration-sec-04:

Storage Layout
--------------------

System init 时通过 Device service 查询所有 storage filesystem，生成一个 internal volume 和零个或多个 external volume。

internal 自动选择优先级：

1. Flash + LittleFS
2. Flash + FATFS
3. SDCard + FATFS
4. 其它 LittleFS
5. 其它 FATFS

external 包含所有 SDCard storage volume。``System::Config::storage`` 可覆盖 internal、external、preferred external id 和默认安装目标。

System 会自动创建以下固定目录：

- internal/external: ``apps``、``music``、``download``、``movies``、``pictures``、``documents``
- internal only: ``system``
- 每个 app: ``apps/<manifest_id>/cache``、``apps/<manifest_id>/data``、``apps/<manifest_id>/files``

runtime app 扫描目录为 internal 和所有 external 的 ``apps``。重复 manifest id 按 internal、preferred external、其它 external 的顺序去重。

默认 runtime app 安装目标为 ``Auto``：优先 preferred external，其次第一个 external，最后 fallback 到 internal。原生应用可通过 ``SystemApi::set_default_install_storage()`` 修改默认目标；System 会保存到 Storage KV 并在下次启动恢复。

.. _system-core-configuration-sec-05:

System::Config
--------------------

``System::Config`` 是运行时配置入口：

- ``gui_backend``：GUI backend。为空时不创建 GUI runtime。
- ``environment``：传给 JSON UI parser/runtime 的屏幕环境。
- ``storage``：可覆盖 internal/external volume、preferred external id 和默认安装目标。
- ``system_type``：默认 ``"core"``。
- ``start_service_manager``：是否在 init 中启动 ``ServiceManager``。
- ``install_registered_apps``：是否安装 ``IAppProvider`` registry 中的 app。
- ``install_package_apps``：是否扫描 storage layout 的 ``apps`` 目录并安装 runtime app。
- ``enable_gui_view_debug``：是否在创建 GUI runtime 后开启 view debug outline，默认关闭。
- ``enable_gui_live_preview``：是否为 file-backed GUI document 自动开启 live preview，默认关闭。
- ``gui_live_preview_options``：传给 ``gui::Runtime::enable_live_preview()`` 的选项。
- ``gui_live_preview_poll_interval_ms``：system 内部 live preview poll task 的周期，默认 ``100ms``。
- ``startup_overlay``：可选 system startup overlay；在 GUI runtime 创建后、app 安装和 Shell 启动前显示，``start()`` 完成后隐藏。
- ``app_launch_transition``：可选 app launch transition；在 app GUI 资源注册和 document 加载前显示，可由 ``start_app(app_id, AppStartOptions{.launch_origin_frame = ...})`` 指定动画起点。

.. _system-core-configuration-sec-06:

目录与文件接口
--------------------

原生应用可通过 ``AppContext::system_service()`` 访问：

.. code-block:: cpp

   auto layout = context.system_service().get_storage_layout();
   auto app_dirs = context.system_service().get_app_storage_paths();
   auto public_dirs = context.system_service().get_public_storage_paths();

runtime app 通过 ``SystemCore`` service 使用同等 JSON API。文件操作必须传入 ``StoragePathKind``、volume id 和相对路径；绝对路径和 ``..`` 会被拒绝，无法逃逸到其它 app 的目录。
