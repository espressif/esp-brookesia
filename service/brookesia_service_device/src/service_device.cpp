/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cmath>
#include <expected>
#include <string>
#include "brookesia/hal_interface/interfaces/audio/codec_recorder.hpp"
#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/display/panel.hpp"
#include "brookesia/hal_interface/interfaces/display/touch.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_device/macro_configs.h"
#if !BROOKESIA_SERVICE_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "brookesia/service_device/service_device.hpp"
#include "brookesia/service_helper/nvs.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::service {

using NVSHelper = helper::NVS;

namespace {

std::expected<uint8_t, std::string> validate_percentage(double value, const char *name)
{
    if (!std::isfinite(value)) {
        return std::unexpected(std::string(name) + " must be finite");
    }
    if ((value < 0.0) || (value > 100.0)) {
        return std::unexpected(std::string(name) + " must be in range [0, 100]");
    }

    return static_cast<uint8_t>(std::lround(value));
}

uint8_t map_percentage_to_hardware(uint8_t percentage, uint8_t min, uint8_t max)
{
    return static_cast<uint8_t>(min + static_cast<int>(percentage) * (max - min) / 100);
}

} // namespace

bool Device::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI("Initialized");

    discover_interfaces();

    return true;
}

void Device::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    reset_interfaces();

    BROOKESIA_LOGI("Deinitialized");
}

bool Device::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    initialize_default_state();
    try_load_data();

    BROOKESIA_CHECK_FALSE_EXECUTE(
        apply_audio_state_to_hal(), {}, { BROOKESIA_LOGE("Failed to apply audio player state to HAL"); }
    );
    BROOKESIA_CHECK_FALSE_EXECUTE(
        apply_display_brightness_to_hal(), {}, { BROOKESIA_LOGE("Failed to apply display backlight state to HAL"); }
    );
    cached_power_battery_state_.reset();
    cached_power_battery_charge_config_.reset();
    if (power_battery_iface_) {
        BROOKESIA_CHECK_FALSE_RETURN(start_power_battery_polling(), false, "Failed to start power battery polling");
    }

    return true;
}

void Device::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_power_battery_polling();
    cached_power_battery_state_.reset();
    cached_power_battery_charge_config_.reset();
    is_data_loaded_ = false;
}

std::expected<boost::json::object, std::string> Device::function_get_capabilities()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return BROOKESIA_DESCRIBE_TO_JSON(hal::get_capabilities()).as_object();
}

std::expected<boost::json::object, std::string> Device::function_get_board_info()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!board_info_iface_) {
        return std::unexpected("Board info interface is not available");
    }

    return BROOKESIA_DESCRIBE_TO_JSON(board_info_iface_->get_info()).as_object();
}

std::expected<void, std::string> Device::function_set_display_backlight_brightness(double brightness)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: brightness(%1%)", brightness);

    if (!hal::has_interface(hal::DisplayBacklightIface::NAME)) {
        return std::unexpected("Display backlight interface is not available");
    }

    auto brightness_result = validate_percentage(brightness, "brightness");
    if (!brightness_result) {
        return std::unexpected(brightness_result.error());
    }

    const auto old_brightness = current_brightness_;
    current_brightness_ = brightness_result.value();
    if (!apply_display_brightness_to_hal()) {
        current_brightness_ = old_brightness;
        return std::unexpected("Failed to set brightness");
    }

    if (old_brightness != current_brightness_) {
        try_save_data(DataType::Brightness);
        publish_brightness_changed();
    }

    return {};
}

std::expected<double, std::string> Device::function_get_display_backlight_brightness()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!hal::has_interface(hal::DisplayBacklightIface::NAME)) {
        return std::unexpected("Display backlight interface is not available");
    }

    return static_cast<double>(current_brightness_);
}

std::expected<void, std::string> Device::function_set_display_backlight_on_off(bool on)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: on(%1%)", on);

    if (!hal::has_interface(hal::DisplayBacklightIface::NAME)) {
        return std::unexpected("Display backlight interface is not available");
    }

    if (on == desired_backlight_on_) {
        return {};
    }

    const auto old_backlight_on = desired_backlight_on_;
    desired_backlight_on_ = on;
    if (!apply_display_brightness_to_hal()) {
        desired_backlight_on_ = !on;
        return std::unexpected("Failed to set display backlight state");
    }

    if (old_backlight_on != desired_backlight_on_) {
        publish_backlight_on_off_changed();
    }

    return {};
}

