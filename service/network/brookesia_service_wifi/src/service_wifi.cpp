/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <expected>
#include <optional>
#include <string_view>
#include <utility>
#include "boost/format.hpp"
#include "brookesia/service_wifi/macro_configs.h"
#if !BROOKESIA_SERVICE_WIFI_SERVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_helper/system/storage.hpp"
#include "brookesia/service_wifi/service_wifi.hpp"

namespace esp_brookesia::service::wifi {

using StorageHelper = service::helper::Storage;

namespace {

std::expected<std::string, std::string> make_storage_namespace(std::string_view raw_namespace)
{
    auto result = StorageHelper::make_kv_namespace({raw_namespace});
    if (!result) {
        return std::unexpected(
                   (boost::format("Failed to make Wi-Fi Storage namespace '%1%': %2%") %
                    raw_namespace % result.error()).str()
               );
    }
    if (result->hashed) {
        BROOKESIA_LOGW("%1%", result->warning);
    }
    return result->name;
}

std::expected<std::string, std::string> make_storage_key(std::string_view raw_key)
{
    auto result = StorageHelper::make_kv_key({raw_key});
    if (!result) {
        return std::unexpected(
                   (boost::format("Failed to make Wi-Fi Storage key '%1%': %2%") %
                    raw_key % result.error()).str()
               );
    }
    if (result->hashed) {
        BROOKESIA_LOGW("%1%", result->warning);
    }
    return result->name;
}

std::optional<hal::wifi::BasicAction> to_basic_action(GeneralAction action)
{
    switch (action) {
    case GeneralAction::Init:
        return hal::wifi::BasicAction::Init;
    case GeneralAction::Deinit:
        return hal::wifi::BasicAction::Deinit;
    case GeneralAction::Start:
        return hal::wifi::BasicAction::Start;
    case GeneralAction::Stop:
        return hal::wifi::BasicAction::Stop;
    default:
        return std::nullopt;
    }
}

std::optional<hal::wifi::BasicEvent> to_basic_event(GeneralEvent event)
{
    switch (event) {
    case GeneralEvent::Inited:
        return hal::wifi::BasicEvent::Inited;
    case GeneralEvent::Deinited:
        return hal::wifi::BasicEvent::Deinited;
    case GeneralEvent::Started:
        return hal::wifi::BasicEvent::Started;
    case GeneralEvent::Stopped:
        return hal::wifi::BasicEvent::Stopped;
    default:
        return std::nullopt;
    }
}

std::optional<hal::wifi::StationAction> to_station_action(GeneralAction action)
{
    switch (action) {
    case GeneralAction::Connect:
        return hal::wifi::StationAction::Connect;
    case GeneralAction::Disconnect:
        return hal::wifi::StationAction::Disconnect;
    default:
        return std::nullopt;
    }
}

std::optional<hal::wifi::StationEvent> to_station_event(GeneralEvent event)
{
    switch (event) {
    case GeneralEvent::Connected:
        return hal::wifi::StationEvent::Connected;
    case GeneralEvent::Disconnected:
        return hal::wifi::StationEvent::Disconnected;
    default:
        return std::nullopt;
    }
}

GeneralAction to_general_action(hal::wifi::BasicAction action)
{
    switch (action) {
    case hal::wifi::BasicAction::Init:
        return GeneralAction::Init;
    case hal::wifi::BasicAction::Deinit:
        return GeneralAction::Deinit;
    case hal::wifi::BasicAction::Start:
        return GeneralAction::Start;
    case hal::wifi::BasicAction::Stop:
        return GeneralAction::Stop;
    default:
        return GeneralAction::Max;
    }
}

GeneralAction to_general_action(hal::wifi::StationAction action)
{
    switch (action) {
    case hal::wifi::StationAction::Connect:
        return GeneralAction::Connect;
    case hal::wifi::StationAction::Disconnect:
        return GeneralAction::Disconnect;
    default:
        return GeneralAction::Max;
    }
}

GeneralEvent to_general_event(hal::wifi::BasicEvent event)
{
    switch (event) {
    case hal::wifi::BasicEvent::Inited:
        return GeneralEvent::Inited;
    case hal::wifi::BasicEvent::Deinited:
        return GeneralEvent::Deinited;
    case hal::wifi::BasicEvent::Started:
        return GeneralEvent::Started;
    case hal::wifi::BasicEvent::Stopped:
        return GeneralEvent::Stopped;
    default:
        return GeneralEvent::Max;
    }
}

GeneralEvent to_general_event(hal::wifi::StationEvent event)
{
    switch (event) {
    case hal::wifi::StationEvent::Connected:
        return GeneralEvent::Connected;
    case hal::wifi::StationEvent::Disconnected:
        return GeneralEvent::Disconnected;
    default:
        return GeneralEvent::Max;
    }
}

bool is_same_ap(const ConnectApInfo &left, const ConnectApInfo &right)
{
    return (left.ssid == right.ssid) && (left.password == right.password);
}

} // namespace

bool Wifi::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_WIFI_VER_MAJOR, BROOKESIA_SERVICE_WIFI_VER_MINOR,
        BROOKESIA_SERVICE_WIFI_VER_PATCH
    );

    auto basic_iface = hal::acquire_first_interface<hal::wifi::BasicIface>();
    auto station_iface = hal::acquire_first_interface<hal::wifi::StationIface>();
    auto softap_iface = hal::acquire_first_interface<hal::wifi::SoftApIface>();

    BROOKESIA_CHECK_NULL_RETURN(basic_iface, false, "Wi-Fi basic HAL interface is not available");
    BROOKESIA_CHECK_NULL_RETURN(station_iface, false, "Wi-Fi station HAL interface is not available");

    BROOKESIA_LOGI("Using Wi-Fi basic interface: %1%", basic_iface.instance_name());
    BROOKESIA_LOGI("Using Wi-Fi station interface: %1%", station_iface.instance_name());
    if (softap_iface) {
        BROOKESIA_LOGI("Using Wi-Fi SoftAP interface: %1%", softap_iface.instance_name());
    } else {
        BROOKESIA_LOGW("Wi-Fi SoftAP HAL interface is not available");
    }

    basic_iface_ = std::move(basic_iface);
    station_iface_ = std::move(station_iface);
    softap_iface_ = std::move(softap_iface);

    BROOKESIA_CHECK_FALSE_RETURN(basic_iface_->init(), false, "Failed to initialize Wi-Fi basic interface");

    /* Create and initialize state machine */
    BROOKESIA_CHECK_EXCEPTION_RETURN(
    state_machine_ = std::make_unique<StateMachine>([this](GeneralAction action) {
        return dispatch_general_action(action);
    }), false,
    "Failed to create StateMachine"
    );
    BROOKESIA_CHECK_FALSE_RETURN(state_machine_->init(), false, "Failed to initialize state machine");

    return true;
}

