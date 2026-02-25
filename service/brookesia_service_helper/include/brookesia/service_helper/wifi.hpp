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

    /**
     * @brief General state for WiFi state machine
     *
     * Stable states: Idle, Inited, Started, Connected
     * Transient states: Initing, Deiniting, Starting, Stopping, Connecting, Disconnecting
     */
    enum class GeneralState {
        Idle,           // Stable state: WiFi is not initialized
        Initing,        // Transient state: WiFi is initializing
        Inited,         // Stable state: WiFi is initialized but not started
        Deiniting,      // Transient state: WiFi is deinitializing
        Starting,       // Transient state: WiFi is starting
        Started,        // Stable state: WiFi is started but not connected
        Stopping,       // Transient state: WiFi is stopping
        Connecting,     // Transient state: WiFi is connecting
        Connected,      // Stable state: WiFi is connected
        Disconnecting,  // Transient state: WiFi is disconnecting
        Max,
    };

    struct ScanParams {
        bool operator==(const ScanParams &other) const
        {
            return (ap_count == other.ap_count) && (interval_ms == other.interval_ms) && (timeout_ms == other.timeout_ms);
        }

        bool operator!=(const ScanParams &other) const
        {
            return !(*this == other);
        }

        size_t ap_count = 20;
        uint32_t interval_ms = 10000;
        uint32_t timeout_ms = 60000;
    };

    enum class ApSignalLevel {
        LEVEL_0, // <= -81
        LEVEL_1, // -80 ~ -71
        LEVEL_2, // -70 ~ -61
        LEVEL_3, // -60 ~ -51
        LEVEL_4, // >= -50
    };

    struct ApInfo {
        static ApSignalLevel get_signal_level(int rssi)
        {
            ApSignalLevel signal_level = ApSignalLevel::LEVEL_0;
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
            return signal_level;
        }

        std::string ssid;
        bool is_locked;
        int rssi;
        ApSignalLevel signal_level;
        uint8_t channel;
    };

    struct SoftApParams {
        bool operator==(const SoftApParams &other) const
        {
            if ((ssid != other.ssid) || (password != other.password) ||
                    (channel.has_value() != other.channel.has_value())) {
                return false;
            }
            if (channel.has_value() && (get_channel() != other.get_channel())) {
                return false;
            }
            return true;
        }

        bool operator!=(const SoftApParams &other) const
        {
            return !(*this == other);
        }

        bool has_channel() const
        {
            return channel.has_value();
        }

        uint8_t get_channel() const
        {
            return channel.value();
        }

        std::string ssid = "";
        std::string password = "";
        uint8_t max_connection = 4;
        std::optional<uint8_t> channel = std::nullopt;  // If not set, will try to find the best channel from scan AP infos
    };

    enum class SoftApEvent {
        Started,
        Stopped,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionId {
        TriggerGeneralAction,
        TriggerScanStart,
        TriggerScanStop,
        TriggerSoftApStart,
        TriggerSoftApStop,
        TriggerSoftApProvisionStart,
        TriggerSoftApProvisionStop,
        SetScanParams,
        SetSoftApParams,
        SetConnectAp,
        GetGeneralState,
        GetConnectAp,
        GetConnectedAps,
        GetSoftApParams,
        ResetData,
        Max,
    };

    enum class EventId {
        GeneralActionTriggered,
        GeneralEventHappened,
        ScanApInfosUpdated,
        SoftApEventHappened,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionTriggerGeneralActionParam {
        Action,
    };

    enum class FunctionSetScanParamsParam {
        Param,
    };

    enum class FunctionSetSoftApParamsParam {
        Param,
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
        IsUnexpected,
    };

    enum class EventScanApInfosUpdatedParam {
        ApInfos,
    };

    enum class EventSoftApEventHappenedParam {
        Event,
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

    static FunctionSchema function_schema_trigger_softap_start()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerSoftApStart),
            .description = "Trigger SoftAP start",
        };
    }

    static FunctionSchema function_schema_trigger_softap_stop()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerSoftApStop),
            .description = "Trigger SoftAP stop",
        };
    }

    static FunctionSchema function_schema_trigger_softap_provision_start()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerSoftApProvisionStart),
            .description = "Trigger SoftAP provision start",
        };
    }

    static FunctionSchema function_schema_trigger_softap_provision_stop()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerSoftApProvisionStop),
            .description = "Trigger SoftAP provision stop",
        };
    }

    static FunctionSchema function_schema_set_scan_params()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetScanParams),
            .description = "Set the scan parameters",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetScanParamsParam::Param),
                    .description = (boost::format("The scan parameters, a JSON object. Example: %1%")
                                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(ScanParams())).str(),
                    .type = FunctionValueType::Object
                }
            },
        };
    }

    static FunctionSchema function_schema_set_ap_params()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetSoftApParams),
            .description = "Set the SoftAP parameters",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetSoftApParamsParam::Param),
                    .description = (boost::format("The SoftAP parameters, a JSON object. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(SoftApParams({
                        .ssid = "ssid",
                        .password = "password",
                        .channel = 1
                    }))).str(),
                    .type = FunctionValueType::Object
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

    static FunctionSchema function_schema_get_general_state()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetGeneralState),
            .description = (boost::format("Get the current general state, should be one of the following: %1%")
            % BROOKESIA_DESCRIBE_TO_STR(std::vector<GeneralState>({
                GeneralState::Idle, GeneralState::Initing, GeneralState::Inited, GeneralState::Deiniting,
                GeneralState::Starting, GeneralState::Started, GeneralState::Stopping, GeneralState::Connecting,
                GeneralState::Connected, GeneralState::Disconnecting
            }))).str(),
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

    static FunctionSchema function_schema_get_softap_params()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetSoftApParams),
            .description = (boost::format("Get the parameters of the SoftAP, a JSON object. Example: %1%")
                            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(SoftApParams())).str(),
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
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventGeneralEventHappenedParam::IsUnexpected),
                    .description = "Indicates whether the event occurred unexpectedly. "
                    "False: triggered by user action or intentional behavior. "
                    "True: occurred due to an unexpected state.",
                    .type = EventItemType::Boolean
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
                    .description = "The scan AP infos, a JSON array of objects",
                    .type = EventItemType::Array
                }
            }
        };
    }

    static EventSchema event_schema_softap_event_happened()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::SoftApEventHappened),
            .description = "SoftAP event happened event, will be triggered when a SoftAP event happens",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventSoftApEventHappenedParam::Event),
                    .description = (boost::format("The SoftAP event happened, should be one of the following: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<SoftApEvent>({
                        SoftApEvent::Started, SoftApEvent::Stopped
                    }))).str(),
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
        return "Wifi";
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> FUNCTION_SCHEMAS = {{
                function_schema_trigger_general_action(),
                function_schema_trigger_scan_start(),
                function_schema_trigger_scan_stop(),
                function_schema_trigger_softap_start(),
                function_schema_trigger_softap_stop(),
                function_schema_trigger_softap_provision_start(),
                function_schema_trigger_softap_provision_stop(),
                function_schema_set_scan_params(),
                function_schema_set_ap_params(),
                function_schema_set_connect_ap(),
                function_schema_get_general_state(),
                function_schema_get_connect_ap(),
                function_schema_get_connected_aps(),
                function_schema_get_softap_params(),
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
                event_schema_softap_event_happened(),
            }
        };
        return std::span<const EventSchema>(EVENT_SCHEMAS);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the describe macros //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief  General related
 */
