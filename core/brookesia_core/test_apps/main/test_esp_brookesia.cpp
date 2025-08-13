/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"
#include "lvgl.h"
#include "esp_brookesia.hpp"

using namespace esp_brookesia;
using namespace esp_brookesia::systems::phone;

#define TEST_LVGL_RESOLUTION_WIDTH          CONFIG_TEST_LVGL_RESOLUTION_WIDTH
#define TEST_LVGL_RESOLUTION_HEIGHT         CONFIG_TEST_LVGL_RESOLUTION_HEIGHT
#define TEST_INSTALL_UNINSTALL_APP_TIMES    (10)

/* Try using a stylesheet that corresponds to the resolution */
#if (TEST_LVGL_RESOLUTION_WIDTH == 320) && (TEST_LVGL_RESOLUTION_HEIGHT == 240)
#define TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   STYLESHEET_320_240_DARK
#elif (TEST_LVGL_RESOLUTION_WIDTH == 320) && (TEST_LVGL_RESOLUTION_HEIGHT == 480)
#define TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   STYLESHEET_320_480_DARK
#elif (TEST_LVGL_RESOLUTION_WIDTH == 480) && (TEST_LVGL_RESOLUTION_HEIGHT == 480)
#define TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   STYLESHEET_480_480_DARK
#elif (TEST_LVGL_RESOLUTION_WIDTH == 720) && (TEST_LVGL_RESOLUTION_HEIGHT == 1280)
#define TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   STYLESHEET_720_1280_DARK
#elif (TEST_LVGL_RESOLUTION_WIDTH == 800) && (TEST_LVGL_RESOLUTION_HEIGHT == 480)
#define TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   STYLESHEET_800_480_DARK
#elif (TEST_LVGL_RESOLUTION_WIDTH == 800) && (TEST_LVGL_RESOLUTION_HEIGHT == 1280)
#define TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   STYLESHEET_800_1280_DARK
#elif (TEST_LVGL_RESOLUTION_WIDTH == 1024) && (TEST_LVGL_RESOLUTION_HEIGHT == 600)
#define TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   STYLESHEET_1024_600_DARK
#elif (TEST_LVGL_RESOLUTION_WIDTH == 1280) && (TEST_LVGL_RESOLUTION_HEIGHT == 800)
#define TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   STYLESHEET_1280_800_DARK
#endif

static const char *TAG = "test_esp_brookesia_phone";

static void test_lvgl_init(lv_display_t **disp_out, lv_indev_t **tp_out);
static void test_lvgl_deinit(lv_display_t *disp, lv_indev_t *indev);
static systems::phone::Phone *test_esp_brookesia_phone_init(lv_display_t *disp, lv_indev_t *tp, bool enable_begin);
static void test_esp_brookesia_phone_deinit(systems::phone::Phone *phone);

TEST_CASE("test esp-brookesia to begin and delete", "[esp-brookesia][phone][begin_del]")
{
    lv_display_t *disp = nullptr;
    lv_indev_t *tp = nullptr;
    systems::phone::Phone *phone = nullptr;

    test_lvgl_init(&disp, &tp);

    ESP_LOGI(TAG, "Initialize phone with no device");
    phone = test_esp_brookesia_phone_init(nullptr, nullptr, true);
    test_esp_brookesia_phone_deinit(phone);

    ESP_LOGI(TAG, "Initialize phone with only display device");
    phone = test_esp_brookesia_phone_init(disp, nullptr, true);
    test_esp_brookesia_phone_deinit(phone);

    ESP_LOGI(TAG, "Initialize phone with only touch device");
    phone = test_esp_brookesia_phone_init(nullptr, tp, true);
    test_esp_brookesia_phone_deinit(phone);

    ESP_LOGI(TAG, "Initialize phone with display and touch device");
    phone = test_esp_brookesia_phone_init(disp, tp, true);
    test_esp_brookesia_phone_deinit(phone);

    test_lvgl_deinit(disp, tp);
}

