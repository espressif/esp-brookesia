.. _service-audio-sec-00:

音频
====

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_service_audio <https://components.espressif.com/components/espressif/brookesia_service_audio>`_
- 辅助头文件： ``#include "brookesia/service_helper/audio.hpp"``
- 辅助类： ``esp_brookesia::service::helper::Audio``

.. _service-audio-sec-01:

概述
----

`brookesia_service_audio` 是为 ESP-Brookesia 生态系统提供的音频服务，提供：

- **音频播放**：支持从 URL 播放音频文件，支持暂停、恢复、停止等播放控制。
- **音频编解码**：支持多种音频编解码格式（PCM、OPUS、G711A），支持编码和解码功能。
- **音量控制**：支持音量设置和查询，提供完整的音量管理能力。
- **播放状态管理**：实时跟踪播放状态（空闲、播放中、暂停），并通过事件通知状态变化。
- **编码器管理**：支持编码器的启动、停止和配置，可设置编码器读取数据大小。
- **解码器管理**：支持解码器的启动、停止和数据输入，支持流式解码。
- **持久化存储**：可选搭配 `brookesia_service_nvs` 服务持久化保存音量等信息。

.. _service-audio-sec-02:

功能特性
--------

.. _service-audio-sec-03:

音频编解码格式
^^^^^^^^^^^^^^

Audio Service 支持以下音频编解码格式：

.. list-table::
   :widths: 18 10 10 62
   :header-rows: 1

   * - 格式
     - 编码
     - 解码
     - 说明
   * - **PCM**
     - ✅
     - ✅
     - 无损音频格式
   * - **OPUS**
     - ✅
     - ✅
     - 支持 VBR 和固定比特率配置
   * - **G711A**
     - ✅
     - ✅
     - 电话音质音频格式

.. _service-audio-sec-04:

播放控制
^^^^^^^^

- 支持从 URL 播放音频文件。
- 支持暂停、恢复、停止等基础播放控制。
- 提供播放状态事件通知，便于业务层同步 UI 与状态。

.. _service-audio-sec-05:

编码器配置
^^^^^^^^^^

- **编解码格式**：PCM、OPUS、G711A。
- **通道数**：1-4 通道。
- **采样位数**：8、16、24、32 位。
- **采样率**：8000、16000、24000、32000、44100、48000 Hz。
- **帧时长**：可配置的帧时长（毫秒）。
- **OPUS 扩展配置**：VBR 开关、比特率设置。

.. _service-audio-sec-06:

解码器配置
^^^^^^^^^^

- **编解码格式**：PCM、OPUS、G711A。
- **通道数**：1-4 通道。
- **采样位数**：8、16、24、32 位。
- **采样率**：8000、16000、24000、32000、44100、48000 Hz。
- **帧时长**：可配置的帧时长（毫秒）。

.. _service-audio-sec-07:

事件通知
^^^^^^^^

- **播放状态变化**：播放状态改变时触发（Idle、Playing、Paused）。
- **编码器事件**：编码器事件发生时触发。
- **编码数据就绪**：编码数据准备好时触发。

.. include-build-file:: contract_guides/service/audio.inc
