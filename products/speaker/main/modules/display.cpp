/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "esp_lvgl_port_disp.h"
#include "esp_brookesia.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "Display"
#include "esp_lib_utils.h"
#include "display.hpp"

constexpr int  LVGL_TASK_PRIORITY        = 4;
constexpr int  LVGL_TASK_CORE_ID         = 1;
constexpr int  LVGL_TASK_STACK_SIZE      = 20 * 1024;
constexpr int  LVGL_TASK_MAX_SLEEP_MS    = 500;
constexpr int  LVGL_TASK_TIMER_PERIOD_MS = 5;
constexpr bool LVGL_TASK_STACK_CAPS_EXT  = true;
constexpr int  BRIGHTNESS_MIN            = 10;
constexpr int  BRIGHTNESS_MAX            = 100;
constexpr int  BRIGHTNESS_DEFAULT        = 100;

using namespace esp_brookesia::gui;
using namespace esp_brookesia::services;
using namespace esp_brookesia::systems::speaker;

static bool draw_bitmap_with_lock(lv_disp_t *disp, int x_start, int y_start, int x_end, int y_end, const void *data);
static bool clear_display(lv_disp_t *disp);

bool display_init(bool default_dummy_draw)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    static bool is_lvgl_dummy_draw = true;

    /* Initialize BSP */
    bsp_power_init(true);
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = {
            .task_priority = LVGL_TASK_PRIORITY,
            .task_stack = LVGL_TASK_STACK_SIZE,
            .task_affinity = LVGL_TASK_CORE_ID,
            .task_max_sleep_ms = LVGL_TASK_MAX_SLEEP_MS,
            .task_stack_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
            .timer_period_ms = LVGL_TASK_TIMER_PERIOD_MS,
        },
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES,
        .double_buffer = true,
        .flags = {
            .buff_spiram = false,
            .default_dummy_draw = default_dummy_draw, // Avoid white screen during initialization
        },
    };
    auto disp = bsp_display_start_with_config(&cfg);
    ESP_UTILS_CHECK_NULL_RETURN(disp, false, "Start display failed");
    if (default_dummy_draw) {
        ESP_UTILS_CHECK_FALSE_RETURN(clear_display(disp), false, "Clear display failed");
        vTaskDelay(pdMS_TO_TICKS(100)); // Avoid snow screen
    }
    bsp_display_backlight_on();

    /* Configure LVGL lock and unlock */
    LvLock::registerCallbacks([](int timeout_ms) {
        if (timeout_ms < 0) {
            timeout_ms = 0;
        } else if (timeout_ms == 0) {
            timeout_ms = 1;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(bsp_display_lock(timeout_ms), false, "Lock failed");

        return true;
    }, []() {
        bsp_display_unlock();

        return true;
    });

    /* Update display brightness when NVS brightness is updated */
    auto &storage_service = StorageNVS::requestInstance();
    storage_service.connectEventSignal([&](const StorageNVS::Event & event) {
        if ((event.operation != StorageNVS::Operation::UpdateNVS) || (event.key != Manager::SETTINGS_BRIGHTNESS)) {
            return;
        }

        ESP_UTILS_LOG_TRACE_GUARD();

        StorageNVS::Value value;
        ESP_UTILS_CHECK_FALSE_EXIT(
            storage_service.getLocalParam(Manager::SETTINGS_BRIGHTNESS, value), "Get NVS brightness failed"
        );

        auto brightness = std::clamp(std::get<int>(value), BRIGHTNESS_MIN, BRIGHTNESS_MAX);
        ESP_UTILS_LOGI("Set display brightness to %d", brightness);
        ESP_UTILS_CHECK_ERROR_EXIT(bsp_display_brightness_set(brightness), "Set display brightness failed");
    });

    /* Initialize display brightness */
    StorageNVS::Value brightness = BRIGHTNESS_DEFAULT;
    if (!storage_service.getLocalParam(Manager::SETTINGS_BRIGHTNESS, brightness)) {
        ESP_UTILS_LOGW("Brightness not found in NVS, set to default value(%d)", std::get<int>(brightness));
    }
    storage_service.setLocalParam(Manager::SETTINGS_BRIGHTNESS, brightness);

    /* Process animation player events */
    AnimPlayer::flush_ready_signal.connect(
        [ = ](int x_start, int y_start, int x_end, int y_end, const void *data, void *user_data
    ) {
        // ESP_UTILS_LOGD("Flush ready: %d, %d, %d, %d", x_start, y_start, x_end, y_end);

        if (is_lvgl_dummy_draw) {
            ESP_UTILS_CHECK_FALSE_EXIT(
                draw_bitmap_with_lock(disp, x_start, y_start, x_end, y_end, data), "Draw bitmap failed"
            );
        }

        auto player = static_cast<AnimPlayer *>(user_data);
        ESP_UTILS_CHECK_NULL_EXIT(player, "Get player failed");

        player->notifyFlushFinished();
    });
    AnimPlayer::animation_stop_signal.connect(
        [ = ](int x_start, int y_start, int x_end, int y_end, void *user_data
    ) {
        // ESP_UTILS_LOGD("Clear area: %d, %d, %d, %d", x_start, y_start, x_end, y_end);

        if (is_lvgl_dummy_draw) {
            std::vector<uint8_t> buffer((x_end - x_start) * (y_end - y_start) * 2, 0);
            ESP_UTILS_CHECK_FALSE_EXIT(
                draw_bitmap_with_lock(disp, x_start, y_start, x_end, y_end, buffer.data()), "Draw bitmap failed"
            );
        }
    });
    Display::on_dummy_draw_signal.connect([ = ](bool enable) {
        ESP_UTILS_LOGI("Dummy draw: %d", enable);

        ESP_UTILS_CHECK_ERROR_EXIT(lvgl_port_disp_take_trans_sem(disp, portMAX_DELAY), "Take trans sem failed");
        lvgl_port_disp_set_dummy_draw(disp, enable);
        lvgl_port_disp_give_trans_sem(disp, false);

        if (!enable) {
            LvLockGuard gui_guard;
            lv_obj_invalidate(lv_screen_active());
        } else {
            ESP_UTILS_CHECK_FALSE_EXIT(clear_display(disp), "Clear display failed");
        }

        is_lvgl_dummy_draw = enable;
    });

    return true;
}

