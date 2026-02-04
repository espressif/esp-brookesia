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

class Coze: public service::helper::Base<Coze> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    struct AuthInfo {
        std::string session_name;
        std::string device_id;
        std::string custom_consumer;
        std::string app_id;
        std::string user_id;
        std::string public_key;
        std::string private_key;
    };

    struct RobotInfo {
        std::string name;
        std::string bot_id;
        std::string voice_id;
        std::string description;
    };

    struct Info {
        AuthInfo authorization;
        std::vector<RobotInfo> robots;
    };

    enum class CozeEvent {
        InsufficientCreditsBalance,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionId {
        SetActiveRobotIndex,
        GetActiveRobotIndex,
        GetRobotInfos,
        Max,
    };

    enum class EventId {
        CozeEventHappened,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionSetActiveRobotIndexParam {
        Index,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class EventCozeEventHappenedParam {
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
            .description = "Set the active robot index",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetActiveRobotIndexParam::Index),
                    .description = "The index of the robot to set as active",
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
            .description = "Get the active robot index",
            .require_scheduler = false
        };
    }

    static service::FunctionSchema function_schema_get_robot_infos()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetRobotInfos),
            .description = (boost::format("Get the robot infos. Return a JSON array of robot infos. Example: %1%")
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
            .description = "Coze event happened event, will be triggered when a coze event happens",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventCozeEventHappenedParam::CozeEvent),
                    .description = (boost::format("The coze event, should be one of the following: %1%")
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
    static constexpr std::string_view get_name()
    {
        return "AgentCoze";
    }

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
