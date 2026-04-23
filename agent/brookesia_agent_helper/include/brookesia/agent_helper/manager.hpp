/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_helper/base.hpp"
#include "brookesia/service_helper/audio.hpp"

namespace esp_brookesia::agent::helper {

/**
 * @brief Helper schema definitions for the agent-manager service.
 */
class Manager: public service::helper::Base<Manager> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Timeout budget for the major lifecycle operations of an agent.
     */
    struct AgentOperationTimeout {
        uint32_t activate = 1000; ///< Timeout for activation.
        uint32_t start = 1000; ///< Timeout for startup.
        uint32_t sleep = 1000; ///< Timeout for sleep.
        uint32_t wake_up = 1000; ///< Timeout for wake-up.
        uint32_t stop = 1000; ///< Timeout for shutdown.
    };

    /**
     * @brief Optional per-agent functions surfaced by the manager.
     */
    enum class AgentGeneralFunction : uint8_t {
        InterruptSpeaking,
        Max,
    };

    /**
     * @brief Optional per-agent events surfaced by the manager.
     */
    enum class AgentGeneralEvent : uint8_t {
        SpeakingStatusChanged,
        ListeningStatusChanged,
        AgentSpeakingTextGot,
        UserSpeakingTextGot,
        EmoteGot,
        Max,
    };

    /**
     * @brief Conversation mode used by the active agent.
     */
    enum class ChatMode : uint8_t {
        RealTime,   ///< Real-time conversation mode. Agent can listen while speaking.
        Manual,     ///< Manual conversation mode. User can start and stop listening manually.
        HalfDuplex, ///< Half-duplex conversation mode. Agent can only speak or only listen.
        Max,
    };

    /**
     * @brief Public metadata describing one agent implementation.
     */
    struct AgentAttributes {
        /**
         * @brief Get the agent name stored in the attributes.
         *
         * @return std::string Agent name.
         */
        std::string get_name() const
        {
            return name;
        }

        /**
         * @brief Check whether an optional general function is supported.
         *
         * @param[in] function Function to check.
         * @return true if the function is listed in `support_general_functions`.
         */
        bool is_general_functions_supported(AgentGeneralFunction function) const
        {
            return std::find(support_general_functions.begin(), support_general_functions.end(), function) !=
                   support_general_functions.end();
        }

        /**
         * @brief Check whether an optional general event is supported.
         *
         * @param[in] event Event to check.
         * @return true if the event is listed in `support_general_events`.
         */
        bool is_general_events_supported(AgentGeneralEvent event) const
        {
            return std::find(support_general_events.begin(), support_general_events.end(), event) !=
                   support_general_events.end();
        }

        std::string name; ///< Agent name exposed to the manager.
        AgentOperationTimeout operation_timeout{}; ///< Timeout configuration for lifecycle actions.
        std::vector<AgentGeneralFunction> support_general_functions{}; ///< Optional functions supported by the agent.
        std::vector<AgentGeneralEvent> support_general_events{}; ///< Optional events emitted by the agent.
        bool require_time_sync = false; ///< Whether activation requires SNTP synchronization first.
    };

    /**
     * @brief High-level actions accepted by the agent manager.
     */
    enum class GeneralAction : uint8_t {
        TimeSync,
        Activate,
        Start,
        Sleep,
        WakeUp,
        Stop,
        Max,
    };

    /**
     * @brief High-level completion events emitted by managed agents.
     */
    enum class GeneralEvent : uint8_t {
        TimeSynced,
        Activated,
        Started,
        Slept,
        Awake,
        Stopped,
        Max,
    };

