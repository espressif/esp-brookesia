/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/service_wifi/macro_configs.h"
#if !BROOKESIA_SERVICE_WIFI_SERVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_helper/nvs.hpp"
#include "brookesia/service_wifi/service_wifi.hpp"

namespace esp_brookesia::service::wifi {

using NVSHelper = service::helper::NVS;

bool Wifi::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_WIFI_VER_MAJOR, BROOKESIA_SERVICE_WIFI_VER_MINOR,
        BROOKESIA_SERVICE_WIFI_VER_PATCH
    );

    /* Create and initialize wifi context */
    BROOKESIA_CHECK_EXCEPTION_RETURN(hal_ = std::make_shared<Hal>(), false, "Failed to create Hal");
    BROOKESIA_CHECK_FALSE_RETURN(hal_->init(), false, "Failed to initialize wifi context");

    /* Create and initialize state machine */
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        state_machine_ = std::make_unique<StateMachine>(hal_), false,
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
}

bool Wifi::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(hal_->start(), false, "Failed to start HAL");
    BROOKESIA_CHECK_FALSE_RETURN(state_machine_->start(), false, "Failed to start state machine");

    // Try to load data from NVS, if NVS is not valid, skip
    try_load_data();

    return true;
}

void Wifi::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    /* Stop the general action queue task */
    stop_general_action_queue_task();

    /* If the WiFi is already deinited, skip */
    if (!hal_->is_general_event_ready(GeneralEvent::Deinited)) {
        /* First, monitor the deinited event */
        auto deinit_finished_promise = std::make_shared<std::promise<void>>();
        auto deinit_finished_future = deinit_finished_promise->get_future();
        auto wait_for_deinit_finished =
        [this, deinit_finished_promise](const std::string & event_name, const std::string & event, bool is_unexpected) {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

            BROOKESIA_LOGD("Params: event_name(%1%), event(%2%)", event_name, event);

            if (event != BROOKESIA_DESCRIBE_TO_STR(Helper::GeneralEvent::Deinited)) {
                BROOKESIA_LOGD("Not deinited, skip");
                return;
            }

            BROOKESIA_LOGI("Deinited, notify deinit finished");

            deinit_finished_promise->set_value();
        };
        auto wait_for_deinit_event_connection = Helper::subscribe_event(
                Helper::EventId::GeneralEventHappened, wait_for_deinit_finished
                                                );

        /* Then, trigger deinit action */
        BROOKESIA_CHECK_FALSE_EXECUTE(trigger_general_action(GeneralAction::Deinit), {}, {
            BROOKESIA_LOGE("Failed to trigger deinit action when stopping service");
        });

        /* Wait for deinit finished or timeout */
        if (deinit_finished_future.wait_for(
                    std::chrono::milliseconds(BROOKESIA_SERVICE_WIFI_SERVICE_WAIT_DEINIT_FINISHED_TIMEOUT_MS)
                ) != std::future_status::ready) {
            BROOKESIA_LOGW(
                "Wait for deinit finished timeout in %1% ms, force stop state machine",
                BROOKESIA_SERVICE_WIFI_SERVICE_WAIT_DEINIT_FINISHED_TIMEOUT_MS
            );
        }
    }

    /* Stop the state machine first */
    state_machine_->stop();
    /* Reset the wifi context */
    hal_->stop();
}

std::expected<void, std::string> Wifi::function_trigger_general_action(const std::string &action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: action(%1%)", action);

    // Parse enum string
    GeneralAction action_enum;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM(action, action_enum)) {
        return std::unexpected("Invalid action: " + action);
    }

    if (!trigger_general_action(action_enum)) {
        return std::unexpected("Failed to trigger general action: " + action);
    }

    return {};
}

std::expected<void, std::string> Wifi::function_trigger_scan_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    /* Trigger start action first */
    if (!trigger_general_action(GeneralAction::Start)) {
        return std::unexpected("Failed to trigger start action");
    }

    if (!hal_->start_scan()) {
        return std::unexpected("Failed to start AP scan");
    }

    return {};
}

std::expected<void, std::string> Wifi::function_trigger_scan_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    hal_->stop_scan();

    return {};
}

