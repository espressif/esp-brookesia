/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/service_wifi/macro_configs.h"
#if !BROOKESIA_SERVICE_WIFI_SERVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_wifi/service_wifi.hpp"
#include "brookesia/service_manager.hpp"

namespace esp_brookesia::service::wifi {

constexpr uint32_t RECONNECT_DELAY_MS = 1000;

constexpr const char *NVS_NAMESPACE = helper::Wifi::SERVICE_NAME;
constexpr const char *NVS_LAST_CONNECTED_AP_INFO_OBJECT_KEY = "last_ap";
constexpr const char *NVS_CONNECTED_AP_INFO_LIST_ARRAY_KEY = "connected_aps";
constexpr const char *NVS_SCAN_PARAMS_OBJECT_KEY = "scan_params";
constexpr uint32_t NVS_CALL_TIMEOUT_MS = 100;

static auto &service_manager = ServiceManager::get_instance();
static const FunctionSchema *NVS_FUNCTION_DEFINITIONS = helper::NVS::get_function_definitions();

bool Wifi::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    task_scheduler_ = get_task_scheduler();

    /* Create and initialize wifi context */
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        hal_ = std::make_shared<Hal>(task_scheduler_), false, "Failed to create Hal"
    );
    BROOKESIA_CHECK_FALSE_RETURN(hal_->init(), false, "Failed to initialize wifi context");
    /* Register wifi general event callback */
    auto general_event_callback =
    [this](GeneralEvent event, const GeneralStateFlags & old_flags, const GeneralStateFlags & new_flags) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD(
            "Params: event(%1%), old_flags(%2%), new_flags(%3%)",
            BROOKESIA_DESCRIBE_TO_STR(event), BROOKESIA_DESCRIBE_TO_STR(old_flags),
            BROOKESIA_DESCRIBE_TO_STR(new_flags)
        );

        GeneralStateFlagBit running_state = GeneralStateFlagBit::Max;
        switch (event) {
        case GeneralEvent::Started: {
            running_state = GeneralStateFlagBit::Starting;
            BROOKESIA_LOGD("WiFi is started, try to connect to last connectable AP");
            // Get connectable AP info
            ConnectApInfo connectable_ap_info = hal_->get_target_connect_ap_info();
            if (!connectable_ap_info.is_connectable) {
                BROOKESIA_LOGD("Target connect AP is not connectable, try to get historical one");
                ConnectApInfo connected_ap_info;
                if (hal_->get_last_connectable_ap_info(connected_ap_info)) {
                    connectable_ap_info = connected_ap_info;
                    hal_->set_target_connect_ap_info(connectable_ap_info);
                } else {
                    BROOKESIA_LOGW("No connectable AP found, skip auto connect");
                    break;
                }
            }
            // Check if the connectable AP info is empty
            if (connectable_ap_info.ssid.empty() || !connectable_ap_info.is_connectable) {
                BROOKESIA_LOGD("No connectable AP info, skip connect");
                break;
            } else {
                BROOKESIA_LOGD("Connectable AP is found: %1%", BROOKESIA_DESCRIBE_TO_STR(connectable_ap_info));
            }
            // Trigger state machine connect action in a new task to avoid deadlock
            auto connect_task = [this]() {
                BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
                BROOKESIA_CHECK_FALSE_EXIT(
                    state_machine_->trigger_general_action(GeneralAction::Connect),
                    "Failed to trigger connect general action"
                );
            };
            BROOKESIA_CHECK_FALSE_EXECUTE(
            task_scheduler_->post(std::move(connect_task), nullptr, Hal::GENERAL_CALLBACK_GROUP), {}, {
                BROOKESIA_LOGE("Failed to post connect task");
            });
            break;
        }
        case GeneralEvent::Connected: {
            running_state = GeneralStateFlagBit::Connecting;
            // Update the last connected AP info
            ConnectApInfo connecting_ap_info = hal_->get_connecting_ap_info();
            ConnectApInfo last_connected_ap_info = hal_->get_last_connected_ap_info();
            if (connecting_ap_info != last_connected_ap_info) {
                hal_->set_last_connected_ap_info(connecting_ap_info);
                try_save_last_connected_ap_info_to_nvs();
            } else {
                BROOKESIA_LOGD("Connecting AP is the same as the last connected AP, skip");
            }
            // Update the connected AP info list
            if (!hal_->has_connected_ap_info(connecting_ap_info)) {
                hal_->add_connected_ap_info(connecting_ap_info);
                try_save_connected_ap_info_list_to_nvs();
            } else {
                BROOKESIA_LOGD("Connecting AP is already in the connected AP info list, skip");
            }
            break;
        }
        case GeneralEvent::Disconnected: {
            // If the WiFi is deinitializing or stopping, skip to avoid duplicate triggering
            if (new_flags.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralStateFlagBit::Deiniting)) ||
                    new_flags.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralStateFlagBit::Stopping))) {
                BROOKESIA_LOGD("WiFi is deinitializing or stopping, skip");
                break;
            }
            if (!new_flags.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralStateFlagBit::Connecting))) {
                BROOKESIA_LOGD("WiFi is not connecting, take it as a unexpected event");
                running_state = GeneralStateFlagBit::Disconnecting;
            }
            // Get connecting AP info
            ConnectApInfo connecting_ap_info = hal_->get_connecting_ap_info();
            // Mark the target AP info as not connectable if it is the same as the connecting AP info
            ConnectApInfo target_ap_info = hal_->get_target_connect_ap_info();
            if (target_ap_info == connecting_ap_info) {
                BROOKESIA_LOGD("Mark the target AP info as not connectable");
                target_ap_info.is_connectable = false;
                hal_->set_target_connect_ap_info(target_ap_info);
            }
            // If the disconnecting action is in progress, skip to avoid duplicate triggering
            if (new_flags.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralStateFlagBit::Disconnecting))) {
                BROOKESIA_LOGD("Disconnecting action is in progress, skip");
                break;
            }
            // Mark the connected AP info as not connectable
            ConnectApInfo last_connected_ap_info = hal_->get_last_connected_ap_info();
            if ((last_connected_ap_info.ssid == connecting_ap_info.ssid) &&
                    (last_connected_ap_info.password == connecting_ap_info.password) &&
                    (last_connected_ap_info.is_connectable)) {
                BROOKESIA_LOGD("Mark the last connected AP info as not connectable");
                last_connected_ap_info.is_connectable = false;
                hal_->set_last_connected_ap_info(last_connected_ap_info);
                try_save_last_connected_ap_info_to_nvs();
            }
            ConnectApInfo connected_ap_info;
            if (hal_->get_connectable_ap_info_by_ssid(connecting_ap_info.ssid, connected_ap_info)) {
                BROOKESIA_LOGD("Mark the connected AP info as not connectable");
                connected_ap_info.is_connectable = false;
                hal_->add_connected_ap_info(connected_ap_info);
                try_save_connected_ap_info_list_to_nvs();
            }
            // Trigger state machine reconnect action in a new task to avoid deadlock
            auto reconnect_task = [this]() {
                BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
                // Try to connect a history connectable AP
                ConnectApInfo history_connectable_ap_info;
                if (hal_->get_last_connectable_ap_info(history_connectable_ap_info)) {
                    BROOKESIA_LOGD(
                        "History connectable AP is found: %1%", BROOKESIA_DESCRIBE_TO_STR(history_connectable_ap_info)
                    );
                    hal_->set_target_connect_ap_info(history_connectable_ap_info);
                    BROOKESIA_CHECK_FALSE_EXIT(
                        state_machine_->trigger_general_action(GeneralAction::Connect),
                        "Failed to trigger connect history connectable AP general action"
                    );
                } else {
                    BROOKESIA_LOGD("No history connectable AP found, skip auto reconnect");
                }
            };
            // Post a task to avoid deadlock
            BROOKESIA_CHECK_FALSE_EXECUTE(
            task_scheduler_->post_delayed(std::move(reconnect_task), RECONNECT_DELAY_MS), {}, {
                BROOKESIA_LOGE("Failed to post reconnect task");
            }
            );
            break;
        }
        default:
            BROOKESIA_LOGD("Ignored");
            break;
        }

        /* When unexpected event happens, synchronize the state machine to the current state of HAL */
        if ((running_state != GeneralStateFlagBit::Max) &&
                !new_flags.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(running_state))) {
            auto target_state = get_target_event_state(event);
            BROOKESIA_LOGW(
                "Detected unexpected '%1%' event, force transition to '%2%' state immediately",
                BROOKESIA_DESCRIBE_TO_STR(event), target_state
            );
            // Trigger state machine force transition to target state action in a new task to avoid deadlock
            auto force_transition_task = [this, target_state]() {
                BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
                BROOKESIA_CHECK_FALSE_EXIT(
                    state_machine_->force_transition_to(target_state),
                    "Failed to force transition to '%1%' state", target_state
                );
            };
            BROOKESIA_CHECK_FALSE_EXECUTE(
            task_scheduler_->post(std::move(force_transition_task), nullptr, Hal::GENERAL_CALLBACK_GROUP), {}, {
                BROOKESIA_LOGE("Failed to post force transition task");
            });
        }

        /* Publish general event happened event */
        if (hal_->is_general_event_changed(event, old_flags, new_flags)) {
            BROOKESIA_CHECK_FALSE_EXECUTE(publish_event(EVENT_DEFINITIONS[Helper::EventIndexGeneralEventHappened].name, {
                BROOKESIA_DESCRIBE_TO_STR(event)
            }), {}, {
                BROOKESIA_LOGE("Failed to publish general event happened event");
            });
        } else {
            BROOKESIA_LOGD("Event is not updated, skip publish");
        }
    };
    hal_->register_general_event_callback(std::move(general_event_callback));
    /* Register general action callback */
    auto general_action_callback = [this](GeneralAction action) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD("Params: action(%1%)", BROOKESIA_DESCRIBE_TO_STR(action));

        /* Publish general action triggered event */
        BROOKESIA_CHECK_FALSE_EXECUTE(publish_event(EVENT_DEFINITIONS[Helper::EventIndexGeneralActionTriggered].name, {
            BROOKESIA_DESCRIBE_TO_STR(action)
        }), {}, {
            BROOKESIA_LOGE("Failed to publish general action triggered event");
        });
    };
    hal_->register_general_action_callback(std::move(general_action_callback));
    /* Register scan AP infos updated callback */
    auto scan_ap_infos_updated_callback = [this](const boost::json::array & ap_infos) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD("Params: ap_infos(%1%)", boost::json::serialize(ap_infos));

        /* Publish scan infos updated event */
        BROOKESIA_CHECK_FALSE_EXECUTE(publish_event(EVENT_DEFINITIONS[Helper::EventIndexScanApInfosUpdated].name, {
            {ap_infos}
        }), {}, {
            BROOKESIA_LOGE("Failed to publish scan infos updated event");
        });

        /* Check if connected AP appeared in the scan infos */
        // If already connecting or connected, skip
        if (hal_->is_general_action_running(GeneralAction::Connect) ||
                hal_->is_general_event_ready(GeneralEvent::Connected)) {
            return;
        }
        // Find first connectable AP in scan infos
        bool is_connectable = false;
        for (const auto &record : ap_infos) {
            if (!record.is_object()) {
                continue;
            }
            const auto &obj = record.as_object();
            auto ssid_it = obj.find("ssid");
            if (ssid_it == obj.end() || !ssid_it->value().is_string()) {
                continue;
            }
            std::string ssid = std::string(ssid_it->value().as_string());

            // Get connectable AP info
            ConnectApInfo ap_info;
            ap_info = hal_->get_target_connect_ap_info();
            if (ap_info.ssid == ssid) {
                if (ap_info.is_connectable) {
                    BROOKESIA_LOGD("Target AP is connectable, connect to it");
                    is_connectable = true;
                    break;
                } else {
                    BROOKESIA_LOGD("Target AP is not connectable, skip");
                }
            } else if (hal_->get_connectable_ap_info_by_ssid(ssid, ap_info)) {
                BROOKESIA_LOGD("Connect to the history connectable AP");
                hal_->set_target_connect_ap_info(ap_info);
                is_connectable = true;
                break;
            }
        }
        if (is_connectable) {
            BROOKESIA_LOGD("Detected connectable AP, trigger connect action");
            // Trigger action in a new task to avoid deadlock
            auto connect_task = [this]() {
                BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
                BROOKESIA_CHECK_FALSE_EXIT(
                    state_machine_->trigger_general_action(GeneralAction::Connect),
                    "Failed to trigger connect general action"
                );
            };
            BROOKESIA_CHECK_FALSE_EXECUTE(task_scheduler_->post(std::move(connect_task)), {}, {
                BROOKESIA_LOGE("Failed to post connect task");
            });
        }
    };
    hal_->register_scan_ap_infos_updated_callback(std::move(scan_ap_infos_updated_callback));

    /* Create and initialize state machine */
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        state_machine_ = std::make_unique<StateMachine>(task_scheduler_, hal_), false,
        "Failed to create StateMachine"
    );
    BROOKESIA_CHECK_FALSE_RETURN(state_machine_->init(), false, "Failed to initialize state machine");

    return true;
}

