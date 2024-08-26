/**
 *
 * # ESP-UI Phone Example
 *
 * The example demonstrates how to use the `esp-ui` and `ESP32_Display_Panel` libraries to display phone UI on the screen.
 *
 * The example is suitable for touchscreens with a resolution of `240 x 240` or higher. Otherwise, the functionality may not work properly. The default style ensures compatibility across resolutions, but the display effect may not be optimal on many resolutions. It is recommended to use a UI stylesheet with the same resolution as the screen. If no suitable stylesheet is available, it needs to be adjusted manually.
 *
 * ## How to Use
 *
 * To use this example, please firstly install the following dependent libraries:
 *
 * - ESP32_Display_Panel  (0.1.*)
 * - lvgl (>= v8.3.9, < v9)
 *
 * To use the external stylesheets, please also install the following dependent libraries and set the `EXAMPLE_USE_EXTERNAL_STYLESHEET` macro to `1`:
 *
 * - esp-ui-phone_480_480_stylesheet
 * - esp-ui-phone_800_480_stylesheet
 * - esp-ui-phone_1024_600_stylesheet
 *
 * Then, follow the steps below to configure the libraries and upload the example:
 *
 * 1. For **esp-ui**:
 *
 *     - [optional] Follow the [steps](../../../README.md#configuration-instructions-1) to configure the library.
 *
 * 2. For **ESP32_Display_Panel**:
 *
 *     - [optional] Follow the [steps](https://github.com/esp-arduino-libs/ESP32_Display_Panel?tab=readme-ov-file#configuring-drivers) to configure drivers.
 *     - [mandatory] If using a supported development board, follow the [steps](https://github.com/esp-arduino-libs/ESP32_Display_Panel?tab=readme-ov-file#using-supported-development-boards) to configure it.
 *     - [mandatory] If using a custom board, follow the [steps](https://github.com/esp-arduino-libs/ESP32_Display_Panel?tab=readme-ov-file#using-custom-development-boards) to configure it.
 *
 * 3. For **lvgl**:
 *
 *     - [optional] Follow the [steps](https://github.com/esp-arduino-libs/ESP32_Display_Panel?tab=readme-ov-file#configuring-lvgl) to add *lv_conf.h* file and change the configurations.
 *     - [mandatory] Enable the `LV_USE_SNAPSHOT` macro in the *lv_conf.h* file.
 *     - [optional] Modify the macros in the [lvgl_port_v8.h](./lvgl_port_v8.h) file to configure the lvgl porting parameters.
 *
 * 4. Navigate to the `Tools` menu in the Arduino IDE to choose a ESP board and configure its parameters. **Please ensure that the APP partition in the partition table is at least 2 MB in size**. For supported boards, please refter to [Configuring Supported Development Boards](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/Board_Instructions.md#recommended-configurations-in-the-arduino-ide)
 * 5. Verify and upload the example to the ESP board.
 *
 * ## Serial Output
 *
 * ```bash
 * ...
 * ESP-UI Phone example start
 * Initialize panel device
 * Initialize lvgl
 * Create ESP-UI Phone
 * [WARN] [esp_ui_template.hpp:107](addStylesheet): Display is not set, use default display
 * [INFO] [esp_ui_core.cpp:150](beginCore): Library version: 0.1.0
 * [WARN] [esp_ui_phone_manager.cpp:73](begin): No touch device is set, try to use default touch device
 * [WARN] [esp_ui_phone_manager.cpp:77](begin): Using default touch device(@0x0x3fcede40)
 * ESP-UI Phone example end
 *   Biggest /     Free /    Total
 * SRAM : [  253940 /   263908 /   387916]
 * PSRAM : [ 7864308 /  7924256 /  8388608]
 * ...
 * ```
 *
 * ## Technical Support and Feedback
 *
 * Please use the following feedback channels:
 *
 * - For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=22) forum.
 * - For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-ui/issues).
 *
 * We will get back to you as soon as possible.
 *
 */

#include <Arduino.h>
#include <ESP_Panel_Library.h>
#include <esp_ui.hpp>
#include <lvgl.h>
#include "lvgl_port_v8.h"
/* These are built-in app examples in `esp-ui` library */
#include <app_examples/phone/simple_conf/src/phone_app_simple_conf.hpp>
#include <app_examples/phone/complex_conf/src/phone_app_complex_conf.hpp>
#include <app_examples/phone/squareline/src/phone_app_squareline.hpp>

/* Enable to show memory information */
#define EXAMPLE_SHOW_MEM_INFO             (1)

/* To use the external stylesheets, please install the dependent libraries and enable the macro */
#define EXAMPLE_USE_EXTERNAL_STYLESHEET   (0)
#if EXAMPLE_USE_EXTERNAL_STYLESHEET
    #if (ESP_PANEL_LCD_WIDTH == 1024) && (ESP_PANEL_LCD_HEIGHT == 600)
        #include "esp_ui_phone_1024_600_stylesheet.h"
        #define EXAMPLE_ESP_UI_PHONE_DARK_STYLESHEET()   ESP_UI_PHONE_1024_600_DARK_STYLESHEET()
    #elif (ESP_PANEL_LCD_WIDTH == 800) && (ESP_PANEL_LCD_HEIGHT == 480)
        #include "esp_ui_phone_800_480_stylesheet.h"
        #define EXAMPLE_ESP_UI_PHONE_DARK_STYLESHEET()   ESP_UI_PHONE_800_480_DARK_STYLESHEET()
    #elif (ESP_PANEL_LCD_WIDTH == 480) && (ESP_PANEL_LCD_HEIGHT == 480)
        #include "esp_ui_phone_480_480_stylesheet.h"
        #define EXAMPLE_ESP_UI_PHONE_DARK_STYLESHEET()   ESP_UI_PHONE_480_480_DARK_STYLESHEET()
    #endif
