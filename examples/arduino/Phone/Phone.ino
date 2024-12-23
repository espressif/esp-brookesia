/**
 *
 * # ESP-Brookesia Phone Example
 *
 * The example demonstrates how to use the `esp-brookesia` and `ESP32_Display_Panel` libraries to display phone UI on the screen.
 *
 * The example is suitable for touchscreens with a resolution of `240 x 240` or higher. Otherwise, the functionality may not work properly. The default style ensures compatibility across resolutions, but the display effect may not be optimal on many resolutions. It is recommended to use a UI stylesheet with the same resolution as the screen. If no suitable stylesheet is available, it needs to be adjusted manually.
 *
 * ## How to Use
 *
 * To use this example, please firstly install the following dependent libraries:
 *
 * - ESP32_Display_Panel (0.2.*)
 * - ESP32_IO_Expander (0.1.*)
 * - lvgl (>= v8.3.9, < v9)
 *
 * Then, follow the steps below to configure the libraries and upload the example:
 *
 * 1. For **esp-brookesia**:
 *
 *     - [optional] Follow the [steps](https://github.com/espressif/esp-brookesia/blob/master/docs/how_to_use.md#configuration-instructions-1) to configure the library.
 *
 * 2. For **ESP32_Display_Panel**:
 *
 *     - [optional] Follow the [steps](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/How_To_Use.md#configuring-drivers) to configure drivers if needed.
 *     - [mandatory] If using a supported development board, follow the [steps](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/How_To_Use.md#using-supported-development-boards) to configure it.
 *     - [mandatory] If using a custom board, follow the [steps](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/How_To_Use.md#using-custom-development-boards) to configure it.
 *
 * 3. For **lvgl**:
 *
 *     - [mandatory] Follow the [steps](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/How_To_Use.md#configuring-lvgl) to add *lv_conf.h* file and change the configurations.
 *     - [mandatory] Enable the `LV_USE_SNAPSHOT` macro in the *lv_conf.h* file.
 *     - [optional] Modify the macros in the [lvgl_port_v8.h](./lvgl_port_v8.h) file to configure the lvgl porting parameters.
 *
 * 4. Navigate to the `Tools` menu in the Arduino IDE to choose a ESP board and configure its parameters. **Please ensure that the size of APP partition in the partition table is enough (e.g. 4 MB)**. For supported boards, please refter to [Configuring Supported Development Boards](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/docs/How_To_Use.md#configuring-supported-development-boards)
 * 5. Verify and upload the example to the ESP board.
 *
 * ## Technical Support and Feedback
 *
 * Please use the following feedback channels:
 *
 * - For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=22) forum.
 * - For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-brookesia/issues).
 *
 * We will get back to you as soon as possible.
 *
 */

#include <Arduino.h>
#include <ESP_Panel_Library.h>
#include <esp_brookesia.hpp>
#include <lvgl.h>
#include "lvgl_port_v8.h"
/* These are built-in app examples in `esp-brookesia` library */
#include <app_examples/phone/simple_conf/src/phone_app_simple_conf.hpp>
#include <app_examples/phone/complex_conf/src/phone_app_complex_conf.hpp>
#include <app_examples/phone/squareline/src/phone_app_squareline.hpp>

/* Enable to show memory information */
#define EXAMPLE_SHOW_MEM_INFO           (1)

/* Try using a stylesheet that corresponds to the resolution */
#if (ESP_PANEL_LCD_WIDTH == 320) && (ESP_PANEL_LCD_HEIGHT == 240)
  #define EXAMPLE_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   ESP_BROOKESIA_PHONE_320_240_DARK_STYLESHEET()
#elif (ESP_PANEL_LCD_WIDTH == 320) && (ESP_PANEL_LCD_HEIGHT == 480)
  #define EXAMPLE_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   ESP_BROOKESIA_PHONE_320_480_DARK_STYLESHEET()
#elif (ESP_PANEL_LCD_WIDTH == 480) && (ESP_PANEL_LCD_HEIGHT == 480)
  #define EXAMPLE_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   ESP_BROOKESIA_PHONE_480_480_DARK_STYLESHEET()
#elif (ESP_PANEL_LCD_WIDTH == 720) && (ESP_PANEL_LCD_HEIGHT == 1280)
  #define EXAMPLE_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   ESP_BROOKESIA_PHONE_720_1280_DARK_STYLESHEET()
#elif (ESP_PANEL_LCD_WIDTH == 800) && (ESP_PANEL_LCD_HEIGHT == 480)
  #define EXAMPLE_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   ESP_BROOKESIA_PHONE_800_480_DARK_STYLESHEET()
#elif (ESP_PANEL_LCD_WIDTH == 800) && (ESP_PANEL_LCD_HEIGHT == 1280)
  #define EXAMPLE_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   ESP_BROOKESIA_PHONE_800_1280_DARK_STYLESHEET()
