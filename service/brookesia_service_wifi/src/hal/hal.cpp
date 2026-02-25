/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <array>
#include "esp_mac.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "lwip/sockets.h"
#include "brookesia/service_wifi/macro_configs.h"
#if !defined(BROOKESIA_SERVICE_WIFI_HAL_ENABLE_DEBUG_LOG)
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "brookesia/service_wifi/hal/hal.hpp"
#include "brookesia/service_wifi/service_wifi.hpp"

// Since it is necessary to call task_scheduler in wifi_event_handler, make sure the sys_event stack size is sufficient
#if CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE < 3072
#   error "`CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE` must be greater than `3072` to avoid stack overflow"
#endif

namespace esp_brookesia::service::wifi {

namespace {
wifi_sta_config_t get_sta_config(const ConnectApInfo &ap_info)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: ap_info(%1%)", BROOKESIA_DESCRIBE_TO_STR(ap_info));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    wifi_sta_config_t cfg = {
        .ssid = "",
        .password = "",
        .scan_method = WIFI_FAST_SCAN,
        .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
        .threshold = {
            .rssi = -127,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
#pragma GCC diagnostic pop

    if (!ap_info.ssid.empty()) {
        snprintf(reinterpret_cast<char *>(cfg.ssid), sizeof(cfg.ssid), "%s", ap_info.ssid.c_str());
    }
    if (!ap_info.password.empty()) {
        snprintf(reinterpret_cast<char *>(cfg.password), sizeof(cfg.password), "%s", ap_info.password.c_str());
        cfg.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    return cfg;
}

ApInfo get_ap_info(const wifi_ap_record_t &ap_record)
{
    return ApInfo{
        .ssid = std::string(reinterpret_cast<const char *>(ap_record.ssid)),
        .is_locked = ap_record.authmode != WIFI_AUTH_OPEN,
        .rssi = ap_record.rssi,
        .signal_level = ApInfo::get_signal_level(ap_record.rssi),
        .channel = ap_record.primary,
    };
}
} // anonymous namespace

Hal::~Hal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    deinit();
}

bool Hal::init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    lib_utils::FunctionGuard deinit_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        deinit();
    });

    is_initialized_ = true;

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

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        softap_ = std::make_unique<SoftAp>(this), false, "Create SoftAP failed"
    );

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

    reset_data();

    // esp_netif_deinit();
    softap_.reset();

    is_initialized_ = false;
}

bool Hal::start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    task_scheduler_ = Wifi::get_instance().get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(task_scheduler_, false, "Invalid task scheduler");

    lib_utils::FunctionGuard stop_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop();
    });

    is_running_ = true;

    auto wifi_ip_event_handler = +[](void *arg, esp_event_base_t base, int32_t id, void *data) {
        BROOKESIA_LOG_TRACE_GUARD();
        BROOKESIA_LOGD("Params: arg(%1%), base(%2%), id(%3%), data(%4%)", arg, base, id, data);

        auto hal = static_cast<Hal *>(arg);
        BROOKESIA_CHECK_NULL_EXIT(hal, "Invalid HAL");

        auto task_scheduler = hal->task_scheduler_;
        BROOKESIA_CHECK_NULL_EXIT(task_scheduler, "Invalid task scheduler");

        std::function<void()> task_func;
        if (base == WIFI_EVENT) {
            task_func = [hal, id = static_cast<uint8_t>(id)]() {
                BROOKESIA_LOG_TRACE_GUARD();
                BROOKESIA_CHECK_FALSE_EXIT(hal->on_wifi_event(id), "Process WiFi event failed");
            };
        } else if (base == IP_EVENT) {
            task_func = [hal, id = static_cast<uint8_t>(id)]() {
                BROOKESIA_LOG_TRACE_GUARD();
                BROOKESIA_CHECK_FALSE_EXIT(hal->on_ip_event(id), "Process IP event failed");
            };
        }
        if (task_func) {
            BROOKESIA_CHECK_FALSE_EXIT(
                task_scheduler->post(task_func, nullptr, Wifi::get_instance().get_state_task_group()),
                "Post process event task failed"
            );
        }
    };
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_ip_event_handler, this,
            reinterpret_cast<esp_event_handler_instance_t *>(&wifi_event_handler_instance_)
        ), false, "Register WiFi event handler failed"
    );
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_event_handler_instance_register(
            IP_EVENT, ESP_EVENT_ANY_ID, wifi_ip_event_handler, this,
            reinterpret_cast<esp_event_handler_instance_t *>(&ip_event_handler_instance_)
        ), false, "Register IP event handler failed"
    );

    stop_guard.release();

    return true;
}

void Hal::stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Try to stop and deinit WiFi
    BROOKESIA_CHECK_FALSE_EXECUTE(do_general_action(GeneralAction::Stop), {}, {
        BROOKESIA_LOGE("Stop WiFi failed when deinit");
    });
    BROOKESIA_CHECK_FALSE_EXECUTE(do_general_action(GeneralAction::Deinit), {}, {
        BROOKESIA_LOGE("Deinit WiFi failed when deinit");
    });

    // Unregister event handlers
    if (wifi_event_handler_instance_ != nullptr) {
        auto result = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler_instance_);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(result, {}, {
            BROOKESIA_LOGE("Unregister WiFi event handler failed");
        });
        wifi_event_handler_instance_ = nullptr;
    }
    if (ip_event_handler_instance_ != nullptr) {
        auto result = esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler_instance_);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(result, {}, {
            BROOKESIA_LOGE("Unregister IP event handler failed");
        });
        ip_event_handler_instance_ = nullptr;
    }

    // Reset local variables
    task_scheduler_.reset();
    state_flags_.reset();

    is_running_ = false;
}

