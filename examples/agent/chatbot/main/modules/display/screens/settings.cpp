/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "esp_system.h"
#include "esp_lv_adapter.h"
#include "private/utils.hpp"
#include "modules/display/display.hpp"
#include "modules/display/squareline_ui/ui.h"
#include "brookesia/agent_helper.hpp"
#include "brookesia/service_helper.hpp"
#include "settings.hpp"

using namespace esp_brookesia;
using AgentHelper = agent::helper::Manager;
using WifiHelper = service::helper::Wifi;
using DeviceHelper = service::helper::Device;

constexpr uint8_t VOLUME_MAX = 100;
constexpr uint8_t VOLUME_MIN = 0;
constexpr uint8_t BRIGHTNESS_MAX = 100;
constexpr uint8_t BRIGHTNESS_MIN = 0;

ScreenSettings::ScreenSettings():
    StateBase(BROOKESIA_DESCRIBE_TO_STR(DisplayScreen::Settings))
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    init_screen_ = lv_screen_active();
    BROOKESIA_CHECK_NULL_EXIT(init_screen_, "Failed to get active screen");

    ui_ScreenSettings_screen_init();

    auto get_device_capabilities_result = DeviceHelper::call_function_sync<boost::json::object>(
            DeviceHelper::FunctionId::GetCapabilities
                                          );
    BROOKESIA_CHECK_FALSE_EXIT(
        get_device_capabilities_result, "Failed to get device capabilities: %1%", get_device_capabilities_result.error()
    );
    DeviceHelper::Capabilities device_capabilities;
    auto convert_result = BROOKESIA_DESCRIBE_FROM_JSON(get_device_capabilities_result.value(), device_capabilities);
    BROOKESIA_CHECK_FALSE_EXIT(convert_result, "Failed to convert device capabilities");

    BROOKESIA_CHECK_FALSE_EXECUTE(init_wifi(), {}, {
        BROOKESIA_LOGE("Failed to init wifi");
    });
    if (device_capabilities.find(std::string(hal::DisplayBacklightIface::NAME)) != device_capabilities.end()) {
        is_backlight_available_ = true;
        BROOKESIA_CHECK_FALSE_EXECUTE(init_brightness(), {}, {
            BROOKESIA_LOGE("Failed to init brightness");
        });
    }
    if (device_capabilities.find(std::string(hal::AudioCodecPlayerIface::NAME)) != device_capabilities.end()) {
        is_volume_available_ = true;
        BROOKESIA_CHECK_FALSE_EXECUTE(init_volume(), {}, {
            BROOKESIA_LOGE("Failed to init volume");
        });
    }
    BROOKESIA_CHECK_FALSE_EXECUTE(init_agent(), {}, {
        BROOKESIA_LOGE("Failed to init agent");
    });
    BROOKESIA_CHECK_FALSE_EXECUTE(init_reset(), {}, {
        BROOKESIA_LOGE("Failed to init reset");
    });
}

ScreenSettings::~ScreenSettings()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ui_ScreenSettings_screen_destroy();

    brightness_changed_connection_.disconnect();
    volume_changed_connection_.disconnect();
}

bool ScreenSettings::on_enter(const std::string &from_state, const std::string &action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI("Entering '%1%' from '%2%' with action '%3%'", get_name(), from_state, action);

    lv_screen_load(ui_ScreenSettings);

    BROOKESIA_CHECK_FALSE_EXECUTE(enter_process_brightness(), {}, {
        BROOKESIA_LOGE("Failed to process brightness");
    });
    BROOKESIA_CHECK_FALSE_EXECUTE(enter_process_volume(), {}, {
        BROOKESIA_LOGE("Failed to process volume");
    });
    BROOKESIA_CHECK_FALSE_EXECUTE(enter_process_agent(), {}, {
        BROOKESIA_LOGE("Failed to enter process agent");
    });

    lv_display_t *lv_disp = lv_display_get_default();
    BROOKESIA_CHECK_NULL_RETURN(lv_disp, false, "Failed to get default display");
    auto ret = esp_lv_adapter_set_dummy_draw(lv_disp, false);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to set dummy draw");

    return true;
}

