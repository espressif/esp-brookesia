/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_helper/base.hpp"

namespace esp_brookesia::service::helper {

class Wifi: public Base<Wifi> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * They are used as parameter and return types for functions and events.
     * Users can access or modify these types via serialization and deserialization.
     */
    enum class GeneralAction {
        Init,
        Deinit,
        Start,
        Stop,
        Connect,
        Disconnect,
        Max,
    };

    enum class GeneralEvent {
        Deinited,
        Inited,
        Stopped,
        Started,
        Disconnected,
        Connected,
        Max,
    };

    enum class ApSignalLevel {
        LEVEL_0, // <= -81
        LEVEL_1, // -80 ~ -71
        LEVEL_2, // -70 ~ -61
        LEVEL_3, // -60 ~ -51
        LEVEL_4, // >= -50
    };

    struct ApInfo {
        ApInfo() = default;
        ApInfo(const std::string &ssid, bool is_locked, int rssi)
            : ssid(ssid)
            , is_locked(is_locked)
            , rssi(rssi)
        {
            if (rssi <= -81) {
                signal_level = ApSignalLevel::LEVEL_0;
            } else if (rssi <= -71) {
                signal_level = ApSignalLevel::LEVEL_1;
            } else if (rssi <= -61) {
                signal_level = ApSignalLevel::LEVEL_2;
            } else if (rssi <= -51) {
                signal_level = ApSignalLevel::LEVEL_3;
            } else {
                signal_level = ApSignalLevel::LEVEL_4;
            }
        }

        std::string ssid;
        bool is_locked;
        int rssi;
        ApSignalLevel signal_level;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionId {
        TriggerGeneralAction,
        TriggerScanStart,
        TriggerScanStop,
        SetScanParams,
        SetConnectAp,
        GetConnectAp,
        GetConnectedAps,
        ResetData,
        Max,
    };

    enum class EventId {
        GeneralActionTriggered,
        GeneralEventHappened,
        ScanApInfosUpdated,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionTriggerGeneralActionParam {
        Action,
    };

    enum class FunctionSetScanParamsParam {
        ApCount,
        IntervalMs,
        TimeoutMs,
    };

    enum class FunctionSetConnectApParam {
        SSID,
        Password,
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

