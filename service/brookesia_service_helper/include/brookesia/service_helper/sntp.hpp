/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_helper/base.hpp"

namespace esp_brookesia::service::helper {

class SNTP: public Base<SNTP> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionId {
        SetServers,
        SetTimezone,
        Start,
        Stop,
        GetServers,
        GetTimezone,
        IsTimeSynced,
        ResetData,
        Max,
    };

    enum class EventId {
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionSetServersParam {
        Servers,
    };

    enum class FunctionSetTimezoneParam {
        Timezone,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // SNTP has no events, so no event parameter types are defined

private:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function schemas /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static FunctionSchema function_schema_set_servers()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::SetServers),
            .description = "Set the NTP servers.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetServersParam::Servers),
                    .description = "The JSON array of NTP servers to set.",
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
            .description = "Set the timezone.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetTimezoneParam::Timezone),
                    .description = "The timezone to set.",
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
            .description = "Start the SNTP service.",
        };
    }

    static FunctionSchema function_schema_stop()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::Stop),
            .description = "Stop the SNTP service",
        };
    }

    static FunctionSchema function_schema_get_servers()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::GetServers),
            .description = "Get the NTP servers, return a JSON array of NTP servers.",
        };
    }

    static FunctionSchema function_schema_get_timezone()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::GetTimezone),
            .description = "Get the timezone, return a string of timezone.",
        };
    }

    static FunctionSchema function_schema_is_time_synced()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::IsTimeSynced),
            .description = "Check if the time is synced. Return a boolean value.",
        };
    }

    static FunctionSchema function_schema_reset_data()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::ResetData),
            .description = "Reset the data of NTP servers, timezone and time sync status.",
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // SNTP has no events, so no event schemas are defined

public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the functions required by the Base class /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static constexpr std::string_view get_name()
    {
        return "SNTP";
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> FUNCTION_SCHEMAS = {{
                function_schema_set_servers(),
                function_schema_set_timezone(),
                function_schema_start(),
                function_schema_stop(),
                function_schema_get_servers(),
                function_schema_get_timezone(),
                function_schema_is_time_synced(),
                function_schema_reset_data(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }

    static std::span<const EventSchema> get_event_schemas()
    {
        return std::span<const EventSchema>();
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the describe macros //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROOKESIA_DESCRIBE_ENUM(
    SNTP::FunctionId, SetServers, SetTimezone, Start, Stop, GetServers, GetTimezone, IsTimeSynced, ResetData, Max
);
BROOKESIA_DESCRIBE_ENUM(SNTP::EventId, Max);
BROOKESIA_DESCRIBE_ENUM(SNTP::FunctionSetServersParam, Servers);
BROOKESIA_DESCRIBE_ENUM(SNTP::FunctionSetTimezoneParam, Timezone);

} // namespace esp_brookesia::service::helper
