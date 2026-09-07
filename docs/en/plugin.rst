.. _plugin-sec-00:

App Dev Plugin
--------------

:link_to_translation:`zh_CN:[中文]`

``@brookesia/app-dev-plugin`` is the **host** Agent plugin for ESP-Brookesia GUI app development.
It attaches to Cursor, Claude Code, Codex, and other Agent IDEs/CLIs through **MCP (Model Context Protocol)**
and drives :ref:`system-toolkit-sec-00` (the ``brookesia`` CLI) for init, build, simulate, pack, and deploy.

**It is not:**

- A listing in official IDE plugin stores (the supported path is writing MCP config, not searching an extension marketplace)
- The firmware component ``brookesia_mcp_utils`` (device-side MCP helpers; unrelated to this host plugin)

After install, look for the ``brookesia-app-dev`` server under **Settings → MCP** (or ``/mcp``, ``codex mcp list``).

Install **both** ``@brookesia/app-dev-plugin`` and ``esp-brookesia-toolkit``.
MCP tools such as ``brookesia_init`` / ``brookesia_build`` / ``brookesia_simulate`` fail if the CLI is missing.

Published package: `@brookesia/app-dev-plugin <https://www.npmjs.com/package/@brookesia/app-dev-plugin>`__.

.. _plugin-sec-00a:

Capabilities
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table:: Main plugin capabilities
   :header-rows: 1
   :widths: 28 72

   * - Area
     - Description
   * - App project
     - Init, build, pack, verify, and release ``.bpk`` (via ``brookesia`` CLI)
   * - WASM simulate
     - Run JSON UI in the browser with probe, hot reload, and screenshots
   * - JSON UI
     - Schema validation, layout inspection, widget enrichment, ``app.js`` scaffold and safety checks
   * - Design import
     - Figma community plugin → JSON UI export session; reference images → icon extract / primitive render
   * - Visual regression
     - Compare simulator screenshots to reference images; prefer ``brookesia_visual_loop``
   * - Device services
     - MCP Resources for ``@brookesia/service`` API catalog; optional board capability checks
   * - Agent Skills
     - Scenario guides for setup, develop, design, screenshot-to-UI, and auto-test

.. _plugin-sec-01:

Install
~~~~~~~~~~~~~~~~~~~~~~~~~~

Requires Node.js 20 or newer (``>=20``), with ``npm`` / ``npx`` on ``PATH``.
Matches the published plugin and Toolkit ``engines.node`` requirement (the installer also enforces major ≥ 20).

Interactive (no global plugin required):

.. code-block:: bash

   npx @brookesia/app-dev-plugin
   npx @brookesia/app-dev-plugin cursor
   npx @brookesia/app-dev-plugin codex

Daily use (global install):

.. code-block:: bash

   npm install -g @brookesia/app-dev-plugin@latest
   brookesia-plugin

The installer can also install or upgrade ``esp-brookesia-toolkit``. After MCP is written, **Reload MCP** in the IDE.
The chat should list the ``brookesia-app-dev`` server.

To upgrade the two global packages later (does not rewrite MCP):

.. code-block:: bash

   npm install -g @brookesia/app-dev-plugin@latest
   npm install -g esp-brookesia-toolkit@latest

Then **Reload MCP**. Reopen the terminal if ``brookesia`` is missing from ``PATH``.

Optional dependencies (installer can prompt):

.. list-table:: Optional dependencies
   :header-rows: 1
   :widths: 30 70

   * - Dependency
     - Purpose
   * - Playwright Chromium
     - WASM ``mode=canvas`` screenshots and pixel compare (~150MB)
   * - ``curl``
     - Local simulator HTTP probes (``/api/health``, ``/api/screenshot``)
   * - ``brookesia-usb-cli``
     - ``brookesia_deploy`` / ``brookesia_device_status``; needs **USB Serial/JTAG** (e.g. ``/dev/ttyACM0``), not a plain USB-UART bridge

Common installer flags:

.. code-block:: bash

   brookesia-plugin --help
   brookesia-plugin deps --yes          # toolkit + missing deps only; no MCP rewrite
   brookesia-plugin --yes cursor        # non-interactive install for Agent ``cursor`` (Agent id required)
   brookesia-plugin --yes-usb-cli       # install brookesia-usb-cli
   brookesia-plugin --npx               # force MCP launch via npx
   brookesia-plugin --local             # force absolute local MCP path (faster / offline)

``--yes`` auto-accepts plugin/toolkit/optional-deps prompts; it does **not** pick an Agent.
Non-interactive runs need an Agent id (for example ``cursor`` / ``codex``) or the ``deps`` subcommand.

.. _plugin-sec-02:

Agent MCP Locations
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table:: Typical MCP Config Paths
   :header-rows: 1
   :widths: 22 78

   * - Agent
     - How
   * - Cursor
     - Installer writes ``~/.cursor/mcp.json`` → Settings → MCP → Reload
   * - Claude Code / Desktop
     - ``~/.claude/mcp.json`` or ``~/.claude.json``
   * - Codex CLI
     - ``codex mcp add brookesia-app-dev`` or ``~/.codex/config.toml``
   * - OpenCode, Windsurf, Trae, Gemini, Copilot, ZCode
     - Installer writes each product's MCP config

Smoke checks:

.. code-block:: text

   Terminal: brookesia --version
   Agent: list MCP tools → expect brookesia_build / brookesia_simulate / brookesia_visual_loop
   Agent: read brookesia://schemas/service-catalog → Service API catalog
   Agent: brookesia_doctor (with projectDir) → capabilities.ok === true

.. _plugin-sec-03:

MCP Tools and Resources
~~~~~~~~~~~~~~~~~~~~~~~~~~

