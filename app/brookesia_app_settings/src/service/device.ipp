/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

void SettingsApp::load_backlight_state()
{
    device_ui_state_.display_service_ready = false;
    device_ui_state_.display_backlight_supported = false;
    device_ui_state_.backlight_output_id.reset();
    device_ui_state_.backlight_output_name.clear();
    if (!SETTINGS_HARDWARE_OPERATIONS_ENABLED) {
        return;
    }

    bind_display_service();
    device_ui_state_.display_service_ready = display_service_binding_.is_valid() && DisplayHelper::is_running();
    if (!device_ui_state_.display_service_ready) {
        BROOKESIA_LOGW("Display service is not ready; brightness controls will stay disabled");
        return;
    }

    std::vector<DisplayHelper::OutputInfo> outputs;
    auto outputs_result = DisplayHelper::call_function_sync<boost::json::array>(
                              DisplayHelper::FunctionId::GetOutputs,
                              service::helper::Timeout(DEVICE_SERVICE_TIMEOUT_MS)
                          );
    if (!outputs_result) {
        BROOKESIA_LOGW("Failed to get Display outputs: %1%", outputs_result.error());
        device_ui_state_.display_service_ready = false;
        return;
    }
    if (!BROOKESIA_DESCRIBE_FROM_JSON(outputs_result.value(), outputs)) {
        BROOKESIA_LOGW("Failed to parse Display outputs");
        device_ui_state_.display_service_ready = false;
        return;
    }
    for (const auto &output : outputs) {
        if (!output.backlight.has_value()) {
            continue;
        }
        device_ui_state_.display_backlight_supported = true;
        device_ui_state_.backlight_output_id = output.id;
        device_ui_state_.backlight_output_name = output.name;
        break;
    }
    if (!device_ui_state_.display_backlight_supported || !device_ui_state_.backlight_output_id.has_value()) {
        BROOKESIA_LOGW("Display service does not expose a backlight-bound output");
        return;
    }

    auto brightness_result = DisplayHelper::call_function_sync<double>(
                                 DisplayHelper::FunctionId::GetBacklightBrightness,
                                 static_cast<double>(*device_ui_state_.backlight_output_id),
                                 service::helper::Timeout(DEVICE_SERVICE_TIMEOUT_MS)
                             );
    if (!brightness_result) {
        BROOKESIA_LOGW("Display backlight brightness is unavailable: %1%", brightness_result.error());
        device_ui_state_.display_backlight_supported = false;
        return;
    }
    device_ui_state_.brightness = clamp_percent(*brightness_result);
    BROOKESIA_LOGI(
        "Settings backlight: output(%1%:%2%), brightness(%3%)",
        *device_ui_state_.backlight_output_id, device_ui_state_.backlight_output_name, device_ui_state_.brightness
    );
}

std::string SettingsApp::brightness_status_text() const
{
    if (!SETTINGS_HARDWARE_OPERATIONS_ENABLED) {
        return localized_text(current_locale_, "display.disabled");
    }
    if (!device_ui_state_.display_service_ready) {
        return localized_text(current_locale_, "display.unavailable");
    }
    return localized_text(current_locale_, "display.no_backlight");
}

std::string SettingsApp::brightness_caption_text() const
{
    const auto locale = current_locale_.empty() ? std::string(LOCALE_EN) : current_locale_;
    if (locale == LOCALE_ZH_CN) {
        return "亮度级别";
    }
    return "Brightness level";
}

void SettingsApp::shutdown_brightness_callbacks()
{
    disconnect_device_events();
}

