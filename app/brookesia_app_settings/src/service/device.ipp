/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

std::expected<void, std::string> SettingsApp::refresh_device_state(system::core::AppContext &context)
{
    disconnect_device_events();
    device_ui_state_ = DeviceUiState{};

    if (!SETTINGS_HARDWARE_OPERATIONS_ENABLED) {
        BROOKESIA_LOGW("Settings hardware operations are disabled; Device controls will stay disabled");
        auto result = refresh_my_device_state(context);
        if (!result) {
            return result;
        }
        result = refresh_display_state(context);
        if (!result) {
            return result;
        }
        return refresh_sound_state(context);
    }

    bind_device_service();
    bind_display_service();
    bind_audio_service();

    device_ui_state_.display_service_ready = display_service_binding_.is_valid() && DisplayHelper::is_running();
    device_ui_state_.audio_service_ready =
        audio_playback_service_binding_.is_valid() && AudioPlaybackHelper::is_running();
    if (!device_ui_state_.display_service_ready) {
        BROOKESIA_LOGW("Display service is not ready; brightness controls will stay disabled");
    }
    if (!device_ui_state_.audio_service_ready) {
        BROOKESIA_LOGW("AudioPlayback service is not ready; audio controls will stay disabled");
    }

    if (device_ui_state_.display_service_ready) {
        std::vector<DisplayHelper::OutputInfo> outputs;
        auto outputs_result = DisplayHelper::call_function_sync<boost::json::array>(
                                  DisplayHelper::FunctionId::GetOutputs,
                                  service::helper::Timeout(DEVICE_SERVICE_TIMEOUT_MS)
                              );
        if (!outputs_result) {
            BROOKESIA_LOGW("Failed to get Display outputs: %1%", outputs_result.error());
            device_ui_state_.display_service_ready = false;
        } else if (!BROOKESIA_DESCRIBE_FROM_JSON(outputs_result.value(), outputs)) {
            BROOKESIA_LOGW("Failed to parse Display outputs");
            device_ui_state_.display_service_ready = false;
        } else {
            for (const auto &output : outputs) {
                if (!output.backlight.has_value()) {
                    continue;
                }
                device_ui_state_.display_backlight_supported = true;
                device_ui_state_.backlight_output_id = output.id;
                device_ui_state_.backlight_output_name = output.name;
                break;
            }
            if (!device_ui_state_.display_backlight_supported) {
                BROOKESIA_LOGW("Display service does not expose a backlight-bound output");
            }
        }
    }

    if (device_ui_state_.display_backlight_supported && device_ui_state_.backlight_output_id.has_value()) {
        auto brightness_result = DisplayHelper::call_function_sync<double>(
                                     DisplayHelper::FunctionId::GetBacklightBrightness,
                                     static_cast<double>(*device_ui_state_.backlight_output_id),
                                     service::helper::Timeout(DEVICE_SERVICE_TIMEOUT_MS)
                                 );
        if (brightness_result) {
            device_ui_state_.brightness = clamp_percent(*brightness_result);
        } else {
            BROOKESIA_LOGW("Display backlight brightness is unavailable: %1%", brightness_result.error());
            device_ui_state_.display_backlight_supported = false;
        }
    }

    if (device_ui_state_.audio_service_ready) {
        device_ui_state_.audio_player_supported = true;
        auto volume_result = AudioPlaybackHelper::call_function_sync<double>(
                                 AudioPlaybackHelper::FunctionId::GetVolume,
                                 service::helper::Timeout(DEVICE_SERVICE_TIMEOUT_MS)
                             );
        auto mute_result = AudioPlaybackHelper::call_function_sync<bool>(
                               AudioPlaybackHelper::FunctionId::GetMute,
                               service::helper::Timeout(DEVICE_SERVICE_TIMEOUT_MS)
                           );
        if (volume_result && mute_result) {
            device_ui_state_.volume = clamp_percent(*volume_result);
            device_ui_state_.muted = *mute_result;
        } else {
            if (!volume_result) {
                BROOKESIA_LOGW("Audio player volume is unavailable: %1%", volume_result.error());
            }
            if (!mute_result) {
                BROOKESIA_LOGW("Audio player mute is unavailable: %1%", mute_result.error());
            }
            device_ui_state_.audio_player_supported = false;
        }
    }

    auto result = refresh_my_device_state(context);
    if (!result) {
        return result;
    }
    result = refresh_display_state(context);
    if (!result) {
        return result;
    }
    result = refresh_sound_state(context);
    if (!result) {
        return result;
    }
    subscribe_device_events();
    return {};
}

