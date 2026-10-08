/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstdio>
#include <string>

#include "private/settings_app_internal.hpp"

#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
#   define GSP_BUNDLE_ENABLE_RAW_IDS 1
#   include "settings_gsp.h"
#   include "brookesia/gui_gsp/app_host.hpp"
#   include "esp_gsp.h"
#   include "ui/gsp_keyboard.hpp"
#   include "ui/gsp_appearance.hpp"
#endif

namespace esp_brookesia::app::settings {

#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP

namespace {

uint16_t scene_for_page(std::string_view page)
{
    if (page == detail::PAGE_HOME) {
        return GSP_SETTINGS_SCENE_SETTINGS_HOME;
    }
    if (page == detail::PAGE_DISPLAY) {
        return GSP_SETTINGS_SCENE_SETTINGS_DISPLAY;
    }
    if (page == detail::PAGE_WIFI) {
        return GSP_SETTINGS_SCENE_SETTINGS_WIFI;
    }
    if (page == detail::PAGE_WIFI_CONNECT) {
        return GSP_SETTINGS_SCENE_SETTINGS_WIFI_CONNECT;
    }
    if (page == detail::PAGE_SOUND) {
        return GSP_SETTINGS_SCENE_SETTINGS_SOUND;
    }
    if (page == detail::PAGE_LANGUAGE) {
        return GSP_SETTINGS_SCENE_SETTINGS_LANGUAGE;
    }
    if (page == detail::PAGE_THEME) {
        return GSP_SETTINGS_SCENE_SETTINGS_THEME;
    }
    return UINT16_MAX;
}

esp_gsp_handle_t current_handle()
{
    auto *host = gui::gsp::AppHost::current();
    if (host == nullptr) {
        return nullptr;
    }
    return host->handle();
}

std::expected<void, std::string> require_handle(esp_gsp_handle_t &handle)
{
    handle = current_handle();
    if (handle == nullptr) {
        return std::unexpected("GSP session is not running");
    }
    return {};
}

} // namespace

class GspSettingsSurface: public SettingsSurface {
public:
    void set_appearance(std::string_view locale, std::string_view theme) override
    {
        locale_ = locale;
        theme_ = theme;
        keyboard_.set_appearance(locale, theme);
    }

    std::expected<void, std::string> set_sound(
        system::core::AppContext &, const SoundPageView &view
    ) override
    {
        if (page_ != detail::PAGE_SOUND) {
            return {};
        }
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        const auto level = view.enabled ? std::to_string(view.volume) + "%" : "--";
        const auto mute = detail::localized_text(locale_, view.muted ? "gsp.muted" : "gsp.unmuted");
        if (esp_gsp_component_set_value(handle, GSP_SETTINGS_SOUND_OBJ_KEY_VOLUME_SLIDER, view.volume) != ESP_GSP_OK ||
                esp_gsp_component_set_enabled(handle, GSP_SETTINGS_SOUND_OBJ_KEY_VOLUME_SLIDER, view.enabled) != ESP_GSP_OK ||
                esp_gsp_component_set_text(handle, GSP_SETTINGS_SOUND_OBJ_KEY_VOLUME_VALUE, level.c_str()) != ESP_GSP_OK ||
                esp_gsp_component_set_text(handle, GSP_SETTINGS_SOUND_OBJ_KEY_MUTE_TOGGLE, mute.c_str()) != ESP_GSP_OK ||
                esp_gsp_component_set_enabled(handle, GSP_SETTINGS_SOUND_OBJ_KEY_MUTE_TOGGLE, view.enabled) != ESP_GSP_OK ||
                esp_gsp_component_set_enabled(handle, GSP_SETTINGS_SOUND_OBJ_KEY_TEST_SOUND, view.test_enabled) != ESP_GSP_OK ||
                esp_gsp_component_set_text(handle, GSP_SETTINGS_SOUND_OBJ_KEY_SOUND_STATUS, view.status.c_str()) != ESP_GSP_OK) {
            return std::unexpected("Failed to update sound controls");
        }
        return {};
    }

