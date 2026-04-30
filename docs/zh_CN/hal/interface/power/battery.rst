.. _hal-interface-power-battery-sec-00:

电池接口
========

:link_to_translation:`en:[English]`

公共头文件： ``#include "brookesia/hal_interface/interfaces/power/battery.hpp"``

类名： ``PowerBatteryIface``

``PowerBatteryIface`` 用于描述电池与充电器相关能力。它既可以查询电池是否存在、电量百分比、电压、电源来源和充电状态，也可以在底层硬件支持时读取或设置充电配置、启停充电。

并非所有开发板都具备完整的电池与充电控制能力。调用方应先通过 ``Info::abilities`` 或 ``Info::has_ability()`` 判断当前实现支持的能力，再访问对应状态字段或控制接口。

.. _hal-interface-power-battery-sec-01:

API 参考
--------

.. include-build-file:: inc/battery.inc
