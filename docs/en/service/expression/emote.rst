.. _expression-emote-sec-00:

Emote
=====

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_expression_emote <https://components.espressif.com/components/espressif/brookesia_expression_emote>`_
- Helper header: ``#include "brookesia/service_helper/expression/emote.hpp"``
- Helper class: ``esp_brookesia::service::helper::ExpressionEmote``

.. _expression-emote-sec-01:

Overview
--------

`brookesia_expression_emote` manages emotes on the Brookesia service framework:

- **Resources**: Load and configure emoji/animation assets.
- **Emotes**: Set and manage emoji for emotional states.
- **Animation**: Playback control; wait for frames; stop.
- **QR codes**: Show and hide QR overlays.
- **Event messages**: Show and hide event text.

.. include-build-file:: contract_guides/service/emote.inc
