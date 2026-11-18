/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <span>

#include "brookesia/hal_interface/interfaces/wifi/types.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/detail/static_schema.hpp"
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
        GetConnectedSsid, ///< Current connected SSID, or an empty string when disconnected.
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

    /**
     * @brief Parameter keys for `FunctionId::TriggerGeneralAction`.
     */

    /**
     * @brief Parameter keys for `FunctionId::SetConnectAp`.
     */

    /**
     * @brief Parameter keys for `FunctionId::RemoveConnectedAp`.
     */

    /**
     * @brief Parameter keys for `FunctionId::SetScanParams`.
     */

    /**
     * @brief Parameter keys for `FunctionId::SetSoftApParams`.
     */

    /**
     * @brief Item keys for `EventId::GeneralActionTriggered`.
     */

    /**
     * @brief Item keys for `EventId::GeneralEventHappened`.
     */

    /**
     * @brief Item keys for `EventId::ScanStateChanged`.
     */

    /**
     * @brief Item keys for `EventId::ScanApInfosUpdated`.
     */

    /**
     * @brief Item keys for `EventId::SoftApEventHappened`.
     */

private:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function schemas /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    using DefaultValueKind = detail::static_schema::DefaultValueKind;
    using DefaultValueSpec = detail::static_schema::DefaultValueSpec;
    using FunctionParameterSpec = detail::static_schema::FunctionParameterSpec;
    using FunctionReturnSpec = detail::static_schema::FunctionReturnSpec;
    using FunctionSpec = detail::static_schema::FunctionSpec;

    inline static constexpr DefaultValueSpec EMPTY_STRING_DEFAULT = {
        .kind = DefaultValueKind::String,
        .string = "",
    };

    inline static constexpr std::array<FunctionParameterSpec, 1> TRIGGER_GENERAL_ACTION_PARAMETERS = {{
            {
                .name = "Action",
                .description = "General action. Allowed values: [Init, Deinit, Start, Stop, Connect, Disconnect]",
                .type = FunctionValueType::String,
            },
        }
    };

    inline static constexpr std::array<FunctionParameterSpec, 2> SET_CONNECT_AP_PARAMETERS = {{
            {
                .name = "SSID",
                .description = "AP SSID.",
                .type = FunctionValueType::String,
            },
            {
                .name = "Password",
                .description = "AP password (optional).",
                .type = FunctionValueType::String,
                .default_value = EMPTY_STRING_DEFAULT,
            },
        }
    };

    inline static constexpr std::array<FunctionParameterSpec, 1> REMOVE_CONNECTED_AP_PARAMETERS = {{
            {
                .name = "SSID",
                .description = "Saved AP SSID to remove.",
                .type = FunctionValueType::String,
            },
        }
    };

    inline static constexpr std::array<FunctionParameterSpec, 1> SET_SCAN_PARAMS_PARAMETERS = {{
            {
                .name = "Param",
                .description = R"(Scan parameters as a JSON object. Example: {"ap_count":20,"interval_ms":10000,)"
                R"("timeout_ms":60000})",
                .type = FunctionValueType::Object,
            },
        }
    };

    inline static constexpr std::array<FunctionParameterSpec, 1> SET_SOFTAP_PARAMS_PARAMETERS = {{
            {
                .name = "Param",
                .description = R"(SoftAP parameters as a JSON object. Example: {"ssid":"ssid","password":"password",)"
                R"("max_connection":4,"channel":1})",
                .type = FunctionValueType::Object,
            },
        }
    };

    inline static constexpr std::array<FunctionSpec, static_cast<size_t>(FunctionId::Max)> FUNCTION_SPECS = {{
            {
                .name = "TriggerGeneralAction",
                .description = "Trigger a Wi-Fi general action.",
                .parameters = TRIGGER_GENERAL_ACTION_PARAMETERS,
            },
            {
                .name = "GetGeneralState",
                .description = "Get current general state.",
                .parameters = std::span<const FunctionParameterSpec>{},
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::String,
                    .description =
                    "Allowed values: [Idle, Initing, Inited, Deiniting, Starting, Started, Stopping, "
                    "Connecting, Connected, Disconnecting]. Example: \"Connected\"",
                },
            },
            {
                .name = "SetConnectAp",
                .description = "Set target AP credentials.",
                .parameters = SET_CONNECT_AP_PARAMETERS,
            },
            {
                .name = "GetConnectAp",
                .description = "Get target AP.",
                .parameters = std::span<const FunctionParameterSpec>{},
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::Object,
                    .description = R"(Example: {"ssid":"ssid1","password":"password1","is_connectable":true})",
                },
            },
            {
                .name = "GetConnectedAps",
                .description = "Get connected AP list.",
                .parameters = std::span<const FunctionParameterSpec>{},
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::Array,
                    .description =
                    R"(Example: [{"ssid":"ssid1","password":"password1","is_connectable":true},)"
                    R"({"ssid":"ssid2","password":"password2","is_connectable":true}])",
                },
            },
            {
                .name = "RemoveConnectedAp",
                .description = "Remove a saved AP from the connected AP list and persisted Wi-Fi data.",
                .parameters = REMOVE_CONNECTED_AP_PARAMETERS,
            },
            {
                .name = "SetScanParams",
                .description = "Set scan parameters.",
                .parameters = SET_SCAN_PARAMS_PARAMETERS,
            },
            {
                .name = "TriggerScanStart",
                .description = "Start Wi-Fi scan.",
                .parameters = std::span<const FunctionParameterSpec>{},
            },
            {
                .name = "TriggerScanStop",
                .description = "Stop Wi-Fi scan.",
                .parameters = std::span<const FunctionParameterSpec>{},
            },
            {
                .name = "SetSoftApParams",
                .description = "Set SoftAP parameters.",
                .parameters = SET_SOFTAP_PARAMS_PARAMETERS,
            },
            {
                .name = "GetSoftApParams",
                .description = "Get SoftAP parameters.",
                .parameters = std::span<const FunctionParameterSpec>{},
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::Object,
                    .description = R"(Example: {"ssid":"","password":"","max_connection":4,"channel":null})",
                },
            },
            {
                .name = "TriggerSoftApStart",
                .description = "Start SoftAP.",
                .parameters = std::span<const FunctionParameterSpec>{},
            },
            {
                .name = "TriggerSoftApStop",
                .description = "Stop SoftAP.",
                .parameters = std::span<const FunctionParameterSpec>{},
            },
            {
                .name = "TriggerSoftApProvisionStart",
                .description = "Start SoftAP provisioning.",
                .parameters = std::span<const FunctionParameterSpec>{},
            },
            {
                .name = "TriggerSoftApProvisionStop",
                .description = "Stop SoftAP provisioning.",
                .parameters = std::span<const FunctionParameterSpec>{},
            },
            {
                .name = "LoadData",
                .description = "Load persisted Wi-Fi data.",
                .parameters = std::span<const FunctionParameterSpec>{},
            },
            {
                .name = "ResetData",
                .description = "Reset Wi-Fi data, including target AP, scan parameters, and connected APs. "
                "Also clears Storage data.",
                .parameters = std::span<const FunctionParameterSpec>{},
            },
            {
                .name = "GetConnectedSsid",
                .description = "Get the established connection's SSID, independently of the next connection target.",
                .parameters = std::span<const FunctionParameterSpec>{},
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::String,
                    .description = "Connected SSID, or an empty string when not connected. Contains no credentials.",
                },
            },
        }
    };
    static_assert(FUNCTION_SPECS.size() == static_cast<size_t>(FunctionId::Max));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    using EventItemSpec = detail::static_schema::EventItemSpec;
    using EventSpec = detail::static_schema::EventSpec;

    inline static constexpr std::array<EventItemSpec, 1> GENERAL_ACTION_TRIGGERED_ITEMS = {{
            {
                .name = "Action",
                .description = "General action. Allowed values: [Init, Deinit, Start, Stop, Connect, Disconnect]",
                .type = EventItemType::String,
            },
        }
    };

    inline static constexpr std::array<EventItemSpec, 2> GENERAL_EVENT_HAPPENED_ITEMS = {{
            {
                .name = "Event",
                .description = "General event. Allowed values: [Deinited, Inited, Stopped, Started, Disconnected, "
                "Connected]",
                .type = EventItemType::String,
            },
            {
                .name = "IsUnexpected",
                .description = "Whether the event was unexpected.",
                .type = EventItemType::Boolean,
            },
        }
    };

    inline static constexpr std::array<EventItemSpec, 1> SCAN_STATE_CHANGED_ITEMS = {{
            {
                .name = "IsRunning",
                .description = "Whether scanning is running.",
                .type = EventItemType::Boolean,
            },
        }
    };

    inline static constexpr std::array<EventItemSpec, 1> SCAN_AP_INFOS_UPDATED_ITEMS = {{
            {
                .name = "ApInfos",
                .description =
                R"(Scanned AP list as a JSON array<object>. Example: [{"ssid":"ssid1","is_locked":true,)"
                R"("rssi":-45,"signal_level":"LEVEL_4","channel":1},{"ssid":"ssid2","is_locked":false,)"
                R"("rssi":-65,"signal_level":"LEVEL_2","channel":6}])",
                .type = EventItemType::Array,
            },
        }
    };

    inline static constexpr std::array<EventItemSpec, 1> SOFTAP_EVENT_HAPPENED_ITEMS = {{
            {
                .name = "Event",
                .description = "SoftAP event. Allowed values: [Started, Stopped]",
                .type = EventItemType::String,
            },
        }
    };

    inline static constexpr std::array<EventSpec, static_cast<size_t>(EventId::Max)> EVENT_SPECS = {{
            {
                .name = "GeneralActionTriggered",
                .description = "Emitted when a general action is triggered.",
                .items = GENERAL_ACTION_TRIGGERED_ITEMS,
            },
            {
                .name = "GeneralEventHappened",
                .description = "Emitted when a general event occurs.",
                .items = GENERAL_EVENT_HAPPENED_ITEMS,
            },
            {
                .name = "ScanStateChanged",
                .description = "Emitted when scan state changes.",
                .items = SCAN_STATE_CHANGED_ITEMS,
            },
            {
                .name = "ScanApInfosUpdated",
                .description = "Emitted when scan AP list is updated.",
                .items = SCAN_AP_INFOS_UPDATED_ITEMS,
            },
            {
                .name = "SoftApEventHappened",
                .description = "Emitted when a SoftAP event occurs.",
                .items = SOFTAP_EVENT_HAPPENED_ITEMS,
            },
        }
    };
    static_assert(EVENT_SPECS.size() == static_cast<size_t>(EventId::Max));

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
        static std::array<FunctionSchema, FUNCTION_SPECS.size()> schemas;
        static const bool initialized = [] {
            detail::static_schema::materialize_function_schemas(FUNCTION_SPECS, schemas);
            return true;
        }();
        static_cast<void>(initialized);
        return schemas;
    }

    /**
     * @brief Get all event schemas exposed by this helper.
     *
     * @return Read-only span of event schemas.
     */
    static std::span<const EventSchema> get_event_schemas()
    {
        static std::array<EventSchema, EVENT_SPECS.size()> schemas;
        static const bool initialized = [] {
            detail::static_schema::materialize_event_schemas(EVENT_SPECS, schemas);
            return true;
        }();
        static_cast<void>(initialized);
        return schemas;
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
    TriggerSoftApProvisionStart, TriggerSoftApProvisionStop, LoadData, ResetData, GetConnectedSsid,
    Max
);

/**
 * @brief  Event related
 */
BROOKESIA_DESCRIBE_ENUM(
    Wifi::EventId, GeneralActionTriggered, GeneralEventHappened, ScanStateChanged, ScanApInfosUpdated,
    SoftApEventHappened, Max
);
} // namespace esp_brookesia::service::helper
