/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cmath>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "boost/format.hpp"
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/service_audio/macro_configs.h"
#if !BROOKESIA_SERVICE_AUDIO_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "brookesia/service_audio/service_audio.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::service {

namespace {

constexpr const char *KEY_VOLUME = "Volume";
constexpr const char *KEY_MUTE = "Mute";

std::expected<std::string, std::string> make_storage_namespace(std::string_view raw_namespace)
{
    auto result = helper::Storage::make_kv_namespace({raw_namespace});
    if (!result) {
        return std::unexpected(
                   (boost::format("Failed to make AudioPlayback Storage namespace '%1%': %2%") %
                    raw_namespace % result.error()).str()
               );
    }
    if (result->hashed) {
        BROOKESIA_LOGW("%1%", result->warning);
    }
    return result->name;
}

std::expected<std::string, std::string> make_storage_key(std::string_view raw_key)
{
    auto result = helper::Storage::make_kv_key({raw_key});
    if (!result) {
        return std::unexpected(
                   (boost::format("Failed to make AudioPlayback Storage key '%1%': %2%") %
                    raw_key % result.error()).str()
               );
    }
    if (result->hashed) {
        BROOKESIA_LOGW("%1%", result->warning);
    }
    return result->name;
}

} // namespace

void AudioPlayback::initialize_control_default_state()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    desired_volume_ = BROOKESIA_SERVICE_AUDIO_PLAYBACK_VOLUME_DEFAULT;
    desired_mute_ = BROOKESIA_SERVICE_AUDIO_PLAYBACK_DEFAULT_MUTE;
}

std::expected<void, std::string> AudioPlayback::function_set_volume(double volume)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: volume(%1%)", volume);

    if (!hal::has_interface(hal::audio::CodecPlayerIface::NAME)) {
        return std::unexpected("Audio player interface is not available");
    }

    auto volume_result = validate_percentage(volume, "volume");
    if (!volume_result) {
        return std::unexpected(volume_result.error());
    }

    const auto target_volume = volume_result.value();
    if (target_volume == desired_volume_) {
        return {};
    }

    desired_volume_ = target_volume;
    save_volume_data();
    if (!apply_control_state_to_hal()) {
        BROOKESIA_LOGW("Failed to apply audio volume to HAL immediately, target state retained");
    }
    publish_volume_changed();

    return {};
}

std::expected<double, std::string> AudioPlayback::function_get_volume()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!hal::has_interface(hal::audio::CodecPlayerIface::NAME)) {
        return std::unexpected("Audio player interface is not available");
    }

    return static_cast<double>(desired_volume_);
}

std::expected<void, std::string> AudioPlayback::function_set_mute(bool enable)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: enable(%1%)", enable);

    if (!hal::has_interface(hal::audio::CodecPlayerIface::NAME)) {
        return std::unexpected("Audio player interface is not available");
    }

    if (enable == desired_mute_) {
        return {};
    }

    desired_mute_ = enable;
    save_mute_data();
    if (!apply_control_state_to_hal()) {
        BROOKESIA_LOGW("Failed to apply audio mute state to HAL immediately, target state retained");
    }
    publish_mute_changed();

    return {};
}

std::expected<bool, std::string> AudioPlayback::function_get_mute()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!hal::has_interface(hal::audio::CodecPlayerIface::NAME)) {
        return std::unexpected("Audio player interface is not available");
    }

    return desired_mute_;
}

std::expected<void, std::string> AudioPlayback::function_reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    const auto old_volume = desired_volume_;
    const auto old_mute = desired_mute_;

    if (StorageHelper::is_available()) {
        auto namespace_result = make_storage_namespace(get_attributes().name);
        if (!namespace_result) {
            return std::unexpected(namespace_result.error());
        }
        auto result = StorageHelper::erase_keys(
                          namespace_result.value(), {}, BROOKESIA_SERVICE_AUDIO_PLAYBACK_KV_ERASE_DATA_TIMEOUT_MS
                      );
        if (!result) {
            return std::unexpected((boost::format("Failed to erase AudioPlayback Storage data: %1%") %
                                    result.error()).str());
        }
    } else {
        BROOKESIA_LOGD("Storage service is not available, skip erase");
    }

    reset_control_data(old_volume, old_mute);
    control_data_loaded_ = true;

    return {};
}

