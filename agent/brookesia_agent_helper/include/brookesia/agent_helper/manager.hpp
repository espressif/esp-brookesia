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

class Manager: public service::helper::Base<Manager> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    struct AgentOperationTimeout {
        uint32_t activate = 1000;
        uint32_t start = 1000;
        uint32_t sleep = 1000;
        uint32_t wake_up = 1000;
        uint32_t stop = 1000;
    };

    enum class AgentGeneralFunction : uint8_t {
        InterruptSpeaking,
        Max,
    };

    enum class AgentGeneralEvent : uint8_t {
        SpeakingStatusChanged,
        ListeningStatusChanged,
        AgentSpeakingTextGot,
        UserSpeakingTextGot,
        EmoteGot,
        Max,
    };

    enum class ChatMode : uint8_t {
        RealTime,
        Manual,
        Max,
    };

    struct AgentAttributes {
        std::string get_name() const
        {
            return name;
        }

        bool is_general_functions_supported(AgentGeneralFunction function) const
        {
            return std::find(support_general_functions.begin(), support_general_functions.end(), function) !=
                   support_general_functions.end();
        }

        bool is_general_events_supported(AgentGeneralEvent event) const
        {
            return std::find(support_general_events.begin(), support_general_events.end(), event) !=
                   support_general_events.end();
        }

        std::string name;
        AgentOperationTimeout operation_timeout{};
        std::vector<AgentGeneralFunction> support_general_functions{};
        std::vector<AgentGeneralEvent> support_general_events{};
        bool require_time_sync = false;
    };

    enum class GeneralAction : uint8_t {
        TimeSync,
        Activate,
        Start,
        Sleep,
        WakeUp,
        Stop,
        Max,
    };

    enum class GeneralEvent : uint8_t {
        TimeSynced,
        Activated,
        Started,
        Slept,
        Awake,
        Stopped,
        Max,
    };

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
        SetAgentInfo,
        SetChatMode,
        ActivateAgent,
        GetAgentAttributes,
        GetChatMode,
        GetActiveAgent,
        TriggerGeneralAction,
        Suspend,
        Resume,
        InterruptSpeaking,
        ManualStartListening,
        ManualStopListening,
        GetGeneralState,
        GetSuspendStatus,
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

    enum class FunctionActivateAgentParam : uint8_t {
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
            .description = "Set the info for an agent",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetAgentInfoParam::Name),
                    .description = "The name of the agent to set the info for",
                    .type = service::FunctionValueType::String
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetAgentInfoParam::Info),
                    .description = "The info for the agent.",
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
            .description = "Set the chat mode of the agent.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetChatModeParam::Mode),
                    .description = (boost::format("The chat mode to set, should be one of the following: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<ChatMode>({
                        ChatMode::Manual, ChatMode::RealTime
                    }))).str(),
                    .type = service::FunctionValueType::String
                }
            },
        };
    }

    static service::FunctionSchema function_schema_activate_agent()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::ActivateAgent),
            .description = "Activate an agent",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionActivateAgentParam::Name),
                    .description = "The name of the agent to activate. Optional. "
                    "If not provided, the last active agent will be activated.",
                    .type = service::FunctionValueType::String,
                    .default_value = std::string("")
                }
            }
        };
    }

    static service::FunctionSchema function_schema_get_attributes()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetAgentAttributes),
            .description = (boost::format("Get the attributes of one or more agents, "
                                          "the result is a JSON array of agent attributes. Example: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<AgentAttributes>({
                AgentAttributes{"Agent", {}},
            }))).str(),
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionGetAgentAttributesParam::Name),
                    .description = "The name of the agent to get the attributes for. Optional. If not provided, "
                    "all agents will be returned.",
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
            .description = (boost::format("Get the chat mode of the agent, the result is a string of the chat mode, "
                                          "should be one of the following: %1%")
            % BROOKESIA_DESCRIBE_TO_STR(std::vector<ChatMode>({
                ChatMode::Manual, ChatMode::RealTime
            }))).str(),
        };
    }

    static service::FunctionSchema function_schema_get_active_agent()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetActiveAgent),
            .description = "Get the information of the active agent",
        };
    }

    static service::FunctionSchema function_schema_trigger_general_action()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerGeneralAction),
            .description = "Trigger a general action",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionTriggerGeneralActionParam::Action),
                    .description = (boost::format("The general action to trigger, should be one of the following: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<GeneralAction>({
                        GeneralAction::Activate, GeneralAction::Start, GeneralAction::Stop, GeneralAction::Sleep,
                        GeneralAction::WakeUp
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
            .description = "Trigger suspend the agent",
        };
    }

    static service::FunctionSchema function_schema_resume()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::Resume),
            .description = "Trigger resume the agent",
        };
    }

    static service::FunctionSchema function_schema_trigger_interrupt_speaking()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::InterruptSpeaking),
            .description = "Interrupt the agent speaking, the agent will stop speaking and keep listening",
        };
    }

    static service::FunctionSchema function_schema_manual_start_listening()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::ManualStartListening),
            .description = "Manually start listening. Only available when the chat mode is `Manual`.",
        };
    }

    static service::FunctionSchema function_schema_manual_stop_listening()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::ManualStopListening),
            .description = "Manually stop listening. Only available when the chat mode is `Manual`.",
        };
    }

    static service::FunctionSchema function_schema_get_general_state()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetGeneralState),
            .description = (boost::format("Get the general state of the agent, should be one of the following: %1%")
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
            .description = "Get the suspend status of the agent, true if the agent is suspended, false if the agent is "
            "not suspended.",
        };
    }

    static service::FunctionSchema function_schema_reset_data()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::ResetData),
            .description = "Reset the data of the manager and all agents.",
        };
    }

    static service::FunctionSchema function_schema_get_speaking_status()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetSpeakingStatus),
            .description = "Get the speaking status of the agent, true if the agent is speaking, false if the agent "
            "is not speaking.",
        };
    }

    static service::FunctionSchema function_schema_get_listening_status()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetListeningStatus),
            .description = "Get the listening status of the agent, true if the agent is listening, false if the agent "
            "is not listening.",
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static service::EventSchema event_schema_general_action_triggered()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::GeneralActionTriggered),
            .description = "Triggered when a general action is triggered",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventGeneralActionTriggeredParam::Action),
                    .description = (boost::format("The general action that was triggered, "
                                                  "should be one of the following: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<GeneralAction>({
                        GeneralAction::Activate, GeneralAction::Start, GeneralAction::Stop, GeneralAction::Sleep,
                        GeneralAction::WakeUp
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
            .description = "Triggered when a general event is happened",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventGeneralEventHappenedParam::Event),
                    .description = (boost::format("The general event that was happened, "
                                                  "should be one of the following: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<GeneralEvent>({
                        GeneralEvent::TimeSynced, GeneralEvent::Activated, GeneralEvent::Started, GeneralEvent::Stopped,
                        GeneralEvent::Slept, GeneralEvent::Awake
                    }))).str(),
                    .type = service::EventItemType::String
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventGeneralEventHappenedParam::IsUnexpected),
                    .description = "Indicates whether the event occurred unexpectedly. "
                    "False: triggered by user action or intentional agent behavior. "
                    "True: occurred due to an unexpected agent state.",
                    .type = service::EventItemType::Boolean
                }
            }
        };
    }

    static service::EventSchema event_schema_suspend_status_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::SuspendStatusChanged),
            .description = "Triggered when the suspend status of the agent changes",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventSuspendStatusChangedParam::IsSuspended),
                    .description = "The suspend status of the agent, true if the agent is suspended, false if the agent "
                    "is resumed.",
                    .type = service::EventItemType::Boolean
                }
            }
        };
    }

    static service::EventSchema event_schema_speaking_status_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::SpeakingStatusChanged),
            .description = "Triggered when the speaking status of the agent changes",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventSpeakingStatusChangedParam::IsSpeaking),
                    .description = "The speaking status of the agent, true if the agent is speaking, false if the agent "
                    "is not speaking.",
                    .type = service::EventItemType::Boolean
                }
            }
        };
    }

    static service::EventSchema event_schema_listening_status_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::ListeningStatusChanged),
            .description = "Triggered when the listening status of the agent changes",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventListeningStatusChangedParam::IsListening),
                    .description = "The listening status of the agent, true if the agent is listening, false if the agent "
                    "is not listening.",
                    .type = service::EventItemType::Boolean
                }
            }
        };
    }

    static service::EventSchema event_schema_agent_speaking_text_got()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::AgentSpeakingTextGot),
            .description = "Triggered when the agent speaks a text",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventAgentSpeakingTextGotParam::Text),
                    .description = "The text that the agent is speaking",
                    .type = service::EventItemType::String
                }
            }
        };
    }

    static service::EventSchema event_schema_user_speaking_text_got()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::UserSpeakingTextGot),
            .description = "Triggered when the user speaks a text",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventUserSpeakingTextGotParam::Text),
                    .description = "The text that the user is speaking",
                    .type = service::EventItemType::String
                }
            }
        };
    }

    static service::EventSchema event_schema_emote_got()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::EmoteGot),
            .description = "Triggered when the agent gets an emote",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventEmoteGotParam::Emote),
                    .description = "The emote that the agent is showing",
                    .type = service::EventItemType::String
                }
            }
        };
    }

public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the functions required by the Base class /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
                function_schema_activate_agent(),
                function_schema_get_attributes(),
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
BROOKESIA_DESCRIBE_ENUM(Manager::ChatMode, Manual, RealTime, Max);
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
    Manager::FunctionId, SetAgentInfo, SetChatMode, ActivateAgent, GetAgentAttributes, GetChatMode, GetActiveAgent,
    TriggerGeneralAction, Suspend, Resume, InterruptSpeaking, ManualStartListening, ManualStopListening,
    GetGeneralState, GetSuspendStatus, GetSpeakingStatus, GetListeningStatus, ResetData, Max
);
BROOKESIA_DESCRIBE_ENUM(Manager::FunctionSetAgentInfoParam, Name, Info);
BROOKESIA_DESCRIBE_ENUM(Manager::FunctionSetChatModeParam, Mode);
BROOKESIA_DESCRIBE_ENUM(Manager::FunctionActivateAgentParam, Name);
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
