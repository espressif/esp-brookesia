/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

/**
 * @file battery.hpp
 * @brief Declares the battery HAL interface.
 */

namespace esp_brookesia::hal {

/**
 * @brief Battery interface for querying battery state and controlling charger settings.
 */
class PowerBatteryIface: public Interface {
public:
    static constexpr const char *NAME = "Power:Battery";  ///< Interface registry name.

    /**
     * @brief Battery backend capability.
     */
    enum class Ability {
        Voltage,        ///< Battery voltage reading.
        Percentage,     ///< Battery level percentage.
        PowerSource,    ///< Battery/external power source state.
        ChargeState,    ///< Charger state.
        VbusVoltage,    ///< VBUS voltage reading.
        SystemVoltage,  ///< System voltage reading.
        ChargerControl, ///< Charger enable/disable control.
        ChargeConfig,   ///< Charger parameter configuration.
    };

    /**
     * @brief Current power source.
     */
    enum class PowerSource {
        Unknown,  ///< Power source is unknown.
        Battery,  ///< Running from battery.
        External, ///< Running from external power.
    };

    /**
     * @brief Current charge state.
     */
    enum class ChargeState {
        Unknown,         ///< Charge state is unknown.
        NotCharging,     ///< Battery is not charging.
        Charging,        ///< Battery is charging, but the phase is unknown.
        Trickle,         ///< Trickle charging.
        PreCharge,       ///< Pre-charge phase.
        ConstantCurrent, ///< Constant-current phase.
        ConstantVoltage, ///< Constant-voltage phase.
        Full,            ///< Battery is full.
        Fault,           ///< Charger fault.
    };

    /**
     * @brief Source used to calculate the battery level.
     */
    enum class LevelSource {
        Unknown,      ///< Level source is unknown.
        FuelGauge,    ///< Level comes from a fuel gauge.
        VoltageCurve, ///< Level is estimated from a voltage curve.
    };

    /**
     * @brief Static battery capability information.
     */
    struct Info {
        std::string name;                  ///< Battery or backend name.
        std::string chemistry;             ///< Battery chemistry, such as Li-ion.
        std::vector<Ability> abilities;    ///< Supported backend abilities.

        /**
         * @brief Check whether an ability is supported.
         *
         * @param[in] ability Ability to query.
         * @return `true` if the ability is present; otherwise `false`.
         */
        bool has_ability(Ability ability) const
        {
            return std::find(abilities.begin(), abilities.end(), ability) != abilities.end();
        }
    };

    /**
     * @brief Runtime battery state snapshot.
     */
    struct State {
        bool is_present = false;                          ///< Whether a battery is detected.
        PowerSource power_source = PowerSource::Unknown; ///< Current power source.
        ChargeState charge_state = ChargeState::Unknown; ///< Current charge state.
        LevelSource level_source = LevelSource::Unknown; ///< Battery percentage source.
        std::optional<uint32_t> voltage_mv = std::nullopt; ///< Battery voltage in mV.
        std::optional<uint8_t> percentage = std::nullopt;  ///< Battery percentage in [0, 100].
        std::optional<uint32_t> vbus_voltage_mv = std::nullopt;   ///< VBUS voltage in mV.
        std::optional<uint32_t> system_voltage_mv = std::nullopt; ///< System voltage in mV.
        bool is_low = false;                             ///< Battery is below the low threshold.
        bool is_critical = false;                        ///< Battery is below the critical threshold.
    };

    /**
     * @brief Charger configuration.
     */
    struct ChargeConfig {
        bool enabled = false;                 ///< Charger enable state.
        uint32_t target_voltage_mv = 0;       ///< Target charge voltage in mV.
        uint32_t charge_current_ma = 0;       ///< Main charge current in mA.
        uint32_t precharge_current_ma = 0;    ///< Pre-charge current in mA.
        uint32_t termination_current_ma = 0;  ///< Termination current in mA.
    };

    /**
     * @brief Construct a battery interface.
     *
     * @param[in] info Static battery capability information.
     */
    explicit PowerBatteryIface(Info info)
        : Interface(NAME)
        , info_(std::move(info))
    {
    }

    /**
     * @brief Virtual destructor for polymorphic battery interfaces.
     */
    virtual ~PowerBatteryIface() = default;

    /**
     * @brief Get static battery capability information.
     *
     * @return Battery information.
     */
    const Info &get_info() const
    {
        return info_;
    }

    /**
     * @brief Get the current battery state.
     *
     * @param[out] state Runtime battery state snapshot.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool get_state(State &state) = 0;

    /**
     * @brief Get charger configuration.
     *
     * @param[out] config Charger configuration.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool get_charge_config(ChargeConfig &config) = 0;

    /**
     * @brief Set charger configuration.
     *
     * @param[in] config Charger configuration.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool set_charge_config(const ChargeConfig &config) = 0;

    /**
     * @brief Enable or disable charging.
     *
     * @param[in] enabled `true` to enable charging; `false` to disable charging.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool set_charging_enabled(bool enabled) = 0;

private:
    Info info_{};
};

BROOKESIA_DESCRIBE_ENUM(
    PowerBatteryIface::Ability, Voltage, Percentage, PowerSource, ChargeState, VbusVoltage, SystemVoltage, ChargerControl,
    ChargeConfig
);
BROOKESIA_DESCRIBE_ENUM(PowerBatteryIface::PowerSource, Unknown, Battery, External);
BROOKESIA_DESCRIBE_ENUM(
    PowerBatteryIface::ChargeState, Unknown, NotCharging, Charging, Trickle, PreCharge, ConstantCurrent, ConstantVoltage,
    Full, Fault
);
BROOKESIA_DESCRIBE_ENUM(PowerBatteryIface::LevelSource, Unknown, FuelGauge, VoltageCurve);
BROOKESIA_DESCRIBE_STRUCT(PowerBatteryIface::Info, (), (name, chemistry, abilities));
BROOKESIA_DESCRIBE_STRUCT(
    PowerBatteryIface::State, (),
    (
        is_present, power_source, charge_state, level_source, voltage_mv, percentage, vbus_voltage_mv,
        system_voltage_mv, is_low, is_critical
    )
);
BROOKESIA_DESCRIBE_STRUCT(
    PowerBatteryIface::ChargeConfig, (),
    (enabled, target_voltage_mv, charge_current_ma, precharge_current_ma, termination_current_ma)
);

} // namespace esp_brookesia::hal