void Wifi::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    state_machine_.reset();
    hal_.reset();
    task_scheduler_.reset();
}

bool Wifi::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(hal_->start(), false, "Failed to start HAL");
    BROOKESIA_CHECK_FALSE_RETURN(state_machine_->start(), false, "Failed to start state machine");

    // Try to load data from NVS, if NVS is not valid, skip
    try_load_from_nvs();

    return true;
}

void Wifi::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    /* Stop the state machine first, because the state machine is waiting for the wifi_event signal */
    state_machine_->stop();
    /* Reset the wifi context */
    hal_->stop();
    /* Stop the task scheduler last, because wifi_event signals are processed in the task scheduler,
     * and hal and wifi_state_machine may be waiting for wifi_event signals */
    // task_scheduler_ will be stopped automatically
}

std::expected<void, std::string> Wifi::function_trigger_general_action(const std::string &action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: action(%1%)", action);

    // Parse enum string
    GeneralAction target_action;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM(action, target_action)) {
        return std::unexpected("Invalid action: " + action);
    }

    // Trigger target action
    if (!state_machine_->trigger_general_action(target_action)) {
        return std::unexpected("Failed to trigger target action: " + BROOKESIA_DESCRIBE_TO_STR(action));
    }

    return {};
}

std::expected<void, std::string> Wifi::function_trigger_scan_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (hal_->is_general_event_ready(GeneralEvent::Stopped)) {
        BROOKESIA_LOGD("WiFi is stopped, trigger start first");
        if (!state_machine_->trigger_general_action(GeneralAction::Start)) {
            return std::unexpected("Failed to trigger start general action");
        }
    }

    if (!hal_->start_ap_scan()) {
        return std::unexpected("Failed to start AP scan");
    }

    return {};
}

