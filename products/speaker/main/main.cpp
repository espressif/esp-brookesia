/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <map>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_spiffs.h"
#if SOC_USB_SERIAL_JTAG_SUPPORTED
#   include "soc/usb_serial_jtag_reg.h"
#   include "hal/usb_serial_jtag_ll.h"
#endif
#include "esp_private/usb_phy.h"

#include "lvgl.h"
#include "boost/thread.hpp"
#include "esp_codec_dev.h"

#include "bsp/esp-bsp.h"
#include "esp_lvgl_port_disp.h"
#include "esp_brookesia.hpp"
#include "esp_brookesia_app_settings.hpp"
#include "esp_brookesia_app_ai_profile.hpp"
#include "esp_brookesia_app_game_2048.hpp"
#include "esp_brookesia_app_calculator.hpp"
#include "esp_brookesia_app_timer.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "Main"
#include "esp_lib_utils.h"

#include "usb_msc.h"
#include "audio_sys.h"
#include "coze_agent_config.h"
#include "coze_agent_config_default.h"

constexpr bool        EXAMPLE_SHOW_MEM_INFO     = false;

constexpr const char *MUSIC_PARTITION_LABEL     = "spiffs_data";
constexpr int         DEVELOPER_MODE_KEY        = 0x655;

constexpr int         LVGL_TASK_PRIORITY        = 4;
constexpr int         LVGL_TASK_CORE_ID         = 1;
constexpr int         LVGL_TASK_STACK_SIZE      = 20 * 1024;
constexpr int         LVGL_TASK_MAX_SLEEP_MS    = 500;
constexpr int         LVGL_TASK_TIMER_PERIOD_MS = 5;
constexpr bool        LVGL_TASK_STACK_CAPS_EXT  = true;

constexpr int         PARAM_SOUND_VOLUME_MIN            = 0;
constexpr int         PARAM_SOUND_VOLUME_MAX            = 100;
constexpr int         PARAM_SOUND_VOLUME_DEFAULT        = 70;
constexpr int         PARAM_DISPLAY_BRIGHTNESS_MIN      = 10;
constexpr int         PARAM_DISPLAY_BRIGHTNESS_MAX      = 100;
constexpr int         PARAM_DISPLAY_BRIGHTNESS_DEFAULT  = 100;

constexpr const char *FUNCTION_OPEN_APP_THREAD_NAME               = "open_app";
constexpr int         FUNCTION_OPEN_APP_THREAD_STACK_SIZE         = 10 * 1024;
constexpr int         FUNCTION_OPEN_APP_WAIT_SPEAKING_PRE_MS      = 2000;
constexpr int         FUNCTION_OPEN_APP_WAIT_SPEAKING_INTERVAL_MS = 10;
constexpr int         FUNCTION_OPEN_APP_WAIT_SPEAKING_MAX_MS      = 2000;
constexpr bool        FUNCTION_OPEN_APP_THREAD_STACK_CAPS_EXT     = true;

constexpr const char *FUNCTION_VOLUME_CHANGE_THREAD_NAME           = "volume_change";
constexpr size_t      FUNCTION_VOLUME_CHANGE_THREAD_STACK_SIZE     = 6 * 1024;
constexpr bool        FUNCTION_VOLUME_CHANGE_THREAD_STACK_CAPS_EXT = true;
constexpr int         FUNCTION_VOLUME_CHANGE_STEP                  = 20;

constexpr const char *FUNCTION_BRIGHTNESS_CHANGE_THREAD_NAME           = "brightness_change";
constexpr size_t      FUNCTION_BRIGHTNESS_CHANGE_THREAD_STACK_SIZE     = 6 * 1024;
constexpr bool        FUNCTION_BRIGHTNESS_CHANGE_THREAD_STACK_CAPS_EXT = true;
constexpr int         FUNCTION_BRIGHTNESS_CHANGE_STEP                  = 30;

using namespace esp_brookesia::speaker;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::speaker_apps;
using namespace esp_brookesia::services;
using namespace esp_brookesia::ai_framework;

static bool init_display_and_draw_logic();
static bool init_sdcard();
static bool check_whether_enter_developer_mode();
static bool init_media_audio();
static bool init_services();
static bool load_coze_agent_config();
static bool create_speaker_and_install_apps();

static esp_codec_dev_handle_t play_dev = nullptr;
static esp_codec_dev_handle_t rec_dev = nullptr;

/**
 * This variable is used to store a special key which indicates whether to enter developer mode.
 * When the device is rebooted by software, this variable will not be initialized.
 */
static RTC_NOINIT_ATTR int developer_mode_key;