std::expected<void, std::string> SettingsApp::refresh_my_device_state(system::core::AppContext &context)
{
    clear_hardware_groups(context);
    if (current_page_ != PAGE_DEVICE) {
        return {};
    }

    const auto system_info = context.system_service().get_system_info();
    const auto system_name = system_info.name.empty() ? std::string(MY_DEVICE_FALLBACK_SYSTEM_NAME) :
                             system_info.name;
    const auto system_version = system_info.version.empty() ? std::string(MY_DEVICE_FALLBACK_SYSTEM_VERSION) :
                                system_info.version;
    std::string device_name = make_localized_unavailable_text(current_locale_);
    std::vector<HardwareGroup> groups;

    if (!device_service_binding_.is_valid() || !DeviceHelper::is_running()) {
        BROOKESIA_LOGW("Device service is not ready; My Device hardware list will be unavailable");
        auto &group = find_or_create_hardware_group(groups, "Other", current_locale_);
        group.lines.push_back(make_localized_unavailable_text(current_locale_));
    } else {
        auto board_result = DeviceHelper::call_function_sync<boost::json::object>(
                                DeviceHelper::FunctionId::GetBoardInfo,
                                service::helper::Timeout(DEVICE_SERVICE_TIMEOUT_MS)
                            );
        if (board_result) {
            DeviceHelper::BoardInfo board_info;
            if (BROOKESIA_DESCRIBE_FROM_JSON(board_result.value(), board_info) && board_info.is_valid()) {
                if (!board_info.name.empty()) {
                    device_name = board_info.name;
                } else if (!board_info.chip.empty()) {
                    device_name = board_info.chip;
                }

                auto &system_group = find_or_create_hardware_group(groups, "System", current_locale_);
                if (!board_info.chip.empty()) {
                    system_group.lines.push_back(
                        localized_text(current_locale_, "text.chip_prefix") + board_info.chip
                    );
                }
                if (!board_info.manufacturer.empty()) {
                    system_group.lines.push_back(
                        localized_text(current_locale_, "text.manufacturer_prefix") + board_info.manufacturer
                    );
                }
                if (!board_info.version.empty()) {
                    system_group.lines.push_back(
                        localized_text(current_locale_, "text.board_version_prefix") + board_info.version
                    );
                }
                if (!board_info.description.empty()) {
                    system_group.lines.push_back(board_info.description);
                }
            }
        } else {
            BROOKESIA_LOGW("Failed to get Device board info: %1%", board_result.error());
        }

        auto capabilities_result = DeviceHelper::call_function_sync<boost::json::array>(
                                       DeviceHelper::FunctionId::GetCapabilities,
                                       service::helper::Timeout(DEVICE_SERVICE_TIMEOUT_MS)
                                   );
        if (capabilities_result) {
            DeviceHelper::Capabilities capabilities;
            if (BROOKESIA_DESCRIBE_FROM_JSON(capabilities_result.value(), capabilities)) {
                for (const auto &device : capabilities) {
                    if (device.interfaces.empty()) {
                        auto &group = find_or_create_hardware_group(groups, "Other", current_locale_);
                        group.lines.push_back(device.name.empty() ? make_localized_unknown_text(current_locale_) :
                                              device.name);
                        continue;
                    }
                    for (const auto &interface : device.interfaces) {
                        auto &group = find_or_create_hardware_group(
                                          groups,
                                          classify_hardware_interface(interface.type_name, interface.instance_name),
                                          current_locale_
                                      );
                        std::ostringstream line;
                        line << (device.name.empty() ? make_localized_unknown_text(current_locale_) : device.name);
                        line << " - " << interface.type_name;
                        if (!interface.instance_name.empty()) {
                            line << " (" << interface.instance_name << ")";
                        }
                        group.lines.push_back(line.str());
                    }
                }
            } else {
                BROOKESIA_LOGW("Failed to parse Device capabilities");
            }
        } else {
            BROOKESIA_LOGW("Failed to get Device capabilities: %1%", capabilities_result.error());
        }

        auto camera_result = DeviceHelper::call_function_sync<boost::json::array>(
                                 DeviceHelper::FunctionId::GetCameraDeviceInfos,
                                 service::helper::Timeout(DEVICE_SERVICE_TIMEOUT_MS)
                             );
        if (camera_result) {
            DeviceHelper::CameraDeviceInfos camera_infos;
            if (BROOKESIA_DESCRIBE_FROM_JSON(camera_result.value(), camera_infos) && !camera_infos.empty()) {
                auto &group = find_or_create_hardware_group(groups, "Video", current_locale_);
                for (const auto &camera : camera_infos) {
                    std::ostringstream line;
                    line << localized_text(current_locale_, "text.camera_prefix") << camera.id << ": ";
                    line << (camera.name.empty() ? make_localized_unknown_text(current_locale_) : camera.name);
                    if (!camera.device_path.empty()) {
                        line << " (" << camera.device_path << ")";
                    }
                    group.lines.push_back(line.str());
                }
            }
        }

        auto network_result = DeviceHelper::call_function_sync<boost::json::array>(
                                  DeviceHelper::FunctionId::GetNetworkConnectivityInfo,
                                  service::helper::Timeout(DEVICE_SERVICE_TIMEOUT_MS)
                              );
        if (network_result) {
            DeviceHelper::NetworkConnectivityInfos network_infos;
            if (BROOKESIA_DESCRIBE_FROM_JSON(network_result.value(), network_infos) && !network_infos.empty()) {
                auto &group = find_or_create_hardware_group(groups, "Network", current_locale_);
                for (const auto &network : network_infos) {
                    std::ostringstream line;
                    line << (network.instance_name.empty() ? localized_text(current_locale_, "text.network") :
                             network.instance_name);
                    line << ": " << BROOKESIA_DESCRIBE_ENUM_TO_STR(network.state);
                    group.lines.push_back(line.str());
                }
            }
        }

        auto battery_info_result = DeviceHelper::call_function_sync<boost::json::object>(
                                       DeviceHelper::FunctionId::GetPowerBatteryInfo,
                                       service::helper::Timeout(DEVICE_SERVICE_TIMEOUT_MS)
                                   );
        if (battery_info_result) {
            DeviceHelper::PowerBatteryInfo battery_info;
            if (BROOKESIA_DESCRIBE_FROM_JSON(battery_info_result.value(), battery_info) &&
                    !battery_info.name.empty()) {
                auto &group = find_or_create_hardware_group(groups, "Power", current_locale_);
                group.lines.push_back(
                    localized_text(current_locale_, "text.battery_prefix") + battery_info.name
                );
            }
        }
    }

    if (groups.empty()) {
        auto &group = find_or_create_hardware_group(groups, "Other", current_locale_);
        group.lines.push_back(make_localized_unavailable_text(current_locale_));
    }

    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(updates, MY_DEVICE_SYSTEM_NAME_PATH, "labelProps.text", system_name);
    add_binding_update(updates, MY_DEVICE_SYSTEM_VERSION_PATH, "labelProps.text", system_version);
    add_binding_update(updates, MY_DEVICE_DEVICE_VALUE_PATH, "labelProps.text", device_name);
    auto result = context.gui().set_binding_values(updates);
    if (!result) {
        return result;
    }

    for (size_t i = 0; i < groups.size(); ++i) {
        const auto instance_id = std::string("hardware_") + std::to_string(i);
        auto created = context.gui().create_view(
                           HARDWARE_GROUP_TEMPLATE_ID,
                           MY_DEVICE_HARDWARE_GROUP_PARENT,
                           instance_id
                       );
        if (!created) {
            clear_hardware_groups(context);
            return std::unexpected(created.error());
        }

        const auto instance_path = join_path(MY_DEVICE_HARDWARE_GROUP_PARENT, instance_id);
        dynamic_hardware_group_paths_.push_back(instance_path);
        result = context.gui().set_text(instance_path + "/title", groups[i].title);
        if (!result) {
            clear_hardware_groups(context);
            return result;
        }
        result = context.gui().set_text(instance_path + "/card/details", join_hardware_lines(groups[i].lines));
        if (!result) {
            clear_hardware_groups(context);
            return result;
        }
    }
    return {};
}

