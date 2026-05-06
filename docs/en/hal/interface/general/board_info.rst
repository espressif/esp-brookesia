.. _hal-interface-general-board-info-sec-00:

Board Information Interface
===========================

:link_to_translation:`zh_CN:[中文]`

Public header: ``#include "brookesia/hal_interface/interfaces/general/board_info.hpp"``

Class: ``BoardInfoIface``

``BoardInfoIface`` exposes static board metadata, such as the board name, main chip, hardware version, description, and manufacturer. Upper layers can use it to identify the current board at runtime and adapt displayed device information or board-specific logic.

.. _hal-interface-general-board-info-sec-01:

API Reference
-------------

.. include-build-file:: inc/board_info.inc
