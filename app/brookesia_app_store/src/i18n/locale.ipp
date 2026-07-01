/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

std::expected<void, std::string> AppStoreApp::refresh_i18n(system::core::AppContext &context)
{
    const auto locale = current_language();
    if (locale == applied_i18n_locale_ && !i18n_strings_.empty()) {
        return {};
    }
    auto strings = load_i18n_strings(resolve_resource_dir(context), locale);
    if (!strings) {
        if (locale == "en") {
            return std::unexpected(strings.error());
        }
        BROOKESIA_LOGW("Failed to load App Store locale '%1%': %2%", locale, strings.error());
        strings = load_i18n_strings(resolve_resource_dir(context), "en");
        if (!strings) {
            return std::unexpected(strings.error());
        }
        applied_i18n_locale_ = "en";
    } else {
        applied_i18n_locale_ = locale;
    }
    i18n_strings_ = std::move(*strings);
    return {};
}
