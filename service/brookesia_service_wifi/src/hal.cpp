/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "nvs_flash.h"
#include "esp_netif.h"
#include "brookesia/service_wifi/macro_configs.h"
#if !defined(BROOKESIA_SERVICE_WIFI_HAL_ENABLE_DEBUG_LOG)
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "brookesia/service_wifi/hal.hpp"

// Since it is necessary to call task_scheduler in wifi_event_handler, make sure the sys_event stack size is sufficient
#if CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE < 3072
#   error "`CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE` must be greater than `3072` to avoid stack overflow"
#endif

namespace esp_brookesia::service::wifi {

constexpr uint32_t WAIT_EVENT_STARTED_TIMEOUT_MS = BROOKESIA_SERVICE_WIFI_HAL_WAIT_EVENT_STARTED_TIMEOUT_MS;
constexpr uint32_t WAIT_EVENT_STOPPED_TIMEOUT_MS = BROOKESIA_SERVICE_WIFI_HAL_WAIT_EVENT_STOPPED_TIMEOUT_MS;
constexpr uint32_t WAIT_EVENT_CONNECTED_TIMEOUT_MS = BROOKESIA_SERVICE_WIFI_HAL_WAIT_EVENT_CONNECTED_TIMEOUT_MS;
constexpr uint32_t WAIT_EVENT_DISCONNECTED_TIMEOUT_MS = BROOKESIA_SERVICE_WIFI_HAL_WAIT_EVENT_DISCONNECTED_TIMEOUT_MS;

Hal::~Hal()
{
    deinit();
}

bool Hal::init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(operation_mutex_);

    lib_utils::FunctionGuard deinit_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        deinit_internal();
    });

    is_initialized_.store(true);

    {
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_in_ext = false,
        });
        auto future = std::async(std::launch::async, []() {
            esp_err_t ret = nvs_flash_init();
            if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) || (ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
                BROOKESIA_LOGI("NVS partition was truncated and needs to be erased");
                BROOKESIA_CHECK_ESP_ERR_RETURN(nvs_flash_erase(), false, "Erase NVS flash failed");
                BROOKESIA_CHECK_ESP_ERR_RETURN(nvs_flash_init(), false, "Init NVS flash failed");
            } else {
                BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Initialize NVS flash failed");
            }
            return true;
        });
        BROOKESIA_CHECK_FALSE_RETURN(future.get(), false, "Initialize NVS flash failed");
    }

    esp_netif_init();

    auto result = esp_event_loop_create_default();
    if (result != ESP_ERR_INVALID_STATE) {
        BROOKESIA_CHECK_ESP_ERR_RETURN(result, false, "Create default event loop failed");
    }

    BROOKESIA_LOGI("HAL initialized");

    deinit_guard.release();

    return true;
}

void Hal::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_initialized()) {
        BROOKESIA_LOGD("Not initialized, skip");
        return;
    }

    if (is_running()) {
        BROOKESIA_LOGD("Running, stop it first");
        stop();
    }

    boost::lock_guard lock(operation_mutex_);
    deinit_internal();
}

void Hal::deinit_internal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    is_initialized_.store(false);

    // esp_netif_deinit();

    BROOKESIA_LOGI("HAL deinitialized");
}

bool Hal::start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(operation_mutex_);

    lib_utils::FunctionGuard stop_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop_internal();
    });

    is_running_.store(true);

    // Keep the execution order of WiFi events and general event callback
    BROOKESIA_CHECK_FALSE_RETURN(task_scheduler_->configure_group(WIFI_EVENT_PROCESS_GROUP, {
        .enable_serial_execution = true
    }), false, "Failed to configure wifi event group");
    BROOKESIA_CHECK_FALSE_RETURN(task_scheduler_->configure_group(GENERAL_CALLBACK_GROUP, {
        .enable_serial_execution = true
    }), false, "Failed to configure general callback group");

    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_ip_event_handler, this, &wifi_event_handler_instance_
        ), false, "Register WiFi event handler failed"
    );
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_event_handler_instance_register(
            IP_EVENT, ESP_EVENT_ANY_ID, on_wifi_ip_event_handler, this, &ip_event_handler_instance_
        ), false, "Register IP event handler failed"
    );

    reset_internal();

    stop_guard.release();

    return true;
}

