.. _agent-xiaozhi-sec-00:

XiaoZhi
=======

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_agent_xiaozhi <https://components.espressif.com/components/espressif/brookesia_agent_xiaozhi>`_
- Helper header: ``#include "brookesia/agent_helper/xiaozhi.hpp"``
- Helper class: ``esp_brookesia::agent::helper::XiaoZhi``

.. _agent-xiaozhi-sec-01:

Overview
--------

`brookesia_agent_xiaozhi` integrates the XiaoZhi AI platform for ESP-Brookesia.

.. _agent-xiaozhi-sec-02:

Features
--------

- **Platform**: Built on `esp_xiaozhi` SDK.
- **Voice**: OPUS at 16 kHz, 24 kbps.
- **Events**: Speaking/listening, agent/user text, emotes, etc.
- **Manual listen**: Start/stop listening for Manual mode.
- **Barge-in**: Interrupt agent speech.
- **Lifecycle**: Unified management via `brookesia_agent_manager`.

.. include-build-file:: contract_guides/agent/xiaozhi.inc
