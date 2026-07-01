/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <utility>

#include "brookesia/service_device/macro_configs.h"
#if !BROOKESIA_SERVICE_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/power/battery.hpp"
#include "brookesia/service_device/service_device.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::service {

std::expected<boost::json::object, std::string> Device::function_get_power_battery_info()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!ensure_power_battery_iface()) {
        return std::unexpected("Power battery interface is not available");
    }

    return BROOKESIA_DESCRIBE_TO_JSON(power_.battery_iface->get_info()).as_object();
}

std::expected<boost::json::object, std::string> Device::function_get_power_battery_state()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!ensure_power_battery_iface()) {
        return std::unexpected("Power battery interface is not available");
    }

    PowerBatteryState state;
    if (!power_.battery_iface->get_state(state)) {
        return std::unexpected("Failed to get power battery state");
    }

    return BROOKESIA_DESCRIBE_TO_JSON(state).as_object();
}

std::expected<boost::json::object, std::string> Device::function_get_power_battery_charge_config()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!ensure_power_battery_iface()) {
        return std::unexpected("Power battery interface is not available");
    }

    if (!power_.battery_iface->get_info().has_ability(hal::power::BatteryIface::Ability::ChargeConfig)) {
        return std::unexpected("Power battery charge config is not supported");
    }

    PowerBatteryChargeConfig config;
    if (!power_.battery_iface->get_charge_config(config)) {
        return std::unexpected("Failed to get power battery charge config");
    }

    return BROOKESIA_DESCRIBE_TO_JSON(config).as_object();
}

std::expected<void, std::string> Device::function_set_power_battery_charge_config(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!ensure_power_battery_iface()) {
        return std::unexpected("Power battery interface is not available");
    }

    if (!power_.battery_iface->get_info().has_ability(hal::power::BatteryIface::Ability::ChargeConfig)) {
        return std::unexpected("Power battery charge config is not supported");
    }

    PowerBatteryChargeConfig charge_config;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config, charge_config)) {
        return std::unexpected("Failed to parse power battery charge config");
    }
    if (!power_.battery_iface->set_charge_config(charge_config)) {
        return std::unexpected("Failed to set power battery charge config");
    }

    power_.cached_state.reset();
    power_.cached_charge_config.reset();

    PowerBatteryChargeConfig current_config;
    if (power_.battery_iface->get_charge_config(current_config)) {
        power_.cached_charge_config = current_config;
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

    if (!ensure_power_battery_iface()) {
        return std::unexpected("Power battery interface is not available");
    }

    if (!power_.battery_iface->get_info().has_ability(hal::power::BatteryIface::Ability::ChargerControl)) {
        return std::unexpected("Power battery charging control is not supported");
    }
    if (!power_.battery_iface->set_charging_enabled(enabled)) {
        return std::unexpected("Failed to set power battery charging state");
    }

    power_.cached_state.reset();
    power_.cached_charge_config.reset();

    PowerBatteryState state;
    if (power_.battery_iface->get_state(state)) {
        power_.cached_state = state;
        publish_power_battery_state_changed(state);
    } else {
        BROOKESIA_LOGW("Failed to read back power battery state after charging enable update");
    }

    if (power_.battery_iface->get_info().has_ability(hal::power::BatteryIface::Ability::ChargeConfig)) {
        PowerBatteryChargeConfig config;
        if (power_.battery_iface->get_charge_config(config)) {
            power_.cached_charge_config = config;
            publish_power_battery_charge_config_changed(config);
        } else {
            BROOKESIA_LOGW("Failed to read back power battery charge config after charging enable update");
        }
    }

    return {};
}

void Device::on_event_subscribed(const std::string &event_name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if ((event_name != BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::PowerBatteryStateChanged)) &&
            (event_name != BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::PowerBatteryChargeConfigChanged))) {
        return;
    }

    request_power_battery_polling();
}

void Device::request_power_battery_polling()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    power_.polling_requested = true;
    if (!is_running()) {
        BROOKESIA_LOGD("Device service is not running yet, defer power battery polling");
        return;
    }
    if (!start_power_battery_polling()) {
        BROOKESIA_LOGW("Failed to start power battery polling after request");
    }
}

bool Device::start_power_battery_polling()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (power_.poll_task_id != 0) {
        return true;
    }
    if (!hal::has_interface(hal::power::BatteryIface::NAME)) {
        BROOKESIA_LOGD("Power battery interface is not available, skip polling");
        return true;
    }
    BROOKESIA_CHECK_FALSE_RETURN(ensure_power_battery_iface(), false, "Failed to acquire power battery interface");

    auto scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Task scheduler is not available");

    auto polling_task = [this]() -> bool {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        if (!poll_power_battery_once())
        {
            BROOKESIA_LOGW("Power battery polling iteration failed");
        }
        return true;
    };
    auto result = scheduler->post_periodic(
                      std::move(polling_task),
                      BROOKESIA_SERVICE_DEVICE_POWER_BATTERY_POLL_INTERVAL_MS,
                      &power_.poll_task_id,
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

    if (power_.poll_task_id == 0) {
        return;
    }

    auto scheduler = get_task_scheduler();
    if (scheduler) {
        scheduler->cancel(power_.poll_task_id);
    }
    power_.poll_task_id = 0;
}

bool Device::poll_power_battery_once()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!power_.battery_iface) {
        return false;
    }

    PowerBatteryState state;
    if (!power_.battery_iface->get_state(state)) {
        BROOKESIA_LOGW("Failed to read power battery state");
        return false;
    }
    if (!power_.cached_state.has_value() ||
            !is_power_battery_state_equal(power_.cached_state.value(), state)) {
        power_.cached_state = state;
        BROOKESIA_CHECK_FALSE_RETURN(
            publish_power_battery_state_changed(state), false, "Failed to publish power battery state changed event"
        );
    }

    if (!power_.battery_iface->get_info().has_ability(hal::power::BatteryIface::Ability::ChargeConfig)) {
        return true;
    }

    PowerBatteryChargeConfig config;
    if (!power_.battery_iface->get_charge_config(config)) {
        BROOKESIA_LOGW("Failed to read power battery charge config");
        return false;
    }
    if (!power_.cached_charge_config.has_value() ||
            !is_power_battery_charge_config_equal(power_.cached_charge_config.value(), config)) {
        power_.cached_charge_config = config;
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

} // namespace esp_brookesia::service