    std::expected<void, std::string> set_preferences(
        system::core::AppContext &, const PreferencePageView &view
    ) override
    {
        if (page_ != detail::PAGE_LANGUAGE && page_ != detail::PAGE_THEME) {
            return {};
        }
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        const bool language = page_ == detail::PAGE_LANGUAGE;
        const std::array<uint32_t, 2> options = language ?
                                                std::array<uint32_t, 2> {GSP_SETTINGS_LANGUAGE_OBJ_KEY_OPTION_0, GSP_SETTINGS_LANGUAGE_OBJ_KEY_OPTION_1} :
                                                std::array<uint32_t, 2> {GSP_SETTINGS_THEME_OBJ_KEY_OPTION_0, GSP_SETTINGS_THEME_OBJ_KEY_OPTION_1};
        const std::array<uint32_t, 2> values = language ?
                                               std::array<uint32_t, 2> {GSP_SETTINGS_LANGUAGE_OBJ_KEY_VALUE_0, GSP_SETTINGS_LANGUAGE_OBJ_KEY_VALUE_1} :
                                               std::array<uint32_t, 2> {GSP_SETTINGS_THEME_OBJ_KEY_VALUE_0, GSP_SETTINGS_THEME_OBJ_KEY_VALUE_1};
        for (size_t index = 0; index < options.size(); ++index) {
            if (esp_gsp_component_set_text(handle, options[index], view.labels[index].c_str()) != ESP_GSP_OK ||
                    esp_gsp_component_set_text(handle, values[index], view.values[index].c_str()) != ESP_GSP_OK) {
                return std::unexpected("Failed to update preference choices");
            }
        }
        const auto status = language ? GSP_SETTINGS_LANGUAGE_OBJ_KEY_STATUS : GSP_SETTINGS_THEME_OBJ_KEY_STATUS;
        const auto restart = language ? GSP_SETTINGS_LANGUAGE_OBJ_KEY_RESTART_NOW : GSP_SETTINGS_THEME_OBJ_KEY_RESTART_NOW;
        const auto later = language ? GSP_SETTINGS_LANGUAGE_OBJ_KEY_LATER : GSP_SETTINGS_THEME_OBJ_KEY_LATER;
        if (esp_gsp_component_set_text(handle, status, view.status.c_str()) != ESP_GSP_OK ||
                esp_gsp_component_set_visible(handle, restart, view.show_restart) != ESP_GSP_OK ||
                esp_gsp_component_set_visible(handle, later, view.show_restart) != ESP_GSP_OK) {
            return std::unexpected("Failed to update preference confirmation");
        }
        return {};
    }

