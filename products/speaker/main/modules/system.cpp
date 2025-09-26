/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string>
#include "soc/soc_caps.h"
#if SOC_USB_SERIAL_JTAG_SUPPORTED
#   include "soc/usb_serial_jtag_reg.h"
#   include "hal/usb_serial_jtag_ll.h"
#endif
#include "esp_private/usb_phy.h"
#include "bsp/esp-bsp.h"
#include "esp_brookesia.hpp"
#include "esp_brookesia_app_settings.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "System"
#include "esp_lib_utils.h"
#include "coze_agent_config.h"
#include "coze_agent_config_default.h"
#include "usb_msc.h"
#include "system.hpp"

#include "battery_monitor.h"
#include "imu_gesture.h"
#include "touch_sensor.h"
#include "led_indicator.h"

constexpr const char *FUNCTION_OPEN_APP_THREAD_NAME               = "open_app";
constexpr int         FUNCTION_OPEN_APP_THREAD_STACK_SIZE         = 20 * 1024;
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

constexpr int         DEVELOPER_MODE_KEY = 0x655;

using namespace esp_brookesia;
using namespace esp_brookesia::systems::speaker;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::apps;
using namespace esp_brookesia::services;
using namespace esp_brookesia::ai_framework;

/**
 * This variable is used to store a special key which indicates whether to enter developer mode.
 * When the device is rebooted by software, this variable will not be initialized.
 */
static RTC_NOINIT_ATTR int developer_mode_key;
#if COZE_AGENT_ENABLE_DEFAULT_CONFIG
extern const char private_key_pem_start[] asm("_binary_private_key_pem_start");
extern const char private_key_pem_end[]   asm("_binary_private_key_pem_end");
#endif
static BatteryMonitor battery_monitor;
static IMUGesture imu_gesture;
static TouchSensor touch_sensor;

static std::string to_lower(const std::string &input);
static std::string get_before_space(const std::string &input);
static bool load_coze_agent_config();
static bool check_whether_enter_developer_mode();
static void touch_btn_event_cb(void *button_handle, void *usr_data);
static void touch_sensor_switch();
static void show_low_power(Speaker *speaker);
static void update_battery_info(Speaker *speaker, const Settings *app_settings);

