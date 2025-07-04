/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <random>
#include "esp_mac.h"
#include "private/esp_brookesia_ai_agent_utils.hpp"
#include "esp_coze_chat.h"
#include "coze_chat_app.hpp"
#include "audio_processor.h"
#include "function_calling.hpp"
#include "esp_brookesia_ai_agent.hpp"

#define SEND_CHAT_EVENT_TIMEOUT_MS              (1000)

#define CHAT_EVENT_THREAD_NAME                  "chat_event"
#define CHAT_EVENT_THREAD_STACK_SIZE            (6 * 1024)
#define CHAT_EVENT_THREAD_STACK_CAPS_EXT        (false)
#define CHAT_EVENT_COZE_START_REPEAT_TIMEOUT_MS (30 * 1000)

#define TIMEOUT_MS_MAX                          (60 * 60 * 1000)

namespace esp_brookesia::ai_framework {

Agent::~Agent()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_EXIT(!_flags.is_begun || del(), "Del failed");
}

bool Agent::configCozeAgentConfig(CozeChatAgentInfo &agent_info, std::vector<CozeChatRobotInfo> &robot_infos)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(!_flags.is_begun, false, "Should be called before `begin()`");

    std::string mac_str;
    ESP_UTILS_CHECK_FALSE_RETURN(getMacStr(mac_str), false, "Failed to get MAC address");
    ESP_UTILS_LOGD("Get MAC address: %s", mac_str.c_str());

    _agent_info.session_name = agent_info.session_name.empty() ? mac_str : agent_info.session_name;
    _agent_info.device_id = agent_info.device_id.empty() ? mac_str : agent_info.device_id;
    _agent_info.user_id = agent_info.user_id.empty() ? mac_str : agent_info.user_id;
    _agent_info.custom_consumer = agent_info.custom_consumer;
    _agent_info.app_id = agent_info.app_id;
    _agent_info.public_key = agent_info.public_key;
    _agent_info.private_key = agent_info.private_key;
    ESP_UTILS_CHECK_FALSE_RETURN(_agent_info.isValid(), false, "Invalid chat info");
#if ESP_UTILS_LOG_LEVEL == ESP_UTILS_LOG_LEVEL_DEBUG
    _agent_info.dump();
#endif

    _robot_infos = robot_infos;
    for (auto &robot_info : _robot_infos) {
        ESP_UTILS_CHECK_FALSE_RETURN(robot_info.isValid(), false, "Invalid robot info");
#if ESP_UTILS_LOG_LEVEL == ESP_UTILS_LOG_LEVEL_DEBUG
        robot_info.dump();
#endif
    }

    return true;
}


bool Agent::begin()
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

    {
        esp_utils::thread_config_guard thread_config(esp_utils::ThreadConfig{
            .name = CHAT_EVENT_THREAD_NAME,
            .stack_size = CHAT_EVENT_THREAD_STACK_SIZE,
            .stack_in_ext = CHAT_EVENT_THREAD_STACK_CAPS_EXT,
        });
        _chat_event_thread = boost::thread([this]() {
            ESP_UTILS_LOG_TRACE_GUARD();

            while (true) {
                std::unique_lock<std::recursive_mutex> lock(_chat_event_mutex);
                _chat_event_cv.wait(lock, [this]() {
                    return !_chat_event_queue.empty();
                });

                while (!_chat_event_queue.empty()) {
                    auto event_wrapper = _chat_event_queue.front();
                    _chat_event_queue.pop();

                    lock.unlock();
                    auto ret = processChatEvent(event_wrapper.event);
                    lock.lock();

                    if (event_wrapper.promise != nullptr) {
                        event_wrapper.promise->set_value(ret);
                    }
                }
            }
        });
    }

    _flags.is_begun = true;

    del_function.release();

    return true;
}

bool Agent::del()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(_mutex);

    bool ret = true;
    if (!sendChatEvent(ChatEvent::Stop, true, SEND_CHAT_EVENT_TIMEOUT_MS)) {
        ESP_UTILS_LOGE("Stop chat event failed");
        ret = false;
    }

    _flags = {};
    _robot_index = 0;
    _chat_state = ChatState::ChatStateDeinit;
    _last_chat_event = ChatEvent::Deinit;
    while (!_chat_event_queue.empty()) {
        _chat_event_queue.pop();
    }

    return ret;
}