static bool draw_bitmap_with_lock(lv_disp_t *disp, int x_start, int y_start, int x_end, int y_end, const void *data)
{
    // ESP_UTILS_LOG_TRACE_GUARD();

    static boost::mutex draw_mutex;

    ESP_UTILS_CHECK_NULL_RETURN(disp, false, "Invalid display");
    ESP_UTILS_CHECK_NULL_RETURN(data, false, "Invalid data");

    auto panel_handle = static_cast<esp_lcd_panel_handle_t>(lv_display_get_user_data(disp));
    ESP_UTILS_CHECK_NULL_RETURN(panel_handle, false, "Get panel handle failed");

    std::lock_guard<boost::mutex> lock(draw_mutex);

    lvgl_port_disp_take_trans_sem(disp, 0);
    ESP_UTILS_CHECK_ERROR_RETURN(
        esp_lcd_panel_draw_bitmap(panel_handle, x_start, y_start, x_end, y_end, data), false, "Draw bitmap failed"
    );

    // Wait for the last frame buffer to complete transmission
    ESP_UTILS_CHECK_ERROR_RETURN(lvgl_port_disp_take_trans_sem(disp, portMAX_DELAY), false, "Take trans sem failed");
    lvgl_port_disp_give_trans_sem(disp, false);

    return true;
}

static bool clear_display(lv_disp_t *disp)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    std::vector<uint8_t> buffer(BSP_LCD_H_RES * BSP_LCD_V_RES * 2, 0);
    ESP_UTILS_CHECK_FALSE_RETURN(
        draw_bitmap_with_lock(disp, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, buffer.data()), false, "Draw bitmap failed"
    );

    return true;
}