void Hal::reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    set_target_connect_ap_info(ConnectApInfo());
    set_last_connected_ap_info(ConnectApInfo());
    set_connected_ap_infos(std::list<ConnectApInfo>());
    set_scan_params(ScanParams());
    if (softap_ != nullptr) {
        softap_->reset_data();
    }
}

bool Hal::do_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Check if netif already exists
    if (sta_netif_ != nullptr) {
        BROOKESIA_LOGD("Already initialized, directly trigger Inited event");
        trigger_general_event(GeneralEvent::Inited);
        return true;
    }

    sta_netif_ = esp_netif_create_default_wifi_sta();
    BROOKESIA_CHECK_NULL_RETURN(sta_netif_, false, "Create default STA netif failed");

    {
        BROOKESIA_LOGD("Initialize WiFi in a thread");
        // Since esp_wifi_init() may operate on NVS, it is necessary to ensure execution in a thread with an SRAM stack
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_in_ext = false,
        });
        boost::thread([this, task_scheduler = task_scheduler_]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            BROOKESIA_CHECK_ESP_ERR_EXIT(esp_wifi_init(&cfg), "Initialize WiFi failed");
            BROOKESIA_CHECK_ESP_ERR_EXIT(esp_wifi_set_mode(WIFI_MODE_STA), "Set WiFi mode failed");

            auto task_func = [this]() {
                BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
                trigger_general_event(GeneralEvent::Inited);
            };
            BROOKESIA_CHECK_FALSE_EXIT(
                task_scheduler->post(task_func, nullptr, Wifi::get_instance().get_state_task_group()),
                "Post init task failed"
            );
        }).detach();
    }

    return true;
}

bool Hal::do_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if !CONFIG_ESP_HOSTED_ENABLED
    if (sta_netif_ != nullptr) {
        esp_netif_destroy_default_wifi(reinterpret_cast<esp_netif_t *>(sta_netif_));
        sta_netif_ = nullptr;
    }

    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_deinit(), false, "Deinitialize WiFi failed");
#else
    BROOKESIA_LOGW("Not supported when hosted enabled, skip");
#endif
    trigger_general_event(GeneralEvent::Deinited);

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

    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_stop(), false, "Stop WiFi failed");

    return true;
}

bool Hal::do_connect(const ConnectApInfo &ap_info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(ap_info.is_valid(), false, "Invalid connecting AP info");

    BROOKESIA_LOGI("Connecting to %1%...", ap_info.ssid);
    BROOKESIA_LOGD("With password: %1%", ap_info.password);

    wifi_config_t wifi_config = {
        .sta = get_sta_config(ap_info),
    };
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config), false, "Failed to set WiFi config"
    );
    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_connect(), false, "Failed to connect WiFi");

    return true;
}

bool Hal::do_disconnect()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_disconnect(), false, "Disconnect WiFi failed");

    return true;
}

bool Hal::do_scan_start(const void *params)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_scanning()) {
        BROOKESIA_LOGD("Scan is already running, skip");
        return true;
    }

    if (!is_general_event_ready(GeneralEvent::Started) || is_general_action_running(GeneralAction::Stop)) {
        BROOKESIA_LOGW("WiFi is not started or stopping, cancel start scan");
        return false;
    }

    if (is_general_action_running(GeneralAction::Connect)) {
        BROOKESIA_LOGW("WiFi is connecting, skip AP scan");
        return true;
    }

    is_scanning_ = true;
    lib_utils::FunctionGuard scan_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        is_scanning_ = false;
    });

    lib_utils::FunctionGuard stop_scan_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        do_scan_stop();
    });

    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_wifi_scan_start(reinterpret_cast<const wifi_scan_config_t *>(params), false), false, "Start scan failed"
    );

    scan_guard.release();
    stop_scan_guard.release();

    return true;
}

bool Hal::do_scan_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_scanning()) {
        BROOKESIA_LOGD("Scan is not running, skip");
        return true;
    }

    is_scanning_ = false;

    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_wifi_scan_stop(), false, "Stop WiFi scan failed");

    return true;
}