void Hal::stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_general_event_ready(GeneralEvent::Started)) {
        BROOKESIA_CHECK_FALSE_EXECUTE(do_general_action(GeneralAction::Stop), {}, {
            BROOKESIA_LOGE("Stop WiFi failed when deinit");
        });
    }

    if (is_general_event_ready(GeneralEvent::Inited)) {
        BROOKESIA_CHECK_FALSE_EXECUTE(do_general_action(GeneralAction::Deinit), {}, {
            BROOKESIA_LOGE("Deinit WiFi failed when deinit");
        });
    }

    boost::lock_guard lock(operation_mutex_);
    stop_internal();
}

void Hal::stop_internal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (wifi_event_handler_instance_ != nullptr) {
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler_instance_), {}, {
            BROOKESIA_LOGE("Unregister WiFi event handler failed");
        });
        wifi_event_handler_instance_ = nullptr;
    }
    if (ip_event_handler_instance_ != nullptr) {
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(
        esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler_instance_), {}, {
            BROOKESIA_LOGE("Unregister IP event handler failed");
        });
        ip_event_handler_instance_ = nullptr;
    }

    reset_internal();
}

void Hal::reset()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(operation_mutex_);
    reset_internal();
}

void Hal::reset_internal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    {
        boost::lock_guard lock(state_mutex_);
        state_flags_.reset();
    }

    // Avoid warning: missing field initializers in initializer list
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    target_wifi_config_ = {
        .sta = {
            .ssid = "",
            .password = "",
            .scan_method = WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .threshold = {
                .rssi = -127,
                .authmode = WIFI_AUTH_OPEN,
            },
        },
    };
#pragma GCC diagnostic pop
    scan_ap_periodic_task = 0;
    scan_ap_timeout_task = 0;

    reset_data_internal();
}

void Hal::reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(operation_mutex_);
    reset_data_internal();
}

void Hal::reset_data_internal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    target_connect_ap_info_ = ConnectApInfo();
    last_connected_ap_info_ = ConnectApInfo();
    connected_ap_info_list_.clear();

    /* Load scan params from function schema */
    auto scan_params_schema = Helper::get_function_schema(Helper::FunctionId::SetScanParams);
    if (scan_params_schema) {
        scan_params_ = ScanParams{
            .ap_count = static_cast<size_t>(
                std::get<double>(scan_params_schema->parameters[0].default_value.value())),
            .interval_ms = static_cast<uint32_t>(
                std::get<double>(scan_params_schema->parameters[1].default_value.value())),
            .timeout_ms = static_cast<uint32_t>(
                std::get<double>(scan_params_schema->parameters[2].default_value.value()))
        };
    }
}

bool Hal::do_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Check if netif already exists
    if (sta_netif_ != nullptr) {
        BROOKESIA_LOGD("Already initialized, skip");
        return true;
    }

    {
        // Since esp_wifi_init() may operate on NVS, it is necessary to ensure execution in a thread with an SRAM stack
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_in_ext = false,
        });
        auto future = std::async(std::launch::async, []() {
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_init(&cfg), false, "Initialize WiFi failed");
            return true;
        });
        BROOKESIA_CHECK_FALSE_RETURN(future.get(), false, "Initialize WiFi failed");
    }
    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_set_mode(WIFI_MODE_STA), false, "Set WiFi mode failed");

    if (sta_netif_ == nullptr) {
        // If netif doesn't exist, create a new one
        BROOKESIA_LOGD("No existing STA netif found, creating new one");
        sta_netif_ = esp_netif_create_default_wifi_sta();
        BROOKESIA_CHECK_NULL_RETURN(sta_netif_, false, "Create default STA netif failed");
    }

    return true;
}

