/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "boost/format.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/function/definition.hpp"
#include "brookesia/service_manager/event/definition.hpp"

namespace esp_brookesia::service::helper {

class Wifi {
public:
    enum class GeneralState {
        Deinited,
        Inited,
        Started,
        Connected,
        Max,
    };

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

    enum FunctionIndex {
        FunctionIndexTriggerGeneralAction,
        FunctionIndexTriggerScanStart,
        FunctionIndexTriggerScanStop,
        FunctionIndexSetScanParams,
        FunctionIndexSetConnectAp,
        FunctionIndexGetConnectAp,
        FunctionIndexGetConnectedAps,
        FunctionIndexMax,
    };

    enum EventIndex {
        EventIndexGeneralActionTriggered,
        EventIndexGeneralEventHappened,
        EventIndexScanApInfosUpdated,
        EventIndexMax,
    };

    static constexpr const char *SERVICE_NAME = "wifi";

    static const FunctionSchema *get_function_definitions()
    {
        static const FunctionSchema FUNCTION_DEFINITIONS[FunctionIndexMax] = {
            [FunctionIndexTriggerGeneralAction] = {
                // name
                "trigger_general_action",
                // description
                "Trigger a general action",
                // parameters
                {
                    // parameters[0]
                    {
                        // name
                        "action",
                        // description
                        (boost::format("The general action, can be one of the following: %1%") %
                        BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<GeneralAction>({
                            GeneralAction::Init, GeneralAction::Deinit, GeneralAction::Start, GeneralAction::Stop,
                            GeneralAction::Connect, GeneralAction::Disconnect
                        }))).str(),
                        // type
                        FunctionValueType::String
                    }
                }
            },
            [FunctionIndexTriggerScanStart] = {
                // name
                "trigger_scan_start",
                // description
                "Trigger WiFi scan start",
                // parameters
                {}
            },
            [FunctionIndexTriggerScanStop] = {
                // name
                "trigger_scan_stop",
                // description
                "Trigger WiFi scan stop",
                // parameters
                {}
            },
            [FunctionIndexSetScanParams] = {
                // name
                "set_scan_params",
                // description
                "Set the scan parameters",
                // parameters
                {
                    // parameters[0]
                    {
                        // name
                        "ap_count",
                        // description
                        "The number of APs to scan, optional",
                        // type
                        FunctionValueType::Number,
                        // default value
                        20.0
                    },
                    // parameters[1]
                    {
                        // name
                        "interval_ms",
                        // description
                        "The interval of the scan in milliseconds, optional",
                        // type
                        FunctionValueType::Number,
                        // default value
                        10000.0
                    },
                    // parameters[2]
                    {
                        // name
                        "timeout_ms",
                        // description
                        "The timeout of the scan in milliseconds, optional",
                        // type
                        FunctionValueType::Number,
                        // default value
                        60000.0
                    }
                }
            },
            [FunctionIndexSetConnectAp] = {
                // name
                "set_connect_ap",
                // description
                "Set the SSID and password of the AP to connect to",
                // parameters
                {
                    // parameters[0]
                    {
                        // name
                        "ssid",
                        // description
                        "The SSID of the AP, required",
                        // type
                        FunctionValueType::String
                    },
                    // parameters[1]
                    {
                        // name
                        "password",
                        // description
                        "The password of the AP, optional",
                        // type
                        FunctionValueType::String,
                        // default value
                        ""
                    }
                }
            },
            [FunctionIndexGetConnectAp] = {
                // name
                "get_connect_ap",
                // description
                (boost::format("Get the connect AP SSID. Return a string. Example: %1%")
                 % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::string("ssid1"))).str(),
                // parameters
                {}
            },
            [FunctionIndexGetConnectedAps] = {
                // name
                "get_connected_aps",
                // description
                (boost::format("Get the connected AP SSIDs. Return a JSON array of strings. Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({"ssid1", "ssid2", "ssid3"}))).str(),
                // parameters
                {}
            },
        };
        return FUNCTION_DEFINITIONS;
    }
    static const EventSchema *get_event_definitions()
    {
        static const EventSchema EVENT_DEFINITIONS[EventIndexMax] = {
            [EventIndexGeneralActionTriggered] = {
                // name
                "general_action_triggered",
                // description
                "General action triggered event, will be triggered when a general action is triggered successfully",
                // items
                {
                    // items[0]
                    {
                        // name
                        "action",
                        // description
                        (boost::format("The general action, can be one of the following: %1%") %
                        BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<GeneralAction>({
                            GeneralAction::Init, GeneralAction::Deinit, GeneralAction::Start, GeneralAction::Stop,
                            GeneralAction::Connect, GeneralAction::Disconnect
                        }))).str(),
                        // type
                        EventItemType::String
                    },
                }
            },
            [EventIndexGeneralEventHappened] = {
                // name
                "general_event_happened",
                // description
                "General event happened event, will be triggered when a general event happens",
                // items
                {
                    // items[0]
                    {
                        // name
                        "event",
                        // description
                        (boost::format("The general event happened, can be one of the following: %1%") %
                        BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<GeneralEvent>({
                            GeneralEvent::Deinited, GeneralEvent::Inited, GeneralEvent::Stopped, GeneralEvent::Started,
                            GeneralEvent::Disconnected, GeneralEvent::Connected
                        }))).str(),
                        // type
                        EventItemType::String
                    },
                }
            },
            [EventIndexScanApInfosUpdated] = {
                // name
                "scan_ap_infos_updated",
                // description
                "Scan AP infos updated event, will be triggered when the scan AP infos are updated",
                // items
                {
                    // items[0]
                    {
                        // name
                        "ap_infos",
                        // description
                        (boost::format("The scan AP infos, a JSON array of objects. Example: %1%") %
                        BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<ApInfo>({
                            ApInfo("ssid1", false, -81), ApInfo("ssid2", true, -71), ApInfo("ssid3", false, -61),
                            ApInfo("ssid4", true, -51), ApInfo("ssid5", false, -41)
                        }))).str(),
                        // type
                        EventItemType::Array
                    }
                }
            },
        };
        return EVENT_DEFINITIONS;
    }
};

BROOKESIA_DESCRIBE_ENUM(Wifi::GeneralState, Deinited, Inited, Started, Connected, Max);
BROOKESIA_DESCRIBE_ENUM(Wifi::GeneralAction, Init, Deinit, Start, Stop, Connect, Disconnect);
BROOKESIA_DESCRIBE_ENUM(Wifi::GeneralEvent, Deinited, Inited, Stopped, Started, Disconnected, Connected);
BROOKESIA_DESCRIBE_ENUM(Wifi::ApSignalLevel, LEVEL_0, LEVEL_1, LEVEL_2, LEVEL_3, LEVEL_4);
BROOKESIA_DESCRIBE_STRUCT(Wifi::ApInfo, (), (ssid, is_locked, rssi, signal_level));

} // namespace esp_brookesia::service::helper
