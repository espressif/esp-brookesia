/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "brookesia/service_helper/nvs.hpp"
#include "general.hpp"

using namespace esp_brookesia;

using WifiHelpler = service::helper::Wifi;
using NVS_Helper = service::helper::NVS;

static auto &service_manager = service::ServiceManager::get_instance();
static auto &time_profiler = lib_utils::TimeProfiler::get_instance();
static service::ServiceBinding wifi_binding;
static service::ServiceBinding nvs_binding;

EventCollector::EventCollector()
{
    setup_event_subscriptions();
}

EventCollector::~EventCollector()
{
    BROOKESIA_LOGI("EventCollector::~EventCollector()");
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
    std::lock_guard<std::mutex> lock(mutex);
    // Clear all connections before destroying other members
    connections.clear();
}

// Event collector for test verification
void EventCollector::on_general_action_triggered(const service::EventItemMap &params)
{
    std::lock_guard<std::mutex> lock(mutex);
    GeneralActionEvent evt;

    auto action_it = params.find(BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::EventGeneralActionTriggeredParam::Action));
    if (action_it != params.end()) {
        auto action_ptr = std::get_if<std::string>(&action_it->second);
        TEST_ASSERT_NOT_NULL_MESSAGE(action_ptr, "Failed to get action");

        evt.action = *action_ptr;
        general_actions.push_back(evt);
        BROOKESIA_LOGI("General action triggered: %s", evt.action.c_str());
        cv.notify_one();
    }
}

void EventCollector::on_general_event_happened(const service::EventItemMap &params)
{
    std::lock_guard<std::mutex> lock(mutex);
    GeneralEventHappened evt;

    auto event_it = params.find(BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::EventGeneralEventHappenedParam::Event));
    if (event_it != params.end()) {
        auto event_ptr = std::get_if<std::string>(&event_it->second);
        TEST_ASSERT_NOT_NULL_MESSAGE(event_ptr, "Failed to get event");

        evt.event = *event_ptr;
        general_events.push_back(evt);
        BROOKESIA_LOGI("General event happened: %s", evt.event.c_str());
        cv.notify_one();
    }
}

void EventCollector::on_scan_ap_infos_updated(const service::EventItemMap &params)
{
    std::lock_guard<std::mutex> lock(mutex);
    ScanApInfosUpdatedEvent evt;

    auto infos_it = params.find(BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::EventScanApInfosUpdatedParam::ApInfos));
    if (infos_it != params.end()) {
        auto infos_ptr = std::get_if<boost::json::array>(&infos_it->second);
        TEST_ASSERT_NOT_NULL_MESSAGE(infos_ptr, "Failed to get infos");

        if (BROOKESIA_DESCRIBE_FROM_JSON(*infos_ptr, evt.ap_infos)) {
            scan_ap_infos_updated.push_back(evt);
            BROOKESIA_LOGI("Scan infos updated: found %zu APs", evt.ap_infos.size());
            cv.notify_one();
        }
    }
}

size_t EventCollector::get_general_action_count(WifiHelpler::GeneralAction action)
{
    return std::count_if(general_actions.begin(), general_actions.end(), [action](const GeneralActionEvent & evt) {
        return evt.action == BROOKESIA_DESCRIBE_TO_STR(action);
    });
}

size_t EventCollector::get_general_event_count(WifiHelpler::GeneralEvent event)
{
    return std::count_if(general_events.begin(), general_events.end(), [event](const GeneralEventHappened & evt) {
        return evt.event == BROOKESIA_DESCRIBE_TO_STR(event);
    });
}

void EventCollector::on_softap_event_happened(const service::EventItemMap &params)
{
    std::lock_guard<std::mutex> lock(mutex);
    SoftApEventHappened evt;

    auto event_it = params.find(BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::EventSoftApEventHappenedParam::Event));
    if (event_it != params.end()) {
        auto event_ptr = std::get_if<std::string>(&event_it->second);
        TEST_ASSERT_NOT_NULL_MESSAGE(event_ptr, "Failed to get SoftAP event");

        evt.event = *event_ptr;
        softap_events.push_back(evt);
        BROOKESIA_LOGI("SoftAP event happened: %s", evt.event.c_str());
        cv.notify_one();
    }
}

size_t EventCollector::get_softap_event_count(WifiHelpler::SoftApEvent event)
{
    return std::count_if(softap_events.begin(), softap_events.end(), [event](const SoftApEventHappened & evt) {
        return evt.event == BROOKESIA_DESCRIBE_TO_STR(event);
    });
}

void EventCollector::clear()
{
    std::lock_guard<std::mutex> lock(mutex);
    general_actions.clear();
    general_events.clear();
    scan_ap_infos_updated.clear();
    softap_events.clear();
}

