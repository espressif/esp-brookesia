/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_super/macro_configs.h"
#if !BROOKESIA_SYSTEM_SUPER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif

#include <chrono>
#include <ctime>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_helper/network/sntp.hpp"
#include "brookesia/service_helper/network/wifi.hpp"
#include "private/shell_app.hpp"
#include "private/system_constants.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::system::super {
namespace {

using SNTPHelper = service::helper::SNTP;
using WifiHelper = service::helper::Wifi;
constexpr int STATUS_EVENT_POLL_INTERVAL_MS = 20;

struct WifiStatusState {
    bool visible = false;
    bool connected = false;
};

WifiStatusState get_wifi_status_from_state(std::string_view state)
{
    if (state == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Connected)) {
        return {
            .visible = true,
            .connected = true,
        };
    }
    if (state == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Started) ||
            state == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Connecting) ||
            state == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Disconnecting)) {
        return {
            .visible = true,
            .connected = false,
        };
    }
    return {};
}

WifiStatusState get_wifi_status_from_event(std::string_view event)
{
    if (event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Connected)) {
        return {
            .visible = true,
            .connected = true,
        };
    }
    if (event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Started) ||
            event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Disconnected)) {
        return {
            .visible = true,
            .connected = false,
        };
    }
    return {};
}

std::string bool_to_binding(bool value)
{
    return value ? "true" : "false";
}

std::string make_clock_text()
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
#if defined(_WIN32)
    localtime_s(&local_time, &time);
#else
    localtime_r(&time, &local_time);
#endif
    char buffer[8] = {};
    if (std::strftime(buffer, sizeof(buffer), "%H:%M", &local_time) == 0) {
        return "--:--";
    }
    return buffer;
}

} // namespace

bool ShellApp::start_status_event_processing()
{
    if (context_ == nullptr) {
        return false;
    }
    auto mailbox = std::make_shared<StatusMailbox>();
    auto timer = context_->timer().start_periodic(SUPER_STATUS_EVENT_TIMER_NAME, STATUS_EVENT_POLL_INTERVAL_MS);
    if (!timer) {
        BROOKESIA_LOGW("Failed to start Shell status event timer: %1%", timer.error());
        return false;
    }
    status_mailbox_ = std::move(mailbox);
    status_event_timer_id_ = *timer;
    return true;
}

void ShellApp::stop_status_event_processing()
{
    auto mailbox = std::move(status_mailbox_);
    if (mailbox) {
        std::lock_guard lock(mailbox->mutex);
        mailbox->open = false;
        mailbox->wifi.reset();
        mailbox->gestures.clear();
        mailbox->timezone_changed = false;
    }
    if (context_ != nullptr && status_event_timer_id_ != core::INVALID_TIMER_ID) {
        (void)context_->timer().stop(status_event_timer_id_);
    }
    status_event_timer_id_ = core::INVALID_TIMER_ID;
}

void ShellApp::process_status_events()
{
    auto mailbox = status_mailbox_;
    if (!mailbox) {
        return;
    }
    std::optional<StatusMailbox::WifiStatus> wifi;
    std::vector<service::Display::TouchGestureInfo> gestures;
    bool timezone_changed = false;
    {
        std::lock_guard lock(mailbox->mutex);
        wifi = std::exchange(mailbox->wifi, std::nullopt);
        gestures.swap(mailbox->gestures);
        timezone_changed = std::exchange(mailbox->timezone_changed, false);
    }
    if (wifi) {
        set_status_wifi_state(wifi->visible, wifi->connected);
    }
    if (timezone_changed) {
        refresh_status_clock();
        stop_status_clock_timer();
        schedule_status_clock_timer();
    }
    for (const auto &gesture : gestures) {
        if (status_mailbox_ != mailbox) {
            break;
        }
        handle_display_touch_gesture(gesture);
    }
}

void ShellApp::invalidate_wifi_status()
{
    if (auto mailbox = status_mailbox_) {
        std::lock_guard lock(mailbox->mutex);
        ++mailbox->wifi_query;
        mailbox->wifi.reset();
    }
}

