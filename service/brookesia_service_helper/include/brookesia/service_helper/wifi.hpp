/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_helper/base.hpp"

namespace esp_brookesia::service::helper {

/**
 * @brief Helper schema definitions for the Wi-Fi service.
 */
class Wifi: public Base<Wifi> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief General Wi-Fi control actions.
     */
    enum class GeneralAction {
        Init,       /*!< Initialize Wi-Fi resources. */
        Deinit,     /*!< Deinitialize Wi-Fi resources. */
        Start,      /*!< Start Wi-Fi subsystem. */
        Stop,       /*!< Stop Wi-Fi subsystem. */
        Connect,    /*!< Connect to configured AP. */
        Disconnect, /*!< Disconnect from current AP. */
        Max,        /*!< Sentinel value. */
    };

    /**
     * @brief General Wi-Fi lifecycle events.
     */
    enum class GeneralEvent {
        Deinited,     /*!< Wi-Fi has been deinitialized. */
        Inited,       /*!< Wi-Fi has been initialized. */
        Stopped,      /*!< Wi-Fi has been stopped. */
        Started,      /*!< Wi-Fi has started. */
        Disconnected, /*!< Wi-Fi disconnected from AP. */
        Connected,    /*!< Wi-Fi connected to AP. */
        Max,          /*!< Sentinel value. */
    };

    /**
     * @brief General state for WiFi state machine
     *
     * Stable states: Idle, Inited, Started, Connected
     * Transient states: Initing, Deiniting, Starting, Stopping, Connecting, Disconnecting
     */
    enum class GeneralState {
        Idle,           /*!< Stable: Wi-Fi is not initialized. */
        Initing,        /*!< Transient: Wi-Fi is initializing. */
        Inited,         /*!< Stable: Wi-Fi initialized but not started. */
        Deiniting,      /*!< Transient: Wi-Fi is deinitializing. */
        Starting,       /*!< Transient: Wi-Fi is starting. */
        Started,        /*!< Stable: Wi-Fi started but not connected. */
        Stopping,       /*!< Transient: Wi-Fi is stopping. */
        Connecting,     /*!< Transient: Wi-Fi is connecting. */
        Connected,      /*!< Stable: Wi-Fi is connected. */
        Disconnecting,  /*!< Transient: Wi-Fi is disconnecting. */
        Max,            /*!< Sentinel value. */
    };

    /**
     * @brief Target AP connection info.
     */
    struct ConnectApInfo {
        ConnectApInfo() = default;
        ConnectApInfo(const std::string &ssid, const std::string &password = "")
            : ssid(ssid)
            , password(password)
        {}

        bool operator==(const ConnectApInfo &other) const
        {
            return (ssid == other.ssid) && (password == other.password) && (is_connectable == other.is_connectable);
        }

        bool operator!=(const ConnectApInfo &other) const
        {
            return !(*this == other);
        }

        bool is_valid() const
        {
            return !ssid.empty();
        }

        std::string ssid;        /*!< AP SSID. */
        std::string password;    /*!< AP password. */
        bool is_connectable = true; /*!< Whether this AP is considered connectable. */
    };

    /**
     * @brief Parameters for periodic AP scanning.
     */
    struct ScanParams {
        bool operator==(const ScanParams &other) const
        {
            return (ap_count == other.ap_count) && (interval_ms == other.interval_ms) && (timeout_ms == other.timeout_ms);
        }

        bool operator!=(const ScanParams &other) const
        {
            return !(*this == other);
        }

        size_t ap_count = 20;         /*!< Maximum number of AP entries to keep. */
        uint32_t interval_ms = 10000; /*!< Scan interval in milliseconds. */
        uint32_t timeout_ms = 60000;  /*!< Total scan timeout in milliseconds. */
    };

    /**
     * @brief Signal-strength bucket for a scanned AP.
     */
    enum class ScanApSignalLevel {
        LEVEL_0, /*!< RSSI <= -81 dBm. */
        LEVEL_1, /*!< RSSI in [-80, -71] dBm. */
        LEVEL_2, /*!< RSSI in [-70, -61] dBm. */
        LEVEL_3, /*!< RSSI in [-60, -51] dBm. */
        LEVEL_4, /*!< RSSI >= -50 dBm. */
    };