bool Hal::do_general_action(GeneralAction action, bool is_force)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto action_str = BROOKESIA_DESCRIBE_TO_STR(action);
    BROOKESIA_LOGD("Params: action(%1%), is_force(%2%)", action_str, is_force);

    if (is_general_action_running(action)) {
        BROOKESIA_LOGD("Action(%1%) is already running, skip", action_str);
        return true;
    }

    if (!is_force) {
        BROOKESIA_LOGD("Not force, check if the event is ready");
        GeneralEvent target_event = get_general_action_target_event(action);
#if (!BROOKESIA_LOG_DISABLE_DEBUG_TRACE) && (BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_DEBUG)
        BROOKESIA_LOGD(
            "running action(%1%)", BROOKESIA_DESCRIBE_TO_STR(get_running_general_action())
        );
        auto [event_flag_bit, event_bit_value] = get_general_event_state_flag_bit(target_event);
        BROOKESIA_LOGD(
            "state_flags(%1%), flag bit(%2%), bit value(%3%)", BROOKESIA_DESCRIBE_TO_STR(state_flags_),
            BROOKESIA_DESCRIBE_TO_STR(event_flag_bit), BROOKESIA_DESCRIBE_TO_STR(event_bit_value)
        );
#endif
        if (is_general_event_ready(target_event)) {
            BROOKESIA_LOGD("Event(%1%) is already matched, skip", BROOKESIA_DESCRIBE_TO_STR(target_event));
            return true;
        }
    } else {
        BROOKESIA_LOGD("Force, skip checking event ready");
    }

    // Process special cases before doing the general action
    switch (action) {
    case GeneralAction::Connect:
        do_scan_stop();
        break;
    case GeneralAction::Stop:
        stop_scan();
        stop_connect_task();
        if (softap_ != nullptr) {
            softap_->stop_softap();
        }
        break;
    default:
        break;
    }

    // Publish "General Action Triggered" event before executing `do_xxxx()`, since `do_xxxx()` may call
    // trigger_general_event() directly, which publishes "General Event Happened" event.
    // This ensures correct event ordering: "General Action Triggered" -> "General Event Happened"
    if (!Wifi::get_instance().publish_general_action(action)) {
        BROOKESIA_LOGE("Failed to publish general action triggered event");
    }

    BROOKESIA_LOGI("WiFi do '%1%' running...", action_str);

    // Action running bit should be cleared by call `trigger_general_event()` when the action is done or failed
    update_general_action_state_bit(action, true);

    bool result = true;
    switch (action) {
    case GeneralAction::Init:
        result = do_init();
        break;
    case GeneralAction::Deinit:
        result = do_deinit();
        break;
    case GeneralAction::Start:
        result = do_start();
        break;
    case GeneralAction::Stop:
        result = do_stop();
        break;
    case GeneralAction::Connect:
        // Since there is a reconnection mechanism, the target AP information needs to be set as the connecting
        // AP information outside of `do_connect()`
        set_connecting_ap_info(get_target_connect_ap_info());
        result = do_connect(get_connecting_ap_info());
        break;
    case GeneralAction::Disconnect:
        result = do_disconnect();
        break;
    default:
        BROOKESIA_LOGE("Invalid action: %1%", action_str);
        result = false;
        break;
    }
    if (!result) {
        update_general_state_error(true);
        BROOKESIA_LOGE("WiFi do '%1%' failed", action_str);
        return false;
    }

    BROOKESIA_LOGI("WiFi do '%1%' finished", action_str);

    return true;
}

void Hal::trigger_general_event(GeneralEvent event)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto event_str = BROOKESIA_DESCRIBE_TO_STR(event);
    BROOKESIA_LOGD("Params: event(%1%)", event_str);

    if (is_unexpected_event_processing_) {
        BROOKESIA_LOGD("Unexpected event processing, skip");
        return;
    }

    if (is_general_event_ready(event)) {
        BROOKESIA_LOGD("Event(%1%) is already matched, skip", event_str);
        return;
    }

    auto event_action = get_general_action_from_target_event(event);
    BROOKESIA_CHECK_FALSE_EXIT(
        event_action != GeneralAction::Max, "No corresponding action for event: %1%", event_str
    );

    auto is_unexpected_event = is_general_event_unexpected(event);
    if (is_unexpected_event) {
        BROOKESIA_LOGW("Detected unexpected event: %1%", event_str);

        // Notify unexpected event happened
        if (!Wifi::get_instance().on_hal_unexpected_general_event(event)) {
            BROOKESIA_LOGE("Failed to handle unexpected general event");
        }

        // Clear the current action running bit
        auto running_action = get_running_general_action();
        update_general_action_state_bit(running_action, false);

        // Force do the event action to clear the unexpected event
        BROOKESIA_LOGW("Force to do the event action: %1%", BROOKESIA_DESCRIBE_TO_STR(event_action));
        is_unexpected_event_processing_ = true;
        BROOKESIA_CHECK_FALSE_EXECUTE(do_general_action(event_action, true), {}, {
            BROOKESIA_LOGE("Failed to do general action: %1%", BROOKESIA_DESCRIBE_TO_STR(event_action));
        });
        is_unexpected_event_processing_ = false;
    } else {
        BROOKESIA_LOGI("Detected expected event: %1%", event_str);
    }

    // Clear target event action running bit
    update_general_action_state_bit(event_action, false);
    // Set target event stable bit
    update_general_event_state_bit(event, true);

    // Publish general event happened
    if (!Wifi::get_instance().publish_general_event(event, is_unexpected_event)) {
        BROOKESIA_LOGE("Failed to publish general event");
    }

    // Process event-specific business logic
    switch (event) {
    case GeneralEvent::Started:
        process_general_event_started();
        break;
    case GeneralEvent::Connected:
        process_general_event_connected();
        break;
    case GeneralEvent::Disconnected:
        if (is_unexpected_event) {
            process_general_event_unexpected_disconnected();
        }
        break;
    default:
        break;
    }
}