std::expected<void, std::string> Wifi::function_trigger_scan_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    hal_->stop_ap_scan();

    return {};
}

std::expected<void, std::string> Wifi::function_set_scan_params(double ap_count, double interval_ms, double timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: ap_count(%1%), interval_ms(%2%), timeout_ms(%3%)", ap_count, interval_ms, timeout_ms);

    ScanParams old_params = hal_->get_scan_params();
    auto new_params = ScanParams{
        .ap_count = static_cast<size_t>(ap_count),
        .interval_ms = static_cast<uint32_t>(interval_ms),
        .timeout_ms = static_cast<uint32_t>(timeout_ms)
    };
    if (old_params == new_params) {
        BROOKESIA_LOGD("Scan params are the same, skip");
        return {};
    }

    if (!hal_->set_scan_params(new_params)) {
        return std::unexpected("Failed to set scan params: " + BROOKESIA_DESCRIBE_TO_STR(new_params));
    }
    try_save_scan_params_to_nvs();

    return {};
}

std::expected<void, std::string> Wifi::function_set_connect_ap(const std::string &ssid, const std::string &password)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: ssid(%1%), password(%2%)", ssid, password);

    ConnectApInfo old_info;
    old_info = hal_->get_target_connect_ap_info();
    ConnectApInfo new_info(ssid, password);

    if (old_info == new_info) {
        BROOKESIA_LOGD("Connect AP info is the same, skip");
        return {};
    }

    if (!hal_->set_target_connect_ap_info(new_info)) {
        return std::unexpected("Failed to set connect AP info: " + BROOKESIA_DESCRIBE_TO_STR(new_info));
    }

    return {};
}

