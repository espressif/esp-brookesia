/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <random>
#include "esp_wifi.h"
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_SPEAKER_AI_BUDDY_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_speaker_utils.hpp"
#include "esp_brookesia_speaker_ai_buddy.hpp"

#define AUDIO_EVENT_THREAD_NAME                 "audio_event"
#define AUDIO_EVENT_THREAD_STACK_SIZE           (10 * 1024)
#define AUDIO_EVENT_THREAD_STACK_CAPS_EXT       (true)

#define AUDIO_PLAY_TIMEOUT_MS                   (10 * 1000)
#define AUDIO_PROCESS_LOOP_TIMEOUT_MS           (100)
#define AUDIO_PLAY_LOOP_COUNT                   (3)

#define AUDIO_WIFI_NEED_CONNECT_REPEAT_INTERVAL_MS      (20 * 1000)
#define AUDIO_WIFI_NEED_CONNECT_DELAY_MS                (10 * 1000)
#define AUDIO_SERVER_CONNECTING_REPEAT_INTERVAL_MS      (20 * 1000)
#define AUDIO_SERVER_DISCONNECTED_REPEAT_INTERVAL_MS    (20 * 1000)
#define AUDIO_INVALID_CONFIG_REPEAT_INTERVAL_MS         (20 * 1000)
#define AUDIO_COZE_ERROR_INSUFFICIENT_CREDITS_BALANCE_REPEAT_INTERVAL_MS (20 * 1000)

using namespace esp_brookesia::ai_framework;

namespace esp_brookesia::systems::speaker {

AI_Buddy::~AI_Buddy()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_EXIT(!_flags.is_begun || del(), "Del failed");
}