std::expected<void, std::string> Wifi::function_trigger_softap_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    /* Trigger start action first */
    if (!trigger_general_action(GeneralAction::Start)) {
        return std::unexpected("Failed to trigger start action");
    }

    if (!hal_->start_softap()) {
        return std::unexpected("Failed to start AP");
    }

    return {};
}

std::expected<void, std::string> Wifi::function_trigger_softap_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    hal_->stop_softap();

    return {};
}

std::expected<void, std::string> Wifi::function_trigger_softap_provision_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    /* Trigger start action first */
    if (!trigger_general_action(GeneralAction::Start)) {
        return std::unexpected("Failed to trigger start action");
    }

    /* Stop AP scan to avoid conflict with SoftAP scan */
    hal_->stop_scan();

    if (!hal_->start_softap_provision()) {
        return std::unexpected("Failed to start AP provision");
    }

    return {};
}

std::expected<void, std::string> Wifi::function_trigger_softap_provision_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    hal_->stop_softap_provision();

    return {};
}

std::expected<void, std::string> Wifi::function_set_scan_params(const boost::json::object &params)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: params(%1%)", BROOKESIA_DESCRIBE_TO_STR(params));

    ScanParams scan_params;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(params, scan_params)) {
        return std::unexpected("Failed to deserialize scan params: " + BROOKESIA_DESCRIBE_TO_STR(params));
    }

    if (!set_data<DataType::ScanParams>(scan_params)) {
        return std::unexpected("Failed to set scan params");
    }

    return {};
}

std::expected<void, std::string> Wifi::function_set_softap_params(const boost::json::object &params)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: params(%1%)", BROOKESIA_DESCRIBE_TO_STR(params));

    SoftApParams ap_params;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(params, ap_params)) {
        return std::unexpected("Failed to deserialize AP params: " + BROOKESIA_DESCRIBE_TO_STR(params));
    }

    if (!set_data<DataType::SoftApParams>(ap_params)) {
        return std::unexpected("Failed to set AP params");
    }

    return {};
}

std::expected<void, std::string> Wifi::function_set_connect_ap(const std::string &ssid, const std::string &password)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: ssid(%1%), password(%2%)", ssid, password);

    ConnectApInfo ap_info(ssid, password);
    if (!ap_info.is_valid()) {
        return std::unexpected("Invalid AP info: " + BROOKESIA_DESCRIBE_TO_STR(ap_info));
    }

    if (!hal_->set_target_connect_ap_info(ap_info)) {
        return std::unexpected("Failed to set connect AP info: " + BROOKESIA_DESCRIBE_TO_STR(ap_info));
    }

    return {};
}

std::expected<std::string, std::string> Wifi::function_get_general_state()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto general_state = state_machine_->get_current_state();
    if (general_state == GeneralState::Max) {
        return std::unexpected("Invalid general state");
    }

    return BROOKESIA_DESCRIBE_TO_STR(general_state);
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

    auto &connected_aps = get_data<DataType::ConnectedAps>();
    boost::json::array ap_ssids;
    for (const auto &ap : connected_aps) {
        ap_ssids.push_back(boost::json::string(ap.ssid));
    }

    return ap_ssids;
}

std::expected<boost::json::object, std::string> Wifi::function_get_softap_params()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return BROOKESIA_DESCRIBE_TO_JSON(get_data<DataType::SoftApParams>()).as_object();
}

std::expected<void, std::string> Wifi::function_reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    reset_data();

    return {};
}