#elif (ESP_PANEL_LCD_WIDTH == 1024) && (ESP_PANEL_LCD_HEIGHT == 600)
  #define EXAMPLE_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   ESP_BROOKESIA_PHONE_1024_600_DARK_STYLESHEET()
#elif (ESP_PANEL_LCD_WIDTH == 1280) && (ESP_PANEL_LCD_HEIGHT == 800)
  #define EXAMPLE_ESP_BROOKESIA_PHONE_DARK_STYLESHEET()   ESP_BROOKESIA_PHONE_1280_800_DARK_STYLESHEET()
#endif

static char buffer[128];    /* Make sure buffer is enough for `sprintf` */
static size_t internal_free = 0;
static size_t internal_total = 0;
static size_t external_free = 0;
static size_t external_total = 0;
ESP_Brookesia_Phone *phone = nullptr;

static void onClockUpdateTimerCallback(struct _lv_timer_t *t);

void setup()
{
    String title = "ESP-Brookesia Phone example";

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
    phone = new ESP_Brookesia_Phone();
    ESP_BROOKESIA_CHECK_NULL_EXIT(phone, "Create phone failed");

#ifdef EXAMPLE_ESP_BROOKESIA_PHONE_DARK_STYLESHEET
    /* Add external stylesheet and activate it */
    ESP_Brookesia_PhoneStylesheet_t *stylesheet = new ESP_Brookesia_PhoneStylesheet_t EXAMPLE_ESP_BROOKESIA_PHONE_DARK_STYLESHEET();
    ESP_BROOKESIA_CHECK_NULL_EXIT(stylesheet, "Create stylesheet failed");

    Serial.printf("Using stylesheet (%s)\n", stylesheet->core.name);
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->addStylesheet(stylesheet), "Add stylesheet failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->activateStylesheet(stylesheet), "Activate stylesheet failed");
    delete stylesheet;
#endif

    /* Configure and begin the phone */
    phone->registerLvLockCallback((ESP_Brookesia_LvLockCallback_t)(lvgl_port_lock), -1);
    phone->registerLvUnlockCallback((ESP_Brookesia_LvUnlockCallback_t)(lvgl_port_unlock));
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->begin(), "Begin phone failed");
    // ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->getCoreHome().showContainerBorder(), "Show container border failed");

    /* Install apps */
    PhoneAppSimpleConf *app_simple_conf = new PhoneAppSimpleConf();
    ESP_BROOKESIA_CHECK_NULL_EXIT(app_simple_conf, "Create app simple conf failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT((phone->installApp(app_simple_conf) >= 0), "Install app simple conf failed");
    PhoneAppComplexConf *app_complex_conf = new PhoneAppComplexConf();
    ESP_BROOKESIA_CHECK_NULL_EXIT(app_complex_conf, "Create app complex conf failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT((phone->installApp(app_complex_conf) >= 0), "Install app complex conf failed");
    PhoneAppSquareline *app_squareline = new PhoneAppSquareline();
    ESP_BROOKESIA_CHECK_NULL_EXIT(app_squareline, "Create app squareline failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT((phone->installApp(app_squareline) >= 0), "Install app squareline failed");

    /* Create a timer to update the clock */
    lv_timer_create(onClockUpdateTimerCallback, 1000, phone);

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

    /**
     * The `lockLv()` and `unlockLv()` functions are used to lock and unlock the LVGL task.
     * They are registered by the `registerLvLockCallback()` and `registerLvUnlockCallback()` functions.
     */
    phone->lockLv();
    // Update memory label on "Recents Screen"
    if (!phone->getHome().getRecentsScreen()->setMemoryLabel(
            internal_free / 1024, internal_total / 1024, external_free / 1024, external_total / 1024
        )) {
        ESP_BROOKESIA_LOGE("Set memory label failed");
    }
    phone->unlockLv();
#endif

    vTaskDelay(pdMS_TO_TICKS(2000));
}

static void onClockUpdateTimerCallback(struct _lv_timer_t *t)
{
    time_t now;
    struct tm timeinfo;
    bool is_time_pm = false;
    ESP_Brookesia_Phone *phone = (ESP_Brookesia_Phone *)t->user_data;

    time(&now);
    localtime_r(&now, &timeinfo);
    is_time_pm = (timeinfo.tm_hour >= 12);

    /* Since this callback is called from LVGL task, it is safe to operate LVGL */
    // Update clock on "Status Bar"
    ESP_BROOKESIA_CHECK_FALSE_EXIT(
        phone->getHome().getStatusBar()->setClock(timeinfo.tm_hour, timeinfo.tm_min, is_time_pm),
        "Refresh status bar failed"
    );
}