bool AI_Buddy::begin(const Data &data)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(_mutex);

    if (_flags.is_begun) {
        ESP_UTILS_LOGD("Already begun");
        return true;
    }

    esp_utils::function_guard del_function([this]() {
        ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();
        if (!del()) {
            ESP_UTILS_LOGE("Del failed");
        }
    });

    ESP_UTILS_CHECK_NULL_RETURN(_agent = ai_framework::Agent::requestInstance(), false, "Invalid agent");
    ESP_UTILS_CHECK_FALSE_RETURN(_agent->begin(), false, "Agent begin failed");
    ESP_UTILS_CHECK_FALSE_RETURN(
        expression.begin(data.expression.data, &_emoji_map, &_system_icon_map), false, "Expression begin failed"
    );

    esp_err_t ret = esp_event_loop_create_default();
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_UTILS_LOGW("Default event loop already created");
    } else {
        ESP_UTILS_CHECK_FALSE_RETURN(ret == ESP_OK, false, "Create default event loop failed(%s)", esp_err_to_name(ret));
    }
    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, [](void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_CHECK_FALSE_EXIT(event_base == WIFI_EVENT, "Invalid event base");

        auto ai_buddy = static_cast<AI_Buddy *>(arg);
        ESP_UTILS_CHECK_NULL_EXIT(ai_buddy, "Invalid arg");

        ESP_UTILS_CHECK_FALSE_EXIT(
            ai_buddy->processOnWiFiEvent(event_id, event_data), "Process WiFi event failed"
        );
    }, this, &_wifi_event_handler);
    ESP_UTILS_CHECK_FALSE_RETURN(ret == ESP_OK, false, "Register WiFi event handler failed(%s)", esp_err_to_name(ret));
    ret = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, [](void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_CHECK_FALSE_EXIT(event_base == IP_EVENT, "Invalid event base");

        auto ai_buddy = static_cast<AI_Buddy *>(arg);
        ESP_UTILS_CHECK_NULL_EXIT(ai_buddy, "Invalid arg");

        ESP_UTILS_CHECK_FALSE_EXIT(
            ai_buddy->processOnIpEvent(event_id, event_data), "Process IP event failed"
        );
    }, this, &_ip_event_handler);
    ESP_UTILS_CHECK_FALSE_RETURN(ret == ESP_OK, false, "Register IP event handler failed(%s)", esp_err_to_name(ret));

    {
        esp_utils::thread_config_guard thread_config(esp_utils::ThreadConfig{
            .name = AUDIO_EVENT_THREAD_NAME,
            .stack_size = AUDIO_EVENT_THREAD_STACK_SIZE,
            .stack_in_ext = AUDIO_EVENT_THREAD_STACK_CAPS_EXT,
        });
        _audio_event_thread = boost::thread([this]() {
            ESP_UTILS_LOG_TRACE_GUARD();

            while (true) {
                std::unique_lock<std::recursive_mutex> lock(_audio_event_mutex);
                _audio_event_cv.wait_for(lock, boost::chrono::milliseconds(AUDIO_PROCESS_LOOP_TIMEOUT_MS));

                if (_audio_current_process_infos.empty() && _audio_next_process_infos.empty()) {
                    continue;
                }

                // Remove stopped audio from current process infos
                if (!_audio_removed_process_infos.empty()) {
                    for (auto type : _audio_removed_process_infos) {
                        ESP_UTILS_LOGD("Remove audio: %s", getAudioName(type).c_str());
                        _audio_current_process_infos.remove_if([type](const AudioProcessInfo & info) {
                            return info.event.type == type;
                        });
                    }
                    _audio_removed_process_infos.clear();
                }

                // Merge next process infos to current process infos
                if (!_audio_next_process_infos.empty()) {
                    _audio_current_process_infos.splice(_audio_current_process_infos.end(), _audio_next_process_infos);
                }

                // Process audio events
                lock.unlock();
                for (auto &info : _audio_current_process_infos) {
                    if (!processAudioEvent(info)) {
                        ESP_UTILS_LOGE("Process audio event failed");
                    }
                    if (!_audio_removed_process_infos.empty()) {
                        break;
                    }
                }

                // Remove finished audio from current process infos
                for (auto it = _audio_current_process_infos.begin(); it != _audio_current_process_infos.end();) {
                    auto &info = *it;
                    if (info.event.repeat_count == 0) {
                        ESP_UTILS_LOGI("Stop audio: %s", getAudioName(info.event.type).c_str());
                        it = _audio_current_process_infos.erase(it);
                    } else {
                        it++;
                    }
                }
            }
        });
    }

    _agent_connections.push_back(_agent->chat_event_process_start_signal.connect([this](const Agent::ChatEvent & current_event, const Agent::ChatEvent & last_event) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD(
            "Param: current_event(%s), last_event(%s)", Agent::chatEventToString(current_event).c_str(),
            Agent::chatEventToString(last_event).c_str()
        );

        switch (current_event) {
        case Agent::ChatEvent::Init:
            ESP_UTILS_CHECK_FALSE_EXIT(expression.setEmoji("neutral"), "Set emoji failed");
            break;
        case Agent::ChatEvent::Start:
            stopAudio(AudioType::ServerDisconnected);
            sendAudioEvent({AudioType::ServerConnecting, AUDIO_PLAY_LOOP_COUNT, AUDIO_SERVER_CONNECTING_REPEAT_INTERVAL_MS});
            ESP_UTILS_CHECK_FALSE_EXIT(
                expression.setSystemIcon("server_connecting", {.immediate = true}), "Set server connecting icon failed"
            );
            break;
        default:
            break;
        }
    }));
    _agent_connections.push_back(_agent->chat_event_process_special_signal.connect([this](const Agent::ChatEventSpecialSignalType & type) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD("Param: type(%d)", static_cast<int>(type));

        switch (type) {
        case Agent::ChatEventSpecialSignalType::InitInvalidConfig:
            sendAudioEvent({AudioType::InvalidConfig, AUDIO_PLAY_LOOP_COUNT, AUDIO_INVALID_CONFIG_REPEAT_INTERVAL_MS});
            ESP_UTILS_CHECK_FALSE_EXIT(
                expression.setSystemIcon("invalid_config", {.immediate = true}), "Set invalid config icon failed"
            );
            break;
        case Agent::ChatEventSpecialSignalType::StartMaxRetry:
            stopAudio(AudioType::ServerConnecting);
            sendAudioEvent({AudioType::ServerDisconnected, AUDIO_PLAY_LOOP_COUNT, AUDIO_SERVER_DISCONNECTED_REPEAT_INTERVAL_MS});
            break;
        default:
            break;
        }
    }));
    _agent_connections.push_back(_agent->chat_event_process_end_signal.connect([this](const Agent::ChatEvent & current_event, const Agent::ChatEvent & last_event) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD(
            "Param: current_event(%s), last_event(%s)", Agent::chatEventToString(current_event).c_str(),
            Agent::chatEventToString(last_event).c_str()
        );

        switch (current_event) {
        case Agent::ChatEvent::Init:
            if (!isWiFiValid()) {
                ESP_UTILS_CHECK_FALSE_EXIT(expression.setSystemIcon("wifi_disconnected"), "Set WiFi icon failed");
                playWiFiNeedConnectAudio();
            } else {
                if (!_agent->hasChatState(Agent::_ChatStateStart)) {
                    ESP_UTILS_CHECK_FALSE_EXIT(
                        _agent->sendChatEvent(Agent::ChatEvent::Start), "Send chat event start failed"
                    );
                }
            }
            break;
        case Agent::ChatEvent::Stop:
            sendAudioEvent({AudioType::ServerDisconnected, AUDIO_PLAY_LOOP_COUNT, AUDIO_SERVER_DISCONNECTED_REPEAT_INTERVAL_MS});
            break;
        case Agent::ChatEvent::Start:
            stopAudio(AudioType::ServerConnecting);
            sendAudioEvent({AudioType::ServerConnected});
            ESP_UTILS_CHECK_FALSE_EXIT(
                expression.setSystemIcon("server_connected", {.immediate = true}), "Set server connected icon failed"
            );
            if (!isPause()) {
                _agent->resume();
                ESP_UTILS_CHECK_FALSE_EXIT(
                    _agent->sendChatEvent(Agent::ChatEvent::Sleep), "Send chat event sleep failed"
                );
            } else {
                stopAudio(AudioType::MicOn);
                sendAudioEvent({AudioType::MicOff});
            }
            break;
        case Agent::ChatEvent::Sleep:
            ESP_UTILS_CHECK_FALSE_EXIT(
                expression.setEmoji("sleepy", {.repeat = false, .keep_when_stop = true}, {.repeat = true}),
                "Set emoji failed"
            );
            sendAudioEvent({AudioType::WakeUp});
            break;
        case Agent::ChatEvent::WakeUp:
            ESP_UTILS_CHECK_FALSE_EXIT(expression.setEmoji("neutral"), "Set emoji failed");
            break;
        default:
            break;
        }
    }));
    _agent_connections.push_back(coze_chat_response_signal.connect([this]() {
        ESP_UTILS_LOG_TRACE_GUARD();

        if (!isWiFiValid()) {
            return;
        }

        ESP_UTILS_CHECK_FALSE_EXIT(playRandomAudio(_response_audios), "Play random audio failed");
    }));
    _agent_connections.push_back(coze_chat_wake_up_signal.connect([this](bool is_wake_up) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD("Param: is_wake_up(%d)", is_wake_up);

        if (!isWiFiValid()) {
            return;
        }

        boost::thread([this, is_wake_up]() {
            if (is_wake_up) {
                ESP_UTILS_CHECK_FALSE_EXIT(expression.setEmoji("neutral"), "Set emoji failed");
                if (_agent->hasChatState(Agent::_ChatStateSleep)) {
                    ESP_UTILS_CHECK_FALSE_EXIT(
                        _agent->sendChatEvent(Agent::ChatEvent::WakeUp), "Send chat event wake up failed"
                    );
                }
            } else {
                if (!_agent->hasChatState(Agent::_ChatStateSleep)) {
                    if (!playRandomAudio(_sleep_audios)) {
                        ESP_UTILS_LOGW("Play sleep audio failed");
                    }
                    ESP_UTILS_CHECK_FALSE_EXIT(
                        _agent->sendChatEvent(Agent::ChatEvent::Sleep), "Send chat event sleep failed"
                    );
                }
            }
        }).detach();
    }));
    _agent_connections.push_back(coze_chat_emoji_signal.connect([this](const std::string & emoji) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGI("Emoji: %s", emoji.c_str());
        bool immediate = false;
        if (emoji != "neutral") {
            immediate = true;
        }
        boost::thread([this, emoji, immediate]() {
            ESP_UTILS_CHECK_FALSE_EXIT(
                expression.setEmoji(emoji, {.immediate = immediate}, {.immediate = immediate}), "Set emoji failed"
            );
        }).detach();
    }));
    _agent_connections.push_back(coze_chat_speaking_signal.connect([this](bool is_speaking) {
        ESP_UTILS_LOG_TRACE_GUARD();

        if (!isWiFiValid()) {
            return;
        }

        ESP_UTILS_LOGI("Speaking: %s", is_speaking ? "true" : "false");
        if (!is_speaking && isWiFiValid()) {
            boost::thread([this]() {
                ESP_UTILS_CHECK_FALSE_EXIT(
                    expression.setEmoji("neutral", {.immediate = false}, {.immediate = false}), "Set emoji failed"
                );
            }).detach();
        }
        _flags.is_speaking = is_speaking;
    }));
    _agent_connections.push_back(coze_chat_websocket_disconnected_signal.connect([this]() {
        ESP_UTILS_LOG_TRACE_GUARD();

        if (!isWiFiValid()) {
            return;
        }

        if (_agent->hasChatState(Agent::_ChatStateStart)) {
            ESP_UTILS_CHECK_FALSE_EXIT(
                _agent->sendChatEvent(Agent::ChatEvent::Stop), "Send chat event stop failed"
            );
        }
        if (isWiFiValid()) {
            if (_flags.is_coze_error) {
                boost::thread([this]() {
                    auto delay_ms = AUDIO_COZE_ERROR_INSUFFICIENT_CREDITS_BALANCE_REPEAT_INTERVAL_MS * AUDIO_PLAY_LOOP_COUNT;
                    boost::this_thread::sleep_for(boost::chrono::milliseconds(delay_ms));

                    if (!_agent->hasChatState(Agent::_ChatStateStart) && isWiFiValid()) {
                        ESP_UTILS_CHECK_FALSE_EXIT(
                            _agent->sendChatEvent(Agent::ChatEvent::Start), "Send chat event start failed"
                        );
                    }
                }).detach();
            } else {
                ESP_UTILS_CHECK_FALSE_EXIT(
                    _agent->sendChatEvent(Agent::ChatEvent::Start, false), "Send chat event start failed"
                );
            }
        }
    }));
    _agent_connections.push_back(coze_chat_error_signal.connect([this](int code) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGI("Chat error code: %d", code);

        if (code == COZE_CHAT_ERROR_CODE_INSUFFICIENT_CREDITS_BALANCE_1 ||
                code == COZE_CHAT_ERROR_CODE_INSUFFICIENT_CREDITS_BALANCE_2) {
            _flags.is_coze_error = true;

            sendAudioEvent({
                AudioType::CozeErrorInsufficientCreditsBalance, AUDIO_PLAY_LOOP_COUNT,
                AUDIO_COZE_ERROR_INSUFFICIENT_CREDITS_BALANCE_REPEAT_INTERVAL_MS
            });
        }
    }));

    FunctionDefinition terminateChat("terminate_chat", "Back down. 退下吧");
    terminateChat.setCallback([this](const std::vector<FunctionParameter> &params) {
        ESP_UTILS_LOG_TRACE_GUARD();

        if (!playRandomAudio(_sleep_audios)) {
            ESP_UTILS_LOGW("Play sleep audio failed");
        }
        ESP_UTILS_CHECK_FALSE_EXIT(
            _agent->sendChatEvent(Agent::ChatEvent::Sleep), "Send chat event sleep failed"
        );
    }, std::nullopt);
    FunctionDefinitionList::requestInstance().addFunction(terminateChat);

    ESP_UTILS_CHECK_FALSE_RETURN(
        _agent->sendChatEvent(Agent::ChatEvent::Init), false, "Send chat event init failed"
    );

    del_function.release();

    _flags.is_begun = true;

    return true;
}