    enum class EventScanApInfosUpdatedParam {
        ApInfos,
    };

private:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function schemas /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static FunctionSchema function_schema_trigger_general_action()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerGeneralAction),
            .description = "Trigger a general action",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionTriggerGeneralActionParam::Action),
                    .description = (boost::format("The general action, should be one of the following: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<GeneralAction>({
                        GeneralAction::Init, GeneralAction::Deinit, GeneralAction::Start, GeneralAction::Stop,
                        GeneralAction::Connect, GeneralAction::Disconnect
                    }))).str(),
                    .type = FunctionValueType::String
                }
            }
        };
    }

    static FunctionSchema function_schema_trigger_scan_start()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerScanStart),
            .description = "Trigger WiFi scan start",
        };
    }

    static FunctionSchema function_schema_trigger_scan_stop()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerScanStop),
            .description = "Trigger WiFi scan stop",
        };
    }

    static FunctionSchema function_schema_set_scan_params()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetScanParams),
            .description = "Set the scan parameters",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetScanParamsParam::ApCount),
                    .description = "The number of APs to scan, optional",
                    .type = FunctionValueType::Number,
                    .default_value = FunctionValue(20)
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetScanParamsParam::IntervalMs),
                    .description = "The interval of the scan in milliseconds, optional",
                    .type = FunctionValueType::Number,
                    .default_value = FunctionValue(10000)
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetScanParamsParam::TimeoutMs),
                    .description = "The timeout of the scan in milliseconds, optional",
                    .type = FunctionValueType::Number,
                    .default_value = FunctionValue(60000)
                }
            }
        };
    }

    static FunctionSchema function_schema_set_connect_ap()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetConnectAp),
            .description = "Set the SSID and password of the AP to connect to",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetConnectApParam::SSID),
                    .description = "The SSID of the AP, required",
                    .type = FunctionValueType::String
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetConnectApParam::Password),
                    .description = "The password of the AP, optional",
                    .type = FunctionValueType::String,
                    .default_value = FunctionValue(std::string(""))
                }
            }
        };
    }

    static FunctionSchema function_schema_get_connect_ap()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetConnectAp),
            .description = (boost::format("Get the connect AP SSID. Return a string. Example: %1%")
                            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::string("ssid1"))).str(),
        };
    }

    static FunctionSchema function_schema_get_connected_aps()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetConnectedAps),
            .description = (boost::format("Get the connected AP SSIDs. Return a JSON array of strings. Example: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({"ssid1", "ssid2", "ssid3"}))).str(),
        };
    }

    static FunctionSchema function_schema_reset_data()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::ResetData),
            .description = "Reset the data of the WiFi service, including the target connect AP, scan parameters, "
            "and connected APs. This function will clear the NVS data.",
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static EventSchema event_schema_general_action_triggered()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::GeneralActionTriggered),
            .description = "General action triggered event, will be triggered when a general action is triggered successfully",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventGeneralActionTriggeredParam::Action),
                    .description = (boost::format("The general action, should be one of the following: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<GeneralAction>({
                        GeneralAction::Init, GeneralAction::Deinit, GeneralAction::Start, GeneralAction::Stop,
                        GeneralAction::Connect, GeneralAction::Disconnect
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
            .description = "General event happened event, will be triggered when a general event happens",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventGeneralEventHappenedParam::Event),
                    .description = (boost::format("The general event happened, should be one of the following: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<GeneralEvent>({
                        GeneralEvent::Deinited, GeneralEvent::Inited, GeneralEvent::Stopped, GeneralEvent::Started,
                        GeneralEvent::Disconnected, GeneralEvent::Connected
                    }))).str(),
                    .type = EventItemType::String
                }
            }
        };
    }

    static EventSchema event_schema_scan_ap_infos_updated()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::ScanApInfosUpdated),
            .description = "Scan AP infos updated event, will be triggered when the scan AP infos are updated",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventScanApInfosUpdatedParam::ApInfos),
                    .description = (boost::format("The scan AP infos, a JSON array of objects. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<ApInfo>({
                        ApInfo("ssid1", false, -81), ApInfo("ssid2", true, -71), ApInfo("ssid3", false, -61),
                        ApInfo("ssid4", true, -51), ApInfo("ssid5", false, -41)
                    }))).str(),
                    .type = EventItemType::Array
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
        return "Wifi";
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> FUNCTION_SCHEMAS = {{
                function_schema_trigger_general_action(),
                function_schema_trigger_scan_start(),
                function_schema_trigger_scan_stop(),
                function_schema_set_scan_params(),
                function_schema_set_connect_ap(),
                function_schema_get_connect_ap(),
                function_schema_get_connected_aps(),
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
                event_schema_scan_ap_infos_updated(),
            }
        };
        return std::span<const EventSchema>(EVENT_SCHEMAS);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the describe macros //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROOKESIA_DESCRIBE_ENUM(
    Wifi::FunctionId, TriggerGeneralAction, TriggerScanStart, TriggerScanStop, SetScanParams, SetConnectAp,
    GetConnectAp, GetConnectedAps, ResetData, Max
);
BROOKESIA_DESCRIBE_ENUM(Wifi::EventId, GeneralActionTriggered, GeneralEventHappened, ScanApInfosUpdated, Max);
BROOKESIA_DESCRIBE_ENUM(Wifi::FunctionTriggerGeneralActionParam, Action);
BROOKESIA_DESCRIBE_ENUM(Wifi::FunctionSetScanParamsParam, ApCount, IntervalMs, TimeoutMs);
BROOKESIA_DESCRIBE_ENUM(Wifi::FunctionSetConnectApParam, SSID, Password);
BROOKESIA_DESCRIBE_ENUM(Wifi::EventGeneralActionTriggeredParam, Action);
BROOKESIA_DESCRIBE_ENUM(Wifi::EventGeneralEventHappenedParam, Event);
BROOKESIA_DESCRIBE_ENUM(Wifi::EventScanApInfosUpdatedParam, ApInfos);
BROOKESIA_DESCRIBE_ENUM(Wifi::GeneralAction, Init, Deinit, Start, Stop, Connect, Disconnect);
BROOKESIA_DESCRIBE_ENUM(Wifi::GeneralEvent, Deinited, Inited, Stopped, Started, Disconnected, Connected);
BROOKESIA_DESCRIBE_ENUM(Wifi::ApSignalLevel, LEVEL_0, LEVEL_1, LEVEL_2, LEVEL_3, LEVEL_4);
BROOKESIA_DESCRIBE_STRUCT(Wifi::ApInfo, (), (ssid, is_locked, rssi, signal_level));

} // namespace esp_brookesia::service::helper
