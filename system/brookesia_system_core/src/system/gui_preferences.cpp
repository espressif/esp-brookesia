/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_core/macro_configs.h"
#if !BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif

#include "brookesia/service_helper/system/storage.hpp"
#include "private/utils.hpp"
#include "private/system/impl.hpp"

namespace esp_brookesia::system::core {
namespace {

using StorageHelper = service::helper::Storage;

static constexpr const char *GUI_PREFERENCE_STORAGE_NAMESPACE = "brookesia.system.core";
static constexpr const char *GUI_PREFERENCE_THEME_KEY = "ThemeId";
static constexpr const char *GUI_PREFERENCE_LANGUAGE_KEY = "Language";
static constexpr uint32_t GUI_PREFERENCE_STORAGE_TIMEOUT_MS = 500;

std::optional<std::string> load_string_preference(std::string_view key)
{
    auto result = StorageHelper::get_key_value<std::string>(
                      GUI_PREFERENCE_STORAGE_NAMESPACE,
                      std::string(key),
                      GUI_PREFERENCE_STORAGE_TIMEOUT_MS
                  );
    if (!result) {
        BROOKESIA_LOGD("System GUI preference '%1%' is not stored yet: %2%", key, result.error());
        return std::nullopt;
    }
    if (result->empty()) {
        return std::nullopt;
    }
    BROOKESIA_LOGI("System GUI preference '%1%: %2%' is loaded", key, *result);
    return *result;
}

void save_string_preference(std::string_view key, std::string value)
{
    const auto handler = [key](service::FunctionResult && result) {
        if (result.success) {
            return;
        }
        BROOKESIA_LOGW("Failed to save system GUI preference '%1%': %2%", key, result.error_message);
    };
    if (!StorageHelper::save_key_value_async(
                GUI_PREFERENCE_STORAGE_NAMESPACE,
                std::string(key),
                value,
                service::ServiceBase::FunctionResultHandler(handler)
            )) {
        BROOKESIA_LOGW("Failed to submit system GUI preference '%1%' save request", key);
    } else {
        BROOKESIA_LOGI("System GUI preference '%1%: %2%' is saved", key, value);
    }
}

} // namespace

void System::Impl::load_gui_preferences()
{
    stored_theme_id_ = load_string_preference(GUI_PREFERENCE_THEME_KEY);
    stored_language_ = load_string_preference(GUI_PREFERENCE_LANGUAGE_KEY);
}

std::optional<std::string> System::get_stored_gui_theme_id() const
{
    return impl_->stored_theme_id_;
}

std::optional<std::string> System::get_stored_gui_language() const
{
    return impl_->stored_language_;
}

void System::begin_gui_preferences_restore()
{
    impl_->gui_preferences_restoring_ = true;
}

void System::mark_gui_preferences_restored()
{
    impl_->gui_preferences_restoring_ = false;
    impl_->gui_preferences_restored_ = true;
}

void System::Impl::persist_gui_theme_preference(std::string_view theme_id)
{
    if (!gui_preferences_restored_ || gui_preferences_restoring_ || theme_id.empty()) {
        return;
    }
    save_string_preference(GUI_PREFERENCE_THEME_KEY, std::string(theme_id));
}

void System::Impl::persist_gui_language_preference(std::string_view language)
{
    if (!gui_preferences_restored_ || gui_preferences_restoring_ || language.empty()) {
        return;
    }
    save_string_preference(GUI_PREFERENCE_LANGUAGE_KEY, std::string(language));
}

} // namespace esp_brookesia::system::core