bool AI_Buddy::resume()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(_mutex);

    ESP_UTILS_CHECK_FALSE_RETURN(_flags.is_begun, false, "Not begun");
    if (!_flags.is_pause) {
        ESP_UTILS_LOGW("Not pause");
        return true;
    }

    _flags.is_pause = false;
    bool is_chat_started = _agent->hasChatState(Agent::ChatStateStarted);

    if (is_chat_started) {
        ESP_UTILS_CHECK_FALSE_RETURN(_agent->resume(), false, "Agent resume failed");
        stopAudio(AudioType::MicOff);
        sendAudioEvent({AudioType::MicOn});
        if (!_agent->hasChatState(Agent::_ChatStateSleep)) {
            ESP_UTILS_CHECK_FALSE_RETURN(
                _agent->sendChatEvent(Agent::ChatEvent::Sleep), false, "Send chat event sleep failed"
            );
        } else {
            sendAudioEvent({AudioType::WakeUp});
        }
    }

    ESP_UTILS_CHECK_FALSE_RETURN(expression.resume(!is_chat_started, !is_chat_started), false, "Expression resume failed");
    if (is_chat_started) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            expression.setEmoji("sleepy", {.repeat = false, .keep_when_stop = true}, {.repeat = true}),
            false, "Set emoji failed"
        );
    }

    if (_agent->hasChatState(Agent::ChatStateInited)) {
        ESP_UTILS_CHECK_ERROR_RETURN(audio_manager_suspend(false), false, "Audio manager suspend failed");
    }

    return true;
}

