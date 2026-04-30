/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_BATTERY_AXP2101_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "axp2101_power_manager.h"
#include "battery_axp2101_impl.hpp"

#if BROOKESIA_HAL_ADAPTOR_POWER_ENABLE_BATTERY && BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_IMPL_AXP2101
#include "esp_board_manager_includes.h"

extern "C" esp_err_t power_manager_get_battery_state(
    void *device_handle, power_manager_battery_state_t *state
) __attribute__((weak));
extern "C" esp_err_t power_manager_get_charge_config(
    void *device_handle, power_manager_battery_charge_config_t *config
) __attribute__((weak));
extern "C" esp_err_t power_manager_set_charge_config(
    void *device_handle, const power_manager_battery_charge_config_t *config
) __attribute__((weak));
extern "C" esp_err_t power_manager_set_charging_enabled(
    void *device_handle, bool enabled
) __attribute__((weak));

namespace esp_brookesia::hal {

namespace {
constexpr const char *AXP2101_POWER_MANAGER_NAME = "axp2101_power_manager";
constexpr const char *BATTERY_NAME = "AXP2101 Battery";
constexpr const char *BATTERY_CHEMISTRY = "Li-ion";

PowerBatteryIface::Info generate_info()
{
    return {
        .name = BATTERY_NAME,
        .chemistry = BATTERY_CHEMISTRY,
        .abilities = {
            PowerBatteryIface::Ability::Voltage,
            PowerBatteryIface::Ability::Percentage,
            PowerBatteryIface::Ability::PowerSource,
            PowerBatteryIface::Ability::ChargeState,
            PowerBatteryIface::Ability::VbusVoltage,
            PowerBatteryIface::Ability::SystemVoltage,
            PowerBatteryIface::Ability::ChargerControl,
            PowerBatteryIface::Ability::ChargeConfig,
        },
    };
}

PowerBatteryIface::PowerSource convert_power_source(power_manager_battery_power_source_t source)
{
    switch (source) {
    case POWER_MANAGER_BATTERY_POWER_SOURCE_BATTERY:
        return PowerBatteryIface::PowerSource::Battery;
    case POWER_MANAGER_BATTERY_POWER_SOURCE_EXTERNAL:
        return PowerBatteryIface::PowerSource::External;
    case POWER_MANAGER_BATTERY_POWER_SOURCE_UNKNOWN:
    default:
        return PowerBatteryIface::PowerSource::Unknown;
    }
}

PowerBatteryIface::ChargeState convert_charge_state(power_manager_battery_charge_state_t state)
{
    switch (state) {
    case POWER_MANAGER_BATTERY_CHARGE_STATE_NOT_CHARGING:
        return PowerBatteryIface::ChargeState::NotCharging;
    case POWER_MANAGER_BATTERY_CHARGE_STATE_CHARGING:
        return PowerBatteryIface::ChargeState::Charging;
    case POWER_MANAGER_BATTERY_CHARGE_STATE_TRICKLE:
        return PowerBatteryIface::ChargeState::Trickle;
    case POWER_MANAGER_BATTERY_CHARGE_STATE_PRE_CHARGE:
        return PowerBatteryIface::ChargeState::PreCharge;
    case POWER_MANAGER_BATTERY_CHARGE_STATE_CONSTANT_CURRENT:
        return PowerBatteryIface::ChargeState::ConstantCurrent;
    case POWER_MANAGER_BATTERY_CHARGE_STATE_CONSTANT_VOLTAGE:
        return PowerBatteryIface::ChargeState::ConstantVoltage;
    case POWER_MANAGER_BATTERY_CHARGE_STATE_FULL:
        return PowerBatteryIface::ChargeState::Full;
    case POWER_MANAGER_BATTERY_CHARGE_STATE_FAULT:
        return PowerBatteryIface::ChargeState::Fault;
    case POWER_MANAGER_BATTERY_CHARGE_STATE_UNKNOWN:
    default:
        return PowerBatteryIface::ChargeState::Unknown;
    }
}

void apply_low_critical_state(PowerBatteryIface::State &state)
{
    state.is_low = false;
    state.is_critical = false;
    if (!state.voltage_mv.has_value()) {
        return;
    }

    if (state.voltage_mv.value() <= BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_CRITICAL_VOLTAGE_MV) {
        state.is_critical = true;
        state.is_low = true;
    } else if (state.voltage_mv.value() <= BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_LOW_VOLTAGE_MV) {
        state.is_low = true;
    }
}
} // namespace

BatteryAxp2101Impl::BatteryAxp2101Impl()
    : PowerBatteryIface(generate_info())
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (!setup_power_manager()) {
        BROOKESIA_LOGW("AXP2101 power manager is not available");
        return;
    }