#ifdef TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET
TEST_CASE("test esp-brookesia to add stylesheet", "[esp-brookesia][phone][add_stylesheet]")
{
    lv_display_t *disp = nullptr;
    lv_indev_t *tp = nullptr;
    systems::phone::Phone *phone = nullptr;
    systems::phone::Stylesheet *phone_stylesheet = nullptr;

    test_lvgl_init(&disp, &tp);
    phone = test_esp_brookesia_phone_init(disp, tp, false);

    phone_stylesheet = new systems::phone::Stylesheet(TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET());
    TEST_ASSERT_TRUE_MESSAGE(phone->addStylesheet(phone_stylesheet), "Failed to add phone stylesheet");
    TEST_ASSERT_TRUE_MESSAGE(phone->activateStylesheet(phone_stylesheet), "Failed to active phone stylesheet");
    delete phone_stylesheet;
    TEST_ASSERT_TRUE_MESSAGE(phone->begin(), "Failed to begin phone");

    test_esp_brookesia_phone_deinit(phone);
    test_lvgl_deinit(disp, tp);
}
#endif

// TEST_CASE("test esp-brookesia to install and uninstall APPs", "[esp-brookesia][phone][install_uninstall_app]")
// {
//     lv_display_t *disp = nullptr;
//     lv_indev_t *tp = nullptr;
//     systems::phone::Phone *phone = nullptr;
//     int phone_app_simple_conf_0_id = -1;
//     int phone_app_simple_conf_1_id = -1;
//     int phone_app_complex_conf_0_id = -1;
//     int phone_app_complex_conf_1_id = -1;
//     PhoneAppSimpleConf *phone_app_simple_conf_0 = nullptr;
//     PhoneAppSimpleConf *phone_app_simple_conf_1 = nullptr;
//     PhoneAppComplexConf *phone_app_complex_conf_0 = nullptr;
//     PhoneAppComplexConf *phone_app_complex_conf_1 = nullptr;

//     test_lvgl_init(&disp, &tp);
//     phone = test_esp_brookesia_phone_init(disp, tp, true);

//     ESP_LOGI(TAG, "Create APP objects");
//     phone_app_simple_conf_0 = new PhoneAppSimpleConf(true, true);
//     TEST_ASSERT_NOT_NULL_MESSAGE(phone_app_simple_conf_0, "Failed to create phone app simple conf 0");
//     phone_app_simple_conf_1 = new PhoneAppSimpleConf(false, false);
//     TEST_ASSERT_NOT_NULL_MESSAGE(phone_app_simple_conf_1, "Failed to create phone app simple conf 1");
//     phone_app_complex_conf_0 = new PhoneAppComplexConf(true, true);
//     TEST_ASSERT_NOT_NULL_MESSAGE(phone_app_complex_conf_0, "Failed to create phone app complex conf 0");
//     phone_app_complex_conf_1 = new PhoneAppComplexConf(false, false);
//     TEST_ASSERT_NOT_NULL_MESSAGE(phone_app_complex_conf_1, "Failed to create phone app complex conf 1");

//     ESP_LOGI(TAG, "Install and uninstall APPs");
//     for (int i = 0; i < TEST_INSTALL_UNINSTALL_APP_TIMES; i++) {
//         phone_app_simple_conf_0_id = phone->installApp(phone_app_simple_conf_0);
//         TEST_ASSERT_TRUE_MESSAGE(phone_app_simple_conf_0_id >= 0, "Failed to install phone app simple conf 0");
//         phone_app_simple_conf_1_id = phone->installApp(phone_app_simple_conf_1);
//         TEST_ASSERT_TRUE_MESSAGE(phone_app_simple_conf_1_id >= 0, "Failed to install phone app simple conf 1");
//         phone_app_complex_conf_0_id = phone->installApp(phone_app_complex_conf_0);
//         TEST_ASSERT_TRUE_MESSAGE(phone_app_complex_conf_0_id >= 0, "Failed to install phone app complex conf 0");
//         phone_app_complex_conf_1_id = phone->installApp(phone_app_complex_conf_1);
//         TEST_ASSERT_TRUE_MESSAGE(phone_app_complex_conf_1_id >= 0, "Failed to install phone app complex conf 1");

