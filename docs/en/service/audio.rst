.. _service-audio-sec-00:

Audio
=====

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_service_audio <https://components.espressif.com/components/espressif/brookesia_service_audio>`_
- Helper header: ``#include "brookesia/service_helper/audio.hpp"``
- Helper class: ``esp_brookesia::service::helper::Audio``

.. _service-audio-sec-01:

Overview
--------

`brookesia_service_audio` provides:

- **Playback**: Stream from URL; pause, resume, stop.
- **Codecs**: PCM, OPUS, G711A encode/decode.
- **Playback state**: Idle / playing / paused with events.
- **Encoder**: Start/stop/configure; configurable read size.
- **Decoder**: Start/stop and feed compressed data; streaming decode.

.. _service-audio-sec-02:

Features
--------

.. _service-audio-sec-03:

Codec Formats
^^^^^^^^^^^^^

The audio service supports the following codec formats:

.. list-table::
   :widths: 18 10 10 62
   :header-rows: 1

   * - Format
     - Encode
     - Decode
     - Notes
   * - **PCM**
     - Yes
     - Yes
     - Lossless
   * - **OPUS**
     - Yes
     - Yes
     - VBR and fixed bitrate
   * - **G711A**
     - Yes
     - Yes
     - Telephony quality

.. _service-audio-sec-04:

Playback
^^^^^^^^

- URL playback.
- Pause, resume, stop.
- State events for UI sync.

.. _service-audio-sec-05:

Encoder Configuration
^^^^^^^^^^^^^^^^^^^^^

- Codecs: PCM, OPUS, G711A.
- Channels: 1–4.
- Bits per sample: 8, 16, 24, 32.
- Sample rates: 8000, 16000, 24000, 32000, 44100, 48000 Hz.
- Frame duration (ms).
- OPUS: VBR and bitrate.

.. _service-audio-sec-06:

Decoder Configuration
^^^^^^^^^^^^^^^^^^^^^

- **Codecs**: PCM, OPUS, G711A.
- **Channels**: 1–4.
- **Bits per sample**: 8, 16, 24, 32.
- **Sample rates**: 8000, 16000, 24000, 32000, 44100, 48000 Hz.
- **Frame duration (ms)**.

.. _service-audio-sec-07:

Events
^^^^^^

- Playback state changes (Idle, Playing, Paused).
- Encoder events.
- Encoded data ready.

.. include-build-file:: contract_guides/service/audio.inc