void SettingsApp::clear_hardware_groups(system::core::AppContext &context)
{
    for (auto it = dynamic_hardware_group_paths_.rbegin(); it != dynamic_hardware_group_paths_.rend(); ++it) {
        (void)context.gui().destroy_view(*it);
    }
    dynamic_hardware_group_paths_.clear();
}

std::expected<void, std::string> SettingsApp::refresh_display_state(system::core::AppContext &context)
{
    const bool enabled = device_ui_state_.display_service_ready && device_ui_state_.display_backlight_supported;
    std::vector<gui::BindingValueUpdate> updates;
    add_control_state_updates(updates, DISPLAY_BRIGHTNESS_ROW_PATH, DISPLAY_BRIGHTNESS_SLIDER_PATH, enabled);
    add_binding_update(
        updates,
        DISPLAY_BRIGHTNESS_SLIDER_PATH,
        "value",
        std::to_string(enabled ? device_ui_state_.brightness : 0)
    );
    return context.gui().set_binding_values(updates);
}

std::expected<void, std::string> SettingsApp::refresh_sound_state(system::core::AppContext &context)
{
    const bool enabled = device_ui_state_.audio_service_ready && device_ui_state_.audio_player_supported;
    std::vector<gui::BindingValueUpdate> updates;
    add_control_state_updates(updates, SOUND_VOLUME_ROW_PATH, SOUND_VOLUME_SLIDER_PATH, enabled);
    add_control_state_updates(updates, SOUND_MUTE_ROW_PATH, SOUND_MUTE_TOGGLE_PATH, enabled);
    add_binding_update(
        updates,
        SOUND_VOLUME_SLIDER_PATH,
        "value",
        std::to_string(enabled ? device_ui_state_.volume : 0)
    );
    add_binding_update(
        updates,
        SOUND_MUTE_TOGGLE_PATH,
        "checked",
        enabled && device_ui_state_.muted ? "true" : "false"
    );
    add_binding_update(
        updates,
        HOME_SOUND_VALUE_PATH,
        "labelProps.text",
        enabled ? (device_ui_state_.muted ? make_localized_muted_text(current_locale_) :
                   make_percent_text(device_ui_state_.volume)) : make_localized_unavailable_text(current_locale_)
    );
    return context.gui().set_binding_values(updates);
}

