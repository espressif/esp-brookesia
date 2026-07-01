.. _system-super-configuration-sec-00:

配置
====================

:link_to_translation:`en:[English]`

本文汇总 ``brookesia_system_super`` 当前实现使用的宏、Kconfig 和资源路径。Super 资源位于 system core internal storage 的 ``system/super`` 目录，不单独定义资源根。

Kconfig
--------------------

ESP 平台配置位于 ``system/brookesia_system_super/Kconfig``。

.. list-table::
   :header-rows: 1

   * - 配置项
     - 默认值
     - 说明
   * - ``BROOKESIA_SYSTEM_SUPER_ENABLE_DEBUG_LOG``
     - ``n``
     - 打开 Super debug log
   * - ``BROOKESIA_SYSTEM_SUPER_ENABLE_STARTUP_OVERLAY``
     - ``y``
     - 开机阶段显示轻量 SystemTop startup overlay
   * - ``BROOKESIA_SYSTEM_SUPER_STARTUP_ROOT_JSON``
     - ``startup/root.json``
     - startup overlay root，相对 ``<system-root>/super``
   * - ``BROOKESIA_SYSTEM_SUPER_ENABLE_RESOURCE_FONT_COPY``
     - ``y``
     - 将组件内置 ``resource/fonts`` 拷贝到 ``<system-root>/fonts``
   * - ``BROOKESIA_SYSTEM_SUPER_ENABLE_APP_LAUNCH_TRANSITION``
     - ``n``
     - 兼容项；当前 Shell overlay 内联 app launch
   * - ``BROOKESIA_SYSTEM_SUPER_APP_LAUNCH_ROOT_JSON``
     - ``app_launch/root.json``
     - 兼容项；当前未使用
   * - ``BROOKESIA_SYSTEM_SUPER_ENABLE_APP_LAUNCH_ANIMATION``
     - ``y``
     - 点击 launcher app icon 后播放 mask/icon 放大移动动画
   * - ``BROOKESIA_SYSTEM_SUPER_APP_LAUNCH_POST_COMPLETE_HOLD_MS``
     - ``0``
     - Shell app launch 动画完成回调触发后的额外停留时间

PC CMake Config
--------------------

PC 平台配置位于 ``system/brookesia_system_super/cmake/pc_platform.cmake``。

.. list-table::
   :header-rows: 1

   * - CMake 变量
     - 默认值
     - 说明
   * - ``BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_DEBUG_LOG``
     - ``OFF``
     - PC debug log 默认值
   * - ``BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_STARTUP_OVERLAY``
     - ``ON``
     - PC startup overlay 默认开关
   * - ``BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_RESOURCE_FONT_COPY``
     - ``ON``
     - PC 内置字体资源 staging 默认开关
   * - ``BROOKESIA_SYSTEM_SUPER_PC_CONFIG_STARTUP_ROOT_JSON``
     - ``startup/root.json``
     - PC startup overlay root 默认值
   * - ``BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_APP_LAUNCH_TRANSITION``
     - ``OFF``
     - 兼容项；当前 Shell overlay 内联 app launch
   * - ``BROOKESIA_SYSTEM_SUPER_PC_CONFIG_APP_LAUNCH_ROOT_JSON``
     - ``app_launch/root.json``
     - 兼容项；当前未使用
   * - ``BROOKESIA_SYSTEM_SUPER_PC_CONFIG_ENABLE_APP_LAUNCH_ANIMATION``
     - ``ON``
     - PC launch mask/icon 动画默认开关
   * - ``BROOKESIA_SYSTEM_SUPER_PC_CONFIG_APP_LAUNCH_POST_COMPLETE_HOLD_MS``
     - ``0``
     - PC launch 动画完成后的额外停留时间

GUI Backend
--------------------

Super 运行时只通过 ``brookesia_system_core`` 和 ``gui_interface`` 抽象访问 GUI backend。调用方必须在 ``core_config.gui_backend`` 中传入 backend 实例。构建期图片打包使用 ``brookesia_gui_lvgl_pack_images()`` 将 ``assets/shell/images`` 下的图片转成 LVGL ``.bin + imageSet index.json``；该能力只用于生成 staged 资源，不在 C++ 运行时代码中直接依赖具体 backend。

Macro Configs
--------------------

公共宏位于 ``include/brookesia/system_super/macro_configs.h``。

.. list-table::
   :header-rows: 1

   * - 宏
     - 当前值
     - 说明
   * - ``BROOKESIA_SYSTEM_SUPER_LOG_TAG``
     - ``"SysSuper"``
     - 日志 tag
   * - ``BROOKESIA_SYSTEM_SUPER_SYSTEM_TYPE``
     - ``"super"``
     - Super system type
   * - ``BROOKESIA_SYSTEM_SUPER_RESOURCE_DIR``
     - ``"/littlefs/system/super"``
     - Super 编译期默认资源根目录
   * - ``BROOKESIA_SYSTEM_SUPER_SHELL_RESOURCE_DIR``
     - ``.../shell``
     - shell 资源目录兼容宏
   * - ``BROOKESIA_SYSTEM_SUPER_ENABLE_DEBUG_LOG``
     - Kconfig/PC config
     - debug log 开关
   * - ``BROOKESIA_SYSTEM_SUPER_ENABLE_STARTUP_OVERLAY``
     - Kconfig/PC config
     - startup overlay 开关
   * - ``BROOKESIA_SYSTEM_SUPER_ENABLE_RESOURCE_FONT_COPY``
     - Kconfig/PC config
     - 内置字体资源 staging 开关
   * - ``BROOKESIA_SYSTEM_SUPER_ENABLE_APP_LAUNCH_ANIMATION``
     - Kconfig/PC config
     - launch mask/icon 动画开关
   * - ``BROOKESIA_SYSTEM_SUPER_APP_LAUNCH_POST_COMPLETE_HOLD_MS``
     - Kconfig/PC config
     - launch 动画完成回调后的额外停留时间

Resource Paths
--------------------

ESP 平台默认资源根、字体根和 app package 根：

.. code-block:: text

   /littlefs/system/super
   /littlefs/system/fonts
   /littlefs/apps

PC 平台默认：

.. code-block:: text

   ${CMAKE_BINARY_DIR}/brookesia/system/super
   ${CMAKE_BINARY_DIR}/brookesia/system/fonts
   ${CMAKE_BINARY_DIR}/brookesia/apps

``littlefs`` staging 根目录第一级只应包含 ``app/`` 和 ``system/``；共享字体属于 ``system/fonts``。关闭内置字体资源 staging 后，``brookesia_system_super`` 不删除也不复制 ``system/fonts``；产品工程可自行提供 ``system/fonts/index.json``，缺失则 Super 初始化时跳过字体预注册并继续运行。生成的图片资源位于 ``<system-root>/super/shell/images``，Shell Runtime theme 位于 ``<system-root>/super/themes``。产品工程需要通过 LittleFS image 规则把 ``littlefs`` 目录打入固件。