bool Agent::setCurrentRobotIndex(int index)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: index(%d)", index);
    ESP_UTILS_CHECK_FALSE_RETURN(_flags.is_begun, false, "Not begun");
    ESP_UTILS_CHECK_VALUE_RETURN(index, 0, _robot_infos.size() - 1, false, "Invalid robot index");

    _robot_index = index;

    return true;
}

bool Agent::getCurrentRobotIndex(int &index) const
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    index = _robot_index;

    return true;
}

bool Agent::getRobotInfo(int index, CozeChatRobotInfo &info) const
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_VALUE_RETURN(index, 0, _robot_infos.size() - 1, false, "Invalid robot index");
    info = _robot_infos[index];

    return true;
}

bool Agent::getRobotInfo(std::vector<CozeChatRobotInfo> &infos) const
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    infos = _robot_infos;

    return true;
}

bool Agent::sendChatEvent(const ChatEvent &event, bool clear_queue, int wait_finish_timeout_ms)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD(
        "Param: event(%d), clear_queue(%d), wait_finish_timeout_ms(%d)", static_cast<int>(event),
        clear_queue, wait_finish_timeout_ms
    );
    ESP_UTILS_CHECK_FALSE_RETURN(_flags.is_begun, false, "Not begun");

    ChatEventWrapper event_wrapper = {
        .event = event,
    };
    if (wait_finish_timeout_ms != 0) {
        event_wrapper.promise = std::make_shared<std::promise<bool>>();
    }
    if (wait_finish_timeout_ms < 0) {
        wait_finish_timeout_ms = TIMEOUT_MS_MAX;
    }

    {
        std::lock_guard lock(_chat_event_mutex);
        if (clear_queue) {
            while (!_chat_event_queue.empty()) {
                auto event_wrapper_tmp = _chat_event_queue.front();
                _chat_event_queue.pop();
                ESP_UTILS_LOGD("Pop event: %d", static_cast<int>(event_wrapper_tmp.event));
            }
        }
        _chat_event_queue.push(event_wrapper);
        _chat_event_cv.notify_all();
    }

    if (event_wrapper.promise == nullptr) {
        ESP_UTILS_LOGD("Don't wait chat event finish");
        return true;
    }

    ESP_UTILS_LOGD(
        "Wait chat event finish: %d, timeout_ms(%d)", static_cast<int>(event_wrapper.event), wait_finish_timeout_ms
    );
    auto future = event_wrapper.promise->get_future();
    ESP_UTILS_CHECK_FALSE_RETURN(
        future.wait_for(std::chrono::milliseconds(wait_finish_timeout_ms)) == std::future_status::ready,
        false, "Wait chat event finish timeout"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(future.get(), false, "Wait chat event finish failed");

    return true;
}

std::shared_ptr<Agent> Agent::requestInstance()
{
    std::lock_guard lock(_instance_mutex);

    if (_instance == nullptr) {
        ESP_UTILS_CHECK_EXCEPTION_RETURN(
            _instance = std::shared_ptr<Agent>(new Agent()), nullptr, "Failed to create instance"
        );
    }

    return _instance;
}

void Agent::releaseInstance()
{
    std::lock_guard lock(_instance_mutex);

    if (_instance.use_count() == 1) {
        _instance = nullptr;
    }
}

bool Agent::isTimeSync()
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    return (timeinfo.tm_year > (2020 - 1900));
}

