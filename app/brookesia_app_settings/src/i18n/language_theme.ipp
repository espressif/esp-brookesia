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
                .value = locale == pending_language_locale_ ?
                         localized_text(current_locale_, "restart_required") :
                         locale == current_locale_ ? localized_text(current_locale_, "current") : "",
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
    if (restart_in_progress_) {
        BROOKESIA_LOGW("Ignore Settings language selection while restart is in progress");
        return {};
    }
    if (normalized_locale == current_locale_) {
        if (auto result = context.gui().save_language_preference(make_runtime_language(current_locale_)); !result) {
            show_preference_save_failure(context, true, result.error());
            return result;
        }
    preference_error_.clear();
    pending_language_locale_.clear();
    restart_prompt_kind_ = RestartPromptKind::None;
    hide_message_dialog_if_visible(context);
    return refresh_language_state(context);
    }

    const auto supported_languages = settings_uses_gsp_backend() ?
                                     std::vector<std::string>{LOCALE_EN, LOCALE_ZH_CN} :
                                     normalize_language_list(context.gui().list_supported_languages());
    if (std::find(supported_languages.begin(), supported_languages.end(), normalized_locale) ==
            supported_languages.end()) {
        return std::unexpected("Unsupported language: " + normalized_locale);
    }
    if (auto result = context.gui().save_language_preference(make_runtime_language(normalized_locale)); !result) {
        show_preference_save_failure(context, true, result.error());
        return result;
    }
    preference_error_.clear();
    pending_language_locale_ = normalized_locale;
    auto result = refresh_language_state(context);
    if (!result) {
        return result;
    }
    return show_restart_prompt(context, RestartPromptKind::Language);
}

std::expected<void, std::string> SettingsApp::schedule_theme_switch(
    system::core::AppContext &context,
    std::string_view theme_id
)
{
    if (restart_in_progress_) {
        BROOKESIA_LOGW("Ignore Settings theme selection while restart is in progress");
        return {};
    }
    if (theme_id != THEME_LIGHT && theme_id != THEME_DARK) {
        return std::unexpected("Unsupported theme: " + std::string(theme_id));
    }
    if (theme_id == current_theme_id_) {
        if (auto result = context.gui().save_theme_preference(current_theme_id_); !result) {
            show_preference_save_failure(context, false, result.error());
            return result;
        }
    preference_error_.clear();
    pending_theme_id_.clear();
    restart_prompt_kind_ = RestartPromptKind::None;
    hide_message_dialog_if_visible(context);
    return refresh_theme_state(context);
    }
    if (auto result = context.gui().save_theme_preference(theme_id); !result) {
        show_preference_save_failure(context, false, result.error());
        return result;
    }
    preference_error_.clear();
    pending_theme_id_ = std::string(theme_id);
    auto result = refresh_theme_state(context);
    if (!result) {
        return result;
    }
    return show_restart_prompt(context, RestartPromptKind::Theme);
}

std::expected<void, std::string> SettingsApp::show_restart_prompt(
    system::core::AppContext &context,
    RestartPromptKind kind
)
{
    const auto text_key = kind == RestartPromptKind::Language ?
                          "language.restart_required" : "theme.restart_required";
    const auto text = localized_text(current_locale_, text_key);
    restart_prompt_kind_ = kind;
    if (settings_uses_gsp_backend()) {
        (void)(kind == RestartPromptKind::Language ? refresh_language_state(context) : refresh_theme_state(context));
    }
    ensure_message_dialog(
        context,
        text,
        system::core::MessageDialogIcon::Question,
        0,
        {
            system::core::MessageDialogButton{
                .text = localized_text(current_locale_, "restart_now"),
                .role = system::core::MessageDialogButtonRole::Accept,
            },
            system::core::MessageDialogButton{
                .text = localized_text(current_locale_, "later"),
                .role = system::core::MessageDialogButtonRole::Reject,
            },
        }
    );
    if (message_dialog_request_id_ == system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID) {
        if (settings_uses_gsp_backend()) {
            return {};
        }
        restart_prompt_kind_ = RestartPromptKind::None;
        return std::unexpected("Failed to show restart confirmation dialog");
    }
    return {};
}

void SettingsApp::handle_restart_dialog_result(
    system::core::AppContext &context,
    RestartPromptKind kind,
    const system::core::MessageDialogResult &result
)
{
    if (kind == RestartPromptKind::None) {
        return;
    }
    if (kind == RestartPromptKind::Failure) {
        restart_prompt_kind_ = RestartPromptKind::None;
        return;
    }
    if (result.reason != system::core::MessageDialogCloseReason::Button || result.button_index != 0) {
        restart_prompt_kind_ = RestartPromptKind::None;
        if (kind == RestartPromptKind::Language) {
            (void)refresh_language_state(context);
        } else {
            (void)refresh_theme_state(context);
        }
        return;
    }
    if (restart_in_progress_) {
        return;
    }
    restart_in_progress_ = true;
    restart_prompt_kind_ = RestartPromptKind::None;
    if (restart_iface_ == nullptr) {
        restart_in_progress_ = false;
        show_restart_failure(context, "Restart interface is unavailable");
        return;
    }
    auto restart_result = restart_iface_->restart();
    if (!restart_result) {
        restart_in_progress_ = false;
        show_restart_failure(context, restart_result.error());
        return;
    }
    BROOKESIA_LOGI("Settings restart request accepted");
}

void SettingsApp::show_restart_failure(system::core::AppContext &context, std::string reason)
{
    restart_prompt_kind_ = RestartPromptKind::Failure;
    if (settings_uses_gsp_backend()) {
        BROOKESIA_LOGW("Settings restart failed: %1%", reason);
        preference_error_ = localized_text(current_locale_, "restart_failed");
        if (auto result = current_page_ == PAGE_LANGUAGE ? refresh_language_state(context) :
                          refresh_theme_state(context); !result) {
            BROOKESIA_LOGW("Failed to refresh Settings restart failure: %1%", result.error());
        }
        return;
    }
    ensure_message_dialog(
        context,
        localized_text(current_locale_, "restart_failed") + ": " + std::move(reason),
        system::core::MessageDialogIcon::Warning,
        0,
        {
            system::core::MessageDialogButton{
                .text = localized_text(current_locale_, "close"),
                .role = system::core::MessageDialogButtonRole::Accept,
            },
        }
    );
}

void SettingsApp::show_preference_save_failure(
    system::core::AppContext &context,
    bool language,
    std::string reason
)
{
    restart_prompt_kind_ = RestartPromptKind::Failure;
    if (settings_uses_gsp_backend()) {
        BROOKESIA_LOGW("Failed to save Settings %1% preference: %2%", language ? "language" : "theme", reason);
        preference_error_ = localized_text(current_locale_, language ? "language.save_failed" : "theme.save_failed");
        if (auto result = language ? refresh_language_state(context) : refresh_theme_state(context); !result) {
            BROOKESIA_LOGW("Failed to refresh Settings preference save failure: %1%", result.error());
        }
        return;
    }
    ensure_message_dialog(
        context,
        localized_text(current_locale_, language ? "language.save_failed" : "theme.save_failed") +
        ": " + std::move(reason),
        system::core::MessageDialogIcon::Warning,
        0,
        {
            system::core::MessageDialogButton{
                .text = localized_text(current_locale_, "close"),
                .role = system::core::MessageDialogButtonRole::Accept,
            },
        }
    );
}
