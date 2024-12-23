/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
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
#include "app_examples/phone/simple_conf/src/phone_app_simple_conf.hpp"
#include "app_examples/phone/complex_conf/src/phone_app_complex_conf.hpp"
#include "app_examples/phone/squareline/src/phone_app_squareline.hpp"

#define TEST_LVGL_RESOLUTION_WIDTH          CONFIG_TEST_LVGL_RESOLUTION_WIDTH
#define TEST_LVGL_RESOLUTION_HEIGHT         CONFIG_TEST_LVGL_RESOLUTION_HEIGHT
#define TEST_INSTALL_UNINSTALL_APP_TIMES    (10)

/* Try using a stylesheet that corresponds to the resolution */
#if (TEST_LVGL_RESOLUTION_WIDTH == 320) && (TEST_LVGL_RESOLUTION_HEIGHT == 240)
#define TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   ESP_BROOKESIA_PHONE_320_240_DARK_STYLESHEET()
#elif (TEST_LVGL_RESOLUTION_WIDTH == 320) && (TEST_LVGL_RESOLUTION_HEIGHT == 480)
#define TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   ESP_BROOKESIA_PHONE_320_480_DARK_STYLESHEET()
#elif (TEST_LVGL_RESOLUTION_WIDTH == 480) && (TEST_LVGL_RESOLUTION_HEIGHT == 480)
#define TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   ESP_BROOKESIA_PHONE_480_480_DARK_STYLESHEET()
#elif (TEST_LVGL_RESOLUTION_WIDTH == 720) && (TEST_LVGL_RESOLUTION_HEIGHT == 1280)
#define TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   ESP_BROOKESIA_PHONE_720_1280_DARK_STYLESHEET()
#elif (TEST_LVGL_RESOLUTION_WIDTH == 800) && (TEST_LVGL_RESOLUTION_HEIGHT == 480)
#define TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   ESP_BROOKESIA_PHONE_800_480_DARK_STYLESHEET()
#elif (TEST_LVGL_RESOLUTION_WIDTH == 800) && (TEST_LVGL_RESOLUTION_HEIGHT == 1280)
#define TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   ESP_BROOKESIA_PHONE_800_1280_DARK_STYLESHEET()
#elif (TEST_LVGL_RESOLUTION_WIDTH == 1024) && (TEST_LVGL_RESOLUTION_HEIGHT == 600)
#define TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   ESP_BROOKESIA_PHONE_1024_600_DARK_STYLESHEET()
#elif (TEST_LVGL_RESOLUTION_WIDTH == 1280) && (TEST_LVGL_RESOLUTION_HEIGHT == 800)
#define TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   ESP_BROOKESIA_PHONE_1280_800_DARK_STYLESHEET()
#endif

static const char *TAG = "test_esp_brookesia_phone";

static void test_lvgl_init(lv_disp_t **disp_out, lv_indev_t **tp_out);
static void test_lvgl_deinit(void);
static ESP_Brookesia_Phone *test_esp_brookesia_phone_init(lv_disp_t *disp, lv_indev_t *tp, bool enable_begin);
static void test_esp_brookesia_phone_deinit(ESP_Brookesia_Phone *phone);

TEST_CASE("test esp-brookesia to begin and delete", "[esp-brookesia][phone][begin_del]")
{
    lv_disp_t *disp = nullptr;
    lv_indev_t *tp = nullptr;
    ESP_Brookesia_Phone *phone = nullptr;

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

    test_lvgl_deinit();
}

#ifdef TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET
TEST_CASE("test esp-brookesia to add stylesheet", "[esp-brookesia][phone][add_stylesheet]")
{
    lv_disp_t *disp = nullptr;
    lv_indev_t *tp = nullptr;
    ESP_Brookesia_Phone *phone = nullptr;
    ESP_Brookesia_PhoneStylesheet_t *phone_stylesheet = nullptr;

    test_lvgl_init(&disp, &tp);
    phone = test_esp_brookesia_phone_init(disp, tp, false);

    phone_stylesheet = new ESP_Brookesia_PhoneStylesheet_t TEST_ESP_BROOKESIA_PHONE_DARK_STYLESHEET();
    TEST_ASSERT_TRUE_MESSAGE(phone->addStylesheet(phone_stylesheet), "Failed to add phone stylesheet");
    TEST_ASSERT_TRUE_MESSAGE(phone->activateStylesheet(phone_stylesheet), "Failed to active phone stylesheet");
    delete phone_stylesheet;
    TEST_ASSERT_TRUE_MESSAGE(phone->begin(), "Failed to begin phone");

    test_esp_brookesia_phone_deinit(phone);
    test_lvgl_deinit();
}
#endif

