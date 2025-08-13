/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <math.h>
#include <string>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_sntp.h"
#include "esp_brookesia_app_timer.hpp"
#include "ui/ui.h"
#include "esp_lib_utils.h"

#define APP_NAME "Timer"

using namespace std;
using namespace esp_brookesia::systems::speaker;

LV_IMG_DECLARE(img_app_timer);

namespace esp_brookesia::apps {

Timer *Timer::_instance = nullptr;

Timer *Timer::requestInstance()
{
    if (_instance == nullptr) {
        _instance = new Timer();
    }
    return _instance;
}

Timer::Timer():
    App( {
    .name = APP_NAME,
    .launcher_icon = gui::StyleImage::IMAGE(&img_app_timer),
    .screen_size = gui::StyleSize::RECT_PERCENT(100, 100),
    .flags = {
        .enable_default_screen = 0,
        .enable_recycle_resource = 1,
        .enable_resize_visual_area = 1,
    },
},
{
    .app_launcher_page_index = 0,
    .flags = {
        .enable_navigation_gesture = 1,
    },
}
   ),
   main_container(nullptr),
   current_screen(TIMER_SCREEN_DIGITAL),
current_time{0} {
}

Timer::~Timer()
{


    if (_clock_timer) {
        esp_timer_stop(_clock_timer);
        esp_timer_delete(_clock_timer);
        _clock_timer = nullptr;
    }

    if (_toast_timer) {
        esp_timer_stop(_toast_timer);
        esp_timer_delete(_toast_timer);
        _toast_timer = nullptr;
    }

    _instance = nullptr;
}

bool Timer::init(void)
{
    return true;
}

bool Timer::deinit(void)
{
    return true;
}

bool Timer::run(void)
{
    _is_starting = true;
    current_screen = TIMER_SCREEN_DIGITAL;

    // Create both screens at initialization
    ui_Screen_watch_digital_screen_init();
    ui_Screen_watch_analog_screen_init();

    // Add event callbacks to both screens
    if (ui_Screen_watch_digital) {
        lv_obj_add_event_cb(ui_Screen_watch_digital, timer_event_cb, LV_EVENT_CLICKED, this);
    }
    if (ui_Screen_watch_analog) {
        lv_obj_add_event_cb(ui_Screen_watch_analog, timer_event_cb, LV_EVENT_CLICKED, this);
    }

    // Load the initial screen
    main_container = ui_Screen_watch_digital;
    lv_scr_load(ui_Screen_watch_digital);

    getSystemTime();
    updateTimeDisplay();
    updateDateDisplay();
    setupClockControls();

    _is_starting = false;
    return true;
}

bool Timer::back(void)
{
    ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
    return true;
}

bool Timer::close(void)
{
    _is_stopping = true;

    if (_clock_timer) {
        esp_timer_stop(_clock_timer);
        esp_timer_delete(_clock_timer);
        _clock_timer = nullptr;
    }

    // No need to manually clean screens due to enable_recycle_resource
    main_container = nullptr;

    _is_stopping = false;
    return true;
}



void Timer::createTimerWithCallback(esp_timer_handle_t *timer, esp_timer_cb_t callback, const char *name)
{
    if (!*timer) {
        esp_timer_create_args_t timer_args = {};
        timer_args.callback = callback;
        timer_args.arg = this;
        timer_args.name = name;

        esp_err_t ret = esp_timer_create(&timer_args, timer);
        if (ret != ESP_OK) {
            ESP_UTILS_LOGE("Timer create failed (%s): %s", name, esp_err_to_name(ret));
        }
    }
}

void Timer::setupClockControls()
{
    createTimerWithCallback(&_clock_timer, clock_tick_callback, "clock_tick");
    manageClockTimer();
}

void Timer::manageClockTimer()
{
    if (!_clock_timer || _is_stopping) {
        return;
    }

    // Start clock timer for both digital and analog screens
    esp_timer_start_periodic(_clock_timer, 1000000);
}



const char *const *Timer::getMonthNames()
{
    static const char *const month_names[] = {
        "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
    };
    return month_names;
}

const char *const *Timer::getWeekdayNames()
{
    static const char *const weekday_names[] = {
        "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
    };
    return weekday_names;
}

void Timer::getSystemTime()
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    current_time.hour = timeinfo.tm_hour;
    current_time.minute = timeinfo.tm_min;
    current_time.second = timeinfo.tm_sec;
    current_time.day = timeinfo.tm_mday;
    current_time.month = timeinfo.tm_mon + 1;
    current_time.year = timeinfo.tm_year + 1900;
    current_time.weekday = timeinfo.tm_wday;
}

