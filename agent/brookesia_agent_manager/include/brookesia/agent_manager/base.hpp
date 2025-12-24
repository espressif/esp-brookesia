/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <bitset>
#include <functional>
#include <string>
#include "boost/thread/shared_mutex.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_helper/agent/manager.hpp"
#include "brookesia/service_helper/audio.hpp"

namespace esp_brookesia::agent {

struct AudioConfig {
    size_t encoder_feed_data_size;
    service::helper::Audio::EncoderConfig encoder;
    service::helper::Audio::DecoderConfig decoder;
};

using GeneralAction = service::helper::AgentManager::GeneralAction;
using GeneralEvent = service::helper::AgentManager::GeneralEvent;
using AgentAttributes = service::helper::AgentManager::AgentAttributes;

enum class GeneralStateFlagBit {
    Starting,
    Stopping,
    Started,
    Sleeping,
    WakingUp,
    Slept,
    Max,
};
BROOKESIA_DESCRIBE_ENUM(
    GeneralStateFlagBit, Starting, Stopping, Started, Sleeping, WakingUp, Slept, Max
);

using GeneralStateFlags = std::bitset<BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralStateFlagBit::Max)>;

class Base {
public:
    friend class Manager;
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
    Base(const AgentAttributes &attributes, const AudioConfig &audio_config)
        : attributes_(attributes)
        , audio_config_(audio_config)
    {}
    virtual ~Base() = default;

    virtual bool on_init()
    {
        return true;
    }
    virtual void on_deinit()
    {}
    virtual bool on_activate()
    {
        return true;
    }
    virtual void on_deactivate()
    {}

    virtual bool on_start() = 0;
    virtual void on_stop() = 0;
    virtual bool on_sleep() = 0;
    virtual void on_wakeup() = 0;

    virtual bool on_suspend()
    {
        return true;
    }
    virtual void on_resume()
    {}
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

    /* Allow subclasses to provide the additional function schemas, event schemas, and function handlers */
    virtual std::vector<service::FunctionSchema> get_function_schemas()
    {
        return {};
    }
    virtual std::vector<service::EventSchema> get_event_schemas()
    {
        return {};
    }
    virtual service::ServiceBase::FunctionHandlerMap get_function_handlers()
    {
        return {};
    }

    void trigger_general_event(GeneralEvent event);
    bool feed_audio_decoder_data(const uint8_t *data, size_t data_size);

    bool publish_service_event(const std::string &event, service::EventItemMap &&items, bool use_dispatch = false);
    std::shared_ptr<lib_utils::TaskScheduler> get_service_scheduler();

    void reset_interrupted_speaking()
    {
        is_interrupted_speaking_ = false;
    }

private:
    using GeneralActionTriggeredCallback = std::function<void(GeneralAction action)>;
    using GeneralEventHappenedCallback = std::function<void(GeneralEvent event, bool is_unexpected_event)>;
    using SuspendStatusChangedCallback = std::function<void(bool is_suspended)>;

    struct Callbacks {
        GeneralActionTriggeredCallback general_action_triggered_callback = nullptr;
        GeneralEventHappenedCallback general_event_happened_callback = nullptr;
        SuspendStatusChangedCallback suspend_status_changed_callback = nullptr;
    };

    bool init();
    void deinit();
    bool activate();
    void deactivate();

    bool do_start();
    void do_stop();
    bool do_sleep();
    void do_wakeup();
    bool do_general_action(GeneralAction action, bool is_force = false);

    bool do_suspend();
    void do_resume();
    bool do_interrupt_speaking();

    bool start_audio_decoder();
    void stop_audio_decoder();
    bool start_audio_encoder();
    void stop_audio_encoder();

    bool is_initialized() const
    {
        return is_initialized_;
    }

    bool is_active() const
    {
        return is_active_;
    }

    bool is_suspended() const
    {
        return is_suspended_;
    }

    bool is_interrupted_speaking()
    {
        return is_interrupted_speaking_;
    }

    bool is_listening_disabled()
    {
        return (is_general_event_ready(GeneralEvent::Stopped) || is_general_action_running(GeneralAction::Stop) ||
                is_general_event_ready(GeneralEvent::Slept) || is_general_action_running(GeneralAction::Sleep) ||
                is_suspended());
    }

    bool is_speaking_disabled()
    {
        return (is_general_event_ready(GeneralEvent::Stopped) || is_general_action_running(GeneralAction::Stop) ||
                is_general_event_ready(GeneralEvent::Slept) || is_general_action_running(GeneralAction::Sleep) ||
                is_suspended() || is_interrupted_speaking());
    }

    bool is_general_action_running(GeneralAction action);
    bool is_general_event_ready(GeneralEvent event);
    bool is_general_event_unexpected(GeneralEvent event);
    GeneralAction get_running_general_action();
    GeneralEvent get_general_action_failed_event(GeneralAction action);

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

    bool is_initialized_ = false;
    bool is_active_ = false;
    bool is_suspended_ = false;
    bool is_interrupted_speaking_ = false;

    GeneralStateFlags state_flags_;

    service::EventRegistry::SignalConnection encoder_event_happened_connection_;
    service::EventRegistry::SignalConnection encoder_data_ready_connection_;

    inline static Callbacks callbacks_{nullptr, nullptr, nullptr};
};

} // namespace esp_brookesia::agent
