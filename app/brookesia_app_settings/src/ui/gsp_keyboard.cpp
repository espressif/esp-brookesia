/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifdef ESP_PLATFORM
#   include "sdkconfig.h"
#endif

#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <utility>

#define GSP_BUNDLE_ENABLE_RAW_IDS 1
#include "settings_gsp.h"

#include "ui/gsp_keyboard.hpp"

namespace esp_brookesia::app::settings {

namespace {

constexpr size_t MAX_PASSWORD_BYTES = 64;
constexpr std::array<unsigned, 36> KEY_ACTIONS = {
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_0_0,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_0_1,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_0_2,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_0_3,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_0_4,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_0_5,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_0_6,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_0_7,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_0_8,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_0_9,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_1_0,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_1_1,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_1_2,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_1_3,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_1_4,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_1_5,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_1_6,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_1_7,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_1_8,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_1_9,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_2_0,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_2_1,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_2_2,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_2_3,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_2_4,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_2_5,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_2_6,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_2_7,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_2_8,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_3_0,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_3_1,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_3_2,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_3_3,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_3_4,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_3_5,
    GSP_SETTINGS_KEYBOARD_ACT_ID_KEY_3_6,
};
constexpr std::array<uint32_t, 36> KEY_OBJECTS = {
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_0_0,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_0_1,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_0_2,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_0_3,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_0_4,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_0_5,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_0_6,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_0_7,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_0_8,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_0_9,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_1_0,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_1_1,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_1_2,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_1_3,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_1_4,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_1_5,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_1_6,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_1_7,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_1_8,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_1_9,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_2_0,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_2_1,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_2_2,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_2_3,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_2_4,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_2_5,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_2_6,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_2_7,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_2_8,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_3_0,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_3_1,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_3_2,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_3_3,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_3_4,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_3_5,
    GSP_SETTINGS_KEYBOARD_OBJ_KEY_KEY_3_6,
};
constexpr std::array<std::string_view, 4> LOWER_ROWS = {
    "1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm",
};
constexpr std::array<std::string_view, 4> SYMBOL_ROWS = {
    "!@#$%^&*()", "-_=+[]{}\\|", ";:'\",.<>?", "~`/+=_-",
};

std::optional<size_t> find_key(unsigned id)
{
    const auto found = std::find(KEY_ACTIONS.begin(), KEY_ACTIONS.end(), id);
    if (found == KEY_ACTIONS.end()) {
        return std::nullopt;
    }
    return static_cast<size_t>(found - KEY_ACTIONS.begin());
}

char key_character(size_t index, bool upper, bool symbols)
{
    const auto &rows = symbols ? SYMBOL_ROWS : LOWER_ROWS;
    for (const auto row : rows) {
        if (index < row.size()) {
            char value = row[index];
            if (upper && value >= 'a' && value <= 'z') {
                value = static_cast<char>(value - 'a' + 'A');
            }
            return value;
        }
        index -= row.size();
    }
    return '\0';
}

bool is_printable_ascii(std::string_view text)
{
    return std::all_of(text.begin(), text.end(), [](unsigned char ch) {
        return ch >= 0x20 && ch <= 0x7e;
    });
}

} // namespace

std::expected<void, std::string> GspKeyboard::show(
    esp_gsp_handle_t handle,
    system::core::AppId app_id,
    system::core::KeyboardRequestId request_id,
    const system::core::KeyboardRequestOptions &options,
    system::core::KeyboardResultHandler completion
)
{
    if (active()) {
        return std::unexpected("GSP keyboard is already active");
    }
    if (handle == nullptr || app_id == system::core::INVALID_APP_ID ||
            request_id == system::core::INVALID_KEYBOARD_REQUEST_ID || !completion) {
        return std::unexpected("GSP keyboard request is not valid");
    }
    if (!options.password || (options.mode != "text" && options.mode != "upper" && options.mode != "special")) {
        return std::unexpected("GSP keyboard supports password text, upper, and special modes");
    }
    if (options.max_length < 0 || options.max_length > static_cast<int32_t>(MAX_PASSWORD_BYTES)) {
        return std::unexpected("GSP password limit must be between 0 and 64 bytes");
    }
    const size_t max_length = options.max_length == 0
                              ? MAX_PASSWORD_BYTES : static_cast<size_t>(options.max_length);
    if (options.initial_text.size() > max_length || !is_printable_ascii(options.initial_text)) {
        return std::unexpected("GSP password initial text must be printable ASCII within the limit");
    }
    for (size_t index = 0; index < options.allowed_modes.size(); ++index) {
        const auto &mode = options.allowed_modes[index];
        if (mode != "text" && mode != "upper" && mode != "number" && mode != "special") {
            return std::unexpected("GSP keyboard allowed mode is unsupported");
        }
        if (std::find(options.allowed_modes.begin(), options.allowed_modes.begin() + index, mode) !=
                options.allowed_modes.begin() + index) {
            return std::unexpected("GSP keyboard allowed modes contain duplicates");
        }
    }
    if (!options.allowed_modes.empty() &&
            std::find(options.allowed_modes.begin(), options.allowed_modes.end(), options.mode) ==
            options.allowed_modes.end()) {
        return std::unexpected("GSP keyboard initial mode is not allowed");
    }

    app_id_ = app_id;
    request_id_ = request_id;
    completion_ = std::move(completion);
    text_ = options.initial_text;
    placeholder_ = options.placeholder.empty() ? "Enter password" : options.placeholder;
    max_length_ = max_length;
    layout_ = options.mode == "upper" ? Layout::Upper :
              options.mode == "special" ? Layout::Symbols : Layout::Lower;
    allow_lower_ = options.allowed_modes.empty() ||
                   std::find(options.allowed_modes.begin(), options.allowed_modes.end(), "text") !=
                   options.allowed_modes.end();
    allow_upper_ = options.allowed_modes.empty() ||
                   std::find(options.allowed_modes.begin(), options.allowed_modes.end(), "upper") !=
                   options.allowed_modes.end();
    allow_symbols_ = options.allowed_modes.empty() ||
                     std::find(options.allowed_modes.begin(), options.allowed_modes.end(), "special") !=
                     options.allowed_modes.end();

    const auto shown = esp_gsp_goto_scene(handle, GSP_SETTINGS_SCENE_SETTINGS_KEYBOARD, ESP_GSP_NO_TRANSITION);
    if (shown != ESP_GSP_OK) {
        clear();
        return std::unexpected("Failed to show GSP keyboard scene");
    }
    if (esp_gsp_flush(handle, 500) != ESP_GSP_OK) {
        clear();
        return std::unexpected("Failed to settle GSP keyboard scene");
    }
    const auto title = options.title.empty() ? "Wi-Fi password" : options.title;
    if (esp_gsp_component_set_text(handle, GSP_SETTINGS_KEYBOARD_OBJ_KEY_TITLE, title.c_str()) != ESP_GSP_OK) {
        clear();
        return std::unexpected("Failed to initialize GSP keyboard scene");
    }
    if (auto preview = refresh_preview(handle); !preview) {
        clear();
        return preview;
    }
    if (auto layout = refresh_layout(handle); !layout) {
        clear();
        return layout;
    }
    return {};
}

void GspKeyboard::hide(esp_gsp_handle_t handle, system::core::KeyboardRequestId request_id)
{
    (void)handle;
    if (active() && request_id_ == request_id) {
        clear();
    }
}

std::expected<bool, std::string> GspKeyboard::handle_action(
    esp_gsp_handle_t handle, unsigned scene, unsigned id, unsigned arg
)
{
    (void)arg;
    if (scene != GSP_SETTINGS_SCENE_SETTINGS_KEYBOARD) {
        return false;
    }
    // Swallow events from an already closed keyboard in the same GSP session.
    if (!active()) {
        return true;
    }
    if (id == GSP_SETTINGS_KEYBOARD_ACT_ID_CANCEL || id == GSP_SETTINGS_KEYBOARD_ACT_ID_CONFIRM) {
        finish(id == GSP_SETTINGS_KEYBOARD_ACT_ID_CONFIRM);
        return true;
    }
    if (handle == nullptr) {
        finish(false);
        return std::unexpected("GSP keyboard session is not running");
    }
    if (id == GSP_SETTINGS_KEYBOARD_ACT_ID_DELETE) {
        if (!text_.empty()) {
            text_.pop_back();
            if (auto result = refresh_preview(handle); !result) {
                const auto error = result.error();
                finish(false);
                return std::unexpected(error);
            }
        }
        return true;
    }
    if (id == GSP_SETTINGS_KEYBOARD_ACT_ID_SPACE) {
        if (text_.size() < max_length_) {
            text_.push_back(' ');
            if (auto result = refresh_preview(handle); !result) {
                const auto error = result.error();
                finish(false);
                return std::unexpected(error);
            }
        }
        return true;
    }
    if (id == GSP_SETTINGS_KEYBOARD_ACT_ID_SHIFT) {
        if (allow_lower_ && allow_upper_ && layout_ != Layout::Symbols) {
            layout_ = layout_ == Layout::Upper ? Layout::Lower : Layout::Upper;
            if (auto result = refresh_layout(handle); !result) {
                const auto error = result.error();
                finish(false);
                return std::unexpected(error);
            }
        }
        return true;
    }
    if (id == GSP_SETTINGS_KEYBOARD_ACT_ID_SYMBOLS) {
        if (allow_symbols_ && (allow_lower_ || allow_upper_)) {
            layout_ = layout_ == Layout::Symbols ?
                      (allow_lower_ ? Layout::Lower : Layout::Upper) : Layout::Symbols;
            if (auto result = refresh_layout(handle); !result) {
                const auto error = result.error();
                finish(false);
                return std::unexpected(error);
            }
        }
        return true;
    }
    const auto index = find_key(id);
    if (!index) {
        return true;
    }
    if (text_.size() >= max_length_) {
        return true;
    }
    const char value = key_character(*index, layout_ == Layout::Upper, layout_ == Layout::Symbols);
    if (value == '\0') {
        return std::unexpected("GSP keyboard key map is invalid");
    }
    text_.push_back(value);
    if (auto result = refresh_preview(handle); !result) {
        const auto error = result.error();
        finish(false);
        return std::unexpected(error);
    }
    return true;
}

bool GspKeyboard::active() const
{
    return request_id_ != system::core::INVALID_KEYBOARD_REQUEST_ID;
}

std::expected<void, std::string> GspKeyboard::refresh_preview(esp_gsp_handle_t handle)
{
    const auto preview = text_.empty() ? placeholder_ : std::string(text_.size(), '*');
    if (esp_gsp_component_set_text(handle, GSP_SETTINGS_KEYBOARD_OBJ_KEY_MASKED_PASSWORD, preview.c_str()) !=
            ESP_GSP_OK) {
        return std::unexpected("Failed to update GSP password mask");
    }
    const auto count = std::to_string(text_.size()) + " / " + std::to_string(max_length_);
    if (esp_gsp_component_set_text(handle, GSP_SETTINGS_KEYBOARD_OBJ_KEY_PASSWORD_COUNT, count.c_str()) !=
            ESP_GSP_OK) {
        return std::unexpected("Failed to update GSP password length");
    }
    return {};
}

std::expected<void, std::string> GspKeyboard::refresh_layout(esp_gsp_handle_t handle)
{
    for (size_t index = 0; index < KEY_OBJECTS.size(); ++index) {
        const char value = key_character(index, layout_ == Layout::Upper, layout_ == Layout::Symbols);
        if (value == '\0') {
            return std::unexpected("GSP keyboard key map is invalid");
        }
        const char label[] = {value, '\0'};
        if (esp_gsp_component_set_text(handle, KEY_OBJECTS[index], label) != ESP_GSP_OK) {
            return std::unexpected("Failed to update GSP keyboard keys");
        }
    }
    const char *shift = layout_ == Layout::Upper ? "Lower" :
                        layout_ == Layout::Symbols ? "Letters" : "Shift";
    const char *symbols = layout_ == Layout::Symbols ? "ABC" : "Symbols";
    if (esp_gsp_component_set_text(handle, GSP_SETTINGS_KEYBOARD_OBJ_KEY_SHIFT, shift) != ESP_GSP_OK ||
            esp_gsp_component_set_text(handle, GSP_SETTINGS_KEYBOARD_OBJ_KEY_SYMBOLS, symbols) != ESP_GSP_OK ||
            esp_gsp_component_set_enabled(handle, GSP_SETTINGS_KEYBOARD_OBJ_KEY_SHIFT,
                                          allow_lower_ && allow_upper_ && layout_ != Layout::Symbols) != ESP_GSP_OK ||
            esp_gsp_component_set_enabled(handle, GSP_SETTINGS_KEYBOARD_OBJ_KEY_SYMBOLS,
                                          allow_symbols_ && (allow_lower_ || allow_upper_)) != ESP_GSP_OK) {
        return std::unexpected("Failed to update GSP keyboard modes");
    }
    return {};
}

void GspKeyboard::finish(bool confirmed)
{
    const auto app_id = app_id_;
    const auto request_id = request_id_;
    auto completion = std::move(completion_);
    system::core::KeyboardResult result{
        .request_id = request_id,
        .app_id = app_id,
        .confirmed = confirmed,
        .text = confirmed ? std::move(text_) : std::string(),
    };
    clear();
    if (completion) {
        completion(result);
    }
    std::fill(result.text.begin(), result.text.end(), '\0');
}

void GspKeyboard::clear()
{
    std::fill(text_.begin(), text_.end(), '\0');
    text_.clear();
    placeholder_.clear();
    completion_ = {};
    app_id_ = system::core::INVALID_APP_ID;
    request_id_ = system::core::INVALID_KEYBOARD_REQUEST_ID;
    max_length_ = MAX_PASSWORD_BYTES;
    layout_ = Layout::Lower;
    allow_lower_ = true;
    allow_upper_ = true;
    allow_symbols_ = true;
}

} // namespace esp_brookesia::app::settings

#endif // CONFIG_BROOKESIA_GUI_BACKEND_GSP
