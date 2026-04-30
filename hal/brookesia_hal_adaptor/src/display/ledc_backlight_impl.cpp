/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_DISPLAY_LEDC_BACKLIGHT_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "esp_board_device.h"
#include "ledc_backlight_impl.hpp"

#if BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LEDC_BACKLIGHT_IMPL
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_board_manager_includes.h"

namespace esp_brookesia::hal {

constexpr uint8_t BRIGHTNESS_DEFAULT = 0;
constexpr uint8_t BRIGHTNESS_MIN = 0;
constexpr uint8_t BRIGHTNESS_MAX = 100;
constexpr bool BACKLIGHT_CONTROL_STATE_DEFAULT = false;

namespace {
periph_ledc_handle_t *get_ledc_handle(void *handle)
{
    return reinterpret_cast<periph_ledc_handle_t *>(handle);
}

periph_gpio_handle_t *get_gpio_handle(void *handle)
{
    return reinterpret_cast<periph_gpio_handle_t *>(handle);
}

periph_ledc_config_t *get_ledc_config(void *config)
{
    return reinterpret_cast<periph_ledc_config_t *>(config);
}
} // namespace

LedcDisplayBacklightImpl::LedcDisplayBacklightImpl()
    : DisplayBacklightIface()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_EXIT(setup_ledc(), "Failed to setup LEDC");
    BROOKESIA_CHECK_FALSE_EXIT(setup_backlight_control(), "Failed to setup backlight control");
}

LedcDisplayBacklightImpl::~LedcDisplayBacklightImpl()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (is_ledc_valid_internal()) {
        auto ret = esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, { BROOKESIA_LOGE("Failed to deinit LEDC"); });
    }
    if (is_backlight_control_valid_internal()) {
        auto ret = esp_board_periph_deinit(ESP_BOARD_PERIPH_NAME_GPIO_BACKLIGHT_CONTROL);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, { BROOKESIA_LOGE("Failed to deinit backlight control GPIO"); });
    }
}

bool LedcDisplayBacklightImpl::set_brightness(uint8_t percent)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: percent(%1%)", percent);

    boost::lock_guard<boost::mutex> lock(mutex_);

    return set_brightness_internal(percent, false);
}

bool LedcDisplayBacklightImpl::get_brightness(uint8_t &percent)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(is_ledc_valid_internal(), false, "LEDC is not initialized");

    percent = ledc_brightness_;

    return true;
}

bool LedcDisplayBacklightImpl::set_light_on_off(bool on)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: on(%1%)", on);

    boost::lock_guard<boost::mutex> lock(mutex_);

    return light_on_off_internal(on, false);
}

bool LedcDisplayBacklightImpl::setup_ledc()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to init LEDC");

    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS, &ledc_dev_handle_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get LEDC handle");
    BROOKESIA_CHECK_NULL_RETURN(ledc_dev_handle_, false, "Failed to get LEDC handle");

    dev_ledc_ctrl_config_t *dev_cfg = nullptr;
    ret = esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS, reinterpret_cast<void **>(&dev_cfg));
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get LEDC config");

    ret = esp_board_periph_get_config(dev_cfg->ledc_name, (void **)&ledc_periph_config_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get LEDC periph config");

    // Force set default brightness
    BROOKESIA_CHECK_FALSE_RETURN(
        set_brightness_internal(BRIGHTNESS_DEFAULT, true), false, "Failed to set default brightness"
    );

    return true;
}

bool LedcDisplayBacklightImpl::setup_backlight_control()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto ret = esp_board_manager_get_periph_handle(ESP_BOARD_PERIPH_NAME_GPIO_BACKLIGHT_CONTROL, &backlight_control_handle_);
    if (ret != ESP_OK) {
        BROOKESIA_LOGW("Backlight control GPIO not found, skip");
        return true;
    }

    BROOKESIA_CHECK_NULL_RETURN(backlight_control_handle_, false, "Failed to get backlight control GPIO handle");

    periph_gpio_config_t *config = nullptr;
    ret = esp_board_manager_get_periph_config(
              ESP_BOARD_PERIPH_NAME_GPIO_BACKLIGHT_CONTROL, reinterpret_cast<void **>(&config)
          );
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get backlight control GPIO config");
    BROOKESIA_CHECK_NULL_RETURN(config, false, "Failed to get backlight control GPIO config");

    backlight_control_active_level_ = config->default_level;

    // Force turn off backlight
    BROOKESIA_CHECK_FALSE_RETURN(
        light_on_off_internal(BACKLIGHT_CONTROL_STATE_DEFAULT, true), false, "Failed to turn off backlight"
    );

    return true;
}

bool LedcDisplayBacklightImpl::set_brightness_internal(uint8_t percent, bool force)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: percent(%1%), force(%2%)", percent, force);

    BROOKESIA_CHECK_FALSE_RETURN(is_ledc_valid_internal(), false, "LEDC is not initialized");

    // Clamp target brightness to [BRIGHTNESS_MIN, BRIGHTNESS_MAX]
    auto percent_clamped = std::clamp<uint8_t>(percent, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
    if (percent_clamped != percent) {
        BROOKESIA_LOGW(
            "Target brightness(%1%) is out of range [%2%, %3%], clamp to %4%", percent, BRIGHTNESS_MIN, BRIGHTNESS_MAX,
            percent_clamped
        );
    }

    if (!force && (percent_clamped == ledc_brightness_)) {
        BROOKESIA_LOGD("Brightness is already %1%, skip", percent_clamped);
        return true;
    }

    auto *ledc_handle = get_ledc_handle(ledc_dev_handle_);
    auto *ledc_config = get_ledc_config(ledc_periph_config_);
    uint32_t duty = (percent_clamped * ((1 << static_cast<uint32_t>(ledc_config->duty_resolution)) - 1)) / 100;
    {
        auto result = ledc_set_duty(ledc_handle->speed_mode, ledc_handle->channel, duty);
        BROOKESIA_CHECK_ESP_ERR_RETURN(result, false, "Failed to set LEDC duty");
    }
    {
        auto result = ledc_update_duty(ledc_handle->speed_mode, ledc_handle->channel);
        BROOKESIA_CHECK_ESP_ERR_RETURN(result, false, "Failed to update LEDC duty");
    }

    ledc_brightness_ = percent_clamped;

    BROOKESIA_LOGI("Set brightness: %1%", percent_clamped);

    return true;
}

bool LedcDisplayBacklightImpl::light_on_off_internal(bool on, bool force)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: on(%1%), force(%2%)", on, force);

    BROOKESIA_CHECK_FALSE_RETURN(is_backlight_control_valid_internal(), false, "Backlight control is not valid");

    if (!force && (on == is_light_on_)) {
        BROOKESIA_LOGD("Backlight is already %1%, skip", on ? "on" : "off");
        return true;
    }

    auto ret = gpio_set_level(
                   get_gpio_handle(backlight_control_handle_)->gpio_num,
                   on ? backlight_control_active_level_ : !backlight_control_active_level_
               );
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to set backlight control GPIO level");

    is_light_on_ = on;

    BROOKESIA_LOGI("Set backlight state: %1%", is_light_on_ ? "on" : "off");

    return true;
}

} // namespace esp_brookesia::hal
#endif // BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LEDC_BACKLIGHT_IMPL
