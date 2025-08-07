/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <queue>
#include <future>
#include "boost/thread.hpp"
#include "boost/signals2.hpp"
#include "coze_chat_app.hpp"
#include "audio_processor.h"
#include "function_calling.hpp"

namespace esp_brookesia::ai_framework {

class Agent {
public:
    enum ChatState {
        ChatStateDeinit   = 0,
        _ChatStateInit    = (1ULL << 0),
        ChatStateIniting  = (_ChatStateInit    | (1ULL << 1)),
        ChatStateInited   = (_ChatStateInit    | (1ULL << 2)),
        _ChatStateStop    = (ChatStateInited   | (1ULL << 3)),
        ChatStateStopping = (_ChatStateStop    | (1ULL << 4)),
        ChatStateStopped  = (_ChatStateStop    | (1ULL << 5)),
        _ChatStateStart   = (ChatStateInited   | (1ULL << 6)),
        ChatStateStarting = (_ChatStateStart   | (1ULL << 7)),
        ChatStateStarted  = (_ChatStateStart   | (1ULL << 8)),
        _ChatStateSleep   = (ChatStateStarted  | (1ULL << 9)),
        ChatStateSleeping = (_ChatStateSleep   | (1ULL << 10)),
        ChatStateSlept    = (_ChatStateSleep   | (1ULL << 11)),
        _ChatStateWake    = (ChatStateStarted  | (1ULL << 12)),
        ChatStateWaking   = (_ChatStateWake    | (1ULL << 13)),
        ChatStateWaked    = (_ChatStateWake    | (1ULL << 14)),
    };
    enum class ChatEvent {
        Deinit,
        Init,
        Stop,
        Start,
        Sleep,
        WakeUp,
    };
    enum class ChatEventSpecialSignalType {
        InitInvalidConfig,
        StartMaxRetry,
    };

    using ChatEventProcessSpecialSignal = boost::signals2::signal<void(const ChatEventSpecialSignalType &type)>;
    using ChatEventProcessStartSignal = boost::signals2::signal<void(const ChatEvent &current_event, const ChatEvent &last_event)>;
    using ChatEventProcessEndSignal = boost::signals2::signal<void(const ChatEvent &current_event, const ChatEvent &last_event)>;

    Agent(const Agent &) = delete;
    Agent(Agent &&) = delete;
    Agent &operator=(const Agent &) = delete;
    Agent &operator=(Agent &&) = delete;

    ~Agent();

    bool configCozeAgentConfig(CozeChatAgentInfo &agent_info, std::vector<CozeChatRobotInfo> &robot_infos);

    bool begin();
    bool del();
    bool pause();
    bool resume();

    bool setCurrentRobotIndex(int index);
    bool getCurrentRobotIndex(int &index) const;
    bool getRobotInfo(int index, CozeChatRobotInfo &info) const;
    bool getRobotInfo(std::vector<CozeChatRobotInfo> &infos) const;

    bool sendChatEvent(const ChatEvent &event, bool clear_queue = true, int wait_finish_timeout_ms = 0);

    bool hasChatState(ChatState state) const
    {
        return (_chat_state & state) == state;
    }
    bool isChatState(ChatState state) const
    {
        return (_chat_state == state);
    }

    bool isPaused() const
    {
        return _flags.is_paused;
    }

    static std::shared_ptr<Agent> requestInstance();
    static void releaseInstance();
    static std::string chatEventToString(const ChatEvent &event);
    static std::string chatStateToString(const ChatState &state);

    ChatEventProcessSpecialSignal chat_event_process_special_signal;
    ChatEventProcessStartSignal chat_event_process_start_signal;
    ChatEventProcessEndSignal chat_event_process_end_signal;

private:
    using EventPromise = std::promise<bool>;
    struct ChatEventWrapper {
        ChatEvent event;
        std::shared_ptr<EventPromise> promise;
    };

    Agent() = default;

    bool processChatEvent(const ChatEvent &event);

    static bool isTimeSync();
    static bool getMacStr(std::string &mac_str);

    struct {
        int is_begun: 1;
        int is_paused: 1;
        int is_coze_error: 1;
    } _flags = {};
    std::mutex _mutex;

    CozeChatAgentInfo _agent_info = {};
    std::vector<CozeChatRobotInfo> _robot_infos;
    int _robot_index = 0;

    ChatState _chat_state = ChatState::ChatStateDeinit;
    ChatEvent _last_chat_event = ChatEvent::Deinit;
    boost::thread _chat_event_thread;
    std::queue<ChatEventWrapper> _chat_event_queue;
    std::recursive_mutex _chat_event_mutex;
    boost::condition_variable_any _chat_event_cv;

    std::vector<boost::signals2::connection> _connections;

    inline static std::mutex _instance_mutex;
    inline static std::shared_ptr<Agent> _instance;
};

} // namespace esp_brookesia::ai_agent