bool Hal::do_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if !defined(CONFIG_ESP_HOSTED_ENABLED)
    if (sta_netif_ != nullptr) {
        esp_netif_destroy_default_wifi(sta_netif_);
        sta_netif_ = nullptr;
    }

    BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_wifi_deinit(), {}, {
        BROOKESIA_LOGE("Deinitialize WiFi failed");
    });
#else
    BROOKESIA_LOGW("Not supported on ESP32-P4, skip");
#endif

    return true;
}

bool Hal::do_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_set_mode(WIFI_MODE_STA), false, "Set WiFi mode failed");
    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_start(), false, "Failed to start WiFi");

    return true;
}

bool Hal::do_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_wifi_stop(), {}, {
        BROOKESIA_LOGE("Stop WiFi failed");
    });

    return true;
}

bool Hal::do_connect()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI("Connecting to %1% (password: %2%)...", target_connect_ap_info_.ssid, target_connect_ap_info_.password);

    strncpy(
        reinterpret_cast<char *>(target_wifi_config_.sta.ssid), target_connect_ap_info_.ssid.c_str(),
        sizeof(target_wifi_config_.sta.ssid) - 1
    );
    if (!target_connect_ap_info_.password.empty()) {
        strncpy(
            reinterpret_cast<char *>(target_wifi_config_.sta.password), target_connect_ap_info_.password.c_str(),
            sizeof(target_wifi_config_.sta.password) - 1
        );
    } else {
        target_wifi_config_.sta.password[0] = '\0';
    }

    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_wifi_set_config(WIFI_IF_STA, &target_wifi_config_), false, "Failed to set WiFi config"
    );
    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_connect(), false, "Failed to connect WiFi");

    return true;
}

bool Hal::do_disconnect()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_wifi_disconnect(), {}, {
        BROOKESIA_LOGE("Disconnect WiFi failed");
    });

    return true;
}

bool Hal::do_scan_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_scanning()) {
        BROOKESIA_LOGD("Scan is already running, skip");
        return true;
    }

    is_scanning_.store(true);
    lib_utils::FunctionGuard scan_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        is_scanning_.store(false);
    });

    {
        boost::lock_guard lock(operation_mutex_);
        BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_scan_start(nullptr, false), false, "Start scan failed");
    }

    scan_guard.release();

    return true;
}

bool Hal::do_scan_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_scanning()) {
        BROOKESIA_LOGD("Scan is not running, skip");
        return true;
    }

    {
        boost::lock_guard lock(operation_mutex_);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_wifi_scan_stop(), {}, {
            BROOKESIA_LOGE("Stop WiFi scan failed");
        });
    }

    return true;
}

