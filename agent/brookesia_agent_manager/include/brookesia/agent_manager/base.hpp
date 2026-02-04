/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <bitset>
#include <functional>
#include <string>
#include "boost/thread/shared_mutex.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/agent_helper/manager.hpp"
#include "brookesia/service_helper/audio.hpp"

namespace esp_brookesia::agent {

struct ServiceConfig {
    std::vector<std::string> dependencies{};
};

struct AudioConfig {
    service::helper::Audio::EncoderDynamicConfig encoder;
    service::helper::Audio::DecoderDynamicConfig decoder;
};

using GeneralAction = helper::Manager::GeneralAction;
using GeneralEvent = helper::Manager::GeneralEvent;
using AgentAttributes = helper::Manager::AgentAttributes;
using ChatMode = helper::Manager::ChatMode;

enum class GeneralStateFlagBit {
    TimeSyncing,
    Ready,
    Activating,
    Activated,
    Starting,
    Stopping,
    Started,
    Sleeping,
    WakingUp,
    Slept,
    Error,
    Max,
};
BROOKESIA_DESCRIBE_ENUM(
    GeneralStateFlagBit, TimeSyncing, Ready, Activating, Activated, Starting, Stopping, Started, Sleeping, WakingUp,
    Slept, Error, Max
);

using GeneralStateFlags = std::bitset<BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralStateFlagBit::Max)>;

class Base: public service::ServiceBase {
public:
    friend class Manager;
    friend class StateMachine;
    friend class GeneralStateClass;

    Base(Base &&) = delete;
    Base(const Base &) = delete;
    Base &operator=(Base &&) = delete;
    Base &operator=(const Base &) = delete;

    const AgentAttributes &get_attributes() const
    {
        return attributes_;
    }

    const AudioConfig &get_audio_config() const
    {
        return audio_config_;
    }

protected:
    Base(const AgentAttributes &attributes, const AudioConfig &audio_config, const ServiceConfig &service_config = {})
        : ServiceBase(Attributes{
        .name = attributes.name.data(),
        .dependencies = [ & ]()
        {
            std::vector<std::string> deps = {service::helper::Audio::get_name().data()};
            if (!service_config.dependencies.empty()) {
                deps.insert(deps.end(), service_config.dependencies.begin(), service_config.dependencies.end());
            }
            return deps;
        }(),
        .bindable = false,
    })
    , attributes_(attributes)
    , audio_config_(audio_config)
    {}
    virtual ~Base() = default;

    std::string get_call_task_group() const override;
    std::string get_event_task_group() const override;
    std::string get_request_task_group() const override;
    std::string get_state_task_group() const;

    virtual bool on_activate() = 0;
    virtual bool on_startup() = 0;
    virtual void on_shutdown() = 0;
    virtual bool on_sleep() = 0;
    virtual bool on_wakeup() = 0;

    virtual bool on_manual_start_listening()
    {
        return true;
    }
    virtual bool on_manual_stop_listening()
    {
        return true;
    }
    virtual bool on_suspend()
    {
        return true;
    }
    virtual bool on_resume()
    {
        return true;
    }
    virtual bool on_interrupt_speaking()
    {
        return false;
    }

    virtual bool on_encoder_data_ready(const uint8_t *data, size_t data_size)
    {
        (void)data;
        (void)data_size;
        return true;
    }

    virtual bool set_info(const boost::json::object &info)
    {
        (void)info;
        return true;
    }

    virtual bool reset_data()
    {
        return true;
    }

    ChatMode get_chat_mode() const;

    void reset_interrupted_speaking();

    bool set_speaking(bool speaking);
    bool is_speaking() const
    {
        return is_speaking_;
    }
    bool set_listening(bool listening);
    bool is_listening() const
    {
        return is_listening_;
    }
    bool set_emote(const std::string emote);
    bool set_agent_speaking_text(const std::string text);
    bool set_user_speaking_text(const std::string text);

    void trigger_general_event(GeneralEvent event);

