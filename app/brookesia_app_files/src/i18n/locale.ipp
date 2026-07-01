/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

std::expected<void, std::string> FileManagerApp::refresh_i18n(system::core::AppContext &)
{
    const auto locale = current_language();
    if (locale == applied_i18n_locale_ && !i18n_strings_.empty()) {
        return {};
    }
    auto strings = load_i18n_strings(resource_dir_, locale);
    if (!strings) {
        if (locale == "en") {
            return std::unexpected(strings.error());
        }
        BROOKESIA_LOGW("Failed to load Files locale '%1%': %2%", locale, strings.error());
        strings = load_i18n_strings(resource_dir_, "en");
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

std::string FileManagerApp::tr(std::string_view key) const
{
    if (auto it = i18n_strings_.find(std::string(key)); it != i18n_strings_.end()) {
        return it->second;
    }
    return std::string(key);
}

std::string FileManagerApp::current_language() const
{
    if (context_ == nullptr) {
        return "en";
    }
    const auto environment = context_->system_service().get_environment();
    auto it = environment.find("language");
    if (it != environment.end() && it->value().is_string()) {
        return it->value().as_string().c_str();
    }
    return "en";
}