bool Hal::do_general_action(GeneralAction action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: action(%1%)", BROOKESIA_DESCRIBE_TO_STR(action));

    GeneralEvent target_event = get_general_action_target_event(action);
    if (is_general_event_ready(target_event)) {
        BROOKESIA_LOGD("Event(%1%) is already matched, skip", BROOKESIA_DESCRIBE_TO_STR(target_event));
        return true;
    }

    // Process special cases before doing the general action
    switch (action) {
    case GeneralAction::Connect:
        if (is_scanning()) {
            BROOKESIA_LOGD("Stop AP scan before connecting to AP");
            do_scan_stop();
        }
        break;
    case GeneralAction::Stop:
        if (is_scan_task_running()) {
            BROOKESIA_LOGD("Stop AP scan before stopping WiFi");
            stop_ap_scan();
        }
        break;
    default:
        break;
    }

    BROOKESIA_LOGI("WiFi %1% running...", BROOKESIA_DESCRIBE_TO_STR(action));

    if (general_action_callback_ != nullptr) {
        auto task = [this, action]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            general_action_callback_(action);
        };
        BROOKESIA_CHECK_FALSE_EXECUTE(
        task_scheduler_->post(std::move(task), nullptr, GENERAL_CALLBACK_GROUP), {}, {
            BROOKESIA_LOGE("Post general callback task failed");
        });
    }

    GeneralStateFlagBit state_flag_bit = get_general_action_state_flag_bit(action);
    if (state_flag_bit != GeneralStateFlagBit::Max) {
        boost::lock_guard lock(state_mutex_);
        state_flags_.set(BROOKESIA_DESCRIBE_ENUM_TO_NUM(state_flag_bit));
    }

    bool result = true;
    bool need_trigger_event = false;
    {
        boost::lock_guard lock(operation_mutex_);
        switch (action) {
        case GeneralAction::Init:
            result = do_init();
            need_trigger_event = true;
            break;
        case GeneralAction::Deinit:
            result = do_deinit();
            need_trigger_event = true;
            break;
        case GeneralAction::Start:
            result = do_start();
            break;
        case GeneralAction::Stop:
            result = do_stop();
            break;
        case GeneralAction::Connect:
            connecting_ap_info_ = target_connect_ap_info_;
            result = do_connect();
            break;
        case GeneralAction::Disconnect:
            result = do_disconnect();
            break;
        default:
            result = false;
            break;
        }
    }

    lib_utils::FunctionGuard restore_guard([this, state_flag_bit]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        if (state_flag_bit != GeneralStateFlagBit::Max) {
            boost::lock_guard lock(state_mutex_);
            state_flags_.reset(BROOKESIA_DESCRIBE_ENUM_TO_NUM(state_flag_bit));
        }
    });

    BROOKESIA_CHECK_FALSE_RETURN(
        result, false, "WiFi %1% failed", BROOKESIA_DESCRIBE_TO_STR(action)
    );

    uint32_t timeout_ms = get_general_event_wait_timeout_ms(target_event);
    if ((target_event != GeneralEvent::Max) && (timeout_ms > 0)) {
        BROOKESIA_LOGI("WiFi waiting for event: %1%...", BROOKESIA_DESCRIBE_TO_STR(target_event));
        if (!wait_for_general_event(target_event, timeout_ms)) {
            BROOKESIA_LOGE("Wait for event timeout (%1%ms)", timeout_ms);
            return false;
        }
    }

    if (need_trigger_event) {
        trigger_general_event(target_event);
    }

    BROOKESIA_LOGI("WiFi %1% finished", BROOKESIA_DESCRIBE_TO_STR(action));

    return true;
}

bool Hal::set_scan_params(const ScanParams &params)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: params(%1%)", BROOKESIA_DESCRIBE_TO_STR(params));

    BROOKESIA_CHECK_OUT_RANGE_RETURN(params.ap_count, 1, SIZE_MAX, false, "Invalid AP count");
    BROOKESIA_CHECK_OUT_RANGE_RETURN(params.interval_ms, 1, UINT32_MAX, false, "Invalid interval");
    BROOKESIA_CHECK_OUT_RANGE_RETURN(params.timeout_ms, params.interval_ms + 1, UINT32_MAX, false, "Invalid timeout");

    boost::lock_guard lock(operation_mutex_);
    scan_params_ = params;

    return true;
}