bool EventCollector::wait_for_general_actions(size_t count, uint32_t timeout_ms, WifiHelpler::GeneralAction action)
{
    std::unique_lock<std::mutex> lock(mutex);
    return cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this, action, count]() {
        if (action != WifiHelpler::GeneralAction::Max) {
            return get_general_action_count(action) >= count;
        }
        return general_actions.size() >= count;
    });
}

bool EventCollector::wait_for_general_events(size_t count, uint32_t timeout_ms, WifiHelpler::GeneralEvent event)
{
    std::unique_lock<std::mutex> lock(mutex);
    return cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this, event, count]() {
        if (event != WifiHelpler::GeneralEvent::Max) {
            return get_general_event_count(event) >= count;
        }
        return general_events.size() >= count;
    });
}

bool EventCollector::wait_for_scan_ap_infos_updated(size_t count, uint32_t timeout_ms)
{
    std::unique_lock<std::mutex> lock(mutex);
    return cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this, count]() {
        return scan_ap_infos_updated.size() >= count;
    });
}

bool EventCollector::wait_for_softap_events(size_t count, uint32_t timeout_ms, WifiHelpler::SoftApEvent event)
{
    std::unique_lock<std::mutex> lock(mutex);
    return cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this, event, count]() {
        if (event != WifiHelpler::SoftApEvent::Max) {
            return get_softap_event_count(event) >= count;
        }
        return softap_events.size() >= count;
    });
}

// Helper function to setup event subscriptions
void EventCollector::setup_event_subscriptions()
{
    auto service = wifi_binding.get_service();
    TEST_ASSERT_NOT_NULL_MESSAGE(service, "Failed to get service");

    // Subscribe to general_action_triggered event
    auto conn1 = service->subscribe_event(BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::EventId::GeneralActionTriggered),
    [&](const std::string & event_name, const service::EventItemMap & event_items) {
        on_general_action_triggered(event_items);
    });
    TEST_ASSERT_TRUE_MESSAGE(conn1.connected(), "Failed to subscribe to general_action_triggered event");
    connections.push_back(std::move(conn1));

    // Subscribe to general_event_happened event
    auto conn2 = service->subscribe_event(BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::EventId::GeneralEventHappened),
    [&](const std::string & event_name, const service::EventItemMap & event_items) {
        on_general_event_happened(event_items);
    });
    TEST_ASSERT_TRUE_MESSAGE(conn2.connected(), "Failed to subscribe to general_event_happened event");
    connections.push_back(std::move(conn2));

    // Subscribe to scan_ap_infos_updated event
    auto conn3 = service->subscribe_event(BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::EventId::ScanApInfosUpdated),
    [&](const std::string & event_name, const service::EventItemMap & event_items) {
        on_scan_ap_infos_updated(event_items);
    });
    TEST_ASSERT_TRUE_MESSAGE(conn3.connected(), "Failed to subscribe to scan_ap_infos_updated event");
    connections.push_back(std::move(conn3));

    // Subscribe to SoftAP event
    auto conn4 = service->subscribe_event(BROOKESIA_DESCRIBE_TO_STR(WifiHelpler::EventId::SoftApEventHappened),
    [&](const std::string & event_name, const service::EventItemMap & event_items) {
        on_softap_event_happened(event_items);
    });
    TEST_ASSERT_TRUE_MESSAGE(conn4.connected(), "Failed to subscribe to SoftAP event");
    connections.push_back(std::move(conn4));
}

void startup()
{
    lib_utils::TimeProfiler::FormatOptions opt;
    opt.use_unicode = true;
    opt.use_color = true;
    opt.sort_by = lib_utils::TimeProfiler::FormatOptions::SortBy::TotalDesc;
    opt.show_percentages = true;
    opt.name_width = 40;
    opt.calls_width = 6;
    opt.num_width = 10;
    opt.percent_width = 7;
    opt.precision = 2;
    opt.time_unit = lib_utils::TimeProfiler::FormatOptions::TimeUnit::Milliseconds;
    time_profiler.set_format_options(opt);

    TEST_ASSERT_TRUE_MESSAGE(service_manager.start(), "Failed to start service manager");

    if (!nvs_binding.is_valid()) {
        nvs_binding = service_manager.bind(NVS_Helper::get_name().data());
        TEST_ASSERT_TRUE_MESSAGE(nvs_binding.is_valid(), "Failed to bind NVS service");
    }

    if (!wifi_binding.is_valid()) {
        wifi_binding = service_manager.bind(WifiHelpler::get_name().data());
        TEST_ASSERT_TRUE_MESSAGE(wifi_binding.is_valid(), "Failed to bind service");
    }
}

void shutdown()
{
    nvs_binding.release();
    wifi_binding.release();
    service_manager.deinit();
    time_profiler.report();
    time_profiler.clear();
}