void SettingsApp::subscribe_device_events()
{
    if (!SETTINGS_HARDWARE_OPERATIONS_ENABLED) {
        return;
    }

    if (!device_ui_state_.display_service_ready && !device_ui_state_.audio_service_ready) {
        return;
    }

    if (device_ui_state_.display_backlight_supported) {
        const auto brightness_callback = [this](
                                             const std::string &,
                                             double output_id,
                                             const std::string &,
                                             double brightness
        ) {
            if (context_ == nullptr) {
                return;
            }
            if (device_ui_state_.backlight_output_id.has_value() &&
                    *device_ui_state_.backlight_output_id != static_cast<uint32_t>(std::lround(output_id))) {
                return;
            }
            device_ui_state_.brightness = clamp_percent(brightness);
            if (auto result = refresh_display_state(*context_); !result) {
                BROOKESIA_LOGW("Failed to refresh display after brightness event: %1%", result.error());
            }
        };
        auto brightness_connection = DisplayHelper::subscribe_event(
                                         DisplayHelper::EventId::BacklightBrightnessChanged,
                                         brightness_callback
                                     );
        if (brightness_connection.connected()) {
            device_event_connections_.push_back(std::move(brightness_connection));
        } else {
            BROOKESIA_LOGW("Failed to subscribe Device brightness event");
        }
    }

    if (device_ui_state_.audio_player_supported) {
        const auto volume_callback = [this](const std::string &, double volume) {
            if (context_ == nullptr) {
                return;
            }
            device_ui_state_.volume = clamp_percent(volume);
            if (auto result = refresh_sound_state(*context_); !result) {
                BROOKESIA_LOGW("Failed to refresh sound after volume event: %1%", result.error());
            }
        };
        auto volume_connection = AudioPlaybackHelper::subscribe_event(
                                     AudioPlaybackHelper::EventId::VolumeChanged,
                                     volume_callback
                                 );
        if (volume_connection.connected()) {
            device_event_connections_.push_back(std::move(volume_connection));
        } else {
            BROOKESIA_LOGW("Failed to subscribe Device volume event");
        }

        const auto mute_callback = [this](const std::string &, bool muted) {
            if (context_ == nullptr) {
                return;
            }
            device_ui_state_.muted = muted;
            if (auto result = refresh_sound_state(*context_); !result) {
                BROOKESIA_LOGW("Failed to refresh sound after mute event: %1%", result.error());
            }
        };
        auto mute_connection = AudioPlaybackHelper::subscribe_event(
                                   AudioPlaybackHelper::EventId::MuteChanged,
                                   mute_callback
                               );
        if (mute_connection.connected()) {
            device_event_connections_.push_back(std::move(mute_connection));
        } else {
            BROOKESIA_LOGW("Failed to subscribe Device mute event");
        }
    }
}

