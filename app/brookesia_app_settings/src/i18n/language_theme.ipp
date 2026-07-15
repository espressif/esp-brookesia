/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

std::expected<void, std::string> SettingsApp::populate_language_options(system::core::AppContext &context)
{
    clear_language_options(context);

    auto languages = normalize_language_list(context.gui().list_supported_languages());

    for (const auto &locale : languages) {
        const auto instance_id = make_language_instance_id(locale);
        auto created = context.gui().create_view(LANGUAGE_TEMPLATE_ID, LANGUAGE_LIST_PARENT, instance_id);
        if (!created) {
            clear_language_options(context);
            return std::unexpected(created.error());
        }

        const auto instance_path = join_path(LANGUAGE_LIST_PARENT, instance_id);
        dynamic_language_paths_.push_back(instance_path);
        language_instance_to_locale_.emplace(instance_id, locale);

        const std::vector<gui::BindingValueUpdate> updates = {
            gui::BindingValueUpdate{
                .absolute_path = instance_path + "/title_box/title",
                .key = "labelProps.text",
                .value = get_language_display_name(current_locale_, locale),
            },
            gui::BindingValueUpdate{
                .absolute_path = instance_path + "/value_box/value",
                .key = "labelProps.text",
                .value = locale == current_locale_ ? localized_text(current_locale_, "current") : "",
            },
        };
        auto result = context.gui().set_binding_values(updates);
        if (!result) {
            clear_language_options(context);
            return result;
        }
    }
    return {};
}

void SettingsApp::clear_language_options(system::core::AppContext &context)
{
    for (auto it = dynamic_language_paths_.rbegin(); it != dynamic_language_paths_.rend(); ++it) {
        (void)context.gui().destroy_view(*it);
    }
    dynamic_language_paths_.clear();
    language_instance_to_locale_.clear();
}

void SettingsApp::handle_language_event(const gui::Event &event)
{
    if (context_ == nullptr) {
        return;
    }
    if (!event.path.starts_with(LANGUAGE_LIST_PARENT)) {
        BROOKESIA_LOGW("Ignore language action from unexpected path: %1%", event.path);
        return;
    }

    auto instance_id = event.path.substr(std::string_view(LANGUAGE_LIST_PARENT).size());
    if (!instance_id.empty() && instance_id.front() == '/') {
        instance_id.erase(instance_id.begin());
    }
    const auto child_separator = instance_id.find('/');
    if (child_separator != std::string::npos) {
        instance_id.resize(child_separator);
    }

    auto it = language_instance_to_locale_.find(instance_id);
    if (it == language_instance_to_locale_.end()) {
        BROOKESIA_LOGW("Ignore language action from unknown item: %1%", event.path);
        return;
    }

    auto result = schedule_language_switch(*context_, it->second);
    if (!result) {
        BROOKESIA_LOGW("Failed to select Settings language '%1%': %2%", it->second, result.error());
    }
}

std::expected<void, std::string> SettingsApp::schedule_language_switch(
    system::core::AppContext &context,
    std::string_view locale
)
{
    const auto normalized_locale = normalize_locale(locale);
    if (language_switch_in_progress_ || theme_switch_in_progress_ ||
            pending_theme_timer_id_ != system::core::INVALID_TIMER_ID) {
        BROOKESIA_LOGW(
            "Ignore Settings language switch while another switch is pending or running: %1%",
            normalized_locale
        );
        return {};
    }
    if (normalized_locale == current_locale_) {
        if (pending_language_timer_id_ != system::core::INVALID_TIMER_ID) {
            if (!context.timer().stop(pending_language_timer_id_)) {
                BROOKESIA_LOGW(
                    "Failed to stop pending Settings language switch timer: %1%",
                    pending_language_timer_id_
                );
            }
            pending_language_timer_id_ = system::core::INVALID_TIMER_ID;
        }
        pending_language_locale_.clear();
        hide_language_loading_if_visible(context);
        return refresh_language_state(context);
    }

    if (pending_language_timer_id_ != system::core::INVALID_TIMER_ID) {
        if (!context.timer().stop(pending_language_timer_id_)) {
            BROOKESIA_LOGW(
                "Failed to stop previous Settings language switch timer: %1%",
                pending_language_timer_id_
            );
        }
        pending_language_timer_id_ = system::core::INVALID_TIMER_ID;
    }
    pending_language_locale_ = normalized_locale;

    if (!language_loading_visible_) {
        ensure_message_dialog(
            context,
            localized_text(current_locale_, "language.switching"),
            system::core::MessageDialogIcon::Information,
            0
        );
        language_loading_visible_ = true;
    }

    auto timer_result = context.timer().start_delayed(LANGUAGE_SWITCH_TIMER_NAME, SETTINGS_SWITCH_DEFER_MS);
    if (!timer_result) {
        BROOKESIA_LOGW("Failed to defer Settings language switch: %1%", timer_result.error());
        const auto target_locale = pending_language_locale_;
        pending_language_locale_.clear();
        return commit_language_switch(context, target_locale);
    }
    pending_language_timer_id_ = *timer_result;
    return {};
}