void Wifi::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_auto_connect_task();
    auto_connect_disabled_ = false;
    softap_provision_running_ = false;
    connect_retries_ = 0;
    connecting_ap_info_ = {};

    state_machine_.reset();
    if (basic_iface_) {
        basic_iface_->deinit();
    }
    clear_hal_interface_callbacks();
    softap_iface_.reset();
    station_iface_.reset();
    basic_iface_.reset();
}

bool Wifi::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto_connect_disabled_ = false;
    softap_provision_running_ = false;
    connect_retries_ = 0;
    connecting_ap_info_ = {};

    BROOKESIA_CHECK_FALSE_RETURN(configure_hal_interfaces(), false, "Failed to configure Wi-Fi HAL interfaces");
    BROOKESIA_CHECK_FALSE_RETURN(basic_iface_->start(), false, "Failed to start Wi-Fi basic interface");
    BROOKESIA_CHECK_FALSE_RETURN(state_machine_->start(), false, "Failed to start state machine");

#if BROOKESIA_SERVICE_WIFI_ENABLE_AUTO_LOAD_DATA
    // Try to load data from Storage, if Storage service is not available, skip
    try_load_data();
#endif

    return true;
}

void Wifi::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_auto_connect_task();
    auto_connect_disabled_ = true;
    softap_provision_running_ = false;

    /* Stop the general action queue task */
    stop_general_action_queue_task();

    /* If the WiFi is already deinited, skip */
    if (!is_general_event_ready(GeneralEvent::Deinited)) {
        /* Trigger deinit action; HAL will set the deinited state bit synchronously
         * inside trigger_general_event() before notify_general_event() returns.
         * on_hal_general_event() runs on the WiFi worker thread and doesn't take
         * state_mutex_, so we can wait on a plain condition_variable here without
         * deadlocking against the unique lock that ServiceBase::stop() holds. */
        BROOKESIA_CHECK_FALSE_EXECUTE(trigger_general_action(GeneralAction::Deinit), {}, {
            BROOKESIA_LOGE("Failed to trigger deinit action when stopping service");
        });

        bool deinited = false;
        {
            std::unique_lock<std::mutex> lock(deinit_wait_mutex_);
            deinited = deinit_wait_cv_.wait_for(
                           lock,
                           std::chrono::milliseconds(BROOKESIA_SERVICE_WIFI_SERVICE_WAIT_DEINIT_FINISHED_TIMEOUT_MS),
            [this]() {
                return is_general_event_ready(GeneralEvent::Deinited);
            }
                       );
        }

        if (!deinited) {
            BROOKESIA_LOGW(
                "Wait for deinit finished timeout in %1% ms, force stop state machine",
                BROOKESIA_SERVICE_WIFI_SERVICE_WAIT_DEINIT_FINISHED_TIMEOUT_MS
            );
        }
    }

    /* Stop the state machine first */
    state_machine_->stop();
    /* Reset the wifi context */
    basic_iface_->stop();
    clear_hal_interface_callbacks();
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

    if (!station_iface_->start_scan()) {
        return std::unexpected("Failed to start AP scan");
    }

    return {};
}

std::expected<void, std::string> Wifi::function_trigger_scan_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    station_iface_->stop_scan();

    return {};
}

