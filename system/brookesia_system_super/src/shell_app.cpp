/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_super/macro_configs.h"
#if !BROOKESIA_SYSTEM_SUPER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif

#include <filesystem>

#include "brookesia/lib_utils/function_guard.hpp"
#include "private/shell_app.hpp"
#include "private/system_constants.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::system::super {
namespace {

std::string make_system_resource_dir(const System &system)
{
    const auto layout = system.get_storage_layout();
    return (std::filesystem::path(layout.internal.root_path) / "system" / "super").lexically_normal().generic_string();
}

} // namespace

ShellApp::ShellApp(System &owner)
    : owner_(owner)
{}

core::AppManifest ShellApp::get_manifest() const
{
    return {
        .id = SUPER_SHELL_APP_ID,
        .name = "Shell",
        .localized_names = {
            {"en", "Shell"},
            {"zh_CN", "桌面"},
        },
        .version = "0.1.0",
        .kind = core::AppKind::Native,
        .visible = false,
        .icon_id = "super",
        .supported_systems = {},
        .icon_path = "",
        .runtime_type = runtime::BackendType::Unknown,
        .app_path = "",
        .entry = "",
        .resource_dir = make_system_resource_dir(owner_),
        .arguments = {},
    };
}

core::AppGuiDescriptor ShellApp::get_gui_descriptor() const
{
    return {
        .root_kind = core::GuiRootKind::File,
        .root = SUPER_SHELL_ROOT_JSON,
        .resources = {},
        .screen_flows = {
            core::GuiScreenFlowEntry{
                .screen_flow = SUPER_BACKGROUND_FLOW_ID,
                .layer = core::GuiAppLayer::SystemBottom,
            },
            core::GuiScreenFlowEntry{
                .screen_flow = SUPER_SHELL_PAGES_FLOW_ID,
                .layer = core::GuiAppLayer::AppDefault,
            },
            core::GuiScreenFlowEntry{
                .screen_flow = SUPER_OVERLAY_FLOW_ID,
                .layer = core::GuiAppLayer::AppTop,
                .mount_mode = gui::MountStackMode::Stack,
                .z_order = BROOKESIA_SYSTEM_CORE_RUNTIME_APP_MAX_Z_ORDER + 1,
            },
            core::GuiScreenFlowEntry{
                .screen_flow = SUPER_KEYBOARD_FLOW_ID,
                .layer = core::GuiAppLayer::AppTop,
                .mount_mode = gui::MountStackMode::Stack,
                .z_order = BROOKESIA_SYSTEM_CORE_RUNTIME_APP_MAX_Z_ORDER + 2,
            },
            core::GuiScreenFlowEntry{
                .screen_flow = SUPER_MESSAGE_DIALOG_FLOW_ID,
                .layer = core::GuiAppLayer::AppTop,
                .mount_mode = gui::MountStackMode::Stack,
                .z_order = BROOKESIA_SYSTEM_CORE_RUNTIME_APP_MAX_Z_ORDER + 3,
            },
        },
    };
}

std::expected<void, std::string> ShellApp::on_start(core::AppContext &context)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    context_ = &context;
    (void)ensure_display_operation();

    auto result = load_fonts(context);
    if (!result) {
        context_ = nullptr;
        return result;
    }

    owner_.begin_shell_gui_preferences_restore();
    {
        lib_utils::FunctionGuard preferences_restore_guard([this]() {
            owner_.mark_shell_gui_preferences_restored();
        });

        result = apply_language_preference(context);
        if (!result) {
            context_ = nullptr;
            return result;
        }

        result = load_themes(context);
        if (!result) {
            context_ = nullptr;
            return result;
        }
    }

    result = mount_overlay(context);
    if (!result) {
        context_ = nullptr;
        return result;
    }

    result = populate_launcher(context);
    if (!result) {
        unmount_overlay();
        context_ = nullptr;
        return result;
    }
    result = set_foreground_app(std::nullopt);
    if (!result) {
        unmount_overlay();
        context_ = nullptr;
        return result;
    }
    start_expansion_notifications();
    return {};
}

std::expected<void, std::string> ShellApp::on_stop(core::AppContext &context)
{
    (void)context;
    stop_expansion_notifications();
    disconnect_launcher_actions();
    launcher_instance_to_app_.clear();
    launcher_populated_ = false;
    cancel_pending_launch();
    unmount_message_dialog();
    unmount_keyboard();
    unmount_overlay();
    applied_i18n_locale_.clear();
    release_display_operation();
    context_ = nullptr;
    return {};
}

std::expected<void, std::string> ShellApp::on_timer(
    core::AppContext &context,
    core::TimerId timer_id,
    std::string_view name
)
{
    (void)context;
    if (name == SUPER_EXPANSION_NOTIFICATION_TIMER_NAME && timer_id == expansion_notification_timer_id_) {
        process_expansion_notifications();
        return {};
    }
    if (name == SUPER_MESSAGE_DIALOG_TIMEOUT_TIMER_NAME && timer_id == message_dialog_auto_close_timer_id_) {
        message_dialog_auto_close_timer_id_ = core::INVALID_TIMER_ID;
        finish_message_dialog(-1, core::MessageDialogCloseReason::Timeout);
        return {};
    }
    if (name == SUPER_GESTURE_EXIT_HOLD_TIMER_NAME && timer_id == gesture_exit_hold_timer_id_) {
        gesture_exit_hold_timer_id_ = core::INVALID_TIMER_ID;
        trigger_gesture_exit();
        return {};
    }
    if (name == SUPER_STATUS_PEEK_AUTO_HIDE_TIMER_NAME && timer_id == status_peek_auto_hide_timer_id_) {
        status_peek_auto_hide_timer_id_ = core::INVALID_TIMER_ID;
        if (status_peek_visible_) {
            reset_status_peek_session(true, false, "auto hide timer");
        }
        return {};
    }
    if (name == SUPER_STATUS_CLOCK_TIMER_NAME && timer_id == status_clock_timer_id_) {
        status_clock_timer_id_ = core::INVALID_TIMER_ID;
        refresh_status_clock();
        schedule_status_clock_timer();
        return {};
    }
    if (name != SUPER_APP_LAUNCH_HOLD_TIMER_NAME || timer_id != launch_hold_timer_id_) {
        return {};
    }

    const auto app_id = pending_launch_app_id_;
    pending_launch_app_id_.reset();
    launch_hold_timer_id_ = core::INVALID_TIMER_ID;
    if (!app_id.has_value()) {
        finish_launch_overlay();
        return {};
    }

    start_app_after_launch(*app_id);
    return {};
}

std::expected<void, std::string> ShellApp::show_page(ShellPage page)
{
    if (context_ == nullptr) {
        return std::unexpected("Shell app is not running");
    }

    auto result = context_->gui().trigger_screen_flow(SUPER_SHELL_PAGES_FLOW_ID, to_screen_flow_action(page));
    if (!result) {
        return result;
    }
    return {};
}

} // namespace esp_brookesia::system::super
