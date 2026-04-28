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

namespace {
esp_lcd_panel_io_handle_t get_io_handle(void *handles)
{
    return reinterpret_cast<dev_display_lcd_handles_t *>(handles)->io_handle;
}

DisplayBacklightIface::Info generate_info()
{
    return DisplayBacklightIface::Info {
        .brightness_default = BROOKESIA_HAL_CUSTOM_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT,
        .brightness_min = BROOKESIA_HAL_CUSTOM_DISPLAY_BACKLIGHT_BRIGHTNESS_MIN,
        .brightness_max = BROOKESIA_HAL_CUSTOM_DISPLAY_BACKLIGHT_BRIGHTNESS_MAX,
    };
}
} // namespace

CustomDisplayBacklightImpl::CustomDisplayBacklightImpl(std::optional<DisplayBacklightIface::Info> info)
    : DisplayBacklightIface(info.has_value() ? info.value() : generate_info())
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: info(%1%)", get_info());

    auto ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    BROOKESIA_CHECK_ESP_ERR_EXIT(ret, "Failed to init LEDC backlight");

    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, &handles_);
    BROOKESIA_CHECK_ESP_ERR_EXIT(ret, "Failed to get handles");

    BROOKESIA_CHECK_FALSE_EXECUTE(turn_off(), {}, { BROOKESIA_LOGE("Failed to turn off backlight"); });
}

CustomDisplayBacklightImpl::~CustomDisplayBacklightImpl()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
}

bool CustomDisplayBacklightImpl::set_brightness(uint8_t percent)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    return set_brightness_internal(percent);
}

bool CustomDisplayBacklightImpl::get_brightness(uint8_t &percent)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    return get_brightness_internal(percent);
}

bool CustomDisplayBacklightImpl::turn_on()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "LCD brightness is not initialized");

    uint8_t current_brightness = 0;
    BROOKESIA_CHECK_FALSE_RETURN(get_brightness_internal(current_brightness), false, "Failed to get current brightness");

    if (current_brightness != 0) {
        BROOKESIA_LOGD("Backlight is not off, skip");
        return true;
    }

    // Restore pre-off brightness; fall back to default if it was zero
    auto restore_percent = (brightness_before_off_ != 0) ? brightness_before_off_ : get_info().brightness_default;

    BROOKESIA_CHECK_FALSE_RETURN(set_brightness_internal(restore_percent), false, "Failed to turn on backlight");

    BROOKESIA_LOGI("Turned on: %1%", restore_percent);

    brightness_before_off_ = 0;

    return true;
}

bool CustomDisplayBacklightImpl::turn_off()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "LCD brightness is not initialized");

    if (brightness_percent_ != std::numeric_limits<uint8_t>::max()) {
        brightness_before_off_ = brightness_percent_;
    }

    BROOKESIA_CHECK_FALSE_RETURN(set_brightness_internal(0, false), false, "Failed to turn off backlight");

    BROOKESIA_LOGI("Turned off");

    return true;
}

bool CustomDisplayBacklightImpl::set_brightness_internal(uint8_t percent, bool need_map)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: percent(%1%)", percent);

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "LCD brightness is not initialized");

    // Clamp external input to [0, 100]
    auto percent_clamped = std::clamp<uint8_t>(percent, 0, 100);
    if (percent_clamped != percent) {
        BROOKESIA_LOGW("Target brightness(%1%) is out of range[0, 100], clamp to %2%", percent, percent_clamped);
    }

    if (percent_clamped == brightness_percent_) {
        BROOKESIA_LOGD("Brightness not changed, skip");
        return true;
    }

    // Map external [0, 100] → hardware [brightness_min, brightness_max]
    uint8_t hardware_percent = percent_clamped;
    if (need_map) {
        const auto &info = get_info();
        hardware_percent = static_cast<uint8_t>(
                               info.brightness_min + static_cast<int>(percent_clamped) * (info.brightness_max - info.brightness_min) / 100
                           );
    }

    uint8_t data[1] = {((uint8_t)((255 * hardware_percent) / 100))};
    int lcd_cmd = 0x51;
    lcd_cmd &= 0xff;
    lcd_cmd <<= 8;
    lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
    auto ret = esp_lcd_panel_io_tx_param(get_io_handle(handles_), lcd_cmd, &data, sizeof(data));
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to transmit brightness command");

    brightness_percent_ = percent_clamped;

    BROOKESIA_LOGI("Set brightness: %1% (hardware: %2%)", brightness_percent_, hardware_percent);

    return true;
}

bool CustomDisplayBacklightImpl::get_brightness_internal(uint8_t &percent) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: percent(%1%)", percent);

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "LCD brightness is not initialized");

    percent = brightness_percent_;

    return true;
}

} // namespace esp_brookesia::hal