std::expected<void, std::string> Wifi::function_trigger_softap_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    /* Trigger start action first */
    if (!trigger_general_action(GeneralAction::Start)) {
        return std::unexpected("Failed to trigger start action");
    }

    if (!softap_iface_) {
        return std::unexpected("Wi-Fi SoftAP interface is not available");
    }

    if (!softap_iface_->start()) {
        return std::unexpected("Failed to start AP");
    }

    return {};
}

std::expected<void, std::string> Wifi::function_trigger_softap_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!softap_iface_) {
        return std::unexpected("Wi-Fi SoftAP interface is not available");
    }

    softap_iface_->stop();

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
    if (!softap_iface_) {
        return std::unexpected("Wi-Fi SoftAP interface is not available");
    }

    station_iface_->stop_scan();
    stop_auto_connect_task();
    auto_connect_disabled_ = true;
    softap_provision_running_ = true;

    BROOKESIA_CHECK_FALSE_EXECUTE(trigger_general_action(GeneralAction::Disconnect), {}, {
        BROOKESIA_LOGW("Failed to trigger disconnect action before starting AP provision");
    });

    if (!softap_iface_->start_provision()) {
        softap_provision_running_ = false;
        auto_connect_disabled_ = false;
        schedule_auto_connect();
        return std::unexpected("Failed to start AP provision");
    }

    return {};
}

std::expected<void, std::string> Wifi::function_trigger_softap_provision_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!softap_iface_) {
        return std::unexpected("Wi-Fi SoftAP interface is not available");
    }

    if (!softap_iface_->stop_provision()) {
        return std::unexpected("Failed to stop AP provision");
    }
    softap_provision_running_ = false;
    auto_connect_disabled_ = false;
    schedule_auto_connect();

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

    if (!softap_iface_) {
        return std::unexpected("Wi-Fi SoftAP interface is not available");
    }

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

    if (!station_iface_->set_target_connect_ap_info(ap_info)) {
        return std::unexpected("Failed to set connect AP info: " + BROOKESIA_DESCRIBE_TO_STR(ap_info));
    }
    connecting_ap_info_ = {};
    connect_retries_ = 0;

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

std::expected<boost::json::object, std::string> Wifi::function_get_connect_ap()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return BROOKESIA_DESCRIBE_TO_JSON(station_iface_->get_target_connect_ap_info()).as_object();
}

std::expected<boost::json::array, std::string> Wifi::function_get_connected_aps()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return BROOKESIA_DESCRIBE_TO_JSON(get_data<DataType::ConnectedAps>()).as_array();
}

std::expected<void, std::string> Wifi::function_remove_connected_ap(const std::string &ssid)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (ssid.empty()) {
        return std::unexpected("Wi-Fi SSID is empty");
    }
    if (!remove_connected_ap_info_by_ssid(ssid)) {
        return std::unexpected("Failed to remove connected AP info: " + ssid);
    }

    return {};
}

std::expected<boost::json::object, std::string> Wifi::function_get_softap_params()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!softap_iface_) {
        return std::unexpected("Wi-Fi SoftAP interface is not available");
    }

    return BROOKESIA_DESCRIBE_TO_JSON(get_data<DataType::SoftApParams>()).as_object();
}

std::expected<void, std::string> Wifi::function_reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    reset_data();

    return {};
}

std::expected<void, std::string> Wifi::function_load_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    is_data_loaded_ = false;
    try_load_data();

    return {};
}

void Wifi::try_load_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_data_loaded_) {
        BROOKESIA_LOGD("Data is already loaded, skip");
        return;
    }

    if (!StorageHelper::is_available()) {
        BROOKESIA_LOGD("Storage service is not available, skip");
        return;
    }

    auto binding = ServiceManager::get_instance().bind(StorageHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_EXIT(binding.is_valid(), "Failed to bind Storage service");

    auto namespace_result = make_storage_namespace(get_attributes().name);
    if (!namespace_result) {
        BROOKESIA_LOGW("%1%", namespace_result.error());
        return;
    }
    auto kv_namespace = std::move(namespace_result.value());
    auto load_data_from_kv = [this, &kv_namespace]<DataType type>() {
        auto raw_key = BROOKESIA_DESCRIBE_TO_STR(type);
        auto key_result = make_storage_key(raw_key);
        if (!key_result) {
            BROOKESIA_LOGW("%1%", key_result.error());
            return;
        }
        auto key = std::move(key_result.value());
        // Remove reference and const/volatile to get value type, as std::expected cannot store reference types
        // and describe_from_json needs non-const reference to modify the value
        using ValueType = std::remove_cvref_t<decltype(get_data<type>())>;
        auto kv_result = StorageHelper::get_key_value<ValueType>(kv_namespace, key);
        if (!kv_result) {
            BROOKESIA_LOGD("No persisted '%1%' in Storage: %2%", raw_key, kv_result.error());
            return;
        }
        // Skip saving to Storage, because the data is already loaded from Storage
        set_data<type>(kv_result.value(), false, true);
        BROOKESIA_LOGI("Loaded '%1%' from Storage", raw_key);
        BROOKESIA_LOGD("\t Value: %1%", BROOKESIA_DESCRIBE_TO_STR(kv_result.value()));
    };

    load_data_from_kv.template operator()<DataType::LastAp>();
    load_data_from_kv.template operator()<DataType::ConnectedAps>();
    load_data_from_kv.template operator()<DataType::ScanParams>();
    if (softap_iface_) {
        load_data_from_kv.template operator()<DataType::SoftApParams>();
    }

    // Set the target connect AP info to the last connected AP info
    station_iface_->set_target_connect_ap_info(get_data<DataType::LastAp>());

    is_data_loaded_ = true;

    BROOKESIA_LOGI("Loaded all data from Storage");
}