//         TEST_ASSERT_TRUE_MESSAGE(phone->uninstallApp(phone_app_simple_conf_0_id),
//                                  "Failed to uninstall phone app simple conf 0");
//         TEST_ASSERT_TRUE_MESSAGE(phone->uninstallApp(phone_app_simple_conf_1_id),
//                                  "Failed to uninstall phone app simple conf 0");
//         TEST_ASSERT_TRUE_MESSAGE(phone->uninstallApp(phone_app_complex_conf_0_id),
//                                  "Failed to uninstall phone app complex conf 0");
//         TEST_ASSERT_TRUE_MESSAGE(phone->uninstallApp(phone_app_complex_conf_1_id),
//                                  "Failed to uninstall phone app complex conf 1");
//     }

//     ESP_LOGI(TAG, "Delete APP objects");
//     delete phone_app_simple_conf_0;
//     delete phone_app_simple_conf_1;
//     delete phone_app_complex_conf_0;
//     delete phone_app_complex_conf_1;

//     test_esp_brookesia_phone_deinit(phone);
//     test_lvgl_deinit(disp, tp);
// }

static void test_lvgl_init(lv_display_t **disp_out, lv_indev_t **tp_out)
{
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Register display driver to LVGL(%dx%d)", TEST_LVGL_RESOLUTION_WIDTH, TEST_LVGL_RESOLUTION_HEIGHT);
    int buf_bytes = TEST_LVGL_RESOLUTION_WIDTH * 10 * lv_color_format_get_size(LV_COLOR_FORMAT_RGB565);
    void *buf = malloc(buf_bytes);
    TEST_ASSERT_NOT_NULL_MESSAGE(buf, "Failed to alloc buffer");

    lv_display_t *disp = lv_display_create(TEST_LVGL_RESOLUTION_WIDTH, TEST_LVGL_RESOLUTION_HEIGHT);
    TEST_ASSERT_NOT_NULL_MESSAGE(disp, "Failed to create display");
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, buf, nullptr, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, [](lv_display_t *disp, const lv_area_t *area, uint8_t *color_p) {});

    lv_indev_t *indev = lv_indev_create();
    TEST_ASSERT_NOT_NULL_MESSAGE(indev, "Failed to create input device");
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_mode(indev, LV_INDEV_MODE_EVENT);
    lv_indev_set_read_cb(indev, [](lv_indev_t *indev, lv_indev_data_t *data) {});

    if (disp_out) {
        *disp_out = disp;
    }
    if (tp_out) {
        *tp_out = indev;
    }
}

static void test_lvgl_deinit(lv_display_t *disp, lv_indev_t *indev)
{
    ESP_LOGI(TAG, "Deinitialize LVGL library");
    free(disp->buf_1->data);
    lv_display_delete(disp);
    lv_indev_delete(indev);
    lv_deinit();
}

static systems::phone::Phone *test_esp_brookesia_phone_init(lv_display_t *disp, lv_indev_t *tp, bool enable_begin)
{
    ESP_LOGI(TAG, "Create phone object");
    systems::phone::Phone *phone = new systems::phone::Phone(disp);
    TEST_ASSERT_NOT_NULL_MESSAGE(phone, "Failed to create phone");

    if (tp != nullptr) {
        ESP_LOGI(TAG, "Phone set touch device");
        TEST_ASSERT_TRUE_MESSAGE(phone->setTouchDevice(tp), "Failed to set touch device");
    }

    if (enable_begin) {
        ESP_LOGI(TAG, "Phone begin");
        TEST_ASSERT_TRUE_MESSAGE(phone->begin(), "Failed to begin phone");
    }

    return phone;
}

static void test_esp_brookesia_phone_deinit(systems::phone::Phone *phone)
{
    ESP_LOGI(TAG, "Phone delete");
    delete phone;
}
