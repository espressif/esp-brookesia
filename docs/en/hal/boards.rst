.. _hal-boards-sec-00:

HAL Boards
==========

:link_to_translation:`zh_CN:[中文]`

- Component Registry: `espressif/brookesia_hal_boards <https://components.espressif.com/components/espressif/brookesia_hal_boards>`_

.. _hal-boards-sec-01:

Overview
--------

``brookesia_hal_boards`` is the board configuration collection for ESP-Brookesia. It uses YAML files to describe the peripheral topology and device parameters for each supported board, allowing :ref:`hal-adaptor-sec-00` to initialize hardware at runtime without any board-specific hardcoding.

.. _hal-boards-sec-02:

Supported Boards
----------------

.. list-table::
   :widths: 35 15 50
   :header-rows: 1

   * - Board Name
     - Chip
     - Description
   * - ``esp_vocat_board_v1_0``
     - ESP32-S3
     - ESP VoCat Board V1.0 — AI Pet Companion Development Board
   * - ``esp_vocat_board_v1_2``
     - ESP32-S3
     - ESP VoCat Board V1.2 — AI Pet Companion Development Board
   * - ``esp_box_3``
     - ESP32-S3
     - ESP-BOX-3 Development Board
   * - ``esp32_s3_korvo2_v3``
     - ESP32-S3
     - ESP32-S3-Korvo-2 V3 Development Board
   * - ``esp32_p4_function_ev``
     - ESP32-P4
     - ESP32-P4-Function-EV-Board
   * - ``esp_sensair_shuttle``
     - ESP32-C5
     - ESP Sensair Shuttle Module

.. _hal-boards-sec-03:

Directory Structure
-------------------

Each board has its own subdirectory under ``boards/<board-name>/`` containing the following files:

.. code-block:: text

   boards/
   └── <board>/
       ├── board_info.yaml          # Board metadata (name, chip, version, manufacturer, etc.)
       ├── board_devices.yaml       # Logical device configurations (audio codec, LCD, touch, storage, etc.)
       ├── board_peripherals.yaml   # Low-level peripheral configurations (I2C/I2S/SPI buses, GPIO, LEDC, etc.)
       ├── sdkconfig.defaults.board # Board-specific Kconfig defaults (Flash, PSRAM, etc.)
       └── setup_device.c           # Board-specific device factory callbacks (for custom driver initialization)

.. _hal-boards-sec-04:

Device Types
^^^^^^^^^^^^

``board_devices.yaml`` describes the logical functional modules on the board. Common device types include:

.. list-table::
   :widths: 25 75
   :header-rows: 1

   * - Device Type
     - Description
   * - ``audio_codec``
     - Audio codec chip (DAC playback / ADC recording); supports ES8311, ES7210, internal ADC, etc.
   * - ``display_lcd``
     - LCD display; supports SPI (ST77916, ILI9341), DSI (EK79007), and other interfaces
   * - ``lcd_touch``
     - Touch panel; supports CST816S, GT911, and other I2C touch controllers
   * - ``ledc_ctrl``
     - PWM backlight control via LEDC
   * - ``fs_fat`` / ``fs_spiffs``
     - File system storage; supports SD card (SDMMC/SPI) and SPIFFS
   * - ``camera``
     - Camera (CSI interface)
   * - ``power_ctrl``
     - GPIO-based power control (audio power, LCD/SD card power, etc.)
   * - ``gpio_ctrl``
     - General-purpose GPIO control (LEDs, buttons, etc.)

.. _hal-boards-sec-05:

Peripheral Configuration
^^^^^^^^^^^^^^^^^^^^^^^^

``board_peripherals.yaml`` describes the pin assignments and parameters for low-level hardware resources:

- **I2C**: SDA/SCL pins, port number
- **I2S**: MCLK/BCLK/WS/DOUT/DIN pins, sample rate, bit depth
- **SPI**: MOSI/MISO/CLK/CS pins, SPI host, transfer size
- **LEDC**: Backlight GPIO, PWM frequency and resolution
- **GPIO**: Power control, amplifier enable, LED, and other standalone pin configurations

``sdkconfig.defaults.board`` contains board-specific Kconfig defaults tightly coupled to the hardware, such as Flash size, PSRAM mode and speed, CPU frequency, and ``brookesia_hal_adaptor`` audio recording parameters.

If a driver requires a custom initialization flow (e.g., injecting vendor-specific register sequences into an LCD driver), this is handled through factory callbacks in ``setup_device.c``.

.. _hal-boards-sec-06:

Usage
-----

.. _hal-boards-sec-07:

Select a Board
^^^^^^^^^^^^^^

Run the following command in the project root directory, specifying the target board name:

.. code-block:: bash

   idf.py gen-bmgr-config -b <board>

Available ``<board>`` values are listed in :ref:`hal-boards-sec-02`. If ``brookesia_hal_boards`` is included as a local path dependency, specify the ``boards/`` directory with the ``-c`` option:

.. code-block:: bash

   idf.py gen-bmgr-config -b <board> -c path/to/brookesia_hal_boards/boards

.. note::

   In example projects that use ``idf_ext.py``, the ``-c`` argument is injected automatically at build time and does not need to be added manually.

.. _hal-boards-sec-08:

Add a Custom Board
^^^^^^^^^^^^^^^^^^

Create a new board subdirectory under ``boards/`` (or any custom directory) and add the following files in order:

1. **board_info.yaml**: Fill in the board name, chip, version, and description.
2. **board_peripherals.yaml**: Configure peripherals according to the actual pin and bus assignments.
3. **board_devices.yaml**: Describe the on-board devices with their types and configurations.
4. **sdkconfig.defaults.board**: Add board-specific Kconfig defaults.
5. **setup_device.c** *(optional)*: Implement factory functions if the driver requires extra initialization steps.

Once done, run ``idf.py gen-bmgr-config -b <new_board>`` to use the new board.

.. note::

   For the complete ``esp_board_manager`` configuration format reference, see the `esp_board_manager component documentation <https://github.com/espressif/esp-gmf/blob/main/packages/esp_board_manager/README.md>`_.
