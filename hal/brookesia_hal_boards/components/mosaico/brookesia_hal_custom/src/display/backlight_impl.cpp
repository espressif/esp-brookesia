/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include "brookesia/hal_custom/macro_configs.h"
#if !BROOKESIA_HAL_CUSTOM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "backlight_impl.hpp"
#include "brookesia/hal_adaptor/display/device.hpp"
#include "esp_board_device.h"
#include "esp_board_manager_includes.h"
#include "brookesia/hal_adaptor/board_manager.h"
#include "esp_lcd_co5300.h"

namespace esp_brookesia::hal {

namespace {

constexpr uint8_t BRIGHTNESS_DEFAULT = 100;
constexpr uint8_t BRIGHTNESS_MIN = 0;
constexpr uint8_t BRIGHTNESS_MAX = 100;

esp_lcd_panel_handle_t get_panel_handle(void *handles)
{
    return reinterpret_cast<dev_display_lcd_handles_t *>(handles)->panel_handle;
}

} // namespace

CustomDisplayBacklightImpl::CustomDisplayBacklightImpl()
    : display::BacklightIface(display::BacklightIface::Info{
    .group_id = DisplayDevice::LCD_GROUP_ID,
})
{
    esp_brookesia::hal::detail::LifecycleGuard lifecycle_guard;
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);
    auto ret = brookesia_hal_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    if (ret != ESP_OK) {
        BROOKESIA_LOGE("Failed to init display LCD: %1%", esp_err_to_name(ret));
        return;
    }
    display_ref_held_ = true;

    ret = brookesia_hal_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, &handles_);
    if (ret != ESP_OK || handles_ == nullptr) {
        BROOKESIA_LOGE("Failed to get display LCD handles");
        release_display_ref_internal();
        return;
    }
    if (!set_brightness_internal(BRIGHTNESS_DEFAULT, true)) {
        BROOKESIA_LOGE("Failed to set default brightness");
        release_display_ref_internal();
    }
}

CustomDisplayBacklightImpl::~CustomDisplayBacklightImpl()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);
    release_display_ref_internal();
}

bool CustomDisplayBacklightImpl::set_brightness(uint8_t percent)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);
    return set_brightness_internal(percent, false);
}

bool CustomDisplayBacklightImpl::get_brightness(uint8_t &percent)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);
    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "LCD brightness is not initialized");
    percent = brightness_;
    return true;
}

bool CustomDisplayBacklightImpl::set_brightness_internal(uint8_t percent, bool force)
{
    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "LCD brightness is not initialized");

    const uint8_t percent_clamped = std::clamp<uint8_t>(percent, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
    if (!force && percent_clamped == brightness_) {
        return true;
    }

    auto ret = esp_lcd_panel_co5300_set_brightness(get_panel_handle(handles_), percent_clamped);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to set CO5300 brightness");
    brightness_ = percent_clamped;
    return true;
}

void CustomDisplayBacklightImpl::release_display_ref_internal()
{
    handles_ = nullptr;
    if (!display_ref_held_) {
        return;
    }

    auto ret = brookesia_hal_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    if (ret != ESP_OK) {
        BROOKESIA_LOGW("Failed to release display LCD reference: %1%", esp_err_to_name(ret));
    }
    display_ref_held_ = false;
}

} // namespace esp_brookesia::hal