bool ScreenSettings::on_exit(const std::string &to_state, const std::string &action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI("Exiting '%1%' to '%2%' with action '%3%'", get_name(), to_state, action);

    lv_screen_load(init_screen_);

    brightness_changed_connection_.disconnect();
    volume_changed_connection_.disconnect();

    BROOKESIA_CHECK_FALSE_EXECUTE(exit_process_agent(), {}, {
        BROOKESIA_LOGE("Failed to exit process agent");
    });

    return true;
}

bool ScreenSettings::init_wifi()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(WifiHelper::is_available(), false, "Wifi helper is not available");

    auto general_action_triggered_slot =
    [this](const std::string & event_name, const std::string & general_action) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD("Params: event_name(%1%), general_action(%2%)", event_name, general_action);

        WifiHelper::GeneralAction general_action_enum;
        auto convert_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(general_action, general_action_enum);
        BROOKESIA_CHECK_FALSE_EXIT(convert_result, "Failed to convert general action: %1%", general_action);

        std::string message = "";

        switch (general_action_enum) {
        case WifiHelper::GeneralAction::Start:
            message = "Starting...";
            break;
        case WifiHelper::GeneralAction::Connect: {
            auto get_ap_handler = [this](service::FunctionResult && result) {
                BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

                BROOKESIA_CHECK_FALSE_EXIT(result.success, "Failed to get connect AP: %1%", result.error_message);

                auto &data = result.get_data<boost::json::object>();
                WifiHelper::ConnectApInfo connect_ap_info;
                auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(data, connect_ap_info);
                BROOKESIA_CHECK_FALSE_EXIT(parse_result, "Failed to parse connect AP info");

                auto message = (boost::format("Connecting to %1% ...") % connect_ap_info.ssid).str();
                wifi_ssid_ = connect_ap_info.ssid;

                auto send_result = Display::get_instance().send_display_task([message]() {
                    lv_label_set_text(ui_SettingsLabelWifiSsidText, message.c_str());
                });
                BROOKESIA_CHECK_FALSE_EXIT(send_result, "Failed to send display task");
            };
            auto get_ap_result = WifiHelper::call_function_async(WifiHelper::FunctionId::GetConnectAp, get_ap_handler);
            BROOKESIA_CHECK_FALSE_EXIT(get_ap_result, "Failed to get connect AP");
            break;
        }
        case WifiHelper::GeneralAction::Disconnect:
            message = "Disconnecting...";
            break;
        case WifiHelper::GeneralAction::Stop:
            message = "Stopping...";
            break;
        default:
            break;
        }

        if (!message.empty()) {
            auto send_result = Display::get_instance().send_display_task([message]() {
                lv_label_set_text(ui_SettingsLabelWifiSsidText, message.c_str());
            });
            BROOKESIA_CHECK_FALSE_EXIT(send_result, "Failed to send display task");
        }
    };
    auto general_action_triggered_connection = WifiHelper::subscribe_event(
                WifiHelper::EventId::GeneralActionTriggered, general_action_triggered_slot
            );
    BROOKESIA_CHECK_FALSE_RETURN(
        general_action_triggered_connection.connected(), false, "Failed to subscribe to general action triggered event"
    );

    service_connections_.push_back(std::move(general_action_triggered_connection));
    auto general_event_happened_slot =
    [this](const std::string & event_name, const std::string & wifi_event, bool is_unexpected) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD(
            "Params: event_name(%1%), wifi_event(%2%), is_unexpected(%3%)", event_name, wifi_event, is_unexpected
        );

        WifiHelper::GeneralEvent wifi_event_enum;
        auto convert_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(wifi_event, wifi_event_enum);
        BROOKESIA_CHECK_FALSE_EXIT(convert_result, "Failed to convert wifi event: %1%", wifi_event);

        std::string message = "";

        switch (wifi_event_enum) {
        case WifiHelper::GeneralEvent::Connected: {
            message = wifi_ssid_;
            break;
        }
        case WifiHelper::GeneralEvent::Disconnected: {
            message = "Disconnected";
            break;
        }
        default:
            break;
        }

        if (!message.empty()) {
            auto send_result = Display::get_instance().send_display_task([message]() {
                lv_label_set_text(ui_SettingsLabelWifiSsidText, message.c_str());
            });
            BROOKESIA_CHECK_FALSE_EXIT(send_result, "Failed to send display task");
        }
    };
    auto general_event_happened_connection = WifiHelper::subscribe_event(
                WifiHelper::EventId::GeneralEventHappened, general_event_happened_slot
            );
    BROOKESIA_CHECK_FALSE_RETURN(
        general_event_happened_connection.connected(), false, "Failed to subscribe to general event happened event"
    );
    service_connections_.push_back(std::move(general_event_happened_connection));

    auto soft_ap_event_happened_slot =
    [this](const std::string & event_name, const std::string & soft_ap_event) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD("Params: event_name(%1%), soft_ap_event(%2%)", event_name, soft_ap_event);

        WifiHelper::SoftApEvent soft_ap_event_enum;
        auto convert_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(soft_ap_event, soft_ap_event_enum);
        BROOKESIA_CHECK_FALSE_EXIT(convert_result, "Failed to convert soft AP event: %1%", soft_ap_event);

        switch (soft_ap_event_enum) {
        case WifiHelper::SoftApEvent::Started: {
            auto get_soft_ap_params_handler = [this](service::FunctionResult && result) {
                BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

                BROOKESIA_CHECK_FALSE_EXIT(result.success, "Failed to get soft AP params: %1%", result.error_message);

                auto &data = result.get_data<boost::json::object>();
                WifiHelper::SoftApParams soft_ap_params;
                auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(data, soft_ap_params);
                BROOKESIA_CHECK_FALSE_EXIT(parse_result, "Failed to parse soft AP params");

                auto message =
                    (boost::format("SoftAP started. Please connect to %1%(PWD:%2%) or scan the QR code on the emote screen") %
                     soft_ap_params.ssid % soft_ap_params.password).str();
                auto send_result = Display::get_instance().send_display_task([message]() {
                    lv_label_set_text(ui_SettingsLabelWifiSsidText, message.c_str());
                });
                BROOKESIA_CHECK_FALSE_EXIT(send_result, "Failed to send display task");
            };
            auto get_soft_ap_params_result = WifiHelper::call_function_async(
                                                 WifiHelper::FunctionId::GetSoftApParams, get_soft_ap_params_handler
                                             );
            BROOKESIA_CHECK_FALSE_EXIT(get_soft_ap_params_result, "Failed to get soft AP params");
            break;
        }
        default:
            break;
        }
    };
    auto soft_ap_event_happened_connection = WifiHelper::subscribe_event(
                WifiHelper::EventId::SoftApEventHappened, soft_ap_event_happened_slot
            );
    BROOKESIA_CHECK_FALSE_RETURN(
        soft_ap_event_happened_connection.connected(), false, "Failed to subscribe to soft AP event happened event"
    );
    service_connections_.push_back(std::move(soft_ap_event_happened_connection));

    return true;
}

