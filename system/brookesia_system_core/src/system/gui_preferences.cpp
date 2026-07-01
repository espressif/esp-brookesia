/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_core/macro_configs.h"
#if !BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif

#include <expected>
#include <optional>
#include <string>
#include <string_view>

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

struct GuiPreferenceKvName {
    std::string nspace;
    std::string key;
};

std::expected<std::string, std::string> make_gui_preference_kv_namespace()
{
    auto result = StorageHelper::make_kv_namespace(
    {GUI_PREFERENCE_STORAGE_NAMESPACE},
    ".",
    GUI_PREFERENCE_STORAGE_TIMEOUT_MS
    );
    if (!result) {
        return std::unexpected(
                   "Failed to make system GUI preference KV namespace: " + result.error()
               );
    }
    if (result->hashed) {
        BROOKESIA_LOGD("%1%", result->warning);
    }
    return result->name;
}

std::expected<std::string, std::string> make_gui_preference_kv_key(std::string_view raw_key)
{
    auto result = StorageHelper::make_kv_key({raw_key}, ".", GUI_PREFERENCE_STORAGE_TIMEOUT_MS);
    if (!result) {
        return std::unexpected(
                   "Failed to make system GUI preference KV key '" + std::string(raw_key) + "': " + result.error()
               );
    }
    if (result->hashed) {
        BROOKESIA_LOGD("%1%", result->warning);
    }
    return result->name;
}

std::expected<GuiPreferenceKvName, std::string> make_gui_preference_kv_name(std::string_view raw_key)
{
    auto namespace_result = make_gui_preference_kv_namespace();
    if (!namespace_result) {
        return std::unexpected(namespace_result.error());
    }

    auto key_result = make_gui_preference_kv_key(raw_key);
    if (!key_result) {
        return std::unexpected(key_result.error());
    }

    return GuiPreferenceKvName{
        .nspace = std::move(namespace_result.value()),
        .key = std::move(key_result.value()),
    };
}

std::optional<std::string> load_string_preference(std::string_view key)
{
    auto kv_name = make_gui_preference_kv_name(key);
    if (!kv_name) {
        BROOKESIA_LOGW("Skip loading system GUI preference '%1%': %2%", key, kv_name.error());
        return std::nullopt;
    }

    auto result = StorageHelper::get_key_value<std::string>(
                      kv_name->nspace,
                      kv_name->key,
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
    auto kv_name = make_gui_preference_kv_name(key);
    if (!kv_name) {
        BROOKESIA_LOGW("Skip saving system GUI preference '%1%': %2%", key, kv_name.error());
        return;
    }

    auto raw_key = std::string(key);
    const auto handler = [raw_key](service::FunctionResult && result) {
        if (result.success) {
            return;
        }
        BROOKESIA_LOGW("Failed to save system GUI preference '%1%': %2%", raw_key, result.error_message);
    };
    if (!StorageHelper::save_key_value_async(
                kv_name->nspace,
                kv_name->key,
                value,
                service::ServiceBase::FunctionResultHandler(handler)
            )) {
        BROOKESIA_LOGW("Failed to submit system GUI preference '%1%' save request", raw_key);
    } else {
        BROOKESIA_LOGI("System GUI preference '%1%: %2%' is saved", raw_key, value);
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
