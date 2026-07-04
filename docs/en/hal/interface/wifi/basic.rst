.. _hal-interface-wifi-basic-sec-00:

Wi-Fi Basic Interface
=====================

:link_to_translation:`zh_CN:[中文]`

Public header: ``#include "brookesia/hal_interface/interfaces/wifi/basic.hpp"``

Class: ``WifiBasicIface``

``WifiBasicIface`` exposes the shared Wi-Fi control plane: initialization,
deinitialization, start/stop actions, reset, and general action/event callbacks.
Connection policy remains above HAL in the Wi-Fi service.

.. _hal-interface-wifi-basic-sec-01:

API Reference
-------------

.. include-build-file:: inc/basic.inc
