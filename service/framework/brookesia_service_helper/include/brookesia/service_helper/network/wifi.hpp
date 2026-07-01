/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <span>
#include <vector>
#include "brookesia/hal_interface/interfaces/wifi/types.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/helper/base.hpp"

namespace esp_brookesia::service::helper {

/**
 * @brief Helper schema definitions for the Wi-Fi service.
 */
class Wifi: public Base<Wifi> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    using GeneralAction = hal::wifi::GeneralAction;
    using GeneralEvent = hal::wifi::GeneralEvent;

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

    using ConnectApInfo = hal::wifi::ConnectApInfo;
    using ScanParams = hal::wifi::ScanParams;
    using ScanApSignalLevel = hal::wifi::ScanApSignalLevel;
    using ScanApInfo = hal::wifi::ScanApInfo;
    using SoftApParams = hal::wifi::SoftApParams;
    using SoftApEvent = hal::wifi::SoftApEvent;

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
        RemoveConnectedAp,
        SetScanParams,
        TriggerScanStart,
        TriggerScanStop,
        SetSoftApParams,
        GetSoftApParams,
        TriggerSoftApStart,
        TriggerSoftApStop,
        TriggerSoftApProvisionStart,
        TriggerSoftApProvisionStop,
        LoadData,
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
     * @brief Parameter keys for `FunctionId::RemoveConnectedAp`.
     */
    enum class FunctionRemoveConnectedApParam {
        SSID,
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
            .description = "Get current general state.",
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::String,
                .description = (boost::format("Allowed values: %1%. Example: \"Connected\"")
                % BROOKESIA_DESCRIBE_TO_STR(std::vector<GeneralState>({
                    GeneralState::Idle, GeneralState::Initing, GeneralState::Inited, GeneralState::Deiniting,
                    GeneralState::Starting, GeneralState::Started, GeneralState::Stopping, GeneralState::Connecting,
                    GeneralState::Connected, GeneralState::Disconnecting
                }))).str(),
            },
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
            .description = "Get target AP.",
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Object,
                .description = (boost::format("Example: %1%")
                                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(ConnectApInfo("ssid1", "password1"))).str(),
            },
        };
    }

    static FunctionSchema function_schema_get_connected_aps()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetConnectedAps),
            .description = "Get connected AP list.",
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Array,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<ConnectApInfo>({
                    ConnectApInfo("ssid1", "password1"), ConnectApInfo("ssid2", "password2")
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_remove_connected_ap()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::RemoveConnectedAp),
            .description = "Remove a saved AP from the connected AP list and persisted Wi-Fi data.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionRemoveConnectedApParam::SSID),
                    .description = "Saved AP SSID to remove.",
                    .type = FunctionValueType::String
                }
            }
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
            .description = "Get SoftAP parameters.",
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Object,
                .description = (boost::format("Example: %1%")
                                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(SoftApParams())).str(),
            },
        };
    }

    static FunctionSchema function_schema_reset_data()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::ResetData),
            .description = "Reset Wi-Fi data, including target AP, scan parameters, and connected APs. "
            "Also clears Storage data.",
        };
    }

    static FunctionSchema function_schema_load_data()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::LoadData),
            .description = "Load persisted Wi-Fi data.",
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
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<ScanApInfo>({
                        {
                            .ssid = "ssid1",
                            .is_locked = true,
                            .rssi = -45,
                            .signal_level = ScanApSignalLevel::LEVEL_4,
                            .channel = 1,
                        },
                        {
                            .ssid = "ssid2",
                            .is_locked = false,
                            .rssi = -65,
                            .signal_level = ScanApSignalLevel::LEVEL_2,
                            .channel = 6,
                        },
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
                function_schema_remove_connected_ap(),
                function_schema_set_scan_params(),
                function_schema_trigger_scan_start(),
                function_schema_trigger_scan_stop(),
                function_schema_set_softap_params(),
                function_schema_get_softap_params(),
                function_schema_trigger_softap_start(),
                function_schema_trigger_softap_stop(),
                function_schema_trigger_softap_provision_start(),
                function_schema_trigger_softap_provision_stop(),
                function_schema_load_data(),
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
BROOKESIA_DESCRIBE_ENUM(
    Wifi::GeneralState, Idle, Initing, Inited, Deiniting, Starting, Started, Stopping, Connecting, Connected,
    Disconnecting, Max
);

/**
 * @brief  Function related
 */
BROOKESIA_DESCRIBE_ENUM(
    Wifi::FunctionId,
    TriggerGeneralAction, GetGeneralState, SetConnectAp, GetConnectAp, GetConnectedAps,
    RemoveConnectedAp, SetScanParams, TriggerScanStart, TriggerScanStop,
    SetSoftApParams, GetSoftApParams, TriggerSoftApStart, TriggerSoftApStop,
    TriggerSoftApProvisionStart, TriggerSoftApProvisionStop, LoadData, ResetData,
    Max
);
BROOKESIA_DESCRIBE_ENUM(Wifi::FunctionTriggerGeneralActionParam, Action);
BROOKESIA_DESCRIBE_ENUM(Wifi::FunctionSetScanParamsParam, Param);
BROOKESIA_DESCRIBE_ENUM(Wifi::FunctionSetSoftApParamsParam, Param);
BROOKESIA_DESCRIBE_ENUM(Wifi::FunctionSetConnectApParam, SSID, Password);
BROOKESIA_DESCRIBE_ENUM(Wifi::FunctionRemoveConnectedApParam, SSID);

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