void Wifi::try_save_data(DataType type)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!StorageHelper::is_available()) {
        BROOKESIA_LOGD("Storage service is not available, skip");
        return;
    }

    auto namespace_result = make_storage_namespace(get_attributes().name);
    BROOKESIA_CHECK_FALSE_EXIT(namespace_result, "%1%", namespace_result.error());
    auto kv_namespace = std::move(namespace_result.value());
    auto save_data_to_kv_function = [this, &kv_namespace]<DataType type>() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        auto raw_key = BROOKESIA_DESCRIBE_TO_STR(type);
        auto key_result = make_storage_key(raw_key);
        BROOKESIA_CHECK_FALSE_EXIT(key_result, "%1%", key_result.error());
        auto key = std::move(key_result.value());
        auto result = StorageHelper::save_key_value_async(kv_namespace, key, get_data<type>(),
        [this, raw_key](auto &&result) mutable {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            BROOKESIA_CHECK_FALSE_EXIT(
                result.success, "Failed to save %1% to Storage: %2%", raw_key, result.error_message
            );
            BROOKESIA_LOGI("Saved '%1%' to Storage", raw_key);
        });
        BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to save data to Storage");
    };

    if (type == DataType::LastAp) {
        save_data_to_kv_function.template operator()<DataType::LastAp>();
    } else if (type == DataType::ConnectedAps) {
        save_data_to_kv_function.template operator()<DataType::ConnectedAps>();
    } else if (type == DataType::ScanParams) {
        save_data_to_kv_function.template operator()<DataType::ScanParams>();
    } else if ((type == DataType::SoftApParams) && softap_iface_) {
        save_data_to_kv_function.template operator()<DataType::SoftApParams>();
    } else {
        BROOKESIA_LOGE("Invalid data type for saving to Storage");
    }
}

void Wifi::try_erase_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!StorageHelper::is_available()) {
        BROOKESIA_LOGD("Storage service is not available, skip");
        return;
    }

    auto namespace_result = make_storage_namespace(get_attributes().name);
    if (!namespace_result) {
        BROOKESIA_LOGE("%1%", namespace_result.error());
        return;
    }
    auto result = StorageHelper::erase_keys(
                      namespace_result.value(), {}, BROOKESIA_SERVICE_WIFI_SERVICE_STORAGE_ERASE_DATA_TIMEOUT_MS
                  );
    if (!result) {
        BROOKESIA_LOGE("Failed to erase Storage data: %1%", result.error());
    } else {
        BROOKESIA_LOGI("Erased Storage data");
    }
}

void Wifi::reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    basic_iface_->reset_data();

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

bool Wifi::publish_scan_state_changed(bool is_running)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: is_running(%1%)", is_running);

    auto result = publish_event(
                      BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::ScanStateChanged),
                      std::vector<EventItem> {is_running}
                  );
    if (!result) {
        BROOKESIA_LOGE("Failed to publish scan state changed event");
        return false;
    }

    return true;
}

bool Wifi::publish_scan_ap_infos(std::span<const ScanApInfo> ap_infos)
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

bool Wifi::on_hal_general_event(GeneralEvent event, bool is_unexpected)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event(%1%), is_unexpected(%2%)", event, is_unexpected);

    /* Signal on_stop() waiters as soon as the HAL reports Deinited so we don't
     * rely on publish_event() (which would deadlock against ServiceBase::stop's
     * unique state_mutex_ via emit_signal_task's shared_lock). */
    if (event == GeneralEvent::Deinited) {
        std::lock_guard<std::mutex> lock(deinit_wait_mutex_);
        deinit_wait_cv_.notify_all();
    }

    if (!state_machine_ || !state_machine_->is_running()) {
        BROOKESIA_LOGD("State machine is not valid or not running, skip");
        return true;
    }

    if (is_unexpected) {
        auto target_state = StateMachine::get_general_event_target_state(event);
        BROOKESIA_LOGW("Force state machine to transition to target state: %1%", BROOKESIA_DESCRIBE_TO_STR(target_state));

        // Trigger state machine force transition to target state
        BROOKESIA_CHECK_FALSE_RETURN(
            state_machine_->force_transition_to(target_state),
            false, "Failed to force transition to '%1%' state", BROOKESIA_DESCRIBE_TO_STR(target_state)
        );
    } else {
        BROOKESIA_CHECK_FALSE_RETURN(
            state_machine_->trigger_extra_action(ExtraAction::Success), false, "Failed to trigger extra action"
        );
    }

    return true;
}