    std::expected<void, std::string> set_wifi(
        system::core::AppContext &, const WifiPageView &view
    ) override
    {
        if (page_ != detail::PAGE_WIFI || keyboard_.active()) {
            return {};
        }
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        const auto set_text = [handle](uint32_t key, const std::string & text) {
            return esp_gsp_component_set_text(handle, key, text.c_str()) == ESP_GSP_OK;
        };
        const auto set_enabled = [handle](uint32_t key, bool enabled) {
            return esp_gsp_component_set_enabled(handle, key, enabled) == ESP_GSP_OK;
        };
        if (!set_text(GSP_SETTINGS_WIFI_OBJ_KEY_WIFI_STATUS, view.status) ||
                !set_text(GSP_SETTINGS_WIFI_OBJ_KEY_CONNECTION_STATUS, view.connection) ||
                !set_text(GSP_SETTINGS_WIFI_OBJ_KEY_PAGINATION, view.pagination) ||
                !set_text(GSP_SETTINGS_WIFI_OBJ_KEY_WIFI_TOGGLE, detail::localized_text(locale_, view.enabled ? "gsp.wifi_on" : "gsp.wifi_off")) ||
                !set_enabled(GSP_SETTINGS_WIFI_OBJ_KEY_WIFI_TOGGLE, view.toggle_enabled) ||
                !set_enabled(GSP_SETTINGS_WIFI_OBJ_KEY_REFRESH, view.refresh_enabled) ||
                !set_enabled(GSP_SETTINGS_WIFI_OBJ_KEY_DISCONNECT, view.disconnect_enabled) ||
                !set_enabled(GSP_SETTINGS_WIFI_OBJ_KEY_PREVIOUS, view.previous_enabled) ||
                !set_enabled(GSP_SETTINGS_WIFI_OBJ_KEY_NEXT, view.next_enabled)) {
            return std::unexpected("Failed to update Wi-Fi controls");
        }
        static constexpr std::array<uint32_t, WIFI_PAGE_ROW_COUNT> ssid_keys = {
            GSP_SETTINGS_WIFI_OBJ_KEY_SSID_0, GSP_SETTINGS_WIFI_OBJ_KEY_SSID_1,
            GSP_SETTINGS_WIFI_OBJ_KEY_SSID_2, GSP_SETTINGS_WIFI_OBJ_KEY_SSID_3, GSP_SETTINGS_WIFI_OBJ_KEY_SSID_4,
        };
        static constexpr std::array<uint32_t, WIFI_PAGE_ROW_COUNT> detail_keys = {
            GSP_SETTINGS_WIFI_OBJ_KEY_DETAIL_0, GSP_SETTINGS_WIFI_OBJ_KEY_DETAIL_1,
            GSP_SETTINGS_WIFI_OBJ_KEY_DETAIL_2, GSP_SETTINGS_WIFI_OBJ_KEY_DETAIL_3, GSP_SETTINGS_WIFI_OBJ_KEY_DETAIL_4,
        };
        static constexpr std::array<uint32_t, WIFI_PAGE_ROW_COUNT> row_keys = {
            GSP_SETTINGS_WIFI_OBJ_KEY_NETWORK_0, GSP_SETTINGS_WIFI_OBJ_KEY_NETWORK_1,
            GSP_SETTINGS_WIFI_OBJ_KEY_NETWORK_2, GSP_SETTINGS_WIFI_OBJ_KEY_NETWORK_3,
            GSP_SETTINGS_WIFI_OBJ_KEY_NETWORK_4,
        };
        for (size_t row = 0; row < WIFI_PAGE_ROW_COUNT; ++row) {
            if (!set_text(ssid_keys[row], view.rows[row].ssid) ||
                    !set_text(detail_keys[row], view.rows[row].detail)) {
                return std::unexpected("Failed to update Wi-Fi network labels: " + std::to_string(row));
            }
            const auto enabled = esp_gsp_component_set_enabled(handle, row_keys[row], view.rows[row].enabled);
            if (enabled != ESP_GSP_OK) {
                return std::unexpected("Failed to enable Wi-Fi network row " + std::to_string(row) +
                                       ": " + std::to_string(static_cast<int>(enabled)));
            }
        }
        return {};
    }

    std::expected<void, std::string> set_wifi_connect(
        system::core::AppContext &, const WifiConnectView &view
    ) override
    {
        wifi_connect_view_ = view;
        if (page_ != detail::PAGE_WIFI_CONNECT || keyboard_.active()) {
            return {};
        }
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        if (esp_gsp_component_set_text(handle, GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_SSID,
                                       view.ssid.c_str()) != ESP_GSP_OK ||
                esp_gsp_component_set_text(handle, GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_DETAIL,
                                           view.detail.c_str()) != ESP_GSP_OK ||
                esp_gsp_component_set_text(handle, GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_PASSWORD_VALUE,
                                           view.password_summary.c_str()) != ESP_GSP_OK ||
                esp_gsp_component_set_text(handle, GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_STATUS,
                                           view.status.c_str()) != ESP_GSP_OK ||
                esp_gsp_component_set_enabled(handle, GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_EDIT_PASSWORD,
                                              view.password_enabled) != ESP_GSP_OK ||
                esp_gsp_component_set_enabled(handle, GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_CONNECT,
                                              view.connect_enabled) != ESP_GSP_OK) {
            return std::unexpected("Failed to update the Wi-Fi connection page");
        }
        return {};
    }

    std::expected<void, std::string> show_keyboard(
        system::core::AppContext &context, system::core::KeyboardRequestId request_id,
        const system::core::KeyboardRequestOptions &options, system::core::KeyboardResultHandler completion
    ) override
    {
        if (page_ != detail::PAGE_WIFI_CONNECT) {
            return std::unexpected("The Wi-Fi password page is not open");
        }
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        auto finished = [this, &context, completion = std::move(completion)](
                            const system::core::KeyboardResult & result
        ) {
            // The edit buffer is already closed before the System completes its request.
            restore_keyboard_page(context);
            if (completion) {
                completion(result);
            }
        };
        auto shown = keyboard_.show(handle, context.app_id(), request_id, options, std::move(finished));
        if (!shown && !keyboard_.active()) {
            restore_keyboard_page(context);
        }
        return shown;
    }