std::expected<bool, std::string> Device::function_get_display_backlight_on_off()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!hal::has_interface(hal::DisplayBacklightIface::NAME)) {
        return std::unexpected("Display backlight interface is not available");
    }

    return desired_backlight_on_;
}

std::expected<void, std::string> Device::function_set_audio_player_volume(double volume)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: volume(%1%)", volume);

    if (!hal::has_interface(hal::AudioCodecPlayerIface::NAME)) {
        return std::unexpected("Audio player interface is not available");
    }

    auto volume_result = validate_percentage(volume, "volume");
    if (!volume_result) {
        return std::unexpected(volume_result.error());
    }

    const auto target_volume = volume_result.value();
    if (target_volume == desired_volume_) {
        BROOKESIA_LOGD("Target volume is already set, skip");
        return {};
    }

    desired_volume_ = target_volume;
    try_save_data(DataType::Volume);
    if (!apply_audio_state_to_hal()) {
        BROOKESIA_LOGW("Failed to apply audio volume to HAL immediately, target state retained");
    }
    publish_volume_changed();

    return {};
}

std::expected<double, std::string> Device::function_get_audio_player_volume()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!hal::has_interface(hal::AudioCodecPlayerIface::NAME)) {
        return std::unexpected("Audio player interface is not available");
    }

    return static_cast<double>(desired_volume_);
}

std::expected<void, std::string> Device::function_set_audio_player_mute(bool enable)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: enable(%1%)", enable);

    if (!hal::has_interface(hal::AudioCodecPlayerIface::NAME)) {
        return std::unexpected("Audio player interface is not available");
    }

    if (enable == desired_mute_) {
        return {};
    }

    desired_mute_ = enable;
    try_save_data(DataType::Mute);
    if (!apply_audio_state_to_hal()) {
        BROOKESIA_LOGW("Failed to apply audio mute state to HAL immediately, target state retained");
    }
    publish_mute_changed();

    return {};
}

std::expected<bool, std::string> Device::function_get_audio_player_mute()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!hal::has_interface(hal::AudioCodecPlayerIface::NAME)) {
        return std::unexpected("Audio player interface is not available");
    }

    return desired_mute_;
}

std::expected<boost::json::array, std::string> Device::function_get_storage_file_systems()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!hal::has_interface(hal::StorageFsIface::NAME)) {
        return std::unexpected("Storage file-system interface is not available");
    }

    auto infos = storage_fs_iface_->get_all_info();

    return BROOKESIA_DESCRIBE_TO_JSON(infos).as_array();
}

std::expected<boost::json::object, std::string> Device::function_get_power_battery_info()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!power_battery_iface_) {
        return std::unexpected("Power battery interface is not available");
    }

    return BROOKESIA_DESCRIBE_TO_JSON(power_battery_iface_->get_info()).as_object();
}

std::expected<boost::json::object, std::string> Device::function_get_power_battery_state()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!power_battery_iface_) {
        return std::unexpected("Power battery interface is not available");
    }

    PowerBatteryState state;
    if (!power_battery_iface_->get_state(state)) {
        return std::unexpected("Failed to get power battery state");
    }

    return BROOKESIA_DESCRIBE_TO_JSON(state).as_object();
}

std::expected<boost::json::object, std::string> Device::function_get_power_battery_charge_config()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!power_battery_iface_) {
        return std::unexpected("Power battery interface is not available");
    }
    if (!power_battery_iface_->get_info().has_ability(hal::PowerBatteryIface::Ability::ChargeConfig)) {
        return std::unexpected("Power battery charge config is not supported");
    }

    PowerBatteryChargeConfig config;
    if (!power_battery_iface_->get_charge_config(config)) {
        return std::unexpected("Failed to get power battery charge config");
    }

    return BROOKESIA_DESCRIBE_TO_JSON(config).as_object();
}

std::expected<void, std::string> Device::function_set_power_battery_charge_config(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!power_battery_iface_) {
        return std::unexpected("Power battery interface is not available");
    }
    if (!power_battery_iface_->get_info().has_ability(hal::PowerBatteryIface::Ability::ChargeConfig)) {
        return std::unexpected("Power battery charge config is not supported");
    }

    PowerBatteryChargeConfig charge_config;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config, charge_config)) {
        return std::unexpected("Failed to parse power battery charge config");
    }
    if (!power_battery_iface_->set_charge_config(charge_config)) {
        return std::unexpected("Failed to set power battery charge config");
    }

    cached_power_battery_state_.reset();
    cached_power_battery_charge_config_.reset();

    PowerBatteryChargeConfig current_config;
    if (power_battery_iface_->get_charge_config(current_config)) {
        cached_power_battery_charge_config_ = current_config;
        publish_power_battery_charge_config_changed(current_config);
    } else {
        BROOKESIA_LOGW("Failed to read back power battery charge config after update");
    }

    return {};
}