    BROOKESIA_LOGD("Info: %1%", get_info());
}

BatteryAxp2101Impl::~BatteryAxp2101Impl()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (device_initialized_) {
        auto ret = esp_board_manager_deinit_device_by_name(AXP2101_POWER_MANAGER_NAME);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, { BROOKESIA_LOGE("Failed to deinit AXP2101 power manager"); });
        device_initialized_ = false;
        device_handle_ = nullptr;
    }
}

bool BatteryAxp2101Impl::get_state(State &state)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "AXP2101 power manager is not initialized");
    BROOKESIA_CHECK_NULL_RETURN(power_manager_get_battery_state, false, "AXP2101 battery state API is unavailable");

    power_manager_battery_state_t pm_state = {};
    auto ret = power_manager_get_battery_state(device_handle_, &pm_state);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get AXP2101 battery state");

    State next = {
        .is_present = pm_state.is_present,
        .power_source = convert_power_source(pm_state.power_source),
        .charge_state = convert_charge_state(pm_state.charge_state),
        .level_source = pm_state.has_percentage ? LevelSource::FuelGauge : LevelSource::Unknown,
    };
    if (pm_state.has_voltage_mv) {
        next.voltage_mv = pm_state.voltage_mv;
    }
    if (pm_state.has_percentage) {
        next.percentage = std::min<uint8_t>(pm_state.percentage, 100);
    }
    if (pm_state.has_vbus_voltage_mv) {
        next.vbus_voltage_mv = pm_state.vbus_voltage_mv;
    }
    if (pm_state.has_system_voltage_mv) {
        next.system_voltage_mv = pm_state.system_voltage_mv;
    }

    apply_low_critical_state(next);

    state = next;

    return true;
}

bool BatteryAxp2101Impl::get_charge_config(ChargeConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "AXP2101 power manager is not initialized");
    BROOKESIA_CHECK_NULL_RETURN(power_manager_get_charge_config, false, "AXP2101 charge config API is unavailable");

    power_manager_battery_charge_config_t pm_config = {};
    auto ret = power_manager_get_charge_config(device_handle_, &pm_config);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get AXP2101 charge config");

    config = {
        .enabled = pm_config.enabled,
        .target_voltage_mv = pm_config.target_voltage_mv,
        .charge_current_ma = pm_config.charge_current_ma,
        .precharge_current_ma = pm_config.precharge_current_ma,
        .termination_current_ma = pm_config.termination_current_ma,
    };

    return true;
}

bool BatteryAxp2101Impl::set_charge_config(const ChargeConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", config);

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "AXP2101 power manager is not initialized");
    BROOKESIA_CHECK_NULL_RETURN(power_manager_set_charge_config, false, "AXP2101 set charge config API is unavailable");

    const power_manager_battery_charge_config_t pm_config = {
        .enabled = config.enabled,
        .target_voltage_mv = config.target_voltage_mv,
        .charge_current_ma = config.charge_current_ma,
        .precharge_current_ma = config.precharge_current_ma,
        .termination_current_ma = config.termination_current_ma,
    };

    auto ret = power_manager_set_charge_config(device_handle_, &pm_config);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to set AXP2101 charge config");

    return true;
}

bool BatteryAxp2101Impl::set_charging_enabled(bool enabled)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: enabled(%1%)", enabled);

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "AXP2101 power manager is not initialized");
    BROOKESIA_CHECK_NULL_RETURN(power_manager_set_charging_enabled, false, "AXP2101 charger control API is unavailable");

    auto ret = power_manager_set_charging_enabled(device_handle_, enabled);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to set AXP2101 charging state");

    return true;
}

bool BatteryAxp2101Impl::setup_power_manager()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_NULL_RETURN(power_manager_get_battery_state, false, "AXP2101 power manager battery API is unavailable");

    auto ret = esp_board_manager_init_device_by_name(AXP2101_POWER_MANAGER_NAME);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to init AXP2101 power manager");
    device_initialized_ = true;

    ret = esp_board_manager_get_device_handle(AXP2101_POWER_MANAGER_NAME, &device_handle_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get AXP2101 power manager handle");
    BROOKESIA_CHECK_NULL_RETURN(device_handle_, false, "Failed to get AXP2101 power manager handle");

    return true;
}

} // namespace esp_brookesia::hal
#endif // BROOKESIA_HAL_ADAPTOR_POWER_ENABLE_BATTERY && BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_IMPL_AXP2101