bool system_init()
{
    ESP_UTILS_LOG_TRACE_GUARD();

    /* Create a speaker object */
    Speaker *speaker = nullptr;
    ESP_UTILS_CHECK_EXCEPTION_RETURN(
        speaker = new Speaker(), false, "Create speaker failed"
    );

    battery_monitor.setBatteryShutdownCallback([speaker]() {
        show_low_power(speaker);
    });
    ESP_UTILS_CHECK_FALSE_RETURN(battery_monitor.init(), false, "Battery monitor init failed");
    ESP_UTILS_CHECK_FALSE_RETURN(imu_gesture.init(), false, "IMU gesture init failed");
    ESP_UTILS_CHECK_FALSE_RETURN(touch_sensor.init(), false, "Touch sensor init failed");
    ESP_UTILS_CHECK_FALSE_RETURN(check_whether_enter_developer_mode(), false, "Check whether enter developer mode failed");

    /* Load coze agent config */
    if (!load_coze_agent_config()) {
        ESP_UTILS_LOGE("Load coze agent config failed");
    }

    /* Try using a stylesheet that corresponds to the resolution */
    std::unique_ptr<Stylesheet> stylesheet;
    ESP_UTILS_CHECK_EXCEPTION_RETURN(
        stylesheet = std::make_unique<Stylesheet>(ESP_BROOKESIA_SPEAKER_360_360_DARK_STYLESHEET), false,
        "Create stylesheet failed"
    );
    ESP_UTILS_LOGI("Using stylesheet (%s)", stylesheet->core.name);
    ESP_UTILS_CHECK_FALSE_RETURN(speaker->addStylesheet(stylesheet.get()), false, "Add stylesheet failed");
    ESP_UTILS_CHECK_FALSE_RETURN(speaker->activateStylesheet(stylesheet.get()), false, "Activate stylesheet failed");
    stylesheet = nullptr;

    /* Begin the speaker */
    LvLockGuard gui_guard;
    ESP_UTILS_CHECK_FALSE_RETURN(speaker->begin(), false, "Begin failed");

    /* Init app from registry */
    std::vector<systems::base::Manager::RegistryAppInfo> inited_apps;
    ESP_UTILS_CHECK_FALSE_RETURN(speaker->initAppFromRegistry(inited_apps), false, "Init app registry failed");

    /* Process apps */
    Settings *app_settings = nullptr;
    for (const auto &[app_name, app] : inited_apps) {
        if (app_name == "Settings") { // Settings
            auto app_settings_ptr = std::dynamic_pointer_cast<Settings>(app);
            ESP_UTILS_CHECK_NULL_RETURN(app_settings_ptr, false, "Failed to get app settings");

            app_settings = app_settings_ptr.get();
        }
    }
    // Settings
    if (app_settings) {
        std::unique_ptr<SettingsStylesheetData> app_settings_stylesheet;
        ESP_UTILS_CHECK_EXCEPTION_RETURN(
            app_settings_stylesheet = std::make_unique<SettingsStylesheetData>(SETTINGS_UI_360_360_STYLESHEET_DARK()),
            false, "Create app settings stylesheet failed"
        );
        app_settings_stylesheet->screen_size = StyleSize::RECT_PERCENT(100, 100);
        app_settings_stylesheet->manager.wlan.scan_ap_count_max = 30;
        app_settings_stylesheet->manager.wlan.scan_interval_ms = 10000;
#if CONFIG_BSP_PCB_VERSION_V1_0
        app_settings_stylesheet->manager.about.device_board_name = "EchoEar V1.0";
#elif CONFIG_BSP_PCB_VERSION_V1_2
        app_settings_stylesheet->manager.about.device_board_name = "EchoEar V1.2";
#else
        app_settings_stylesheet->manager.about.device_board_name = "EchoEar";
#endif
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
        app_settings->manager.event_signal.connect([ = ](SettingsManager::EventType event_type, SettingsManager::EventData event_data) {
            ESP_UTILS_LOGD("Param: event_type(%d), event_data(%s)", static_cast<int>(event_type), event_data.type().name());

            switch (event_type) {
            case SettingsManager::EventType::EnterDeveloperMode: {
                ESP_UTILS_CHECK_FALSE_RETURN(
                    event_data.type() == typeid(SettingsManager::EventDataEnterDeveloperMode), false,
                    "Invalid developer mode type"
                );

                ESP_UTILS_LOGW("Enter developer mode");
                developer_mode_key = DEVELOPER_MODE_KEY;
                esp_restart();
                break;
            }
            case SettingsManager::EventType::EnterScreen: {
                ESP_UTILS_CHECK_FALSE_RETURN(
                    event_data.type() == typeid(SettingsManager::EventDataEnterScreenIndex), false,
                    "Invalid developer mode type"
                );
                auto screen_index = std::any_cast<SettingsManager::EventDataEnterScreenIndex>(event_data);
                if (screen_index == SettingsManager::UI_Screen::MORE_ABOUT) {
                    // update about info immediately
                    update_battery_info(speaker, app_settings);
                }
                break;
            }
            default:
                return false;
            }

            return true;
        });
    }

    /* Install app from registry */
    // The app will be installed in the order of the vector, this determines the order of app icons in the main interface
    std::vector<std::string> ordered_app_names = {"Settings", "AI_Profile", "2048", "Calculator", "Timer", "Pos", "UsbdNcm"};
    ESP_UTILS_CHECK_FALSE_RETURN(
        speaker->installAppFromRegistry(inited_apps, &ordered_app_names), false, "Install app registry failed"
    );

    /* Register agent function calling */
    FunctionDefinition openApp("open_app", "Open a specific app.打开一个应用");
    openApp.addParameter("app_name", "The name of the app to open.应用名称", FunctionParameter::ValueType::String);
    openApp.setCallback([ = ](const std::vector<FunctionParameter> &params) {
        ESP_UTILS_LOG_TRACE_GUARD();

        static auto all_apps = inited_apps;
        static std::map<std::string, std::vector<std::string>> app_name_map = {
            {"Settings",   {"setting", "settings", "设置", "设置应用", "设置app"}},
            {"2048",       {"2048", "game", "游戏", "2048游戏", "2048app"}},
            {"Calculator", {"calculator", "calc", "计算器", "计算器应用", "计算器app"}},
            {"AI_Profile", {"aiprofile", "ai配置", "ai配置", "ai设置", "ai设置应用", "ai设置app"}},
            {"Timer",      {"timer", "时钟", "时钟应用", "时钟app"}},
            {"Pos",        {"pos", "pos应用", "posapp"}},
            {"UsbdNcm",    {"usbdncm", "usb网卡", "usbdncmapp"}}
        };
        auto get_app_id = [&](const std::string & target_name) -> int {
            for (const auto &[app_name, alias_names] : app_name_map)
            {
                if (std::find(alias_names.begin(), alias_names.end(), target_name) == alias_names.end()) {
                    continue;
                }

                for (const auto &[name, app] : all_apps) {
                    if (name == app_name) {
                        return app->getId();
                    }
                }

                break;
            }
            return -1;
        };

        for (const auto &param : params) {
            if (param.name() == "app_name") {
                auto target_name = param.string();
                systems::base::Context::AppEventData event_data = {
                    .id = get_app_id(to_lower(get_before_space(target_name))),
                    .type = systems::base::Context::AppEventType::START,
                    .data = nullptr
                };
                ESP_UTILS_CHECK_FALSE_EXIT(speaker->checkAppID_Valid(event_data.id), "App not found");

                ESP_UTILS_LOGI("Opening app(%s, %d)", target_name.c_str(), event_data.id);

                boost::this_thread::sleep_for(boost::chrono::milliseconds(FUNCTION_OPEN_APP_WAIT_SPEAKING_PRE_MS));

                int wait_count = 0;
                int wait_interval_ms = FUNCTION_OPEN_APP_WAIT_SPEAKING_INTERVAL_MS;
                int wait_max_count = FUNCTION_OPEN_APP_WAIT_SPEAKING_MAX_MS / wait_interval_ms;
                while ((wait_count < wait_max_count) && AI_Buddy::requestInstance()->isSpeaking()) {
                    boost::this_thread::sleep_for(boost::chrono::milliseconds(wait_interval_ms));
                    wait_count++;
                }

                LvLockGuard gui_guard;
                speaker->getManager().processDisplayScreenChange(
                    Manager::Screen::MAIN, nullptr
                );
                speaker->sendAppEvent(&event_data);
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
                StorageNVS::Value value;
                ESP_UTILS_CHECK_FALSE_EXIT(
                    StorageNVS::requestInstance().getLocalParam(Manager::SETTINGS_VOLUME, value),
                    "Get media sound volume failed"
                );

                int last_volume = std::get<int>(value);
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
                ESP_UTILS_CHECK_FALSE_EXIT(
                    StorageNVS::requestInstance().setLocalParam(Manager::SETTINGS_VOLUME, volume), "Failed to set volume"
                );
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
                StorageNVS::Value value;
                ESP_UTILS_CHECK_FALSE_EXIT(
                    StorageNVS::requestInstance().getLocalParam(Manager::SETTINGS_BRIGHTNESS, value),
                    "Get media display brightness failed"
                );

                int last_brightness = std::get<int>(value);
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
                ESP_UTILS_CHECK_FALSE_EXIT(
                    StorageNVS::requestInstance().setLocalParam(Manager::SETTINGS_BRIGHTNESS, brightness),
                    "Failed to set brightness"
                );
            }
        }
    }, std::make_optional<FunctionDefinition::CallbackThreadConfig>(FunctionDefinition::CallbackThreadConfig{
        .name = FUNCTION_BRIGHTNESS_CHANGE_THREAD_NAME,
        .stack_size = FUNCTION_BRIGHTNESS_CHANGE_THREAD_STACK_SIZE,
        .stack_in_ext = FUNCTION_BRIGHTNESS_CHANGE_THREAD_STACK_CAPS_EXT,
    }));
    FunctionDefinitionList::requestInstance().addFunction(setBrightness);

    /* Process quick settings */
    speaker->getDisplay().getQuickSettings().connectEventSignal([ = ](QuickSettings::EventData event_data) {
        ESP_UTILS_LOG_TRACE_GUARD();

        auto &type = event_data.type;
        std::optional<SettingsManager::AppOperationData> operation_data = {};
        switch (type) {
        case QuickSettings::EventType::WifiButtonLongPressed: {
            ESP_UTILS_LOGI("Wifi button long pressed");
            operation_data = SettingsManager::AppOperationData{
                .code = SettingsManager::AppOperationCode::EnterScreen,
                .payload = SettingsManager::UI_Screen::WIRELESS_WLAN,
            };
            break;
        }
        case QuickSettings::EventType::VolumeButtonLongPressed: {
            ESP_UTILS_LOGI("Volume button long pressed");
            operation_data = SettingsManager::AppOperationData{
                .code = SettingsManager::AppOperationCode::EnterScreen,
                .payload = SettingsManager::UI_Screen::MEDIA_SOUND,
            };
            break;
        }
        case QuickSettings::EventType::BrightnessButtonLongPressed: {
            ESP_UTILS_LOGI("Brightness button long pressed");
            operation_data = SettingsManager::AppOperationData{
                .code = SettingsManager::AppOperationCode::EnterScreen,
                .payload = SettingsManager::UI_Screen::MEDIA_DISPLAY,
            };
            break;
        }
        default:
            break;
        }

        if (operation_data.has_value() && (app_settings != nullptr)) {
            auto &operation_data_value = operation_data.value();
            systems::base::Context::AppEventData event_data = {
                .id = app_settings->getId(),
                .type = systems::base::Context::AppEventType::OPERATION,
                .data = &operation_data_value,
            };
            ESP_UTILS_CHECK_FALSE_EXIT(speaker->sendAppEvent(&event_data), "Send app event failed");
        }
    });

    /* Process touch sensor */
    auto &storage_service = StorageNVS::requestInstance();
    storage_service.connectEventSignal([](const StorageNVS::Event & event) {
        if ((event.operation != StorageNVS::Operation::UpdateNVS) || (event.key != SETTINGS_NVS_KEY_TOUCH_SENSOR_SWITCH)) {
            return;
        }
        ESP_UTILS_LOG_TRACE_GUARD();
        touch_sensor_switch();
    });
    touch_sensor_switch(); // update touch switch

    /* Bind imu gesture to expression */
    auto ai_buddy = AI_Buddy::requestInstance();
    ESP_UTILS_CHECK_NULL_RETURN(ai_buddy, false, "Failed to get ai buddy instance");
    imu_gesture.gesture_signal.connect([ai_buddy](IMUGesture::GestureType type) {
        if (type == IMUGesture::GestureType::ANY_MOTION) {
            ESP_UTILS_CHECK_FALSE_EXIT(ai_buddy->expression.insertEmojiTemporary("dizzy", 2500), "Set emoji failed");
        }
    });

    /* Process battery monitor */
    battery_monitor.setBatteryStatusCallback(
    [speaker](const BatteryStatus & status) {
        static BatteryStatus bat_last_status = {};
        if (bat_last_status.full != status.full) {
            bat_last_status = status;

            LvLockGuard gui_guard;
            auto &quick_settings = speaker->getDisplay().getQuickSettings();
            ESP_UTILS_CHECK_FALSE_EXIT(
                quick_settings.setBatteryPercent(!status.DSG, battery_monitor.getBatterySOC()),
                "Set battery percent failed"
            );
        }
    }
    );
    battery_monitor.setMonitorPeriodCallback([speaker, app_settings]() {
        update_battery_info(speaker, app_settings);
    });

    ESP_UTILS_CHECK_FALSE_RETURN(led_indicator_register_wifi_event(), false, "Failed to register wifi event");

    return true;
}