std::expected<void, std::string> Device::function_set_power_battery_charging_enabled(bool enabled)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: enabled(%1%)", enabled);

    if (!power_battery_iface_) {
        return std::unexpected("Power battery interface is not available");
    }
    if (!power_battery_iface_->get_info().has_ability(hal::PowerBatteryIface::Ability::ChargerControl)) {
        return std::unexpected("Power battery charging control is not supported");
    }
    if (!power_battery_iface_->set_charging_enabled(enabled)) {
        return std::unexpected("Failed to set power battery charging state");
    }

    cached_power_battery_state_.reset();
    cached_power_battery_charge_config_.reset();

    PowerBatteryState state;
    if (power_battery_iface_->get_state(state)) {
        cached_power_battery_state_ = state;
        publish_power_battery_state_changed(state);
    } else {
        BROOKESIA_LOGW("Failed to read back power battery state after charging enable update");
    }

    if (power_battery_iface_->get_info().has_ability(hal::PowerBatteryIface::Ability::ChargeConfig)) {
        PowerBatteryChargeConfig config;
        if (power_battery_iface_->get_charge_config(config)) {
            cached_power_battery_charge_config_ = config;
            publish_power_battery_charge_config_changed(config);
        } else {
            BROOKESIA_LOGW("Failed to read back power battery charge config after charging enable update");
        }
    }

    return {};
}

std::expected<void, std::string> Device::function_reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    reset_data();

    return {};
}

helper::Device::Capabilities Device::get_capabilities() const
{
    return hal::get_capabilities();
}

void Device::discover_interfaces()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto [board_info_name, board_info_iface] = hal::get_first_interface<hal::BoardInfoIface>();
    auto [backlight_name, backlight_iface] = hal::get_first_interface<hal::DisplayBacklightIface>();
    auto [audio_name, audio_iface] = hal::get_first_interface<hal::AudioCodecPlayerIface>();
    auto [storage_name, storage_iface] = hal::get_first_interface<hal::StorageFsIface>();
    auto [power_battery_name, power_battery_iface] = hal::get_first_interface<hal::PowerBatteryIface>();

    board_info_iface_ = std::move(board_info_iface);
    display_backlight_iface_ = std::move(backlight_iface);
    audio_player_iface_ = std::move(audio_iface);
    storage_fs_iface_ = std::move(storage_iface);
    power_battery_iface_ = std::move(power_battery_iface);
}

void Device::reset_interfaces()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    board_info_iface_.reset();
    display_backlight_iface_.reset();
    audio_player_iface_.reset();
    storage_fs_iface_.reset();
    power_battery_iface_.reset();
}

void Device::initialize_default_state()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    current_brightness_ = BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT;
    desired_backlight_on_ = display_backlight_iface_ ? BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_DEFAULT_ON : false;
    desired_volume_ = BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_DEFAULT;
    desired_mute_ = BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_DEFAULT_MUTE;

    BROOKESIA_LOGD(
        "Initialized default device state: brightness(%1%), backlight_on(%2%), volume(%3%), mute(%4%)",
        current_brightness_, desired_backlight_on_, desired_volume_, desired_mute_
    );
}

bool Device::apply_audio_state_to_hal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!hal::has_interface(hal::AudioCodecPlayerIface::NAME)) {
        BROOKESIA_LOGD("Audio player interface is not available, skip");
        return true;
    }

    auto desired_hw_volume = map_percentage_to_hardware(
                                 desired_volume_,
                                 BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_MIN,
                                 BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_MAX
                             );
    const uint8_t hw_volume = (audio_player_iface_->is_pa_on_off_supported() || !desired_mute_) ? desired_hw_volume : 0;
    if (!audio_player_iface_->set_volume(hw_volume)) {
        BROOKESIA_LOGW("Failed to set player volume to %1%", hw_volume);
        return false;
    }

    if (audio_player_iface_->is_pa_on_off_supported() && !audio_player_iface_->set_pa_on_off(!desired_mute_)) {
        BROOKESIA_LOGW("Failed to apply supported player PA state for mute(%1%)", desired_mute_);
        return false;
    }

    return true;
}