bool ScreenSettings::init_agent()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    lv_dropdown_clear_options(ui_SettingsDropdownAgents);

    BROOKESIA_CHECK_FALSE_RETURN(AgentHelper::is_running(), false, "Agent helper is not running");

    auto get_agent_names_result = AgentHelper::call_function_sync<boost::json::array>(
                                      AgentHelper::FunctionId::GetAgentNames
                                  );
    BROOKESIA_CHECK_FALSE_RETURN(
        get_agent_names_result, false, "Failed to get agent names: %1%", get_agent_names_result.error()
    );

    auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(get_agent_names_result.value(), agents_);
    BROOKESIA_CHECK_FALSE_RETURN(parse_result, false, "Failed to parse agent names");

    size_t index = 0;
    for (const auto &agent : agents_) {
        lv_dropdown_add_option(ui_SettingsDropdownAgents, agent.c_str(), index);
        index++;
    }

    auto get_target_agent_result = AgentHelper::call_function_sync<std::string>(AgentHelper::FunctionId::GetTargetAgent);
    BROOKESIA_CHECK_FALSE_RETURN(
        get_target_agent_result, false, "Failed to get target agent: %1%", get_target_agent_result.error()
    );

    target_agent_ = get_target_agent_result.value();
    if (!target_agent_.empty()) {
        auto it = std::find(agents_.begin(), agents_.end(), target_agent_);
        if (it != agents_.end()) {
            lv_dropdown_set_selected(ui_SettingsDropdownAgents, std::distance(agents_.begin(), it));
        }
    }

    auto agent_changed_slot = +[](lv_event_t *event) {
        BROOKESIA_LOG_TRACE_GUARD();

        auto *context = static_cast<ScreenSettings *>(lv_event_get_user_data(event));
        BROOKESIA_CHECK_NULL_EXIT(context, "Context is null");

        std::vector<char> agent_name(128);
        lv_dropdown_get_selected_str(
            ui_SettingsDropdownAgents, reinterpret_cast<char *>(agent_name.data()), agent_name.size()
        );
        auto agent_text = std::string(agent_name.data());

        if (agent_text == context->target_agent_) {
            BROOKESIA_LOGD("Agent is already active, skip");
            return;
        }

        context->target_agent_ = agent_text;

        BROOKESIA_LOGI("Agent changed to: %1%", agent_text);

        auto set_target_agent_result = AgentHelper::call_function_async(
                                           AgentHelper::FunctionId::SetTargetAgent, agent_text
                                       );
        BROOKESIA_CHECK_FALSE_EXIT(set_target_agent_result, "Failed to set target agent");
    };
    lv_obj_add_event_cb(ui_SettingsDropdownAgents, agent_changed_slot, LV_EVENT_VALUE_CHANGED, this);

    return true;
}