bool Hal::start_ap_scan()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Stop scan and delayed tasks
    stop_ap_scan();

    lib_utils::FunctionGuard stop_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop_ap_scan();
    });

    if (is_general_action_running(GeneralAction::Connect)) {
        BROOKESIA_LOGD("Skip do scan start during connecting to AP");
    } else if (is_general_event_ready(GeneralEvent::Stopped)) {
        BROOKESIA_LOGD("Skip do scan start when WiFi is stopped");
    } else {
        // Start scan immediately
        BROOKESIA_CHECK_FALSE_RETURN(do_scan_start(), false, "Start scan failed");
    }

    // Post periodic task to start next scan after interval
    auto periodic_task = [this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        if (is_general_event_ready(GeneralEvent::Stopped) || is_general_action_running(GeneralAction::Connect)) {
            BROOKESIA_LOGD("Skip AP scan because WiFi is stopped or connecting");
            return true;
        }
        BROOKESIA_CHECK_FALSE_RETURN(do_scan_start(), false, "Do scan start failed");
        return true;
    };
    BROOKESIA_CHECK_FALSE_RETURN(
        task_scheduler_->post_periodic(std::move(periodic_task), scan_params_.interval_ms, &scan_ap_periodic_task),
        false, "Post scan AP periodic task failed"
    );

    // Post delayed task to stop scan after timeout
    auto delayed_task = [this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop_ap_scan();
    };
    BROOKESIA_CHECK_FALSE_RETURN(
        task_scheduler_->post_delayed(std::move(delayed_task), scan_params_.timeout_ms, &scan_ap_timeout_task), false,
        "Post scan AP delayed task failed"
    );

    stop_guard.release();

    return true;
}

void Hal::stop_ap_scan()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (scan_ap_periodic_task != 0) {
        task_scheduler_->cancel(scan_ap_periodic_task);
        scan_ap_periodic_task = 0;
    }
    if (scan_ap_timeout_task != 0) {
        task_scheduler_->cancel(scan_ap_timeout_task);
        scan_ap_timeout_task = 0;
    }
    do_scan_stop();
}

uint32_t Hal::get_general_event_wait_timeout_ms(GeneralEvent event)
{
    switch (event) {
    case GeneralEvent::Started:
        return WAIT_EVENT_STARTED_TIMEOUT_MS;
    case GeneralEvent::Stopped:
        return WAIT_EVENT_STOPPED_TIMEOUT_MS;
    case GeneralEvent::Connected:
        return WAIT_EVENT_CONNECTED_TIMEOUT_MS;
    case GeneralEvent::Disconnected:
        return WAIT_EVENT_DISCONNECTED_TIMEOUT_MS;
    default:
        return 0;
    }
}

bool Hal::is_general_event_ready_internal(GeneralEvent event)
{
    bool need_revert = false;
    GeneralStateFlagBit flag_bit = GeneralStateFlagBit::Max;

    switch (event) {
    case GeneralEvent::Inited:
        flag_bit = GeneralStateFlagBit::Inited;
        break;
    case GeneralEvent::Deinited:
        flag_bit = GeneralStateFlagBit::Inited;
        need_revert = true;
        break;
    case GeneralEvent::Started:
        flag_bit = GeneralStateFlagBit::Started;
        break;
    case GeneralEvent::Stopped:
        flag_bit = GeneralStateFlagBit::Started;
        need_revert = true;
        break;
    case GeneralEvent::Connected:
        flag_bit = GeneralStateFlagBit::Connected;
        break;
    case GeneralEvent::Disconnected:
        flag_bit = GeneralStateFlagBit::Connected;
        need_revert = true;
        break;
    default:
        return false;
    }

    bool result = state_flags_.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit));

    return need_revert ? !result : result;
}

bool Hal::is_general_event_changed(
    GeneralEvent event, const GeneralStateFlags &old_flags, const GeneralStateFlags &new_flags
)
{
    auto flag_bit = get_general_event_state_flag_bit(event);
    if (flag_bit == GeneralStateFlagBit::Max) {
        return false;
    }
    return old_flags.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit)) !=
           new_flags.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit));
}