bool ShellApp::ensure_wifi_service_binding()
{
    if (wifi_service_binding_.is_valid()) {
        if (WifiHelper::is_running()) {
            return true;
        }
        invalidate_wifi_status();
        set_status_wifi_state(false, false);
        return false;
    }
    if (!WifiHelper::is_available()) {
        invalidate_wifi_status();
        set_status_wifi_state(false, false);
        return false;
    }
    wifi_service_binding_ = service::ServiceManager::get_instance().bind(WifiHelper::get_name().data());
    if (!wifi_service_binding_.is_valid()) {
        BROOKESIA_LOGW("Failed to bind Wi-Fi service for Shell status");
        invalidate_wifi_status();
        set_status_wifi_state(false, false);
        return false;
    }
    if (!WifiHelper::is_running()) {
        invalidate_wifi_status();
        wifi_service_binding_.release();
        set_status_wifi_state(false, false);
        return false;
    }

    if (!wifi_event_connection_.connected()) {
        auto mailbox = status_mailbox_;
        if (!mailbox) {
            return false;
        }
        uint64_t subscription = 0;
        {
            std::lock_guard lock(mailbox->mutex);
            subscription = ++mailbox->wifi_subscription;
        }
        const std::weak_ptr<StatusMailbox> weak_mailbox = mailbox;
        const auto general_callback = [weak_mailbox, subscription](
                                          const std::string &, const std::string & event, bool unexpected
        ) {
            if (unexpected) {
                BROOKESIA_LOGW("Wi-Fi service reported unexpected event for Shell status: %1%", event);
            }
            const auto status = get_wifi_status_from_event(event);
            if (auto state = weak_mailbox.lock()) {
                std::lock_guard lock(state->mutex);
                if (state->open && state->wifi_subscription == subscription) {
                    ++state->wifi_query;
                    state->wifi = StatusMailbox::WifiStatus{status.visible, status.connected};
                }
            }
        };
        wifi_event_connection_ = WifiHelper::subscribe_event(
                                     WifiHelper::EventId::GeneralEventHappened,
                                     general_callback
                                 );
        if (!wifi_event_connection_.connected()) {
            BROOKESIA_LOGW("Failed to subscribe Wi-Fi general event for Shell status");
        }
    }
    return true;
}

void ShellApp::release_wifi_service_binding()
{
    if (auto mailbox = status_mailbox_) {
        std::lock_guard lock(mailbox->mutex);
        ++mailbox->wifi_subscription;
        ++mailbox->wifi_query;
        mailbox->wifi.reset();
    }
    wifi_event_connection_.disconnect_without_wait();
    wifi_service_binding_.release();
    wifi_connected_ = false;
    set_status_wifi_state(false, false);
}

void ShellApp::refresh_wifi_status()
{
    if (!ensure_wifi_service_binding()) {
        return;
    }

    auto mailbox = status_mailbox_;
    if (!mailbox) {
        return;
    }
    uint64_t generation = 0;
    {
        std::lock_guard lock(mailbox->mutex);
        generation = ++mailbox->wifi_query;
    }
    const std::weak_ptr<StatusMailbox> weak_mailbox = mailbox;
    const auto state_handler = [weak_mailbox, generation](service::FunctionResult && result) {
        const auto status = !result.success || !result.has_data()
                            ? WifiStatusState{.visible = true, .connected = false}
                            : get_wifi_status_from_state(result.get_data<std::string>());
        if (auto state = weak_mailbox.lock()) {
            std::lock_guard lock(state->mutex);
            if (state->open && state->wifi_query == generation) {
                state->wifi = StatusMailbox::WifiStatus{status.visible, status.connected};
            }
        }
    };
    if (!WifiHelper::call_function_async(WifiHelper::FunctionId::GetGeneralState, state_handler)) {
        BROOKESIA_LOGW("Failed to submit Wi-Fi state request for Shell status");
        set_status_wifi_state(true, false);
    }
}

