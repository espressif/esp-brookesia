.. _service-nes-sec-00:

NES Service
===========

:link_to_translation:`zh_CN:[中文]`

- Helper header: ``#include "brookesia/service_helper/emulation/nes.hpp"``
- Helper class: ``esp_brookesia::service::helper::Nes``

.. rubric:: Overview

``brookesia_emulation_nes`` exposes a service control plane for the NES emulator runtime.

.. rubric:: Core Responsibilities

- Controls ROM loading, start, pause, stop, save, and gamepad state.
- Uses Display service sources for frame output instead of sending frames through RPC.
- Keeps nofrendo backend code inside the service component.

.. rubric:: Integration Position

This component is an independent ESP-Brookesia release component. It can be integrated through ESP-IDF component dependencies and combined with peer framework components as needed.

.. include-build-file:: contract_guides/service/nes.inc