void Hal::trigger_general_event(GeneralEvent event)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event(%1%)", BROOKESIA_DESCRIBE_TO_STR(event));

    bool need_notify = false;
    GeneralStateFlagBit flag_bit = get_general_event_state_flag_bit(event);
    GeneralStateFlags old_state_flags;
    GeneralStateFlags new_state_flags;
    {
        boost::lock_guard lock(state_mutex_);
        old_state_flags = state_flags_;
        bool bit_value = true;

        switch (event) {
        case GeneralEvent::Inited:
            bit_value = true;
            break;
        case GeneralEvent::Deinited:
            bit_value = false;
            break;
        case GeneralEvent::Started:
            bit_value = true;
            need_notify = true;
            break;
        case GeneralEvent::Stopped:
            bit_value = false;
            need_notify = true;
            break;
        case GeneralEvent::Connected:
            bit_value = true;
            need_notify = true;
            break;
        case GeneralEvent::Disconnected:
            bit_value = false;
            need_notify = true;
            break;
        default:
            BROOKESIA_LOGD("Ignored");
            break;
        }

        if (flag_bit != GeneralStateFlagBit::Max) {
            state_flags_.set(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit), bit_value);
        }

        new_state_flags = state_flags_;

        if (need_notify) {
            state_condition_variable_.notify_all();
        }
    }

    if (general_event_callback_ != nullptr) {
        auto task = [this, event, old_state_flags, new_state_flags]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            general_event_callback_(event, old_state_flags, new_state_flags);
        };
        BROOKESIA_CHECK_FALSE_EXIT(
            task_scheduler_->post(std::move(task), nullptr, GENERAL_CALLBACK_GROUP),
            "Post general callback task failed"
        );
    }
}

bool Hal::wait_for_general_event(GeneralEvent event, uint32_t timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event(%1%), timeout_ms(%2%)", BROOKESIA_DESCRIBE_TO_STR(event), timeout_ms);

    if (is_general_event_ready(event)) {
        BROOKESIA_LOGD("Event is already matched, skip");
        return true;
    }

    boost::unique_lock lock(state_mutex_);
    bool is_ready = state_condition_variable_.wait_for(lock, boost::chrono::milliseconds(timeout_ms),
    [this, &event] {
        return is_general_event_ready_internal(event);
    });
    if (!is_ready) {
        BROOKESIA_LOGE("Wait for event ready timeout (%1%ms)", timeout_ms);
        return false;
    }

    return true;
}

bool Hal::set_target_connect_ap_info(const ConnectApInfo &ap_info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: ap_info(%1%)", BROOKESIA_DESCRIBE_TO_STR(ap_info));

    BROOKESIA_CHECK_FALSE_RETURN(!ap_info.ssid.empty(), false, "Invalid SSID");

    boost::lock_guard lock(operation_mutex_);
    target_connect_ap_info_ = ap_info;

    return true;
}

void Hal::set_last_connected_ap_info(const ConnectApInfo &ap_info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: ap_info(%1%)", BROOKESIA_DESCRIBE_TO_STR(ap_info));

    boost::lock_guard lock(operation_mutex_);
    last_connected_ap_info_ = ap_info;
}

void Hal::add_connected_ap_info(const ConnectApInfo &ap_info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: ap_info(%1%)", BROOKESIA_DESCRIBE_TO_STR(ap_info));

    boost::lock_guard lock(operation_mutex_);

    for (auto &info : connected_ap_info_list_) {
        if (info.ssid == ap_info.ssid) {
            info = ap_info;
            return;
        }
    }

    connected_ap_info_list_.push_back(ap_info);
}

void Hal::remove_connected_ap_info_by_ssid(const std::string &ssid)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: ssid(%1%)", ssid);

    boost::lock_guard lock(operation_mutex_);
    connected_ap_info_list_.remove_if([&ssid](const ConnectApInfo & info) {
        return info.ssid == ssid;
    });
}

void Hal::clear_connected_ap_infos()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(operation_mutex_);
    connected_ap_info_list_.clear();
}

