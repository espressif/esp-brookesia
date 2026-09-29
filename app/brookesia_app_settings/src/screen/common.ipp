/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

std::string SettingsApp::page_title_text(std::string_view page) const
{
    const auto locale = current_locale_.empty() ? std::string_view(LOCALE_EN) : std::string_view(current_locale_);
    auto text = localized_text(locale, page);
    if (text != page) {
        return text;
    }
    if (page == PAGE_HOME) {
        return "Settings";
    }
    if (page == PAGE_DISPLAY) {
        return "Display & brightness";
    }
    return std::string(page);
}

std::expected<void, std::string> SettingsApp::refresh_header(system::core::AppContext &context)
{
    return surface_->set_header(context, page_title_text(current_page_), current_page_ != PAGE_HOME);
}

std::expected<void, std::string> SettingsApp::refresh_theme_state(system::core::AppContext &context)
{
    const bool dark = current_theme_id_ == THEME_DARK;
    auto tokens = load_selectable_theme_tokens(context, current_theme_id_);
    if (!tokens) {
        return std::unexpected(tokens.error());
    }

    std::vector<gui::BindingValueUpdate> updates;
    auto light_label = localized_text(current_locale_, "light");
    if (pending_theme_id_ == THEME_LIGHT) {
        light_label += " (" + localized_text(current_locale_, "restart_required") + ")";
    }
    auto dark_label = localized_text(current_locale_, "dark");
    if (pending_theme_id_ == THEME_DARK) {
        dark_label += " (" + localized_text(current_locale_, "restart_required") + ")";
    }
    add_theme_mode_updates(
        updates, "/display/page/theme_modes/light_mode", !dark, *tokens, "#f3f3f3", std::move(light_label)
    );
    add_theme_mode_updates(
        updates, "/display/page/theme_modes/dark_mode", dark, *tokens, "#38393a", std::move(dark_label)
    );
    add_binding_update(
        updates,
        "/settings_home/page/main_list/display/value_box/value",
        "labelProps.text",
        localized_text(current_locale_, dark ? "dark" : "light")
    );
    return context.gui().set_binding_values(updates);
}

std::expected<void, std::string> SettingsApp::refresh_language_state(system::core::AppContext &context)
{
    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(
        updates,
        "/more/page/language_card/language/value_box/value",
        "labelProps.text",
        get_language_summary_name(current_locale_)
    );
    for (const auto &path : dynamic_language_paths_) {
        const auto instance_id = path.substr(std::string_view(LANGUAGE_LIST_PARENT).size() + 1);
        auto it = language_instance_to_locale_.find(instance_id);
        if (it == language_instance_to_locale_.end()) {
            continue;
        }
        add_binding_update(
            updates,
            path + "/title_box/title",
            "labelProps.text",
            get_language_display_name(current_locale_, it->second)
        );
        add_binding_update(
            updates,
            path + "/value_box/value",
            "labelProps.text",
            it->second == pending_language_locale_ ?
            localized_text(current_locale_, "restart_required") :
            it->second == current_locale_ ? localized_text(current_locale_, "current") : ""
        );
    }
    return context.gui().set_binding_values(updates);
}