bool ScreenSettings::init_brightness()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto brightness_increase_slot = +[](lv_event_t *event) {
        BROOKESIA_LOG_TRACE_GUARD();
        auto *context = static_cast<ScreenSettings *>(lv_event_get_user_data(event));
        BROOKESIA_CHECK_NULL_EXIT(context, "Context is null");

        if (context->current_brightness_ >= BRIGHTNESS_MAX) {
            BROOKESIA_LOGW("Brightness is already at maximum: %1%", context->current_brightness_);
            return;
        }

        const auto brightness = context->current_brightness_ + 1;
        auto set_brightness_result = DeviceHelper::call_function_sync(
                                         DeviceHelper::FunctionId::SetDisplayBacklightBrightness, brightness
                                     );
        BROOKESIA_CHECK_FALSE_EXIT(set_brightness_result, "Failed to set brightness");
        context->current_brightness_ = brightness;
    };
    lv_obj_add_event_cb(ui_SettingsButtonBrightnessInc, brightness_increase_slot, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_SettingsButtonBrightnessInc, brightness_increase_slot, LV_EVENT_LONG_PRESSED, this);
    lv_obj_add_event_cb(ui_SettingsButtonBrightnessInc, brightness_increase_slot, LV_EVENT_LONG_PRESSED_REPEAT, this);

    auto brightness_decrease_slot = +[](lv_event_t *event) {
        BROOKESIA_LOG_TRACE_GUARD();
        auto *context = static_cast<ScreenSettings *>(lv_event_get_user_data(event));
        BROOKESIA_CHECK_NULL_EXIT(context, "Context is null");

        if (context->current_brightness_ <= BRIGHTNESS_MIN) {
            BROOKESIA_LOGW("Brightness is already at minimum: %1%", context->current_brightness_);
            return;
        }

        const auto brightness = context->current_brightness_ - 1;
        auto set_brightness_result = DeviceHelper::call_function_sync(
                                         DeviceHelper::FunctionId::SetDisplayBacklightBrightness, brightness
                                     );
        BROOKESIA_CHECK_FALSE_EXIT(set_brightness_result, "Failed to set brightness");
        context->current_brightness_ = brightness;
    };
    lv_obj_add_event_cb(ui_SettingsButtonBrightnessDec, brightness_decrease_slot, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_SettingsButtonBrightnessDec, brightness_decrease_slot, LV_EVENT_LONG_PRESSED, this);
    lv_obj_add_event_cb(ui_SettingsButtonBrightnessDec, brightness_decrease_slot, LV_EVENT_LONG_PRESSED_REPEAT, this);

    return true;
}