    /**
     * @brief One scanned AP entry.
     */
    struct ScanApInfo {
        /**
         * @brief Convert RSSI to a signal-level bucket.
         *
         * @param[in] rssi RSSI value in dBm.
         * @return Corresponding `ScanApSignalLevel`.
         */
        static ScanApSignalLevel get_signal_level(int rssi)
        {
            ScanApSignalLevel signal_level = ScanApSignalLevel::LEVEL_0;
            if (rssi <= -81) {
                signal_level = ScanApSignalLevel::LEVEL_0;
            } else if (rssi <= -71) {
                signal_level = ScanApSignalLevel::LEVEL_1;
            } else if (rssi <= -61) {
                signal_level = ScanApSignalLevel::LEVEL_2;
            } else if (rssi <= -51) {
                signal_level = ScanApSignalLevel::LEVEL_3;
            } else {
                signal_level = ScanApSignalLevel::LEVEL_4;
            }
            return signal_level;
        }

        std::string ssid;               /*!< AP SSID. */
        bool is_locked;                 /*!< Whether AP authentication is required. */
        int rssi;                       /*!< Signal strength in dBm. */
        ScanApSignalLevel signal_level; /*!< Bucketed signal level derived from RSSI. */
        uint8_t channel;                /*!< RF channel. */
    };

    /**
     * @brief SoftAP runtime configuration.
     */
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