bool Hal::set_scan_params(const ScanParams &params)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: params(%1%)", BROOKESIA_DESCRIBE_TO_STR(params));

    BROOKESIA_CHECK_OUT_RANGE_RETURN(params.ap_count, 1, SIZE_MAX, false, "Invalid AP count");
    BROOKESIA_CHECK_OUT_RANGE_RETURN(params.interval_ms, 1, UINT32_MAX, false, "Invalid interval");
    BROOKESIA_CHECK_OUT_RANGE_RETURN(params.timeout_ms, params.interval_ms + 1, UINT32_MAX, false, "Invalid timeout");

    scan_params_ = params;

    return true;
}

bool Hal::start_scan()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Stop scan and delayed tasks
    stop_scan();

    lib_utils::FunctionGuard stop_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop_scan();
    });

    // Try to start scan immediately if WiFi is started and not stopping
    if (is_general_event_ready(GeneralEvent::Started) && !is_general_action_running(GeneralAction::Stop)) {
        BROOKESIA_LOGD("Do initial scan");
        BROOKESIA_CHECK_FALSE_RETURN(do_scan_start(), false, "Start scan failed");
    }

    auto task_scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(task_scheduler, false, "Invalid task scheduler");

    auto &scan_params = get_scan_params();

    // Post periodic task to start next scan after interval
    auto periodic_task = [this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        if (!is_general_event_ready(GeneralEvent::Started) || is_general_action_running(GeneralAction::Stop)) {
            BROOKESIA_LOGD("WiFi is not started or stopping, skip scan");
            return true;
        }

        if (!do_scan_start()) {
            BROOKESIA_LOGW("Start scan failed, stop scan");
            stop_scan();
            return false;
        }

        return true;
    };
    BROOKESIA_CHECK_FALSE_RETURN(
        task_scheduler->post_periodic(
            std::move(periodic_task), scan_params.interval_ms, &scan_periodic_task_,
            Wifi::get_instance().get_state_task_group()
        ),
        false, "Post scan AP periodic task failed"
    );

    // Post delayed task to stop scan after timeout
    auto delayed_task = [this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop_scan();
    };
    BROOKESIA_CHECK_FALSE_RETURN(
        task_scheduler->post_delayed(
            std::move(delayed_task), scan_params.timeout_ms, &scan_ap_timeout_task_,
            Wifi::get_instance().get_state_task_group()
        ), false, "Post scan AP delayed task failed"
    );

    stop_guard.release();

    return true;
}

void Hal::stop_scan()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (scan_periodic_task_ != 0) {
        task_scheduler_->cancel(scan_periodic_task_);
        scan_periodic_task_ = 0;
    }
    if (scan_ap_timeout_task_ != 0) {
        task_scheduler_->cancel(scan_ap_timeout_task_);
        scan_ap_timeout_task_ = 0;
    }
    do_scan_stop();
}

void Hal::set_disable_auto_connect(bool disable)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: disable(%1%)", BROOKESIA_DESCRIBE_TO_STR(disable));

    disable_auto_connect_ = disable;
}

GeneralEvent Hal::get_general_action_target_event(GeneralAction action)
{
    switch (action) {
    case GeneralAction::Init:
        return GeneralEvent::Inited;
    case GeneralAction::Deinit:
        return GeneralEvent::Deinited;
    case GeneralAction::Start:
        return GeneralEvent::Started;
    case GeneralAction::Stop:
        return GeneralEvent::Stopped;
    case GeneralAction::Connect:
        return GeneralEvent::Connected;
    case GeneralAction::Disconnect:
        return GeneralEvent::Disconnected;
    default:
        return GeneralEvent::Max;
    }
}

GeneralStateFlagBit Hal::get_general_action_state_flag_bit(GeneralAction action)
{
    switch (action) {
    case GeneralAction::Init:
        return GeneralStateFlagBit::Initing;
    case GeneralAction::Deinit:
        return GeneralStateFlagBit::Deiniting;
    case GeneralAction::Start:
        return GeneralStateFlagBit::Starting;
    case GeneralAction::Stop:
        return GeneralStateFlagBit::Stopping;
    case GeneralAction::Connect:
        return GeneralStateFlagBit::Connecting;
    case GeneralAction::Disconnect:
        return GeneralStateFlagBit::Disconnecting;
    default:
        return GeneralStateFlagBit::Max;
    }
}

std::pair<GeneralStateFlagBit, bool> Hal::get_general_event_state_flag_bit(GeneralEvent event)
{
    bool bit_value = true;
    GeneralStateFlagBit flag_bit = GeneralStateFlagBit::Max;
    switch (event) {
    case GeneralEvent::Inited:
        flag_bit = GeneralStateFlagBit::Inited;
        break;
    case GeneralEvent::Deinited:
        flag_bit = GeneralStateFlagBit::Inited;
        bit_value = false;
        break;
    case GeneralEvent::Started:
        flag_bit = GeneralStateFlagBit::Started;
        break;
    case GeneralEvent::Stopped:
        flag_bit = GeneralStateFlagBit::Started;
        bit_value = false;
        break;
    case GeneralEvent::Connected:
        flag_bit = GeneralStateFlagBit::Connected;
        break;
    case GeneralEvent::Disconnected:
        flag_bit = GeneralStateFlagBit::Connected;
        bit_value = false;
        break;
    default:
        break;
    }

    return {flag_bit, bit_value};
}