bool ScreenSettings::init_volume()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto volume_increase_slot = +[](lv_event_t *event) {
        BROOKESIA_LOG_TRACE_GUARD();
        auto *context = static_cast<ScreenSettings *>(lv_event_get_user_data(event));
        BROOKESIA_CHECK_NULL_EXIT(context, "Context is null");

        if (context->current_volume_ >= VOLUME_MAX) {
            BROOKESIA_LOGW("Volume is already at maximum: %1%", context->current_volume_);
            return;
        }

        const auto volume = context->current_volume_ + 1;
        auto set_volume_result = DeviceHelper::call_function_sync(
                                     DeviceHelper::FunctionId::SetAudioPlayerVolume, volume
                                 );
        BROOKESIA_CHECK_FALSE_EXIT(set_volume_result, "Failed to set volume");
        context->current_volume_ = volume;
    };
    lv_obj_add_event_cb(ui_SettingsButtonVolumeInc, volume_increase_slot, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_SettingsButtonVolumeInc, volume_increase_slot, LV_EVENT_LONG_PRESSED, this);
    lv_obj_add_event_cb(ui_SettingsButtonVolumeInc, volume_increase_slot, LV_EVENT_LONG_PRESSED_REPEAT, this);

    auto volume_decrease_slot = +[](lv_event_t *event) {
        BROOKESIA_LOG_TRACE_GUARD();
        auto *context = static_cast<ScreenSettings *>(lv_event_get_user_data(event));
        BROOKESIA_CHECK_NULL_EXIT(context, "Context is null");

        if (context->current_volume_ <= VOLUME_MIN) {
            BROOKESIA_LOGW("Volume is already at minimum: %1%", context->current_volume_);
            return;
        }

        const auto volume = context->current_volume_ - 1;
        auto set_volume_result = DeviceHelper::call_function_sync(
                                     DeviceHelper::FunctionId::SetAudioPlayerVolume, volume
                                 );
        BROOKESIA_CHECK_FALSE_EXIT(set_volume_result, "Failed to set volume");
        context->current_volume_ = volume;
    };
    lv_obj_add_event_cb(ui_SettingsButtonVolumeDec, volume_decrease_slot, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_SettingsButtonVolumeDec, volume_decrease_slot, LV_EVENT_LONG_PRESSED, this);
    lv_obj_add_event_cb(ui_SettingsButtonVolumeDec, volume_decrease_slot, LV_EVENT_LONG_PRESSED_REPEAT, this);

    return true;
}

bool ScreenSettings::init_reset()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto clicked_slot = +[](lv_event_t *event) {
        BROOKESIA_LOG_TRACE_GUARD();

        if (WifiHelper::is_running()) {
            auto reset_result = WifiHelper::call_function_sync(WifiHelper::FunctionId::ResetData);
            BROOKESIA_CHECK_FALSE_EXECUTE(reset_result, {}, {
                BROOKESIA_LOGE("Failed to reset Wi-Fi data");
            });
        }

        if (AgentHelper::is_running()) {
            auto reset_result = AgentHelper::call_function_sync(AgentHelper::FunctionId::ResetData);
            BROOKESIA_CHECK_FALSE_EXECUTE(reset_result, {}, {
                BROOKESIA_LOGE("Failed to reset agent data");
            });
        }

        {
            auto reset_result = DeviceHelper::call_function_sync(DeviceHelper::FunctionId::ResetData);
            BROOKESIA_CHECK_FALSE_EXECUTE(reset_result, {}, {
                BROOKESIA_LOGE("Failed to reset device data");
            });
        }

        esp_restart();
    };
    lv_obj_add_event_cb(ui_SettingsButtonResetBtn, clicked_slot, LV_EVENT_CLICKED, this);

    return true;
}