    /**
     * @brief State-machine states used by the agent manager.
     */
    enum class GeneralState : uint8_t {
        TimeSyncing,
        Ready,
        Activating,
        Activated,
        Starting,
        Started,
        Sleeping,
        Slept,
        WakingUp,
        Stopping,
        Max
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionId : uint8_t {
        GetAgentNames,
        GetAgentAttributes,
        SetAgentInfo,
        SetTargetAgent,
        GetTargetAgent,
        GetActiveAgent,
        TriggerGeneralAction,
        GetGeneralState,
        InterruptSpeaking,
        Suspend,
        Resume,
        GetSuspendStatus,
        SetChatMode,
        GetChatMode,
        ManualStartListening,
        ManualStopListening,
        GetSpeakingStatus,
        GetListeningStatus,
        ResetData,
        Max,
    };

    enum class EventId : uint8_t {
        GeneralActionTriggered,
        GeneralEventHappened,
        SuspendStatusChanged,
        SpeakingStatusChanged,
        ListeningStatusChanged,
        AgentSpeakingTextGot,
        UserSpeakingTextGot,
        EmoteGot,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionSetAgentInfoParam : uint8_t {
        Name,
        Info,
    };

    enum class FunctionSetChatModeParam : uint8_t {
        Mode,
    };

    enum class FunctionSetTargetAgentParam : uint8_t {
        Name,
    };

    enum class FunctionGetAgentAttributesParam : uint8_t {
        Name,
    };

    enum class FunctionTriggerGeneralActionParam : uint8_t {
        Action,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class EventGeneralActionTriggeredParam : uint8_t {
        Action,
    };

    enum class EventGeneralEventHappenedParam : uint8_t {
        Event,
        IsUnexpected,
    };

    enum class EventSuspendStatusChangedParam : uint8_t {
        IsSuspended,
    };

    enum class EventAgentSpeakingTextGotParam : uint8_t {
        Text,
    };

    enum class EventUserSpeakingTextGotParam : uint8_t {
        Text,
    };

    enum class EventEmoteGotParam : uint8_t {
        Emote,
    };

    enum class EventSpeakingStatusChangedParam : uint8_t {
        IsSpeaking,
    };