    void hide_keyboard(system::core::AppContext &context, system::core::KeyboardRequestId request_id) override
    {
        const bool was_active = keyboard_.active();
        keyboard_.hide(current_handle(), request_id);
        if (was_active && !keyboard_.active()) {
            restore_keyboard_page(context);
        }
    }

    std::expected<bool, std::string> handle_keyboard_action(unsigned scene, unsigned id, unsigned arg) override
    {
        if (scene == GSP_SETTINGS_SCENE_SETTINGS_KEYBOARD || keyboard_.active()) {
            auto handled = keyboard_.handle_action(current_handle(), scene, id, arg);
            if (!handled) {
                return std::unexpected(handled.error());
            }
            // Discard delayed keys and background controls while the modal owns input.
            return true;
        }
        return false;
    }

    std::expected<void, std::string> show_page(
        system::core::AppContext &,
        std::string_view page
    ) override
    {
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        const auto scene = scene_for_page(page);
        if (scene == UINT16_MAX) {
            return std::unexpected("Unsupported settings page");
        }
        if (esp_gsp_goto_scene(handle, scene, ESP_GSP_NO_TRANSITION) != ESP_GSP_OK) {
            return std::unexpected("Failed to show the Settings page");
        }
        // The scene command has been accepted. Keep input routing aligned with it even if
        // a later render fence or cosmetic label update fails.
        page_ = page;
        const auto flushed = esp_gsp_flush(handle, 500);
        if (flushed != ESP_GSP_OK) {
            BROOKESIA_LOGW("Failed to settle the Settings page: %1%", static_cast<int>(flushed));
        }
        auto appearance = apply_settings_appearance(handle, scene, locale_, theme_);
        if (!appearance) {
            BROOKESIA_LOGW("Failed to update Settings page labels: %1%", appearance.error());
        }
        return {};
    }

    std::expected<void, std::string> set_header(
        system::core::AppContext &,
        std::string_view title,
        bool show_back
    ) override
    {
        if (keyboard_.active()) {
            return {};
        }
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        (void)show_back;
        const auto title_key = page_ == detail::PAGE_WIFI_CONNECT ? GSP_SETTINGS_WIFI_CONNECT_OBJ_KEY_HEADER_TITLE :
                               page_ == detail::PAGE_WIFI ? GSP_SETTINGS_WIFI_OBJ_KEY_HEADER_TITLE :
                               page_ == detail::PAGE_SOUND ? GSP_SETTINGS_SOUND_OBJ_KEY_HEADER_TITLE :
                               page_ == detail::PAGE_LANGUAGE ? GSP_SETTINGS_LANGUAGE_OBJ_KEY_HEADER_TITLE :
                               page_ == detail::PAGE_THEME ? GSP_SETTINGS_THEME_OBJ_KEY_HEADER_TITLE :
                               page_ == detail::PAGE_DISPLAY ? GSP_SETTINGS_DISPLAY_OBJ_KEY_HEADER_TITLE :
                               GSP_SETTINGS_HOME_OBJ_KEY_HEADER_TITLE;
        if (esp_gsp_component_set_text(handle, title_key, std::string(title).c_str()) != ESP_GSP_OK) {
            return std::unexpected("Failed to update the Settings title");
        }
        return {};
    }