bool ScreenSettings::enter_process_brightness()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_backlight_available_) {
        BROOKESIA_LOGW("Backlight is not available, skip");
        lv_obj_add_flag(ui_SettingsLabelBrightnessText, LV_OBJ_FLAG_HIDDEN);
        return true;
    }

    auto get_brightness_result = DeviceHelper::call_function_sync<double>(
                                     DeviceHelper::FunctionId::GetDisplayBacklightBrightness
                                 );
    BROOKESIA_CHECK_FALSE_RETURN(
        get_brightness_result, false, "Failed to get brightness: %1%", get_brightness_result.error()
    );

    current_brightness_ = static_cast<int>(get_brightness_result.value());
    refresh_brightness_ui();

    brightness_changed_connection_.disconnect();

    auto brightness_changed_slot = [this](const std::string & event_name, double brightness) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD("Params: event_name(%1%), brightness(%2%)", event_name, brightness);

        current_brightness_ = static_cast<int>(brightness);
        auto send_result = Display::get_instance().send_display_task([this]() {
            refresh_brightness_ui();
        });
        BROOKESIA_CHECK_FALSE_EXIT(send_result, "Failed to send brightness update task");
    };
    brightness_changed_connection_ = DeviceHelper::subscribe_event(
                                         DeviceHelper::EventId::DisplayBacklightBrightnessChanged,
                                         brightness_changed_slot
                                     );
    BROOKESIA_CHECK_FALSE_RETURN(
        brightness_changed_connection_.connected(), false, "Failed to subscribe to brightness changed event"
    );

    return true;
}

bool ScreenSettings::enter_process_volume()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_volume_available_) {
        BROOKESIA_LOGW("Volume is not available, skip");
        lv_obj_add_flag(ui_SettingsLabelVolumeText, LV_OBJ_FLAG_HIDDEN);
        return true;
    }

    auto volume_result = DeviceHelper::call_function_sync<double>(DeviceHelper::FunctionId::GetAudioPlayerVolume);
    BROOKESIA_CHECK_FALSE_RETURN(volume_result, false, "Failed to get volume: %1%", volume_result.error());

    current_volume_ = static_cast<int>(volume_result.value());
    refresh_volume_ui();

    volume_changed_connection_.disconnect();

    auto volume_changed_slot = [this](const std::string & event_name, double volume) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD("Params: event_name(%1%), volume(%2%)", event_name, volume);

        current_volume_ = static_cast<int>(volume);
        auto send_result = Display::get_instance().send_display_task([this]() {
            refresh_volume_ui();
        });
        BROOKESIA_CHECK_FALSE_EXIT(send_result, "Failed to send volume update task");
    };
    volume_changed_connection_ = DeviceHelper::subscribe_event(
                                     DeviceHelper::EventId::AudioPlayerVolumeChanged, volume_changed_slot
                                 );
    BROOKESIA_CHECK_FALSE_RETURN(
        volume_changed_connection_.connected(), false, "Failed to subscribe to volume changed event"
    );


    return true;
}