bool Device::apply_display_brightness_to_hal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!hal::has_interface(hal::DisplayBacklightIface::NAME)) {
        BROOKESIA_LOGD("Display backlight interface is not available, skip");
        return true;
    }

    if (desired_backlight_on_) {
        auto hw_value = map_percentage_to_hardware(
                            current_brightness_,
                            BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MIN,
                            BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MAX
                        );
        if (!display_backlight_iface_->set_brightness(hw_value)) {
            BROOKESIA_LOGW("Failed to set backlight brightness to %1%", hw_value);
            return false;
        }

        if (!display_backlight_iface_->is_light_on_off_supported()) {
            return true;
        }
        if (display_backlight_iface_->set_light_on_off(true)) {
            return true;
        }

        BROOKESIA_LOGW("Failed to turn on supported backlight explicitly");
        return true;
    }

    if (!display_backlight_iface_->is_light_on_off_supported()) {
        if (!display_backlight_iface_->set_brightness(0)) {
            BROOKESIA_LOGW("Failed to apply fallback backlight brightness(0)");
            return false;
        }
        return true;
    }
    if (display_backlight_iface_->set_light_on_off(false)) {
        return true;
    }

    BROOKESIA_LOGW("Failed to turn off supported backlight explicitly");
    if (!display_backlight_iface_->set_brightness(0)) {
        BROOKESIA_LOGW("Failed to apply fallback backlight brightness(0)");
        return false;
    }

    return true;
}

bool Device::start_power_battery_polling()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!power_battery_iface_) {
        BROOKESIA_LOGD("Power battery interface is not available, skip polling");
        return true;
    }

    auto scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Task scheduler is not available");

    stop_power_battery_polling();

    auto result = scheduler->post_periodic(
    [this]() -> bool {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        if (!poll_power_battery_once())
        {
            BROOKESIA_LOGW("Power battery polling iteration failed");
        }
        return true;
    },
    BROOKESIA_SERVICE_DEVICE_POWER_BATTERY_POLL_INTERVAL_MS,
    &power_battery_poll_task_id_,
    get_call_task_group()
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to schedule power battery polling task");

    if (!poll_power_battery_once()) {
        BROOKESIA_LOGW("Initial power battery poll failed");
    }

    return true;
}

void Device::stop_power_battery_polling()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (power_battery_poll_task_id_ == 0) {
        return;
    }

    auto scheduler = get_task_scheduler();
    if (scheduler) {
        scheduler->cancel(power_battery_poll_task_id_);
    }
    power_battery_poll_task_id_ = 0;
}

bool Device::poll_power_battery_once()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!power_battery_iface_) {
        return false;
    }

    PowerBatteryState state;
    if (!power_battery_iface_->get_state(state)) {
        BROOKESIA_LOGW("Failed to read power battery state");
        return false;
    }
    if (!cached_power_battery_state_.has_value() ||
            !is_power_battery_state_equal(cached_power_battery_state_.value(), state)) {
        cached_power_battery_state_ = state;
        BROOKESIA_CHECK_FALSE_RETURN(
            publish_power_battery_state_changed(state), false, "Failed to publish power battery state changed event"
        );
    }

    if (!power_battery_iface_->get_info().has_ability(hal::PowerBatteryIface::Ability::ChargeConfig)) {
        return true;
    }

    PowerBatteryChargeConfig config;
    if (!power_battery_iface_->get_charge_config(config)) {
        BROOKESIA_LOGW("Failed to read power battery charge config");
        return false;
    }
    if (!cached_power_battery_charge_config_.has_value() ||
            !is_power_battery_charge_config_equal(cached_power_battery_charge_config_.value(), config)) {
        cached_power_battery_charge_config_ = config;
        BROOKESIA_CHECK_FALSE_RETURN(
            publish_power_battery_charge_config_changed(config), false,
            "Failed to publish power battery charge config changed event"
        );
    }

    return true;
}

bool Device::publish_power_battery_state_changed(const PowerBatteryState &state)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto result = publish_event(
                      BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::PowerBatteryStateChanged),
                      std::vector<EventItem> {EventItem(BROOKESIA_DESCRIBE_TO_JSON(state).as_object())}
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish power battery state changed event");

    return true;
}

bool Device::publish_power_battery_charge_config_changed(const PowerBatteryChargeConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto result = publish_event(
                      BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::PowerBatteryChargeConfigChanged),
                      std::vector<EventItem> {EventItem(BROOKESIA_DESCRIBE_TO_JSON(config).as_object())}
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish power battery charge config changed event");

    return true;
}