std::expected<std::string, std::string> Wifi::function_get_connect_ap()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ConnectApInfo ap_info;
    ap_info = hal_->get_target_connect_ap_info();

    return ap_info.ssid;
}

std::expected<boost::json::array, std::string> Wifi::function_get_connected_aps()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::vector<ConnectApInfo> ap_infos;
    hal_->get_connected_ap_infos(ap_infos);

    boost::json::array array;
    for (const auto &ap_info : ap_infos) {
        array.push_back(boost::json::string(ap_info.ssid));
    }

    return array;
}

std::string Wifi::get_target_event_state(GeneralEvent event)
{
    switch (event) {
    case GeneralEvent::Started:
        return BROOKESIA_DESCRIBE_TO_STR(GeneralState::Started);
    case GeneralEvent::Connected:
        return BROOKESIA_DESCRIBE_TO_STR(GeneralState::Connected);
    case GeneralEvent::Disconnected:
        return BROOKESIA_DESCRIBE_TO_STR(GeneralState::Started);
    default:
        return BROOKESIA_DESCRIBE_TO_STR(GeneralState::Max);
    }
}

bool Wifi::is_nvs_valid()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto nvs_service = ServiceManager::get_instance().get_service(helper::NVS::SERVICE_NAME);

    return (nvs_service != nullptr);
}