void Wifi::try_load_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_data_loaded_) {
        BROOKESIA_LOGD("Data is already loaded, skip");
        return;
    }

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not valid, skip");
        return;
    }

    auto binding = ServiceManager::get_instance().bind(NVSHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_EXIT(binding.is_valid(), "Failed to bind NVS service");

    auto nvs_namespace = get_attributes().name;
    auto load_data_from_nvs = [this, &nvs_namespace]<DataType type>() {
        auto key = BROOKESIA_DESCRIBE_TO_STR(type);
        // Remove reference and const/volatile to get value type, as std::expected cannot store reference types
        // and describe_from_json needs non-const reference to modify the value
        using ValueType = std::remove_cvref_t<decltype(get_data<type>())>;
        auto nvs_result = NVSHelper::get_key_value<ValueType>(nvs_namespace, key);
        if (!nvs_result) {
            BROOKESIA_LOGW("Failed to load '%1%' from NVS: %2%", key, nvs_result.error());
            return;
        }
        // Skip saving to NVS, because the data is already loaded from NVS
        set_data<type>(nvs_result.value(), false, true);
        BROOKESIA_LOGI("Loaded '%1%' from NVS", key);
        BROOKESIA_LOGD("\t Value: %1%", BROOKESIA_DESCRIBE_TO_STR(nvs_result.value()));
    };

    load_data_from_nvs.template operator()<DataType::LastAp>();
    load_data_from_nvs.template operator()<DataType::ConnectedAps>();
    load_data_from_nvs.template operator()<DataType::ScanParams>();
    load_data_from_nvs.template operator()<DataType::SoftApParams>();

    // Set the target connect AP info to the last connected AP info
    hal_->set_target_connect_ap_info(get_data<DataType::LastAp>());

    is_data_loaded_ = true;

    BROOKESIA_LOGI("Loaded all data from NVS");
}

void Wifi::try_save_data(DataType type)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto nvs_namespace = get_attributes().name;
    auto save_data_to_nvs_function = [this, &nvs_namespace]<DataType type>() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        auto key = BROOKESIA_DESCRIBE_TO_STR(type);
        auto result = NVSHelper::save_key_value(nvs_namespace, key, get_data<type>(), BROOKESIA_SERVICE_WIFI_SERVICE_NVS_SAVE_DATA_TIMEOUT_MS);
        if (!result) {
            BROOKESIA_LOGE("Failed to save '%1%' to NVS: %2%", key, result.error());
        } else {
            BROOKESIA_LOGI("Saved '%1%' to NVS", key);
            BROOKESIA_LOGD("\t Value: %1%", BROOKESIA_DESCRIBE_TO_STR(get_data<type>()));
        }
    };

    if (type == DataType::LastAp) {
        save_data_to_nvs_function.template operator()<DataType::LastAp>();
    } else if (type == DataType::ConnectedAps) {
        save_data_to_nvs_function.template operator()<DataType::ConnectedAps>();
    } else if (type == DataType::ScanParams) {
        save_data_to_nvs_function.template operator()<DataType::ScanParams>();
    } else if (type == DataType::SoftApParams) {
        save_data_to_nvs_function.template operator()<DataType::SoftApParams>();
    } else {
        BROOKESIA_LOGE("Invalid data type for saving to NVS");
    }
}

void Wifi::try_erase_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto result = NVSHelper::erase_keys(
                      get_attributes().name, {}, BROOKESIA_SERVICE_WIFI_SERVICE_NVS_ERASE_DATA_TIMEOUT_MS
                  );
    if (!result) {
        BROOKESIA_LOGE("Failed to erase NVS data: %1%", result.error());
    } else {
        BROOKESIA_LOGI("Erased NVS data");
    }
}

void Wifi::reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    hal_->reset_data();

    try_erase_data();
}

bool Wifi::publish_general_event(GeneralEvent event, bool is_unexpected)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event(%1%)", BROOKESIA_DESCRIBE_TO_STR(event));

    auto result = publish_event(BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::GeneralEventHappened), {
        BROOKESIA_DESCRIBE_TO_STR(event), is_unexpected
    });
    if (!result) {
        BROOKESIA_LOGE("Failed to publish general event happened event");
        return false;
    }

    return true;
}

bool Wifi::publish_general_action(GeneralAction action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: action(%1%)", BROOKESIA_DESCRIBE_TO_STR(action));

    auto result = publish_event(BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::GeneralActionTriggered), {
        BROOKESIA_DESCRIBE_TO_STR(action)
    });
    if (!result) {
        BROOKESIA_LOGE("Failed to publish general action triggered event");
        return false;
    }

    return true;
}