bool AI_Buddy::pause()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(_mutex);

    ESP_UTILS_CHECK_FALSE_RETURN(_flags.is_begun, false, "Not begun");
    if (_flags.is_pause) {
        ESP_UTILS_LOGW("Already pause");
        return true;
    }

    ESP_UTILS_CHECK_FALSE_RETURN(_agent->pause(), false, "Agent pause failed");
    if (_agent->hasChatState(Agent::ChatStateStarted)) {
        stopAudio(AudioType::MicOn);
        sendAudioEvent({AudioType::MicOff});
    } else if (_agent->hasChatState(Agent::ChatStateInited)) {
        ESP_UTILS_CHECK_ERROR_RETURN(audio_manager_suspend(true), false, "Audio manager suspend failed");
    }

    ESP_UTILS_CHECK_FALSE_RETURN(expression.pause(), false, "Expression pause failed");

    _flags.is_pause = true;
    return true;
}

bool AI_Buddy::del()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(_mutex);

    _flags = {};
    for (auto &connection : _agent_connections) {
        connection.disconnect();
    }
    _agent_connections.clear();

    if (!expression.del()) {
        ESP_UTILS_LOGE("Expression del failed");
    }
    if (!_agent->del()) {
        ESP_UTILS_LOGE("Agent del failed");
    }

    return true;
}