std::expected<void, std::string> AudioPlayback::function_load_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    control_data_loaded_ = false;
    load_control_data_from_storage();

    return {};
}

void AudioPlayback::load_control_data_from_storage()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (control_data_loaded_) {
        return;
    }
    if (!StorageHelper::is_available()) {
        BROOKESIA_LOGD("Storage service is not available, skip loading audio playback state");
        control_data_loaded_ = true;
        return;
    }

    auto namespace_result = make_storage_namespace(get_attributes().name);
    if (!namespace_result) {
        BROOKESIA_LOGW("%1%", namespace_result.error());
        control_data_loaded_ = true;
        return;
    }
    const auto kv_namespace = std::move(namespace_result.value());
    {
        auto key_result = make_storage_key(KEY_VOLUME);
        if (!key_result) {
            BROOKESIA_LOGW("%1%", key_result.error());
            control_data_loaded_ = true;
            return;
        }
        const auto key = std::move(key_result.value());
        auto result = StorageHelper::get_key_value<int32_t>(kv_namespace, key);
        if (!result) {
            BROOKESIA_LOGD("No persisted '%1%' in Storage: %2%", KEY_VOLUME, result.error());
        } else if ((result.value() < 0) || (result.value() > 100)) {
            BROOKESIA_LOGW("Ignored out-of-range persisted '%1%': %2%", KEY_VOLUME, result.value());
        } else {
            const auto old_volume = desired_volume_;
            desired_volume_ = static_cast<uint8_t>(result.value());
            BROOKESIA_LOGI("Loaded '%1%' from Storage: %2%", KEY_VOLUME, desired_volume_);
            if (old_volume != desired_volume_) {
                publish_volume_changed();
            }
        }
    }

    {
        auto key_result = make_storage_key(KEY_MUTE);
        if (!key_result) {
            BROOKESIA_LOGW("%1%", key_result.error());
            control_data_loaded_ = true;
            return;
        }
        const auto key = std::move(key_result.value());
        auto result = StorageHelper::get_key_value<bool>(kv_namespace, key);
        if (!result) {
            BROOKESIA_LOGD("No persisted '%1%' in Storage: %2%", KEY_MUTE, result.error());
        } else {
            const auto old_mute = desired_mute_;
            desired_mute_ = result.value();
            BROOKESIA_LOGI("Loaded '%1%' from Storage: %2%", KEY_MUTE, desired_mute_);
            if (old_mute != desired_mute_) {
                publish_mute_changed();
            }
        }
    }

    if (!apply_control_state_to_hal()) {
        BROOKESIA_LOGW("Failed to apply persisted audio playback state to HAL, cached state retained");
    }
    control_data_loaded_ = true;
}

void AudioPlayback::save_volume_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!StorageHelper::is_available()) {
        BROOKESIA_LOGD("Storage service is not available, skip");
        return;
    }

    auto namespace_result = make_storage_namespace(get_attributes().name);
    BROOKESIA_CHECK_FALSE_EXIT(namespace_result, "%1%", namespace_result.error());
    auto key_result = make_storage_key(KEY_VOLUME);
    BROOKESIA_CHECK_FALSE_EXIT(key_result, "%1%", key_result.error());
    auto key = std::move(key_result.value());
    auto result = StorageHelper::save_key_value_async(namespace_result.value(), key, desired_volume_,
    [](auto &&result) mutable {
        BROOKESIA_CHECK_FALSE_EXIT(result.success, "Failed to save %1% to Storage: %2%", KEY_VOLUME,
                                   result.error_message);
        BROOKESIA_LOGI("Saved '%1%' to Storage", KEY_VOLUME);
    });
    BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to save audio playback volume to Storage");
}