bool Wifi::publish_scan_ap_infos(std::span<const ApInfo> &ap_infos)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // BROOKESIA_LOGD("Params: ap_infos(%1%)", boost::json::serialize(ap_infos));

    /* Publish scan infos updated event */
    auto result = publish_event(BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::ScanApInfosUpdated), {
        BROOKESIA_DESCRIBE_TO_JSON(ap_infos).as_array()
    });
    if (!result) {
        BROOKESIA_LOGE("Failed to publish scan infos updated event");
        return false;
    }

    return true;
}

bool Wifi::publish_softap_event(SoftApEvent event)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event(%1%)", BROOKESIA_DESCRIBE_TO_STR(event));

    auto result = publish_event(BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::SoftApEventHappened), {
        BROOKESIA_DESCRIBE_TO_STR(event)
    });
    if (!result) {
        BROOKESIA_LOGE("Failed to publish softap event happened event");
        return false;
    }

    return true;
}

bool Wifi::on_hal_unexpected_general_event(GeneralEvent event)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto target_state = StateMachine::get_general_event_target_state(event);
    BROOKESIA_LOGW("Force state machine to transition to target state: %1%", BROOKESIA_DESCRIBE_TO_STR(target_state));

    // Trigger state machine force transition to target state
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->force_transition_to(target_state),
        false, "Failed to force transition to '%1%' state", BROOKESIA_DESCRIBE_TO_STR(target_state)
    );

    return true;
}

bool Wifi::is_general_action_queue_task_running()
{
    auto task_scheduler = get_task_scheduler();
    if (!task_scheduler) {
        return false;
    }

    if (general_action_queue_processing_task_ == 0) {
        return false;
    }

    if (task_scheduler->get_state(general_action_queue_processing_task_) !=
            lib_utils::TaskScheduler::TaskState::Running) {
        return false;
    }

    return true;
}

GeneralAction Wifi::pop_general_action_pending_queue_front()
{
    auto action = GeneralAction::Max;
    if (general_action_pending_queue_.empty()) {
        return action;
    }

    action = general_action_pending_queue_.front();
    general_action_pending_queue_.pop();

    return action;
}

GeneralAction Wifi::pop_general_action_ready_queue_front()
{
    auto action = GeneralAction::Max;
    if (general_action_ready_queue_.empty()) {
        return action;
    }

    action = general_action_ready_queue_.front();
    general_action_ready_queue_.pop();

    return action;
}


bool Wifi::start_general_action_queue_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_general_action_queue_task_running()) {
        BROOKESIA_LOGD("Already running, skip");
        return true;
    }

    auto task = [this]() {
        // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        if (state_machine_->is_action_running()) {
            // BROOKESIA_LOGD("Action is running, skip");
            return true;
        }

        // If the ready queue is empty, pop the pending queue and make the action ready
        if (general_action_ready_queue_.empty()) {
            BROOKESIA_LOGD("Checking pending queue: %1%", BROOKESIA_DESCRIBE_TO_STR(general_action_pending_queue_));

            auto pending_action = pop_general_action_pending_queue_front();
            if (pending_action == GeneralAction::Max) {
                BROOKESIA_LOGD("No action in the pending queue, stop task");
                return false;
            }

            BROOKESIA_LOGD("Pop the pending action '%1%'", BROOKESIA_DESCRIBE_TO_STR(pending_action));

            if (pending_action == GeneralAction::Disconnect) {
                BROOKESIA_LOGD("Disconnect action detected, mark the target connectable AP as not connectable");
                hal_->mark_target_connect_ap_info_connectable(false);
            }
            make_general_action_ready_queue(pending_action);
        }

        BROOKESIA_LOGD("Checking ready queue: %1%", BROOKESIA_DESCRIBE_TO_STR(general_action_ready_queue_));

        auto ready_action = pop_general_action_ready_queue_front();
        if (ready_action == GeneralAction::Max) {
            BROOKESIA_LOGD("No action in the ready queue, stop task");
            return false;
        }

        BROOKESIA_CHECK_FALSE_RETURN(
            state_machine_->trigger_general_action(ready_action), true,
            "Failed to trigger ready action: %1%", BROOKESIA_DESCRIBE_TO_STR(ready_action)
        );

        if (general_action_pending_queue_.empty() && general_action_ready_queue_.empty()) {
            BROOKESIA_LOGD("No action in the pending or ready queue, stop task");
            return false;
        }

        return true;
    };
    auto task_scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(task_scheduler, false, "Invalid task scheduler");

    auto result = task_scheduler->post_periodic(
                      task, BROOKESIA_SERVICE_WIFI_SERVICE_GENERAL_ACTION_QUEUE_DISPATCH_INTERVAL_MS,
                      &general_action_queue_processing_task_, Wifi::get_instance().get_state_task_group()
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to post general action queue processing task");

    return true;
}

