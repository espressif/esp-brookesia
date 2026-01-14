/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_helper/base.hpp"

namespace esp_brookesia::service::helper {

class AgentManager: public Base<AgentManager> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class GeneralAction {
        Start,
        Stop,
        Sleep,
        WakeUp,
        Max,
    };

    enum class GeneralEvent {
        Started,
        Stopped,
        Slept,
        Awake,
        Max,
    };

    enum class GeneralState {
        TimeSyncing,
        TimeSynced,
        Starting,
        Stopping,
        Started,
        Sleeping,
        WakingUp,
        Slept,
        Max
    };

    struct AgentAttributes {
        std::string name;
        std::array<uint32_t, BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralEvent::Max)> general_event_wait_timeout_ms;
        bool support_interrupt_speaking = false;
        bool support_function_calling = false;
        bool support_agent_speaking_text = false;
        bool support_user_speaking_text = false;
        bool support_emote = false;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionId {
        SetAgentInfo,
        ActivateAgent,
        DeactivateAgent,
        GetAgentAttributes,
        GetActiveAgent,
        TriggerGeneralAction,
        TriggerSuspend,
        TriggerResume,
        TriggerInterruptSpeaking,
        GetGeneralState,
        GetSuspendStatus,
        ResetData,
        Max,
    };

    enum class EventId {
        GeneralActionTriggered,
        GeneralEventHappened,
        SuspendStatusChanged,
        AgentSpeakingTextGot,
        UserSpeakingTextGot,
        EmoteGot,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionSetAgentInfoParam {
        Name,
        Info,
    };

    enum class FunctionActivateAgentParam {
        Name,
    };

    enum class FunctionGetAgentAttributesParam {
        Name,
    };

    enum class FunctionTriggerGeneralActionParam {
        Action,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class EventGeneralActionTriggeredParam {
        Action,
    };

    enum class EventGeneralEventHappenedParam {
        Event,
    };

    enum class EventSuspendStatusChangedParam {
        IsSuspended,
    };

    enum class EventAgentSpeakingTextGotParam {
        Text,
    };

    enum class EventUserSpeakingTextGotParam {
        Text,
    };

