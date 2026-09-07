.. _plugin-sec-00:

应用开发插件
------------

:link_to_translation:`en:[English]`

``@brookesia/app-dev-plugin`` 是 ESP-Brookesia GUI 应用开发的 **主机端** Agent 插件。
它通过 **MCP** (Model Context Protocol) 挂到 Cursor、Claude Code、Codex 等 Agent IDE/CLI，
并驱动 :ref:`system-toolkit-sec-00` (``brookesia`` CLI) 完成初始化、构建、仿真、打包与部署。

**它不是：**

- 各 IDE 官方插件商店里的扩展（当前推荐路径是写 MCP 配置，不是在商店里搜扩展名）
- 固件组件 ``brookesia_mcp_utils`` (那是设备侧 MCP 相关能力，与本插件无关)

安装后请在 **Settings → MCP** (或 ``/mcp``、``codex mcp list``) 中查找服务器名 ``brookesia-app-dev``。

请 **同时安装** ``@brookesia/app-dev-plugin`` 和 ``esp-brookesia-toolkit``。
未安装 CLI 时，``brookesia_init`` / ``brookesia_build`` / ``brookesia_simulate`` 等 MCP 工具会失败。

已发布包：`@brookesia/app-dev-plugin <https://www.npmjs.com/package/@brookesia/app-dev-plugin>`__。

.. _plugin-sec-00a:

能力概览
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table:: 插件主要能力
   :header-rows: 1
   :widths: 28 72

   * - 能力
     - 说明
   * - 应用工程
     - 初始化、构建、打包、验证、发布 ``.bpk`` (底层调用 ``brookesia`` CLI)
   * - WASM 仿真
     - 浏览器内运行 JSON UI，支持探测、热重载、截图
   * - JSON UI
     - Schema 校验、布局检查、交互控件补全、``app.js`` 脚手架与安全检查
   * - 设计导入
     - Figma 社区插件 → JSON UI 导出会话；参考图 → 图标提取 / 基元绘制
   * - 视觉回归
     - 仿真截图与参考图像素对比；``brookesia_visual_loop`` 一键收敛
   * - 设备 Service
     - 通过 MCP Resource 提供 ``@brookesia/service`` API 目录；可校验板级能力
   * - Agent Skills
     - 环境搭建、开发、设计、截图转 UI、自动测试等分场景指引

.. _plugin-sec-01:

安装
~~~~~~~~~~~~~~~~~~~~~~~~~~

需要 Node.js 20 或更高 (``>=20``), 且 ``npm`` / ``npx`` 在 ``PATH`` 中。
与已发布插件和 Toolkit 的 ``engines.node`` 一致（安装器也会校验主版本 ≥ 20）。

交互安装（不必全局安装插件）：

.. code-block:: bash

   npx @brookesia/app-dev-plugin
   npx @brookesia/app-dev-plugin cursor
   npx @brookesia/app-dev-plugin codex

日常使用（全局安装）：

.. code-block:: bash

   npm install -g @brookesia/app-dev-plugin@latest
   brookesia-plugin

安装器也可以安装或升级 ``esp-brookesia-toolkit``。写完 MCP 后，在 IDE 中 **Reload MCP**。
对话里应出现 ``brookesia-app-dev`` 服务器。

之后单独升级两个全局包（不改写 MCP）：

.. code-block:: bash

   npm install -g @brookesia/app-dev-plugin@latest
   npm install -g esp-brookesia-toolkit@latest

然后 **Reload MCP**。全局安装后若 ``brookesia`` 不在 ``PATH``，请重开终端。

可选依赖（安装器可提示安装）：

.. list-table:: 可选依赖
   :header-rows: 1
   :widths: 30 70

   * - 依赖
     - 用途
   * - Playwright Chromium
     - WASM ``mode=canvas`` 截图、像素对比（约 150MB）
   * - ``curl``
     - 本地仿真器 HTTP 探测 (``/api/health``、``/api/screenshot``)
   * - ``brookesia-usb-cli``
     - ``brookesia_deploy`` / ``brookesia_device_status``；需 **USB Serial/JTAG** (如 ``/dev/ttyACM0``)，不是普通 USB 转 UART

常用安装器参数：