void Wifi::stop_general_action_queue_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (general_action_queue_processing_task_ != 0) {
        auto task_scheduler = get_task_scheduler();
        if (task_scheduler) {
            task_scheduler->cancel(general_action_queue_processing_task_);
        }
        general_action_queue_processing_task_ = 0;
    }
    std::queue<GeneralAction>().swap(general_action_pending_queue_);
    std::queue<GeneralAction>().swap(general_action_ready_queue_);
}

void Wifi::make_general_action_ready_queue(GeneralAction action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: action(%1%)", BROOKESIA_DESCRIBE_TO_STR(action));

    GeneralAction pre_action = GeneralAction::Max;
    switch (action) {
    case GeneralAction::Deinit:
        if (hal_->is_general_event_ready(GeneralEvent::Started) ||
                hal_->is_general_event_ready(GeneralEvent::Connected)) {
            BROOKESIA_LOGD("WiFi is started or connected");
            pre_action = GeneralAction::Stop;
        }
        break;
    case GeneralAction::Start:
        if (hal_->is_general_event_ready(GeneralEvent::Deinited)) {
            BROOKESIA_LOGD("WiFi is deinited");
            pre_action = GeneralAction::Init;
        }
        break;
    case GeneralAction::Stop:
        if (hal_->is_general_event_ready(GeneralEvent::Connected)) {
            BROOKESIA_LOGD("WiFi is connected");
            pre_action = GeneralAction::Disconnect;
        }
        break;
    case GeneralAction::Connect:
        if (hal_->is_general_event_ready(GeneralEvent::Stopped) ||
                hal_->is_general_event_ready(GeneralEvent::Deinited)) {
            pre_action = GeneralAction::Start;
        } else if (hal_->is_general_event_ready(GeneralEvent::Connected)) {
            BROOKESIA_LOGD("WiFi is already connected");
            pre_action = GeneralAction::Disconnect;
        }
        break;
    default:
        break;
    }

    if (pre_action != GeneralAction::Max) {
        BROOKESIA_LOGD("Add pre action '%1%' to the queue", BROOKESIA_DESCRIBE_TO_STR(pre_action));
        make_general_action_ready_queue(pre_action);
    } else {
        BROOKESIA_LOGD("No pre action, skip");
    }

    BROOKESIA_LOGD("Add the target action '%1%' to the queue", BROOKESIA_DESCRIBE_TO_STR(action));
    general_action_ready_queue_.push(action);
}

bool Wifi::trigger_general_action(const GeneralAction &action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: action(%1%)", BROOKESIA_DESCRIBE_TO_STR(action));

    if (!general_action_pending_queue_.empty() && (action == general_action_pending_queue_.front())) {
        BROOKESIA_LOGD("Action '%1%' is already in the pending queue front, skip", BROOKESIA_DESCRIBE_TO_STR(action));
        return true;
    }

    BROOKESIA_LOGD("Added action '%1%' to the pending queue", BROOKESIA_DESCRIBE_TO_STR(action));
    general_action_pending_queue_.push(action);

    BROOKESIA_CHECK_FALSE_RETURN(start_general_action_queue_task(), false, "Failed to start general action queue task");

    return true;
}

#if BROOKESIA_SERVICE_WIFI_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    ServiceBase, Wifi, Wifi::get_instance().get_attributes().name, Wifi::get_instance(),
    BROOKESIA_SERVICE_WIFI_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::service::wifi