#endif

static char buffer[128];    /* Make sure buffer is enough for `sprintf` */
static size_t internal_free = 0;
static size_t internal_total = 0;
static size_t external_free = 0;
static size_t external_total = 0;
ESP_UI_Phone *phone = nullptr;

static void on_clock_update_timer_cb(struct _lv_timer_t *t);

void setup()
{
    String title = "ESP-UI Phone example";

    Serial.begin(115200);
    Serial.println(title + " start");

    Serial.println("Initialize panel device");
    ESP_Panel *panel = new ESP_Panel();
    panel->init();
#if LVGL_PORT_AVOID_TEAR
    // When avoid tearing function is enabled, configure the RGB bus according to the LVGL configuration
    ESP_PanelBus_RGB *rgb_bus = static_cast<ESP_PanelBus_RGB *>(panel->getLcd()->getBus());
    rgb_bus->configRgbFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
    rgb_bus->configRgbBounceBufferSize(LVGL_PORT_RGB_BOUNCE_BUFFER_SIZE);
#endif
    panel->begin();

    Serial.println("Initialize lvgl");
    lvgl_port_init(panel->getLcd(), panel->getTouch());

    Serial.println("Create and begin phone");
    /**
     * To avoid errors caused by multiple tasks simultaneously accessing LVGL,
     * should acquire a lock before operating on LVGL.
     */
    lvgl_port_lock(-1);

    /* Create a phone object */
    phone = new ESP_UI_Phone();
    ESP_UI_CHECK_NULL_EXIT(phone, "Create phone failed");

#ifdef EXAMPLE_ESP_UI_PHONE_DARK_STYLESHEET
    /* Add external stylesheet and activate it */
    ESP_UI_PhoneStylesheet_t *phone_stylesheet = new ESP_UI_PhoneStylesheet_t EXAMPLE_ESP_UI_PHONE_DARK_STYLESHEET();
    ESP_UI_CHECK_NULL_EXIT(phone_stylesheet, "Create phone stylesheet failed");
    ESP_UI_CHECK_FALSE_EXIT(phone->addStylesheet(phone_stylesheet), "Add phone stylesheet failed");
    ESP_UI_CHECK_FALSE_EXIT(phone->activateStylesheet(phone_stylesheet), "Activate phone stylesheet failed");
    delete phone_stylesheet;
#endif

    /* Configure and begin the phone */
    ESP_UI_CHECK_FALSE_EXIT(phone->begin(), "Begin phone failed");
    // ESP_UI_CHECK_FALSE_EXIT(phone->getCoreHome().showContainerBorder(), "Show container border failed");

    /* Install apps */
    bool enable_navigation_bar = phone->getStylesheet()->home.flags.enable_navigation_bar;
    PhoneAppSimpleConf *phone_app_simple_conf = new PhoneAppSimpleConf(true, enable_navigation_bar);
    ESP_UI_CHECK_NULL_EXIT(phone_app_simple_conf, "Create phone app simple conf failed");
    ESP_UI_CHECK_FALSE_EXIT((phone->installApp(phone_app_simple_conf) >= 0), "Install phone app simple conf failed");
    PhoneAppComplexConf *phone_app_complex_conf = new PhoneAppComplexConf(true, enable_navigation_bar);
    ESP_UI_CHECK_NULL_EXIT(phone_app_complex_conf, "Create phone app complex conf failed");
    ESP_UI_CHECK_FALSE_EXIT((phone->installApp(phone_app_complex_conf) >= 0), "Install phone app complex conf failed");
    PhoneAppSquareline *phone_app_squareline = new PhoneAppSquareline(true, enable_navigation_bar);
    ESP_UI_CHECK_NULL_EXIT(phone_app_squareline, "Create phone app squareline failed");
    ESP_UI_CHECK_FALSE_EXIT((phone->installApp(phone_app_squareline) >= 0), "Install phone app squareline failed");

    /* Create a timer to update the clock */
    lv_timer_create(on_clock_update_timer_cb, 1000, phone);

    /* Release the lock */
    lvgl_port_unlock();

    Serial.println(title + " end");
}

void loop()
{
#if EXAMPLE_SHOW_MEM_INFO
    internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    internal_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    external_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    external_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    sprintf(buffer, "  Biggest /     Free /    Total\n"
                    "SRAM : [%8d / %8d / %8d]\n"
                    "PSRAM : [%8d / %8d / %8d]\n",
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), internal_free, internal_total,
            heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM), external_free, external_total);
    Serial.printf("%s", buffer);

    lvgl_port_lock(-1);
    // Update memory label on "Recents Screen"
    if (!phone->getHome().getRecentsScreen()->setMemoryLabel(internal_free / 1024, internal_total / 1024,
        external_free / 1024, external_total / 1024)) {
        ESP_UI_LOGE("Set memory label failed");
    }
    lvgl_port_unlock();

    vTaskDelay(pdMS_TO_TICKS(2000));
#endif
}

static void on_clock_update_timer_cb(struct _lv_timer_t *t)
{
    time_t now;
    struct tm timeinfo;
    bool is_time_pm = false;
    ESP_UI_Phone *phone = (ESP_UI_Phone *)t->user_data;

    time(&now);
    localtime_r(&now, &timeinfo);
    is_time_pm = (timeinfo.tm_hour >= 12);

    // Update clock on "Status Bar"
    ESP_UI_CHECK_FALSE_EXIT(phone->getHome().getStatusBar()->setClock(timeinfo.tm_hour, timeinfo.tm_min, is_time_pm),
                            "Refresh status bar failed");
}