The plugin currently exposes about **32 MCP Tools** and **9 MCP Resources**.
Agents should call these tools instead of hand-rolling long equivalent shell pipelines.

Tool groups:

.. list-table:: MCP tool groups
   :header-rows: 1
   :widths: 28 72

   * - Group
     - Representative tools
   * - Project / CLI
     - ``brookesia_init``, ``brookesia_doctor``, ``brookesia_build``, ``brookesia_pack``, ``brookesia_verify``, ``brookesia_release``
   * - Simulate
     - ``brookesia_simulate``, ``brookesia_simulate_reload``, ``brookesia_simulate_stop``, ``brookesia_simulator_probe``
   * - JSON UI
     - ``validate_gui_json``, ``inspect_json_ui_layout``, ``list_json_ui_elements``, ``enrich_interactive_widgets``, ``scaffold_app_js``
   * - Design import
     - ``prepare_json_ui_export``, ``get_json_ui_export_status``, ``cancel_json_ui_export``
   * - Visual / icons
     - ``prepare_reference_screenshot``, ``brookesia_visual_loop``, ``compare_simulator_to_reference``, ``extract_icons_from_reference``
   * - Hardware
     - ``brookesia_deploy``, ``brookesia_device_status``, ``brookesia_board_capabilities``, ``brookesia_validate_service_usage``

Common Resource URIs (ask the Agent to read ``brookesia://...``):

.. list-table:: MCP Resources
   :header-rows: 1
   :widths: 48 52

   * - URI
     - Content
   * - ``brookesia://schemas/gui``
     - JSON UI schema
   * - ``brookesia://schemas/manifest``
     - ``manifest.json`` schema
   * - ``brookesia://schemas/service-catalog``
     - ``@brookesia/service`` API catalog
   * - ``brookesia://schemas/board-capabilities``
     - Board hardware catalog (do not hard-code into app config)
   * - ``brookesia://guides/json-ui-and-bpk``
     - JSON UI + package authoring guide
   * - ``brookesia://skills/figma-to-json-ui``
     - Figma → JSON UI skill
   * - ``brookesia://skills/screenshot-to-json-ui``
     - Screenshot → JSON UI skill

Technical notes:

- Most project tools require ``projectDir`` (absolute app root).
- ``brookesia_build`` / ``brookesia_board_capabilities`` may take ``boardId`` or ``probeDevice``, but **do not write the board into** ``brookesia.config.js``.
- Simulator screenshots need Playwright Chromium; build/pack alone does not.
- Full argument schemas live in the Agent MCP descriptors and the plugin README.

.. _plugin-sec-04:

Typical Agent Workflows
~~~~~~~~~~~~~~~~~~~~~~~~~~

**1. Create and run an app**

1. Ask the Agent to call ``brookesia_init`` (or equivalent ``brookesia init``).
2. Edit JSON UI under ``src/res/`` and logic under ``src/app/``.
3. ``brookesia_build`` → ``brookesia_simulate`` for browser preview.
4. ``brookesia_pack`` / ``brookesia_verify`` to produce a ``.bpk``.

Natural language also works: “init a js-gui app here and simulate it”.

**2. Figma → JSON UI**

Prerequisites: ``brookesia-app-dev`` MCP is connected; Figma **desktop** has the community plugin
`ESP-Brookesia JSON UI <https://www.figma.com/community/plugin/1675135947357457765/esp-brookesia-json-ui>`__.
This is not Figma MCP and not screenshot-to-UI.

1. Tell the Agent to export Figma into the app.
2. Agent calls ``prepare_json_ui_export`` and **shows a one-time token**.
3. In Figma: **Export Package** → **Send to Brookesia Agent** → paste token → **Send**.
4. Do not use Copy Bundle; the token is typically ~10 minutes and single-use. The Agent cannot paste for you.

**3. Screenshot → JSON UI**

1. Provide a reference screenshot.
2. Prefer ``brookesia_visual_loop`` (normalize → build → simulate → compare).
3. Avoid hand-chaining screenshot tools unless debugging a single step.

**4. Device services (Storage / Wi-Fi / …)**

1. Read ``brookesia://schemas/service-catalog``.
2. When a board is named or USB is connected, call ``brookesia_board_capabilities`` / ``brookesia_validate_service_usage`` first.
3. Call APIs from ``app.js`` via ``@brookesia/service``.
4. PC simulate covers some logic; hardware-backed services still need device firmware.

**5. Hardware deploy**

``brookesia_deploy`` uses :ref:`system-usb-cli-sec-00` (``brookesia-usb install``):

- Firmware must enable the USB CDC / USB service.
- The host port must be **USB Serial/JTAG** (often ``/dev/ttyACM0``).
- With only a **USB-UART bridge** (e.g. CP2102 ``/dev/ttyUSB0``), ``brookesia_deploy`` **does not work**; use littlefs staging or SD/network install instead.
- Leave ~1–2 s between ``deploy`` and ``device_status``: the USB control session is exclusive and back-to-back calls may return busy.

.. _plugin-sec-05:

Upgrade, Uninstall, and More
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

After upgrading global packages, **Reload MCP**:

.. code-block:: bash

   npm install -g @brookesia/app-dev-plugin@latest esp-brookesia-toolkit@latest
   # or deps only, no MCP rewrite:
   npx @brookesia/app-dev-plugin deps --yes

Uninstall MCP entries:

.. code-block:: bash

   brookesia-plugin uninstall
   brookesia-plugin uninstall --yes

Installer flags, Agent manifests, Skills/Hooks packaging, and the full tool table are described on the
`@brookesia/app-dev-plugin <https://www.npmjs.com/package/@brookesia/app-dev-plugin>`__ npm page.

See also:

- Toolkit CLI: :ref:`system-toolkit-sec-00`
- USB host tool: :ref:`system-usb-cli-sec-00`
