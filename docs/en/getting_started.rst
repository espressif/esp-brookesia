.. _getting-started-guide:

Getting Started
===============

:link_to_translation:`zh_CN:[中文]`

This guide explains how to obtain and use ESP-Brookesia components and how to build and run example projects.

.. _getting-started-versioning:

ESP-Brookesia Versioning
------------------------

From **v0.7**, ESP-Brookesia is componentized. Obtain components via the component registry as follows:

1. Components evolve independently but share the same **major.minor** version and depend on the same ESP-IDF release.
2. The **release** branch maintains historical major versions; **master** integrates new features.

Version support:

.. list-table:: ESP-Brookesia version support
   :header-rows: 1
   :widths: 20 20 30 20

   * - ESP-Brookesia
     - ESP-IDF
     - Main changes
     - Status
   * - master (v0.7)
     - >= v5.5, < 6.0
     - Component manager support
     - Active development
   * - release/v0.6
     - >= v5.3, <= 5.5
     - Preview system framework; ESP-VoCat firmware project
     - End of maintenance

.. _getting-started-dev-environment:

Development Environment Setup
-----------------------------

ESP-IDF is Espressif’s framework for ESP series chips:

- Libraries and headers provide core building blocks for ESP SoC software.
- Tools for build, flash, debug, and measurement are included for development and production.

.. note::

   - Follow the `ESP-IDF Programming Guide <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/index.html>`__ to set up the ESP-IDF environment.
   - It is not recommended to install the ESP-IDF environment using the VSCode extension, as this may cause build failures for some examples that depend on the `esp_board_manager` component.

.. _getting-started-hardware:

Hardware Preparation
--------------------

ESP SoCs typically provide:

- Wi-Fi (2.4 GHz / 5 GHz dual band where applicable)
- Bluetooth 5.x (BLE / Mesh)
- High-performance multi-core CPUs (up to ~400 MHz)
- Ultra-low-power coprocessor and deep sleep
- Rich peripherals:
    - General-purpose: GPIO, UART, I2C, I2S, SPI, SDIO, USB OTG, etc.
    - Dedicated: LCD, camera, Ethernet, CAN, touch, LED PWM, temperature sensors, and more
- Memory:
    - Up to ~768 KB internal RAM
    - Optional external PSRAM
    - Optional external Flash
- Security:
    - Hardware crypto engine
    - Secure boot
    - Flash encryption
    - Digital signature

ESP SoCs use advanced process technology and offer leading RF performance, low power, and reliability for IoT, industrial, smart home, wearables, and similar applications.

.. note::

   - Refer to the `ESP Product Selector <https://products.espressif.com/>`__ for per-series details.
   - Refer to `ESP-Brookesia HAL Boards <../hal/boards/index>`__ for supported boards.
   - ESP-Brookesia requires a Flash capacity of at least 8 MB and a PSRAM capacity of at least 4 MB.


.. _getting-started-component-usage:

How to Obtain and Use Components
--------------------------------

Use the `ESP Component Registry <https://components.espressif.com/>`__ to add ESP-Brookesia components.

Example: add **brookesia_service_wifi**:

1. **Command line**

   From your project directory:

   .. code-block:: bash

      idf.py add-dependency "espressif/brookesia_service_wifi"

2. **Manifest**

   Create or edit *idf_component.yml*:

   .. code-block:: yaml

      dependencies:
         espressif/brookesia_service_wifi: "*"

See `Espressif IDF Component Manager Docs <https://docs.espressif.com/projects/idf-component-manager/en/latest/>`__ for more.

.. _getting-started-example-projects:

How to Use Example Projects
---------------------------

ESP-Brookesia provides multiple example projects. Some of them support online flashing through `ESP Launchpad <https://espressif.github.io/esp-brookesia/index.html>`__, allowing you to flash prebuilt firmware and view serial output directly in the browser without setting up a local development environment first.

.. only:: html

   .. raw:: html

      <div style="text-align: center;">
         <a href="https://espressif.github.io/esp-brookesia/index.html">
            <img alt="Try it with ESP Launchpad" src="https://dl.espressif.com/AE/esp-dev-kits/new_launchpad.png" width="400">
         </a>
         <p>Click the image to try with ESP Launchpad</p>
      </div>
      <br>

The typical build and flash steps for an example project are as follows:

1. Select the target chip or development board. The choice depends on the peripherals required by the example and usually falls into one of the following cases:

   - **Select a target chip**:

      For ``examples/service/wifi``, the project only relies on the chip's built-in Wi-Fi peripheral, so you only need to select a target chip such as ``esp32s3``:

      .. code-block:: bash

         idf.py set-target <target>

   - **Select a target development board**:

      For ``examples/service/console``, the project depends on audio peripherals provided by the board, so you need to select a target development board such as ``esp_vocat_board_v1_2``:

      .. code-block:: bash

         idf.py gen-bmgr-config -b <board>
         idf.py set-target <target>

      .. note::

         These projects include an ``idf_ext.py`` script in the project directory. Compared with regular example projects that simply depend on ``esp_board_manager``, this script provides a few extra conveniences:

         - No manual ``IDF_EXTRA_ACTIONS_PATH`` setup is required.
         - When a board is selected, ``esp_board_manager`` and ``brookesia_hal_boards`` are downloaded automatically based on the dependencies declared in ``idf_component.yml``.
         - When ``idf.py gen-bmgr-config`` is executed, the script automatically adds the ``-c`` option to point to a ``boards/`` directory and searches in the following order: the project's local ``boards/`` directory first, then the ``boards/`` directory inside the ``brookesia_hal_boards`` component.
         - If needed, you can still provide a custom directory manually with the ``-c`` option.

      .. note::

         Refer to :ref:`HAL Board Support <hal-boards-sec-02>` for the full list of supported boards.

2. Optional configuration:

    .. code-block:: bash

        idf.py menuconfig

3. Build and flash:

    .. code-block:: bash

        idf.py build
        idf.py -p <PORT> flash

4. Monitor serial output:

    .. code-block:: bash

        idf.py -p <PORT> monitor

More examples are available under `examples/ <https://github.com/espressif/esp-brookesia/tree/master/examples>`__. For detailed usage instructions, refer to the README file in each example directory.
