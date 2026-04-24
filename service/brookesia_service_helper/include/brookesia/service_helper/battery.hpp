/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include "boost/format.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_helper/base.hpp"

namespace esp_brookesia::service::helper {

/**
 * @brief Helper schema definitions for the battery service.
 */
class Battery: public Base<Battery> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Battery status snapshot.
     */
    struct Status {
        uint8_t level = 0;
        bool charging = false;
        bool discharging = false;
        int voltage_mv = 0;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Battery service function identifiers.
     */
    enum class FunctionId : uint8_t {
        GetStatus,
        Max,
    };

    /**
     * @brief Battery service event identifiers.
     */
    enum class EventId : uint8_t {
        Max,
    };

private:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function schemas /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static Status get_status_example()
    {
        return Status{76, false, true, 3870};
    }

    static FunctionSchema function_schema_get_status()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::GetStatus),
            .description = (boost::format("Get current battery status. Return type: JSON object. Example: %1%")
                            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(get_status_example())).str(),
            .require_scheduler = false,
        };
    }

public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the functions required by the Base class /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Service name used by `ServiceManager`.
     *
     * @return std::string_view Stable service name.
     */
    static constexpr std::string_view get_name()
    {
        return "Battery";
    }

    /**
     * @brief Get function schemas exported by battery service.
     *
     * @return std::span<const FunctionSchema> Static function schema span.
     */
    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> FUNCTION_SCHEMAS = {{
                function_schema_get_status(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }

    /**
     * @brief Get event schemas exported by battery service.
     *
     * @return std::span<const EventSchema> Empty span because battery service exposes no events.
     */
    static std::span<const EventSchema> get_event_schemas()
    {
        return std::span<const EventSchema>();
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the describe macros //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROOKESIA_DESCRIBE_STRUCT(Battery::Status, (), (level, charging, discharging, voltage_mv));
BROOKESIA_DESCRIBE_ENUM(Battery::FunctionId, GetStatus, Max);
BROOKESIA_DESCRIBE_ENUM(Battery::EventId, Max);

} // namespace esp_brookesia::service::helper