bool Hal::get_connectable_ap_info_by_ssid(const std::string &ssid, ConnectApInfo &ap_info)
{
    for (const auto &info : connected_ap_info_list_) {
        // BROOKESIA_LOGD("\t- Found connected AP: %1%", BROOKESIA_DESCRIBE_TO_STR(info));
        if ((info.ssid == ssid) && info.is_connectable) {
            ap_info = info;
            return true;
        }
    }
    return false;
}

bool Hal::get_last_connectable_ap_info(ConnectApInfo &ap_info)
{
    for (auto it = connected_ap_info_list_.rbegin(); it != connected_ap_info_list_.rend(); ++it) {
        const auto &info = *it;
        if (info.is_connectable) {
            ap_info = info;
            return true;
        }
    }
    return false;
}

void Hal::set_connecting_ap_info(const ConnectApInfo &ap_info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: ap_info(%1%)", BROOKESIA_DESCRIBE_TO_STR(ap_info));

    connecting_ap_info_ = ap_info;
}

bool Hal::set_connected_ap_infos(const std::list<ConnectApInfo> &ap_infos)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: ap_infos(%1%)", BROOKESIA_DESCRIBE_TO_STR(ap_infos));

    connected_ap_info_list_ = ap_infos;

    return true;
}

bool Hal::is_general_action_running(GeneralAction action)
{
    GeneralStateFlagBit flag_bit = get_general_action_state_flag_bit(action);
    BROOKESIA_CHECK_FALSE_RETURN(flag_bit != GeneralStateFlagBit::Max, false, "Invalid action");

    return state_flags_.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit));
}

bool Hal::is_general_event_conflicting(GeneralEvent event)
{
    switch (event) {
    case GeneralEvent::Deinited:
        return is_general_action_running(GeneralAction::Init);
    case GeneralEvent::Stopped:
        return is_general_action_running(GeneralAction::Start);
    case GeneralEvent::Disconnected:
        return is_general_action_running(GeneralAction::Connect);
    case GeneralEvent::Connected:
        return is_general_action_running(GeneralAction::Disconnect);
    default:
        return false;
    }
}

bool Hal::is_general_event_ready(GeneralEvent event)
{
    // Check if the corresponding action is running
    if (is_general_action_running(get_general_action_from_target_event(event))) {
        // BROOKESIA_LOGD(
        //     "Event(%1%) is not ready because the corresponding action is running", BROOKESIA_DESCRIBE_TO_STR(event)
        // );
        return false;
    }

    // Check for conflicting state transitions: prevent events that contradict the currently running action
    if (is_general_event_conflicting(event)) {
        // BROOKESIA_LOGD(
        //     "Event(%1%) is not ready because it contradicts the currently running action",
        //     BROOKESIA_DESCRIBE_TO_STR(event)
        // );
        return false;
    }

    auto [flag_bit, bit_value] = get_general_event_state_flag_bit(event);
    BROOKESIA_CHECK_FALSE_RETURN(flag_bit != GeneralStateFlagBit::Max, false, "Invalid event");

    return state_flags_.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit)) == bit_value;
}

void Hal::update_general_state_error(bool enable)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: enable(%1%)", BROOKESIA_DESCRIBE_TO_STR(enable));

    state_flags_.set(BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralStateFlagBit::Error), enable);
}

void Hal::update_general_action_state_bit(GeneralAction action, bool enable)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: action(%1%), enable(%2%)", BROOKESIA_DESCRIBE_TO_STR(action), BROOKESIA_DESCRIBE_TO_STR(enable)
    );

    if (action == GeneralAction::Max) {
        BROOKESIA_LOGD("Invalid action, skip");
        return;
    }

    auto flag_bit = get_general_action_state_flag_bit(action);
    if (flag_bit == GeneralStateFlagBit::Max) {
        BROOKESIA_LOGD("Invalid action, skip");
        return;
    }

    state_flags_.set(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit), enable);
}

bool Hal::is_general_event_unexpected(GeneralEvent event)
{
    auto event_action = get_general_action_from_target_event(event);
    if (event_action == GeneralAction::Max) {
        return true;
    }

    auto running_action = get_running_general_action();
    if (running_action == GeneralAction::Max) {
        return true;
    }

    if ((event == GeneralEvent::Disconnected) && (running_action == GeneralAction::Stop)) {
        return false;
    }

    return (running_action != event_action);
}

GeneralAction Hal::get_running_general_action()
{
    for (auto action = static_cast<GeneralAction>(0); action < GeneralAction::Max;
            action = static_cast<GeneralAction>(static_cast<int>(action) + 1)) {
        auto flag_bit = get_general_action_state_flag_bit(action);
        // Only handle transient state flag bits
        BROOKESIA_CHECK_FALSE_RETURN(
            flag_bit != GeneralStateFlagBit::Max, GeneralAction::Max, "Invalid action: %1%",
            BROOKESIA_DESCRIBE_TO_STR(action)
        );
        if (state_flags_.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit))) {
            return action;
        }
    }
    return GeneralAction::Max;
}