TEST_CASE("test esp-brookesia to install and uninstall APPs", "[esp-brookesia][phone][install_uninstall_app]")
{
    lv_disp_t *disp = nullptr;
    lv_indev_t *tp = nullptr;
    ESP_Brookesia_Phone *phone = nullptr;
    int phone_app_simple_conf_0_id = -1;
    int phone_app_simple_conf_1_id = -1;
    int phone_app_complex_conf_0_id = -1;
    int phone_app_complex_conf_1_id = -1;
    PhoneAppSimpleConf *phone_app_simple_conf_0 = nullptr;
    PhoneAppSimpleConf *phone_app_simple_conf_1 = nullptr;
    PhoneAppComplexConf *phone_app_complex_conf_0 = nullptr;
    PhoneAppComplexConf *phone_app_complex_conf_1 = nullptr;

    test_lvgl_init(&disp, &tp);
    phone = test_esp_brookesia_phone_init(disp, tp, true);

    ESP_LOGI(TAG, "Create APP objects");
    phone_app_simple_conf_0 = new PhoneAppSimpleConf(true, true);
    TEST_ASSERT_NOT_NULL_MESSAGE(phone_app_simple_conf_0, "Failed to create phone app simple conf 0");
    phone_app_simple_conf_1 = new PhoneAppSimpleConf(false, false);
    TEST_ASSERT_NOT_NULL_MESSAGE(phone_app_simple_conf_1, "Failed to create phone app simple conf 1");
    phone_app_complex_conf_0 = new PhoneAppComplexConf(true, true);
    TEST_ASSERT_NOT_NULL_MESSAGE(phone_app_complex_conf_0, "Failed to create phone app complex conf 0");
    phone_app_complex_conf_1 = new PhoneAppComplexConf(false, false);
    TEST_ASSERT_NOT_NULL_MESSAGE(phone_app_complex_conf_1, "Failed to create phone app complex conf 1");

    ESP_LOGI(TAG, "Install and uninstall APPs");
    for (int i = 0; i < TEST_INSTALL_UNINSTALL_APP_TIMES; i++) {
        phone_app_simple_conf_0_id = phone->installApp(phone_app_simple_conf_0);
        TEST_ASSERT_TRUE_MESSAGE(phone_app_simple_conf_0_id >= 0, "Failed to install phone app simple conf 0");
        phone_app_simple_conf_1_id = phone->installApp(phone_app_simple_conf_1);
        TEST_ASSERT_TRUE_MESSAGE(phone_app_simple_conf_1_id >= 0, "Failed to install phone app simple conf 1");
        phone_app_complex_conf_0_id = phone->installApp(phone_app_complex_conf_0);
        TEST_ASSERT_TRUE_MESSAGE(phone_app_complex_conf_0_id >= 0, "Failed to install phone app complex conf 0");
        phone_app_complex_conf_1_id = phone->installApp(phone_app_complex_conf_1);
        TEST_ASSERT_TRUE_MESSAGE(phone_app_complex_conf_1_id >= 0, "Failed to install phone app complex conf 1");

        TEST_ASSERT_TRUE_MESSAGE(phone->uninstallApp(phone_app_simple_conf_0_id),
                                 "Failed to uninstall phone app simple conf 0");
        TEST_ASSERT_TRUE_MESSAGE(phone->uninstallApp(phone_app_simple_conf_1_id),
                                 "Failed to uninstall phone app simple conf 0");
        TEST_ASSERT_TRUE_MESSAGE(phone->uninstallApp(phone_app_complex_conf_0_id),
                                 "Failed to uninstall phone app complex conf 0");
        TEST_ASSERT_TRUE_MESSAGE(phone->uninstallApp(phone_app_complex_conf_1_id),
                                 "Failed to uninstall phone app complex conf 1");
    }

    ESP_LOGI(TAG, "Delete APP objects");
    delete phone_app_simple_conf_0;
    delete phone_app_simple_conf_1;
    delete phone_app_complex_conf_0;
    delete phone_app_complex_conf_1;

    test_esp_brookesia_phone_deinit(phone);
    test_lvgl_deinit();
}

static void test_lvgl_init(lv_disp_t **disp_out, lv_indev_t **tp_out)
{
    static lv_disp_drv_t disp_drv;
    static lv_indev_drv_t indev_drv;
    lv_disp_t *disp = nullptr;
    lv_indev_t *indev = nullptr;

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Register display driver to LVGL(%dx%d)", TEST_LVGL_RESOLUTION_WIDTH, TEST_LVGL_RESOLUTION_HEIGHT);
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TEST_LVGL_RESOLUTION_WIDTH;
    disp_drv.ver_res = TEST_LVGL_RESOLUTION_HEIGHT;
    disp = lv_disp_drv_register(&disp_drv);
    TEST_ASSERT_NOT_NULL_MESSAGE(disp, "Failed to register display driver to LVGL");

    ESP_LOGI(TAG, "Register touch driver to LVGL");
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = disp;
    indev = lv_indev_drv_register(&indev_drv);
    TEST_ASSERT_NOT_NULL_MESSAGE(indev, "Failed to register touch driver to LVGL");

    if (disp_out) {
        *disp_out = disp;
    }
    if (tp_out) {
        *tp_out = indev;
    }
}

static void test_lvgl_deinit(void)
{
    ESP_LOGI(TAG, "Deinitialize LVGL library");
    lv_deinit();
}

static ESP_Brookesia_Phone *test_esp_brookesia_phone_init(lv_disp_t *disp, lv_indev_t *tp, bool enable_begin)
{
    ESP_LOGI(TAG, "Create phone object");
    ESP_Brookesia_Phone *phone = new ESP_Brookesia_Phone(disp);
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

static void test_esp_brookesia_phone_deinit(ESP_Brookesia_Phone *phone)
{
    ESP_LOGI(TAG, "Phone delete");
    delete phone;
}
