.. _agent-coze-sec-00:

Coze
====

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_agent_coze <https://components.espressif.com/components/espressif/brookesia_agent_coze>`_
- Helper header: ``#include "brookesia/agent_helper/coze.hpp"``
- Helper class: ``esp_brookesia::agent::helper::Coze``

.. _agent-coze-sec-01:

Overview
--------

`brookesia_agent_coze` implements the Coze agent on top of ESP-Brookesia Agent Manager.

.. _agent-coze-sec-02:

Features
--------

- **Coze API**: WebSocket to Coze for real-time voice and text.
- **OAuth2**: Token acquisition and refresh for Coze.
- **Multi-bot**: Configure multiple bots and switch the active one.
- **Audio**: Built-in codecs; default G711A at 16 kHz.
- **Emotes**: Receive and display Coze expression events.
- **Platform events**: e.g. balance warnings.
- **Persistence**: Optional `brookesia_service_nvs` for auth and bot metadata.

.. include-build-file:: contract_guides/agent/coze.inc
