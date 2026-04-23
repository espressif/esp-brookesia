.. _hal-boards-sec-00:

HAL Board Support
=================

:link_to_translation:`zh_CN:[中文]`

- Component Registry: `espressif/brookesia_hal_boards <https://components.espressif.com/components/espressif/brookesia_hal_boards>`_

.. _hal-boards-sec-01:

Overview
--------

``brookesia_hal_boards`` is the board configuration collection for ESP-Brookesia. It uses YAML files to describe the peripheral topology and device parameters for each supported board, allowing :ref:`HAL Adaptor <hal-adaptor-sec-00>` to initialize hardware at runtime without any board-specific hardcoding.

.. _hal-boards-sec-02:

Supported Boards
----------------

- :doc:`Espressif Boards <espressif>`

.. toctree::
   :hidden:

   Espressif Boards <espressif>

.. _hal-boards-sec-03:

Directory Structure
-------------------

Each board has its own subdirectory under ``boards/<vendor>/<board-name>/`` containing the following files:

.. code-block:: text

   boards/
   └── <vendor>/
       └── <board>/
          ├── components/...           # Special component dependencies (optional, for non-standard peripherals or special features)
          ├── board_info.yaml          # Board metadata (name, chip, version, manufacturer, etc.)
          ├── board_devices.yaml       # Logical device configurations (audio codec, LCD, touch, storage, etc.)
          ├── board_peripherals.yaml   # Low-level peripheral configurations (I2C/I2S/SPI buses, GPIO, LEDC, etc.)
          ├── sdkconfig.defaults.board # Board-specific Kconfig defaults (Flash, PSRAM, etc.)
          └── setup_device.c           # Board-specific device factory callbacks (optional, for custom driver initialization)

.. note::

   For the complete board configuration format reference, see the `esp_board_manager component documentation <https://github.com/espressif/esp-gmf/blob/main/packages/esp_board_manager/README.md>`_.

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
     - Audio codec chip (DAC playback / ADC recording); supports ES8311, ES7210, internal ADC, and more
   * - ``display_lcd``
     - LCD display; supports SPI (ST77916, ILI9341), DSI (EK79007), and other interfaces
   * - ``lcd_touch``
     - Touch panel; supports CST816S, GT911, and other I2C touch controllers
   * - ``ledc_ctrl``
     - PWM backlight control via LEDC
   * - ``fs_fat`` / ``fs_spiffs``
     - File system storage; supports SD cards (SDMMC/SPI) and SPIFFS
   * - ``camera``
     - Camera (CSI interface)
   * - ``power_ctrl``
     - GPIO-based power control (audio power, LCD/SD card power, and more)
   * - ``gpio_ctrl``
     - General-purpose GPIO control (LEDs, buttons, and more)
   * - ``gpio_expander``
     - GPIO expander chip (e.g. TCA9554)
   * - ``custom``
     - Custom device type; for implementing non-standard peripherals or special features

.. _hal-boards-sec-05:

Peripheral Configuration
^^^^^^^^^^^^^^^^^^^^^^^^

``board_peripherals.yaml`` describes the pin assignments and parameters for low-level hardware resources:

- **I2C**: SDA/SCL pins and port number
- **I2S**: MCLK/BCLK/WS/DOUT/DIN pins, sample rate, and bit depth
- **SPI**: MOSI/MISO/CLK/CS pins, SPI host number, and transfer size
- **LEDC**: Backlight GPIO, PWM frequency, and resolution
- **GPIO**: Standalone pin configurations such as power control, amplifier enable, and LEDs

``sdkconfig.defaults.board`` contains Kconfig defaults tightly coupled to the board hardware, such as Flash size, PSRAM mode and frequency, CPU clock frequency, and audio recording format parameters for ``brookesia_hal_adaptor``.

If a driver requires a custom initialization flow, such as passing a vendor-specific register sequence to an LCD driver, it is implemented through factory callbacks in ``setup_device.c``.

.. _hal-boards-sec-06:

Usage
-----

.. _hal-boards-sec-07:

Select a Board
^^^^^^^^^^^^^^

See :ref:`How to Use Example Projects <getting-started-example-projects>`.

.. _hal-boards-sec-08:

Add a Custom Board
^^^^^^^^^^^^^^^^^^

Create a new board subdirectory under ``boards/<vendor>/`` and add the following files in order:

1. **board_info.yaml**: Fill in the board name, chip model, version, and description.
2. **board_peripherals.yaml**: Configure peripheral parameters based on the actual pins and buses.
3. **board_devices.yaml**: Describe on-board devices with their types and configurations.
4. **sdkconfig.defaults.board**: Add board-specific Kconfig defaults.
5. **setup_device.c** *(optional)*: Implement factory functions if the driver requires extra initialization steps.
6. **components/...** *(optional)*: Add special component dependencies for non-standard peripherals or special features.

Once done, run ``idf.py gen-bmgr-config -b <new_board>`` to use the new board.