void ShellApp::set_status_wifi_state(bool visible, bool connected)
{
    wifi_connected_ = connected;
    if (context_ == nullptr) {
        return;
    }
    std::vector<gui::BindingValueUpdate> updates;
    updates.push_back(gui::BindingValueUpdate{
        .absolute_path = SUPER_STATUS_WIFI_PATH,
        .key = "wifi_hidden",
        .value = bool_to_binding(!visible),
    });
    updates.push_back(gui::BindingValueUpdate{
        .absolute_path = SUPER_STATUS_WIFI_PATH,
        .key = "wifi_bg",
        .value = connected ? "${color.success.fill}" : "${color.border.strong}",
    });
    updates.push_back(gui::BindingValueUpdate{
        .absolute_path = std::string(SUPER_STATUS_WIFI_PATH) + "/label",
        .key = "wifi_text",
        .value = connected ? "${color.success.on}" : "${color.text.inverse}",
    });
    auto result = context_->gui().set_binding_values(updates);
    if (!result) {
        BROOKESIA_LOGW("Failed to refresh Shell Wi-Fi status icon: %1%", result.error());
    }
}

bool ShellApp::ensure_sntp_service_binding()
{
    if (sntp_service_binding_.is_valid()) {
        return true;
    }
    if (!SNTPHelper::is_available()) {
        BROOKESIA_LOGD("SNTP service is not available for Shell status clock updates");
        return false;
    }

    sntp_service_binding_ = service::ServiceManager::get_instance().bind(SNTPHelper::get_name().data());
    if (!sntp_service_binding_.is_valid()) {
        BROOKESIA_LOGW("Failed to bind SNTP service for Shell status clock updates");
        return false;
    }
    return true;
}

void ShellApp::release_sntp_service_binding()
{
    disconnect_sntp_events();
    sntp_service_binding_.release();
}

void ShellApp::subscribe_sntp_events()
{
    disconnect_sntp_events();
    if (!ensure_sntp_service_binding()) {
        return;
    }

    auto mailbox = status_mailbox_;
    if (!mailbox) {
        return;
    }
    uint64_t subscription = 0;
    {
        std::lock_guard lock(mailbox->mutex);
        subscription = ++mailbox->sntp_subscription;
    }
    const std::weak_ptr<StatusMailbox> weak_mailbox = mailbox;
    const auto timezone_callback = [weak_mailbox, subscription](const std::string &, const service::EventItemMap &) {
        if (auto state = weak_mailbox.lock()) {
            std::lock_guard lock(state->mutex);
            if (state->open && state->sntp_subscription == subscription) {
                state->timezone_changed = true;
            }
        }
    };
    sntp_event_connection_ = SNTPHelper::subscribe_event(
                                 SNTPHelper::EventId::TimezoneChanged,
                                 timezone_callback
                             );
    if (!sntp_event_connection_.connected()) {
        BROOKESIA_LOGW("Failed to subscribe SNTP timezone event for Shell status clock");
    }
}

void ShellApp::disconnect_sntp_events()
{
    if (auto mailbox = status_mailbox_) {
        std::lock_guard lock(mailbox->mutex);
        ++mailbox->sntp_subscription;
        mailbox->timezone_changed = false;
    }
    sntp_event_connection_.disconnect_without_wait();
}

void ShellApp::refresh_status_clock()
{
    if (context_ == nullptr) {
        return;
    }
    auto result = context_->gui().set_binding_value(SUPER_STATUS_CLOCK_PATH, "clock_text", make_clock_text());
    if (!result) {
        BROOKESIA_LOGW("Failed to refresh Shell status clock: %1%", result.error());
    }
}

void ShellApp::schedule_status_clock_timer()
{
    if (context_ == nullptr || status_clock_timer_id_ != core::INVALID_TIMER_ID) {
        return;
    }
    auto timer = context_->timer().start_delayed(SUPER_STATUS_CLOCK_TIMER_NAME, 1000);
    if (!timer) {
        BROOKESIA_LOGW("Failed to start Shell status clock timer: %1%", timer.error());
        return;
    }
    status_clock_timer_id_ = *timer;
}

void ShellApp::stop_status_clock_timer()
{
    if (context_ != nullptr && status_clock_timer_id_ != core::INVALID_TIMER_ID) {
        (void)context_->timer().stop(status_clock_timer_id_);
    }
    status_clock_timer_id_ = core::INVALID_TIMER_ID;
}

} // namespace esp_brookesia::system::super
