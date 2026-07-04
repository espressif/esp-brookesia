/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <any>
#include <string>
#include "brookesia/lib_utils/signal.hpp"
#include "cJSON.h"

namespace esp_brookesia::agent {

enum class OpenaiDataChannelMessageType {
    ResponseAudioDelta,
    ResponseAudioDone,
    ResponseTextDelta,
    ResponseTextDone,
    ResponseAudioTranscriptDelta,
    ResponseAudioTranscriptDone,
    ResponseContentPartAdded,
    ResponseContentPartDone,
    ResponseFunctionCallArgumentsDelta,
    ResponseFunctionCallArgumentsDone,
    ResponseDone,
    InputAudioBufferSpeechStarted,
    InputAudioBufferSpeechStopped,
    InputAudioBufferCommitted,
    InputAudioBufferCleared,
    ResponseOutputItemAdded,
    ResponseOutputItemDone,
    OutputAudioBufferStarted,
    OutputAudioBufferStopped,
    OutputAudioBufferCleared,
    SessionCreated,
    SessionUpdated,
    ConversationItemCreated,
    ConversationItemTruncated,
    ResponseCreated,
    RateLimitsUpdated,
    Unknown,
    Error,
};

using OpenaiDataChannelMessage = std::string;
using OpenaiDataChannelSignal = esp_brookesia::lib_utils::signal<void(OpenaiDataChannelMessageType, OpenaiDataChannelMessage)>;

bool openai_datachannel_handle_message(cJSON *json, const char *message);

esp_brookesia::lib_utils::connection openai_datachannel_connect_signal(const OpenaiDataChannelSignal::slot_type &slot);

} // namespace esp_brookesia::agent