bool system_check_is_developer_mode()
{
    return (developer_mode_key == DEVELOPER_MODE_KEY);
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

    {
        LvLockGuard gui_guard;

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
    }

    led_indicator_start(led_indicator_handle, BLINK_DEVELOP_MODE);
    // mount_wl_basic_and_tusb();
    ESP_UTILS_CHECK_ERROR_RETURN(usb_msc_mount(), false, "Mount USB MSC failed");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void touch_sensor_switch()
{
    static bool is_callback_registered = false;

    auto &storage_service = StorageNVS::requestInstance();
    StorageNVS::Value value;
    ESP_UTILS_CHECK_FALSE_EXIT(
        storage_service.getLocalParam(SETTINGS_NVS_KEY_TOUCH_SENSOR_SWITCH, value), "Get NVS volume failed"
    );

    auto enable = std::get<int>(value);
    ESP_UTILS_LOGI("switch touch to %d", enable);
    button_handle_t btn = touch_sensor.get_button_handle();
    if (enable && !is_callback_registered) {
        ESP_UTILS_CHECK_FALSE_EXIT(ESP_OK == iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, touch_btn_event_cb, NULL), "Failed to register button event callback");
        ESP_UTILS_CHECK_FALSE_EXIT(ESP_OK == iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, NULL, touch_btn_event_cb, NULL), "Failed to register button event callback");
        ESP_UTILS_CHECK_FALSE_EXIT(ESP_OK == iot_button_register_cb(btn, BUTTON_PRESS_DOWN, NULL, touch_btn_event_cb, NULL), "Failed to register button event callback");
        ESP_UTILS_CHECK_FALSE_EXIT(ESP_OK == iot_button_register_cb(btn, BUTTON_PRESS_UP, NULL, touch_btn_event_cb, NULL), "Failed to register button event callback");
        is_callback_registered = true;
    } else if (!enable && is_callback_registered) {
        ESP_UTILS_CHECK_FALSE_EXIT(ESP_OK == iot_button_unregister_cb(btn, BUTTON_SINGLE_CLICK, NULL), "Failed to unregister button event callback");
        ESP_UTILS_CHECK_FALSE_EXIT(ESP_OK == iot_button_unregister_cb(btn, BUTTON_LONG_PRESS_START, NULL), "Failed to unregister button event callback");
        ESP_UTILS_CHECK_FALSE_EXIT(ESP_OK == iot_button_unregister_cb(btn, BUTTON_PRESS_DOWN, NULL), "Failed to unregister button event callback");
        ESP_UTILS_CHECK_FALSE_EXIT(ESP_OK == iot_button_unregister_cb(btn, BUTTON_PRESS_UP, NULL), "Failed to unregister button event callback");
        is_callback_registered = false;
    }
}

