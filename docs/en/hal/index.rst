.. _hal-index-sec-00:

Hardware Components
===================

:link_to_translation:`zh_CN:[中文]`

ESP-Brookesia HAL consists of three components that work together in layers, bridging the gap between board-level hardware and upper-layer business logic:

.. only:: html

   .. raw:: html
      :file: ../../_static/mermaid/en/hal/index/diagram.html

.. only:: latex

   .. image:: ../../_static/mermaid/en/hal/index/diagram.png
      :width: 100%

- ``brookesia_hal_interface``: **Defines abstract interfaces**; upper-layer code depends only on this layer and remains decoupled from hardware details.
- ``brookesia_hal_adaptor``: **Implements the abstract interfaces** by reading board configuration via ``esp_board_manager`` and initializing real peripherals.
- ``brookesia_hal_boards``: **Provides board-level YAML configuration** describing the peripheral topology, pin assignments, and driver parameters for each supported board.

.. note::

   Custom boards can be integrated into the ESP-Brookesia HAL framework in two ways:

   - **Option 1 (Recommended)**: Create a new board subdirectory under ``brookesia_hal_boards/boards/<vendor>/`` following the ``esp_board_manager`` specification and add the required configuration files. No changes to the adaptor layer are needed. See :ref:`Add a Custom Board <hal-boards-sec-08>`.
   - **Option 2 (Fully Custom)**: Remove the dependencies on ``brookesia_hal_adaptor`` and ``brookesia_hal_boards``, and implement board-level initialization directly against the abstract interfaces in ``brookesia_hal_interface``. This is suitable for cases where ``esp_board_manager`` cannot be used, but requires maintaining compatibility with the interface specification manually.

.. toctree::
   :maxdepth: 1

   Interface Abstraction <interface/index>
   ESP Device Board Adaptation <adaptor>
   ESP Device Board Configuration <boards/index>
   Host Platform Simulation Support <pc_simulation>