bool Device::is_power_battery_state_equal(const PowerBatteryState &lhs, const PowerBatteryState &rhs)
{
    return (lhs.is_present == rhs.is_present) &&
           (lhs.power_source == rhs.power_source) &&
           (lhs.charge_state == rhs.charge_state) &&
           (lhs.level_source == rhs.level_source) &&
           (lhs.voltage_mv == rhs.voltage_mv) &&
           (lhs.percentage == rhs.percentage) &&
           (lhs.vbus_voltage_mv == rhs.vbus_voltage_mv) &&
           (lhs.system_voltage_mv == rhs.system_voltage_mv) &&
           (lhs.is_low == rhs.is_low) &&
           (lhs.is_critical == rhs.is_critical);
}

bool Device::is_power_battery_charge_config_equal(
    const PowerBatteryChargeConfig &lhs, const PowerBatteryChargeConfig &rhs
)
{
    return (lhs.enabled == rhs.enabled) &&
           (lhs.target_voltage_mv == rhs.target_voltage_mv) &&
           (lhs.charge_current_ma == rhs.charge_current_ma) &&
           (lhs.precharge_current_ma == rhs.precharge_current_ma) &&
           (lhs.termination_current_ma == rhs.termination_current_ma);
}

bool Device::publish_volume_changed()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto result = publish_event(
                      BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::AudioPlayerVolumeChanged),
                      std::vector<EventItem> {EventItem(static_cast<double>(desired_volume_))}
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish volume changed event");

    return true;
}

bool Device::publish_mute_changed()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto result = publish_event(
                      BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::AudioPlayerMuteChanged),
                      std::vector<EventItem> {EventItem(desired_mute_)}
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish mute changed event");

    return true;
}

bool Device::publish_brightness_changed()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto result = publish_event(
                      BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::DisplayBacklightBrightnessChanged),
                      std::vector<EventItem> {EventItem(static_cast<double>(current_brightness_))}
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish brightness changed event");

    return true;
}

bool Device::publish_backlight_on_off_changed()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto result = publish_event(
                      BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::DisplayBacklightOnOffChanged),
                      std::vector<EventItem> {EventItem(desired_backlight_on_)}
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish backlight on/off changed event");

    return true;
}

void Device::try_load_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_data_loaded_) {
        BROOKESIA_LOGD("Data is already loaded, skip");
        return;
    }
    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        is_data_loaded_ = true;
        return;
    }

    auto binding = ServiceManager::get_instance().bind(NVSHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_EXIT(binding.is_valid(), "Failed to bind NVS service");

    const auto nvs_namespace = get_attributes().name;
    {
        const auto key = BROOKESIA_DESCRIBE_TO_STR(DataType::Volume);
        auto result = NVSHelper::get_key_value<uint8_t>(nvs_namespace, key);
        if (!result) {
            BROOKESIA_LOGW("Failed to load '%1%' from NVS: %2%", key, result.error());
        } else {
            BROOKESIA_LOGD("Loaded '%1%' from NVS: %2%", key, BROOKESIA_DESCRIBE_TO_STR(result.value()));
            const auto old_volume = desired_volume_;
            desired_volume_ = result.value();
            if (!apply_audio_state_to_hal()) {
                BROOKESIA_LOGW("Failed to apply persisted audio state to HAL immediately");
            }
            if (old_volume != desired_volume_) {
                publish_volume_changed();
            }
        }
    }

    {
        const auto key = BROOKESIA_DESCRIBE_TO_STR(DataType::Mute);
        auto result = NVSHelper::get_key_value<bool>(nvs_namespace, key);
        if (!result) {
            BROOKESIA_LOGW("Failed to load '%1%' from NVS: %2%", key, result.error());
        } else {
            BROOKESIA_LOGD("Loaded '%1%' from NVS: %2%", key, BROOKESIA_DESCRIBE_TO_STR(result.value()));
            const auto old_mute = desired_mute_;
            desired_mute_ = result.value();
            if (!apply_audio_state_to_hal()) {
                BROOKESIA_LOGW("Failed to apply persisted audio state to HAL immediately");
            }
            if (old_mute != desired_mute_) {
                publish_mute_changed();
            }
        }
    }

    {
        const auto key = BROOKESIA_DESCRIBE_TO_STR(DataType::Brightness);
        auto result = NVSHelper::get_key_value<uint8_t>(nvs_namespace, key);
        if (!result) {
            BROOKESIA_LOGW("Failed to load '%1%' from NVS: %2%", key, result.error());
        } else {
            BROOKESIA_LOGD("Loaded '%1%' from NVS: %2%", key, BROOKESIA_DESCRIBE_TO_STR(result.value()));
            const auto old_brightness = current_brightness_;
            current_brightness_ = result.value();
            if (!apply_display_brightness_to_hal()) {
                BROOKESIA_LOGW("Failed to apply persisted brightness to HAL immediately");
            }
            if (old_brightness != current_brightness_) {
                publish_brightness_changed();
            }
        }
    }

    is_data_loaded_ = true;

    BROOKESIA_LOGI("Loaded all data from NVS");
}