void ScreenSettings::refresh_brightness_ui() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    lv_label_set_text(ui_SettingsLabelBrightnessText, std::to_string(current_brightness_).c_str());
    if (current_brightness_ <= BRIGHTNESS_MIN) {
        lv_obj_remove_flag(ui_SettingsButtonBrightnessDec, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(ui_SettingsButtonBrightnessDec, LV_STATE_DISABLED);
    } else {
        lv_obj_add_flag(ui_SettingsButtonBrightnessDec, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_state(ui_SettingsButtonBrightnessDec, LV_STATE_DISABLED);
    }

    if (current_brightness_ >= BRIGHTNESS_MAX) {
        lv_obj_remove_flag(ui_SettingsButtonBrightnessInc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(ui_SettingsButtonBrightnessInc, LV_STATE_DISABLED);
    } else {
        lv_obj_add_flag(ui_SettingsButtonBrightnessInc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_state(ui_SettingsButtonBrightnessInc, LV_STATE_DISABLED);
    }
}

void ScreenSettings::refresh_volume_ui() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    lv_label_set_text(ui_SettingsLabelVolumeText, std::to_string(current_volume_).c_str());
    if (current_volume_ <= VOLUME_MIN) {
        lv_obj_remove_flag(ui_SettingsButtonVolumeDec, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(ui_SettingsButtonVolumeDec, LV_STATE_DISABLED);
    } else {
        lv_obj_add_flag(ui_SettingsButtonVolumeDec, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_state(ui_SettingsButtonVolumeDec, LV_STATE_DISABLED);
    }

    if (current_volume_ >= VOLUME_MAX) {
        lv_obj_remove_flag(ui_SettingsButtonVolumeInc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(ui_SettingsButtonVolumeInc, LV_STATE_DISABLED);
    } else {
        lv_obj_add_flag(ui_SettingsButtonVolumeInc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_state(ui_SettingsButtonVolumeInc, LV_STATE_DISABLED);
    }
}

bool ScreenSettings::enter_process_agent()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Make sure the agent is suspended when the screen is entered
    auto agent_general_event_slot =
    [this](const std::string & event_name, const std::string & event_str, bool is_unexpected) {
        BROOKESIA_LOG_TRACE_GUARD();

        AgentHelper::GeneralEvent event_enum = AgentHelper::GeneralEvent::Max;
        auto parse_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(event_str, event_enum);
        BROOKESIA_CHECK_FALSE_EXIT(parse_result, "Failed to parse agent general event: %1%", event_str);

        if (event_enum == AgentHelper::GeneralEvent::Started) {
            auto suspend_result = AgentHelper::call_function_async(AgentHelper::FunctionId::Suspend);
            BROOKESIA_CHECK_FALSE_EXIT(suspend_result, "Failed to suspend agent");
        }
    };
    agent_general_event_connection_ = AgentHelper::subscribe_event(
                                          AgentHelper::EventId::GeneralEventHappened, agent_general_event_slot
                                      );
    BROOKESIA_CHECK_FALSE_RETURN(
        agent_general_event_connection_.connected(), false, "Failed to subscribe to agent general event happened event"
    );

    return true;
}

bool ScreenSettings::exit_process_agent()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    agent_general_event_connection_.disconnect();

    auto wifi_state_handler = [this](service::FunctionResult && result) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_CHECK_FALSE_EXIT(result.success, "Failed to get WiFi state: %1%", result.error_message);

        auto &data = result.get_data<std::string>();
        WifiHelper::GeneralState wifi_state = WifiHelper::GeneralState::Max;
        auto parse_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(data, wifi_state);
        BROOKESIA_CHECK_FALSE_EXIT(parse_result, "Failed to parse WiFi state");

        if (wifi_state != WifiHelper::GeneralState::Connected) {
            BROOKESIA_LOGW("WiFi is not connected(%1%), skip agent activation", wifi_state);
            return;
        }

        auto activate_handler = [this](service::FunctionResult && result) {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            BROOKESIA_CHECK_FALSE_EXIT(result.success, "Failed to activate agent: %1%", result.error_message);
            BROOKESIA_LOGI("Agent activated");
        };
        auto activate_agent_result = AgentHelper::call_function_async(
                                         AgentHelper::FunctionId::TriggerGeneralAction,
                                         BROOKESIA_DESCRIBE_TO_STR(AgentHelper::GeneralAction::Activate), activate_handler
                                     );
        BROOKESIA_CHECK_FALSE_EXIT(activate_agent_result, "Failed to activate agent");
    };
    auto wifi_state_result = WifiHelper::call_function_async(WifiHelper::FunctionId::GetGeneralState, wifi_state_handler);
    BROOKESIA_CHECK_FALSE_RETURN(wifi_state_result, true, "Failed to get WiFi state");

    return true;
}