        std::string ssid = "";                    /*!< SoftAP SSID. */
        std::string password = "";                /*!< SoftAP password. */
        uint8_t max_connection = 4;               /*!< Maximum station connections. */
        std::optional<uint8_t> channel = std::nullopt; /*!< Optional fixed channel. */
    };

    /**
     * @brief SoftAP lifecycle events.
     */
    enum class SoftApEvent {
        Started, /*!< SoftAP started. */
        Stopped, /*!< SoftAP stopped. */
        Max,     /*!< Sentinel value. */
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Wi-Fi service function identifiers.
     */
    enum class FunctionId {
        TriggerGeneralAction,
        GetGeneralState,
        SetConnectAp,
        GetConnectAp,
        GetConnectedAps,
        SetScanParams,
        TriggerScanStart,
        TriggerScanStop,
        SetSoftApParams,
        GetSoftApParams,
        TriggerSoftApStart,
        TriggerSoftApStop,
        TriggerSoftApProvisionStart,
        TriggerSoftApProvisionStop,
        ResetData,
        Max,
    };

    /**
     * @brief Wi-Fi service event identifiers.
     */
    enum class EventId {
        GeneralActionTriggered,
        GeneralEventHappened,
        ScanStateChanged,
        ScanApInfosUpdated,
        SoftApEventHappened,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Parameter keys for `FunctionId::TriggerGeneralAction`.
     */
    enum class FunctionTriggerGeneralActionParam {
        Action,
    };

    /**
     * @brief Parameter keys for `FunctionId::SetConnectAp`.
     */
    enum class FunctionSetConnectApParam {
        SSID,
        Password,
    };

    /**
     * @brief Parameter keys for `FunctionId::SetScanParams`.
     */
    enum class FunctionSetScanParamsParam {
        Param,
    };

    /**
     * @brief Parameter keys for `FunctionId::SetSoftApParams`.
     */
    enum class FunctionSetSoftApParamsParam {
        Param,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Item keys for `EventId::GeneralActionTriggered`.
     */
    enum class EventGeneralActionTriggeredParam {
        Action,
    };

    /**
     * @brief Item keys for `EventId::GeneralEventHappened`.
     */
    enum class EventGeneralEventHappenedParam {
        Event,
        IsUnexpected,
    };

    /**
     * @brief Item keys for `EventId::ScanStateChanged`.
     */
    enum class EventScanStateChangedParam {
        IsRunning,
    };

    /**
     * @brief Item keys for `EventId::ScanApInfosUpdated`.
     */
    enum class EventScanApInfosUpdatedParam {
        ApInfos,
    };

    /**
     * @brief Item keys for `EventId::SoftApEventHappened`.
     */
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
            .description = "Trigger a Wi-Fi general action.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionTriggerGeneralActionParam::Action),
                    .description = (boost::format("General action. Allowed values: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<GeneralAction>({
                        GeneralAction::Init, GeneralAction::Deinit, GeneralAction::Start, GeneralAction::Stop,
                        GeneralAction::Connect, GeneralAction::Disconnect
                    }))).str(),
                    .type = FunctionValueType::String
                }
            }
        };
    }

    static FunctionSchema function_schema_get_general_state()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetGeneralState),
            .description = (boost::format("Get current general state. Return type: string. Allowed values: %1%. "
                                          "Example: \"Connected\"")
            % BROOKESIA_DESCRIBE_TO_STR(std::vector<GeneralState>({
                GeneralState::Idle, GeneralState::Initing, GeneralState::Inited, GeneralState::Deiniting,
                GeneralState::Starting, GeneralState::Started, GeneralState::Stopping, GeneralState::Connecting,
                GeneralState::Connected, GeneralState::Disconnecting
            }))).str(),
        };
    }

    static FunctionSchema function_schema_set_connect_ap()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetConnectAp),
            .description = "Set target AP credentials.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetConnectApParam::SSID),
                    .description = "AP SSID.",
                    .type = FunctionValueType::String
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetConnectApParam::Password),
                    .description = "AP password (optional).",
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
            .description = (boost::format("Get target AP. Return type: JSON object. Example: %1%")
                            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(ConnectApInfo("ssid1", "password1"))).str(),
        };
    }

    static FunctionSchema function_schema_get_connected_aps()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetConnectedAps),
            .description = (boost::format("Get connected AP list. Return type: JSON array<object>. Example: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<ConnectApInfo>({
                ConnectApInfo("ssid1", "password1"), ConnectApInfo("ssid2", "password2")
            }))).str(),
        };
    }

    static FunctionSchema function_schema_set_scan_params()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetScanParams),
            .description = "Set scan parameters.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetScanParamsParam::Param),
                    .description = (boost::format("Scan parameters as a JSON object. Example: %1%")
                                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(ScanParams())).str(),
                    .type = FunctionValueType::Object
                }
            },
        };
    }

    static FunctionSchema function_schema_trigger_scan_start()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerScanStart),
            .description = "Start Wi-Fi scan.",
        };
    }

    static FunctionSchema function_schema_trigger_scan_stop()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerScanStop),
            .description = "Stop Wi-Fi scan.",
        };
    }

    static FunctionSchema function_schema_trigger_softap_start()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerSoftApStart),
            .description = "Start SoftAP.",
        };
    }

    static FunctionSchema function_schema_trigger_softap_stop()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerSoftApStop),
            .description = "Stop SoftAP.",
        };
    }

    static FunctionSchema function_schema_trigger_softap_provision_start()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerSoftApProvisionStart),
            .description = "Start SoftAP provisioning.",
        };
    }

    static FunctionSchema function_schema_trigger_softap_provision_stop()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::TriggerSoftApProvisionStop),
            .description = "Stop SoftAP provisioning.",
        };
    }

    static FunctionSchema function_schema_set_softap_params()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetSoftApParams),
            .description = "Set SoftAP parameters.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetSoftApParamsParam::Param),
                    .description = (boost::format("SoftAP parameters as a JSON object. Example: %1%")
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

    static FunctionSchema function_schema_get_softap_params()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetSoftApParams),
            .description = (boost::format("Get SoftAP parameters. Return type: JSON object. Example: %1%")
                            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(SoftApParams())).str(),
        };
    }

    static FunctionSchema function_schema_reset_data()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::ResetData),
            .description = "Reset Wi-Fi data, including target AP, scan parameters, and connected APs. "
            "Also clears NVS data.",
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static EventSchema event_schema_general_action_triggered()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::GeneralActionTriggered),
            .description = "Emitted when a general action is triggered.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventGeneralActionTriggeredParam::Action),
                    .description = (boost::format("General action. Allowed values: %1%")
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
            .description = "Emitted when a general event occurs.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventGeneralEventHappenedParam::Event),
                    .description = (boost::format("General event. Allowed values: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<GeneralEvent>({
                        GeneralEvent::Deinited, GeneralEvent::Inited, GeneralEvent::Stopped, GeneralEvent::Started,
                        GeneralEvent::Disconnected, GeneralEvent::Connected
                    }))).str(),
                    .type = EventItemType::String
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventGeneralEventHappenedParam::IsUnexpected),
                    .description = "Whether the event was unexpected.",
                    .type = EventItemType::Boolean
                }
            }
        };
    }

    static EventSchema event_schema_scan_state_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::ScanStateChanged),
            .description = "Emitted when scan state changes.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventScanStateChangedParam::IsRunning),
                    .description = "Whether scanning is running.",
                    .type = EventItemType::Boolean
                }
            }
        };
    }

    static EventSchema event_schema_scan_ap_infos_updated()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::ScanApInfosUpdated),
            .description = "Emitted when scan AP list is updated.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventScanApInfosUpdatedParam::ApInfos),
                    .description = (boost::format("Scanned AP list as a JSON array<object>. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<ConnectApInfo>({
                        ConnectApInfo("ssid1", "password1"),
                        ConnectApInfo("ssid2", "password2")
                    }))).str(),
                    .type = EventItemType::Array
                }
            }
        };
    }

    static EventSchema event_schema_softap_event_happened()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::SoftApEventHappened),
            .description = "Emitted when a SoftAP event occurs.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventSoftApEventHappenedParam::Event),
                    .description = (boost::format("SoftAP event. Allowed values: %1%")
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
    /**
     * @brief Get helper contract name.
     *
     * @return Constant service name string.
     */
    static constexpr std::string_view get_name()
    {
        return "Wifi";
    }

    /**
     * @brief Get all function schemas exposed by this helper.
     *
     * @return Read-only span of function schemas.
     */
    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> FUNCTION_SCHEMAS = {{
                function_schema_trigger_general_action(),
                function_schema_get_general_state(),
                function_schema_set_connect_ap(),
                function_schema_get_connect_ap(),
                function_schema_get_connected_aps(),
                function_schema_set_scan_params(),
                function_schema_trigger_scan_start(),
                function_schema_trigger_scan_stop(),
                function_schema_set_softap_params(),
                function_schema_get_softap_params(),
                function_schema_trigger_softap_start(),
                function_schema_trigger_softap_stop(),
                function_schema_trigger_softap_provision_start(),
                function_schema_trigger_softap_provision_stop(),
                function_schema_reset_data(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }

    /**
     * @brief Get all event schemas exposed by this helper.
     *
     * @return Read-only span of event schemas.
     */
    static std::span<const EventSchema> get_event_schemas()
    {
        static const std::array<EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EventId::Max)> EVENT_SCHEMAS = {{
                event_schema_general_action_triggered(),
                event_schema_general_event_happened(),
                event_schema_scan_state_changed(),
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
 * @brief  Connect related
 */
BROOKESIA_DESCRIBE_STRUCT(Wifi::ConnectApInfo, (), (ssid, password, is_connectable));

/**
 * @brief  Scan related
 */
BROOKESIA_DESCRIBE_ENUM(Wifi::ScanApSignalLevel, LEVEL_0, LEVEL_1, LEVEL_2, LEVEL_3, LEVEL_4);
BROOKESIA_DESCRIBE_STRUCT(Wifi::ScanApInfo, (), (ssid, is_locked, rssi, signal_level, channel));
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
    Wifi::FunctionId,
    TriggerGeneralAction, GetGeneralState, SetConnectAp, GetConnectAp, GetConnectedAps,
    SetScanParams, TriggerScanStart, TriggerScanStop,
    SetSoftApParams, GetSoftApParams, TriggerSoftApStart, TriggerSoftApStop,
    TriggerSoftApProvisionStart, TriggerSoftApProvisionStop, ResetData,
    Max
);
BROOKESIA_DESCRIBE_ENUM(Wifi::FunctionTriggerGeneralActionParam, Action);
BROOKESIA_DESCRIBE_ENUM(Wifi::FunctionSetScanParamsParam, Param);
BROOKESIA_DESCRIBE_ENUM(Wifi::FunctionSetSoftApParamsParam, Param);
BROOKESIA_DESCRIBE_ENUM(Wifi::FunctionSetConnectApParam, SSID, Password);

/**
 * @brief  Event related
 */
BROOKESIA_DESCRIBE_ENUM(
    Wifi::EventId, GeneralActionTriggered, GeneralEventHappened, ScanStateChanged, ScanApInfosUpdated,
    SoftApEventHappened, Max
);
BROOKESIA_DESCRIBE_ENUM(Wifi::EventGeneralActionTriggeredParam, Action);
BROOKESIA_DESCRIBE_ENUM(Wifi::EventGeneralEventHappenedParam, Event, IsUnexpected);
BROOKESIA_DESCRIBE_ENUM(Wifi::EventScanStateChangedParam, IsRunning);
BROOKESIA_DESCRIBE_ENUM(Wifi::EventScanApInfosUpdatedParam, ApInfos);
BROOKESIA_DESCRIBE_ENUM(Wifi::EventSoftApEventHappenedParam, Event);
} // namespace esp_brookesia::service::helper
