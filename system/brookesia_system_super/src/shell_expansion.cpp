/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_super/macro_configs.h"
#if !BROOKESIA_SYSTEM_SUPER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif

#include "brookesia/service_helper/system/device.hpp"
#include "private/expansion_notifications.hpp"
#include "private/shell_app.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::system::super {
namespace {

using DeviceHelper = service::helper::Device;
inline constexpr int EXPANSION_NOTIFICATION_INTERVAL_MS = 250;
inline constexpr int EXPANSION_NOTIFICATION_AUTO_CLOSE_MS = 3000;

core::MessageDialogOptions make_expansion_dialog(
    const ExpansionNotifications::Notification &notification, bool chinese
);

} // namespace

void ShellApp::start_expansion_notifications()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    if (!DeviceHelper::is_available()) {
        return;
    }
    expansion_service_binding_ = service::ServiceManager::get_instance().bind(DeviceHelper::get_name().data());
    if (!expansion_service_binding_.is_valid()) {
        BROOKESIA_LOGW("Failed to bind Device service for expansion notifications");
        return;
    }

    auto result = DeviceHelper::call_function_sync<boost::json::array>(DeviceHelper::FunctionId::GetCapabilities);
    DeviceHelper::Capabilities capabilities;
    if (!result || !BROOKESIA_DESCRIBE_FROM_JSON(*result, capabilities)) {
        BROOKESIA_LOGW("Failed to query expansion notification capability");
        stop_expansion_notifications();
        return;
    }
    bool available = false;
    for (const auto &device : capabilities) {
        for (const auto &iface : device.interfaces) {
            available |= iface.type_name == hal::expansion::ModuleManagerIface::NAME;
        }
    }
    if (!available) {
        stop_expansion_notifications();
        return;
    }

    expansion_notifications_ = std::make_shared<ExpansionNotifications>();
    const std::weak_ptr<ExpansionNotifications> weak_state = expansion_notifications_;
    const auto on_module = [weak_state](const std::string &, const boost::json::object & payload) {
        auto state = weak_state.lock();
        if (!state) {
            return;
        }
        hal::expansion::ModuleInfo module;
        if (!BROOKESIA_DESCRIBE_FROM_JSON(payload, module)) {
            BROOKESIA_LOGW("Invalid expansion module event");
            return;
        }
        state->update(module);
    };
    expansion_event_connection_ = DeviceHelper::subscribe_event(
                                      DeviceHelper::EventId::ExpansionModuleChanged, on_module
                                  );
    if (!expansion_event_connection_.connected()) {
        BROOKESIA_LOGW("Failed to subscribe expansion notifications");
        stop_expansion_notifications();
        return;
    }

    // Subscribe before querying so modules present at startup and concurrent changes are both covered.
    result = DeviceHelper::call_function_sync<boost::json::array>(DeviceHelper::FunctionId::GetExpansionModuleInfos);
    DeviceHelper::ExpansionModuleInfos modules;
    if (result && BROOKESIA_DESCRIBE_FROM_JSON(*result, modules)) {
        for (const auto &module : modules) {
            expansion_notifications_->update(module);
        }
    } else {
        BROOKESIA_LOGW("Failed to query initial expansion modules; waiting for module events");
    }

    // Drain on the app task; service callbacks must not wait for GUI work during Shell shutdown.
    auto timer = context_->timer().start_periodic(SUPER_EXPANSION_NOTIFICATION_TIMER_NAME,
                 EXPANSION_NOTIFICATION_INTERVAL_MS);
    if (!timer) {
        BROOKESIA_LOGW("Failed to start expansion notification timer: %1%", timer.error());
        stop_expansion_notifications();
        return;
    }
    expansion_notification_timer_id_ = *timer;
}

void ShellApp::stop_expansion_notifications()
{
    expansion_event_connection_.disconnect();
    expansion_notifications_.reset();
    if (context_ != nullptr && expansion_notification_timer_id_ != core::INVALID_TIMER_ID) {
        (void)context_->timer().stop(expansion_notification_timer_id_);
    }
    expansion_notification_timer_id_ = core::INVALID_TIMER_ID;
    for (const auto &[slot, request_id] : expansion_dialogs_) {
        (void)slot;
        (void)owner_.hide_system_message_dialog(request_id);
    }
    expansion_dialogs_.clear();
    expansion_service_binding_.release();
}

void ShellApp::process_expansion_notifications()
{
    if (context_ == nullptr || !expansion_notifications_) {
        return;
    }
    for (const auto &notification : expansion_notifications_->take_pending()) {
        const auto &module = notification.module;
        BROOKESIA_LOGI("Expansion board %1%: provider(%2%), slot(%3%), name(%4%)",
                       notification.removed ? "removed" : "detected", module.provider, module.slot, module.board_name);
        const auto key = std::make_pair(module.provider, module.slot);
        auto options = make_expansion_dialog(notification, context_->gui().get_language() == "zh_CN");
        const auto existing = expansion_dialogs_.find(key);
        if (existing != expansion_dialogs_.end()) {
            // Replace an active or queued notice from the same slot with its latest state.
            if (owner_.update_system_message_dialog(existing->second, options)) {
                continue;
            }
            expansion_dialogs_.erase(existing);
        }
        auto result = owner_.show_system_message_dialog(std::move(options));
        if (!result) {
            BROOKESIA_LOGW("Failed to show expansion notification: %1%", result.error());
            continue;
        }
        expansion_dialogs_[key] = *result;
    }
}

namespace {

core::MessageDialogOptions make_expansion_dialog(
    const ExpansionNotifications::Notification &notification, bool chinese
)
{
    const auto &module = notification.module;
    auto name = module.board_name;
    if (name.empty()) {
        name = module.type.empty() ? (chinese ? "扩展板" : "Expansion board") : module.type;
    }
    auto slot = module.slot;
    if (slot == "left") {
        slot = chinese ? "左侧插槽" : "Left slot";
    } else if (slot == "right") {
        slot = chinese ? "右侧插槽" : "Right slot";
    }

    core::MessageDialogOptions options{
        .text = notification.removed ? (chinese ? "扩展板已拔出" : "Expansion board removed") :
        (chinese ? "扩展板识别成功" : "Expansion board detected"),
        .informative_text = name + "\n" + slot,
        .icon = core::MessageDialogIcon::Information,
        .buttons = {{chinese ? "确定" : "OK", core::MessageDialogButtonRole::Accept}},
        .auto_close_ms = EXPANSION_NOTIFICATION_AUTO_CLOSE_MS,
    };
    if (!notification.removed && module.state == hal::expansion::ModuleState::Unsupported) {
        options.informative_text += chinese ? "\n此扩展板暂不支持" : "\nThis board is not supported yet";
    }
    return options;
}

} // namespace
} // namespace esp_brookesia::system::super
