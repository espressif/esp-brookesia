/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

std::string AppStoreApp::localized(const std::map<std::string, std::string> &values) const
{
    const auto language = current_language();
    if (auto it = values.find(language); it != values.end() && !it->second.empty()) {
        return it->second;
    }
    if (language == "zh_CN") {
        if (auto it = values.find("zh-CN"); it != values.end() && !it->second.empty()) {
            return it->second;
        }
    }
    if (auto it = values.find("en"); it != values.end() && !it->second.empty()) {
        return it->second;
    }
    if (!values.empty()) {
        return values.begin()->second;
    }
    return "App";
}

std::string AppStoreApp::tr(std::string_view key) const
{
    if (auto it = i18n_strings_.find(std::string(key)); it != i18n_strings_.end()) {
        return it->second;
    }
    return std::string(key);
}

std::string AppStoreApp::current_language() const
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
