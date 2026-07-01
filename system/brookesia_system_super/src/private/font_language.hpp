/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <algorithm>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "private/system_constants.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::system::super {

struct FontLanguageFallback {
    std::string language;
    std::string font_id;
    bool language_changed = false;
    bool font_changed = false;
};

inline bool contains_language(const std::vector<std::string> &languages, std::string_view language)
{
    return std::find_if(
               languages.begin(),
               languages.end(),
    [language](const std::string & supported_language) {
        return supported_language == language;
    }
           ) != languages.end();
}

template <typename GuiAccess>
std::expected<FontLanguageFallback, std::string> select_font_language_fallback(
    GuiAccess &gui_access,
    std::string_view requested_language,
    std::string_view default_font_id)
{
    const std::string default_font(default_font_id);
    if (default_font.empty()) {
        return std::unexpected("Default font id must not be empty");
    }

    const auto default_font_languages = gui_access.list_supported_languages(default_font);
    if (default_font_languages.empty()) {
        return std::unexpected(
                   "Default font '" + default_font + "' is not registered or declares no supported languages"
               );
    }

    const std::string language(requested_language);
    if (language.empty()) {
        return FontLanguageFallback{
            .language = default_font_languages.front(),
            .font_id = default_font,
            .language_changed = true,
            .font_changed = false,
        };
    }
    if (contains_language(default_font_languages, language)) {
        return FontLanguageFallback{
            .language = language,
            .font_id = default_font,
            .language_changed = false,
            .font_changed = false,
        };
    }

    auto fallback_font_ids = gui_access.list_supported_fonts(language);
    if (!fallback_font_ids.empty()) {
        return FontLanguageFallback{
            .language = language,
            .font_id = std::move(fallback_font_ids.front()),
            .language_changed = false,
            .font_changed = true,
        };
    }

    return FontLanguageFallback{
        .language = default_font_languages.front(),
        .font_id = default_font,
        .language_changed = true,
        .font_changed = false,
    };
}

template <typename GuiAccess>
std::expected<FontLanguageFallback, std::string> apply_font_language_fallback(
    GuiAccess &gui_access,
    std::string_view requested_language,
    std::string_view default_font_id,
    bool reapply_loaded_documents)
{
    auto selection = select_font_language_fallback(gui_access, requested_language, default_font_id);
    if (!selection) {
        return std::unexpected(selection.error());
    }

    auto result = gui_access.set_default_font_for_language(selection->language, selection->font_id);
    if (!result) {
        return std::unexpected(
                   "Failed to set default font '" + selection->font_id + "' for language '" + selection->language +
                   "': " + result.error()
               );
    }

    const std::string requested(requested_language);
    if (selection->font_changed) {
        BROOKESIA_LOGW(
            "Default font '%1%' is not available for language '%2%'; falling back to font '%3%'",
            default_font_id,
            requested,
            selection->font_id
        );
    } else if (selection->language_changed && !requested.empty()) {
        BROOKESIA_LOGW(
            "No registered font supports language '%1%'; falling back to language '%2%' from default font '%3%'",
            requested,
            selection->language,
            default_font_id
        );
    }

    const auto current_language = gui_access.get_language();
    if (current_language == selection->language) {
        return *selection;
    }

    result = gui_access.set_language(selection->language, reapply_loaded_documents);
    if (!result) {
        return std::unexpected("Failed to set Shell language '" + selection->language + "': " + result.error());
    }
    return *selection;
}

template <typename GuiAccess>
std::string select_default_font_language(GuiAccess &gui_access)
{
    const auto default_font_languages = gui_access.list_supported_languages(SUPER_DEFAULT_FONT_ID);
    if (!default_font_languages.empty()) {
        return default_font_languages.front();
    }

    const auto supported_languages = gui_access.list_supported_languages();
    if (!supported_languages.empty()) {
        return supported_languages.front();
    }

    return {};
}

} // namespace esp_brookesia::system::super
