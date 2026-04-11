/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define BROOKESIA_LOG_TAG "WifiProvisioning"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper/wifi.hpp"
#include "wifi_provisioning.hpp"

using namespace esp_brookesia;

using WifiHelper = service::helper::Wifi;

constexpr uint32_t WIFI_DO_SOFTAP_PROVISION_START_TIMEOUT_MS = 1000;
constexpr uint32_t WIFI_DO_SOFTAP_PROVISION_STOP_TIMEOUT_MS = 1000;

static bool get_wifi_softap_params(WifiHelper::SoftApParams &softap_params);

bool WifiProvisioning::init(const Config &config)
{
    BROOKESIA_CHECK_FALSE_RETURN(!is_initialized_.load(), false, "Already initialized");
    BROOKESIA_CHECK_FALSE_RETURN(config.task_scheduler != nullptr, false, "Invalid task scheduler");
    BROOKESIA_CHECK_FALSE_RETURN(WifiHelper::is_available(), false, "WiFi service not available");

    auto &service_manager = service::ServiceManager::get_instance();
    auto binding = service_manager.bind(WifiHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_RETURN(binding.is_valid(), false, "Failed to bind WiFi service");

    service_bindings_.push_back(std::move(binding));
    config_ = config;
    is_initialized_.store(true);

    return true;
}

bool WifiProvisioning::start()
{
    BROOKESIA_CHECK_FALSE_RETURN(is_initialized_.load(), false, "Not initialized");

    auto run_connect_or_provision = [this]() {
        auto get_aps_result = WifiHelper::call_function_sync<boost::json::array>(
                                  WifiHelper::FunctionId::GetConnectedAps
                              );
        if (!get_aps_result) {
            BROOKESIA_LOGE("Failed to get connected APs: %1%", get_aps_result.error());
            return;
        }

        std::vector<WifiHelper::ConnectApInfo> saved_aps;
        auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(get_aps_result.value(), saved_aps);
        BROOKESIA_CHECK_FALSE_EXIT(parse_result, "Failed to parse connected APs");

        if (saved_aps.empty()) {
            BROOKESIA_LOGI("No connected APs, starting SoftAP provisioning");
            start_softap_provision_flow();
        } else {
            BROOKESIA_LOGI("Found connectable AP, starting STA connect flow");
            start_sta_connect_flow();
        }
    };

    return config_.task_scheduler->post(std::move(run_connect_or_provision));
}

void WifiProvisioning::start_sta_connect_flow()
{
    auto post_reset_active_conn = [this]() {
        active_conn_.reset();
    };

    auto post_reset_and_softap = [this]() {
        active_conn_.reset();
        start_softap_provision_flow();
    };

    auto on_sta_general_event = [this, post_reset_active_conn, post_reset_and_softap](
    const std::string &, const std::string & event, bool) {
        if (event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Connected)) {
            BROOKESIA_LOGI("WiFi connected");
            config_.task_scheduler->post(post_reset_active_conn);
        } else if (event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Disconnected)) {
            BROOKESIA_LOGW("WiFi disconnected, switching to SoftAP provisioning");
            config_.task_scheduler->post(post_reset_and_softap);
        }
    };

    auto conn = WifiHelper::subscribe_event(WifiHelper::EventId::GeneralEventHappened, on_sta_general_event);
    active_conn_ = std::make_shared<service::EventRegistry::SignalConnection>(std::move(conn));

    auto start_result = WifiHelper::call_function_sync(
                            WifiHelper::FunctionId::TriggerGeneralAction,
                            BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Start)
                        );
    if (!start_result) {
        BROOKESIA_LOGE("Failed to trigger WiFi start: %1%", start_result.error());
        active_conn_.reset();
    }
}

void WifiProvisioning::start_softap_provision_flow()
{
    auto post_stop_softap_provision = [this]() {
        active_conn_.reset();
        auto stop_result = WifiHelper::call_function_sync(
                               WifiHelper::FunctionId::TriggerSoftApProvisionStop,
                               service::helper::Timeout(WIFI_DO_SOFTAP_PROVISION_STOP_TIMEOUT_MS)
                           );
        if (!stop_result) {
            BROOKESIA_LOGE("Failed to stop SoftAP provision: %1%", stop_result.error());
        }
    };

    auto on_softap_general_event = [this, post_stop_softap_provision](
    const std::string &, const std::string & event, bool) {
        if (event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Connected)) {
            BROOKESIA_LOGI("WiFi connected via SoftAP provisioning");
            config_.task_scheduler->post(post_stop_softap_provision);
        }
    };

    auto conn = WifiHelper::subscribe_event(WifiHelper::EventId::GeneralEventHappened, on_softap_general_event);
    active_conn_ = std::make_shared<service::EventRegistry::SignalConnection>(std::move(conn));

    auto provision_start_result = WifiHelper::call_function_sync(
                                      WifiHelper::FunctionId::TriggerSoftApProvisionStart,
                                      service::helper::Timeout(WIFI_DO_SOFTAP_PROVISION_START_TIMEOUT_MS)
                                  );
    if (!provision_start_result) {
        BROOKESIA_LOGE("Failed to start SoftAP provision: %1%", provision_start_result.error());
        active_conn_.reset();
        return;
    }

    WifiHelper::SoftApParams softap_params;
    if (get_wifi_softap_params(softap_params)) {
        BROOKESIA_LOGW(
            "\nSoftAP provisioning started. Connect to '%1%' (password: '%2%') to provision WiFi\n",
            softap_params.ssid, softap_params.password
        );
    }
}

static bool get_wifi_softap_params(WifiHelper::SoftApParams &softap_params)
{
    auto get_result = WifiHelper::call_function_sync<boost::json::object>(
                          WifiHelper::FunctionId::GetSoftApParams
                      );
    BROOKESIA_CHECK_FALSE_RETURN(
        get_result, false, "Failed to get SoftAP params: %1%", get_result.error()
    );

    return BROOKESIA_DESCRIBE_FROM_JSON(get_result.value(), softap_params);
}