GeneralAction Hal::get_general_action_from_target_event(GeneralEvent event)
{
    switch (event) {
    case GeneralEvent::Inited:
        return GeneralAction::Init;
    case GeneralEvent::Deinited:
        return GeneralAction::Deinit;
    case GeneralEvent::Started:
        return GeneralAction::Start;
    case GeneralEvent::Stopped:
        return GeneralAction::Stop;
    case GeneralEvent::Connected:
        return GeneralAction::Connect;
    case GeneralEvent::Disconnected:
        return GeneralAction::Disconnect;
    default:
        return GeneralAction::Max;
    }
}

void Hal::update_general_event_state_bit(GeneralEvent event, bool enable)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: event(%1%), enable(%2%)", BROOKESIA_DESCRIBE_TO_STR(event), BROOKESIA_DESCRIBE_TO_STR(enable)
    );

    if (event == GeneralEvent::Max) {
        BROOKESIA_LOGD("Invalid event, skip");
        return;
    }

    auto [flag_bit, bit_value] = get_general_event_state_flag_bit(event);
    if (flag_bit == GeneralStateFlagBit::Max) {
        BROOKESIA_LOGD("Invalid event, skip");
        return;
    }

    BROOKESIA_LOGD(
        "Target bit(%1%), original state flags(%2%)", BROOKESIA_DESCRIBE_TO_STR(flag_bit),
        BROOKESIA_DESCRIBE_TO_STR(state_flags_)
    );

    state_flags_.set(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit), enable ? bit_value : !bit_value);

    BROOKESIA_LOGD(
        "New state flags(%1%)", BROOKESIA_DESCRIBE_TO_STR(state_flags_)
    );
}

bool Hal::process_general_event_started()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_disable_auto_connect()) {
        BROOKESIA_LOGD("Auto connect is disabled, skip");
        return true;
    }

    BROOKESIA_LOGD("WiFi is started, try to connect to last connectable AP");

    stop_connect_task();

    auto task_scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(task_scheduler, false, "Invalid task scheduler");

    auto delayed_task = [this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD("WiFi connected delayed task started");

        if (is_general_event_ready(GeneralEvent::Stopped) || is_general_action_running(GeneralAction::Stop) ||
                is_general_event_ready(GeneralEvent::Connected) || is_general_action_running(GeneralAction::Connect)) {
            BROOKESIA_LOGD("Skip started connect task");
            return;
        }

        auto target_connect_ap_info = get_target_connect_ap_info();
        if (!target_connect_ap_info.is_valid()) {
            BROOKESIA_LOGD("No target connect AP info, try to get last connected AP info");
            target_connect_ap_info = get_last_connected_ap_info();
        }

        if (!target_connect_ap_info.is_connectable) {
            BROOKESIA_LOGD(
                "Target connect AP(%1%) is not connectable, try to get last connectable one",
                BROOKESIA_DESCRIBE_TO_STR(target_connect_ap_info)
            );
            if (!get_last_connectable_ap_info(target_connect_ap_info)) {
                BROOKESIA_LOGD("No one connectable AP found, use the target AP directly");
            }
        }

        // Check if the AP info is empty
        if (!target_connect_ap_info.is_valid()) {
            BROOKESIA_LOGD("No connectable AP info, skip connect");
            return;
        }

        BROOKESIA_LOGD("Connectable AP is found: %1%", BROOKESIA_DESCRIBE_TO_STR(target_connect_ap_info));
        set_target_connect_ap_info(target_connect_ap_info);

        // Trigger state machine connect action
        auto result = Wifi::get_instance().trigger_general_action(GeneralAction::Connect);
        BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to handle connectable AP found");
    };
    BROOKESIA_CHECK_FALSE_RETURN(task_scheduler->post_delayed(
                                     std::move(delayed_task), BROOKESIA_SERVICE_WIFI_HAL_AUTO_CONNECT_DELAY_MS,
                                     &wifi_started_connect_task_, Wifi::get_instance().get_state_task_group()
                                 ), false, "Post WiFi started connect delayed task failed");

    BROOKESIA_LOGD(
        "WiFi connected delayed task posted successfully, will trigger connect action after delay(%1%) ms",
        BROOKESIA_SERVICE_WIFI_HAL_AUTO_CONNECT_DELAY_MS
    );

    return true;
}

bool Hal::process_general_event_connected()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto connecting_ap_info = get_connecting_ap_info();
    if (get_target_connect_ap_info() == connecting_ap_info) {
        BROOKESIA_LOGD("The target connect AP is the same as the connecting AP, mark it as connectable");
        mark_target_connect_ap_info_connectable(true);
    }

    // Mark the connecting AP info as connectable
    connecting_ap_info.is_connectable = true;
    // Update the last connected AP info
    Wifi::get_instance().set_data<Wifi::DataType::LastAp>(connecting_ap_info);
    // Update the connected AP info list
    add_connected_ap_info(connecting_ap_info);

    return true;
}