std::expected<void, std::string> SettingsApp::commit_language_switch(
    system::core::AppContext &context,
    std::string_view locale
)
{
    const auto normalized_locale = normalize_locale(locale);
    if (normalized_locale == current_locale_) {
        hide_language_loading_if_visible(context);
        return refresh_language_state(context);
    }

    language_switch_in_progress_ = true;
    bool switch_succeeded = false;
    lib_utils::FunctionGuard finish_language_switch_guard([this, &context, &switch_succeeded]() {
        language_switch_in_progress_ = false;
        if (!language_loading_visible_) {
            return;
        }
        update_message_dialog_if_visible(
            context,
            switch_succeeded ?
            localized_text(current_locale_, "language.switch.success") :
            localized_text(current_locale_, "language.switch.failed"),
            switch_succeeded ?
            system::core::MessageDialogIcon::Information :
            system::core::MessageDialogIcon::Warning,
            switch_succeeded ? MESSAGE_DIALOG_SUCCESS_AUTO_CLOSE_MS : MESSAGE_DIALOG_FAILED_AUTO_CLOSE_MS
        );
        language_loading_visible_ = false;
    });

    const auto previous_locale = current_locale_;
    current_locale_ = normalized_locale;
    const auto runtime_language = make_runtime_language(current_locale_);
    // Settings applies its own i18n bindings below; avoid reloading other preloaded app DOMs on every language switch.
    auto language_result = context.gui().set_language(runtime_language, false);
    if (!language_result) {
        BROOKESIA_LOGW("Failed to set Settings runtime language: %1%", language_result.error());
        current_locale_ = previous_locale;
        const auto rollback_language = make_runtime_language(current_locale_);
        if (auto rollback_result = context.gui().set_language(rollback_language, false);
                !rollback_result) {
            BROOKESIA_LOGW("Failed to roll back Settings runtime language: %1%", rollback_result.error());
        }
        return std::unexpected(language_result.error());
    }

    auto result = apply_locale(context);
    if (!result) {
        current_locale_ = previous_locale;
        if (auto rollback_result = context.gui().set_language(make_runtime_language(current_locale_), false);
                !rollback_result) {
            BROOKESIA_LOGW("Failed to roll back Settings runtime language: %1%", rollback_result.error());
        }
        if (auto locale_result = apply_locale(context); !locale_result) {
            BROOKESIA_LOGW("Failed to restore Settings locale text: %1%", locale_result.error());
        }
        if (auto header_result = refresh_header(context); !header_result) {
            BROOKESIA_LOGW("Failed to restore Settings header: %1%", header_result.error());
        }
        if (auto theme_state_result = refresh_theme_state(context); !theme_state_result) {
            BROOKESIA_LOGW("Failed to restore Settings theme state: %1%", theme_state_result.error());
        }
        if (auto language_state_result = refresh_language_state(context); !language_state_result) {
            BROOKESIA_LOGW("Failed to restore Settings language state: %1%", language_state_result.error());
        }
        return result;
    }
    result = refresh_header(context);
    if (!result) {
        return result;
    }
    result = refresh_theme_state(context);
    if (!result) {
        return result;
    }
    result = populate_language_options(context);
    if (!result) {
        return result;
    }
    result = refresh_language_state(context);
    if (!result) {
        return result;
    }
    result = refresh_time_zone_page_state(context);
    if (!result) {
        return result;
    }
    result = refresh_device_state(context);
    if (!result) {
        return result;
    }
    result = refresh_wifi_summary_state(context);
    if (!result) {
        return result;
    }
    result = refresh_wifi_page_state(context);
    if (!result) {
        return result;
    }
    result = refresh_wifi_lists(context);
    if (!result) {
        return result;
    }
    if (current_page_ == PAGE_WIFI_CONNECT) {
        result = refresh_wifi_connect_state(context);
        if (!result) {
            return result;
        }
    }
    switch_succeeded = true;
    return {};
}