BROOKESIA_DESCRIBE_ENUM(Wifi::GeneralAction, Init, Deinit, Start, Stop, Connect, Disconnect);
BROOKESIA_DESCRIBE_ENUM(Wifi::GeneralEvent, Deinited, Inited, Stopped, Started, Disconnected, Connected);
BROOKESIA_DESCRIBE_ENUM(
    Wifi::GeneralState, Idle, Initing, Inited, Deiniting, Starting, Started, Stopping, Connecting, Connected,
    Disconnecting, Max
);

/**
 * @brief  Scan related
 */
BROOKESIA_DESCRIBE_ENUM(Wifi::ApSignalLevel, LEVEL_0, LEVEL_1, LEVEL_2, LEVEL_3, LEVEL_4);
BROOKESIA_DESCRIBE_STRUCT(Wifi::ApInfo, (), (ssid, is_locked, rssi, signal_level, channel));
BROOKESIA_DESCRIBE_STRUCT(Wifi::ScanParams, (), (ap_count, interval_ms, timeout_ms));

/**
 * @brief SoftAP related
 */
BROOKESIA_DESCRIBE_STRUCT(Wifi::SoftApParams, (), (ssid, password, max_connection, channel));
BROOKESIA_DESCRIBE_ENUM(Wifi::SoftApEvent, Started, Stopped);

/**
 * @brief  Function related
 */
BROOKESIA_DESCRIBE_ENUM(
    Wifi::FunctionId, TriggerGeneralAction, TriggerScanStart, TriggerScanStop, TriggerSoftApStart, TriggerSoftApStop,
    TriggerSoftApProvisionStart, TriggerSoftApProvisionStop, SetScanParams, SetSoftApParams, SetConnectAp,
    GetGeneralState, GetConnectAp, GetConnectedAps, GetSoftApParams, ResetData, Max
);
BROOKESIA_DESCRIBE_ENUM(Wifi::FunctionTriggerGeneralActionParam, Action);
BROOKESIA_DESCRIBE_ENUM(Wifi::FunctionSetScanParamsParam, Param);
BROOKESIA_DESCRIBE_ENUM(Wifi::FunctionSetSoftApParamsParam, Param);
BROOKESIA_DESCRIBE_ENUM(Wifi::FunctionSetConnectApParam, SSID, Password);

/**
 * @brief  Event related
 */
BROOKESIA_DESCRIBE_ENUM(
    Wifi::EventId, GeneralActionTriggered, GeneralEventHappened, ScanApInfosUpdated, SoftApEventHappened, Max
);
BROOKESIA_DESCRIBE_ENUM(Wifi::EventGeneralActionTriggeredParam, Action);
BROOKESIA_DESCRIBE_ENUM(Wifi::EventGeneralEventHappenedParam, Event, IsUnexpected);
BROOKESIA_DESCRIBE_ENUM(Wifi::EventScanApInfosUpdatedParam, ApInfos);
BROOKESIA_DESCRIBE_ENUM(Wifi::EventSoftApEventHappenedParam, Event);
} // namespace esp_brookesia::service::helper