#if COZE_AGENT_ENABLE_DEFAULT_CONFIG
extern const char private_key_pem_start[] asm("_binary_private_key_pem_start");
extern const char private_key_pem_end[]   asm("_binary_private_key_pem_end");
#endif

extern "C" void app_main()
{
    printf("Project version: %s\n", CONFIG_APP_PROJECT_VER);

    assert(init_display_and_draw_logic()        && "Initialize display and draw logic failed");
    assert(init_sdcard()                        && "Initialize SD card failed");
    assert(check_whether_enter_developer_mode() && "Check whether enter developer mode failed");
    assert(init_media_audio()                   && "Initialize media audio failed");
    assert(init_services()                      && "Initialize services failed");
    if (!load_coze_agent_config()) {
        ESP_UTILS_LOGE("Load coze agent config failed");
    }
    assert(create_speaker_and_install_apps()    && "Create speaker and install apps failed");

    if constexpr (EXAMPLE_SHOW_MEM_INFO) {
        esp_utils::thread_config_guard thread_config({
            .name = "mem_info",
            .stack_size = 4096,
        });
        boost::thread([ = ]() {
            char buffer[512];    /* Make sure buffer is enough for `sprintf` */

            while (1) {
                // heap_caps_check_integrity_all(true);

                esp_utils_mem_print_info();

                lv_mem_monitor_t mon;
                lv_mem_monitor(&mon);
                sprintf(buffer, "used: %zu (%3d %%), frag: %3d %%, biggest free: %zu, total: %zu, free: %zu",
                        mon.total_size - mon.free_size, mon.used_pct, mon.frag_pct,
                        mon.free_biggest_size,
                        mon.total_size, mon.free_size);
                ESP_UTILS_LOGI("%s", buffer);

                audio_sys_get_real_time_stats();

                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        }).detach();
    }
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

static bool init_display_and_draw_logic()
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
            .buff_spiram = true,
            .default_dummy_draw = (developer_mode_key != DEVELOPER_MODE_KEY), // Avoid white screen during initialization
        },
    };
    auto disp = bsp_display_start_with_config(&cfg);
    ESP_UTILS_CHECK_NULL_RETURN(disp, false, "Start display failed");
    if (developer_mode_key != DEVELOPER_MODE_KEY) {
        ESP_UTILS_CHECK_FALSE_RETURN(clear_display(disp), false, "Clear display failed");
        vTaskDelay(pdMS_TO_TICKS(100)); // Avoid snow screen
    }
    bsp_display_backlight_on();

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
            bsp_display_lock(0);
            lv_obj_invalidate(lv_screen_active());
            bsp_display_unlock();
        } else {
            ESP_UTILS_CHECK_FALSE_EXIT(clear_display(disp), "Clear display failed");
        }

        is_lvgl_dummy_draw = enable;
    });

    return true;
}

static bool init_sdcard()
{
    ESP_UTILS_LOG_TRACE_GUARD();

    auto ret = bsp_sdcard_mount();
    if (ret == ESP_OK) {
        ESP_UTILS_LOGI("Mount SD card successfully");
        return true;
    } else {
        ESP_UTILS_LOGE("Mount SD card failed(%s)", esp_err_to_name(ret));
    }

    Display::on_dummy_draw_signal(false);

    bsp_display_lock(0);

    auto label = lv_label_create(lv_screen_active());
    lv_obj_set_size(label, 300, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(label, &esp_brookesia_font_maison_neue_book_26, 0);
    lv_label_set_text(label, "SD card not found, please insert a SD card!");
    lv_obj_center(label);

    bsp_display_unlock();

    while ((ret = bsp_sdcard_mount()) != ESP_OK) {
        ESP_UTILS_LOGE("Mount SD card failed(%s), retry...", esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    bsp_display_lock(0);
    lv_obj_del(label);
    bsp_display_unlock();

    Display::on_dummy_draw_signal(true);

    return true;
}

static void _usb_serial_jtag_phy_init()
{
    SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_PAD_PULL_OVERRIDE);
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLUP);
    SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLDOWN);
    vTaskDelay(pdMS_TO_TICKS(10));
#if USB_SERIAL_JTAG_LL_EXT_PHY_SUPPORTED
    usb_serial_jtag_ll_phy_enable_external(false);  // Use internal PHY
    usb_serial_jtag_ll_phy_enable_pad(true);        // Enable USB PHY pads
#else // USB_SERIAL_JTAG_LL_EXT_PHY_SUPPORTED
    usb_serial_jtag_ll_phy_set_defaults();          // External PHY not supported. Set default values.
#endif // USB_WRAP_LL_EXT_PHY_SUPPORTED
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLDOWN);
    SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLUP);
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_PAD_PULL_OVERRIDE);
}