void Wifi::try_load_from_nvs()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_EXIT(is_initialized(), "Not initialized");

    if (!is_nvs_valid()) {
        BROOKESIA_LOGD("NVS is not valid, skip");
        return;
    }

    auto binding = service_manager.bind(helper::NVS::SERVICE_NAME);
    BROOKESIA_CHECK_FALSE_EXIT(binding.is_valid(), "Failed to bind NVS service");

    auto nvs = binding.get_service();
    BROOKESIA_CHECK_NULL_EXIT(nvs, "Failed to get NVS service");

    auto result = nvs->call_function_sync(NVS_FUNCTION_DEFINITIONS[helper::NVS::FunctionIndexGet].name,
    FunctionParameterMap{
        {NVS_FUNCTION_DEFINITIONS[helper::NVS::FunctionIndexGet].parameters[0].name, NVS_NAMESPACE}
    }, NVS_CALL_TIMEOUT_MS);
    if (!result.success) {
        BROOKESIA_LOGW("Failed to get NVS data: %1%", result.error_message);
        return;
    }

    auto data_obj = std::get_if<boost::json::object>(&result.data.value());
    BROOKESIA_CHECK_NULL_EXIT(data_obj, "Invalid NVS data");

    auto load_last_connected_ap_info = [&]() {
        if (!data_obj->contains(NVS_LAST_CONNECTED_AP_INFO_OBJECT_KEY)) {
            return;
        }

        auto value_json = data_obj->at(NVS_LAST_CONNECTED_AP_INFO_OBJECT_KEY);
        BROOKESIA_CHECK_FALSE_EXIT(value_json.is_string(), "Invalid last connected AP info");

        auto value_str = std::string(value_json.as_string());
        ConnectApInfo ap_info;
        BROOKESIA_CHECK_FALSE_EXIT(
            BROOKESIA_DESCRIBE_JSON_DESERIALIZE(value_str, ap_info),
            "Failed to deserialize last connected AP info from: %1%", value_str
        );

        hal_->set_target_connect_ap_info(ap_info);
        hal_->set_last_connected_ap_info(ap_info);
    };
    auto load_connected_ap_info_list = [&]() {
        if (!data_obj->contains(NVS_CONNECTED_AP_INFO_LIST_ARRAY_KEY)) {
            return;
        }

        auto value_json = data_obj->at(NVS_CONNECTED_AP_INFO_LIST_ARRAY_KEY);
        BROOKESIA_CHECK_FALSE_EXIT(value_json.is_string(), "Invalid connected AP info list");

        auto value_str = std::string(value_json.as_string());
        std::vector<ConnectApInfo> ap_infos;
        BROOKESIA_CHECK_FALSE_EXIT(
            BROOKESIA_DESCRIBE_JSON_DESERIALIZE(value_str, ap_infos),
            "Failed to deserialize connected AP info list from: %1%", value_str
        );

        for (const auto &ap_info : ap_infos) {
            hal_->add_connected_ap_info(ap_info);
        }
    };
    auto load_scan_params = [&]() {
        if (!data_obj->contains(NVS_SCAN_PARAMS_OBJECT_KEY)) {
            return;
        }

        auto value_json = data_obj->at(NVS_SCAN_PARAMS_OBJECT_KEY);
        BROOKESIA_CHECK_FALSE_EXIT(value_json.is_string(), "Invalid scan params");

        auto value_str = std::string(value_json.as_string());
        ScanParams params;
        BROOKESIA_CHECK_FALSE_EXIT(
            BROOKESIA_DESCRIBE_JSON_DESERIALIZE(value_str, params),
            "Failed to deserialize scan params from: %1%", value_str
        );

        hal_->set_scan_params(params);
    };

    load_last_connected_ap_info();
    load_connected_ap_info_list();
    load_scan_params();

    BROOKESIA_LOGI("Loaded data from NVS");
}