static void touch_btn_event_cb(void *button_handle, void *usr_data)
{
    button_event_t event = iot_button_get_event((button_handle_t)button_handle);

    auto _agent = Agent::requestInstance();
    auto ai_buddy = AI_Buddy::requestInstance();
    if (!ai_buddy || !_agent || ai_buddy->isPause()) {
        return;
    }
    switch (event) {
    case BUTTON_PRESS_DOWN:
        led_indicator_start(led_indicator_handle, BLINK_TOUCH_PRESS_DOWN);
        break;
    case BUTTON_PRESS_UP:
        led_indicator_stop(led_indicator_handle, BLINK_TOUCH_PRESS_DOWN);
        break;
    case BUTTON_SINGLE_CLICK:
        if (_agent->hasChatState(Agent::ChatState::ChatStateStarted)) {
            if (_agent->isChatState(Agent::ChatState::ChatStateSlept)) {
                ESP_UTILS_LOGI("Chat Wake up");
                audio_gmf_trigger_wakeup();
            } else if (ai_buddy->isSpeaking()) {
                ESP_UTILS_LOGI("Chat interrupt");
                coze_chat_response_signal();
                coze_chat_app_interrupt();
            }
        } else {
            ESP_UTILS_LOGI("Chat nothing to do");
        }
        break;
    case BUTTON_LONG_PRESS_START:
        if (_agent->hasChatState(Agent::ChatState::ChatStateStarted) && !_agent->isChatState(Agent::ChatState::ChatStateSlept)) {
            ESP_UTILS_LOGI("Chat Sleep");
            ai_buddy->sendAudioEvent({AI_Buddy::AudioType::SleepBaiBaiLo});
            ESP_UTILS_CHECK_FALSE_EXIT(_agent->sendChatEvent(Agent::ChatEvent::Sleep), "Send chat event sleep failed");
        }
        break;
    default:
        break;
    }
}