static bool check_whether_enter_developer_mode()
{
    ESP_UTILS_LOG_TRACE_GUARD();

    if (developer_mode_key != DEVELOPER_MODE_KEY) {
        ESP_UTILS_LOGI("Developer mode disabled");
        return true;
    }

    bsp_display_lock(0);

    auto title_label = lv_label_create(lv_screen_active());
    lv_obj_set_size(title_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(title_label, &esp_brookesia_font_maison_neue_book_26, 0);
    lv_label_set_text(title_label, "Developer Mode");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 60);

    auto content_label = lv_label_create(lv_screen_active());
    lv_obj_set_size(content_label, LV_PCT(80), LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(content_label, &esp_brookesia_font_maison_neue_book_18, 0);
    lv_obj_set_style_text_align(content_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(
        content_label, "Please connect the device to your computer via USB. A USB drive will appear. "
        "You can create or modify the files in the SD card (like `bot_setting.json` and `private_key.pem`) as needed."
    );
    lv_obj_align_to(content_label, title_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

    auto exit_button = lv_btn_create(lv_screen_active());
    lv_obj_set_size(exit_button, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(exit_button, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_add_event_cb(exit_button, [](lv_event_t *e) {
        ESP_UTILS_LOGI("Exit developer mode");
        developer_mode_key = 0;
        _usb_serial_jtag_phy_init();
        esp_restart();
    }, LV_EVENT_CLICKED, nullptr);

    auto label_button = lv_label_create(exit_button);
    lv_obj_set_style_text_font(label_button, &esp_brookesia_font_maison_neue_book_16, 0);
    lv_label_set_text(label_button, "Exit and reboot");
    lv_obj_center(label_button);

    bsp_display_unlock();

    // mount_wl_basic_and_tusb();
    ESP_UTILS_CHECK_ERROR_RETURN(usb_msc_mount(), false, "Mount USB MSC failed");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static bool init_media_audio()
{
    ESP_UTILS_LOG_TRACE_GUARD();

    esp_gmf_setup_periph_hardware_info periph_info = {
        .i2c = {
            .handle = bsp_i2c_get_handle(),
        },
        .codec = {
            .io_pa = BSP_POWER_AMP_IO,
            .type = ESP_GMF_CODEC_TYPE_ES7210_IN_ES8311_OUT,
            .dac = {
                .io_mclk = BSP_I2S_MCLK,
                .io_bclk = BSP_I2S_SCLK,
                .io_ws = BSP_I2S_LCLK,
                .io_do = BSP_I2S_DOUT,
                .io_di = BSP_I2S_DSIN,
                .sample_rate = 16000,
                .channel = 2,
                .bits_per_sample = 32,
                .port_num  = 0,
            },
            .adc = {
                .io_mclk = BSP_I2S_MCLK,
                .io_bclk = BSP_I2S_SCLK,
                .io_ws = BSP_I2S_LCLK,
                .io_do = BSP_I2S_DOUT,
                .io_di = BSP_I2S_DSIN,
                .sample_rate = 16000,
                .channel = 2,
                .bits_per_sample = 32,
                .port_num  = 0,
            },
        },
    };
    ESP_UTILS_CHECK_ERROR_RETURN(
        audio_manager_init(&periph_info, &play_dev, &rec_dev), false, "Initialize audio manager failed"
    );
    ESP_UTILS_CHECK_ERROR_RETURN(audio_prompt_open(), false, "Open audio prompt failed");

    /* Initialize SPIFFS with music partition */
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = MUSIC_PARTITION_LABEL,
        .max_files = 5,
        .format_if_mount_failed = false
    };
    auto ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Failed to find SPIFFS partition");
        } else {
            ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
    }
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(MUSIC_PARTITION_LABEL, &total, &used);
    if (ret != ESP_OK) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            false, false, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret)
        );
    } else {
        ESP_UTILS_LOGI("Partition size: total: %d, used: %d", total, used);
    }

    return true;
}

static bool set_media_sound_volume(int volume)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    ESP_UTILS_CHECK_NULL_RETURN(play_dev, false, "Invalid play device");

    ESP_UTILS_LOGI("Param: volume(%d)", volume);

    volume = std::clamp(volume, PARAM_SOUND_VOLUME_MIN, PARAM_SOUND_VOLUME_MAX);
    ESP_UTILS_CHECK_FALSE_RETURN(
        esp_codec_dev_set_out_vol(play_dev, volume) == ESP_CODEC_DEV_OK, false, "Set media sound volume failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        StorageNVS::requestInstance().setLocalParam(SETTINGS_NVS_KEY_VOLUME, volume), false,
        "Set NVS sound volume failed"
    );

    return true;
}

