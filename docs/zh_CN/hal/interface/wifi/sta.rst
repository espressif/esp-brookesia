.. _hal-interface-wifi-sta-sec-00:

Wi-Fi Station 接口
=====================

:link_to_translation:`en:[English]`

公共头文件： ``#include "brookesia/hal_interface/interfaces/wifi/station.hpp"``

类名： ``StationIface``

``StationIface`` 定义 Station 模式动作、AP 元数据持久化钩子、
扫描配置、扫描控制，以及扫描和 AP 更新回调。重试、fallback
和自动连接策略不属于该接口职责。

.. _hal-interface-wifi-sta-sec-01:

API 参考
--------

.. include-build-file:: inc/station.inc
