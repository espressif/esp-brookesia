/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cmath>
#include <expected>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "boost/format.hpp"
#include "brookesia/service_display/service_display.hpp"
#include "brookesia/service_helper/system/storage.hpp"

#if !BROOKESIA_SERVICE_DISPLAY_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

namespace esp_brookesia::service {

namespace {

constexpr const char *KEY_PREFIX_BACKLIGHT = "backlight";
constexpr const char *KEY_BRIGHTNESS = "Brightness";
constexpr const char *KEY_ON = "On";

} // namespace

std::expected<void, std::string> Display::function_set_backlight_brightness(
    double output_id, double brightness
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: output_id(%1%), brightness(%2%)", output_id, brightness);

    auto parsed_output_id = validate_output_id_param(output_id);
    if (!parsed_output_id) {
        return std::unexpected(parsed_output_id.error());
    }
    auto brightness_result = validate_percentage(brightness, "brightness");
    if (!brightness_result) {
        return std::unexpected(brightness_result.error());
    }

    uint32_t resolved_output_id = 0;
    std::string resolved_output_name;
    std::string storage_output_id;
    uint8_t old_brightness = 0;
    uint8_t new_brightness = brightness_result.value();
    bool changed = false;
    {
        std::lock_guard lock(mutex_);
        auto output_it = outputs_.find(parsed_output_id.value());
        if (output_it == outputs_.end()) {
            return std::unexpected((boost::format("Display output id %1% is not available") %
                                    parsed_output_id.value()).str());
        }

        auto &output = output_it->second;
        if (!output.backlight) {
            return std::unexpected((boost::format("Display output '%1%' has no backlight") %
                                    output.info.name).str());
        }
        resolved_output_id = output.info.id;
        resolved_output_name = output.info.name;
        storage_output_id = get_backlight_storage_output_id(output);
        old_brightness = output.backlight_brightness;
        if (old_brightness == new_brightness) {
            return {};
        }

        output.backlight_brightness = new_brightness;
        if (!apply_backlight_state_to_hal(output)) {
            output.backlight_brightness = old_brightness;
            return std::unexpected("Failed to set display backlight brightness");
        }
        changed = true;
    }

    if (changed) {
        save_backlight_brightness_data(storage_output_id, new_brightness);
        emit_backlight_brightness_changed(resolved_output_id, resolved_output_name, new_brightness);
    }
    return {};
}

std::expected<double, std::string> Display::function_get_backlight_brightness(double output_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto parsed_output_id = validate_output_id_param(output_id);
    if (!parsed_output_id) {
        return std::unexpected(parsed_output_id.error());
    }
    std::lock_guard lock(mutex_);
    auto output_it = outputs_.find(parsed_output_id.value());
    if (output_it == outputs_.end()) {
        return std::unexpected((boost::format("Display output id %1% is not available") %
                                parsed_output_id.value()).str());
    }

    const auto &output = output_it->second;
    if (!output.backlight) {
        return std::unexpected((boost::format("Display output '%1%' has no backlight") % output.info.name).str());
    }
    return static_cast<double>(output.backlight_brightness);
}

std::expected<void, std::string> Display::function_set_backlight_on_off(
    double output_id, bool on
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: output_id(%1%), on(%2%)", output_id, on);

    auto parsed_output_id = validate_output_id_param(output_id);
    if (!parsed_output_id) {
        return std::unexpected(parsed_output_id.error());
    }
    uint32_t resolved_output_id = 0;
    std::string resolved_output_name;
    std::string storage_output_id;
    bool changed = false;
    bool old_on = false;
    {
        std::lock_guard lock(mutex_);
        auto output_it = outputs_.find(parsed_output_id.value());
        if (output_it == outputs_.end()) {
            return std::unexpected((boost::format("Display output id %1% is not available") %
                                    parsed_output_id.value()).str());
        }

        auto &output = output_it->second;
        if (!output.backlight) {
            return std::unexpected((boost::format("Display output '%1%' has no backlight") %
                                    output.info.name).str());
        }
        resolved_output_id = output.info.id;
        resolved_output_name = output.info.name;
        storage_output_id = get_backlight_storage_output_id(output);
        old_on = output.backlight_on;
        if (old_on == on) {
            return {};
        }

        output.backlight_on = on;
        if (!apply_backlight_state_to_hal(output)) {
            output.backlight_on = old_on;
            return std::unexpected("Failed to set display backlight state");
        }
        changed = true;
    }

    if (changed) {
        save_backlight_on_off_data(storage_output_id, on);
        emit_backlight_on_off_changed(resolved_output_id, resolved_output_name, on);
    }
    return {};
}

std::expected<bool, std::string> Display::function_get_backlight_on_off(double output_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto parsed_output_id = validate_output_id_param(output_id);
    if (!parsed_output_id) {
        return std::unexpected(parsed_output_id.error());
    }
    std::lock_guard lock(mutex_);
    auto output_it = outputs_.find(parsed_output_id.value());
    if (output_it == outputs_.end()) {
        return std::unexpected((boost::format("Display output id %1% is not available") %
                                parsed_output_id.value()).str());
    }

    const auto &output = output_it->second;
    if (!output.backlight) {
        return std::unexpected((boost::format("Display output '%1%' has no backlight") % output.info.name).str());
    }
    return output.backlight_on;
}

std::expected<void, std::string> Display::function_reset_data(double output_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: output_id(%1%)", output_id);

    auto parsed_output_id = validate_output_id_param(output_id);
    if (!parsed_output_id) {
        return std::unexpected(parsed_output_id.error());
    }
    std::vector<uint32_t> output_ids;
    std::vector<std::string> keys;
    std::string storage_output_id;
    const auto target_output_id = parsed_output_id.value();
    {
        std::lock_guard lock(mutex_);
        auto result = collect_backlight_output_ids_locked(target_output_id);
        if (!result) {
            return std::unexpected(result.error());
        }
        output_ids = std::move(result.value());
        if (target_output_id != 0) {
            const auto &output = outputs_.at(target_output_id);
            storage_output_id = get_backlight_storage_output_id(output);
        }
    }

    if (helper::Storage::is_available()) {
        auto namespace_result = make_backlight_storage_namespace();
        if (!namespace_result) {
            return std::unexpected(namespace_result.error());
        }
        if (target_output_id != 0) {
            auto brightness_key = make_backlight_storage_key(storage_output_id, KEY_BRIGHTNESS);
            if (!brightness_key) {
                return std::unexpected(brightness_key.error());
            }
            auto on_key = make_backlight_storage_key(storage_output_id, KEY_ON);
            if (!on_key) {
                return std::unexpected(on_key.error());
            }
            keys.push_back(std::move(brightness_key.value()));
            keys.push_back(std::move(on_key.value()));
        }
        auto erase_result = helper::Storage::erase_keys(
                                namespace_result.value(), keys,
                                BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_KV_ERASE_DATA_TIMEOUT_MS
                            );
        if (!erase_result) {
            return std::unexpected((boost::format("Failed to erase Display Storage data: %1%") %
                                    erase_result.error()).str());
        }
    } else {
        BROOKESIA_LOGD("Storage service is not available, skip erase");
    }

    std::vector<std::tuple<uint32_t, std::string, uint8_t>> brightness_events;
    std::vector<std::tuple<uint32_t, std::string, bool>> on_off_events;
    {
        std::lock_guard lock(mutex_);
        for (const auto output_id : output_ids) {
            auto output_it = outputs_.find(output_id);
            if ((output_it == outputs_.end()) || !output_it->second.backlight) {
                continue;
            }
            auto &output = output_it->second;
            const auto old_brightness = output.backlight_brightness;
            const auto old_on = output.backlight_on;
            reset_backlight_data_locked(output);
            if (old_brightness != output.backlight_brightness) {
                brightness_events.emplace_back(output.info.id, output.info.name, output.backlight_brightness);
            }
            if (old_on != output.backlight_on) {
                on_off_events.emplace_back(output.info.id, output.info.name, output.backlight_on);
            }
        }
    }

    for (const auto &[id, name, value] : brightness_events) {
        emit_backlight_brightness_changed(id, name, value);
    }
    for (const auto &[id, name, value] : on_off_events) {
        emit_backlight_on_off_changed(id, name, value);
    }

    return {};
}

std::expected<void, std::string> Display::function_load_data(double output_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: output_id(%1%)", output_id);

    auto parsed_output_id = validate_output_id_param(output_id);
    if (!parsed_output_id) {
        return std::unexpected(parsed_output_id.error());
    }

    std::vector<uint32_t> output_ids;
    {
        std::lock_guard lock(mutex_);
        auto result = collect_backlight_output_ids_locked(parsed_output_id.value());
        if (!result) {
            return std::unexpected(result.error());
        }
        output_ids = std::move(result.value());
    }

    std::vector<std::tuple<uint32_t, std::string, uint8_t>> brightness_events;
    std::vector<std::tuple<uint32_t, std::string, bool>> on_off_events;
    {
        std::lock_guard lock(mutex_);
        for (const auto output_id : output_ids) {
            auto output_it = outputs_.find(output_id);
            if ((output_it == outputs_.end()) || !output_it->second.backlight) {
                continue;
            }
            auto &output = output_it->second;
            const auto old_brightness = output.backlight_brightness;
            const auto old_on = output.backlight_on;
            load_backlight_data_from_storage_locked(output);
            if (!apply_backlight_state_to_hal(output)) {
                BROOKESIA_LOGW("Failed to apply Display backlight state for output %1%", output.info.name);
            }
            if (old_brightness != output.backlight_brightness) {
                brightness_events.emplace_back(output.info.id, output.info.name, output.backlight_brightness);
            }
            if (old_on != output.backlight_on) {
                on_off_events.emplace_back(output.info.id, output.info.name, output.backlight_on);
            }
        }
    }

    for (const auto &[id, name, value] : brightness_events) {
        emit_backlight_brightness_changed(id, name, value);
    }
    for (const auto &[id, name, value] : on_off_events) {
        emit_backlight_on_off_changed(id, name, value);
    }

    return {};
}

void Display::bind_backlights_to_outputs_locked(
    std::vector<hal::InterfaceHandle<hal::display::BacklightIface>> backlights
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::vector<bool> used(backlights.size(), false);
    for (auto &[_, output] : outputs_) {
        if (output.info.slot != OutputSlot::HalPanel) {
            output.backlight_brightness = BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT;
            output.backlight_on = false;
            refresh_output_capability_locked(output);
            continue;
        }

        if (!output.info.group_id.empty()) {
            std::vector<size_t> matched_indices;
            for (size_t i = 0; i < backlights.size(); ++i) {
                if (used[i] || !backlights[i] || (backlights[i]->get_info().group_id != output.info.group_id)) {
                    continue;
                }
                matched_indices.push_back(i);
            }
            if (matched_indices.size() == 1) {
                const auto index = matched_indices.front();
                output.backlight = std::move(backlights[index]);
                used[index] = true;
                output.backlight_brightness = BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT;
                output.backlight_on = BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_DEFAULT_ON;
                BROOKESIA_LOGI(
                    "Bound display output %1% to backlight %2% by group_id(%3%)", output.info.name,
                    output.backlight.instance_name(), output.info.group_id
                );
                refresh_output_capability_locked(output);
                continue;
            }
            if (matched_indices.size() > 1) {
                BROOKESIA_LOGW(
                    "Display output %1% group_id(%2%) matches %3% backlights; leave unbound",
                    output.info.name, output.info.group_id, matched_indices.size()
                );
            }
            output.backlight_brightness = BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT;
            output.backlight_on = false;
            refresh_output_capability_locked(output);
            continue;
        }

        size_t fallback_index = backlights.size();
        for (size_t i = 0; i < backlights.size(); ++i) {
            if (used[i] || !backlights[i] || !backlights[i]->get_info().group_id.empty()) {
                continue;
            }
            fallback_index = i;
            break;
        }
        if (fallback_index == backlights.size()) {
            output.backlight_brightness = BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT;
            output.backlight_on = false;
            refresh_output_capability_locked(output);
            continue;
        }

        output.backlight = std::move(backlights[fallback_index]);
        used[fallback_index] = true;
        output.backlight_brightness = BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT;
        output.backlight_on = BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_DEFAULT_ON;
        BROOKESIA_LOGI(
            "Bound display output %1% to backlight %2% by legacy order fallback", output.info.name,
            output.backlight.instance_name()
        );
        refresh_output_capability_locked(output);
    }

    for (size_t i = 0; i < backlights.size(); ++i) {
        if (!used[i] && backlights[i]) {
            BROOKESIA_LOGW(
                "Unused display backlight interface: %1%, group_id=%2%", backlights[i].instance_name(),
                backlights[i]->get_info().group_id
            );
        }
    }
}

bool Display::apply_backlight_state_to_hal(OutputContext &output)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!output.backlight) {
        return false;
    }

    const auto hw_value = map_percentage_to_hardware(
                              output.backlight_brightness,
                              BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MIN,
                              BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MAX
                          );
    if (output.backlight_on) {
        if (!output.backlight->set_brightness(hw_value)) {
            BROOKESIA_LOGW("Failed to set backlight brightness to %1% for output %2%", hw_value, output.info.name);
            return false;
        }

        if (!output.backlight->is_light_on_off_supported()) {
            return true;
        }
        if (output.backlight->set_light_on_off(true)) {
            return true;
        }

        BROOKESIA_LOGW("Failed to turn on supported backlight explicitly for output %1%", output.info.name);
        return true;
    }

    if (!output.backlight->is_light_on_off_supported()) {
        if (!output.backlight->set_brightness(0)) {
            BROOKESIA_LOGW("Failed to apply fallback backlight brightness(0) for output %1%", output.info.name);
            return false;
        }
        return true;
    }
    if (!output.backlight->set_brightness(hw_value)) {
        BROOKESIA_LOGW("Failed to update supported backlight brightness cache to %1%", hw_value);
        return false;
    }
    if (output.backlight->set_light_on_off(false)) {
        return true;
    }

    BROOKESIA_LOGW("Failed to turn off supported backlight explicitly for output %1%", output.info.name);
    if (!output.backlight->set_brightness(0)) {
        BROOKESIA_LOGW("Failed to apply fallback backlight brightness(0) for output %1%", output.info.name);
        return false;
    }

    return true;
}

