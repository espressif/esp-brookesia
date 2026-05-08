/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include "brookesia/hal_custom/macro_configs.h"
#if !BROOKESIA_HAL_CUSTOM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "esp_lcd_panel_ops.h"
#include "esp_board_device.h"
#include "esp_board_manager_includes.h"
#include "backlight_impl.hpp"

#define LCD_OPCODE_WRITE_CMD (0x02ULL)

namespace esp_brookesia::hal {

constexpr uint8_t BRIGHTNESS_DEFAULT = 0;
constexpr uint8_t BRIGHTNESS_MIN = 0;
constexpr uint8_t BRIGHTNESS_MAX = 100;

namespace {
esp_lcd_panel_io_handle_t get_io_handle(void *handles)
{
    return reinterpret_cast<dev_display_lcd_handles_t *>(handles)->io_handle;
}
} // namespace

CustomDisplayBacklightImpl::CustomDisplayBacklightImpl()
    : DisplayBacklightIface()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    auto ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    BROOKESIA_CHECK_ESP_ERR_EXIT(ret, "Failed to init display LCD");

    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, &handles_);
    BROOKESIA_CHECK_ESP_ERR_EXIT(ret, "Failed to get handles");
    BROOKESIA_CHECK_NULL_EXIT(handles_, "Failed to get handles");

    BROOKESIA_CHECK_FALSE_EXIT(set_brightness_internal(BRIGHTNESS_DEFAULT, true), "Failed to set default brightness");
}

CustomDisplayBacklightImpl::~CustomDisplayBacklightImpl()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
}

bool CustomDisplayBacklightImpl::set_brightness(uint8_t percent)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: percent(%1%)", percent);

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
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: percent(%1%), force(%2%)", percent, force);

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "LCD brightness is not initialized");

    auto percent_clamped = std::clamp<uint8_t>(percent, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
    if (percent_clamped != percent) {
        BROOKESIA_LOGW(
            "Target brightness(%1%) is out of range [%2%, %3%], clamp to %4%", percent, BRIGHTNESS_MIN, BRIGHTNESS_MAX,
            percent_clamped
        );
    }

    if (!force && (percent_clamped == brightness_)) {
        BROOKESIA_LOGD("Brightness is already %1%, skip", percent_clamped);
        return true;
    }

    uint8_t data[1] = {static_cast<uint8_t>((255 * percent_clamped) / 100)};
    int lcd_cmd = 0x51;
    lcd_cmd &= 0xff;
    lcd_cmd <<= 8;
    lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
    auto ret = esp_lcd_panel_io_tx_param(get_io_handle(handles_), lcd_cmd, data, sizeof(data));
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to transmit brightness command");

    brightness_ = percent_clamped;

    BROOKESIA_LOGI("Set brightness: %1%", brightness_);

    return true;
}

} // namespace esp_brookesia::hal