std::shared_ptr<AI_Buddy> AI_Buddy::requestInstance()
{
    std::lock_guard lock(_instance_mutex);

    if (_instance == nullptr) {
        ESP_UTILS_CHECK_EXCEPTION_RETURN(
            _instance = std::shared_ptr<AI_Buddy>(new AI_Buddy()), nullptr, "New AI_Buddy failed"
        );
    }

    return _instance;
}

void AI_Buddy::releaseInstance()
{
    ESP_UTILS_LOG_TRACE_GUARD();

    std::lock_guard lock(_instance_mutex);

    if (_instance.use_count() == 1) {
        _instance = nullptr;
    }
}

void AI_Buddy::sendAudioEvent(const AudioEvent &event)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: type(%s)", getAudioName(event.type).c_str());

    std::lock_guard lock(_audio_event_mutex);
    stopAudio(event.type);
    _audio_next_process_infos.push_back({event, esp_timer_get_time() / 1000, 0});
    _audio_event_cv.notify_all();
}

void AI_Buddy::stopAudio(AudioType type)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: type(%s)", getAudioName(type).c_str());

    std::lock_guard lock(_audio_event_mutex);
    _audio_next_process_infos.remove_if([type](const AudioProcessInfo & info) {
        return info.event.type == type;
    });

    auto it = std::find(_audio_removed_process_infos.begin(), _audio_removed_process_infos.end(), type);
    if (it != _audio_removed_process_infos.end()) {
        ESP_UTILS_LOGD("Audio type already in removed process infos: %d", static_cast<int>(type));
        return;
    }
    _audio_removed_process_infos.push_back(type);
}

bool AI_Buddy::processOnWiFiEvent(int32_t event_id, void *event_data)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Process WiFi event: %d", static_cast<int>(event_id));

    switch (event_id) {
    case WIFI_EVENT_STA_DISCONNECTED: {
        if (!_flags.is_wifi_connected) {
            break;
        }

        _flags.is_wifi_connected = false;
        ESP_UTILS_CHECK_FALSE_RETURN(
            _agent->sendChatEvent(Agent::ChatEvent::Stop), false, "Send chat event stop failed"
        );
        ESP_UTILS_CHECK_FALSE_RETURN(expression.setEmoji("neutral"), false, "Set emoji failed");
        ESP_UTILS_CHECK_FALSE_RETURN(expression.setSystemIcon("wifi_disconnected"), false, "Set WiFi icon failed");
        sendAudioEvent({AudioType::WifiDisconnected});
        playWiFiNeedConnectAudio();
        break;
    }
    default:
        break;
    }

    return true;
}

bool AI_Buddy::processOnIpEvent(int32_t event_id, void *event_data)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Process IP event: %d", static_cast<int>(event_id));

    switch (event_id) {
    case IP_EVENT_STA_GOT_IP:
        _flags.is_wifi_connected = true;
        if (_agent->hasChatState(Agent::ChatStateInited) && !_agent->hasChatState(Agent::_ChatStateStart)) {
            ESP_UTILS_CHECK_FALSE_RETURN(
                _agent->sendChatEvent(Agent::ChatEvent::Start), false, "Send chat event start failed"
            );
        }
        stopAudio(AudioType::WifiNeedConnect);
        sendAudioEvent({AudioType::WifiConnected});
        break;
    default:
        break;
    }

    return true;
}

