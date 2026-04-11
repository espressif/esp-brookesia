.. _hal-index-sec-00:

HAL Components
==============

:link_to_translation:`zh_CN:[中文]`

ESP-Brookesia HAL consists of three components that work together in layers, bridging the gap between board-level hardware and upper-layer business logic:

.. only:: html

   .. mermaid::

      flowchart TD
          A["Upper Layer (Services / Examples)"]
          B["**brookesia_hal_interface**<br/>· Device / Interface abstract base classes<br/>· Audio, display, storage HAL definitions<br/>· Global interface registry"]
          C["**brookesia_hal_adaptor**<br/>· AudioDevice / DisplayDevice / StorageDevice implementations<br/>· Init peripherals via esp_board_manager<br/>· Register interface instances globally"]
          D["**brookesia_hal_boards**<br/>· Peripheral topology (pins, buses)<br/>· Logical device config (codec, LCD)<br/>· Board Kconfig defaults & callbacks"]

          A -->|"discover & call by interface name"| B
          B -->|"implements abstract interfaces"| C
          C -->|"provides YAML configuration"| D

.. only:: latex

   .. image:: ../../_static/hal/index_diagram_en.png
      :width: 100%

- ``brookesia_hal_interface``: **Defines abstract interfaces**; upper-layer code depends only on this layer and remains decoupled from hardware details.
- ``brookesia_hal_adaptor``: **Implements the abstract interfaces** by reading board configuration via ``esp_board_manager`` and initializing real peripherals.
- ``brookesia_hal_boards``: **Provides board-level YAML configuration** describing the peripheral topology, pin assignments, and driver parameters for each supported board.

.. note::

   Custom boards can be integrated into the ESP-Brookesia HAL framework in two ways:

   - **Option 1 (Recommended)**: Create a new board subdirectory under ``brookesia_hal_boards/boards/`` following the ``esp_board_manager`` specification and add the required configuration files. No changes to the adaptor layer are needed. See :ref:`hal-boards-sec-08`.
   - **Option 2 (Fully Custom)**: Remove the dependencies on ``brookesia_hal_adaptor`` and ``brookesia_hal_boards``, and implement board-level initialization directly against the abstract interfaces in ``brookesia_hal_interface``. This is suitable for cases where ``esp_board_manager`` cannot be used, but requires maintaining compatibility with the interface specification manually.

.. toctree::
   :maxdepth: 1

   HAL Interface <interface/index>
   HAL Adaptor <adaptor>
   HAL Boards <boards>