bool Wifi::on_hal_error_state()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!state_machine_ || !state_machine_->is_running()) {
        BROOKESIA_LOGD("State machine is not valid or not running, skip");
        return true;
    }

    // Trigger extra action to handle error state
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->trigger_extra_action(ExtraAction::Failure), false, "Failed to trigger error state action"
    );

    return true;
}

bool Wifi::handle_hal_general_event_policy(GeneralEvent event, bool is_unexpected, bool was_connecting)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: event(%1%), is_unexpected(%2%), was_connecting(%3%)",
        event, BROOKESIA_DESCRIBE_TO_STR(is_unexpected), BROOKESIA_DESCRIBE_TO_STR(was_connecting)
    );

    switch (event) {
    case GeneralEvent::Started:
        return schedule_auto_connect();
    case GeneralEvent::Stopped:
    case GeneralEvent::Deinited:
        stop_auto_connect_task();
        connect_retries_ = 0;
        return true;
    case GeneralEvent::Connected:
        stop_auto_connect_task();
        return update_connected_ap_info();
    case GeneralEvent::Disconnected:
        stop_auto_connect_task();
        if (is_unexpected) {
            return handle_unexpected_disconnected(was_connecting);
        }
        return true;
    default:
        return true;
    }
}

bool Wifi::handle_scan_ap_infos_policy(std::span<const ScanApInfo> ap_infos)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_auto_connect_allowed()) {
        BROOKESIA_LOGD("Auto connect is not allowed, skip scan policy");
        return true;
    }

    auto target_ap_info = station_iface_->get_target_connect_ap_info();
    for (const auto &scan_ap_info : ap_infos) {
        if (scan_ap_info.ssid.empty()) {
            continue;
        }

        if ((target_ap_info.is_valid()) && (scan_ap_info.ssid == target_ap_info.ssid)) {
            if (target_ap_info.is_connectable) {
                BROOKESIA_LOGD("Detected target AP in scan result: %1%", BROOKESIA_DESCRIBE_TO_STR(target_ap_info));
                return trigger_connect_to_ap(target_ap_info);
            }
            BROOKESIA_LOGD("Target AP is not connectable, skip: %1%", BROOKESIA_DESCRIBE_TO_STR(target_ap_info));
            continue;
        }

        ConnectApInfo history_ap_info;
        if (get_connectable_ap_info_by_ssid(scan_ap_info.ssid, history_ap_info)) {
            BROOKESIA_LOGD("Detected history AP in scan result: %1%", BROOKESIA_DESCRIBE_TO_STR(history_ap_info));
            return trigger_connect_to_ap(history_ap_info);
        }
    }

    return true;
}

bool Wifi::configure_hal_interfaces()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto handle_general_event = [this](GeneralEvent general_event, bool is_unexpected) {
        bool was_connecting = false;
        if (state_machine_) {
            was_connecting = (state_machine_->get_current_state() == GeneralState::Connecting);
        }
        was_connecting = was_connecting || is_general_action_running(GeneralAction::Connect);

        if (!on_hal_general_event(general_event, is_unexpected)) {
            BROOKESIA_LOGE("Failed to handle HAL event");
        }
        if (!publish_general_event(general_event, is_unexpected)) {
            BROOKESIA_LOGE("Failed to publish HAL event");
        }
        if (!handle_hal_general_event_policy(general_event, is_unexpected, was_connecting)) {
            BROOKESIA_LOGE("Failed to handle HAL event policy");
        }
    };

    auto basic_callbacks = hal::wifi::BasicIface::Callbacks{
        .on_action = [this](hal::wifi::BasicAction action)
        {
            auto general_action = to_general_action(action);
            if (general_action != GeneralAction::Max) {
                publish_general_action(general_action);
            }
        },
        .on_event = [handle_general_event](hal::wifi::BasicEvent event, bool is_unexpected)
        {
            auto general_event = to_general_event(event);
            if (general_event == GeneralEvent::Max) {
                return;
            }
            handle_general_event(general_event, is_unexpected);
        },
        .on_error = [this]()
        {
            on_hal_error_state();
        },
    };
    BROOKESIA_CHECK_FALSE_RETURN(
    basic_iface_->configure({
        .task_scheduler = get_task_scheduler(),
        .task_group = get_state_task_group(),
    }, std::move(basic_callbacks)),
    false, "Failed to configure Wi-Fi basic interface"
    );

    auto station_callbacks = hal::wifi::StationIface::Callbacks{
        .on_action = [this](hal::wifi::StationAction action)
        {
            auto general_action = to_general_action(action);
            if (general_action != GeneralAction::Max) {
                publish_general_action(general_action);
            }
        },
        .on_event = [handle_general_event](hal::wifi::StationEvent event, bool is_unexpected)
        {
            auto general_event = to_general_event(event);
            if (general_event == GeneralEvent::Max) {
                return;
            }
            handle_general_event(general_event, is_unexpected);
        },
        .on_error = [this]()
        {
            on_hal_error_state();
        },
        .on_action_requested = [this](hal::wifi::StationAction action)
        {
            auto general_action = to_general_action(action);
            return (general_action != GeneralAction::Max) && trigger_general_action(general_action);
        },
        .on_scan_state_changed = [this](bool is_running)
        {
            publish_scan_state_changed(is_running);
        },
        .on_scan_ap_infos_updated = [this](std::span<const ScanApInfo> ap_infos)
        {
            publish_scan_ap_infos(ap_infos);
            if (!handle_scan_ap_infos_policy(ap_infos)) {
                BROOKESIA_LOGE("Failed to handle scan AP infos policy");
            }
        },
        .on_last_connected_ap_info_updated = [this](const ConnectApInfo &)
        {
            try_save_data(DataType::LastAp);
        },
        .on_connected_ap_infos_updated = [this](const hal::wifi::ConnectApInfoList &)
        {
            try_save_data(DataType::ConnectedAps);
        },
    };
    BROOKESIA_CHECK_FALSE_RETURN(
        station_iface_->configure(std::move(station_callbacks)), false, "Failed to configure Wi-Fi station interface"
    );

    if (!softap_iface_) {
        return true;
    }

    auto softap_callbacks = hal::wifi::SoftApIface::Callbacks{
        .on_event = [this](hal::wifi::SoftApEvent event)
        {
            if ((event == hal::wifi::SoftApEvent::Stopped) && softap_provision_running_) {
                softap_provision_running_ = false;
                auto_connect_disabled_ = false;
                schedule_auto_connect();
            }
            publish_softap_event(event);
        },
        .on_params_updated = [this](const SoftApParams &)
        {
            try_save_data(DataType::SoftApParams);
        },
        .on_station_action_requested = [this](hal::wifi::StationAction action)
        {
            auto general_action = to_general_action(action);
            return (general_action != GeneralAction::Max) && trigger_general_action(general_action);
        },
    };
    BROOKESIA_CHECK_FALSE_RETURN(
        softap_iface_->configure(std::move(softap_callbacks)), false,
        "Failed to configure Wi-Fi SoftAP interface"
    );

    return true;
}