void Display::load_backlight_data_from_storage_locked(OutputContext &output)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!helper::Storage::is_available()) {
        BROOKESIA_LOGD("Storage service is not available, skip loading Display backlight state");
        return;
    }

    const auto storage_output_id = get_backlight_storage_output_id(output);
    auto namespace_result = make_backlight_storage_namespace();
    if (!namespace_result) {
        BROOKESIA_LOGW("%1%", namespace_result.error());
        return;
    }
    const auto &kv_namespace = namespace_result.value();
    {
        auto key_result = make_backlight_storage_key(storage_output_id, KEY_BRIGHTNESS);
        if (!key_result) {
            BROOKESIA_LOGW("Failed to make Display backlight brightness Storage key: %1%", key_result.error());
            return;
        }
        const auto &key = key_result.value();
        auto result = helper::Storage::get_key_value<int32_t>(kv_namespace, key);
        if (!result) {
            BROOKESIA_LOGD("No persisted '%1%' in Storage: %2%", key, result.error());
        } else if ((result.value() < 0) || (result.value() > 100)) {
            BROOKESIA_LOGW("Ignored out-of-range persisted '%1%': %2%", key, result.value());
        } else {
            output.backlight_brightness = static_cast<uint8_t>(result.value());
            BROOKESIA_LOGI(
                "Loaded '%1%' from Storage for output %2%: %3%", KEY_BRIGHTNESS, output.info.name,
                output.backlight_brightness
            );
        }
    }

    {
        auto key_result = make_backlight_storage_key(storage_output_id, KEY_ON);
        if (!key_result) {
            BROOKESIA_LOGW("Failed to make Display backlight on/off Storage key: %1%", key_result.error());
            return;
        }
        const auto &key = key_result.value();
        auto result = helper::Storage::get_key_value<bool>(kv_namespace, key);
        if (!result) {
            BROOKESIA_LOGD("No persisted '%1%' in Storage: %2%", key, result.error());
        } else {
            output.backlight_on = result.value();
            BROOKESIA_LOGI(
                "Loaded '%1%' from Storage for output %2%: %3%", KEY_ON, output.info.name,
                output.backlight_on
            );
        }
    }
}

