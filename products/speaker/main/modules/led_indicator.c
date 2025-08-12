/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "esp_wifi.h"
#include "esp_event.h"
#include "bsp/esp-bsp.h"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "LedIndicator"
#include "esp_lib_utils.h"
#include "led_indicator.h"

led_indicator_handle_t led_indicator_handle = NULL;

static const blink_step_t led_indicator_low_power[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 200},
    {LED_BLINK_HOLD, LED_STATE_OFF, 200},
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t led_indicator_develop_mode[] = {
    {LED_BLINK_BREATHE, LED_STATE_ON, 1000},
    {LED_BLINK_BRIGHTNESS, LED_STATE_ON, 500},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 1000},
    {LED_BLINK_BRIGHTNESS, LED_STATE_OFF, 500},
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t led_indicator_touch_press_down[] = {
    {LED_BLINK_BRIGHTNESS, LED_STATE_25_PERCENT, 200},
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t led_indicator_wifi_disconnected[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 100},
    {LED_BLINK_HOLD, LED_STATE_OFF, 200},
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t led_indicator_wifi_connected[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 1000},
    {LED_BLINK_LOOP, 0, 0},
};

blink_step_t const *led_indicator_blink_lists[] = {
    [BLINK_LOW_POWER] = led_indicator_low_power,
    [BLINK_DEVELOP_MODE] = led_indicator_develop_mode,
    [BLINK_TOUCH_PRESS_DOWN] = led_indicator_touch_press_down,
    [BLINK_WIFI_CONNECTED] = led_indicator_wifi_connected,
    [BLINK_WIFI_DISCONNECTED] = led_indicator_wifi_disconnected,
    [BLINK_MAX] = NULL,
};


static void wifi_update_led_indicator_state(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_UTILS_LOGI("Wifi disconnected, update led indicator");
        led_indicator_stop(led_indicator_handle, BLINK_WIFI_CONNECTED);
        led_indicator_start(led_indicator_handle, BLINK_WIFI_DISCONNECTED);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_UTILS_LOGI("Wifi connected, update led indicator");
        led_indicator_stop(led_indicator_handle, BLINK_WIFI_DISCONNECTED);
        led_indicator_start(led_indicator_handle, BLINK_WIFI_CONNECTED);
    }
}

bool led_indicator_init(void)
{
    led_indicator_ledc_config_t ledc_config = {
        .is_active_level_high = false,
        .timer_inited = false,
        .timer_num = LEDC_TIMER_1,
        .gpio_num = BSP_HEAD_LED,
        .channel = CONFIG_BSP_HEAD_LED_LEDC_CH,
    };

    const led_indicator_config_t config = {
        .blink_lists = led_indicator_blink_lists,
        .blink_list_num = BLINK_MAX,
    };

    ESP_UTILS_CHECK_ERROR_RETURN(led_indicator_new_ledc_device(&config, &ledc_config, &led_indicator_handle), false, "Failed to create led indicator device");
    ESP_UTILS_CHECK_ERROR_RETURN(led_indicator_start(led_indicator_handle, BLINK_WIFI_DISCONNECTED), false, "Failed to start led indicator");
    return true;
}

bool led_indicator_register_wifi_event(void)
{
    ESP_UTILS_CHECK_FALSE_RETURN(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_update_led_indicator_state, NULL) == ESP_OK, false, "Failed to register wifi event handler");
    ESP_UTILS_CHECK_FALSE_RETURN(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_update_led_indicator_state, NULL) == ESP_OK, false, "Failed to register wifi event handler");
    return true;
}