bool Hal::process_general_event_unexpected_disconnected()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto &connecting_ap_info = get_connecting_ap_info();

    // Mark the target AP info as not connectable if it is the same as the connecting AP info
    if (get_target_connect_ap_info() == connecting_ap_info) {
        mark_target_connect_ap_info_connectable(false);
    }

    // Mark the last connected AP info as not connectable
    auto last_connected_ap_info = get_last_connected_ap_info();
    if ((last_connected_ap_info.ssid == connecting_ap_info.ssid) &&
            (last_connected_ap_info.password == connecting_ap_info.password) &&
            (last_connected_ap_info.is_connectable)) {
        BROOKESIA_LOGD("Mark the last connected AP info as not connectable");
        last_connected_ap_info.is_connectable = false;
        Wifi::get_instance().set_data<Wifi::DataType::LastAp>(last_connected_ap_info);
    }

    ConnectApInfo connected_ap_info;
    if (get_connectable_ap_info_by_ssid(connecting_ap_info.ssid, connected_ap_info)) {
        BROOKESIA_LOGD("Mark the connected AP(%1%) as not connectable", BROOKESIA_DESCRIBE_TO_STR(connected_ap_info));
        connected_ap_info.is_connectable = false;
        add_connected_ap_info(connected_ap_info);
    }

    if (!is_disable_auto_connect()) {
        // Try to connect a history connectable AP
        ConnectApInfo history_connectable_ap_info;
        if (get_last_connectable_ap_info(history_connectable_ap_info)) {
            BROOKESIA_LOGD(
                "History connectable AP is found: %1%", BROOKESIA_DESCRIBE_TO_STR(history_connectable_ap_info)
            );
            set_target_connect_ap_info(history_connectable_ap_info);
            if (!Wifi::get_instance().trigger_general_action(GeneralAction::Connect)) {
                BROOKESIA_LOGE("Failed to handle connectable AP found");
                return false;
            }
        } else {
            BROOKESIA_LOGD("No history connectable AP found, skip auto reconnect");
        }
    }

    return true;
}

bool Hal::set_target_connect_ap_info(const ConnectApInfo &ap_info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: ap_info(%1%)", BROOKESIA_DESCRIBE_TO_STR(ap_info));

    target_connect_ap_info_ = ap_info;

    return true;
}

void Hal::mark_target_connect_ap_info_connectable(bool connectable)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: connectable(%1%)", BROOKESIA_DESCRIBE_TO_STR(connectable));

    target_connect_ap_info_.is_connectable = connectable;
}

bool Hal::set_last_connected_ap_info(const ConnectApInfo &ap_info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: ap_info(%1%)", BROOKESIA_DESCRIBE_TO_STR(ap_info));

    last_connected_ap_info_ = ap_info;

    return true;
}

void Hal::add_connected_ap_info(const ConnectApInfo &ap_info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: ap_info(%1%)", BROOKESIA_DESCRIBE_TO_STR(ap_info));

    bool is_changed = false;

    lib_utils::FunctionGuard save_data_guard([this, &is_changed]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        if (is_changed) {
            Wifi::get_instance().set_data<Wifi::DataType::ConnectedAps>(get_connected_ap_infos(), true);
        }
    });

    bool is_found = false;
    for (auto &info : connected_ap_info_list_) {
        if (info.ssid == ap_info.ssid) {
            is_found = true;
            if (info != ap_info) {
                is_changed = true;
                info = ap_info;
                return;
            }
        }
    }

    if (is_found) {
        BROOKESIA_LOGD("Already exists, skip");
        return;
    }

    connected_ap_info_list_.push_back(ap_info);
    is_changed = true;
}

void Hal::remove_connected_ap_info_by_ssid(const std::string &ssid)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: ssid(%1%)", ssid);

    connected_ap_info_list_.remove_if([&ssid](const ConnectApInfo & info) {
        return info.ssid == ssid;
    });
}

void Hal::clear_connected_ap_infos()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    connected_ap_info_list_.clear();
}

