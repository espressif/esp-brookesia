/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <any>
#include <string>
#include "boost/signals2.hpp"
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
using OpenaiDataChannelSignal = boost::signals2::signal<void(OpenaiDataChannelMessageType, OpenaiDataChannelMessage)>;

bool openai_datachannel_handle_message(cJSON *json, const char *message);

boost::signals2::connection openai_datachannel_connect_signal(const OpenaiDataChannelSignal::slot_type &slot);

} // namespace esp_brookesia::agent
