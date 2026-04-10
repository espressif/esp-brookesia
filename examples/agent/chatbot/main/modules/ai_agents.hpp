/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <atomic>
#include <vector>
#include "brookesia/service_manager/event/registry.hpp"
#include "brookesia/service_manager/service/manager.hpp"

class AI_Agents {
public:
    struct Config {
        std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> task_scheduler;
        uint32_t afe_wakeup_start_timeout_ms = 30000;
        uint32_t afe_wakeup_end_timeout_ms = 10000;
        uint32_t emote_animation_duration_ms = 5000;
        uint32_t agent_restart_delay_s = 30;
        uint32_t coze_error_show_emote_delay_s = 2;
    };

    static AI_Agents &get_instance()
    {
        static AI_Agents instance;
        return instance;
    }

    bool init(const Config &config);

    bool is_initialized() const
    {
        return task_scheduler_ != nullptr;
    }

    void init_coze();
    void init_openai();
    void init_xiaozhi();

private:
    AI_Agents() = default;
    ~AI_Agents() = default;
    AI_Agents(const AI_Agents &) = delete;
    AI_Agents &operator=(const AI_Agents &) = delete;

    bool start_agent();
    void stop_agent();

    void process_agent_general_events();
    void process_agent_general_unexpected_events();
    void process_agent_general_suspend_status_changed();

    void process_emote();
    void process_emote_when_general_action_triggered();
    void process_emote_when_general_event_happened();
    void process_emote_when_suspend_status_changed();
    void process_emote_when_speaking_status_changed();
    void process_emote_when_listening_status_changed();
    void process_emote_when_agent_speaking_text_got();
    void process_emote_when_user_speaking_text_got();
    void process_emote_when_emote_got();
    void process_emote_when_coze_event_happened();

    void process_wifi_events();

    bool is_listening() const
    {
        return is_listening_;
    }

    bool is_speaking() const
    {
        return is_speaking_;
    }

    bool is_suspended() const
    {
        return is_suspended_;
    }

    bool is_sleeping() const
    {
        return is_sleeping_;
    }

    bool is_stopped() const
    {
        return is_stopped_;
    }

    bool is_inactive() const
    {
        return is_sleeping() || is_suspended() || is_stopped();
    }

    bool is_wifi_connected() const
    {
        return is_wifi_connected_;
    }

    static boost::json::object get_agent_coze_info();
    static boost::json::object get_agent_openai_info();

    std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> task_scheduler_;
    uint32_t emote_animation_duration_ms_ = 0;
    uint32_t agent_restart_delay_s_ = 0;
    uint32_t coze_error_show_emote_delay_s_ = 0;

    bool is_listening_ = false;
    bool is_speaking_ = false;
    bool is_suspended_ = false;
    bool is_sleeping_ = false;
    bool is_stopped_ = false;
    bool is_wifi_connected_ = false;

    // Keep service bindings to avoid frequent start and stop of services
    std::vector<esp_brookesia::service::ServiceBinding> service_bindings_;
    // Keep event connections
    std::vector<esp_brookesia::service::EventRegistry::SignalConnection> service_connections_;
};