void Wifi::clear_hal_interface_callbacks()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (softap_iface_) {
        softap_iface_->clear_callbacks();
    }
    if (station_iface_) {
        station_iface_->clear_callbacks();
    }
    if (basic_iface_) {
        basic_iface_->clear_callbacks();
    }
}

bool Wifi::dispatch_general_action(GeneralAction action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (auto basic_action = to_basic_action(action)) {
        return basic_iface_->do_action(*basic_action);
    }
    if (auto station_action = to_station_action(action)) {
        if (action == GeneralAction::Connect) {
            connecting_ap_info_ = station_iface_->get_target_connect_ap_info();
        }
        return station_iface_->do_action(*station_action);
    }

    BROOKESIA_LOGE("Invalid general action: %1%", BROOKESIA_DESCRIBE_TO_STR(action));
    return false;
}

bool Wifi::is_general_action_running(GeneralAction action)
{
    if (auto basic_action = to_basic_action(action)) {
        return basic_iface_->is_action_running(*basic_action);
    }
    if (auto station_action = to_station_action(action)) {
        return station_iface_->is_action_running(*station_action);
    }
    return false;
}

bool Wifi::is_general_event_ready(GeneralEvent event)
{
    if (auto basic_event = to_basic_event(event)) {
        return basic_iface_->is_event_ready(*basic_event);
    }
    if (auto station_event = to_station_event(event)) {
        return station_iface_->is_event_ready(*station_event);
    }
    return false;
}

bool Wifi::is_auto_connect_allowed()
{
    if (auto_connect_disabled_ || softap_provision_running_) {
        return false;
    }
    if (!is_general_event_ready(GeneralEvent::Started) || is_general_event_ready(GeneralEvent::Connected)) {
        return false;
    }
    if (is_general_action_running(GeneralAction::Stop) || is_general_action_running(GeneralAction::Deinit) ||
            is_general_action_running(GeneralAction::Connect)) {
        return false;
    }

    return true;
}

bool Wifi::schedule_auto_connect()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_auto_connect_task();
    if (!is_auto_connect_allowed()) {
        BROOKESIA_LOGD("Auto connect is not allowed, skip scheduling");
        return true;
    }

    auto task_scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(task_scheduler, false, "Invalid task scheduler");

    auto delayed_task = [this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        auto_connect_task_ = 0;
        BROOKESIA_CHECK_FALSE_EXECUTE(trigger_auto_connect(), {}, {
            BROOKESIA_LOGE("Failed to trigger auto connect");
        });
    };
    BROOKESIA_CHECK_FALSE_RETURN(
        task_scheduler->post_delayed(
            std::move(delayed_task), BROOKESIA_SERVICE_WIFI_AUTO_CONNECT_DELAY_MS, &auto_connect_task_,
            get_state_task_group()
        ), false, "Failed to post auto connect delayed task"
    );

    BROOKESIA_LOGD(
        "Auto connect task posted successfully, delay(%1%) ms", BROOKESIA_SERVICE_WIFI_AUTO_CONNECT_DELAY_MS
    );

    return true;
}