void AudioPlayback::save_mute_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!StorageHelper::is_available()) {
        BROOKESIA_LOGD("Storage service is not available, skip");
        return;
    }

    auto namespace_result = make_storage_namespace(get_attributes().name);
    BROOKESIA_CHECK_FALSE_EXIT(namespace_result, "%1%", namespace_result.error());
    auto key_result = make_storage_key(KEY_MUTE);
    BROOKESIA_CHECK_FALSE_EXIT(key_result, "%1%", key_result.error());
    auto key = std::move(key_result.value());
    auto result = StorageHelper::save_key_value_async(namespace_result.value(), key, desired_mute_,
    [](auto &&result) mutable {
        BROOKESIA_CHECK_FALSE_EXIT(result.success, "Failed to save %1% to Storage: %2%", KEY_MUTE,
                                   result.error_message);
        BROOKESIA_LOGI("Saved '%1%' to Storage", KEY_MUTE);
    });
    BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to save audio playback mute to Storage");
}

void AudioPlayback::reset_control_data(uint8_t old_volume, bool old_mute)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    initialize_control_default_state();
    if (!apply_control_state_to_hal()) {
        BROOKESIA_LOGW("Failed to restore default audio playback state");
    }
    if (old_volume != desired_volume_) {
        publish_volume_changed();
    }
    if (old_mute != desired_mute_) {
        publish_mute_changed();
    }
}

bool AudioPlayback::ensure_player_iface()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (player_iface_) {
        return true;
    }
    if (!hal::has_interface(hal::audio::CodecPlayerIface::NAME)) {
        BROOKESIA_LOGD("Audio player interface is not available");
        return false;
    }

    player_iface_ = hal::acquire_first_interface<hal::audio::CodecPlayerIface>();
    return static_cast<bool>(player_iface_);
}

bool AudioPlayback::apply_control_state_to_hal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!hal::has_interface(hal::audio::CodecPlayerIface::NAME)) {
        BROOKESIA_LOGD("Audio player interface is not available, skip");
        return true;
    }
    BROOKESIA_CHECK_FALSE_RETURN(ensure_player_iface(), false, "Failed to acquire audio player interface");

    auto desired_hw_volume = map_percentage_to_hardware(
                                 desired_volume_,
                                 BROOKESIA_SERVICE_AUDIO_PLAYBACK_VOLUME_MIN,
                                 BROOKESIA_SERVICE_AUDIO_PLAYBACK_VOLUME_MAX
                             );
    const uint8_t hw_volume = (player_iface_->is_pa_on_off_supported() || !desired_mute_) ? desired_hw_volume : 0;
    if (!player_iface_->set_volume(hw_volume)) {
        BROOKESIA_LOGW("Failed to set player volume to %1%", hw_volume);
        return false;
    }

    if (player_iface_->is_pa_on_off_supported() && !player_iface_->set_pa_on_off(!desired_mute_)) {
        BROOKESIA_LOGW("Failed to apply supported player PA state for mute(%1%)", desired_mute_);
        return false;
    }

    return true;
}

bool AudioPlayback::publish_volume_changed()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto result = publish_event(
                      BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::VolumeChanged),
                      std::vector<EventItem> {EventItem(static_cast<double>(desired_volume_))}
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish volume changed event");

    return true;
}

bool AudioPlayback::publish_mute_changed()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto result = publish_event(
                      BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::MuteChanged),
                      std::vector<EventItem> {EventItem(desired_mute_)}
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish mute changed event");

    return true;
}

std::expected<uint8_t, std::string> AudioPlayback::validate_percentage(double value, const char *name)
{
    if (!std::isfinite(value)) {
        return std::unexpected(std::string(name) + " must be finite");
    }
    if ((value < 0.0) || (value > 100.0)) {
        return std::unexpected(std::string(name) + " must be in range [0, 100]");
    }

    return static_cast<uint8_t>(std::lround(value));
}

uint8_t AudioPlayback::map_percentage_to_hardware(uint8_t percentage, uint8_t min, uint8_t max)
{
    return static_cast<uint8_t>(min + static_cast<int>(percentage) * (max - min) / 100);
}

} // namespace esp_brookesia::service
