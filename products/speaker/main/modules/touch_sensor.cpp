/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_lib_utils.h"
#include "bsp/esp-bsp.h"
#include "touch_button.h"
#include "iot_button.h"
#include "touch_sensor.h"
#include "touch_sensor_lowlevel.h"
extern "C" { // temporary solution for eliminate compilation errors
#include "touch_slider_sensor.h"
}
#include "esp_brookesia_app_settings.hpp"

const static char *TAG = "Touch Sensor";

#define TOUCH_SLIDER_ENABLED 0 // Disable touch slider for now

static const uint32_t touch_channel_list[] = { // define touch channels
#ifdef BSP_TOUCH_PAD1
    BSP_TOUCH_PAD1,
#endif
#ifdef BSP_TOUCH_PAD2
    BSP_TOUCH_PAD2,
#endif
};

// Touch button handles for multi-tap gestures
static button_handle_t touch_btn_handle[2] = {NULL, NULL};

static esp_err_t init_touch_button(void)
{
    touch_lowlevel_type_t channel_type[] = {TOUCH_LOWLEVEL_TYPE_TOUCH, TOUCH_LOWLEVEL_TYPE_TOUCH};
    uint32_t channel_num = sizeof(touch_channel_list) / sizeof(touch_channel_list[0]);
    ESP_LOGI(TAG, "touch channel num: %ld\n", channel_num);
    touch_lowlevel_config_t low_config = {
        .channel_num = channel_num,
        .channel_list = (uint32_t *)touch_channel_list,
        .channel_type = channel_type,
    };

    esp_err_t ret = touch_sensor_lowlevel_create(&low_config);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to create touch sensor lowlevel");

    // Touch button configuration (shared by both buttons)
    const button_config_t btn_cfg = {
        .long_press_time = 1500,  // Long press time in ms
        .short_press_time = 245,  // Short press time in ms
    };

    for (size_t i = 0; i < channel_num; i++) {
        button_touch_config_t touch_cfg_1 = {
            .touch_channel = (int32_t)touch_channel_list[i],
            .channel_threshold = 0.05, // Touch threshold (adjust as needed)
            .skip_lowlevel_init = true,
        };
        ESP_LOGI(TAG, "Touch button %d channel: %d", i + 1, touch_channel_list[i]);
        // Create first touch button device
        ret = iot_button_new_touch_button_device(&btn_cfg, &touch_cfg_1, &touch_btn_handle[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create touch button 1 device: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    touch_sensor_lowlevel_start();
    ESP_LOGI(TAG, "touch button initialized");
    return ESP_OK;
}


#if TOUCH_SLIDER_ENABLED
static int current_volume = 50;  // Default volume (0-100)
// Touch gesture coordination variables (for shared channels 6&7)
static bool is_sliding_detected = false;           // True when sliding is detected
static touch_slider_handle_t touch_slider_handle = NULL;

static void touch_slider_callback(touch_slider_handle_t handle, touch_slider_event_t event, int32_t data, void *cb_arg)
{
    const char *TAG = "TOUCH_VOLUME";
    uint32_t current_time = esp_timer_get_time() / 1000;

    switch (event) {
    // We ignore POSITION events to avoid conflicts with button system
    case TOUCH_SLIDER_EVENT_POSITION:
        ESP_LOGD(TAG, "Position event ignored to prevent button conflicts");
        break;

    case TOUCH_SLIDER_EVENT_RIGHT_SWIPE:
        // Swipe events indicate definite sliding
        if (!is_sliding_detected) {
            is_sliding_detected = true;
            ESP_LOGI(TAG, "Swipe detected, taking control from buttons");
        }

        ESP_LOGI(TAG, "Right swipe - Volume up");
        current_volume += 10;  // Smaller increment for finer control
        if (current_volume > 100) {
            current_volume = 100;
        }
        // bsp_extra_codec_volume_set(current_volume, NULL);


        break;

    case TOUCH_SLIDER_EVENT_LEFT_SWIPE:
        // Swipe events indicate definite sliding
        if (!is_sliding_detected) {
            is_sliding_detected = true;
            ESP_LOGI(TAG, "Swipe detected, taking control from buttons");
        }

        ESP_LOGI(TAG, "Left swipe - Volume down");
        current_volume -= 10;  // Smaller decrement for finer control
        if (current_volume < 0) {
            current_volume = 0;
        }

        break;

    case TOUCH_SLIDER_EVENT_RELEASE:
        ESP_LOGI(TAG, "Touch released, sliding_detected: %s",
                 is_sliding_detected ? "YES" : "NO");

        // If sliding was detected, show final volume
        if (is_sliding_detected) {
            ESP_LOGI(TAG, "Final volume: %d%%", current_volume);
        } else {
            // No sliding detected - let button system handle this as a tap
            ESP_LOGI(TAG, "No sliding detected, button system will handle this touch");
        }

        // Reset gesture state
        is_sliding_detected = false;
        break;

    default:
        break;
    }
}

// Task function to handle touch slider events
static void touch_slider_task(void *pvParameters)
{
    touch_slider_handle_t handle = (touch_slider_handle_t)pvParameters;
    ESP_LOGI(TAG, "Touch volume control task started");
    while (1) {
        if (touch_slider_sensor_handle_events(handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to handle touch slider events");
        }
        vTaskDelay(pdMS_TO_TICKS(20));  // 20ms polling interval
    }
}

// Initialize touch volume control
static esp_err_t init_touch_slider(void)
{
    // Touch slider configuration - sharing channels 6&7 with button system
    float threshold[] = {0.015f, 0.015f};  // Touch thresholds for each channel
    uint32_t channel_num = sizeof(touch_channel_list) / sizeof(touch_channel_list[0]);

    // Configure touch slider for swipe-only volume control
    touch_slider_config_t config = {
        .channel_num = channel_num,
        .channel_list = touch_channel_list,
        .channel_threshold = threshold,
        .channel_gold_value = NULL,
        .debounce_times = 1,            // Reduced debounce for faster response
        .filter_reset_times = 2,        // Reduced for faster response
        .position_range = 100,          // Simple volume range 0-100
        .calculate_window = 2,
        .swipe_threshold = 4.0f,       // Lower threshold for easier swipe detection
        .swipe_hysterisis = 2.0f,      // Lower hysteresis for better responsiveness
        .swipe_alpha = 0.3f,            // Slightly less smoothing for more responsive swipes
        .skip_lowlevel_init = true,     // Use existing lowlevel init from touch buttons
    };

    // Create touch slider sensor
    esp_err_t ret = touch_slider_sensor_create(&config, &touch_slider_handle, touch_slider_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create touch slider sensor: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create task to handle touch events
    BaseType_t task_ret = xTaskCreate(touch_slider_task, "touchslider_task", 4096, touch_slider_handle, 5, NULL);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create touch volume task");
        touch_slider_sensor_delete(touch_slider_handle);
        touch_slider_handle = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Touch slider initialized successfully");
    return ESP_OK;
}

#endif

TouchSensor::TouchSensor()
{

}

TouchSensor::~TouchSensor()
{

}

bool TouchSensor::init()
{
    ESP_UTILS_CHECK_FALSE_RETURN(ESP_OK == init_touch_button(), false, "Failed to init touch button");
#if TOUCH_SLIDER_ENABLED
    ESP_UTILS_CHECK_FALSE_RETURN(ESP_OK == init_touch_slider(), false, "Failed to init touch slider");
#endif
    return true;
}

button_handle_t TouchSensor::get_button_handle()
{
    return touch_btn_handle[0];
}