void Wifi::stop_auto_connect_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (auto_connect_task_ == 0) {
        return;
    }

    auto task_scheduler = get_task_scheduler();
    if (task_scheduler) {
        task_scheduler->cancel(auto_connect_task_);
    }
    auto_connect_task_ = 0;
}

bool Wifi::trigger_auto_connect()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_auto_connect_allowed()) {
        BROOKESIA_LOGD("Auto connect is not allowed, skip");
        return true;
    }

    auto target_ap_info = station_iface_->get_target_connect_ap_info();
    if (!target_ap_info.is_valid()) {
        BROOKESIA_LOGD("No target connect AP info, try last connected AP");
        target_ap_info = station_iface_->get_last_connected_ap_info();
    }

    if (target_ap_info.is_valid() && !target_ap_info.is_connectable) {
        BROOKESIA_LOGD(
            "Target AP is not connectable, try history connectable AP: %1%",
            BROOKESIA_DESCRIBE_TO_STR(target_ap_info)
        );
        if (!get_last_connectable_ap_info(target_ap_info)) {
            BROOKESIA_LOGD("No history connectable AP found, use target AP directly");
        }
    }

    if (!target_ap_info.is_valid()) {
        BROOKESIA_LOGD("No connectable AP info, skip auto connect");
        return true;
    }

    BROOKESIA_LOGD("Auto connect to AP: %1%", BROOKESIA_DESCRIBE_TO_STR(target_ap_info));
    return trigger_connect_to_ap(target_ap_info);
}

bool Wifi::trigger_connect_to_ap(const ConnectApInfo &ap_info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(ap_info.is_valid(), false, "Invalid AP info");
    BROOKESIA_CHECK_FALSE_RETURN(
        station_iface_->set_target_connect_ap_info(ap_info), false, "Failed to set target AP info"
    );

    return trigger_general_action(GeneralAction::Connect);
}

bool Wifi::get_connectable_ap_info_by_ssid(const std::string &ssid, ConnectApInfo &ap_info)
{
    for (const auto &info : station_iface_->get_connected_ap_infos()) {
        if ((info.ssid == ssid) && info.is_connectable) {
            ap_info = info;
            return true;
        }
    }

    return false;
}

bool Wifi::get_last_connectable_ap_info(ConnectApInfo &ap_info)
{
    const auto &ap_infos = station_iface_->get_connected_ap_infos();
    for (auto it = ap_infos.rbegin(); it != ap_infos.rend(); ++it) {
        if (it->is_connectable) {
            ap_info = *it;
            return true;
        }
    }

    return false;
}

bool Wifi::mark_ap_connectable(const ConnectApInfo &ap_info, bool connectable)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!ap_info.is_valid()) {
        return true;
    }

    auto target_ap_info = station_iface_->get_target_connect_ap_info();
    if (is_same_ap(target_ap_info, ap_info) && (target_ap_info.is_connectable != connectable)) {
        station_iface_->mark_target_connect_ap_info_connectable(connectable);
    }

    auto last_ap_info = station_iface_->get_last_connected_ap_info();
    if (is_same_ap(last_ap_info, ap_info) && (last_ap_info.is_connectable != connectable)) {
        last_ap_info.is_connectable = connectable;
        set_data<DataType::LastAp>(last_ap_info);
    }

    auto connected_ap_infos = station_iface_->get_connected_ap_infos();
    bool is_changed = false;
    for (auto &connected_ap_info : connected_ap_infos) {
        if ((connected_ap_info.ssid == ap_info.ssid) && (connected_ap_info.is_connectable != connectable)) {
            connected_ap_info.is_connectable = connectable;
            is_changed = true;
        }
    }
    if (is_changed) {
        return set_data<DataType::ConnectedAps>(connected_ap_infos);
    }

    return true;
}

bool Wifi::add_connected_ap_info(const ConnectApInfo &ap_info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto connected_ap_infos = station_iface_->get_connected_ap_infos();
    for (auto &connected_ap_info : connected_ap_infos) {
        if (connected_ap_info.ssid == ap_info.ssid) {
            if (connected_ap_info == ap_info) {
                return true;
            }
            connected_ap_info = ap_info;
            return set_data<DataType::ConnectedAps>(connected_ap_infos);
        }
    }

    connected_ap_infos.push_back(ap_info);
    return set_data<DataType::ConnectedAps>(connected_ap_infos);
}