template <typename T>
static bool save_data_to_nvs(const T &data, const char *key)
{
    auto binding = service_manager.bind(helper::NVS::SERVICE_NAME);
    BROOKESIA_CHECK_FALSE_RETURN(binding.is_valid(), false, "Failed to bind NVS service");

    auto nvs = binding.get_service();
    BROOKESIA_CHECK_NULL_RETURN(nvs, false, "Failed to get NVS service");

    boost::json::object data_object{{key, std::move(BROOKESIA_DESCRIBE_JSON_SERIALIZE(data))}};
    auto result = nvs->call_function_sync(NVS_FUNCTION_DEFINITIONS[helper::NVS::FunctionIndexSet].name,
    FunctionParameterMap{
        {NVS_FUNCTION_DEFINITIONS[helper::NVS::FunctionIndexSet].parameters[0].name, NVS_NAMESPACE},
        {NVS_FUNCTION_DEFINITIONS[helper::NVS::FunctionIndexSet].parameters[1].name, std::move(data_object)}
    }, NVS_CALL_TIMEOUT_MS);
    BROOKESIA_CHECK_FALSE_RETURN(
        result.success, false, "Failed to save %1% to NVS: %2%", key, result.error_message
    );

    return true;
}

void Wifi::try_save_last_connected_ap_info_to_nvs()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_nvs_valid()) {
        BROOKESIA_LOGD("NVS is not valid, skip");
        return;
    }

    ConnectApInfo ap_info = hal_->get_last_connected_ap_info();
    task_scheduler_->post([this, ap_info = std::move(ap_info)]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_CHECK_FALSE_EXIT(
            save_data_to_nvs(ap_info, NVS_LAST_CONNECTED_AP_INFO_OBJECT_KEY),
            "Failed to save last connected AP info to NVS"
        );
    });
}

void Wifi::try_save_connected_ap_info_list_to_nvs()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_nvs_valid()) {
        BROOKESIA_LOGD("NVS is not valid, skip");
        return;
    }

    std::vector<ConnectApInfo> ap_infos;
    hal_->get_connected_ap_infos(ap_infos);
    task_scheduler_->post([this, ap_infos = std::move(ap_infos)]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_CHECK_FALSE_EXIT(
            save_data_to_nvs(ap_infos, NVS_CONNECTED_AP_INFO_LIST_ARRAY_KEY),
            "Failed to save connected AP info list to NVS"
        );
    });
}

void Wifi::try_save_scan_params_to_nvs()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_nvs_valid()) {
        BROOKESIA_LOGD("NVS is not valid, skip");
        return;
    }

    ScanParams params = hal_->get_scan_params();
    task_scheduler_->post([this, params = std::move(params)]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_CHECK_FALSE_EXIT(
            save_data_to_nvs(params, NVS_SCAN_PARAMS_OBJECT_KEY), "Failed to save scan params to NVS"
        );
    });
}

BROOKESIA_PLUGIN_REGISTER_SINGLETON(ServiceBase, Wifi, Wifi::get_instance().get_attributes().name, Wifi::get_instance());

} // namespace esp_brookesia::service::wifi