.. code-block:: bash

   brookesia-plugin --help
   brookesia-plugin deps --yes          # 只装/升 toolkit 与缺失依赖，不配 MCP
   brookesia-plugin --yes cursor        # 非交互安装到 Agent ``cursor`` (必须带 Agent id)
   brookesia-plugin --yes-usb-cli       # 自动安装 brookesia-usb-cli
   brookesia-plugin --npx               # 强制 MCP 用 npx 启动
   brookesia-plugin --local             # 强制 MCP 用本地绝对路径（更快 / 离线）

``--yes`` 只会自动接受插件 / toolkit / 可选依赖等提示， **不会** 选择要写入 MCP 的 Agent。
非交互运行需要带上 Agent id (例如 ``cursor`` / ``codex``)，或使用 ``deps`` 子命令。

.. _plugin-sec-02:

各 Agent 的 MCP 位置
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table:: 常见 MCP 配置路径
   :header-rows: 1
   :widths: 22 78

   * - Agent
     - 做法
   * - Cursor
     - 安装器写入 ``~/.cursor/mcp.json`` → Settings → MCP → Reload
   * - Claude Code / Desktop
     - ``~/.claude/mcp.json`` 或 ``~/.claude.json``
   * - Codex CLI
     - ``codex mcp add brookesia-app-dev`` 或 ``~/.codex/config.toml``
   * - OpenCode、Windsurf、Trae、Gemini、Copilot、ZCode
     - 安装器写入各产品自己的 MCP 配置

验收建议：

.. code-block:: text

   终端：brookesia --version
   Agent：列出 MCP 工具 → 应看到 brookesia_build / brookesia_simulate / brookesia_visual_loop 等
   Agent：读取 brookesia://schemas/service-catalog → 应返回 Service API 目录
   Agent：调用 brookesia_doctor（指定 projectDir）→ capabilities.ok === true

.. _plugin-sec-03:

MCP 工具与资源（技术要点）
~~~~~~~~~~~~~~~~~~~~~~~~~~

插件当前提供约 **32 个 MCP Tools** 与 **9 个 MCP Resources**。
Agent 应优先调用这些工具，而不是在 shell 里手写一长串等价命令。

工具分组：

.. list-table:: MCP 工具分组
   :header-rows: 1
   :widths: 28 72

   * - 分组
     - 代表工具
   * - 工程 / CLI
     - ``brookesia_init``、``brookesia_doctor``、``brookesia_build``、``brookesia_pack``、``brookesia_verify``、``brookesia_release``
   * - 仿真
     - ``brookesia_simulate``、``brookesia_simulate_reload``、``brookesia_simulate_stop``、``brookesia_simulator_probe``
   * - JSON UI
     - ``validate_gui_json``、``inspect_json_ui_layout``、``list_json_ui_elements``、``enrich_interactive_widgets``、``scaffold_app_js``
   * - 设计导入
     - ``prepare_json_ui_export``、``get_json_ui_export_status``、``cancel_json_ui_export``
   * - 视觉 / 图标
     - ``prepare_reference_screenshot``、``brookesia_visual_loop``、``compare_simulator_to_reference``、``extract_icons_from_reference``
   * - 真机
     - ``brookesia_deploy``、``brookesia_device_status``、``brookesia_board_capabilities``、``brookesia_validate_service_usage``

常用 Resource URI（在 Agent 中「读取 ``brookesia://...``」）：

.. list-table:: MCP Resources
   :header-rows: 1
   :widths: 48 52

   * - URI
     - 内容
   * - ``brookesia://schemas/gui``
     - JSON UI Schema
   * - ``brookesia://schemas/manifest``
     - ``manifest.json`` Schema
   * - ``brookesia://schemas/service-catalog``
     - ``@brookesia/service`` API 目录
   * - ``brookesia://schemas/board-capabilities``
     - 开发板硬件目录（不要写进 app 配置）
   * - ``brookesia://guides/json-ui-and-bpk``
     - JSON UI + 应用包编写指南
   * - ``brookesia://skills/figma-to-json-ui``
     - Figma → JSON UI Skill
   * - ``brookesia://skills/screenshot-to-json-ui``
     - 截图转 JSON UI Skill

