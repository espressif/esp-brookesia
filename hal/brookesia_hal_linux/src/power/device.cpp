/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "brookesia/hal_linux/macro_configs.h"
#if !BROOKESIA_HAL_LINUX_POWER_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_interface/interfaces/power/battery.hpp"
#include "brookesia/hal_linux/power/device.hpp"

namespace esp_brookesia::hal {

namespace {

constexpr const char *POWER_SUPPLY_ROOT = "/sys/class/power_supply";

#if BROOKESIA_HAL_LINUX_POWER_BACKEND_STUB
power::BatteryIface::Info make_stub_battery_info()
{
    return power::BatteryIface::Info{
        .name = "Linux battery",
        .chemistry = "Li-ion",
        .abilities = {
            power::BatteryIface::Ability::Voltage,
            power::BatteryIface::Ability::Percentage,
            power::BatteryIface::Ability::PowerSource,
            power::BatteryIface::Ability::ChargeState,
            power::BatteryIface::Ability::VbusVoltage,
            power::BatteryIface::Ability::ChargerControl,
            power::BatteryIface::Ability::ChargeConfig,
        },
    };
}
#endif

power::BatteryIface::State make_stub_battery_state()
{
    return power::BatteryIface::State{
        .is_present = true,
        .power_source = power::BatteryIface::PowerSource::External,
        .charge_state = power::BatteryIface::ChargeState::ConstantCurrent,
        .level_source = power::BatteryIface::LevelSource::FuelGauge,
        .voltage_mv = 3850,
        .percentage = 67,
        .vbus_voltage_mv = 5000,
        .system_voltage_mv = std::nullopt,
        .is_low = false,
        .is_critical = false,
    };
}

power::BatteryIface::ChargeConfig make_stub_charge_config()
{
    return power::BatteryIface::ChargeConfig{
        .enabled = true,
        .target_voltage_mv = 4100,
        .charge_current_ma = 400,
        .precharge_current_ma = 50,
        .termination_current_ma = 25,
    };
}

#if !BROOKESIA_HAL_LINUX_POWER_BACKEND_STUB
std::optional<std::string> read_text_file(const std::filesystem::path &path)
{
    std::ifstream input(path);
    if (!input) {
        return std::nullopt;
    }
    std::string value;
    std::getline(input, value);
    while (!value.empty() && ((value.back() == '\n') || (value.back() == '\r') || (value.back() == ' '))) {
        value.pop_back();
    }
    return value;
}

std::optional<int64_t> read_integer_file(const std::filesystem::path &path)
{
    const auto text = read_text_file(path);
    if (!text.has_value()) {
        return std::nullopt;
    }
    try {
        return std::stoll(text.value());
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

std::vector<std::filesystem::path> find_power_supply_entries(const std::string &type)
{
    std::vector<std::filesystem::path> entries;
    std::error_code error;
    if (!std::filesystem::exists(POWER_SUPPLY_ROOT, error)) {
        return entries;
    }

    for (const auto &entry : std::filesystem::directory_iterator(POWER_SUPPLY_ROOT, error)) {
        if (error || !entry.is_directory()) {
            continue;
        }
        auto supply_type = read_text_file(entry.path() / "type");
        if (supply_type.has_value() && (supply_type.value() == type)) {
            entries.push_back(entry.path());
        }
    }
    std::sort(entries.begin(), entries.end());
    return entries;
}

power::BatteryIface::ChargeState parse_charge_state(const std::string &status)
{
    if (status == "Charging") {
        return power::BatteryIface::ChargeState::Charging;
    }
    if (status == "Full") {
        return power::BatteryIface::ChargeState::Full;
    }
    if ((status == "Discharging") || (status == "Not charging")) {
        return power::BatteryIface::ChargeState::NotCharging;
    }
    return power::BatteryIface::ChargeState::Unknown;
}

bool is_external_power_online(const std::vector<std::filesystem::path> &external_entries)
{
    return std::any_of(external_entries.begin(), external_entries.end(), [](const auto & entry) {
        const auto online = read_integer_file(entry / "online");
        return online.has_value() && (online.value() > 0);
    });
}

std::optional<uint32_t> read_mv_from_microvolt_file(const std::filesystem::path &path)
{
    const auto value = read_integer_file(path);
    if (!value.has_value()) {
        return std::nullopt;
    }
    return static_cast<uint32_t>(std::max<int64_t>(0, value.value() / 1000));
}

power::BatteryIface::Info make_real_battery_info()
{
    auto batteries = find_power_supply_entries("Battery");
    if (batteries.empty()) {
        return power::BatteryIface::Info{
            .name = "Linux power supply",
            .chemistry = "Unknown",
            .abilities = {
                power::BatteryIface::Ability::PowerSource,
                power::BatteryIface::Ability::ChargeState,
                power::BatteryIface::Ability::VbusVoltage,
            },
        };
    }

    const auto &battery = batteries.front();
    return power::BatteryIface::Info{
        .name = read_text_file(battery / "model_name").value_or(battery.filename().string()),
        .chemistry = read_text_file(battery / "technology").value_or("Unknown"),
        .abilities = {
            power::BatteryIface::Ability::Voltage,
            power::BatteryIface::Ability::Percentage,
            power::BatteryIface::Ability::PowerSource,
            power::BatteryIface::Ability::ChargeState,
            power::BatteryIface::Ability::VbusVoltage,
        },
    };
}

bool read_real_battery_state(power::BatteryIface::State &state)
{
    const auto batteries = find_power_supply_entries("Battery");
    auto external_entries = find_power_supply_entries("Mains");
    auto usb_entries = find_power_supply_entries("USB");
    external_entries.insert(external_entries.end(), usb_entries.begin(), usb_entries.end());

    const bool external_online = is_external_power_online(external_entries);
    state = power::BatteryIface::State{
        .is_present = !batteries.empty(),
        .power_source = external_online ? power::BatteryIface::PowerSource::External :
        (!batteries.empty() ? power::BatteryIface::PowerSource::Battery :
         power::BatteryIface::PowerSource::Unknown),
        .charge_state = external_online ? power::BatteryIface::ChargeState::Charging :
        power::BatteryIface::ChargeState::NotCharging,
        .level_source = !batteries.empty() ? power::BatteryIface::LevelSource::FuelGauge :
        power::BatteryIface::LevelSource::Unknown,
        .vbus_voltage_mv = external_online ? std::optional<uint32_t>(5000) : std::nullopt,
    };

    if (!batteries.empty()) {
        const auto &battery = batteries.front();
        if (const auto status = read_text_file(battery / "status"); status.has_value()) {
            state.charge_state = parse_charge_state(status.value());
        }
        if (const auto capacity = read_integer_file(battery / "capacity"); capacity.has_value()) {
            const auto percent = static_cast<uint8_t>(std::clamp<int64_t>(capacity.value(), 0, 100));
            state.percentage = percent;
            state.is_low = percent <= 20;
            state.is_critical = percent <= 5;
        }
        state.voltage_mv = read_mv_from_microvolt_file(battery / "voltage_now");
    }

    return !batteries.empty() || !external_entries.empty();
}
#endif

} // namespace

class PowerBatteryLinuxImpl: public power::BatteryIface {
public:
    PowerBatteryLinuxImpl()
        : power::BatteryIface(make_info())
        , state_(make_stub_battery_state())
        , charge_config_(make_stub_charge_config())
    {
#if !BROOKESIA_HAL_LINUX_POWER_BACKEND_STUB
        power::BatteryIface::State real_state;
        if (read_real_battery_state(real_state)) {
            state_ = real_state;
            real_backend_available_ = true;
            BROOKESIA_LOGI("Linux power backend initialized from %1%", POWER_SUPPLY_ROOT);
        } else {
            BROOKESIA_LOGW("Linux power backend unavailable, using deterministic stub state");
        }
#endif
    }

    bool get_state(State &state) override
    {
        std::lock_guard lock(mutex_);
#if !BROOKESIA_HAL_LINUX_POWER_BACKEND_STUB
        if (real_backend_available_) {
            power::BatteryIface::State real_state;
            if (read_real_battery_state(real_state)) {
                state_ = real_state;
            }
        }
#endif
        state = state_;
        return true;
    }

    bool get_charge_config(ChargeConfig &config) override
    {
        std::lock_guard lock(mutex_);
        config = charge_config_;
        return true;
    }

    bool set_charge_config(const ChargeConfig &config) override
    {
        std::lock_guard lock(mutex_);
#if !BROOKESIA_HAL_LINUX_POWER_BACKEND_STUB
        if (real_backend_available_) {
            BROOKESIA_LOGW("Linux power backend does not support charger parameter control");
            return false;
        }
#endif
        charge_config_ = config;
        state_.charge_state = config.enabled ? ChargeState::ConstantCurrent : ChargeState::NotCharging;
        return true;
    }

    bool set_charging_enabled(bool enabled) override
    {
        std::lock_guard lock(mutex_);
#if !BROOKESIA_HAL_LINUX_POWER_BACKEND_STUB
        if (real_backend_available_) {
            BROOKESIA_LOGW("Linux power backend does not support charger enable control");
            return false;
        }
#endif
        charge_config_.enabled = enabled;
        state_.charge_state = enabled ? ChargeState::ConstantCurrent : ChargeState::NotCharging;
        return true;
    }

private:
    static power::BatteryIface::Info make_info()
    {
#if !BROOKESIA_HAL_LINUX_POWER_BACKEND_STUB
        return make_real_battery_info();
#else
        return make_stub_battery_info();
#endif
    }

    std::mutex mutex_;
    State state_{};
    ChargeConfig charge_config_{};
#if !BROOKESIA_HAL_LINUX_POWER_BACKEND_STUB
    bool real_backend_available_ = false;
#endif
};

bool PowerLinuxDevice::probe()
{
    return true;
}

std::vector<InterfaceSpec> PowerLinuxDevice::get_interface_specs() const
{
    return {
        {power::BatteryIface::NAME, BATTERY_IFACE_NAME},
    };
}

bool PowerLinuxDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        battery_iface_ = std::make_shared<PowerBatteryLinuxImpl>(), false,
        "Failed to create power battery linux"
    );

    interfaces_.emplace(BATTERY_IFACE_NAME, battery_iface_);

    return true;
}

void PowerLinuxDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(BATTERY_IFACE_NAME);
    battery_iface_.reset();
}

#if BROOKESIA_HAL_LINUX_ENABLE_POWER_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, PowerLinuxDevice, PowerLinuxDevice::DEVICE_NAME, PowerLinuxDevice::get_instance(),
    BROOKESIA_HAL_LINUX_POWER_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
