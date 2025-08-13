/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "esp_timer.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include <atomic>
#include <string>
#include <ctime>

namespace esp_brookesia::apps {

typedef enum {
    TIMER_SCREEN_DIGITAL,
    TIMER_SCREEN_ANALOG,
    TIMER_SCREEN_MAX,
} timer_screen_t;

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t weekday;
} system_time_t;

class Timer: public systems::speaker::App {
public:
    /**
     * @brief Get the singleton instance of Timer
     *
     * @return Pointer to the singleton instance
     */
    static Timer *requestInstance();

    /**
     * @brief Destructor for the timer app
     *
     */
    ~Timer();

    bool run(void) override;
    bool back(void) override;
    bool close(void) override;
    bool init(void) override;
    bool deinit(void) override;

    using App::startRecordResource;
    using App::endRecordResource;

protected:
    /**
     * @brief Private constructor to enforce singleton pattern
     *
     */
    Timer();

private:
    static void timer_event_cb(lv_event_t *e);
    static void clock_tick_callback(void *arg);
    static void toast_timer_callback(void *arg);

    void setupClockControls();
    void manageClockTimer();
    void updateTimeDisplay();
    void updateDateDisplay();
    void updateAnalogClock();
    void switchScreen();
    void showToast(const char *message, uint32_t duration_ms = 3000);
    void hideToast();
    void createTimerWithCallback(esp_timer_handle_t *timer, esp_timer_cb_t callback, const char *name);
    const char *const *getMonthNames();
    const char *const *getWeekdayNames();

    void getSystemTime();

    // Member variables
    lv_obj_t *main_container;
    timer_screen_t current_screen;
    system_time_t current_time;
    uint16_t _height = 400;
    uint16_t _width = 400;

    std::atomic<bool> _is_starting = false;
    std::atomic<bool> _is_stopping = false;
    static Timer *_instance;

    esp_timer_handle_t _clock_timer = nullptr;
    esp_timer_handle_t _toast_timer = nullptr;

    lv_obj_t *_toast_container = nullptr;
    lv_obj_t *_toast_label = nullptr;
};

} // namespace esp_brookesia::apps