void SettingsApp::apply_scheduled_brightness(system::core::AppContext &context, uint64_t timer_id)
{
    int brightness = 0;
    {
        std::lock_guard lock(brightness_callbacks_->mutex);
        if (!brightness_callbacks_->open || !brightness_callbacks_->sync_queued ||
                (timer_id != brightness_callbacks_->timer_id)) {
            return;
        }
        brightness_callbacks_->sync_queued = false;
        brightness = brightness_callbacks_->pending_brightness;
    }
    device_ui_state_.brightness = brightness;
    if (auto result = refresh_display_state(context); !result) {
        BROOKESIA_LOGW("Failed to refresh display after brightness event: %1%", result.error());
    }
}

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
    load_backlight_state();

    if (auto audio_result = load_audio_state(); !audio_result) {
        BROOKESIA_LOGW("Failed to load audio state: %1%", audio_result.error());
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

std::expected<void, std::string> SettingsApp::load_audio_state()
{
    bind_audio_service();
    device_ui_state_.audio_service_ready =
        audio_playback_service_binding_.is_valid() && AudioPlaybackHelper::is_running();
    device_ui_state_.audio_player_supported = false;
    if (!device_ui_state_.audio_service_ready) {
        BROOKESIA_LOGW("AudioPlayback service is not ready; audio controls will stay disabled");
        return {};
    }

    auto volume_result = AudioPlaybackHelper::call_function_sync<double>(
                             AudioPlaybackHelper::FunctionId::GetVolume,
                             service::helper::Timeout(DEVICE_SERVICE_TIMEOUT_MS)
                         );
    auto mute_result = AudioPlaybackHelper::call_function_sync<bool>(
                           AudioPlaybackHelper::FunctionId::GetMute,
                           service::helper::Timeout(DEVICE_SERVICE_TIMEOUT_MS)
                       );
    if (!volume_result || !mute_result) {
        if (!volume_result) {
            BROOKESIA_LOGW("Audio player volume is unavailable: %1%", volume_result.error());
        }
        if (!mute_result) {
            BROOKESIA_LOGW("Audio player mute is unavailable: %1%", mute_result.error());
        }
        return {};
    }
    device_ui_state_.volume = clamp_percent(*volume_result);
    device_ui_state_.muted = *mute_result;
    device_ui_state_.audio_player_supported = true;
    return {};
}

std::expected<void, std::string> SettingsApp::refresh_my_device_state(system::core::AppContext &context)
{
    if (!settings_uses_gsp_backend()) {
        clear_hardware_groups(context);
    }
    if (current_page_ != PAGE_DEVICE && !settings_uses_gsp_backend()) {
        return {};
    }
    const auto system_info = context.system_service().get_system_info();
    const auto system_name = system_info.name.empty() ? std::string(MY_DEVICE_FALLBACK_SYSTEM_NAME) :
                             system_info.name;
    const auto system_version = system_info.version.empty() ? std::string(MY_DEVICE_FALLBACK_SYSTEM_VERSION) :
                                system_info.version;
    if (!settings_uses_gsp_backend() || cached_hardware_groups_.empty()) {
        bind_device_service();
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

        cached_device_name_ = device_name;
        cached_hardware_groups_ = groups;
    }
    // Capabilities and camera discovery are cached; connectivity belongs to this page visit.
    auto groups = cached_hardware_groups_;
    if (current_page_ == PAGE_DEVICE && device_service_binding_.is_valid() && DeviceHelper::is_running()) {
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

        if (!network_result) {
            BROOKESIA_LOGW("Failed to get Device network state: %1%", network_result.error());
            auto &group = find_or_create_hardware_group(groups, "Network", current_locale_);
            group.lines.push_back(make_localized_unavailable_text(current_locale_));
        }
    }

    const auto &device_name = cached_device_name_;
    if (settings_uses_gsp_backend()) {
        if (current_page_ != PAGE_DEVICE) {
            return {};
        }
        std::vector<MyDevicePageView::GroupCard> cards;
        for (const auto &group : groups) {
            if (group.lines.empty()) {
                MyDevicePageView::GroupCard card;
                card.title = group.title;
                card.visible = true;
                cards.push_back(std::move(card));
                continue;
            }
            const size_t chunks = (group.lines.size() + MY_DEVICE_GROUP_LINES - 1) / MY_DEVICE_GROUP_LINES;
            for (size_t chunk = 0; chunk < chunks; ++chunk) {
                MyDevicePageView::GroupCard card;
                card.title = chunks > 1 ?
                             (group.title + " (" + std::to_string(chunk + 1) + "/" + std::to_string(chunks) + ")") :
                             group.title;
                card.visible = true;
                const size_t start = chunk * MY_DEVICE_GROUP_LINES;
                const size_t count = std::min(MY_DEVICE_GROUP_LINES, group.lines.size() - start);
                card.line_count = count;
                for (size_t line = 0; line < count; ++line) {
                    card.lines[line] = group.lines[start + line];
                }
                cards.push_back(std::move(card));
            }
        }
        const size_t total = cards.size();
        const size_t pages = std::max<size_t>(1, (total + MY_DEVICE_GROUP_SLOTS - 1) / MY_DEVICE_GROUP_SLOTS);
        my_device_page_ = std::min(my_device_page_, pages - 1);

        MyDevicePageView view;
        view.system_name = system_name;
        view.system_version = system_version;
        view.device_name = cached_device_name_;
        view.previous_enabled = my_device_page_ > 0;
        view.next_enabled = my_device_page_ + 1 < pages;
        view.pagination = std::to_string(groups.size()) + localized_text(current_locale_, "gsp.groups") +
                          "   " + std::to_string(my_device_page_ + 1) + " / " + std::to_string(pages);
        for (size_t slot = 0; slot < MY_DEVICE_GROUP_SLOTS; ++slot) {
            const size_t index = my_device_page_ * MY_DEVICE_GROUP_SLOTS + slot;
            if (index < total) {
                view.groups[slot] = cards[index];
            }
        }
        return surface_->set_my_device(context, view);
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
    const int shown = enabled ? device_ui_state_.brightness : 0;
    const auto status = enabled ? std::string() : brightness_status_text();
    // The brightness slider is on the display page. Leave it alone while
    // another page is showing, and apply the value when that page opens.
    std::expected<void, std::string> result = {};
    if (!settings_uses_gsp_backend() || (current_page_ == PAGE_DISPLAY)) {
        result = surface_->set_brightness(context, shown, enabled, status, brightness_caption_text());
    }
    if (result && ((shown != logged_brightness_) || (enabled != logged_brightness_enabled_))) {
        logged_brightness_ = shown;
        logged_brightness_enabled_ = enabled;
        BROOKESIA_LOGI(
            "Settings brightness UI: value(%1%), enabled(%2%), status(%3%)",
            shown, enabled, status.empty() ? std::string("-") : status
        );
    }
    return result;
}

std::expected<void, std::string> SettingsApp::refresh_sound_state(system::core::AppContext &context)
{
    const bool enabled = device_ui_state_.audio_service_ready && device_ui_state_.audio_player_supported;
    if (settings_uses_gsp_backend()) {
        if (current_page_ != PAGE_SOUND) {
            return {};
        }
        SoundPageView view;
        view.volume = enabled ? device_ui_state_.volume : 0;
        view.muted = enabled && device_ui_state_.muted;
        view.enabled = enabled;
        view.test_enabled = enabled && !device_ui_state_.sound_pending &&
                            !settings_tone_active_ && BROOKESIA_APP_SETTINGS_SOUND_TEST_URL[0] != '\0';
        const auto status_key = !enabled ? "sound.unavailable" :
                                !device_ui_state_.sound_error.empty() ? device_ui_state_.sound_error.c_str() :
                                settings_tone_active_ ? "sound.playing" :
                                device_ui_state_.sound_pending ? "sound.saving" : "sound.ready";
        view.status = localized_text(current_locale_, status_key);
        return surface_->set_sound(context, view);
    }
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

std::expected<void, std::string> SettingsApp::drain_sound_events(system::core::AppContext &context)
{
    auto mailbox = sound_ui_mailbox_;
    if (!mailbox) {
        return {};
    }
    std::optional<double> volume;
    std::optional<bool> muted;
    std::optional<std::string> error;
    bool playback_finished = false;
    bool control_result_pending = false;
    bool control_result_success = false;
    uint64_t control_result_generation = 0;
    {
        boost::lock_guard lock(mailbox->mutex);
        if (!mailbox->open) {
            return {};
        }
        volume.swap(mailbox->volume);
        muted.swap(mailbox->muted);
        error.swap(mailbox->error);
        playback_finished = std::exchange(mailbox->playback_finished, false);
        control_result_pending = std::exchange(mailbox->control_result_pending, false);
        control_result_success = mailbox->control_result_success;
        control_result_generation = mailbox->control_result_generation;
    }
    bool changed = false;
    if (volume && !device_ui_state_.sound_pending) {
        device_ui_state_.volume = clamp_percent(*volume);
        changed = true;
    }
    if (muted && !device_ui_state_.sound_pending) {
        device_ui_state_.muted = *muted;
        changed = true;
    }
    const bool control_completed = control_result_pending && control_result_generation == sound_control_generation_;
    const bool control_timed_out = device_ui_state_.sound_pending && SteadyClock::now() >= sound_control_deadline_;
    if (control_completed || control_timed_out) {
        device_ui_state_.sound_pending = false;
        sound_control_deadline_ = {};
        if (control_completed && control_result_success) {
            device_ui_state_.sound_error.clear();
        } else {
            ++sound_control_generation_;
            device_ui_state_.sound_error = "sound.update_failed";
            ensure_message_dialog(
                context, localized_text(current_locale_, "sound.update_failed"),
                system::core::MessageDialogIcon::Warning, MESSAGE_DIALOG_FAILED_AUTO_CLOSE_MS
            );
        }
        if (auto loaded = load_audio_state(); !loaded) {
            BROOKESIA_LOGW("Failed to reload audio state after update: %1%", loaded.error());
        }
        changed = true;
    }
    if (error) {
        device_ui_state_.sound_error = *error;
        changed = true;
    }
    if (settings_tone_active_ &&
            (playback_finished || SteadyClock::now() >= settings_tone_deadline_)) {
        if (!playback_finished) {
            device_ui_state_.sound_error = "sound.play_failed";
        }
        settings_tone_active_ = false;
        settings_tone_deadline_ = {};
        {
            boost::lock_guard lock(mailbox->mutex);
            mailbox->playback_started = false;
            mailbox->playback_finished = false;
        }
        changed = true;
    }
    return changed ? refresh_sound_state(context) : std::expected<void, std::string>{};
}

void SettingsApp::subscribe_brightness_events()
{
    if (device_ui_state_.display_backlight_supported && device_ui_state_.backlight_output_id && context_ != nullptr) {
        auto brightness_state = std::make_shared<BrightnessCallbackState>();
        brightness_state->open = true;
        brightness_state->output_valid = true;
        brightness_state->output_id = *device_ui_state_.backlight_output_id;
        brightness_callbacks_ = brightness_state;
        auto timer = context_->timer().start_periodic(BRIGHTNESS_UI_TIMER_NAME, BRIGHTNESS_UI_POLL_INTERVAL_MS);
        if (!timer) {
            BROOKESIA_LOGE("Failed to start Settings brightness event timer: %1%", timer.error());
        } else {
            {
                std::lock_guard lock(brightness_state->mutex);
                brightness_state->timer_id = *timer;
            }
            const auto brightness_callback = [brightness_state](const std::string &, const service::EventItemMap &items) {
                const auto *output_id = std::get_if<double>(find_event_item(items, "OutputId"));
                const auto *output_name = std::get_if<std::string>(find_event_item(items, "OutputName"));
                const auto *brightness = std::get_if<double>(find_event_item(items, "Brightness"));
                if (!output_id || !output_name || !brightness) {
                    return;
                }
                const auto watched_output = static_cast<uint32_t>(std::lround(*output_id));
                const auto level = clamp_percent(*brightness);
                std::lock_guard lock(brightness_state->mutex);
                if (brightness_state->open && brightness_state->output_valid &&
                        brightness_state->output_id == watched_output) {
                    brightness_state->pending_brightness = level;
                    brightness_state->sync_queued = true;
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
                (void)context_->timer().stop(*timer);
                std::lock_guard lock(brightness_state->mutex);
                brightness_state->timer_id = system::core::INVALID_TIMER_ID;
            }
        }
    }

}

void SettingsApp::subscribe_device_events()
{
    if (!SETTINGS_HARDWARE_OPERATIONS_ENABLED) {
        return;
    }

    if (!device_ui_state_.display_service_ready && !device_ui_state_.audio_service_ready) {
        return;
    }

    subscribe_brightness_events();

    if (device_ui_state_.audio_player_supported) {
        if (context_ == nullptr) {
            BROOKESIA_LOGW("Settings has no App context for sound updates");
            return;
        }
        sound_ui_mailbox_ = std::make_shared<SoundUiMailbox>();
        auto timer = context_->timer().start_periodic(SOUND_UI_TIMER_NAME, SOUND_UI_POLL_INTERVAL_MS);
        if (!timer) {
            BROOKESIA_LOGW("Failed to start Settings sound UI timer: %1%", timer.error());
            sound_ui_mailbox_->close();
            sound_ui_mailbox_.reset();
            device_ui_state_.audio_player_supported = false;
            if (auto result = refresh_sound_state(*context_); !result) {
                BROOKESIA_LOGW("Failed to disable sound UI after timer error: %1%", result.error());
            }
            return;
        }
        sound_ui_timer_id_ = *timer;
        const std::weak_ptr<SoundUiMailbox> weak_mailbox = sound_ui_mailbox_;
        const service::EventRegistry::SignalSlot volume_callback =
            [weak_mailbox](const std::string &, const service::EventItemMap &items) {
            const auto *volume = std::get_if<double>(find_event_item(items, "Volume"));
            if (!volume) {
                return;
            }
            if (auto mailbox = weak_mailbox.lock()) {
                boost::lock_guard lock(mailbox->mutex);
                if (mailbox->open) {
                    mailbox->volume = *volume;
                }
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

        const service::EventRegistry::SignalSlot mute_callback =
            [weak_mailbox](const std::string &, const service::EventItemMap &items) {
            const auto *muted = std::get_if<bool>(find_event_item(items, "IsMuted"));
            if (!muted) {
                return;
            }
            if (auto mailbox = weak_mailbox.lock()) {
                boost::lock_guard lock(mailbox->mutex);
                if (mailbox->open) {
                    mailbox->muted = *muted;
                }
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

        const service::EventRegistry::SignalSlot play_state_callback =
            [weak_mailbox](const std::string &, const service::EventItemMap &items) {
            const auto *state = std::get_if<std::string>(find_event_item(items, "State"));
            if (!state) {
                return;
            }
            if (auto mailbox = weak_mailbox.lock()) {
                boost::lock_guard lock(mailbox->mutex);
                if (!mailbox->open) {
                    return;
                }
                if (*state == "Playing") {
                    mailbox->playback_started = true;
                } else if (*state == "Idle" && mailbox->playback_started) {
                    mailbox->playback_finished = true;
                }
            }
        };
        auto play_state_connection = AudioPlaybackHelper::subscribe_event(
                                         AudioPlaybackHelper::EventId::PlayStateChanged,
                                         play_state_callback
                                     );
        if (play_state_connection.connected()) {
            device_event_connections_.push_back(std::move(play_state_connection));
        } else {
            BROOKESIA_LOGW("Failed to subscribe Device audio playback state event");
        }
    }
}

void SettingsApp::disconnect_device_events()
{
    system::core::TimerId brightness_timer = system::core::INVALID_TIMER_ID;
    {
        std::lock_guard lock(brightness_callbacks_->mutex);
        brightness_callbacks_->open = false;
        brightness_callbacks_->output_valid = false;
        brightness_callbacks_->sync_queued = false;
        brightness_timer = std::exchange(brightness_callbacks_->timer_id, system::core::INVALID_TIMER_ID);
    }
    if (context_ != nullptr && brightness_timer != system::core::INVALID_TIMER_ID) {
        (void)context_->timer().stop(brightness_timer);
    }
    if (sound_ui_mailbox_) {
        sound_ui_mailbox_->close();
        sound_ui_mailbox_.reset();
    }
    if (sound_ui_timer_id_ != system::core::INVALID_TIMER_ID) {
        if (context_ != nullptr) {
            (void)context_->timer().stop(sound_ui_timer_id_);
        }
        sound_ui_timer_id_ = system::core::INVALID_TIMER_ID;
    }
    settings_tone_active_ = false;
    settings_tone_deadline_ = {};
    sound_control_deadline_ = {};
    ++sound_control_generation_;
    for (auto &connection : device_event_connections_) {
        connection.disconnect_without_wait();
    }
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
    if (clamped_volume == device_ui_state_.volume) {
        return;
    }
    const auto generation = ++sound_control_generation_;
    const std::weak_ptr<SoundUiMailbox> weak_mailbox = sound_ui_mailbox_;
    device_ui_state_.sound_pending = true;
    device_ui_state_.sound_error.clear();
    const auto completion = [weak_mailbox, generation](service::FunctionResult &&result) {
        if (auto mailbox = weak_mailbox.lock()) {
            boost::lock_guard lock(mailbox->mutex);
            if (mailbox->open && generation >= mailbox->control_result_generation) {
                mailbox->control_result_generation = generation;
                mailbox->control_result_pending = true;
                mailbox->control_result_success = result.success;
            }
        }
        if (!result.success) {
            BROOKESIA_LOGW("Failed to set audio volume: %1%", result.error_message);
        }
    };
    auto submitted = AudioPlaybackHelper::call_function_async(
                         AudioPlaybackHelper::FunctionId::SetVolume,
                         static_cast<double>(clamped_volume), completion
                     );
    if (!submitted) {
        BROOKESIA_LOGW("Failed to submit audio volume change");
        device_ui_state_.sound_pending = false;
        device_ui_state_.sound_error = "sound.update_failed";
        sound_control_deadline_ = {};
    } else {
        device_ui_state_.volume = clamped_volume;
        sound_control_deadline_ = SteadyClock::now() + std::chrono::milliseconds(DEVICE_SERVICE_TIMEOUT_MS);
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
    if (muted == device_ui_state_.muted) {
        return;
    }
    const auto generation = ++sound_control_generation_;
    const std::weak_ptr<SoundUiMailbox> weak_mailbox = sound_ui_mailbox_;
    device_ui_state_.sound_pending = true;
    device_ui_state_.sound_error.clear();
    const auto completion = [weak_mailbox, generation](service::FunctionResult &&result) {
        if (auto mailbox = weak_mailbox.lock()) {
            boost::lock_guard lock(mailbox->mutex);
            if (mailbox->open && generation >= mailbox->control_result_generation) {
                mailbox->control_result_generation = generation;
                mailbox->control_result_pending = true;
                mailbox->control_result_success = result.success;
            }
        }
        if (!result.success) {
            BROOKESIA_LOGW("Failed to set audio mute: %1%", result.error_message);
        }
    };
    auto submitted = AudioPlaybackHelper::call_function_async(
                         AudioPlaybackHelper::FunctionId::SetMute, muted, completion
                     );
    if (!submitted) {
        BROOKESIA_LOGW("Failed to submit audio mute change");
        device_ui_state_.sound_pending = false;
        device_ui_state_.sound_error = "sound.update_failed";
        sound_control_deadline_ = {};
    } else {
        device_ui_state_.muted = muted;
        sound_control_deadline_ = SteadyClock::now() + std::chrono::milliseconds(DEVICE_SERVICE_TIMEOUT_MS);
    }
    if (auto refresh_result = refresh_sound_state(context); !refresh_result) {
        BROOKESIA_LOGW("Failed to refresh audio mute UI: %1%", refresh_result.error());
    }
}

std::expected<void, std::string> SettingsApp::play_test_sound(system::core::AppContext &context)
{
    if (!device_ui_state_.audio_service_ready || !device_ui_state_.audio_player_supported ||
            device_ui_state_.sound_pending || settings_tone_active_ ||
            BROOKESIA_APP_SETTINGS_SOUND_TEST_URL[0] == '\0' || !sound_ui_mailbox_) {
        device_ui_state_.sound_error = "sound.unavailable";
        return refresh_sound_state(context);
    }

    auto mailbox = sound_ui_mailbox_;
    bool mailbox_open = false;
    {
        boost::lock_guard lock(mailbox->mutex);
        mailbox_open = mailbox->open;
        if (mailbox_open) {
            mailbox->playback_started = false;
            mailbox->playback_finished = false;
        }
    }
    if (!mailbox_open) {
        device_ui_state_.sound_error = "sound.unavailable";
        return refresh_sound_state(context);
    }
    device_ui_state_.sound_error.clear();
    settings_tone_active_ = true;
    settings_tone_deadline_ = SteadyClock::now() + std::chrono::milliseconds(2500);
    service::helper::Audio::PlayUrlConfig config{
        .interrupt = false,
        .loop_count = 0,
        .timeout_ms = 2000,
        .only_if_idle = true,
    };
    const std::weak_ptr<SoundUiMailbox> weak_mailbox = mailbox;
    const auto completion = [weak_mailbox](service::FunctionResult &&result) {
        if (result.success) {
            return;
        }
        BROOKESIA_LOGW("Failed to play Settings sound test: %1%", result.error_message);
        if (auto active = weak_mailbox.lock()) {
            boost::lock_guard lock(active->mutex);
            if (active->open) {
                active->error = result.error_message.find("Audio playback is busy") != std::string::npos ?
                                "sound.busy" : "sound.play_failed";
                active->playback_finished = true;
            }
        }
    };
    auto submitted = AudioPlaybackHelper::call_function_async(
                         AudioPlaybackHelper::FunctionId::Play,
                         std::string(BROOKESIA_APP_SETTINGS_SOUND_TEST_URL),
                         BROOKESIA_DESCRIBE_TO_JSON(config).as_object(), completion
                     );
    if (!submitted) {
        BROOKESIA_LOGW("Failed to submit Settings sound test");
        settings_tone_active_ = false;
        settings_tone_deadline_ = {};
        device_ui_state_.sound_error = "sound.play_failed";
    }
    return refresh_sound_state(context);
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