std::expected<void, std::string> SettingsApp::schedule_theme_switch(
    system::core::AppContext &context,
    std::string_view theme_id
)
{
    if (language_switch_in_progress_ || theme_switch_in_progress_ ||
            pending_language_timer_id_ != system::core::INVALID_TIMER_ID) {
        BROOKESIA_LOGW("Ignore Settings theme switch while another switch is pending or running: %1%", theme_id);
        return {};
    }
    if (theme_id == current_theme_id_) {
        if (pending_theme_timer_id_ != system::core::INVALID_TIMER_ID) {
            if (!context.timer().stop(pending_theme_timer_id_)) {
                BROOKESIA_LOGW(
                    "Failed to stop pending Settings theme switch timer: %1%",
                    pending_theme_timer_id_
                );
            }
            pending_theme_timer_id_ = system::core::INVALID_TIMER_ID;
        }
        pending_theme_id_.clear();
        hide_theme_loading_if_visible(context);
        return refresh_theme_state(context);
    }

    if (pending_theme_timer_id_ != system::core::INVALID_TIMER_ID) {
        if (!context.timer().stop(pending_theme_timer_id_)) {
            BROOKESIA_LOGW(
                "Failed to stop previous Settings theme switch timer: %1%",
                pending_theme_timer_id_
            );
        }
        pending_theme_timer_id_ = system::core::INVALID_TIMER_ID;
    }
    pending_theme_id_ = theme_id;

    if (!theme_loading_visible_) {
        ensure_message_dialog(
            context,
            localized_text(current_locale_, "theme.switching"),
            system::core::MessageDialogIcon::Information,
            0
        );
        theme_loading_visible_ = true;
    }

    auto timer_result = context.timer().start_delayed(THEME_SWITCH_TIMER_NAME, SETTINGS_SWITCH_DEFER_MS);
    if (!timer_result) {
        BROOKESIA_LOGW("Failed to defer Settings theme switch: %1%", timer_result.error());
        const auto target_theme_id = pending_theme_id_;
        pending_theme_id_.clear();
        return commit_theme_switch(context, target_theme_id);
    }
    pending_theme_timer_id_ = *timer_result;
    return {};
}

std::expected<void, std::string> SettingsApp::commit_theme_switch(
    system::core::AppContext &context,
    std::string_view theme_id
)
{
    if (theme_id == current_theme_id_) {
        hide_theme_loading_if_visible(context);
        return refresh_theme_state(context);
    }

    theme_switch_in_progress_ = true;
    bool switch_succeeded = false;
    lib_utils::FunctionGuard finish_theme_switch_guard([this, &context, &switch_succeeded]() {
        theme_switch_in_progress_ = false;
        if (!theme_loading_visible_) {
            return;
        }
        update_message_dialog_if_visible(
            context,
            switch_succeeded ?
            localized_text(current_locale_, "theme.switch.success") :
            localized_text(current_locale_, "theme.switch.failed"),
            switch_succeeded ?
            system::core::MessageDialogIcon::Information :
            system::core::MessageDialogIcon::Warning,
            switch_succeeded ? MESSAGE_DIALOG_SUCCESS_AUTO_CLOSE_MS : MESSAGE_DIALOG_FAILED_AUTO_CLOSE_MS
        );
        theme_loading_visible_ = false;
    });

    const auto previous_theme_id = current_theme_id_;
    current_theme_id_ = theme_id;
    auto theme_result = context.gui().set_theme(current_theme_id_, true);
    if (!theme_result) {
        current_theme_id_ = previous_theme_id;
        BROOKESIA_LOGW("Failed to switch Settings theme: %1%", theme_result.error());
        if (auto rollback_result = context.gui().set_theme(current_theme_id_, true); !rollback_result) {
            BROOKESIA_LOGW("Failed to roll back Settings theme: %1%", rollback_result.error());
        }
        if (auto refresh_result = refresh_theme_state(context); !refresh_result) {
            BROOKESIA_LOGW("Failed to refresh Settings theme state after rollback: %1%", refresh_result.error());
        }
        return std::unexpected(theme_result.error());
    }
    auto result = refresh_theme_state(context);
    if (!result) {
        return result;
    }
    switch_succeeded = true;
    return {};
}

void SettingsApp::hide_language_loading_if_visible(system::core::AppContext &context)
{
    if (!language_loading_visible_) {
        return;
    }
    hide_message_dialog_if_visible(context);
    language_loading_visible_ = false;
}

void SettingsApp::hide_theme_loading_if_visible(system::core::AppContext &context)
{
    if (!theme_loading_visible_) {
        return;
    }
    hide_message_dialog_if_visible(context);
    theme_loading_visible_ = false;
}