    std::expected<void, std::string> set_brightness(
        system::core::AppContext &,
        int value,
        bool enabled,
        std::string_view status,
        std::string_view
    ) override
    {
        esp_gsp_handle_t handle = nullptr;
        auto ready = require_handle(handle);
        if (!ready) {
            return ready;
        }
        const auto slider_result = esp_gsp_component_set_value(
                                       handle, GSP_SETTINGS_DISPLAY_OBJ_KEY_BRIGHTNESS_SLIDER, value
                                   );
        if (slider_result != ESP_GSP_OK) {
            return std::unexpected(
                       "Failed to update the brightness slider: " + std::to_string(static_cast<int>(slider_result))
                   );
        }
        if (esp_gsp_component_set_enabled(
                    handle, GSP_SETTINGS_DISPLAY_OBJ_KEY_BRIGHTNESS_SLIDER, enabled
                ) != ESP_GSP_OK) {
            return std::unexpected("Failed to update the brightness slider state");
        }
        char value_text[16];
        if (enabled) {
            std::snprintf(value_text, sizeof(value_text), "%d%%", value);
        } else {
            std::snprintf(value_text, sizeof(value_text), "--");
        }
        if (esp_gsp_component_set_text(handle, GSP_SETTINGS_DISPLAY_OBJ_KEY_BRIGHTNESS_VALUE, value_text) != ESP_GSP_OK) {
            return std::unexpected("Failed to update the brightness value");
        }
        if (esp_gsp_component_set_text(
                    handle, GSP_SETTINGS_DISPLAY_OBJ_KEY_BRIGHTNESS_STATUS, std::string(status).c_str()
                ) != ESP_GSP_OK) {
            return std::unexpected("Failed to update the brightness status");
        }
        return {};
    }
private:
    void restore_keyboard_page(system::core::AppContext &context)
    {
        if (page_ != detail::PAGE_WIFI_CONNECT || current_handle() == nullptr) {
            return;
        }
        auto shown = show_page(context, detail::PAGE_WIFI_CONNECT);
        if (shown) {
            shown = set_wifi_connect(context, wifi_connect_view_);
        }
        if (!shown) {
            BROOKESIA_LOGW("Failed to restore the Wi-Fi password page: %1%", shown.error());
        }
    }

