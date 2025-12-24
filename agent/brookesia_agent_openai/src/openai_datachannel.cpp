/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <tuple>
#include "cJSON.h"
#include "esp_log.h"
#include "private/utils.hpp"
#include "openai_datachannel.hpp"

namespace esp_brookesia::agent {

// Define function pointer type for message handlers
using MessageHandlerFunc = std::function<bool(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)>;

// Define a structure to map message types to handler functions
struct MessageHandlerMapping {
    OpenaiDataChannelMessageType type;
    const char *type_str;
    MessageHandlerFunc handler;
};

static OpenaiDataChannelSignal openai_datachannel_signal;

// Handle response.audio.delta: Processes incremental audio message
bool handle_response_audio_delta(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    // const cJSON *delta = cJSON_GetObjectItem(json, "delta");
    // if (delta && cJSON_IsString(delta)) {

    //     static int printf_once = 0;
    //     if (printf_once < 1) {
    //         printf_once++;
    //         BROOKESIA_LOGI("response.audio:\r\n%.100s", message);
    //     } else {
    //     }
    // }

    return true;
}

// Handle response.audio.done: Marks the end of audio transmission
bool handle_response_audio_done(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    // TODO: Add logic to handle completion of audio response
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle response.text.delta: Processes incremental text message
bool handle_response_text_delta(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    // TODO: Add logic to process text delta messages
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle response.text.done: Marks the end of text response
bool handle_response_text_done(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    // TODO: Add logic to handle completion of text response
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle response.audio_transcript.delta: Processes incremental audio transcript message
bool handle_response_audio_transcript_delta(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    // BROOKESIA_LOG_TRACE_GUARD();

    cJSON *delta_item = cJSON_GetObjectItem(json, "delta");
    if (delta_item != NULL && cJSON_IsString(delta_item)) {
        // BROOKESIA_LOGI("delta: %s", delta_item->valuestring);
        openai_datachannel_signal(message_type, delta_item->valuestring);
    } else {
        BROOKESIA_LOGI("delta field not found or is not a string");
        return false;
    }

    return true;
}

// Handle response.audio_transcript.done: Marks the end of audio transcript
bool handle_response_audio_transcript_done(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    // TODO: Add logic to handle completion of audio transcript
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle response.content_part.added: Handles new content part addition
bool handle_response_content_part_added(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    // TODO: Add logic to process added content parts
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle response.content_part.done: Marks the completion of a content part
bool handle_response_content_part_done(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    // TODO: Add logic to handle completion of content parts
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle response.function_call_arguments.delta: Processes incremental function call arguments
bool handle_response_function_call_arguments_delta(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    // TODO: Add logic to process delta of function call arguments
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle response.function_call_arguments.done: Marks the end of function call arguments
bool handle_response_function_call_arguments_done(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle response.done: Marks the completion of a response
bool handle_response_done(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    cJSON *response = cJSON_GetObjectItemCaseSensitive(json, "response");
    if (response != NULL) {
        cJSON *output = cJSON_GetObjectItemCaseSensitive(response, "output");
        if (cJSON_IsArray(output)) {
            cJSON *item = NULL;
            cJSON_ArrayForEach(item, output) {
                cJSON *item_type = cJSON_GetObjectItemCaseSensitive(item, "type");
                cJSON *status = cJSON_GetObjectItemCaseSensitive(item, "status");
                if (cJSON_IsString(item_type) && cJSON_IsString(status) &&
                        strcmp(item_type->valuestring, "message") == 0 &&
                        strcmp(status->valuestring, "completed") == 0) {

                    BROOKESIA_LOGI("status: %s", status->valuestring);
                    cJSON *role = cJSON_GetObjectItemCaseSensitive(item, "role");
                    if (cJSON_IsString(role)) {
                        BROOKESIA_LOGI("Role: %s", role->valuestring);
                    }

                    cJSON *content = cJSON_GetObjectItemCaseSensitive(item, "content");
                    if (cJSON_IsArray(content)) {
                        cJSON *content_item = NULL;
                        cJSON_ArrayForEach(content_item, content) {
                            cJSON *content_type = cJSON_GetObjectItemCaseSensitive(content_item, "type");
                            cJSON *text = cJSON_GetObjectItemCaseSensitive(content_item, "text");

                            if (cJSON_IsString(content_type) && strcmp(content_type->valuestring, "text") == 0 &&
                                    cJSON_IsString(text)) {
                                BROOKESIA_LOGI("Text:\r\n%s", text->valuestring);
                                openai_datachannel_signal(message_type, text->valuestring);
                            }
                        }
                    }
                }
            }
        }
    }

    return true;
}

// Handle input_audio_buffer.speech_started: Detects when speech starts
bool handle_input_audio_buffer_speech_started(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    openai_datachannel_signal(message_type, "");

    return true;
}

// Handle input_audio_buffer.speech_stopped: Detects when speech stops
bool handle_input_audio_buffer_speech_stopped(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle input_audio_buffer.committed: Handles committed input audio buffer
bool handle_input_audio_buffer_committed(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle input_audio_buffer.cleared: Handles cleared input audio buffer
bool handle_input_audio_buffer_cleared(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle response.output_item.added: Processes newly added output items
bool handle_response_output_item_added(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle output_audio_buffer.started: Handles the start of output audio buffer
bool handle_output_audio_buffer_started(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle output_audio_buffer.stopped: Handles the stop of output audio buffer
bool handle_output_audio_buffer_stopped(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    openai_datachannel_signal(message_type, "");

    return true;
}

// Handle output_audio_buffer.cleared: Handles the clearing of output audio buffer
bool handle_output_audio_buffer_cleared(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle response.output_item.done: Marks the completion of an output item
bool handle_response_output_item_done(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    const cJSON *item = cJSON_GetObjectItem(json, "item");
    if (item) {
        const cJSON *role = cJSON_GetObjectItem(item, "role");
        const cJSON *status = cJSON_GetObjectItem(item, "status");
        const cJSON *content = cJSON_GetObjectItem(item, "content");

        if (role && cJSON_IsString(role)) {
            BROOKESIA_LOGI("  Role: %s", role->valuestring);
        }
        if (status && cJSON_IsString(status)) {
            BROOKESIA_LOGI("  Status: %s", status->valuestring);
        }
        if (content && cJSON_IsArray(content)) {
            cJSON *c = NULL;
            cJSON_ArrayForEach(c, content) {
                const cJSON *content_type = cJSON_GetObjectItem(c, "type");
                const cJSON *transcript = cJSON_GetObjectItem(c, "transcript");
                if (content_type && cJSON_IsString(content_type)) {
                    BROOKESIA_LOGI("    Type: %s", content_type->valuestring);
                    if (strcmp(content_type->valuestring, "audio") == 0 && transcript && cJSON_IsString(transcript)) {
                        BROOKESIA_LOGI("    Transcript: %s", transcript->valuestring);
                        openai_datachannel_signal(message_type, transcript->valuestring);
                        // ui_enter_label(transcript->valuestring);
                    } else {
                        BROOKESIA_LOGI("    unknown");
                    }
                }
            }
        }
    }

    return true;
}

// Handle session.created: Handles creation of a new session
bool handle_session_created(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle session.updated: Handles session updates
bool handle_session_updated(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle conversation.item.created: Handles creation of a new conversation item
bool handle_conversation_item_created(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle conversation.item.truncated: Handles truncation of a conversation item
bool handle_conversation_item_truncated(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    openai_datachannel_signal(message_type, "");

    return true;
}

// Handle response.created: Handles creation of a new response
bool handle_response_created(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle rate_limits.updated: Updates rate limits information
bool handle_rate_limits_updated(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle unknown message types: Default fallback handler
bool handle_unknown_message(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    return true;
}

// Handle response.error: Handles error messages from the response
bool handle_response_error(const cJSON *json, const char *message, OpenaiDataChannelMessageType message_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    openai_datachannel_signal(message_type, "");

    return true;
}

static MessageHandlerMapping message_handlers[] = {
    // Response-related handlers
    {OpenaiDataChannelMessageType::ResponseAudioDelta, "response.audio.delta", handle_response_audio_delta},
    {OpenaiDataChannelMessageType::ResponseAudioDone, "response.audio.done", handle_response_audio_done},
    {OpenaiDataChannelMessageType::ResponseTextDelta, "response.text.delta", handle_response_text_delta},
    {OpenaiDataChannelMessageType::ResponseTextDone, "response.text.done", handle_response_text_done},
    {OpenaiDataChannelMessageType::ResponseAudioTranscriptDelta, "response.audio_transcript.delta", handle_response_audio_transcript_delta},
    {OpenaiDataChannelMessageType::ResponseAudioTranscriptDone, "response.audio_transcript.done", handle_response_audio_transcript_done},
    {OpenaiDataChannelMessageType::ResponseContentPartAdded, "response.content_part.added", handle_response_content_part_added},
    {OpenaiDataChannelMessageType::ResponseContentPartDone, "response.content_part.done", handle_response_content_part_done},
    {OpenaiDataChannelMessageType::ResponseFunctionCallArgumentsDelta, "response.function_call.delta", handle_response_function_call_arguments_delta},
    {OpenaiDataChannelMessageType::ResponseFunctionCallArgumentsDone, "response.function_call.done", handle_response_function_call_arguments_done},
    {OpenaiDataChannelMessageType::ResponseDone, "response.done", handle_response_done},

    // Input audio buffer handlers
    {OpenaiDataChannelMessageType::InputAudioBufferSpeechStarted, "input_audio_buffer.speech_started", handle_input_audio_buffer_speech_started},
    {OpenaiDataChannelMessageType::InputAudioBufferSpeechStopped, "input_audio_buffer.speech_stopped", handle_input_audio_buffer_speech_stopped},
    {OpenaiDataChannelMessageType::InputAudioBufferCommitted, "input_audio_buffer.committed", handle_input_audio_buffer_committed},
    {OpenaiDataChannelMessageType::InputAudioBufferCleared, "input_audio_buffer.cleared", handle_input_audio_buffer_cleared},

    // Response output item handlers
    {OpenaiDataChannelMessageType::ResponseOutputItemAdded, "response.output_item.added", handle_response_output_item_added},
    {OpenaiDataChannelMessageType::ResponseOutputItemDone, "response.output_item.done", handle_response_output_item_done},

    {OpenaiDataChannelMessageType::OutputAudioBufferStarted, "output_audio_buffer.started", handle_output_audio_buffer_started},
    {OpenaiDataChannelMessageType::OutputAudioBufferStopped, "output_audio_buffer.stopped", handle_output_audio_buffer_stopped},
    {OpenaiDataChannelMessageType::OutputAudioBufferCleared, "output_audio_buffer.cleared", handle_output_audio_buffer_cleared},

    // Session-related handlers
    {OpenaiDataChannelMessageType::SessionCreated, "session.created", handle_session_created},
    {OpenaiDataChannelMessageType::SessionUpdated, "session.updated", handle_session_updated},

    // Conversation and rate limit handlers
    {OpenaiDataChannelMessageType::ConversationItemCreated, "conversation.item.created", handle_conversation_item_created},
    {OpenaiDataChannelMessageType::ConversationItemTruncated, "conversation.item.truncated", handle_conversation_item_truncated},
    {OpenaiDataChannelMessageType::ResponseCreated, "response.created", handle_response_created},
    {OpenaiDataChannelMessageType::RateLimitsUpdated, "rate_limits.updated", handle_rate_limits_updated},

    // Default and unknown handlers
    {OpenaiDataChannelMessageType::Error, "error", handle_response_error},
    // Default handler for unmatched types
    {OpenaiDataChannelMessageType::Unknown, NULL, handle_unknown_message}
};

bool openai_datachannel_handle_message(cJSON *json, const char *message)
{
    // BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_CHECK_NULL_RETURN(json, false, "Invalid json");
    BROOKESIA_CHECK_NULL_RETURN(message, false, "Invalid message");

    const cJSON *type = cJSON_GetObjectItem(json, "type");
    BROOKESIA_CHECK_FALSE_RETURN(type && cJSON_IsString(type), false, "Invalid type");

    for (MessageHandlerMapping *handler = message_handlers; handler->type != OpenaiDataChannelMessageType::Unknown; handler++) {
        if (strcmp(type->valuestring, handler->type_str) == 0) {
            BROOKESIA_CHECK_FALSE_RETURN(
                handler->handler(json, message, handler->type), false,
                "Handler failed"
            );
            return true;
        }
    }

    BROOKESIA_CHECK_FALSE_RETURN(
        handle_unknown_message(json, message, OpenaiDataChannelMessageType::Unknown),
        false, "Unknown message"
    );

    return true;
}

boost::signals2::connection openai_datachannel_connect_signal(const OpenaiDataChannelSignal::slot_type &slot)
{
    return openai_datachannel_signal.connect(slot);
}

} // namespace esp_brookesia::agent
