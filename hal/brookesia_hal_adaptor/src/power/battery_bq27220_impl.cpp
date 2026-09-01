/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_BATTERY_BQ27220_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/board_custom_device_bridges.h"
#include "battery_bq27220_impl.hpp"

#if BROOKESIA_HAL_ADAPTOR_POWER_ENABLE_BATTERY && BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_IMPL_BQ27220
#include "esp_board_manager_includes.h"

namespace esp_brookesia::hal {

namespace {
constexpr const char *BQ27220_FUEL_GAUGE_NAME = "bq27220_fuel_gauge";
constexpr const char *BATTERY_NAME = "BQ27220 Battery";
constexpr const char *BATTERY_CHEMISTRY = "Li-ion";

power::BatteryIface::Info generate_info()
{
    return {
        .name = BATTERY_NAME,
        .chemistry = BATTERY_CHEMISTRY,
        .abilities = {
            power::BatteryIface::Ability::Voltage,
        },
    };
}

void apply_low_critical_state(power::BatteryIface::State &state)
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

BatteryBq27220Impl::BatteryBq27220Impl()
    : power::BatteryIface(generate_info())
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (!setup_fuel_gauge()) {
        BROOKESIA_LOGW("BQ27220 fuel gauge is not available");
        return;
    }

    BROOKESIA_LOGD("Info: %1%", get_info());
}

BatteryBq27220Impl::~BatteryBq27220Impl()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (device_initialized_) {
        auto ret = esp_board_manager_deinit_device_by_name(BQ27220_FUEL_GAUGE_NAME);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, { BROOKESIA_LOGE("Failed to deinit BQ27220 fuel gauge"); });
        device_initialized_ = false;
        device_handle_ = nullptr;
    }
}

bool BatteryBq27220Impl::get_state(State &state)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "BQ27220 fuel gauge is not initialized");
    BROOKESIA_CHECK_NULL_RETURN(
        bq27220_fuel_gauge_get_snapshot, false, "BQ27220 fuel gauge snapshot API is unavailable"
    );

    bq27220_fuel_gauge_snapshot_t snapshot = {};
    auto ret = bq27220_fuel_gauge_get_snapshot(device_handle_, &snapshot);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get BQ27220 battery state");

    State next = {
        .is_present = snapshot.is_present,
        .power_source = PowerSource::Unknown,
        .charge_state = ChargeState::Unknown,
        .level_source = LevelSource::Unknown,
    };
    if (snapshot.has_voltage_mv) {
        next.voltage_mv = snapshot.voltage_mv;
    }
    apply_low_critical_state(next);

    state = next;

    return true;
}

bool BatteryBq27220Impl::get_charge_config(ChargeConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    (void)config;

    return false;
}

bool BatteryBq27220Impl::set_charge_config(const ChargeConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", config);

    return false;
}

bool BatteryBq27220Impl::set_charging_enabled(bool enabled)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: enabled(%1%)", enabled);

    return false;
}

bool BatteryBq27220Impl::setup_fuel_gauge()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_NULL_RETURN(
        bq27220_fuel_gauge_get_snapshot, false, "BQ27220 fuel gauge snapshot API is unavailable"
    );

    if (!esp_board_manager_check_name(BQ27220_FUEL_GAUGE_NAME)) {
        BROOKESIA_LOGW("BQ27220 fuel gauge device not found, skip");
        return false;
    }

    auto ret = esp_board_manager_init_device_by_name(BQ27220_FUEL_GAUGE_NAME);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to init BQ27220 fuel gauge");
    device_initialized_ = true;

    ret = esp_board_manager_get_device_handle(BQ27220_FUEL_GAUGE_NAME, &device_handle_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get BQ27220 fuel gauge handle");
    BROOKESIA_CHECK_NULL_RETURN(device_handle_, false, "Failed to get BQ27220 fuel gauge handle");

    return true;
}

} // namespace esp_brookesia::hal
#endif // BROOKESIA_HAL_ADAPTOR_POWER_ENABLE_BATTERY && BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_IMPL_BQ27220
