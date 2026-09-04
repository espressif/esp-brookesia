.. _system-toolkit-sec-00:

Toolkit
-------

:link_to_translation:`en:[English]`

ESP-Brookesia Toolkit 是用于初始化、构建、打包、校验、仿真和部署 ESP-Brookesia 应用包（``.bpk``）的 npm CLI。

已发布包：`esp-brookesia-toolkit <https://www.npmjs.com/package/esp-brookesia-toolkit>`__。

Agent IDE 集成（MCP）见 :ref:`plugin-sec-00`。

.. _system-toolkit-sec-01:

环境要求
~~~~~~~~~~~~~~~~~~~~~~~~~~

- Node.js 20 或更高（``>=20``），且 ``npm`` / ``npx`` 在 ``PATH`` 中。与已发布 Toolkit / 插件的 ``engines.node`` 一致。
- ``brookesia init --template js-bundle`` 之后，在应用目录执行 ``npm install``（模板会拉取 ``@rspack/cli``）。``js-gui``、``lua-gui``、``wasm-gui`` 没有额外的 npm 依赖。

.. _system-toolkit-sec-02:

安装 Toolkit
~~~~~~~~~~~~~~~~~~~~~~~~~~

最终用户全局安装已发布的 CLI：

.. code-block:: bash

   npm install -g esp-brookesia-toolkit
   brookesia --help

WASM 模拟器包 ``@brookesia/simulator-wasm`` 会作为 CLI 依赖安装，并从全局 CLI 解析，而不是应用的 ``node_modules``。``brookesia simulate`` 需要该包内已 staging 的模拟器产物。若缺失，``brookesia doctor`` 会报告，simulate 会失败。

之后单独升级 CLI：

.. code-block:: bash

   npm install -g esp-brookesia-toolkit@latest

全局安装后若 ``brookesia`` 不在 ``PATH`` 中，请重开终端。

.. _system-toolkit-sec-03:

快速开始
~~~~~~~~~~~~~~~~~~~~~~~~~~

创建并运行一个 JavaScript GUI 应用：

.. code-block:: bash

   brookesia init my-app --template js-gui
   cd my-app
   brookesia doctor
   brookesia build
   brookesia simulate

``brookesia build`` 生成 debug 版 ``.bpk``。签名发布：

.. code-block:: bash

   brookesia sign init
   brookesia release
   brookesia verify

把 ``dist/`` 中最新的 ``.bpk`` 装到真机（依赖 :ref:`system-usb-cli-sec-00`）：

.. code-block:: bash

   brookesia deploy
   brookesia deploy --port /dev/ttyACM0

.. _system-toolkit-sec-04:

应用模板
~~~~~~~~~~~~~~~~~~~~~~~~~~

``brookesia init`` 支持以下模板：

.. list-table:: Toolkit 应用模板
   :header-rows: 1
   :widths: 20 35 45

   * - 模板
     - 用途
     - 说明
   * - ``js-gui``
     - JavaScript GUI 应用
     - 默认模板，包含 GUI 资源
   * - ``js-bundle``
     - JavaScript 打包应用
     - rspack 风格 bundler；init 后需在应用目录 ``npm install``
   * - ``lua-gui``
     - Lua GUI 应用
     - 无额外 npm 依赖
   * - ``wasm-gui``
     - WebAssembly GUI 应用
     - 在 ``manifest.runtime.entry`` 提供预构建 ``.wasm``。pipeline 已不再做原生 WASM 编译；配置 ``wasm.sources`` 会被拒绝。

例如：

.. code-block:: bash

   brookesia init test --template js-gui --dir my-app

.. _system-toolkit-sec-05:

CLI 命令
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   brookesia --help
   brookesia <command> --help

.. list-table:: brookesia CLI 命令
   :header-rows: 1
   :widths: 28 72

   * - 命令
     - 说明
   * - ``brookesia init <name>``
     - 从模板创建应用目录，可用 ``--template`` 和 ``--dir`` 指定模板与父目录。
   * - ``brookesia doctor``
     - 检查应用环境、WASM 模拟器产物、bundler、USB CLI 和已连接设备。
   * - ``brookesia build``
     - 开发构建，在 ``dist/`` 生成 debug 版 ``.bpk``。
   * - ``brookesia release``
     - 发布构建，生成已签名的 release 版 ``.bpk``。
   * - ``brookesia sign init``
     - 在 ``sign/`` 中生成签名密钥。
   * - ``brookesia pack``
     - 使用 ``--source-dir`` 和 ``--output`` 将目录打成 ``.bpk``，``--release`` 做发布签名。
   * - ``brookesia verify``
     - 校验 release 版 ``.bpk`` 签名，可用 ``--package`` 指定文件。
   * - ``brookesia simulate``
     - 启动 WASM 浏览器模拟器（本地 HTTP 服务）。没有 ``--target`` 参数。
   * - ``brookesia deploy``
     - 通过 ``brookesia-usb`` 把 ``.bpk`` 装到设备；``--package`` 和 ``--port`` 可选。

.. _system-toolkit-sec-06:

WASM 模拟器
~~~~~~~~~~~~~~~~~~~~~~~~~~

``brookesia simulate`` 只启动 WASM 浏览器模拟器：

.. code-block:: bash

   brookesia simulate
   brookesia simulate --package dist/my-app.debug.bpk
   brookesia simulate --smoke --duration-ms 2000
   brookesia simulate --gui-debug
   brookesia simulate --resolution 1024x600
   brookesia simulate --no-open --port 8787 --pid-file /tmp/brookesia-sim.pid
   brookesia simulate --stop --pid-file /tmp/brookesia-sim.pid

.. list-table:: simulate 选项
   :header-rows: 1
   :widths: 30 70

   * - 选项
     - 说明
   * - ``--package <bpk>``
     - 加载指定 ``.bpk``（默认 ``dist/`` 中最新文件）。
   * - ``--smoke``
     - 无头冒烟：启动未运行的应用后退出。
   * - ``--duration-ms <ms>``
     - ``--smoke`` 时每个应用的持续时间。
   * - ``--gui-debug``
     - JSON UI 调试描边。
   * - ``--resolution WxH``
     - 窗口尺寸，例如 ``1024x600``。
   * - ``--no-open``
     - 不自动打开浏览器。
   * - ``--port N``
     - HTTP 端口（默认 ``8787``）。
   * - ``--pid-file <path>``
     - 写入服务 PID；与 ``--stop`` 一起使用时必填。
   * - ``--stop``
     - 停止 ``--pid-file`` 记录的服务。

.. _system-toolkit-sec-07:

相关页面
~~~~~~~~~~~~~~~~~~~~~~~~~~

- ``brookesia deploy`` 使用的 USB Serial/JTAG 主机工具：:ref:`system-usb-cli-sec-00`
- 主机端 Agent 插件（MCP）：:ref:`plugin-sec-00`