    enum class EventEmoteGotParam {
        Emote,
    };

private:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function schemas /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static FunctionSchema function_schema_set_agent_info()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetAgentInfo),
            .description = "Set the info for an agent",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetAgentInfoParam::Name),
                    .description = "The name of the agent to set the info for",
                    .type = FunctionValueType::String
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetAgentInfoParam::Info),
                    .description = "The info for the agent.",
                    .type = FunctionValueType::Object
                }
            },
            .require_async = false
        };
    }

    static FunctionSchema function_schema_activate_agent()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::ActivateAgent),
            .description = "Activate an agent",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionActivateAgentParam::Name),
                    .description = "The name of the agent to activate",
                    .type = FunctionValueType::String
                }
            }
        };
    }

    static FunctionSchema function_schema_deactivate_agent()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::DeactivateAgent),
            .description = "Deactivate the active agent",
        };
    }

    static FunctionSchema function_schema_get_agent_attributes()
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
                    .type = FunctionValueType::String,
                    .default_value = std::optional<FunctionValue>(std::string(""))
                }
            },
            .require_async = false
        };
    }

    static FunctionSchema function_schema_get_active_agent()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetActiveAgent),
            .description = "Get the information of the active agent",
        };
    }

    static FunctionSchema function_schema_trigger_general_action()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerGeneralAction),
            .description = "Trigger a general action",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionTriggerGeneralActionParam::Action),
                    .description = (boost::format("The general action to trigger, should be one of the following: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<GeneralAction>({
                        GeneralAction::Start, GeneralAction::Stop, GeneralAction::Sleep, GeneralAction::WakeUp
                    }))).str(),
                    .type = FunctionValueType::String
                }
            }
        };
    }

    static FunctionSchema function_schema_trigger_suspend()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerSuspend),
            .description = "Trigger suspend the agent",
        };
    }

    static FunctionSchema function_schema_trigger_resume()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerResume),
            .description = "Trigger resume the agent",
        };
    }

    static FunctionSchema function_schema_trigger_interrupt_speaking()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerInterruptSpeaking),
            .description = "Interrupt the agent speaking, the agent will stop speaking and keep listening",
        };
    }

    static FunctionSchema function_schema_get_general_state()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetGeneralState),
            .description = (boost::format("Get the general state of the agent, should be one of the following: %1%")
            % BROOKESIA_DESCRIBE_TO_STR(std::vector<GeneralState>({
                GeneralState::TimeSyncing, GeneralState::TimeSynced, GeneralState::Starting, GeneralState::Stopping,
                GeneralState::Started, GeneralState::Sleeping, GeneralState::WakingUp, GeneralState::Slept
            }))).str(),
        };
    }

    static FunctionSchema function_schema_get_suspend_status()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetSuspendStatus),
            .description = "Get the suspend status of the agent, true if the agent is suspended, false if the agent is "
            "not suspended.",
        };
    }

    static FunctionSchema function_schema_reset_data()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::ResetData),
            .description = "Reset the data of the manager and all agents.",
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static EventSchema event_schema_general_action_triggered()
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
                        GeneralAction::Start, GeneralAction::Stop, GeneralAction::Sleep, GeneralAction::WakeUp
                    }))).str(),
                    .type = EventItemType::String
                }
            }
        };
    }

    static EventSchema event_schema_general_event_happened()
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
                        GeneralEvent::Started, GeneralEvent::Stopped, GeneralEvent::Slept, GeneralEvent::Awake
                    }))).str(),
                    .type = EventItemType::String
                }
            }
        };
    }

    static EventSchema event_schema_suspend_status_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::SuspendStatusChanged),
            .description = "Triggered when the suspend status of the agent changes",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventSuspendStatusChangedParam::IsSuspended),
                    .description = "The suspend status of the agent, true if the agent is suspended, false if the agent "
                    "is resumed.",
                    .type = EventItemType::Boolean
                }
            }
        };
    }

    static EventSchema event_schema_agent_speaking_text_got()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::AgentSpeakingTextGot),
            .description = "Triggered when the agent speaks a text",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventAgentSpeakingTextGotParam::Text),
                    .description = "The text that the agent is speaking",
                    .type = EventItemType::String
                }
            }
        };
    }

    static EventSchema event_schema_user_speaking_text_got()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::UserSpeakingTextGot),
            .description = "Triggered when the user speaks a text",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventUserSpeakingTextGotParam::Text),
                    .description = "The text that the user is speaking",
                    .type = EventItemType::String
                }
            }
        };
    }

    static EventSchema event_schema_emote_got()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::EmoteGot),
            .description = "Triggered when the agent gets an emote",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventEmoteGotParam::Emote),
                    .description = "The emote that the agent is showing",
                    .type = EventItemType::String
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
        return "Agent";
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> FUNCTION_SCHEMAS = {{
                function_schema_set_agent_info(),
                function_schema_activate_agent(),
                function_schema_deactivate_agent(),
                function_schema_get_agent_attributes(),
                function_schema_get_active_agent(),
                function_schema_trigger_general_action(),
                function_schema_trigger_suspend(),
                function_schema_trigger_resume(),
                function_schema_trigger_interrupt_speaking(),
                function_schema_get_general_state(),
                function_schema_get_suspend_status(),
                function_schema_reset_data(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }

    static std::span<const EventSchema> get_event_schemas()
    {
        static const std::array<EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EventId::Max)> EVENT_SCHEMAS = {{
                event_schema_general_action_triggered(),
                event_schema_general_event_happened(),
                event_schema_suspend_status_changed(),
                event_schema_agent_speaking_text_got(),
                event_schema_user_speaking_text_got(),
                event_schema_emote_got(),
            }
        };
        return std::span<const EventSchema>(EVENT_SCHEMAS);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the describe macros //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROOKESIA_DESCRIBE_STRUCT(
    AgentManager::AgentAttributes, (), (
        name, general_event_wait_timeout_ms, support_interrupt_speaking, support_function_calling,
        support_agent_speaking_text, support_user_speaking_text, support_emote
    )
);
BROOKESIA_DESCRIBE_ENUM(AgentManager::GeneralAction, Start, Stop, Sleep, WakeUp, Max);
BROOKESIA_DESCRIBE_ENUM(AgentManager::GeneralEvent, Started, Stopped, Slept, Awake, Max);
BROOKESIA_DESCRIBE_ENUM(
    AgentManager::GeneralState, TimeSyncing, TimeSynced, Starting, Stopping, Started, Sleeping, WakingUp, Slept, Max
);
BROOKESIA_DESCRIBE_ENUM(
    AgentManager::FunctionId, SetAgentInfo, ActivateAgent, DeactivateAgent, GetAgentAttributes, GetActiveAgent,
    TriggerGeneralAction, TriggerSuspend, TriggerResume, TriggerInterruptSpeaking, GetGeneralState, GetSuspendStatus,
    ResetData, Max
);
BROOKESIA_DESCRIBE_ENUM(
    AgentManager::EventId, GeneralActionTriggered, GeneralEventHappened, SuspendStatusChanged,
    AgentSpeakingTextGot, UserSpeakingTextGot, EmoteGot, Max
);
BROOKESIA_DESCRIBE_ENUM(AgentManager::FunctionSetAgentInfoParam, Name, Info);
BROOKESIA_DESCRIBE_ENUM(AgentManager::FunctionActivateAgentParam, Name);
BROOKESIA_DESCRIBE_ENUM(AgentManager::FunctionGetAgentAttributesParam, Name);
BROOKESIA_DESCRIBE_ENUM(AgentManager::FunctionTriggerGeneralActionParam, Action);
BROOKESIA_DESCRIBE_ENUM(AgentManager::EventGeneralActionTriggeredParam, Action);
BROOKESIA_DESCRIBE_ENUM(AgentManager::EventGeneralEventHappenedParam, Event);
BROOKESIA_DESCRIBE_ENUM(AgentManager::EventSuspendStatusChangedParam, IsSuspended);
BROOKESIA_DESCRIBE_ENUM(AgentManager::EventAgentSpeakingTextGotParam, Text);
BROOKESIA_DESCRIBE_ENUM(AgentManager::EventUserSpeakingTextGotParam, Text);
BROOKESIA_DESCRIBE_ENUM(AgentManager::EventEmoteGotParam, Emote);

} // namespace esp_brookesia::service::helper