void SettingsApp::disconnect_device_events()
{
    device_event_connections_.clear();
}

void SettingsApp::handle_brightness_event(const gui::Event &event)
{
    if (context_ == nullptr || !device_ui_state_.display_backlight_supported) {
        return;
    }
    auto value = event.get_double("value");
    if (!value) {
        BROOKESIA_LOGW("Ignore brightness event without numeric value");
        return;
    }
    set_brightness(*context_, clamp_percent(*value));
}

void SettingsApp::handle_volume_event(const gui::Event &event)
{
    if (context_ == nullptr || !device_ui_state_.audio_player_supported) {
        return;
    }
    auto value = event.get_double("value");
    if (!value) {
        BROOKESIA_LOGW("Ignore volume event without numeric value");
        return;
    }
    set_volume(*context_, clamp_percent(*value));
}

void SettingsApp::handle_mute_event(const gui::Event &event)
{
    if (context_ == nullptr || !device_ui_state_.audio_player_supported) {
        return;
    }
    auto checked = event.get_bool("checked");
    if (!checked) {
        BROOKESIA_LOGW("Ignore mute event without checked value");
        return;
    }
    set_mute(*context_, *checked);
}

void SettingsApp::set_brightness(system::core::AppContext &context, int brightness)
{
    if (!SETTINGS_HARDWARE_OPERATIONS_ENABLED) {
        BROOKESIA_LOGW("Ignore brightness change because Settings hardware operations are disabled");
        return;
    }

    if (!device_ui_state_.display_service_ready || !device_ui_state_.display_backlight_supported ||
            !device_ui_state_.backlight_output_id.has_value()) {
        BROOKESIA_LOGW("Ignore brightness change because Display backlight control is unavailable");
        if (auto refresh_result = refresh_display_state(context); !refresh_result) {
            BROOKESIA_LOGW("Failed to refresh display brightness UI: %1%", refresh_result.error());
        }
        return;
    }
    const int clamped_brightness = std::clamp(brightness, 0, 100);
    auto result = DisplayHelper::call_function_sync(
                      DisplayHelper::FunctionId::SetBacklightBrightness,
                      static_cast<double>(*device_ui_state_.backlight_output_id),
                      static_cast<double>(clamped_brightness),
                      service::helper::Timeout(DEVICE_SERVICE_TIMEOUT_MS)
                  );
    if (!result) {
        BROOKESIA_LOGW("Failed to set display brightness: %1%", result.error());
    } else {
        device_ui_state_.brightness = clamped_brightness;
    }
    if (auto refresh_result = refresh_display_state(context); !refresh_result) {
        BROOKESIA_LOGW("Failed to refresh display brightness UI: %1%", refresh_result.error());
    }
}

void SettingsApp::set_volume(system::core::AppContext &context, int volume)
{
    if (!SETTINGS_HARDWARE_OPERATIONS_ENABLED) {
        BROOKESIA_LOGW("Ignore volume change because Settings hardware operations are disabled");
        return;
    }

    if (!device_ui_state_.audio_service_ready || !device_ui_state_.audio_player_supported) {
        BROOKESIA_LOGW("Ignore volume change because AudioPlayback control is unavailable");
        if (auto refresh_result = refresh_sound_state(context); !refresh_result) {
            BROOKESIA_LOGW("Failed to refresh audio volume UI: %1%", refresh_result.error());
        }
        return;
    }
    const int clamped_volume = std::clamp(volume, 0, 100);
    auto result = AudioPlaybackHelper::call_function_sync(
                      AudioPlaybackHelper::FunctionId::SetVolume,
                      static_cast<double>(clamped_volume),
                      service::helper::Timeout(DEVICE_SERVICE_TIMEOUT_MS)
                  );
    if (!result) {
        BROOKESIA_LOGW("Failed to set audio volume: %1%", result.error());
    } else {
        device_ui_state_.volume = clamped_volume;
    }
    if (auto refresh_result = refresh_sound_state(context); !refresh_result) {
        BROOKESIA_LOGW("Failed to refresh audio volume UI: %1%", refresh_result.error());
    }
}

