/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_helper/base.hpp"
#include "manager.hpp"

namespace esp_brookesia::agent::helper {

/**
 * @brief Helper schema definitions for the Coze agent service.
 */
class Coze: public service::helper::Base<Coze> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Credentials required to authenticate with the Coze backend.
     */
    struct AuthInfo {
        std::string session_name; ///< Session name used by the Coze SDK.
        std::string device_id; ///< Unique device identifier.
        std::string custom_consumer; ///< Consumer identifier passed to the backend.
        std::string app_id; ///< Application id.
        std::string user_id; ///< End-user id.
        std::string public_key; ///< Public key used for authentication.
        std::string private_key; ///< Private key used for authentication.
    };

    /**
     * @brief Metadata for one available Coze robot.
     */
    struct RobotInfo {
        std::string name; ///< Human-readable robot name.
        std::string bot_id; ///< Coze bot identifier.
        std::string voice_id; ///< Voice profile identifier.
        std::string description; ///< Optional robot description.
    };

    /**
     * @brief Persistent Coze agent configuration.
     */
    struct Info {
        AuthInfo authorization; ///< Authentication material.
        std::vector<RobotInfo> robots; ///< Available robot definitions.
    };

    /**
     * @brief Coze-specific events surfaced by the agent.
     */
    enum class CozeEvent {
        InsufficientCreditsBalance,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionId : uint8_t {
        SetActiveRobotIndex,
        GetActiveRobotIndex,
        GetRobotInfos,
        Max,
    };

    enum class EventId : uint8_t {
        CozeEventHappened,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionSetActiveRobotIndexParam : uint8_t {
        Index,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class EventCozeEventHappenedParam : uint8_t {
        CozeEvent,
    };

private:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function schemas /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static service::FunctionSchema function_schema_set_active_robot_index()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetActiveRobotIndex),
            .description = "Set active robot index.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetActiveRobotIndexParam::Index),
                    .description = "Robot index to activate.",
                    .type = service::FunctionValueType::Number
                }
            },
            .require_scheduler = false
        };
    }

    static service::FunctionSchema function_schema_get_active_robot_index()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetActiveRobotIndex),
            .description = "Get active robot index. Return type: number. Example: 0",
            .require_scheduler = false
        };
    }

    static service::FunctionSchema function_schema_get_robot_infos()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetRobotInfos),
            .description = (boost::format("Get robot info list. Return type: JSON array<object>. Example: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<RobotInfo>({
                RobotInfo{"robot1", "bot_id1", "voice_id1", "description1"},
                RobotInfo{"robot2", "bot_id2", "voice_id2", "description2"}
            }))).str(),
            .require_scheduler = false
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static service::EventSchema event_schema_coze_event_happened()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::CozeEventHappened),
            .description = "Emitted when a Coze event occurs.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventCozeEventHappenedParam::CozeEvent),
                    .description = (boost::format("Coze event. Allowed values: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<CozeEvent>({
                        CozeEvent::InsufficientCreditsBalance
                    }))).str(),
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
     * @brief Name of the Coze agent service.
     *
     * @return std::string_view Stable service name.
     */
    static constexpr std::string_view get_name()
    {
        return "AgentCoze";
    }

    /**
     * @brief Get the function schemas exported by the Coze agent.
     *
     * @return std::span<const service::FunctionSchema> Static schema span.
     */
    static std::span<const service::FunctionSchema> get_function_schemas()
    {
        static const std::array < service::FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max) > FUNCTION_SCHEMAS = {{
                function_schema_set_active_robot_index(),
                function_schema_get_active_robot_index(),
                function_schema_get_robot_infos(),
            }
        };
        return std::span<const service::FunctionSchema>(FUNCTION_SCHEMAS);
    }

    /**
     * @brief Get the event schemas exported by the Coze agent.
     *
     * @return std::span<const service::EventSchema> Static schema span.
     */
    static std::span<const service::EventSchema> get_event_schemas()
    {
        static const std::array < service::EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EventId::Max) > EVENT_SCHEMAS = {{
                event_schema_coze_event_happened(),
            }
        };
        return std::span<const service::EventSchema>(EVENT_SCHEMAS);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the describe macros //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROOKESIA_DESCRIBE_STRUCT(
    Coze::AuthInfo, (), (session_name, device_id, custom_consumer, app_id, user_id, public_key, private_key)
);
BROOKESIA_DESCRIBE_STRUCT(
    Coze::RobotInfo, (), (name, bot_id, voice_id, description)
);
BROOKESIA_DESCRIBE_STRUCT(
    Coze::Info, (), (authorization, robots)
);
BROOKESIA_DESCRIBE_ENUM(Coze::CozeEvent, InsufficientCreditsBalance, Max);
BROOKESIA_DESCRIBE_ENUM(Coze::FunctionId, SetActiveRobotIndex, GetActiveRobotIndex, GetRobotInfos, Max);
BROOKESIA_DESCRIBE_ENUM(Coze::EventId, CozeEventHappened, Max);
BROOKESIA_DESCRIBE_ENUM(Coze::FunctionSetActiveRobotIndexParam, Index);
BROOKESIA_DESCRIBE_ENUM(Coze::EventCozeEventHappenedParam, CozeEvent);

} // namespace esp_brookesia::agent::helper