void Device::try_save_data(DataType type)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    const auto nvs_namespace = get_attributes().name;
    switch (type) {
    case DataType::Volume: {
        const auto key = BROOKESIA_DESCRIBE_TO_STR(DataType::Volume);
        auto result = NVSHelper::save_key_value(
                          nvs_namespace, key, desired_volume_, BROOKESIA_SERVICE_DEVICE_NVS_SAVE_DATA_TIMEOUT_MS
                      );
        if (!result) {
            BROOKESIA_LOGE("Failed to save '%1%' to NVS: %2%", key, result.error());
            break;
        }
        BROOKESIA_LOGD("Saved '%1%' to NVS", key);
        BROOKESIA_LOGD("\tValue: %1%", BROOKESIA_DESCRIBE_TO_STR(desired_volume_));
        break;
    }
    case DataType::Mute: {
        const auto key = BROOKESIA_DESCRIBE_TO_STR(DataType::Mute);
        auto result = NVSHelper::save_key_value(
                          nvs_namespace, key, desired_mute_, BROOKESIA_SERVICE_DEVICE_NVS_SAVE_DATA_TIMEOUT_MS
                      );
        if (!result) {
            BROOKESIA_LOGE("Failed to save '%1%' to NVS: %2%", key, result.error());
            break;
        }
        BROOKESIA_LOGD("Saved '%1%' to NVS", key);
        BROOKESIA_LOGD("\tValue: %1%", BROOKESIA_DESCRIBE_TO_STR(desired_mute_));
        break;
    }
    case DataType::Brightness: {
        const auto key = BROOKESIA_DESCRIBE_TO_STR(DataType::Brightness);
        auto result = NVSHelper::save_key_value(
                          nvs_namespace, key, current_brightness_,
                          BROOKESIA_SERVICE_DEVICE_NVS_SAVE_DATA_TIMEOUT_MS
                      );
        if (!result) {
            BROOKESIA_LOGE("Failed to save '%1%' to NVS: %2%", key, result.error());
            break;
        }
        BROOKESIA_LOGD("Saved '%1%' to NVS", key);
        BROOKESIA_LOGD("\tValue: %1%", BROOKESIA_DESCRIBE_TO_STR(current_brightness_));
        break;
    }
    case DataType::Max:
    default:
        BROOKESIA_LOGE("Invalid data type for saving to NVS");
        break;
    }
}

void Device::try_erase_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto result = NVSHelper::erase_keys(
                      get_attributes().name, {}, BROOKESIA_SERVICE_DEVICE_NVS_ERASE_DATA_TIMEOUT_MS
                  );
    if (!result) {
        BROOKESIA_LOGE("Failed to erase NVS data: %1%", result.error());
        return;
    }

    BROOKESIA_LOGI("Erased NVS data");
}

void Device::reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    const auto old_volume = desired_volume_;
    const auto old_mute = desired_mute_;
    const auto old_brightness = current_brightness_;

    try_erase_data();
    initialize_default_state();

    if (hal::has_interface(hal::DisplayBacklightIface::NAME)) {
        if (!apply_display_brightness_to_hal()) {
            BROOKESIA_LOGW("Failed to restore default brightness");
        }
        if (old_brightness != current_brightness_) {
            publish_brightness_changed();
        }
    }
    if (hal::has_interface(hal::AudioCodecPlayerIface::NAME)) {
        if (!apply_audio_state_to_hal()) {
            BROOKESIA_LOGW("Failed to restore default audio state");
        }
        if (old_volume != desired_volume_) {
            publish_volume_changed();
        }
        if (old_mute != desired_mute_) {
            publish_mute_changed();
        }
    }
}

#if BROOKESIA_SERVICE_DEVICE_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    ServiceBase, Device, Device::get_instance().get_attributes().name, Device::get_instance(),
    BROOKESIA_SERVICE_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::service