    std::string page_;
    std::string locale_ = "en";
    std::string theme_ = "light";
    WifiConnectView wifi_connect_view_;
    GspKeyboard keyboard_;
};

std::unique_ptr<SettingsSurface> make_gsp_settings_surface()
{
    return std::make_unique<GspSettingsSurface>();
}

esp_gsp_config_t settings_dark_config();

std::span<const std::byte> SettingsApp::gui_bundle() const
{
    const auto config = *static_cast<const esp_gsp_config_t *>(gui_runtime_config());
    return {reinterpret_cast<const std::byte *>(config.bundle), config.bundle_size};
}

const void *SettingsApp::gui_runtime_config() const
{
    static esp_gsp_config_t config;
    const auto light = gsp_settings_config();
    // App permissions are available after installation, before the first GUI load.
    const auto theme = impl_->installed_context != nullptr ?
                       impl_->installed_context->gui().get_theme_language().theme_id : current_theme_id_;
    config = theme == detail::THEME_DARK ? settings_dark_config() : light;
    // Both palettes use the same font and object layout.
    config.ttf = light.ttf;
    config.ttf_size = light.ttf_size;
    return &config;
}

std::expected<void, std::string> SettingsApp::start_gsp_pages(system::core::AppContext &context)
{
    context_ = &context;
    const auto active = context.gui().get_theme_language();
    current_locale_ = detail::normalize_locale(active.language);
    current_theme_id_ = active.theme_id == detail::THEME_DARK ? detail::THEME_DARK : detail::THEME_LIGHT;
    surface_->set_appearance(current_locale_, current_theme_id_);
    current_page_ = detail::PAGE_HOME;
    pending_language_locale_.clear();
    if (const auto stored = context.gui().get_stored_language_preference(); stored && *stored != current_locale_ &&
            (*stored == detail::LOCALE_EN || *stored == detail::LOCALE_ZH_CN)) {
        pending_language_locale_ = *stored;
    }
    pending_theme_id_.clear();
    if (const auto stored = context.gui().get_stored_theme_preference(); stored && *stored != current_theme_id_ &&
            (*stored == detail::THEME_LIGHT || *stored == detail::THEME_DARK)) {
        pending_theme_id_ = *stored;
    }
    preference_error_.clear();
    restart_prompt_kind_ = RestartPromptKind::None;
    restart_in_progress_ = false;
    restart_iface_ = hal::acquire_first_interface<hal::system::GeneralIface>();
    device_ui_state_ = DeviceUiState{};
    arm_ui_epoch();

    auto loaded = load_backlight_state();
    if (!loaded) {
        shutdown_brightness_callbacks();
        return loaded;
    }
    auto audio = load_audio_state();
    if (!audio) {
        shutdown_brightness_callbacks();
        return audio;
    }
    subscribe_device_events();
    auto shown = surface_->show_page(context, detail::PAGE_HOME);
    if (!shown) {
        shutdown_brightness_callbacks();
        return shown;
    }
    auto header = refresh_header(context);
    if (!header) {
        shutdown_brightness_callbacks();
        return header;
    }
    if (auto handle = current_handle(); handle != nullptr) {
        (void)esp_gsp_set_swipe_enabled(handle, false);
    }
    BROOKESIA_LOGI("Settings GSP pages ready");
    return {};
}

std::expected<void, std::string> SettingsApp::handle_gsp_action(
    system::core::AppContext &context,
    std::string_view action
)
{
    if (action.starts_with("scene:")) {
        return {};
    }
    unsigned scene = 0;
    unsigned id = 0;
    unsigned arg = 0;
    const std::string text(action);
    if (std::sscanf(text.c_str(), "%u:%u:%u", &scene, &id, &arg) != 3) {
        return std::unexpected("Unrecognized GSP action");
    }
    const auto keyboard_action = surface_->handle_keyboard_action(scene, id, arg);
    if (!keyboard_action) {
        return std::unexpected(keyboard_action.error());
    }
    if (*keyboard_action) {
        return {};
    }
    if (scene != scene_for_page(current_page_)) {
        return {};
    }
    const auto open_page = [&](std::string_view page) -> std::expected<void, std::string> {
        if (page == detail::PAGE_THEME)
        {
            theme_from_display_ = current_page_ == detail::PAGE_DISPLAY;
        }
        if (page == detail::PAGE_SOUND)
        {
            auto loaded = load_audio_state();
            if (!loaded) {
                return loaded;
            }
            // Keep the App-owned mailbox so an in-flight control completion can
            // clear its pending state even when this page is closed and reopened.
            if (!sound_ui_mailbox_) {
                disconnect_device_events();
                subscribe_device_events();
            }
        }
        auto shown = surface_->show_page(context, page);
        if (!shown)
        {
            return shown;
        }
        current_page_ = page;
        preference_error_.clear();
        restart_prompt_kind_ = RestartPromptKind::None;
        auto header = refresh_header(context);
        if (!header)
        {
            return header;
        }
        if (page == detail::PAGE_SOUND)
        {
            return refresh_sound_state(context);
        }
        if (page == detail::PAGE_LANGUAGE)
        {
            return refresh_language_state(context);
        }
        if (page == detail::PAGE_THEME)
        {
            return refresh_theme_state(context);
        }
        if (page == detail::PAGE_DISPLAY)
        {
            return refresh_display_state(context);
        }
        return {};
    };
    if (scene == GSP_SETTINGS_SCENE_SETTINGS_HOME) {
        if (id == GSP_SETTINGS_HOME_ACT_ID_OPEN_SOUND) {
            return open_page(detail::PAGE_SOUND);
        }
        if (id == GSP_SETTINGS_HOME_ACT_ID_OPEN_LANGUAGE) {
            return open_page(detail::PAGE_LANGUAGE);
        }
        if (id == GSP_SETTINGS_HOME_ACT_ID_OPEN_THEME) {
            return open_page(detail::PAGE_THEME);
        }
    }
    if (scene == GSP_SETTINGS_SCENE_SETTINGS_DISPLAY && id == GSP_SETTINGS_DISPLAY_ACT_ID_OPEN_THEME) {
        return open_page(detail::PAGE_THEME);
    }
    if (scene == GSP_SETTINGS_SCENE_SETTINGS_SOUND) {
        if (id == GSP_SETTINGS_SOUND_ACT_ID_HEADER_BACK) {
            return open_page(detail::PAGE_HOME);
        }
        if (id == GSP_SETTINGS_SOUND_ACT_ID_VOLUME) {
            set_volume(context, detail::clamp_percent(arg));
        }
        if (id == GSP_SETTINGS_SOUND_ACT_ID_MUTE) {
            set_mute(context, !device_ui_state_.muted);
        }
        if (id == GSP_SETTINGS_SOUND_ACT_ID_TEST_SOUND) {
            return play_test_sound(context);
        }
        return {};
    }
    if (scene == GSP_SETTINGS_SCENE_SETTINGS_LANGUAGE || scene == GSP_SETTINGS_SCENE_SETTINGS_THEME) {
        const bool language = scene == GSP_SETTINGS_SCENE_SETTINGS_LANGUAGE;
        const auto back = language ? GSP_SETTINGS_LANGUAGE_ACT_ID_HEADER_BACK : GSP_SETTINGS_THEME_ACT_ID_HEADER_BACK;
        const auto first = language ? GSP_SETTINGS_LANGUAGE_ACT_ID_SELECT_0 : GSP_SETTINGS_THEME_ACT_ID_SELECT_0;
        const auto second = language ? GSP_SETTINGS_LANGUAGE_ACT_ID_SELECT_1 : GSP_SETTINGS_THEME_ACT_ID_SELECT_1;
        const auto restart = language ? GSP_SETTINGS_LANGUAGE_ACT_ID_RESTART_NOW : GSP_SETTINGS_THEME_ACT_ID_RESTART_NOW;
        const auto later = language ? GSP_SETTINGS_LANGUAGE_ACT_ID_LATER : GSP_SETTINGS_THEME_ACT_ID_LATER;
        if (id == back) {
            return open_page(!language && theme_from_display_ ? detail::PAGE_DISPLAY : detail::PAGE_HOME);
        }
        if (id == first || id == second) {
            return language ? schedule_language_switch(context, id == first ? detail::LOCALE_EN : detail::LOCALE_ZH_CN) :
                   schedule_theme_switch(context, id == first ? detail::THEME_LIGHT : detail::THEME_DARK);
        }
        if (id == restart || id == later) {
            system::core::MessageDialogResult result;
            result.reason = system::core::MessageDialogCloseReason::Button;
            result.button_index = id == restart ? 0 : 1;
            handle_restart_dialog_result(context, restart_prompt_kind_, result);
        }
        return {};
    }
    if ((scene == GSP_SETTINGS_SCENE_SETTINGS_HOME) && (id == GSP_SETTINGS_HOME_ACT_ID_OPEN_WIFI)) {
        auto shown = surface_->show_page(context, detail::PAGE_WIFI);
        if (!shown) {
            return shown;
        }
        current_page_ = detail::PAGE_WIFI;
        return open_wifi_page(context);
    }
    if (scene == GSP_SETTINGS_SCENE_SETTINGS_WIFI) {
        if (id == GSP_SETTINGS_WIFI_ACT_ID_HEADER_BACK) {
            auto shown = surface_->show_page(context, detail::PAGE_HOME);
            if (!shown) {
                return shown;
            }
            close_wifi_page(context);
            current_page_ = detail::PAGE_HOME;
            return refresh_header(context);
        }
        if (id == GSP_SETTINGS_WIFI_ACT_ID_WIFI_TOGGLE) {
            return toggle_wifi_page(context);
        }
        if (id == GSP_SETTINGS_WIFI_ACT_ID_REFRESH) {
            return scan_wifi_page(context);
        }
        if (id == GSP_SETTINGS_WIFI_ACT_ID_DISCONNECT) {
            return disconnect_current_wifi(context);
        }
        static constexpr std::array<unsigned, WIFI_PAGE_ROW_COUNT> selection_actions = {
            GSP_SETTINGS_WIFI_ACT_ID_SELECT_0, GSP_SETTINGS_WIFI_ACT_ID_SELECT_1,
            GSP_SETTINGS_WIFI_ACT_ID_SELECT_2, GSP_SETTINGS_WIFI_ACT_ID_SELECT_3, GSP_SETTINGS_WIFI_ACT_ID_SELECT_4,
        };
        for (size_t row = 0; row < selection_actions.size(); ++row) {
            if (id == selection_actions[row]) {
                return select_wifi_page_network(context, row);
            }
        }
        if (id == GSP_SETTINGS_WIFI_ACT_ID_PREVIOUS && available_wifi_page_ > 0) {
            --available_wifi_page_;
        } else if (id == GSP_SETTINGS_WIFI_ACT_ID_NEXT) {
            ++available_wifi_page_;
        }
        return render_wifi_page(context);
    }
    if (scene == GSP_SETTINGS_SCENE_SETTINGS_WIFI_CONNECT) {
        if (id == GSP_SETTINGS_WIFI_CONNECT_ACT_ID_HEADER_BACK) {
            return back_wifi_connect(context);
        }
        if (id == GSP_SETTINGS_WIFI_CONNECT_ACT_ID_EDIT_PASSWORD) {
            auto shown = show_wifi_password_keyboard(context);
            if (!shown) {
                BROOKESIA_LOGW("Failed to show Settings Wi-Fi password keyboard: %1%", shown.error());
                wifi_page_.error = detail::localized_text(current_locale_, "wifi.keyboard_unavailable");
                return refresh_wifi_connect_state(context);
            }
            return {};
        }
        if (id == GSP_SETTINGS_WIFI_CONNECT_ACT_ID_CONNECT) {
            return submit_wifi_connect(context);
        }
        return {};
    }
    if ((scene == GSP_SETTINGS_SCENE_SETTINGS_HOME) && (id == GSP_SETTINGS_HOME_ACT_ID_HEADER_EXIT)) {
        return context.system_service().stop_app(context.app_id());
    }
    if ((scene == GSP_SETTINGS_SCENE_SETTINGS_HOME) && (id == GSP_SETTINGS_HOME_ACT_ID_OPEN_DISPLAY)) {
        auto loaded = load_backlight_state();
        if (!loaded) {
            return loaded;
        }
        auto shown = surface_->show_page(context, detail::PAGE_DISPLAY);
        if (!shown) {
            return shown;
        }
        current_page_ = detail::PAGE_DISPLAY;
        auto header = refresh_header(context);
        if (!header) {
            return header;
        }
        return refresh_display_state(context);
    }
    if ((scene == GSP_SETTINGS_SCENE_SETTINGS_DISPLAY) && (id == GSP_SETTINGS_DISPLAY_ACT_ID_HEADER_BACK)) {
        auto shown = surface_->show_page(context, detail::PAGE_HOME);
        if (!shown) {
            return shown;
        }
        current_page_ = detail::PAGE_HOME;
        return refresh_header(context);
    }
    if ((scene == GSP_SETTINGS_SCENE_SETTINGS_DISPLAY) && (id == GSP_SETTINGS_DISPLAY_ACT_ID_BRIGHTNESS)) {
        set_brightness(context, detail::clamp_percent(static_cast<double>(arg)));
        return {};
    }
    BROOKESIA_LOGI("Ignore unsupported Settings GSP action %1%", id);
    return {};
}

#else

std::span<const std::byte> SettingsApp::gui_bundle() const
{
    return {};
}

const void *SettingsApp::gui_runtime_config() const
{
    return nullptr;
}

std::expected<void, std::string> SettingsApp::start_gsp_pages(system::core::AppContext &)
{
    return std::unexpected("GSP backend is not enabled");
}

std::expected<void, std::string> SettingsApp::handle_gsp_action(system::core::AppContext &, std::string_view)
{
    return std::unexpected("GSP backend is not enabled");
}

#endif

std::expected<void, std::string> SettingsApp::show_gsp_keyboard(
    system::core::KeyboardRequestId request_id,
    const system::core::KeyboardRequestOptions &options,
    system::core::KeyboardResultHandler completion
)
{
    if (!settings_uses_gsp_backend() || context_ == nullptr || current_page_ != detail::PAGE_WIFI_CONNECT ||
            !wifi_page_.mailbox || !wifi_ui_state_.service_ready || !wifi_ui_state_.enabled) {
        return std::unexpected("The Settings Wi-Fi password page is not active");
    }
    return surface_->show_keyboard(*context_, request_id, options, std::move(completion));
}

void SettingsApp::hide_gsp_keyboard(system::core::KeyboardRequestId request_id)
{
    if (settings_uses_gsp_backend() && context_ != nullptr) {
        surface_->hide_keyboard(*context_, request_id);
    }
}

} // namespace esp_brookesia::app::settings