void Display::save_backlight_brightness_data(const std::string &storage_output_id, uint8_t brightness)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!helper::Storage::is_available()) {
        BROOKESIA_LOGD("Storage service is not available, skip");
        return;
    }

    auto key_result = make_backlight_storage_key(storage_output_id, KEY_BRIGHTNESS);
    if (!key_result) {
        BROOKESIA_LOGW("Failed to make Display backlight brightness Storage key: %1%", key_result.error());
        return;
    }
    auto namespace_result = make_backlight_storage_namespace();
    if (!namespace_result) {
        BROOKESIA_LOGW("%1%", namespace_result.error());
        return;
    }
    const auto key = std::move(key_result.value());
    auto result = helper::Storage::save_key_value_async(namespace_result.value(), key, brightness,
    [key](auto &&result) mutable {
        BROOKESIA_CHECK_FALSE_EXIT(result.success, "Failed to save %1% to Storage: %2%", key,
                                   result.error_message);
        BROOKESIA_LOGI("Saved '%1%' to Storage", key);
    });
    BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to save Display backlight brightness to Storage");
}

void Display::save_backlight_on_off_data(const std::string &storage_output_id, bool on)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!helper::Storage::is_available()) {
        BROOKESIA_LOGD("Storage service is not available, skip");
        return;
    }

    auto key_result = make_backlight_storage_key(storage_output_id, KEY_ON);
    if (!key_result) {
        BROOKESIA_LOGW("Failed to make Display backlight on/off Storage key: %1%", key_result.error());
        return;
    }
    auto namespace_result = make_backlight_storage_namespace();
    if (!namespace_result) {
        BROOKESIA_LOGW("%1%", namespace_result.error());
        return;
    }
    const auto key = std::move(key_result.value());
    auto result = helper::Storage::save_key_value_async(namespace_result.value(), key, on,
    [key](auto &&result) mutable {
        BROOKESIA_CHECK_FALSE_EXIT(result.success, "Failed to save %1% to Storage: %2%", key,
                                   result.error_message);
        BROOKESIA_LOGI("Saved '%1%' to Storage", key);
    });
    BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to save Display backlight on/off state to Storage");
}