    enum class EventListeningStatusChangedParam : uint8_t {
        IsListening,
    };

private:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function schemas /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static service::FunctionSchema function_schema_set_agent_info()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetAgentInfo),
            .description = "Set agent info.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetAgentInfoParam::Name),
                    .description = "Agent name.",
                    .type = service::FunctionValueType::String
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetAgentInfoParam::Info),
                    .description = (boost::format("Agent info as a JSON object. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE((std::map<std::string, std::string>({
                        {"model", "model_name"},
                        {"api_key", "api_key_value"}
                    })))).str(),
                    .type = service::FunctionValueType::Object
                }
            },
            .require_scheduler = false
        };
    }

    static service::FunctionSchema function_schema_set_chat_mode()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetChatMode),
            .description = "Set chat mode.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetChatModeParam::Mode),
                    .description = (boost::format("Chat mode. Allowed values: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<ChatMode>({
                        ChatMode::Manual, ChatMode::RealTime
                    }))).str(),
                    .type = service::FunctionValueType::String
                }
            },
        };
    }

    static service::FunctionSchema function_schema_set_target_agent()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetTargetAgent),
            .description = "Set target agent. The agent will be activated by triggering `Activate` action.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetTargetAgentParam::Name),
                    .description = "Agent name to set as target.",
                    .type = service::FunctionValueType::String,
                }
            }
        };
    }

    static service::FunctionSchema function_schema_get_target_agent()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetTargetAgent),
            .description = "Get target agent name. Return type: string. Example: \"AgentCoze\"",
        };
    }

    static service::FunctionSchema function_schema_get_agent_names()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetAgentNames),
            .description = "Get agent names. Return type: JSON array<string>. Example: [\"Agent1\", \"Agent2\"]",
        };
    }

    static service::FunctionSchema function_schema_get_agent_attributes()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetAgentAttributes),
            .description = (boost::format("Get agent attributes. Return type: JSON array<object>. Example: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<AgentAttributes>({
                AgentAttributes{"Agent", {}},
            }))).str(),
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionGetAgentAttributesParam::Name),
                    .description = "Agent name (optional). Returns all agents when omitted.",
                    .type = service::FunctionValueType::String,
                    .default_value = std::optional<service::FunctionValue>(std::string(""))
                }
            },
            .require_scheduler = false
        };
    }

    static service::FunctionSchema function_schema_get_chat_mode()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetChatMode),
            .description = (boost::format("Get chat mode. Return type: string. Allowed values: %1%. "
                                          "Example: \"Manual\"")
            % BROOKESIA_DESCRIBE_TO_STR(std::vector<ChatMode>({
                ChatMode::Manual, ChatMode::RealTime
            }))).str(),
        };
    }

    static service::FunctionSchema function_schema_get_active_agent()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetActiveAgent),
            .description = "Get active agent name. Return type: string. Example: \"AgentCoze\"",
        };
    }

    static service::FunctionSchema function_schema_trigger_general_action()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerGeneralAction),
            .description = "Trigger a general action.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionTriggerGeneralActionParam::Action),
                    .description = (boost::format("General action. Allowed values: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<GeneralAction>({
                        GeneralAction::TimeSync, GeneralAction::Activate, GeneralAction::Start, GeneralAction::Stop,
                        GeneralAction::Sleep, GeneralAction::WakeUp
                    }))).str(),
                    .type = service::FunctionValueType::String
                }
            }
        };
    }

    static service::FunctionSchema function_schema_suspend()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::Suspend),
            .description = "Suspend the agent.",
        };
    }

    static service::FunctionSchema function_schema_resume()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::Resume),
            .description = "Resume the agent.",
        };
    }

    static service::FunctionSchema function_schema_trigger_interrupt_speaking()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::InterruptSpeaking),
            .description = "Interrupt speaking; the agent stops and keeps listening.",
        };
    }

    static service::FunctionSchema function_schema_manual_start_listening()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::ManualStartListening),
            .description = "Manually start listening. Only in `Manual` mode.",
        };
    }

    static service::FunctionSchema function_schema_manual_stop_listening()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::ManualStopListening),
            .description = "Manually stop listening. Only in `Manual` mode.",
        };
    }

    static service::FunctionSchema function_schema_get_general_state()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetGeneralState),
            .description = (boost::format("Get general state. Return type: string. Allowed values: %1%. "
                                          "Example: \"Started\"")
            % BROOKESIA_DESCRIBE_TO_STR(std::vector<GeneralState>({
                GeneralState::TimeSyncing, GeneralState::Ready, GeneralState::Activating, GeneralState::Activated,
                GeneralState::Starting, GeneralState::Stopping, GeneralState::Started, GeneralState::Sleeping,
                GeneralState::WakingUp, GeneralState::Slept
            }))).str(),
        };
    }

    static service::FunctionSchema function_schema_get_suspend_status()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetSuspendStatus),
            .description = "Get suspend status. Return type: boolean. Example: false",
        };
    }

    static service::FunctionSchema function_schema_reset_data()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::ResetData),
            .description = "Reset manager and agent data.",
        };
    }

    static service::FunctionSchema function_schema_get_speaking_status()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetSpeakingStatus),
            .description = "Get speaking status. Return type: boolean. Example: true",
        };
    }

    static service::FunctionSchema function_schema_get_listening_status()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetListeningStatus),
            .description = "Get listening status. Return type: boolean. Example: true",
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static service::EventSchema event_schema_general_action_triggered()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::GeneralActionTriggered),
            .description = "Emitted when a general action is triggered.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventGeneralActionTriggeredParam::Action),
                    .description = (boost::format("General action. Allowed values: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<GeneralAction>({
                        GeneralAction::TimeSync, GeneralAction::Activate, GeneralAction::Start, GeneralAction::Stop,
                        GeneralAction::Sleep, GeneralAction::WakeUp
                    }))).str(),
                    .type = service::EventItemType::String
                }
            }
        };
    }

    static service::EventSchema event_schema_general_event_happened()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::GeneralEventHappened),
            .description = "Emitted when a general event occurs.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventGeneralEventHappenedParam::Event),
                    .description = (boost::format("General event. Allowed values: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<GeneralEvent>({
                        GeneralEvent::TimeSynced, GeneralEvent::Activated, GeneralEvent::Started, GeneralEvent::Stopped,
                        GeneralEvent::Slept, GeneralEvent::Awake
                    }))).str(),
                    .type = service::EventItemType::String
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventGeneralEventHappenedParam::IsUnexpected),
                    .description = "Whether the event was unexpected.",
                    .type = service::EventItemType::Boolean
                }
            }
        };
    }

    static service::EventSchema event_schema_suspend_status_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::SuspendStatusChanged),
            .description = "Emitted when suspend status changes.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventSuspendStatusChangedParam::IsSuspended),
                    .description = "Whether the agent is suspended.",
                    .type = service::EventItemType::Boolean
                }
            }
        };
    }

    static service::EventSchema event_schema_speaking_status_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::SpeakingStatusChanged),
            .description = "Emitted when speaking status changes.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventSpeakingStatusChangedParam::IsSpeaking),
                    .description = "Whether the agent is speaking.",
                    .type = service::EventItemType::Boolean
                }
            }
        };
    }

    static service::EventSchema event_schema_listening_status_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::ListeningStatusChanged),
            .description = "Emitted when listening status changes.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventListeningStatusChangedParam::IsListening),
                    .description = "Whether the agent is listening.",
                    .type = service::EventItemType::Boolean
                }
            }
        };
    }

    static service::EventSchema event_schema_agent_speaking_text_got()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::AgentSpeakingTextGot),
            .description = "Emitted when the agent speaks text.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventAgentSpeakingTextGotParam::Text),
                    .description = "Spoken text.",
                    .type = service::EventItemType::String
                }
            }
        };
    }

    static service::EventSchema event_schema_user_speaking_text_got()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::UserSpeakingTextGot),
            .description = "Emitted when the user speaks text.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventUserSpeakingTextGotParam::Text),
                    .description = "User text.",
                    .type = service::EventItemType::String
                }
            }
        };
    }

    static service::EventSchema event_schema_emote_got()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::EmoteGot),
            .description = "Emitted when the agent shows an emote.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventEmoteGotParam::Emote),
                    .description = "Emote name.",
                    .type = service::EventItemType::String
                }
            }
        };
    }

