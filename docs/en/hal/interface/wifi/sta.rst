.. _hal-interface-wifi-sta-sec-00:

Wi-Fi Station Interface
=======================

:link_to_translation:`zh_CN:[中文]`

Public header: ``#include "brookesia/hal_interface/interfaces/wifi/station.hpp"``

Class: ``StationIface``

``StationIface`` defines station-mode actions, AP metadata persistence hooks, scan
configuration, scan control, and scan/AP update callbacks. It does not own retry,
fallback, or auto-connect policy.

.. _hal-interface-wifi-sta-sec-01:

API Reference
-------------

.. include-build-file:: inc/station.inc