void Display::reset_backlight_data_locked(OutputContext &output)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    output.backlight_brightness = BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT;
    output.backlight_on = BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_DEFAULT_ON;
    if (!apply_backlight_state_to_hal(output)) {
        BROOKESIA_LOGW("Failed to restore default backlight state for output %1%", output.info.name);
    }
}

std::expected<std::vector<uint32_t>, std::string> Display::collect_backlight_output_ids_locked(
    uint32_t output_id
) const
{
    std::vector<uint32_t> output_ids;
    if (output_id == 0) {
        for (const auto &[output_id, output] : outputs_) {
            if (output.backlight) {
                output_ids.push_back(output_id);
            }
        }
        return output_ids;
    }

    auto output_it = outputs_.find(output_id);
    if (output_it == outputs_.end()) {
        return std::unexpected((boost::format("Display output id %1% is not available") % output_id).str());
    }
    const auto &output = output_it->second;
    if (!output.backlight) {
        return std::unexpected((boost::format("Display output '%1%' has no backlight") % output.info.name).str());
    }
    output_ids.push_back(output_id);
    return output_ids;
}

std::string Display::get_backlight_storage_output_id(const OutputContext &output) const
{
    if (!output.info.group_id.empty()) {
        return output.info.group_id;
    }
    return output.info.name;
}

