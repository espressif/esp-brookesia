.. _hal-pc-simulation-sec-00:

PC 仿真支持
================

:link_to_translation:`en:[English]`

ESP-Brookesia 提供面向非 ESP 目标的 HAL 后端实现，让上层代码在 PC 环境中直接复用 :ref:`HAL 接口 <hal-interface-index-sec-00>` 的抽象设备模型，无需改动业务逻辑即可完成开发、调试、自动化测试和 Web 演示。

目前包含两个平台后端：

- ``brookesia_hal_linux``：Linux host 后端，CMake alias 为 ``brookesia::hal_linux``。
- ``brookesia_hal_wasm``：WebAssembly 后端，CMake alias 为 ``brookesia::hal_wasm``。

.. _hal-pc-simulation-sec-01:

概述
--------

PC 仿真后端与 :ref:`ESP 设备板级适配 <hal-adaptor-sec-00>` 处于同一层级：两者都实现 ``brookesia_hal_interface`` 定义的设备/接口模型，并把 **系统**、 **网络**、 **音频**、 **显示**、 **存储**、 **电源**、 **视频**、 **Wi-Fi** 等能力注册到全局 HAL 表，供上层按名称发现。区别在于 PC 后端运行在 Linux 或 WebAssembly 运行时之上，而非真实开发板与 ``esp_board_manager``。

.. _hal-pc-simulation-sec-02:

Linux 平台
------------

``brookesia_hal_linux`` 是 ``brookesia_hal_interface`` 的 Linux host 后端，可在桌面 Linux 上构建并运行框架与上层应用，便于在无真实硬件时进行开发与联调。

.. _hal-pc_simulation-sec-03:

依赖与构建
^^^^^^^^^^^^

- 依赖安装脚本位于组件目录下的 ``scripts/install_linux_deps.sh``，支持 ``apt`` / ``dnf`` / ``pacman``；``--minimal`` 仅安装构建、Boost 与 OpenSSL 依赖，``--full`` 额外安装透明绝对路径所需的 ``proot`` 辅助工具，``--media`` / ``--video`` / ``--display`` / ``--network`` 分别安装 FFmpeg/PortAudio/V4L2、SDL2、NetworkManager/UPower 等可选依赖组。
- 组件提供 host test 工程，可通过 ``cmake -S host_test -B host_test/build`` 构建并用 ``ctest`` 运行；默认强制使用 stub 后端以保证 CI 确定性。

.. _hal-pc_simulation-sec-04:

后端策略
^^^^^^^^^^

各能力可在 stub 与真实后端之间选择，由 CMake 变量或 Kconfig 控制：

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - 能力
     - 后端选项
   * - Media（音频）
     - ``stub`` 或 ``ffmpeg_portaudio`` （FFmpeg + PortAudio 播放、采集、编解码）
   * - Video（视频）
     - ``stub`` 或 ``ffmpeg_v4l2`` （FFmpeg ``libavdevice`` / V4L2 采集与解码）
   * - Display（显示）
     - ``stub`` 或 ``sdl2`` （SDL2 窗口、位图刷新、鼠标/触摸、背光亮度模拟）
   * - Power（电源）
     - ``stub`` 或 ``upower`` （读取 ``/sys/class/power_supply`` 真实电池状态）
   * - Wi-Fi
     - ``stub`` 或 ``networkmanager`` （基于 ``nmcli`` 扫描、连接、断开、状态查询）

``auto`` 会在依赖可用时优先选择真实后端；运行时若缺少图形会话、摄像头、电池或权限不足，会打印 warning 并降级到确定性 stub 路径。

.. _hal-pc_simulation-sec-05:

运行时数据
^^^^^^^^^^^^

- KV 数据默认按 ``BROOKESIA_HAL_LINUX_KV_DIR`` -> 可执行文件目录 ``.brookesia/kv`` -> ``$XDG_STATE_HOME`` -> ``$HOME`` -> 系统临时目录的优先级持久化。
- 文件系统通过 ``BROOKESIA_HAL_LINUX_FS_ROOT`` 把 ``/spiffs`` / ``/littlefs`` / ``/fatfs`` / ``/sdcard`` 映射到 host 子目录；未设置时默认使用 ``.brookesia/fs``。
- HTTP 使用 Boost.Beast 与 OpenSSL，并可通过 Kconfig 限制响应读取速率以模拟 ESP 级下载吞吐；SNTP 与 Storage 使用 host 网络栈与本地文件系统。

.. _hal-pc-simulation-sec-03:

WASM 平台
-----------

``brookesia_hal_wasm`` 是面向 WebAssembly 的后端，用于把框架编译到浏览器或 WebAssembly 运行时，实现 Web 端仿真与演示。

- 与 Linux 后端一样，它实现并注册 system、network、audio、video、display、power、storage、Wi-Fi 等设备族接口到全局 HAL 表。
- Display 后端基于 SDL2，默认窗口为 ``800x480``，可通过 ``DisplayWasmDevice::Config`` 配置分辨率与窗口标题，并轮询退出与触摸事件。
- Network 后端提供 connectivity 与 HTTP client 接口，构建为 Emscripten 目标时通过链接选项启用浏览器能力。
- 组件在 ``EMSCRIPTEN`` 下追加链接选项 ``-sUSE_SDL=2`` / ``-sFETCH=1`` / ``-sASYNCIFY=1`` / ``-sALLOW_MEMORY_GROWTH=1``，分别支持 SDL2 渲染、HTTP 抓取、异步阻塞与内存增长。

.. _hal-pc-simulation-sec-04:

选择建议
--------

- ESP 真机：使用 :ref:`ESP 设备板级适配 <hal-adaptor-sec-00>` 与 :ref:`ESP 设备板级配置 <hal-boards-sec-00>`，通过 ``esp_board_manager`` 初始化真实外设。
- PC 本地开发与 CI：使用 ``brookesia_hal_linux``，可在无真实硬件时用 stub 后端获得确定性行为。
- Web 演示与跨平台展示：使用 ``brookesia_hal_wasm`` 编译到 WebAssembly。

.. _hal-pc-simulation-sec-05:

注意事项
--------

- PC 仿真后端保证 HAL 接口行为兼容，便于在桌面或浏览器环境复用上层逻辑，但不复现目标 ESP 硬件的真实时序、电源管理与外设限制。
- stub 后端用于确定性测试，真实后端依赖 host 环境能力，能力缺失时会自动降级到 stub 路径。
- 平台相关的能力差异（如充电控制、SoftAP、摄像头）以各组件源码与 Kconfig/CMake 选项为准。