bool Agent::processChatEvent(const ChatEvent &event)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Process chat event: %d", static_cast<int>(event));
    ESP_UTILS_LOGD(
        "Current chat state(%d), last chat event(%d)", static_cast<int>(_chat_state),
        static_cast<int>(_last_chat_event)
    );

    chat_event_process_start_signal(event, _last_chat_event);

    switch (event) {
    case ChatEvent::Deinit:
        break;
    case ChatEvent::Init: {
        if (hasChatState(_ChatStateInit)) {
            ESP_UTILS_LOGW("Chat already initing or inited");
            return true;
        }

        {
            esp_utils::value_guard chat_state_guard(_chat_state);
            chat_state_guard.set(ChatStateIniting);

            bool is_coze_agent_info_valid = true;
            if (!_agent_info.isValid() || _robot_infos.empty()) {
                is_coze_agent_info_valid = false;
            }
            for (const auto &robot_info : _robot_infos) {
                if (!robot_info.isValid()) {
                    is_coze_agent_info_valid = false;
                    break;
                }
            }
            if (!is_coze_agent_info_valid) {
                chat_event_process_special_signal(ChatEventSpecialSignalType::InitInvalidConfig);
                ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Coze agent info init failed");
            }

            ESP_UTILS_CHECK_FALSE_RETURN(coze_chat_app_init() == ESP_OK, false, "Init chat failed");

            chat_state_guard.set(ChatStateInited);
            chat_state_guard.release();
        }
        break;
    }
    case ChatEvent::Stop: {
        if (hasChatState(_ChatStateStop)) {
            ESP_UTILS_LOGW("Chat already stopping or stopped");
            return true;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(
            hasChatState(_ChatStateStart), false, "Invalid chat state(%d)", static_cast<int>(_chat_state)
        );

        {
            esp_utils::value_guard chat_state_guard(_chat_state);
            chat_state_guard.set(ChatStateStopping);

            coze_chat_app_pause();
            ESP_UTILS_CHECK_FALSE_RETURN(coze_chat_app_stop() == ESP_OK, false, "Stop chat failed");

            chat_state_guard.set(ChatStateStopped);
            chat_state_guard.release();
        }
        break;
    }
    case ChatEvent::Start: {
        if (hasChatState(_ChatStateStart)) {
            ESP_UTILS_LOGW("Chat already starting or started");
            return true;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(
            hasChatState(ChatStateInited), false, "Invalid chat state(%d)", static_cast<int>(_chat_state)
        );

        {
            esp_utils::value_guard chat_state_guard(_chat_state);
            chat_state_guard.set(ChatStateStarting);

            while (!isTimeSync()) {
                ESP_UTILS_LOGI("Wait for time sync...");
                boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
            }

            int retry_count = 0;
            const int max_retries = CHAT_EVENT_COZE_START_REPEAT_TIMEOUT_MS / 1000;
            while (retry_count < max_retries) {
                if (coze_chat_app_start(_agent_info, _robot_infos[_robot_index]) == ESP_OK) {
                    break;
                }
                ESP_UTILS_LOGE("Start chat failed, retry %d/%d", retry_count + 1, max_retries);
                retry_count++;
                if (retry_count < max_retries) {
                    boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
                }
            }

            if (retry_count >= max_retries) {
                chat_event_process_special_signal(ChatEventSpecialSignalType::StartMaxRetry);
                ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Start chat failed after %d retries", max_retries);
            }
            coze_chat_app_resume();
            _flags.is_sleep = false;
            _flags.is_paused = false;

            chat_state_guard.set(ChatStateStarted);
            chat_state_guard.release();
        }
        break;
    }
    case ChatEvent::Pause: {
        if (_flags.is_paused) {
            ESP_UTILS_LOGW("Chat already paused");
            return true;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(hasChatState(_ChatStateStart), false, "Chat not started");

        coze_chat_app_pause();
        _flags.is_paused = true;
        break;
    }
    case ChatEvent::Resume: {
        if (!_flags.is_paused) {
            ESP_UTILS_LOGW("Chat not paused");
            return true;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(hasChatState(_ChatStateStart), false, "Chat not started");

        coze_chat_app_resume();
        _flags.is_paused = false;
        break;
    }
    case ChatEvent::Sleep: {
        if (_flags.is_sleep) {
            ESP_UTILS_LOGW("Chat already sleep");
            return true;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(hasChatState(_ChatStateStart), false, "Chat not started");

        // Should be called before set chat state to sleep
        coze_chat_app_sleep();
        _flags.is_sleep = true;
        break;
    }
    case ChatEvent::WakeUp: {
        if (!_flags.is_sleep) {
            ESP_UTILS_LOGW("Chat not sleep");
            return true;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(hasChatState(_ChatStateStart), false, "Chat not started");

        coze_chat_app_wakeup();
        _flags.is_sleep = false;
        break;
    }
    default:
        break;
    }

    chat_event_process_end_signal(event, _last_chat_event);

    _last_chat_event = event;
    return true;
}

bool Agent::getMacStr(std::string &mac_str)
{
    uint8_t mac[6] = {0};
    esp_err_t err = esp_efuse_mac_get_default(mac);
    ESP_UTILS_CHECK_FALSE_RETURN(err == ESP_OK, false, "Failed to get MAC address(%s)", esp_err_to_name(err));

    char mac_hex[13];
    snprintf(mac_hex, sizeof(mac_hex), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    mac_str = "ESP_" + std::string(mac_hex);

    return true;
}

} // namespace esp_brookesia::ai_agent