std::expected<std::string, std::string> Display::make_backlight_storage_key(
    std::string_view storage_output_id, std::string_view key
) const
{
    auto result = helper::Storage::make_kv_key({KEY_PREFIX_BACKLIGHT, storage_output_id, key});
    if (!result) {
        return std::unexpected(
                   (boost::format("Failed to make Display backlight Storage key '%1%.%2%.%3%': %4%") %
                    KEY_PREFIX_BACKLIGHT % storage_output_id % key % result.error()).str()
               );
    }
    if (result->hashed) {
        BROOKESIA_LOGW("%1%", result->warning);
    }
    return result->name;
}

std::expected<std::string, std::string> Display::make_backlight_storage_namespace() const
{
    auto result = helper::Storage::make_kv_namespace({get_attributes().name});
    if (!result) {
        return std::unexpected(
                   (boost::format("Failed to make Display Storage namespace '%1%': %2%") %
                    get_attributes().name % result.error()).str()
               );
    }
    if (result->hashed) {
        BROOKESIA_LOGW("%1%", result->warning);
    }
    return result->name;
}

bool Display::emit_backlight_brightness_changed(uint32_t output_id, const std::string &output_name, uint8_t brightness)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto result = publish_event(
                      BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::BacklightBrightnessChanged),
    std::vector<EventItem> {
        EventItem(static_cast<double>(output_id)),
        EventItem(output_name),
        EventItem(static_cast<double>(brightness)),
    }
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish backlight brightness changed event");

    return true;
}

bool Display::emit_backlight_on_off_changed(uint32_t output_id, const std::string &output_name, bool on)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto result = publish_event(
                      BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::BacklightOnOffChanged),
    std::vector<EventItem> {
        EventItem(static_cast<double>(output_id)),
        EventItem(output_name),
        EventItem(on),
    }
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish backlight on/off changed event");

    return true;
}

std::expected<uint8_t, std::string> Display::validate_percentage(double value, const char *name) const
{
    if (!std::isfinite(value)) {
        return std::unexpected(std::string(name) + " must be finite");
    }
    if ((value < 0.0) || (value > 100.0)) {
        return std::unexpected(std::string(name) + " must be in range [0, 100]");
    }

    return static_cast<uint8_t>(std::lround(value));
}

uint8_t Display::map_percentage_to_hardware(uint8_t percentage, uint8_t min, uint8_t max)
{
    return static_cast<uint8_t>(min + static_cast<int>(percentage) * (max - min) / 100);
}

} // namespace esp_brookesia::service