public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the functions required by the Base class /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Name of the agent-manager service.
     *
     * @return std::string_view Stable service name.
     */
    static constexpr std::string_view get_name()
    {
        return "AgentManager";
    }

    static std::span<const service::FunctionSchema> get_function_schemas()
    {
        static const std::array<service::FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)>
        FUNCTION_SCHEMAS = {{
                function_schema_set_agent_info(),
                function_schema_set_chat_mode(),
                function_schema_set_target_agent(),
                function_schema_get_target_agent(),
                function_schema_get_agent_names(),
                function_schema_get_agent_attributes(),
                function_schema_get_chat_mode(),
                function_schema_get_active_agent(),
                function_schema_trigger_general_action(),
                function_schema_suspend(),
                function_schema_resume(),
                function_schema_trigger_interrupt_speaking(),
                function_schema_manual_start_listening(),
                function_schema_manual_stop_listening(),
                function_schema_get_general_state(),
                function_schema_get_suspend_status(),
                function_schema_get_speaking_status(),
                function_schema_get_listening_status(),
                function_schema_reset_data(),
            }
        };
        return std::span<const service::FunctionSchema>(FUNCTION_SCHEMAS);
    }

    static std::span<const service::EventSchema> get_event_schemas()
    {
        static const std::array<service::EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EventId::Max)> EVENT_SCHEMAS = {{
                event_schema_general_action_triggered(),
                event_schema_general_event_happened(),
                event_schema_suspend_status_changed(),
                event_schema_speaking_status_changed(),
                event_schema_listening_status_changed(),
                event_schema_agent_speaking_text_got(),
                event_schema_user_speaking_text_got(),
                event_schema_emote_got(),
            }
        };
        return std::span<const service::EventSchema>(EVENT_SCHEMAS);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the describe macros //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief  Agent related
 */
