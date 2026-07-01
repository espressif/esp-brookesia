.. _hal-interface-wifi-basic-sec-00:

Wi-Fi 基础接口
==============

:link_to_translation:`en:[English]`

公共头文件： ``#include "brookesia/hal_interface/interfaces/wifi/basic.hpp"``

类名： ``WifiBasicIface``

``WifiBasicIface`` 暴露共享 Wi-Fi 控制面，包括初始化、反初始化、
启动/停止、重置，以及通用 action/event 回调。连接策略由上层 Wi-Fi service 负责，
HAL 只执行单次动作并上报事件。

.. _hal-interface-wifi-basic-sec-01:

API 参考
--------

.. include-build-file:: inc/basic.inc
