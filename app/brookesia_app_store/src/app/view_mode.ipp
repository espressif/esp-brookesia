/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

void AppStoreApp::start_view_mode_load(system::core::AppContext &context, ViewMode mode)
{
    if (view_mode_ == mode && view_mode_load_timer_id_ == system::core::INVALID_TIMER_ID) {
        return;
    }

    stop_view_mode_load_timer(context);
    pending_view_mode_ = mode;
    const auto loading_text = mode == ViewMode::Local ?
                              tr("status.loading_local_packages") :
                              tr("status.loading_installed_apps");
    ensure_message_dialog(
        context,
        loading_text,
        {},
        system::core::MessageDialogIcon::Information,
        0
    );

    auto timer = context.timer().start_delayed(VIEW_MODE_LOAD_TIMER_NAME, VIEW_MODE_LOAD_DELAY_MS);
    if (!timer) {
        BROOKESIA_LOGW("Failed to schedule App Store view mode load: %1%", timer.error());
        const auto fallback_mode = *pending_view_mode_;
        pending_view_mode_.reset();
        set_view_mode(context, fallback_mode);
        return;
    }
    view_mode_load_timer_id_ = *timer;
}
std::expected<void, std::string> AppStoreApp::process_view_mode_load(system::core::AppContext &context)
{
    if (!pending_view_mode_) {
        return {};
    }

    const auto mode = *pending_view_mode_;
    pending_view_mode_.reset();
    set_view_mode(context, mode);
    const auto loaded_text = mode == ViewMode::Local ?
                             tr("status.local_packages_loaded") :
                             tr("status.installed_apps_loaded");
    update_message_dialog_if_visible(
        context,
        loaded_text,
        {},
        system::core::MessageDialogIcon::Information,
        VIEW_MODE_LOAD_COMPLETE_DIALOG_AUTO_CLOSE_MS
    );
    return {};
}
void AppStoreApp::stop_view_mode_load_timer(system::core::AppContext &context)
{
    if (view_mode_load_timer_id_ == system::core::INVALID_TIMER_ID) {
        return;
    }
    if (!context.timer().stop(view_mode_load_timer_id_)) {
        BROOKESIA_LOGW("Failed to stop App Store view mode load timer: %1%", view_mode_load_timer_id_);
    }
    view_mode_load_timer_id_ = system::core::INVALID_TIMER_ID;
}
void AppStoreApp::set_view_mode(system::core::AppContext &context, ViewMode mode)
{
    if (view_mode_ == mode) {
        return;
    }
    view_mode_ = mode;
    list_page_ = 0;
    refresh_installed_apps(context);
    if (view_mode_ == ViewMode::Local) {
        scan_local_packages(context);
    }
    refresh_storage_capacity(context);
    (void)populate_entries(context);
}
