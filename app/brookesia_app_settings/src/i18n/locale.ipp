/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

std::expected<void, std::string> SettingsApp::apply_locale(system::core::AppContext &context)
{
    if (current_locale_ != LOCALE_EN && !i18n_string_tables.contains(LOCALE_EN)) {
        auto english_bundle = load_i18n_bundle(context, LOCALE_EN);
        if (english_bundle) {
            i18n_string_tables[LOCALE_EN] = std::move(english_bundle->strings);
        } else {
            BROOKESIA_LOGW("Failed to load Settings default i18n fallback: %1%", english_bundle.error());
        }
    }

    auto bundle = load_i18n_bundle(context, current_locale_);
    if (!bundle) {
        return std::unexpected(bundle.error());
    }
    i18n_string_tables[std::string(current_locale_)] = std::move(bundle->strings);
    return context.gui().set_binding_values(bundle->updates);
}