static int get_media_sound_volume()
{
    StorageNVS::Value volume;
    ESP_UTILS_CHECK_FALSE_RETURN(
        StorageNVS::requestInstance().getLocalParam(SETTINGS_NVS_KEY_VOLUME, volume),
        PARAM_SOUND_VOLUME_DEFAULT, "Get NVS sound volume failed"
    );

    return std::get<int>(volume);
}

static bool set_media_display_brightness(int brightness)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    ESP_UTILS_LOGI("Param: brightness(%d)", brightness);

    brightness = std::clamp(brightness, PARAM_DISPLAY_BRIGHTNESS_MIN, PARAM_DISPLAY_BRIGHTNESS_MAX);
    ESP_UTILS_CHECK_FALSE_RETURN(
        bsp_display_brightness_set(brightness) == ESP_OK, false, "Set display brightness failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        StorageNVS::requestInstance().setLocalParam(SETTINGS_NVS_KEY_BRIGHTNESS, brightness), false,
        "Set NVS display brightness failed"
    );

    return true;
}

static int get_media_display_brightness()
{
    StorageNVS::Value brightness;
    ESP_UTILS_CHECK_FALSE_RETURN(
        StorageNVS::requestInstance().getLocalParam(SETTINGS_NVS_KEY_BRIGHTNESS, brightness),
        PARAM_DISPLAY_BRIGHTNESS_DEFAULT, "Get NVS display brightness failed"
    );

    return std::get<int>(brightness);
}

static bool init_services()
{
    ESP_UTILS_LOG_TRACE_GUARD();

    /* Startup NVS Service */
    ESP_UTILS_CHECK_FALSE_RETURN(StorageNVS::requestInstance().begin(), false, "Failed to begin storage NVS");

    /* Initialize system settings */
    StorageNVS::Value volume = PARAM_SOUND_VOLUME_DEFAULT;
    if (!StorageNVS::requestInstance().getLocalParam(SETTINGS_NVS_KEY_VOLUME, volume)) {
        ESP_UTILS_LOGW("Volume not found in NVS, set to default value(%d)", std::get<int>(volume));
    }
    ESP_UTILS_CHECK_FALSE_RETURN(
        set_media_sound_volume(std::get<int>(volume)), false, "Set media sound volume failed"
    );
    StorageNVS::Value brightness = PARAM_DISPLAY_BRIGHTNESS_DEFAULT;
    if (!StorageNVS::requestInstance().getLocalParam(SETTINGS_NVS_KEY_BRIGHTNESS, brightness)) {
        ESP_UTILS_LOGW("Brightness not found in NVS, set to default value(%d)", std::get<int>(brightness));
    }
    ESP_UTILS_CHECK_FALSE_RETURN(
        set_media_display_brightness(std::get<int>(brightness)), false, "Set media display brightness failed"
    );

    return true;
}

static std::string to_lower(const std::string &input)
{
    std::string result = input;
    for (char &c : result) {
        c = std::tolower(static_cast<unsigned char>(c));
    }
    return result;
}

static std::string get_before_space(const std::string &input)
{
    size_t pos = input.find(' ');
    return input.substr(0, pos);
}

