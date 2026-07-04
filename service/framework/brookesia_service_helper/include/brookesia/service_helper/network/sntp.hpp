/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <span>
#include <vector>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/helper/base.hpp"

namespace esp_brookesia::service::helper {

/**
 * @brief Helper schema definitions for the SNTP service.
 */
class SNTP: public Base<SNTP> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief SNTP synchronization state.
     */
    enum class State {
        Stopped,
        CheckingNetwork,
        Syncing,
        Synced,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief SNTP service function identifiers.
     */
    enum class FunctionId {
        SetServers,
        SetTimezone,
        Start,
        Stop,
        GetServers,
        GetTimezone,
        GetState,
        IsTimeSynced,
        LoadData,
        ResetData,
        Max,
    };

    /**
     * @brief SNTP service event identifiers.
     */
    enum class EventId {
        StateChanged,
        TimezoneChanged,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Parameter keys for `FunctionId::SetServers`.
     */
    enum class FunctionSetServersParam {
        Servers,
    };

    /**
     * @brief Parameter keys for `FunctionId::SetTimezone`.
     */
    enum class FunctionSetTimezoneParam {
        Timezone,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Event keys for `EventId::StateChanged`.
     */
    enum class EventStateChangedParam {
        State,
    };

    /**
     * @brief Event keys for `EventId::TimezoneChanged`.
     */
    enum class EventTimezoneChangedParam {
        Timezone,
    };

private:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function schemas /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static FunctionSchema function_schema_set_servers()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::SetServers),
            .description = "Set NTP servers.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetServersParam::Servers),
                    .description = (boost::format("NTP servers as JSON array<string>. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({
                        "pool.ntp.org",
                        "cn.pool.ntp.org"
                    }))).str(),
                    .type = FunctionValueType::Array
                }
            },
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_set_timezone()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::SetTimezone),
            .description = "Set timezone.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetTimezoneParam::Timezone),
                    .description = "Timezone string.",
                    .type = FunctionValueType::String
                }
            },
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_start()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::Start),
            .description = "Start SNTP service.",
        };
    }

    static FunctionSchema function_schema_stop()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::Stop),
            .description = "Stop SNTP service.",
        };
    }

    static FunctionSchema function_schema_get_servers()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::GetServers),
            .description = "Get NTP servers.",
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Array,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({
                    "pool.ntp.org",
                    "cn.pool.ntp.org"
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_get_timezone()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::GetTimezone),
            .description = "Get timezone.",
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::String,
                .description = "Example: \"CST-8\"",
            },
        };
    }

    static FunctionSchema function_schema_get_state()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::GetState),
            .description = "Get SNTP synchronization state.",
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::String,
                .description = (boost::format("Example: \"%1%\"")
                                % BROOKESIA_DESCRIBE_ENUM_TO_STR(State::CheckingNetwork)).str(),
            },
        };
    }

    static FunctionSchema function_schema_is_time_synced()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::IsTimeSynced),
            .description = "Check whether time is synced.",
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Boolean,
                .description = "Example: true",
            },
        };
    }

    static FunctionSchema function_schema_reset_data()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::ResetData),
            .description = "Reset NTP servers, timezone, and sync status.",
        };
    }

    static FunctionSchema function_schema_load_data()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::LoadData),
            .description = "Load persisted NTP servers and timezone.",
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static EventSchema event_schema_state_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(EventId::StateChanged),
            .description = "Published when the SNTP synchronization state changes.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventStateChangedParam::State),
                    .description = "Current SNTP synchronization state.",
                    .type = EventItemType::String,
                },
            },
        };
    }

    static EventSchema event_schema_timezone_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(EventId::TimezoneChanged),
            .description = "Published when the SNTP timezone changes.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventTimezoneChangedParam::Timezone),
                    .description = "Current timezone string.",
                    .type = EventItemType::String,
                },
            },
        };
    }

public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the functions required by the Base class /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Name of the SNTP service.
     *
     * @return std::string_view Stable service name.
     */
    static constexpr std::string_view get_name()
    {
        return "SNTP";
    }

    /**
     * @brief Get the function schemas exported by the SNTP service.
     *
     * @return std::span<const FunctionSchema> Static schema span.
     */
    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> FUNCTION_SCHEMAS = {{
                function_schema_set_servers(),
                function_schema_set_timezone(),
                function_schema_start(),
                function_schema_stop(),
                function_schema_get_servers(),
                function_schema_get_timezone(),
                function_schema_get_state(),
                function_schema_is_time_synced(),
                function_schema_load_data(),
                function_schema_reset_data(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }

    /**
     * @brief Get the event schemas exported by the SNTP service.
     *
     * @return std::span<const EventSchema> Static event schema span.
     */
    static std::span<const EventSchema> get_event_schemas()
    {
        static const std::array<EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EventId::Max)> EVENT_SCHEMAS = {{
                event_schema_state_changed(),
                event_schema_timezone_changed(),
            }
        };
        return std::span<const EventSchema>(EVENT_SCHEMAS);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the describe macros //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROOKESIA_DESCRIBE_ENUM(SNTP::State, Stopped, CheckingNetwork, Syncing, Synced, Max);
BROOKESIA_DESCRIBE_ENUM(
    SNTP::FunctionId, SetServers, SetTimezone, Start, Stop, GetServers, GetTimezone, GetState, IsTimeSynced,
    LoadData, ResetData, Max
);
BROOKESIA_DESCRIBE_ENUM(SNTP::EventId, StateChanged, TimezoneChanged, Max);
BROOKESIA_DESCRIBE_ENUM(SNTP::FunctionSetServersParam, Servers);
BROOKESIA_DESCRIBE_ENUM(SNTP::FunctionSetTimezoneParam, Timezone);
BROOKESIA_DESCRIBE_ENUM(SNTP::EventStateChangedParam, State);
BROOKESIA_DESCRIBE_ENUM(SNTP::EventTimezoneChangedParam, Timezone);

} // namespace esp_brookesia::service::helper
