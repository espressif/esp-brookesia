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
#if ESP_UTILS_CONF_LOG_LEVEL == ESP_UTILS_LOG_LEVEL_DEBUG
    _agent_info.dump();
#endif

    _robot_infos = robot_infos;
    for (auto &robot_info : _robot_infos) {
        ESP_UTILS_CHECK_FALSE_RETURN(robot_info.isValid(), false, "Invalid robot info");
#if ESP_UTILS_CONF_LOG_LEVEL == ESP_UTILS_LOG_LEVEL_DEBUG
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

    _connections.push_back(coze_chat_error_signal.connect([this](int code) {
        ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

        if (code == COZE_CHAT_ERROR_CODE_INSUFFICIENT_CREDITS_BALANCE_1 ||
                code == COZE_CHAT_ERROR_CODE_INSUFFICIENT_CREDITS_BALANCE_2) {
            _flags.is_coze_error = true;
        }
    }));

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

    for (auto &connection : _connections) {
        connection.disconnect();
    }
    _connections.clear();

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

bool Agent::pause()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (_flags.is_paused) {
        ESP_UTILS_LOGW("Already paused");
        return true;
    }

    coze_chat_app_pause();
    _flags.is_paused = true;

    return true;
}

bool Agent::resume()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (!_flags.is_paused) {
        ESP_UTILS_LOGW("Not paused");
        return true;
    }

    coze_chat_app_resume();
    _flags.is_paused = false;

    return true;
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
        "Param: event(%s), clear_queue(%d), wait_finish_timeout_ms(%d)", chatEventToString(event).c_str(),
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
                ESP_UTILS_LOGD("Pop event: %s", chatEventToString(event_wrapper_tmp.event).c_str());
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
        "Wait chat event finish: %s, timeout_ms(%d)", chatEventToString(event_wrapper.event).c_str(),
        wait_finish_timeout_ms
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

std::string Agent::chatStateToString(const ChatState &state)
{
    switch (state) {
    case ChatState::ChatStateDeinit:
        return "Deinit";
    case ChatState::ChatStateIniting:
        return "Initing";
    case ChatState::ChatStateInited:
        return "Inited";
    case ChatState::ChatStateStopping:
        return "Stopping";
    case ChatState::ChatStateStopped:
        return "Stopped";
    case ChatState::ChatStateStarting:
        return "Starting";
    case ChatState::ChatStateStarted:
        return "Started";
    case ChatState::ChatStateSleeping:
        return "Sleeping";
    case ChatState::ChatStateSlept:
        return "Slept";
    case ChatState::ChatStateWaking:
        return "Waking";
    case ChatState::ChatStateWaked:
        return "Waked";
    default:
        return "Unknown";
    }
}

std::string Agent::chatEventToString(const ChatEvent &event)
{
    switch (event) {
    case ChatEvent::Deinit:
        return "Deinit";
    case ChatEvent::Init:
        return "Init";
    case ChatEvent::Stop:
        return "Stop";
    case ChatEvent::Start:
        return "Start";
    case ChatEvent::Sleep:
        return "Sleep";
    case ChatEvent::WakeUp:
        return "WakeUp";
    default:
        return "Unknown";
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

    ESP_UTILS_LOGD("Process chat event: %s", chatEventToString(event).c_str());
    ESP_UTILS_LOGD(
        "Current chat state(%s), last chat event(%s)", chatStateToString(_chat_state).c_str(),
        chatEventToString(_last_chat_event).c_str()
    );

    if (event == _last_chat_event) {
        ESP_UTILS_LOGW("Chat event already processed");
        return true;
    }

    chat_event_process_start_signal(event, _last_chat_event);

    switch (event) {
    case ChatEvent::Deinit:
        break;
    case ChatEvent::Init: {
        if (hasChatState(_ChatStateInit)) {
            ESP_UTILS_LOGW("Chat already init");
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
            ESP_UTILS_LOGW("Chat already stopped");
            return true;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(hasChatState(ChatStateInited), false, "Invalid chat state");

        {
            esp_utils::value_guard chat_state_guard(_chat_state);
            chat_state_guard.set(ChatStateStopping);

            ESP_UTILS_CHECK_FALSE_RETURN(coze_chat_app_stop() == ESP_OK, false, "Stop chat failed");

            chat_state_guard.set(ChatStateStopped);
            chat_state_guard.release();
        }
        break;
    }
    case ChatEvent::Start: {
        if (hasChatState(_ChatStateStart)) {
            ESP_UTILS_LOGW("Chat already started");
            return true;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(hasChatState(ChatStateInited), false, "Invalid chat state");

        {
            esp_utils::value_guard chat_state_guard(_chat_state);
            chat_state_guard.set(ChatStateStarting);

            while (!isTimeSync()) {
                ESP_UTILS_LOGI("Wait for time sync...");
                boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
            }

            _flags.is_coze_error = false;
            int retry_count = 0;
            const int max_retries = CHAT_EVENT_COZE_START_REPEAT_TIMEOUT_MS / 1000;
            while ((retry_count < max_retries) && !_flags.is_coze_error) {
                if (coze_chat_app_start(_agent_info, _robot_infos[_robot_index]) == ESP_OK) {
                    break;
                }
                ESP_UTILS_LOGE("Start chat failed, retry %d/%d", retry_count + 1, max_retries);
                retry_count++;
                if (retry_count < max_retries) {
                    boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
                }
            }

            ESP_UTILS_CHECK_FALSE_RETURN(!_flags.is_coze_error, false, "Coze error");

            if (retry_count >= max_retries) {
                chat_event_process_special_signal(ChatEventSpecialSignalType::StartMaxRetry);
                ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Start chat failed after %d retries", max_retries);
            }

            chat_state_guard.set(ChatStateStarted);
            chat_state_guard.release();
        }
        break;
    }
    case ChatEvent::Sleep: {
        if (hasChatState(_ChatStateSleep)) {
            ESP_UTILS_LOGW("Chat already slept");
            return true;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(hasChatState(ChatStateStarted), false, "Invalid chat state");

        esp_utils::value_guard chat_state_guard(_chat_state);
        chat_state_guard.set(ChatStateSleeping);

        coze_chat_app_sleep();

        chat_state_guard.set(ChatStateSlept);
        chat_state_guard.release();
        break;
    }
    case ChatEvent::WakeUp: {
        if (hasChatState(_ChatStateWake)) {
            ESP_UTILS_LOGW("Chat already woke up");
            return true;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(isChatState(ChatStateSlept), false, "Invalid chat state");

        esp_utils::value_guard chat_state_guard(_chat_state);
        chat_state_guard.set(ChatStateWaking);

        coze_chat_app_wakeup();

        chat_state_guard.set(ChatStateWaked);
        chat_state_guard.release();
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