bool Hal::process_wifi_event(wifi_event_t event, void *data)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event(%1%), data(%2%)", event, data);

    auto event_id = static_cast<int>(event);

    switch (event_id) {
    case WIFI_EVENT_STA_START:
        trigger_general_event(GeneralEvent::Started);
        break;
    case WIFI_EVENT_STA_STOP:
        trigger_general_event(GeneralEvent::Stopped);
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        trigger_general_event(GeneralEvent::Disconnected);
        break;
    case WIFI_EVENT_SCAN_DONE: {
        {
            boost::lock_guard lock(operation_mutex_);
            is_scanning_.store(false);
        }
        // If WiFi is stopping or already stopped, skip update scan AP infos
        if (is_general_action_running(GeneralAction::Stop) ||
                is_general_event_ready(GeneralEvent::Stopped) ||
                is_general_action_running(GeneralAction::Deinit) ||
                is_general_event_ready(GeneralEvent::Deinited)) {
            break;
        }
        // Update scan AP infos
        BROOKESIA_CHECK_FALSE_EXECUTE(update_scan_ap_infos(), {}, {
            BROOKESIA_LOGE("Update scan AP infos failed");
        });
        break;
    }
    default:
        BROOKESIA_LOGD("Ignored");
    }

    return true;
}

bool Hal::process_ip_event(ip_event_t event, void *data)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event(%1%), data(%2%)", event, data);

    auto event_id = static_cast<int>(event);

    switch (event_id) {
    case IP_EVENT_STA_GOT_IP: {
        {
            boost::lock_guard lock(operation_mutex_);
            target_connect_ap_info_.is_connectable = true;
        }
        trigger_general_event(GeneralEvent::Connected);
        break;
    }
    default:
        BROOKESIA_LOGD("Ignored");
    }

    return true;
}

void Hal::on_wifi_ip_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: arg(%1%), base(%2%), id(%3%), data(%4%)", arg, base, id, data);

    auto context = static_cast<Hal *>(arg);
    BROOKESIA_CHECK_NULL_EXIT(context, "Invalid context");

    auto process_event_task = [context, base, id, data]() {
        BROOKESIA_LOG_TRACE_GUARD();
        if (base == WIFI_EVENT) {
            BROOKESIA_CHECK_FALSE_EXIT(
                context->process_wifi_event(static_cast<wifi_event_t>(id), data), "Process WiFi event failed"
            );
        } else if (base == IP_EVENT) {
            BROOKESIA_CHECK_FALSE_EXIT(
                context->process_ip_event(static_cast<ip_event_t>(id), data), "Process IP event failed"
            );
        } else {
            BROOKESIA_CHECK_FALSE_EXIT(false, "Invalid event base: %1%", base);
        }
    };
    BROOKESIA_CHECK_FALSE_EXIT(
        context->task_scheduler_->post(std::move(process_event_task), nullptr, WIFI_EVENT_PROCESS_GROUP),
        "Post process event task failed"
    );
}

bool Hal::post_scan_interval_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return true;
}

bool Hal::update_scan_ap_infos()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    uint16_t actual_ap_count = 0;
    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_scan_get_ap_num(&actual_ap_count), false, "Get AP number failed");

    if (actual_ap_count == 0) {
        BROOKESIA_LOGD("No AP found, skip");
        return true;
    }

    actual_ap_count = std::min(actual_ap_count, static_cast<uint16_t>(scan_params_.ap_count));
    std::vector<wifi_ap_record_t> ap_records(actual_ap_count);
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_wifi_scan_get_ap_records(&actual_ap_count, ap_records.data()), false, "Get AP records failed"
    );

    BROOKESIA_LOGI("Scanned AP count: %d", actual_ap_count);

    if (scan_ap_infos_updated_callback_ != nullptr) {
        std::vector<ApInfo> ap_infos;
        for (const auto &ap_record : ap_records) {
            ap_infos.emplace_back(
                reinterpret_cast<const char *>(ap_record.ssid), ap_record.authmode != WIFI_AUTH_OPEN, ap_record.rssi
            );
        }
        scan_ap_infos_ = std::move(BROOKESIA_DESCRIBE_TO_JSON(ap_infos).as_array());
        scan_ap_infos_updated_callback_(scan_ap_infos_);
    }

    return true;
}

} // namespace esp_brookesia::service::wifi