    bool feed_audio_decoder_data(const uint8_t *data, size_t data_size);

private:
    using service::ServiceBase::start;
    using service::ServiceBase::stop;

    using UnexpectedGeneralEventHappenedCallback = std::function<void(GeneralEvent event)>;
    using AFE_EventHappenedCallback = std::function<void(service::helper::Audio::AFE_Event event)>;

    struct Callbacks {
        UnexpectedGeneralEventHappenedCallback unexpected_general_event_happened;
        AFE_EventHappenedCallback afe_event_happened;
    };

    // Inherited from service::ServiceBase
    void on_stop() override;

    bool do_activate();
    bool do_start();
    void do_stop();
    bool do_sleep();
    bool do_wakeup();
    bool do_general_action(GeneralAction action, bool is_force = false);

    bool do_suspend();
    bool do_resume();
    bool do_interrupt_speaking();
    bool do_manual_start_listening();
    bool do_manual_stop_listening();

    bool start_audio_decoder();
    void stop_audio_decoder();
    bool is_decoder_started() const
    {
        return is_decoder_started_;
    }

    bool start_audio_encoder();
    void stop_audio_encoder();
    bool is_encoder_started() const
    {
        return is_encoder_started_;
    }

    bool is_suspended() const
    {
        return is_suspended_;
    }

    bool is_interrupted_speaking()
    {
        return is_interrupted_speaking_;
    }

    bool is_manual_listening()
    {
        return is_manual_listening_;
    }

    bool is_listening_disabled()
    {
        return (is_general_event_ready(GeneralEvent::Stopped) || is_general_action_running(GeneralAction::Stop) ||
                is_general_event_ready(GeneralEvent::Slept) || is_general_action_running(GeneralAction::Sleep) ||
                is_suspended() ||
                ((get_chat_mode() == ChatMode::Manual) && !is_manual_listening()));
    }

    bool is_speaking_disabled()
    {
        return (is_general_event_ready(GeneralEvent::Stopped) || is_general_action_running(GeneralAction::Stop) ||
                is_general_event_ready(GeneralEvent::Slept) || is_general_action_running(GeneralAction::Sleep) ||
                is_suspended() ||
                is_interrupted_speaking() ||
                ((get_chat_mode() == ChatMode::Manual) && is_manual_listening()));
    }

    bool is_general_action_running(GeneralAction action);
    bool is_general_event_ready(GeneralEvent event);
    bool is_general_event_unexpected(GeneralEvent event);
    GeneralAction get_running_general_action();

    void update_general_action_state_bit(GeneralAction action, bool enable);
    void update_general_event_state_bit(GeneralEvent event, bool enable);
    void reset_general_state_flag_bits();
    void clear_general_state_flag_bits();
    void update_general_state_error(bool enable);
    bool is_general_state_error()
    {
        return state_flags_.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralStateFlagBit::Error));
    }

    static GeneralEvent get_general_action_target_event(GeneralAction action);
    static GeneralAction get_general_action_from_target_event(GeneralEvent event);
    static GeneralStateFlagBit get_general_action_state_flag_bit(GeneralAction action);
    static std::pair<GeneralStateFlagBit, bool> get_general_event_state_flag_bit(GeneralEvent event);

    static void register_callbacks(const Callbacks &callbacks)
    {
        callbacks_ = callbacks;
    }

    AgentAttributes attributes_;
    AudioConfig audio_config_;

    bool is_suspended_ = false;
    bool is_speaking_ = false;
    bool is_listening_ = false;
    bool is_interrupted_speaking_ = false;
    bool is_encoder_started_ = false;
    bool is_decoder_started_ = false;
    bool is_unexpected_event_processing_ = false;
    bool is_manual_listening_ = false;

    GeneralStateFlags state_flags_;

    service::EventRegistry::SignalConnection afe_event_happened_connection_;
    service::EventRegistry::SignalConnection encoder_data_ready_connection_;

    inline static Callbacks callbacks_{};
};

} // namespace esp_brookesia::agent