static bool load_coze_agent_config()
{
    ESP_UTILS_LOG_TRACE_GUARD();

    coze_agent_config_t config = {};
    CozeChatAgentInfo agent_info = {};
    std::vector<CozeChatRobotInfo> robot_infos;

    if (coze_agent_config_read(&config) == ESP_OK) {
        agent_info.custom_consumer = config.custom_consumer ? config.custom_consumer : "";
        agent_info.app_id = config.appid ? config.appid : "";
        agent_info.public_key = config.public_key ? config.public_key : "";
        agent_info.private_key = config.private_key ? config.private_key : "";
        for (int i = 0; i < config.bot_num; i++) {
            robot_infos.push_back(CozeChatRobotInfo{
                .name = config.bot[i].bot_name ? config.bot[i].bot_name : "",
                .bot_id = config.bot[i].bot_id ? config.bot[i].bot_id : "",
                .voice_id = config.bot[i].voice_id ? config.bot[i].voice_id : "",
                .description = config.bot[i].bot_description ? config.bot[i].bot_description : "",
            });
        }
        ESP_UTILS_CHECK_FALSE_RETURN(coze_agent_config_release(&config) == ESP_OK, false, "Release bot config failed");
    } else {
#if COZE_AGENT_ENABLE_DEFAULT_CONFIG
        ESP_UTILS_LOGW("Failed to read bot config from flash, use default config");
        agent_info.custom_consumer = COZE_AGENT_CUSTOM_CONSUMER;
        agent_info.app_id = COZE_AGENT_APP_ID;
        agent_info.public_key = COZE_AGENT_DEVICE_PUBLIC_KEY;
        agent_info.private_key = std::string(private_key_pem_start, private_key_pem_end - private_key_pem_start);
#if COZE_AGENT_BOT1_ENABLE
        robot_infos.push_back(CozeChatRobotInfo {
            .name = COZE_AGENT_BOT1_NAME,
            .bot_id = COZE_AGENT_BOT1_ID,
            .voice_id = COZE_AGENT_BOT1_VOICE_ID,
            .description = COZE_AGENT_BOT1_DESCRIPTION,
        });
#endif // COZE_AGENT_BOT1_ENABLE
#if COZE_AGENT_BOT2_ENABLE
        robot_infos.push_back(CozeChatRobotInfo {
            .name = COZE_AGENT_BOT2_NAME,
            .bot_id = COZE_AGENT_BOT2_ID,
            .voice_id = COZE_AGENT_BOT2_VOICE_ID,
            .description = COZE_AGENT_BOT2_DESCRIPTION,
        });
#endif // COZE_AGENT_BOT2_ENABLE
#else
        ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Failed to read bot config");
#endif // COZE_AGENT_ENABLE_DEFAULT_CONFIG
    }

    ESP_UTILS_CHECK_FALSE_RETURN(
        Agent::requestInstance()->configCozeAgentConfig(agent_info, robot_infos), false, "Config coze agent failed"
    );

    return true;
}

// static void on_clock_update_timer_cb(struct _lv_timer_t *t)

// {
//     time_t now;
//     struct tm timeinfo;
//     Speaker *speaker = (Speaker *)t->user_data;

//     time(&now);
//     localtime_r(&now, &timeinfo);

//     /* Since this callback is called from LVGL task, it is safe to operate LVGL */
//     // Update clock on "Status Bar"
//     ESP_UTILS_CHECK_FALSE_EXIT(
//         speaker->display.getQuickSettings().setClockTime(timeinfo.tm_hour, timeinfo.tm_min),
//         "Refresh status bar failed"
//     );

// }