static void show_low_power(Speaker *speaker)
{
    ESP_UTILS_LOGW("Low power triggered");

    {
        LvLockGuard gui_guard;

        lv_obj_t *low_batt_scr = lv_obj_create(NULL);
        auto title_label = lv_label_create(low_batt_scr);
        lv_obj_set_size(title_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_text_font(title_label, &esp_brookesia_font_maison_neue_book_30, 0);
        lv_label_set_text(title_label, "Low Power");
        lv_obj_set_style_text_color(title_label, lv_color_make(255, 0, 0), 0);
        lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 60);

        auto content_label = lv_label_create(low_batt_scr);
        lv_obj_set_size(content_label, LV_PCT(80), LV_SIZE_CONTENT);
        lv_obj_set_style_text_font(content_label, &esp_brookesia_font_maison_neue_book_20, 0);
        lv_obj_set_style_text_align(content_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(content_label, "The battery is low. Device will sleep soon.\n"
                          "Please connect the device to a power source to charge it.");
        lv_obj_align_to(content_label, title_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
        lv_scr_load(low_batt_scr);

        auto &quick_settings = speaker->getDisplay().getQuickSettings();
        quick_settings.setVisible(false);
    }

    auto ai_buddy = AI_Buddy::requestInstance();
    if (ai_buddy) {
        if (ai_buddy->expression.pause()) {
            vTaskDelay(pdMS_TO_TICKS(1300));
        }
    }

    Display::on_dummy_draw_signal(false); // Disable dummy draw to enable LVGL
    StorageNVS::Value volume_value;
    StorageNVS::requestInstance().getLocalParam(Manager::SETTINGS_VOLUME, volume_value);
    StorageNVS::requestInstance().setLocalParam(Manager::SETTINGS_VOLUME, 65); // set volume to 65%
    audio_prompt_play_with_block("file://spiffs/low_power.mp3", 1500);
    led_indicator_start(led_indicator_handle, BLINK_LOW_POWER);
    vTaskDelay(pdMS_TO_TICKS(4000));
    led_indicator_stop(led_indicator_handle, BLINK_LOW_POWER);
    StorageNVS::requestInstance().setLocalParam(Manager::SETTINGS_VOLUME, volume_value); // restore volume
    vTaskDelay(pdMS_TO_TICKS(100)); // ensure storage nvs write completed
    bsp_set_peripheral_power(false); // board peripheral off
    ESP_UTILS_LOGW("Low power triggered, device will sleep now");
}

static void update_battery_info(Speaker *speaker, const Settings *app_settings)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    LvLockGuard gui_guard;

    auto &quick_settings = speaker->getDisplay().getQuickSettings();
    ESP_UTILS_CHECK_FALSE_EXIT(
        quick_settings.setBatteryPercent(battery_monitor.is_charging(), battery_monitor.getBatterySOC()),
        "Set battery percent failed"
    );

    char battery_info_str[32] = {0};
    auto _cell = app_settings->ui.screen_about.getCell(
                     static_cast<int>(SettingsUI_ScreenAboutContainerIndex::DEVICE),
                     static_cast<int>(SettingsUI_ScreenAboutCellIndex::DEVICE_BATTERY_CAPACITY)
                 );
    if (_cell) {
        snprintf(battery_info_str, sizeof(battery_info_str), "%dmAh", battery_monitor.getCapacity());
        _cell->updateRightMainLabel(battery_info_str);
    }

    _cell = app_settings->ui.screen_about.getCell(
                static_cast<int>(SettingsUI_ScreenAboutContainerIndex::DEVICE),
                static_cast<int>(SettingsUI_ScreenAboutCellIndex::DEVICE_BATTERY_VOLTAGE)
            );
    if (_cell) {
        snprintf(battery_info_str, sizeof(battery_info_str), "%dmV", battery_monitor.getVoltage());
        _cell->updateRightMainLabel(battery_info_str);
    }

    _cell = app_settings->ui.screen_about.getCell(
                static_cast<int>(SettingsUI_ScreenAboutContainerIndex::DEVICE),
                static_cast<int>(SettingsUI_ScreenAboutCellIndex::DEVICE_BATTERY_CURRENT)
            );
    if (_cell) {
        snprintf(battery_info_str, sizeof(battery_info_str), "%dmA", battery_monitor.getCurrent());
        _cell->updateRightMainLabel(battery_info_str);
    }
}

void restart_usb_serial_jtag()
{
    gpio_config_t io_usb_power_conf = {
        .pin_bit_mask = (1ULL << BSP_USB_DP),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_usb_power_conf);
    gpio_set_level(BSP_USB_DP, 0);
    _usb_serial_jtag_phy_init();
}