void Timer::updateTimeDisplay()
{
    if (_is_stopping) {
        return;
    }

    getSystemTime();

    if (current_screen == TIMER_SCREEN_DIGITAL) {
        if (ui_watch_digital_Label_label_hour) {
            lv_label_set_text_fmt(ui_watch_digital_Label_label_hour, "%02d", current_time.hour);
        } else {
            ESP_UTILS_LOGE("Hour label is null");
        }

        if (ui_watch_digital_Label_label_min) {
            lv_label_set_text_fmt(ui_watch_digital_Label_label_min, "%02d", current_time.minute);
        } else {
            ESP_UTILS_LOGE("Minute label is null");
        }

        // Update date display every minute
        static uint8_t last_minute_digital = 0xFF;
        if (current_time.minute != last_minute_digital) {
            updateDateDisplay();
            last_minute_digital = current_time.minute;
        }
    } else if (current_screen == TIMER_SCREEN_ANALOG) {
        updateAnalogClock();

        if (ui_watch_analog_Label_clock) {
            lv_label_set_text_fmt(ui_watch_analog_Label_clock, "%02d : %02d",
                                  current_time.hour, current_time.minute);
        } else {
            ESP_UTILS_LOGE("Analog clock label is null");
        }

        // Update date display every minute
        static uint8_t last_minute_analog = 0xFF;
        if (current_time.minute != last_minute_analog) {
            updateDateDisplay();
            last_minute_analog = current_time.minute;
        }
    }
}

void Timer::updateDateDisplay()
{
    if (_is_stopping) {
        return;
    }

    const char *const *month_names = getMonthNames();
    const char *const *weekday_names = getWeekdayNames();

    if (current_screen == TIMER_SCREEN_DIGITAL) {
        if (ui_watch_digital_Label_day1) {
            lv_label_set_text_fmt(ui_watch_digital_Label_day1, "%s",
                                  weekday_names[current_time.weekday]);
        } else {
            ESP_UTILS_LOGE("Day label is null");
        }

        if (ui_watch_digital_Label_month1) {
            lv_label_set_text_fmt(ui_watch_digital_Label_month1, "%d  %s", current_time.day, month_names[current_time.month - 1]);
        } else {
            ESP_UTILS_LOGE("Month label is null");
        }

        if (ui_watch_digital_Label_year1) {
            lv_label_set_text_fmt(ui_watch_digital_Label_year1, "%d", current_time.year);
        } else {
            ESP_UTILS_LOGE("Year label is null");
        }
    } else if (current_screen == TIMER_SCREEN_ANALOG) {
        if (ui_watch_analog_Label_day2) {
            lv_label_set_text_fmt(ui_watch_analog_Label_day2, "%s",
                                  weekday_names[current_time.weekday]);
        }

        if (ui_watch_analog_Label_month2) {
            lv_label_set_text_fmt(ui_watch_analog_Label_month2, "%d %s", current_time.day, month_names[current_time.month - 1]);
        }

        if (ui_watch_analog_Label_year2) {
            lv_label_set_text_fmt(ui_watch_analog_Label_year2, "%d", current_time.year);
        }
    }
}

void Timer::updateAnalogClock()
{
    if (_is_stopping || current_screen != TIMER_SCREEN_ANALOG) {
        return;
    }

    int16_t hour_angle = (current_time.hour % 12) * 30;
    int16_t minute_angle = current_time.minute * 6;
    int16_t second_angle = current_time.second * 6;

    if (hour_angle < 0) {
        hour_angle += 360;
    }
    if (minute_angle < 0) {
        minute_angle += 360;
    }
    if (second_angle < 0) {
        second_angle += 360;
    }

    if (ui_watch_analog_Image_hour) {
        lv_img_set_angle(ui_watch_analog_Image_hour, hour_angle * 10);
    }

    if (ui_watch_analog_Image_min) {
        lv_img_set_angle(ui_watch_analog_Image_min, minute_angle * 10);
    }

    if (ui_watch_analog_Image_sec) {
        lv_img_set_angle(ui_watch_analog_Image_sec, second_angle * 10);
    }
}





void Timer::switchScreen()
{
    if (_is_stopping) {
        return;
    }

    // Switch to next screen
    current_screen = (timer_screen_t)((current_screen + 1) % TIMER_SCREEN_MAX);

    // Simply load the target screen without destroying anything
    if (current_screen == TIMER_SCREEN_DIGITAL) {
        main_container = ui_Screen_watch_digital;
        lv_scr_load(ui_Screen_watch_digital);
    } else if (current_screen == TIMER_SCREEN_ANALOG) {
        main_container = ui_Screen_watch_analog;
        lv_scr_load(ui_Screen_watch_analog);
    }

    // Update display content
    updateTimeDisplay();
    updateDateDisplay();
}



void Timer::timer_event_cb(lv_event_t *e)
{
    Timer *timer = (Timer *)lv_event_get_user_data(e);
    if (!timer || timer->_is_stopping) {
        return;
    }

    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_CLICKED) {
        timer->switchScreen();
    }
}



void Timer::clock_tick_callback(void *arg)
{
    Timer *timer = static_cast<Timer *>(arg);
    if (!timer || timer->_is_stopping) {
        return;
    }

    // Update clock display for both digital and analog screens
    lv_async_call([](void *user_data) {
        Timer *t = static_cast<Timer *>(user_data);
        if (t && !t->_is_stopping) {
            t->updateTimeDisplay();
        }
    }, timer);
}

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, Timer, APP_NAME, []()
{
    return std::shared_ptr<Timer>(Timer::requestInstance(), [](Timer * p) {});
})

} // namespace esp_brookesia::apps::speaker
