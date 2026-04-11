.. _agent-openai-sec-00:

OpenAI
======

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_agent_openai <https://components.espressif.com/components/espressif/brookesia_agent_openai>`_
- Helper header: ``#include "brookesia/agent_helper/openai.hpp"``
- Helper class: ``esp_brookesia::agent::helper::Openai``

.. _agent-openai-sec-01:

Overview
--------

`brookesia_agent_openai` implements the OpenAI agent on ESP-Brookesia Agent Manager.

.. _agent-openai-sec-02:

Features
--------

- **OpenAI Realtime**: WebRTC data channels for real-time voice and text.
- **P2P**: `esp_peer` for signaling and data channels.
- **Audio**: OPUS by default at 16 kHz, 24 kbps.
- **Data channel**: Audio, text, function calls, session control, etc.
- **Low latency**: Real-time duplex audio.
- **Persistence**: Optional `brookesia_service_nvs` for configuration.

.. include-build-file:: contract_guides/agent/openai.inc