void SettingsApp::set_mute(system::core::AppContext &context, bool muted)
{
    if (!SETTINGS_HARDWARE_OPERATIONS_ENABLED) {
        BROOKESIA_LOGW("Ignore mute change because Settings hardware operations are disabled");
        return;
    }

    if (!device_ui_state_.audio_service_ready || !device_ui_state_.audio_player_supported) {
        BROOKESIA_LOGW("Ignore mute change because AudioPlayback control is unavailable");
        if (auto refresh_result = refresh_sound_state(context); !refresh_result) {
            BROOKESIA_LOGW("Failed to refresh audio mute UI: %1%", refresh_result.error());
        }
        return;
    }
    auto result = AudioPlaybackHelper::call_function_sync(
                      AudioPlaybackHelper::FunctionId::SetMute,
                      muted,
                      service::helper::Timeout(DEVICE_SERVICE_TIMEOUT_MS)
                  );
    if (!result) {
        BROOKESIA_LOGW("Failed to set audio mute: %1%", result.error());
    } else {
        device_ui_state_.muted = muted;
    }
    if (auto refresh_result = refresh_sound_state(context); !refresh_result) {
        BROOKESIA_LOGW("Failed to refresh audio mute UI: %1%", refresh_result.error());
    }
}

void SettingsApp::bind_display_service()
{
    if (!SETTINGS_HARDWARE_OPERATIONS_ENABLED) {
        BROOKESIA_LOGW("Settings hardware operations are disabled; skip Display service binding");
        return;
    }

    if (display_service_binding_.is_valid()) {
        return;
    }

    if (!DisplayHelper::is_available()) {
        BROOKESIA_LOGW("Display service is not registered; brightness controls will stay disabled");
        return;
    }

    auto binding = service::ServiceManager::get_instance().bind(DisplayHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGW("Display service is available but failed to bind; brightness controls will stay disabled");
        return;
    }
    display_service_binding_ = std::move(binding);
    if (!DisplayHelper::is_running()) {
        BROOKESIA_LOGW("Display service binding was created but service is not running");
        display_service_binding_.release();
    }
}

void SettingsApp::release_display_service()
{
    display_service_binding_.release();
}

void SettingsApp::bind_audio_service()
{
    if (!SETTINGS_HARDWARE_OPERATIONS_ENABLED) {
        BROOKESIA_LOGW("Settings hardware operations are disabled; skip AudioPlayback service binding");
        return;
    }

    if (audio_playback_service_binding_.is_valid()) {
        return;
    }

    if (!AudioPlaybackHelper::is_available()) {
        BROOKESIA_LOGW("AudioPlayback service is not registered; audio controls will stay disabled");
        return;
    }

    auto binding = service::ServiceManager::get_instance().bind(AudioPlaybackHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGW("AudioPlayback service is available but failed to bind; audio controls will stay disabled");
        return;
    }
    audio_playback_service_binding_ = std::move(binding);
    if (!AudioPlaybackHelper::is_running()) {
        BROOKESIA_LOGW("AudioPlayback service binding was created but service is not running");
        audio_playback_service_binding_.release();
    }
}

void SettingsApp::release_audio_service()
{
    audio_playback_service_binding_.release();
}

void SettingsApp::bind_storage_service()
{
    if (storage_service_binding_.is_valid() || !StorageHelper::is_available()) {
        return;
    }

    auto binding = service::ServiceManager::get_instance().bind(StorageHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGW("Storage service is available but failed to bind; Settings persistence will use defaults");
        return;
    }
    storage_service_binding_ = std::move(binding);
}

void SettingsApp::release_storage_service()
{
    storage_service_binding_.release();
}

void SettingsApp::bind_device_service()
{
    if (!SETTINGS_HARDWARE_OPERATIONS_ENABLED) {
        BROOKESIA_LOGW("Settings hardware operations are disabled; skip Device service binding");
        return;
    }

    if (device_service_binding_.is_valid()) {
        return;
    }

    if (!DeviceHelper::is_available()) {
        BROOKESIA_LOGW("Device service is not registered; My Device hardware list will stay unavailable");
        return;
    }

    auto binding = service::ServiceManager::get_instance().bind(DeviceHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGW("Device service is available but failed to bind; My Device hardware list will stay unavailable");
        return;
    }
    device_service_binding_ = std::move(binding);
    if (!DeviceHelper::is_running()) {
        BROOKESIA_LOGW("Device service binding was created but service is not running");
        device_service_binding_.release();
    }
}

void SettingsApp::release_device_service()
{
    device_service_binding_.release();
}