技术约定：

- 多数工程类工具需要 ``projectDir`` (应用根目录绝对路径)。
- ``brookesia_build`` / ``brookesia_board_capabilities`` 可传 ``boardId`` 或 ``probeDevice``，但 **不要把板型写入** ``brookesia.config.js``。
- 仿真截图依赖 Playwright Chromium；仅做 build/pack 可不装。
- 完整工具参数以 Agent 侧 MCP schema 与插件 README 为准。

.. _plugin-sec-04:

常见 Agent 工作流
~~~~~~~~~~~~~~~~~~~~~~~~~~

**1. 新建并运行应用**

1. 让 Agent 调用 ``brookesia_init`` (或等价 ``brookesia init``)。
2. 编辑 ``src/res/`` 下 JSON UI 与 ``src/app/`` 逻辑。
3. ``brookesia_build`` → ``brookesia_simulate`` 做浏览器预览。
4. ``brookesia_pack`` / ``brookesia_verify`` 产出 ``.bpk``。

也可直接对 Agent 说：「在当前目录初始化 js-gui 应用并仿真」。

**2. Figma → JSON UI**

前提：Agent 已挂上 ``brookesia-app-dev`` MCP；Figma **桌面端** 已安装社区插件
`ESP-Brookesia JSON UI <https://www.figma.com/community/plugin/1675135947357457765/esp-brookesia-json-ui>`__。
这不是 Figma MCP，也不是「截图转 UI」。

1. 在 Agent 中说「把 Figma 导出到这个 app」。
2. Agent 调用 ``prepare_json_ui_export``，并 **展示一次性 token**。
3. 在 Figma：**Export Package** → 展开 **Send to Brookesia Agent** → 粘贴 token → **Send**。
4. 不要用 Copy Bundle；token 通常约 10 分钟、一次性。Agent 无法代贴。

**3. 截图 → JSON UI**

1. 提供参考截图。
2. 优先让 Agent 走 ``brookesia_visual_loop`` (规范化 → 构建 → 仿真 → 对比)。
3. 不要手工串 ``prepare_reference_screenshot`` + ``compare_simulator_to_reference``，除非在排查单步问题。

**4. 调用设备 Service** (Storage / Wi-Fi 等)

1. 读取 ``brookesia://schemas/service-catalog``。
2. 用户点名开发板或已连 USB 时，先 ``brookesia_board_capabilities`` / ``brookesia_validate_service_usage``。
3. 在 ``app.js`` 中通过 ``@brookesia/service`` 调用对应 API。
4. PC 仿真可验证部分逻辑；依赖真实硬件的 Service 仍需设备固件。

**5. 真机部署**

``brookesia_deploy`` 使用 :ref:`system-usb-cli-sec-00` (``brookesia-usb install``)：

- 固件需启用 USB CDC / USB service。
- 主机口必须是 **USB Serial/JTAG** (常见 ``/dev/ttyACM0``)。
- 仅有 **USB 转 UART** (如 CP2102 的 ``/dev/ttyUSB0``) 时，``brookesia_deploy`` **不可用**；应改用 littlefs 预置或 SD/网络安装。
- ``deploy`` 与 ``device_status`` 之间建议间隔约 1–2 秒：USB 控制会话是独占的，紧跟调用可能 busy。

.. _plugin-sec-05:

升级、卸载与更多细节
~~~~~~~~~~~~~~~~~~~~~~~~~~

升级全局包后请 **Reload MCP**：

.. code-block:: bash

   npm install -g @brookesia/app-dev-plugin@latest esp-brookesia-toolkit@latest
   # 或不改写 MCP、只处理依赖：
   npx @brookesia/app-dev-plugin deps --yes

卸载 MCP 配置：

.. code-block:: bash

   brookesia-plugin uninstall
   brookesia-plugin uninstall --yes

安装器参数、各 Agent manifest、Skills/Hooks 包形态，以及完整工具表见
`@brookesia/app-dev-plugin <https://www.npmjs.com/package/@brookesia/app-dev-plugin>`__ npm 页面。

相关页面：

- Toolkit CLI：:ref:`system-toolkit-sec-00`
- USB 主机工具：:ref:`system-usb-cli-sec-00`