bool AI_Buddy::processAudioEvent(AudioProcessInfo &info)
{
    // ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    auto it = _audio_file_map.find(info.event.type);
    ESP_UTILS_CHECK_FALSE_RETURN(
        it != _audio_file_map.end(), false, "Invalid audio type(%d)", static_cast<int>(info.event.type)
    );

    // If the audio has been played before, check if the playback interval has been reached, if not, skip playback
    if ((info.last_play_time_ms != 0) && (info.last_play_time_ms + info.event.repeat_interval_ms) > (esp_timer_get_time() / 1000)) {
        return true;
    }

    // If the audio is playing other audio, use the interval time of the audio as the timeout time
    int64_t timeout_ms = 0;
    if (_audio_playing_type != AudioType::Max) {
        auto it_playing = _audio_file_map.find(_audio_playing_type);
        ESP_UTILS_CHECK_FALSE_RETURN(
            it_playing != _audio_file_map.end(), false, "Invalid audio type(%d)", static_cast<int>(_audio_playing_type)
        );
        timeout_ms = it_playing->second.second;
    }

    ESP_UTILS_LOGI("Play audio: %s(create_time_ms: %d, last_play_time_ms: %d) with timeout_ms: %d",
                   getAudioName(info.event.type).c_str(), static_cast<int>(info.create_time_ms),
                   static_cast<int>(info.last_play_time_ms), static_cast<int>(timeout_ms)
                  );
    ESP_UTILS_CHECK_FALSE_RETURN(
        audio_prompt_play_with_block(it->second.first.c_str(), -1) == ESP_OK, false, "Play audio failed"
    );
    _audio_playing_type = info.event.type;

    info.last_play_time_ms = esp_timer_get_time() / 1000;
    if (info.event.repeat_count > 0) {
        info.event.repeat_count--;
    }

    return true;
}

void AI_Buddy::playWiFiNeedConnectAudio()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (isWiFiValid()) {
        ESP_UTILS_LOGD("WiFi is valid");
    }

    ESP_UTILS_LOGD("WiFi is not valid, play audio in %d ms", AUDIO_WIFI_NEED_CONNECT_DELAY_MS);
    {
        esp_utils::thread_config_guard thread_config(esp_utils::ThreadConfig{
            .name = "wifi_check",
            .stack_size = 4 * 1024,
            .stack_in_ext = true,
        });
        boost::thread([this]() {
            ESP_UTILS_LOG_TRACE_GUARD();
            // Wait for delay time to ensure the WiFi is connected
            boost::this_thread::sleep_for(boost::chrono::milliseconds(AUDIO_WIFI_NEED_CONNECT_DELAY_MS));
            if (!isWiFiValid()) {
                sendAudioEvent({AudioType::WifiNeedConnect, AUDIO_PLAY_LOOP_COUNT, AUDIO_WIFI_NEED_CONNECT_REPEAT_INTERVAL_MS});
            }
        }).detach();
    }
}

bool AI_Buddy::playRandomAudio(const RandomAudios &audios)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    static std::random_device rd;
    static std::mt19937 gen(rd());

    float total_probability = 0.0f;
    for (const auto &pair : audios) {
        total_probability += pair.first;
    }

    std::uniform_real_distribution<> dis(0.0f, total_probability);
    float random_value = dis(gen);
    ESP_UTILS_LOGD("Random value: %f", random_value);

    float cumulative_probability = 0.0f;
    AudioType selected_audio = AudioType::Max;

    for (const auto &pair : audios) {
        cumulative_probability += pair.first;
        if (random_value <= cumulative_probability) {
            selected_audio = pair.second;
            break;
        }
    }
    ESP_UTILS_CHECK_FALSE_RETURN(selected_audio != AudioType::Max, false, "Invalid audio type");

    auto it = _audio_file_map.find(selected_audio);
    ESP_UTILS_CHECK_FALSE_RETURN(it != _audio_file_map.end(), false, "Invalid audio type");

    sendAudioEvent({selected_audio});

    return true;
}

std::string AI_Buddy::getAudioName(AudioType type) const
{
    auto it = _audio_file_map.find(type);
    ESP_UTILS_CHECK_FALSE_RETURN(it != _audio_file_map.end(), "", "Invalid audio type");

    return it->second.first;
}

} // namespace esp_brookesia::systems::speaker