BROOKESIA_DESCRIBE_STRUCT(Manager::AgentOperationTimeout, (), (activate, start, sleep, wake_up, stop));
BROOKESIA_DESCRIBE_ENUM(Manager::AgentGeneralFunction, InterruptSpeaking, Max);
BROOKESIA_DESCRIBE_ENUM(Manager::ChatMode, RealTime, Manual, HalfDuplex, Max);
BROOKESIA_DESCRIBE_ENUM(
    Manager::AgentGeneralEvent, SpeakingStatusChanged, ListeningStatusChanged, AgentSpeakingTextGot,
    UserSpeakingTextGot, EmoteGot, Max
);
BROOKESIA_DESCRIBE_STRUCT(
    Manager::AgentAttributes, (), (
        name, operation_timeout, support_general_functions, support_general_events, require_time_sync
    )
);
BROOKESIA_DESCRIBE_ENUM(Manager::GeneralAction, TimeSync, Activate, Start, Sleep, WakeUp, Stop, Max);
BROOKESIA_DESCRIBE_ENUM(Manager::GeneralEvent, TimeSynced, Activated, Started, Slept, Awake, Stopped, Max);
BROOKESIA_DESCRIBE_ENUM(
    Manager::GeneralState, TimeSyncing, Ready, Activating, Activated, Starting, Started, Sleeping, Slept, WakingUp,
    Stopping, Max
);

/**
 * @brief  Function related
 */
BROOKESIA_DESCRIBE_ENUM(
    Manager::FunctionId,
    GetAgentNames, GetAgentAttributes, SetAgentInfo, SetTargetAgent, GetTargetAgent, GetActiveAgent,
    TriggerGeneralAction, GetGeneralState, InterruptSpeaking, Suspend, Resume, GetSuspendStatus,
    SetChatMode, GetChatMode, ManualStartListening, ManualStopListening, GetSpeakingStatus, GetListeningStatus,
    ResetData,
    Max
);
BROOKESIA_DESCRIBE_ENUM(Manager::FunctionSetAgentInfoParam, Name, Info);
BROOKESIA_DESCRIBE_ENUM(Manager::FunctionSetChatModeParam, Mode);
BROOKESIA_DESCRIBE_ENUM(Manager::FunctionSetTargetAgentParam, Name);
BROOKESIA_DESCRIBE_ENUM(Manager::FunctionGetAgentAttributesParam, Name);
BROOKESIA_DESCRIBE_ENUM(Manager::FunctionTriggerGeneralActionParam, Action);

/**
 * @brief  Event related
 */
BROOKESIA_DESCRIBE_ENUM(
    Manager::EventId, GeneralActionTriggered, GeneralEventHappened, SuspendStatusChanged,
    SpeakingStatusChanged, ListeningStatusChanged, AgentSpeakingTextGot, UserSpeakingTextGot, EmoteGot, Max
);
BROOKESIA_DESCRIBE_ENUM(Manager::EventGeneralActionTriggeredParam, Action);
BROOKESIA_DESCRIBE_ENUM(Manager::EventGeneralEventHappenedParam, Event, IsUnexpected);
BROOKESIA_DESCRIBE_ENUM(Manager::EventSuspendStatusChangedParam, IsSuspended);
BROOKESIA_DESCRIBE_ENUM(Manager::EventAgentSpeakingTextGotParam, Text);
BROOKESIA_DESCRIBE_ENUM(Manager::EventUserSpeakingTextGotParam, Text);
BROOKESIA_DESCRIBE_ENUM(Manager::EventEmoteGotParam, Emote);
BROOKESIA_DESCRIBE_ENUM(Manager::EventSpeakingStatusChangedParam, IsSpeaking);
BROOKESIA_DESCRIBE_ENUM(Manager::EventListeningStatusChangedParam, IsListening);

} // namespace esp_brookesia::agent::helper