bool Wifi::remove_connected_ap_info_by_ssid(std::string_view ssid)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (ssid.empty()) {
        return true;
    }

    auto result = true;
    auto target_ap_info = station_iface_->get_target_connect_ap_info();
    const auto is_connected_to_removed_ap =
        state_machine_->get_current_state() == GeneralState::Connected && target_ap_info.ssid == ssid;
    if (target_ap_info.ssid == ssid && !is_connected_to_removed_ap) {
        result = station_iface_->set_target_connect_ap_info(ConnectApInfo()) && result;
        connecting_ap_info_ = {};
        connect_retries_ = 0;
    }

    auto last_ap_info = station_iface_->get_last_connected_ap_info();
    if (last_ap_info.ssid == ssid) {
        result = set_data<DataType::LastAp>(ConnectApInfo()) && result;
    }

    auto connected_ap_infos = station_iface_->get_connected_ap_infos();
    const auto old_size = connected_ap_infos.size();
    connected_ap_infos.remove_if([ssid](const auto & ap_info) {
        return ap_info.ssid == ssid;
    });
    if (connected_ap_infos.size() != old_size) {
        result = set_data<DataType::ConnectedAps>(connected_ap_infos) && result;
    }

    return result;
}

bool Wifi::update_connected_ap_info()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    connect_retries_ = 0;
    auto connected_ap_info = connecting_ap_info_;
    if (!connected_ap_info.is_valid()) {
        connected_ap_info = station_iface_->get_target_connect_ap_info();
    }
    if (!connected_ap_info.is_valid()) {
        BROOKESIA_LOGD("No connected AP info, skip update");
        return true;
    }

    connected_ap_info.is_connectable = true;
    mark_ap_connectable(connected_ap_info, true);
    BROOKESIA_CHECK_FALSE_RETURN(set_data<DataType::LastAp>(connected_ap_info), false, "Failed to set last AP info");
    BROOKESIA_CHECK_FALSE_RETURN(add_connected_ap_info(connected_ap_info), false, "Failed to add connected AP info");

    return true;
}

bool Wifi::handle_unexpected_disconnected(bool was_connecting)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (was_connecting && connecting_ap_info_.is_valid()) {
        if (connect_retries_ < BROOKESIA_SERVICE_WIFI_CONNECT_RETRIES_MAX) {
            connect_retries_++;
            BROOKESIA_LOGW(
                "Try to connect again... (%1%/%2%)", connect_retries_, BROOKESIA_SERVICE_WIFI_CONNECT_RETRIES_MAX
            );
            return trigger_connect_to_ap(connecting_ap_info_);
        }

        BROOKESIA_LOGE("Connect retries reached max(%1%)", BROOKESIA_SERVICE_WIFI_CONNECT_RETRIES_MAX);
        connect_retries_ = 0;
    }

    BROOKESIA_CHECK_FALSE_RETURN(
        mark_ap_connectable(connecting_ap_info_, false), false, "Failed to mark AP not connectable"
    );
    if (!is_auto_connect_allowed()) {
        BROOKESIA_LOGD("Auto connect is not allowed, skip reconnect");
        return true;
    }

    ConnectApInfo history_ap_info;
    if (!get_last_connectable_ap_info(history_ap_info)) {
        BROOKESIA_LOGD("No history connectable AP found, skip auto reconnect");
        return true;
    }

    BROOKESIA_LOGD("Reconnect to history AP: %1%", BROOKESIA_DESCRIBE_TO_STR(history_ap_info));
    return trigger_connect_to_ap(history_ap_info);
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
                station_iface_->mark_target_connect_ap_info_connectable(false);
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
    // Drain in place. Avoid the `std::queue<>().swap()` "shrink-to-fit" idiom: the
    // temporary empty queue's default-constructed deque also allocates an 8-pointer map
    // plus one 512-byte node (~544 B), so the swap pattern simply churns ~1 KB of
    // internal heap per stop without freeing anything net.
    while (!general_action_pending_queue_.empty()) {
        general_action_pending_queue_.pop();
    }
    while (!general_action_ready_queue_.empty()) {
        general_action_ready_queue_.pop();
    }
}

void Wifi::make_general_action_ready_queue(GeneralAction action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: action(%1%)", BROOKESIA_DESCRIBE_TO_STR(action));

    GeneralAction pre_action = GeneralAction::Max;
    switch (action) {
    case GeneralAction::Deinit:
        if (is_general_event_ready(GeneralEvent::Started) ||
                is_general_event_ready(GeneralEvent::Connected)) {
            BROOKESIA_LOGD("WiFi is started or connected");
            pre_action = GeneralAction::Stop;
        }
        break;
    case GeneralAction::Start:
        if (is_general_event_ready(GeneralEvent::Deinited)) {
            BROOKESIA_LOGD("WiFi is deinited");
            pre_action = GeneralAction::Init;
        }
        break;
    case GeneralAction::Stop:
        if (is_general_event_ready(GeneralEvent::Connected)) {
            BROOKESIA_LOGD("WiFi is connected");
            pre_action = GeneralAction::Disconnect;
        }
        break;
    case GeneralAction::Connect:
        if (is_general_event_ready(GeneralEvent::Stopped) ||
                is_general_event_ready(GeneralEvent::Deinited)) {
            pre_action = GeneralAction::Start;
        } else if (is_general_event_ready(GeneralEvent::Connected)) {
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

    switch (action) {
    case GeneralAction::Connect:
    case GeneralAction::Disconnect:
    case GeneralAction::Stop:
    case GeneralAction::Deinit:
        stop_auto_connect_task();
        break;
    default:
        break;
    }

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
