.. _hal-interface-power-battery-sec-00:

Battery Interface
=================

:link_to_translation:`zh_CN:[中文]`

Public header: ``#include "brookesia/hal_interface/interfaces/power/battery.hpp"``

Class: ``PowerBatteryIface``

``PowerBatteryIface`` describes battery and charger capabilities. It can query battery presence, level percentage, voltage, power source, and charge state, and can also read or update charger configuration and enable or disable charging when supported by the underlying hardware.

Not every board exposes the full set of battery and charger controls. Callers should first inspect ``Info::abilities`` or ``Info::has_ability()`` before reading capability-specific state fields or invoking control APIs.

.. _hal-interface-power-battery-sec-01:

API Reference
-------------

.. include-build-file:: inc/battery.inc