bool Hal::on_wifi_event(uint8_t event)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event(%1%)", event);

    auto running_action = get_running_general_action();
    auto event_id = static_cast<wifi_event_t>(event);
    bool skip_softap_event = false;

    switch (event_id) {
    case WIFI_EVENT_STA_START:
        trigger_general_event(GeneralEvent::Started);
        break;
    case WIFI_EVENT_STA_STOP:
        trigger_general_event(GeneralEvent::Stopped);
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        skip_softap_event = true;
        if (running_action == GeneralAction::Connect) {
            BROOKESIA_LOGW("Detected disconnected event when connecting");
            if (increase_connect_retries()) {
                BROOKESIA_LOGW(
                    "Try to connect again... (%1%/%2%)", get_connect_retries(), get_connect_retries_max()
                );
                do_connect(get_connecting_ap_info());
                break;
            } else {
                BROOKESIA_LOGE("Connect retries reached max(%1%)", get_connect_retries_max());
            }
        }
        trigger_general_event(GeneralEvent::Disconnected);
        skip_softap_event = false;
        break;
    case WIFI_EVENT_SCAN_DONE: {
        is_scanning_ = false;
        // If WiFi is stopping or already stopped, skip update scan AP infos
        if ((running_action == GeneralAction::Stop) || is_general_event_ready(GeneralEvent::Stopped) ||
                (running_action == GeneralAction::Deinit) || is_general_event_ready(GeneralEvent::Deinited)) {
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

    // Handle SoftAP WiFi event
    if (skip_softap_event) {
        BROOKESIA_LOGD("Skip SoftAP WiFi event");
    } else if (softap_ != nullptr) {
        BROOKESIA_CHECK_FALSE_EXECUTE(softap_->on_hal_softap_wifi_event(event), {}, {
            BROOKESIA_LOGE("SoftAP failed to handle WiFi event");
        });
    }

    return true;
}

bool Hal::on_ip_event(uint8_t event)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event(%1%)", event);

    auto event_id = static_cast<ip_event_t>(event);
    switch (event_id) {
    case IP_EVENT_STA_GOT_IP: {
        reset_connect_retries();
        trigger_general_event(GeneralEvent::Connected);
        break;
    }
    default:
        BROOKESIA_LOGD("Ignored");
    }

    if (softap_ != nullptr) {
        BROOKESIA_CHECK_FALSE_EXECUTE(softap_->on_hal_softap_ip_event(event), {}, {
            BROOKESIA_LOGE("SoftAP failed to handle IP event");
        });
    }

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

    std::vector<wifi_ap_record_t> ap_records(actual_ap_count);
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_wifi_scan_get_ap_records(&actual_ap_count, ap_records.data()), false, "Get AP records failed"
    );

    BROOKESIA_LOGI("Scanned AP count: %1%", actual_ap_count);

    std::vector<ApInfo> scan_ap_infos(actual_ap_count);
    for (size_t i = 0; (i < actual_ap_count); i++) {
        const auto &ap_record = ap_records[i];
        scan_ap_infos[i] = get_ap_info(ap_record);
        BROOKESIA_LOGD("\t- Scanned AP[%1%]: %2%", i, BROOKESIA_DESCRIBE_TO_STR(scan_ap_infos[i]));
    }

    auto publish_ap_count = std::min(actual_ap_count, static_cast<uint16_t>(get_scan_params().ap_count));
    std::span<const ApInfo> publish_ap_infos(scan_ap_infos.begin(), publish_ap_count);
    BROOKESIA_CHECK_FALSE_EXECUTE(Wifi::get_instance().publish_scan_ap_infos(publish_ap_infos), {}, {
        BROOKESIA_LOGE("Failed to publish scan AP infos");
    });

    /* Check if connected AP appeared in the scan infos */
    bool is_connectable = false;
    // If already connecting or connected, skip
    if (!is_disable_auto_connect() && !is_general_action_running(GeneralAction::Connect) &&
            !is_general_event_ready(GeneralEvent::Connected)) {
        // Find first connectable AP in scan infos
        for (const auto &scan_ap_info : scan_ap_infos) {
            // Get connectable AP info
            ConnectApInfo connectable_ap_info = get_target_connect_ap_info();
            if (!scan_ap_info.ssid.empty() && (scan_ap_info.ssid == connectable_ap_info.ssid)) {
                if (connectable_ap_info.is_connectable) {
                    BROOKESIA_LOGD("Target AP is connectable, connect to it");
                    is_connectable = true;
                    break;
                } else {
                    BROOKESIA_LOGD("Target AP is not connectable, skip");
                }
            } else if (get_connectable_ap_info_by_ssid(scan_ap_info.ssid, connectable_ap_info)) {
                BROOKESIA_LOGD("Connect to the history connectable AP: %1%", BROOKESIA_DESCRIBE_TO_STR(connectable_ap_info));
                set_target_connect_ap_info(connectable_ap_info);
                is_connectable = true;
                break;
            }
        }
        if (is_connectable) {
            BROOKESIA_LOGD("Detected connectable AP, trigger connect action");
            BROOKESIA_CHECK_FALSE_EXECUTE(Wifi::get_instance().trigger_general_action(GeneralAction::Connect), {}, {
                BROOKESIA_LOGE("Failed to handle connectable AP found");
            });
        }
    }

    // Handle scan AP infos updated
    if (softap_ != nullptr) {
        BROOKESIA_CHECK_FALSE_EXECUTE(softap_->on_hal_scan_ap_infos_updated(scan_ap_infos), {}, {
            BROOKESIA_LOGE("SoftAP failed to handle scan AP infos updated");
        });
    }

    return true;
}

bool Hal::increase_connect_retries()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    connect_retries_++;

    BROOKESIA_LOGD("Connect retries: %1%", connect_retries_);

    if (connect_retries_ > get_connect_retries_max()) {
        BROOKESIA_LOGD("Connect retries reached max(%1%), reset", get_connect_retries_max());
        reset_connect_retries();
        return false;
    }

    return true;
}

void Hal::reset_connect_retries()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    connect_retries_ = 0;
}

void Hal::stop_connect_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (wifi_started_connect_task_ == 0) {
        BROOKESIA_LOGD("No started connect task, skip");
        return;
    }

    auto task_scheduler = get_task_scheduler();
    if (task_scheduler) {
        task_scheduler->cancel(wifi_started_connect_task_);
    }

    wifi_started_connect_task_ = 0;
}

} // namespace esp_brookesia::service::wifi