static bool create_speaker_and_install_apps()
{
    ESP_UTILS_LOG_TRACE_GUARD();

    Speaker *speaker = nullptr;
    /* Create a speaker object */
    ESP_UTILS_CHECK_EXCEPTION_RETURN(
        speaker = new Speaker(), false, "Create speaker failed"
    );

    /* Try using a stylesheet that corresponds to the resolution */
    std::unique_ptr<SpeakerStylesheet_t> stylesheet;
    ESP_UTILS_CHECK_EXCEPTION_RETURN(
        stylesheet = std::make_unique<SpeakerStylesheet_t>(ESP_BROOKESIA_SPEAKER_360_360_DARK_STYLESHEET), false,
        "Create stylesheet failed"
    );
    ESP_UTILS_LOGI("Using stylesheet (%s)", stylesheet->core.name);
    ESP_UTILS_CHECK_FALSE_RETURN(speaker->addStylesheet(stylesheet.get()), false, "Add stylesheet failed");
    ESP_UTILS_CHECK_FALSE_RETURN(speaker->activateStylesheet(stylesheet.get()), false, "Activate stylesheet failed");
    stylesheet = nullptr;

    /* Configure and begin the speaker */
    speaker->registerLvLockCallback((LockCallback)(bsp_display_lock), 0);
    speaker->registerLvUnlockCallback((UnlockCallback)(bsp_display_unlock));
    ESP_UTILS_LOGI("Display ESP-Brookesia speaker demo");

    speaker->lockLv();

    ESP_UTILS_CHECK_FALSE_RETURN(speaker->begin(), false, "Begin failed");

    /* Install app settings */
    auto app_settings = Settings::requestInstance();
    ESP_UTILS_CHECK_NULL_RETURN(app_settings, false, "Get app settings failed");
    // Add app settings stylesheet
    std::unique_ptr<SettingsStylesheetData> app_settings_stylesheet;
    ESP_UTILS_CHECK_EXCEPTION_RETURN(
        app_settings_stylesheet = std::make_unique<SettingsStylesheetData>(SETTINGS_UI_360_360_STYLESHEET_DARK()),
        false, "Create app settings stylesheet failed"
    );
    app_settings_stylesheet->screen_size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(100, 100);
    app_settings_stylesheet->manager.wlan.scan_ap_count_max = 30;
    app_settings_stylesheet->manager.wlan.scan_interval_ms = 10000;
    app_settings_stylesheet->manager.about.device_board_name = "EchoEar";
    app_settings_stylesheet->manager.about.device_ram_main = "512KB";
    app_settings_stylesheet->manager.about.device_ram_minor = "16MB";
    ESP_UTILS_CHECK_FALSE_RETURN(
        app_settings->addStylesheet(speaker, app_settings_stylesheet.get()), false, "Add app settings stylesheet failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        app_settings->activateStylesheet(app_settings_stylesheet.get()), false, "Activate app settings stylesheet failed"
    );
    app_settings_stylesheet = nullptr;
    // Process settings events
    app_settings->manager.event_signal.connect([](SettingsManager::EventType event_type, SettingsManager::EventData event_data) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_LOGD("Param: event_type(%d), event_data(%s)", static_cast<int>(event_type), event_data.type().name());

        switch (event_type) {
        case SettingsManager::EventType::EnterDeveloperMode: {
            ESP_UTILS_CHECK_FALSE_RETURN(
                event_data.type() == typeid(SETTINGS_EVENT_DATA_TYPE_ENTER_DEVELOPER_MODE), false,
                "Invalid developer mode type"
            );

            ESP_UTILS_LOGW("Enter developer mode");
            developer_mode_key = DEVELOPER_MODE_KEY;
            esp_restart();
            break;
        }
        case SettingsManager::EventType::SetSoundVolume: {
            ESP_UTILS_CHECK_FALSE_RETURN(
                event_data.type() == typeid(SETTINGS_EVENT_DATA_TYPE_SET_SOUND_VOLUME), false, "Invalid volume type"
            );

            auto volume = std::any_cast<SETTINGS_EVENT_DATA_TYPE_SET_SOUND_VOLUME>(event_data);
            ESP_UTILS_CHECK_FALSE_RETURN(
                set_media_sound_volume(volume), false, "Set media sound volume failed"
            );
            break;
        }
        case SettingsManager::EventType::GetSoundVolume: {
            ESP_UTILS_CHECK_FALSE_RETURN(
                event_data.type() == typeid(SETTINGS_EVENT_DATA_TYPE_GET_SOUND_VOLUME), false, "Invalid volume type"
            );

            auto &volume = std::any_cast<SETTINGS_EVENT_DATA_TYPE_GET_SOUND_VOLUME>(event_data).get();
            volume = get_media_sound_volume();
            break;
        }
        case SettingsManager::EventType::SetDisplayBrightness: {
            ESP_UTILS_CHECK_FALSE_RETURN(
                event_data.type() == typeid(SETTINGS_EVENT_DATA_TYPE_SET_DISPLAY_BRIGHTNESS), false,
                "Invalid brightness type"
            );

            auto brightness = std::any_cast<SETTINGS_EVENT_DATA_TYPE_SET_DISPLAY_BRIGHTNESS>(event_data);
            ESP_UTILS_CHECK_FALSE_RETURN(
                set_media_display_brightness(brightness), false, "Set media display brightness failed"
            );
            break;
        }
        case SettingsManager::EventType::GetDisplayBrightness: {
            ESP_UTILS_CHECK_FALSE_RETURN(
                event_data.type() == typeid(SETTINGS_EVENT_DATA_TYPE_GET_DISPLAY_BRIGHTNESS), false,
                "Invalid brightness type"
            );

            auto &brightness = std::any_cast<SETTINGS_EVENT_DATA_TYPE_GET_DISPLAY_BRIGHTNESS>(event_data).get();
            brightness = get_media_display_brightness();
            break;
        }
        default:
            return false;
        }

        return true;
    });
    auto app_settings_id = speaker->installApp(app_settings);
    ESP_UTILS_CHECK_FALSE_RETURN(speaker->checkAppID_Valid(app_settings_id), false, "Install app settings failed");

    /* Install app ai profile */
    auto app_ai_profile = AI_Profile::requestInstance();
    ESP_UTILS_CHECK_NULL_RETURN(app_ai_profile, false, "Get app ai profile failed");
    auto app_ai_profile_id = speaker->installApp(app_ai_profile);
    ESP_UTILS_CHECK_FALSE_RETURN(speaker->checkAppID_Valid(app_ai_profile_id), false, "Install app ai profile failed");

    /* Install 2048 game app */
    Game2048 *app_game_2048 = nullptr;
    ESP_UTILS_CHECK_EXCEPTION_RETURN(app_game_2048 = new Game2048(240, 360), false, "Create 2048 game app failed");
    auto app_game_2048_id = speaker->installApp(app_game_2048);
    ESP_UTILS_CHECK_FALSE_RETURN(speaker->checkAppID_Valid(app_game_2048_id), false, "Install 2048 game app failed");

    /* Install calculator app */
    Calculator *app_calculator = nullptr;
    ESP_UTILS_CHECK_EXCEPTION_RETURN(app_calculator = new Calculator(), false, "Create calculator app failed");
    auto app_calculator_id = speaker->installApp(app_calculator);
    ESP_UTILS_CHECK_FALSE_RETURN(speaker->checkAppID_Valid(app_calculator_id), false, "Install calculator app failed");

    /* Install timer app */
    auto app_timer = Timer::requestInstance();
    ESP_UTILS_CHECK_NULL_RETURN(app_timer, false, "Get timer app failed");
    auto app_timer_id = speaker->installApp(app_timer);
    ESP_UTILS_CHECK_FALSE_RETURN(speaker->checkAppID_Valid(app_timer_id), false, "Install timer app failed");

    speaker->unlockLv();

    /* Register function callings */
    FunctionDefinition openApp("open_app", "Open a specific app.打开一个应用");
    openApp.addParameter("app_name", "The name of the app to open.应用名称", FunctionParameter::ValueType::String);
    openApp.setCallback([ = ](const std::vector<FunctionParameter> &params) {
        ESP_UTILS_LOG_TRACE_GUARD();

        static std::map<int, std::vector<std::string>> app_name_map = {
            {app_settings_id, {app_settings->getName(), "setting", "settings", "设置", "设置应用", "设置app"}},
            {app_game_2048_id, {app_game_2048->getName(), "2048", "game", "游戏", "2048游戏", "2048app"}},
            {app_calculator_id, {app_calculator->getName(), "calculator", "calc", "计算器", "计算器应用", "计算器app"}},
            {app_ai_profile_id, {app_ai_profile->getName(), "AI profile", "ai 配置", "ai配置", "ai设置", "ai设置应用", "ai设置app"}},
            {app_timer_id, {app_timer->getName(), "timer", "时钟", "时钟应用", "时钟app"}}
        };

        for (const auto &param : params) {
            if (param.name() == "app_name") {
                auto app_name = param.string();
                ESP_UTILS_LOGI("Opening app: %s", app_name.c_str());

                ESP_Brookesia_CoreAppEventData_t event_data = {
                    .id = -1,
                    .type = ESP_BROOKESIA_CORE_APP_EVENT_TYPE_START,
                    .data = nullptr
                };
                auto target_name = to_lower(get_before_space(app_name));
                for (const auto &pair : app_name_map) {
                    if (std::find(pair.second.begin(), pair.second.end(), target_name) != pair.second.end()) {
                        event_data.id = pair.first;
                        break;
                    }
                }

                if (event_data.id == -1) {
                    ESP_UTILS_LOGW("App name not found");
                    return;
                }

                boost::this_thread::sleep_for(boost::chrono::milliseconds(FUNCTION_OPEN_APP_WAIT_SPEAKING_PRE_MS));

                int wait_count = 0;
                int wait_interval_ms = FUNCTION_OPEN_APP_WAIT_SPEAKING_INTERVAL_MS;
                int wait_max_count = FUNCTION_OPEN_APP_WAIT_SPEAKING_MAX_MS / wait_interval_ms;
                while ((wait_count < wait_max_count) && AI_Buddy::requestInstance()->isSpeaking()) {
                    boost::this_thread::sleep_for(boost::chrono::milliseconds(wait_interval_ms));
                    wait_count++;
                }

                speaker->lockLv();
                speaker->manager.processDisplayScreenChange(
                    ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_MAIN, nullptr
                );
                speaker->sendAppEvent(&event_data);
                speaker->unlockLv();
            }
        }
    }, std::make_optional<FunctionDefinition::CallbackThreadConfig>({
        .name = FUNCTION_OPEN_APP_THREAD_NAME,
        .stack_size = FUNCTION_OPEN_APP_THREAD_STACK_SIZE,
        .stack_in_ext = FUNCTION_OPEN_APP_THREAD_STACK_CAPS_EXT,
    }));
    FunctionDefinitionList::requestInstance().addFunction(openApp);

    FunctionDefinition setVolume("set_volume", "Adjust the system volume. Range is from 0 to 100.");
    setVolume.addParameter("level", "The desired volume level (0 to 100).", FunctionParameter::ValueType::String);
    setVolume.setCallback([ = ](const std::vector<FunctionParameter> &params) {
        ESP_UTILS_LOG_TRACE_GUARD();

        auto ai_buddy = AI_Buddy::requestInstance();
        ESP_UTILS_CHECK_NULL_EXIT(ai_buddy, "Failed to get ai buddy instance");

        for (const auto &param : params) {
            if (param.name() == "level") {
                int last_volume = get_media_sound_volume();
                int volume = atoi(param.string().c_str());

                if (volume < 0) {
                    volume = last_volume - FUNCTION_VOLUME_CHANGE_STEP;
                    if (volume <= 0) {
                        ESP_UTILS_CHECK_FALSE_EXIT(
                            ai_buddy->expression.setSystemIcon("volume_mute"),
                            "Failed to set volume mute icon"
                        );
                    } else {
                        ESP_UTILS_CHECK_FALSE_EXIT(
                            ai_buddy->expression.setSystemIcon("volume_down"), "Failed to set volume down icon"
                        );
                    }
                } else if (volume > 100) {
                    volume = last_volume + FUNCTION_VOLUME_CHANGE_STEP;
                    ESP_UTILS_CHECK_FALSE_EXIT(
                        ai_buddy->expression.setSystemIcon("volume_up"), "Failed to set volume up icon"
                    );
                }
                ESP_UTILS_CHECK_FALSE_EXIT(set_media_sound_volume(volume), "Failed to set volume");
            }
        }
    }, std::make_optional<FunctionDefinition::CallbackThreadConfig>(FunctionDefinition::CallbackThreadConfig{
        .name = FUNCTION_VOLUME_CHANGE_THREAD_NAME,
        .stack_size = FUNCTION_VOLUME_CHANGE_THREAD_STACK_SIZE,
        .stack_in_ext = FUNCTION_VOLUME_CHANGE_THREAD_STACK_CAPS_EXT,
    }));
    FunctionDefinitionList::requestInstance().addFunction(setVolume);

    FunctionDefinition setBrightness("set_brightness", "Adjust the system brightness. Range is from 10 to 100.");
    setBrightness.addParameter("level", "The desired brightness level (10 to 100).", FunctionParameter::ValueType::String);
    setBrightness.setCallback([ = ](const std::vector<FunctionParameter> &params) {
        ESP_UTILS_LOG_TRACE_GUARD();

        auto ai_buddy = AI_Buddy::requestInstance();
        ESP_UTILS_CHECK_NULL_EXIT(ai_buddy, "Failed to get ai buddy instance");

        for (const auto &param : params) {
            if (param.name() == "level") {
                int last_brightness = get_media_display_brightness();
                int brightness = atoi(param.string().c_str());

                if (brightness < 0) {
                    brightness = last_brightness - FUNCTION_BRIGHTNESS_CHANGE_STEP;
                    ESP_UTILS_CHECK_FALSE_EXIT(
                        ai_buddy->expression.setSystemIcon("brightness_down"), "Failed to set brightness down icon"
                    );
                } else if (brightness > 100) {
                    brightness = last_brightness + FUNCTION_BRIGHTNESS_CHANGE_STEP;
                    ESP_UTILS_CHECK_FALSE_EXIT(
                        ai_buddy->expression.setSystemIcon("brightness_up"), "Failed to set brightness up icon"
                    );
                }
                ESP_UTILS_CHECK_FALSE_EXIT(set_media_display_brightness(brightness), "Failed to set brightness");
            }
        }
    }, std::make_optional<FunctionDefinition::CallbackThreadConfig>(FunctionDefinition::CallbackThreadConfig{
        .name = FUNCTION_BRIGHTNESS_CHANGE_THREAD_NAME,
        .stack_size = FUNCTION_BRIGHTNESS_CHANGE_THREAD_STACK_SIZE,
        .stack_in_ext = FUNCTION_BRIGHTNESS_CHANGE_THREAD_STACK_CAPS_EXT,
    }));
    FunctionDefinitionList::requestInstance().addFunction(setBrightness);

    // /* Connect the quick settings event signal */
    // speaker->getDisplay().getQuickSettings().on_event_signal.connect([=](QuickSettings::EventType event_type) {
    //     if ((event_type != QuickSettings::EventType::SettingsButtonClicked) &&
    //         (speaker->getCoreManager().getRunningAppById(app_settings_id) != nullptr)) {
    //         return true;
    //     }

    //     ESP_Brookesia_CoreAppEventData_t event_data = {
    //         .id = app_settings_id,
    //         .type = ESP_BROOKESIA_CORE_APP_EVENT_TYPE_START,
    //         .data = nullptr
    //     };
    //     speaker->sendAppEvent(&event_data);

    //     return true;
    // });

    /* Create a timer to update the clock */
    // ESP_UTILS_CHECK_NULL_EXIT(lv_timer_create(on_clock_update_timer_cb, 1000, speaker), "Create clock update timer failed");

    return true;
}
