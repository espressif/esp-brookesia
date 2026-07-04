.. _system-super-troubleshooting-sec-00:

故障排查
====================

:link_to_translation:`en:[English]`

本文列出 Super 运行中的常见问题。

.. _system-super-troubleshooting-sec-01:

shell root 缺失
--------------------

现象是 ``start()`` 失败、日志中出现 shell ``root.json`` 加载失败。检查 ``BROOKESIA_SYSTEM_SUPER_SHELL_RESOURCE_DIR`` 是否正确，ESP LittleFS 中是否存在 ``/littlefs/system/super/shell/root.json``，PC build 目录中是否存在 ``brookesia/system/super/shell/root.json``。

.. _system-super-troubleshooting-sec-02:

overlay screen 缺失
--------------------

现象是 Super 启动失败、顶部状态栏不显示或底部 gesture indicator 不显示。检查 ``root.json`` 是否包含 ``screens/overlay.json``，对应文件是否存在，Shell manifest 是否包含 ``overlay -> AppTop + Stack + z_order 101``，且 ``flows/overlay.json`` 是否被 root 加载。

.. _system-super-troubleshooting-sec-03:

background screen 缺失
----------------------

如果 Shell 页面底层没有显示桌面背景或 app 纯色背景，检查 ``root.json`` 是否包含 ``screens/background.json`` 和 ``screens/app_background.json``，staged ``shell/images/index.json`` 和对应 ``.bin`` 是否存在，Shell manifest 是否包含 ``background -> SystemBottom`` 且 ``flows/background.json`` 被加载，content page 是否设置为透明背景。

.. _system-super-troubleshooting-sec-04:

shell 图片缺失
--------------------

现象是 navigation icon 不显示或 shell root 加载失败提示 image asset descriptor 缺失。检查 PC build 目录和 ESP LittleFS 中是否存在 ``shell/images/index.json`` 和 ``*.bin``，``root.json`` 是否包含 ``images/index.json`` 等生成后的 ``imageSet`` descriptor，``brookesia_gui_lvgl_pack_images()`` 是否成功处理了 ``assets/shell/images`` 下的图片。

.. _system-super-troubleshooting-sec-05:

launcher 没有 app
--------------------

检查 app 是否已安装到 core，app manifest 的 ``visible`` 是否为 ``true``，runtime app package 是否位于 storage layout 的 ``apps/<manifest-id>`` 目录并被扫描到，shell 是否已重新启动以刷新 launcher。Shell summary 会显示当前可启动 app 数量。

.. _system-super-troubleshooting-sec-06:

点击 launcher 无反应
--------------------

检查 ``root.json`` 中 button template root 是否仍使用 action ``super.launch.app``，icon/label 子节点是否保持 ``clickable=false``，``clicked`` effect 是否仍以 ``requireValidPress=true`` 派发 ``super.launch.app``，template id 是否仍为 ``launcher_app_button``，launcher grid path 是否仍为 ``/launcher/content/grid``，动态实例 path 是否符合 ``/launcher/content/grid/app_<app_id>``。

.. _system-super-troubleshooting-sec-07:

close app 后没有恢复 shell
--------------------------

检查 active app 是否确实不是 shell，Display service 是否启用了 touch gesture，是否收到从 BottomEdge 开始、方向为 Up 的 ``TouchGesture``，``stop_app()`` 是否返回失败，``on_app_stopped()`` 是否因为 ``stopping_ = true`` 被跳过，shell app id 是否有效。

.. _system-super-troubleshooting-sec-08:

Shell page 没有切换
--------------------

检查 Shell root 是否包含 ``/home``、``/launcher`` 和 ``/notifications`` 三个 content screen，``ShellApp::show_page()`` 是否返回错误日志。

.. _system-super-troubleshooting-sec-09:

gesture indicator 被普通 app 覆盖
---------------------------------

当前设计中 overlay 以 ``AppTop + Stack + z_order 101`` 挂到 ``GuiLayer::Top``，普通 app screen 使用 replace mount。如果被覆盖，检查 overlay 是否成功 mount，app 是否绕过应用自有 GUI API 直接使用了 system-only mount target 或更高 ``z_order``，JSON UI screen 是否因为不透明全屏元素导致视觉上遮挡但 layer 顺序仍正确。

.. _system-super-troubleshooting-sec-10:

普通 app 打开后无法上滑退出
---------------------------

当前设计会在普通 app 前台时通过底部上滑手势退出 app。检查 Display service 是否已启动并绑定了当前 output 的 touch，``/overlay/gesture_indicator`` 是否存在且可通过 binding 显示，手势是否从屏幕底部边缘开始并保持到达退出阈值。
