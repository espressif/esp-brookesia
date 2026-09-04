.. _system-toolkit-sec-00:

Toolkit
-------

:link_to_translation:`zh_CN:[中文]`

ESP-Brookesia Toolkit is the npm CLI for initializing, building, packing, verifying, simulating, and deploying ESP-Brookesia app packages (``.bpk``).

The published package is `esp-brookesia-toolkit <https://www.npmjs.com/package/esp-brookesia-toolkit>`__.

Agent IDE integration (MCP) is documented separately: :ref:`plugin-sec-00`.

.. _system-toolkit-sec-01:

Environment Requirements
~~~~~~~~~~~~~~~~~~~~~~~~~~

- Node.js 20 or newer (``>=20``), with ``npm`` / ``npx`` on ``PATH``. Matches the published Toolkit and plugin ``engines.node`` requirement.
- After ``brookesia init --template js-bundle``, run ``npm install`` in the app directory (the template pulls in ``@rspack/cli``). ``js-gui``, ``lua-gui``, and ``wasm-gui`` have no extra npm dependencies.

.. _system-toolkit-sec-02:

Install the Toolkit
~~~~~~~~~~~~~~~~~~~~~~~~~~

End users install the published CLI globally:

.. code-block:: bash

   npm install -g esp-brookesia-toolkit
   brookesia --help

The WASM simulator package ``@brookesia/simulator-wasm`` is installed as a CLI dependency and resolved from the global CLI, not from the app's ``node_modules``. ``brookesia simulate`` needs the staged simulator artifacts inside that package. If they are missing, ``brookesia doctor`` reports it and simulate fails.

To bump the CLI later:

.. code-block:: bash

   npm install -g esp-brookesia-toolkit@latest

After a global install, reopen the terminal if ``brookesia`` is missing from ``PATH``.

.. _system-toolkit-sec-03:

Quick Start
~~~~~~~~~~~~~~~~~~~~~~~~~~

Create and run a JavaScript GUI app:

.. code-block:: bash

   brookesia init my-app --template js-gui
   cd my-app
   brookesia doctor
   brookesia build
   brookesia simulate

``brookesia build`` creates a debug ``.bpk``. For a signed release:

.. code-block:: bash

   brookesia sign init
   brookesia release
   brookesia verify

Install the newest ``.bpk`` in ``dist/`` onto hardware (requires :ref:`system-usb-cli-sec-00`):

.. code-block:: bash

   brookesia deploy
   brookesia deploy --port /dev/ttyACM0

.. _system-toolkit-sec-04:

App Templates
~~~~~~~~~~~~~~~~~~~~~~~~~~

``brookesia init`` supports the following templates:

.. list-table:: Toolkit App Templates
   :header-rows: 1
   :widths: 20 35 45

   * - Template
     - Purpose
     - Notes
   * - ``js-gui``
     - JavaScript GUI app
     - Default template with GUI resources
   * - ``js-bundle``
     - Bundled JavaScript app
     - rspack-style bundler; run ``npm install`` in the app after init
   * - ``lua-gui``
     - Lua GUI app
     - No extra npm dependencies
   * - ``wasm-gui``
     - WebAssembly GUI app
     - Ship a prebuilt ``.wasm`` at ``manifest.runtime.entry``. Native WASM compilation in the pipeline was removed; ``wasm.sources`` is rejected.

Example:

.. code-block:: bash

   brookesia init test --template js-gui --dir my-app

.. _system-toolkit-sec-05:

Command Reference
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   brookesia --help
   brookesia <command> --help

.. list-table:: brookesia CLI Commands
   :header-rows: 1
   :widths: 28 72

   * - Command
     - Description
   * - ``brookesia init <name>``
     - Create an app directory from a template; ``--template`` and ``--dir`` select the template and parent directory.
   * - ``brookesia doctor``
     - Check the app environment, WASM simulator artifacts, bundler, USB CLI, and connected device.
   * - ``brookesia build``
     - Development build; writes a debug ``.bpk`` under ``dist/``.
   * - ``brookesia release``
     - Release build; writes a signed release ``.bpk``.
   * - ``brookesia sign init``
     - Generate signing keys under ``sign/``.
   * - ``brookesia pack``
     - Pack a directory into a ``.bpk`` with ``--source-dir`` and ``--output``; ``--release`` applies release signing.
   * - ``brookesia verify``
     - Verify a release ``.bpk`` signature; ``--package`` selects the file.
   * - ``brookesia simulate``
     - Start the WASM browser simulator (local HTTP server). There is no ``--target`` flag.
   * - ``brookesia deploy``
     - Install a ``.bpk`` onto a device via ``brookesia-usb``; ``--package`` and ``--port`` are optional.

.. _system-toolkit-sec-06:

WASM Simulator
~~~~~~~~~~~~~~~~~~~~~~~~~~

``brookesia simulate`` launches only the WASM browser simulator:

.. code-block:: bash

   brookesia simulate
   brookesia simulate --package dist/my-app.debug.bpk
   brookesia simulate --smoke --duration-ms 2000
   brookesia simulate --gui-debug
   brookesia simulate --resolution 1024x600
   brookesia simulate --no-open --port 8787 --pid-file /tmp/brookesia-sim.pid
   brookesia simulate --stop --pid-file /tmp/brookesia-sim.pid

.. list-table:: simulate Options
   :header-rows: 1
   :widths: 30 70

   * - Option
     - Description
   * - ``--package <bpk>``
     - Load this ``.bpk`` (default: newest file under ``dist/``).
   * - ``--smoke``
     - Headless smoke: start non-running apps, then exit.
   * - ``--duration-ms <ms>``
     - Per-app duration for ``--smoke``.
   * - ``--gui-debug``
     - JSON UI debug outlines.
   * - ``--resolution WxH``
     - Window size, for example ``1024x600``.
   * - ``--no-open``
     - Do not open a browser.
   * - ``--port N``
     - HTTP port (default ``8787``).
   * - ``--pid-file <path>``
     - Write the server PID; required with ``--stop``.
   * - ``--stop``
     - Stop the server recorded by ``--pid-file``.

.. _system-toolkit-sec-07:

Related Pages
~~~~~~~~~~~~~~~~~~~~~~~~~~

- USB Serial/JTAG host CLI used by ``brookesia deploy``: :ref:`system-usb-cli-sec-00`
- Host Agent plugin (MCP): :ref:`plugin-sec-00`
