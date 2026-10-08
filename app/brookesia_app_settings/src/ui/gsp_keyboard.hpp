/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef ESP_PLATFORM
#   include "sdkconfig.h"
#endif

#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "brookesia/system_core/app/types.hpp"
#include "esp_gsp.h"

namespace esp_brookesia::app::settings {

/**
 * @brief Password editor for the Settings GSP keyboard scene.
 *
 * The compiled scene receives only a masked preview. The owner keeps the
 * Settings GSP session alive and restores the connection scene on completion.
 * All methods run on the System app task after GSP events have been forwarded.
 */
class GspKeyboard {
public:
    void set_appearance(std::string_view locale, std::string_view theme);

    std::expected<void, std::string> show(
        esp_gsp_handle_t handle,
        system::core::AppId app_id,
        system::core::KeyboardRequestId request_id,
        const system::core::KeyboardRequestOptions &options,
        system::core::KeyboardResultHandler completion
    );
    void hide(esp_gsp_handle_t handle, system::core::KeyboardRequestId request_id);
    std::expected<bool, std::string> handle_action(
        esp_gsp_handle_t handle, unsigned scene, unsigned id, unsigned arg
    );
    bool active() const;

private:
    enum class Layout : uint8_t {
        Lower,
        Upper,
        Symbols,
    };

    std::expected<void, std::string> refresh_preview(esp_gsp_handle_t handle);
    std::expected<void, std::string> refresh_layout(esp_gsp_handle_t handle);
    void finish(bool confirmed);
    void clear();

    system::core::AppId app_id_ = system::core::INVALID_APP_ID;
    system::core::KeyboardRequestId request_id_ = system::core::INVALID_KEYBOARD_REQUEST_ID;
    system::core::KeyboardResultHandler completion_;
    std::string text_;
    std::string placeholder_;
    std::string locale_ = "en";
    std::string theme_ = "light";
    size_t max_length_ = 64;
    Layout layout_ = Layout::Lower;
    bool allow_lower_ = true;
    bool allow_upper_ = true;
    bool allow_symbols_ = true;
};

} // namespace esp_brookesia::app::settings

#endif // CONFIG_BROOKESIA_GUI_BACKEND_GSP
