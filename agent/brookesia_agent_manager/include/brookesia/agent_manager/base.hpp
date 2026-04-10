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
#include "brookesia/service_helper/audio.hpp"
#include "brookesia/agent_helper/manager.hpp"
#include "brookesia/agent_manager/macro_configs.h"

namespace esp_brookesia::agent {

/**
 * @brief Extra service dependencies required by an agent implementation.
 */
struct ServiceConfig {
    std::vector<std::string> dependencies{}; ///< Additional service names that must be bound before the agent starts.
};

/**
 * @brief Audio encoder and decoder configuration used by an agent.
 */
struct AudioConfig {
    service::helper::Audio::EncoderDynamicConfig encoder; ///< Encoder configuration for uplink audio.
    service::helper::Audio::DecoderDynamicConfig decoder; ///< Decoder configuration for downlink audio.
};

/**
 * @brief Re-exported general agent actions supported by the manager helper.
 */
using GeneralAction = helper::Manager::GeneralAction;
/**
 * @brief Re-exported general agent events emitted by the manager helper.
 */
using GeneralEvent = helper::Manager::GeneralEvent;
/**
 * @brief Re-exported metadata that describes an agent implementation.
 */
using AgentAttributes = helper::Manager::AgentAttributes;
/**
 * @brief Re-exported chat-mode enumeration.
 */
using ChatMode = helper::Manager::ChatMode;

/**
 * @brief Bit positions stored in `GeneralStateFlags`.
 */
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

/**
 * @brief Common base class for all agent implementations.
 *
 * The class extends `service::ServiceBase` with agent lifecycle hooks,
 * audio pipeline coordination, and shared state tracked by `Manager` and
 * `StateMachine`.
 */
class Base: public service::ServiceBase {
public:
    friend class Manager;
    friend class StateMachine;
    friend class GeneralStateClass;

    Base(Base &&) = delete;
    Base(const Base &) = delete;
    Base &operator=(Base &&) = delete;
    Base &operator=(const Base &) = delete;

    /**
     * @brief Get immutable metadata describing the agent.
     *
     * @return const AgentAttributes& Agent attributes passed at construction time.
     */
    const AgentAttributes &get_attributes() const
    {
        return attributes_;
    }

    /**
     * @brief Get the audio configuration used by the agent.
     *
     * @return const AudioConfig& Current encoder and decoder configuration.
     */
    const AudioConfig &get_audio_config() const
    {
        return audio_config_;
    }

protected:
    /**
     * @brief Construct an agent base with metadata, audio settings, and service dependencies.
     *
     * @param[in] attributes Public agent metadata.
     * @param[in] audio_config Audio configuration used by the agent.
     * @param[in] service_config Additional service dependencies beyond the audio service.
     */
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
    /**
     * @brief Virtual destructor.
     */
    virtual ~Base() = default;

    /**
     * @brief Get the task group name used by agent state transitions.
     *
     * @return std::string Scheduler group name for state-machine work.
     */
    std::string get_state_task_group() const;

    /**
     * @brief Activate the agent before it can start normal interaction.
     *
     * @return true if activation succeeds.
     */
    virtual bool on_activate() = 0;
    /**
     * @brief Start the agent's main interaction loop.
     *
     * @return true if startup succeeds.
     */
    virtual bool on_startup() = 0;
    /**
     * @brief Stop the agent's main interaction loop.
     */
    virtual void on_shutdown() = 0;
    /**
     * @brief Transition the agent into sleep mode.
     *
     * @return true if the sleep transition succeeds.
     */
    virtual bool on_sleep() = 0;
    /**
     * @brief Wake the agent from sleep mode.
     *
     * @return true if the wake-up transition succeeds.
     */
    virtual bool on_wakeup() = 0;

    /**
     * @brief Optional hook for manual-listening start requests.
     *
     * @return true if the request is accepted.
     */
    virtual bool on_manual_start_listening()
    {
        return true;
    }
    /**
     * @brief Optional hook for manual-listening stop requests.
     *
     * @return true if the request is accepted.
     */
    virtual bool on_manual_stop_listening()
    {
        return true;
    }
    /**
     * @brief Optional hook invoked when the active agent is suspended.
     *
     * @return true if suspension succeeds.
     */
    virtual bool on_suspend()
    {
        return true;
    }
    /**
     * @brief Optional hook invoked when the agent resumes from suspension.
     *
     * @return true if resume succeeds.
     */
    virtual bool on_resume()
    {
        return true;
    }
    /**
     * @brief Optional hook invoked when speech playback should be interrupted.
     *
     * @return true if interruption is supported and succeeds.
     */
    virtual bool on_interrupt_speaking()
    {
        return false;
    }

    /**
     * @brief Optional hook invoked when encoded uplink audio becomes available.
     *
     * @param[in] data Encoded audio buffer.
     * @param[in] data_size Size of `data` in bytes.
     * @return true if the data was consumed successfully.
     */
    virtual bool on_encoder_data_ready(const uint8_t *data, size_t data_size)
    {
        (void)data;
        (void)data_size;
        return true;
    }

    /**
     * @brief Update agent-specific persistent configuration.
     *
     * @param[in] info JSON object with implementation-defined contents.
     * @return true if the information is accepted.
     */
    virtual bool set_info(const boost::json::object &info)
    {
        (void)info;
        return true;
    }

    /**
     * @brief Reset agent-specific persisted state.
     *
     * @return true if the reset succeeds.
     */
    virtual bool reset_data()
    {
        return true;
    }

    const std::vector<std::string> &get_wake_words() const
    {
        return wake_words_;
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
    void set_manual_listening(bool listening);
    bool is_manual_listening()
    {
        return is_manual_listening_;
    }
    void set_encoder_paused(bool paused);
    bool set_emote(const std::string emote);
    bool set_agent_speaking_text(const std::string text);
    bool set_user_speaking_text(const std::string text);

    void trigger_general_event(GeneralEvent event);

    bool feed_audio_decoder_data(const uint8_t *data, size_t data_size);

    bool is_general_action_running(GeneralAction action);

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
    bool on_start() override;
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

    std::vector<std::string> wake_words_;

    service::EventRegistry::SignalConnection afe_event_happened_connection_;
    service::EventRegistry::SignalConnection encoder_data_ready_connection_;

    inline static Callbacks callbacks_{};
};

} // namespace esp_brookesia::agent
