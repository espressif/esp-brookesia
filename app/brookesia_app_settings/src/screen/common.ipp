/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

std::expected<void, std::string> SettingsApp::refresh_header(system::core::AppContext &context)
{
    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(
        updates,
        HEADER_BACK_PATH,
        "commonProps.hidden",
        current_page_ == PAGE_HOME ? "true" : "false"
    );
    add_binding_update(
        updates,
        HEADER_TITLE_PATH,
        "labelProps.text",
        localized_text(current_locale_, current_page_)
    );
    return context.gui().set_binding_values(updates);
}

std::expected<void, std::string> SettingsApp::refresh_theme_state(system::core::AppContext &context)
{
    const bool dark = current_theme_id_ == THEME_DARK;
    auto tokens = load_selectable_theme_tokens(context, current_theme_id_);
    if (!tokens) {
        return std::unexpected(tokens.error());
    }

    std::vector<gui::BindingValueUpdate> updates;
    add_theme_mode_updates(updates, "/display/page/theme_modes/light_mode", !dark, *tokens, "#f3f3f3");
    add_theme_mode_updates(updates, "/display/page/theme_modes/dark_mode", dark, *tokens, "#38393a");
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
            it->second == current_locale_ ? localized_text(current_locale_, "current") : ""
        );
    }
    return context.gui().set_binding_values(updates);
}
