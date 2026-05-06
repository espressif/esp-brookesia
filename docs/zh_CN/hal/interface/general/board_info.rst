.. _hal-interface-general-board-info-sec-00:

开发板信息接口
==============

:link_to_translation:`en:[English]`

公共头文件： ``#include "brookesia/hal_interface/interfaces/general/board_info.hpp"``

类名： ``BoardInfoIface``

``BoardInfoIface`` 用于暴露开发板静态元信息，例如开发板名称、主控芯片、硬件版本、描述和制造商。上层可以通过该接口在运行时识别当前板型，并据此展示设备信息或选择板级相关逻辑。

.. _hal-interface-general-board-info-sec-01:

API 参考
--------

.. include-build-file:: inc/board_info.inc
