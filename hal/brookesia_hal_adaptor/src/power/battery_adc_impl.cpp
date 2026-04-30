/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <array>
#include <cstdint>
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_BATTERY_ADC_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "battery_adc_impl.hpp"

#if BROOKESIA_HAL_ADAPTOR_POWER_ENABLE_BATTERY && BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_IMPL_ADC && \
    defined(CONFIG_ESP_BOARD_PERIPH_ADC_SUPPORT)
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_board_manager_includes.h"

namespace esp_brookesia::hal {

namespace {
constexpr const char *ADC_BATTERY_VOLTAGE_NAME = "adc_battery_voltage";
constexpr const char *ADC_BATTERY_CHARGE_NAME = "adc_battery_charge";
constexpr const char *BATTERY_NAME = "ADC Battery";
constexpr const char *BATTERY_CHEMISTRY = "Li-ion";
constexpr uint32_t PERCENTAGE_VOLTAGE_MIN_MV =
    BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_ADC_PERCENTAGE_VOLTAGE_MIN_MV;
constexpr uint32_t PERCENTAGE_VOLTAGE_MAX_MV =
    BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_ADC_PERCENTAGE_VOLTAGE_MAX_MV;
constexpr int ADC_SAMPLE_COUNT = BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_ADC_SAMPLE_COUNT;
constexpr int ADC_RAW_MAX = BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_ADC_RAW_MAX;
constexpr int ADC_REF_VOLTAGE_MV = BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_ADC_REF_VOLTAGE_MV;

bool is_charge_adc_config_available()
{
    void *config = nullptr;
    return esp_board_periph_get_config(ADC_BATTERY_CHARGE_NAME, &config) == ESP_OK;
}

PowerBatteryIface::Info generate_info()
{
    PowerBatteryIface::Info info = {
        .name = BATTERY_NAME,
        .chemistry = BATTERY_CHEMISTRY,
        .abilities = {
            PowerBatteryIface::Ability::Voltage,
            PowerBatteryIface::Ability::Percentage,
        },
    };

    if (is_charge_adc_config_available()) {
        info.abilities.push_back(PowerBatteryIface::Ability::ChargeState);
    }

    return info;
}

periph_adc_handle_t *get_adc_handle(void *handle)
{
    return reinterpret_cast<periph_adc_handle_t *>(handle);
}

periph_adc_config_t *get_adc_config(void *config)
{
    return reinterpret_cast<periph_adc_config_t *>(config);
}

adc_cali_handle_t get_adc_cali_handle(void *handle)
{
    return reinterpret_cast<adc_cali_handle_t>(handle);
}

bool is_valid_oneshot_adc_config(periph_adc_config_t *config)
{
    return (config != nullptr) && (config->role == ESP_BOARD_PERIPH_ROLE_ONESHOT);
}

uint8_t estimate_percentage(uint32_t voltage_mv)
{
    if (PERCENTAGE_VOLTAGE_MAX_MV <= PERCENTAGE_VOLTAGE_MIN_MV) {
        return (voltage_mv <= PERCENTAGE_VOLTAGE_MIN_MV) ? 0 : 100;
    }

    if (voltage_mv <= PERCENTAGE_VOLTAGE_MIN_MV) {
        return 0;
    }
    if (voltage_mv >= PERCENTAGE_VOLTAGE_MAX_MV) {
        return 100;
    }

    const uint32_t range = PERCENTAGE_VOLTAGE_MAX_MV - PERCENTAGE_VOLTAGE_MIN_MV;
    return static_cast<uint8_t>(((voltage_mv - PERCENTAGE_VOLTAGE_MIN_MV) * 100) / range);
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

BatteryAdcImpl::BatteryAdcImpl()
    : PowerBatteryIface(generate_info())
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (!setup_voltage_adc()) {
        BROOKESIA_LOGW("Battery voltage ADC is not available");
        return;
    }

    if (!setup_charge_adc()) {
        BROOKESIA_LOGW("Battery charge ADC is not available, charge state will be unknown");
    }

    BROOKESIA_LOGD("Info: %1%", get_info());
}

BatteryAdcImpl::~BatteryAdcImpl()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (charge_adc_cali_handle_ != nullptr) {
        delete_adc_calibration(charge_adc_cali_handle_, charge_adc_cali_scheme_);
        charge_adc_cali_handle_ = nullptr;
        charge_adc_cali_scheme_ = CalibrationScheme::None;
    }
    if (voltage_adc_cali_handle_ != nullptr) {
        delete_adc_calibration(voltage_adc_cali_handle_, voltage_adc_cali_scheme_);
        voltage_adc_cali_handle_ = nullptr;
        voltage_adc_cali_scheme_ = CalibrationScheme::None;
    }
    if (charge_adc_handle_ != nullptr) {
        auto ret = esp_board_periph_unref_handle(ADC_BATTERY_CHARGE_NAME);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, { BROOKESIA_LOGE("Failed to unref charge ADC"); });
        charge_adc_handle_ = nullptr;
    }
    if (voltage_adc_handle_ != nullptr) {
        auto ret = esp_board_periph_unref_handle(ADC_BATTERY_VOLTAGE_NAME);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, { BROOKESIA_LOGE("Failed to unref voltage ADC"); });
        voltage_adc_handle_ = nullptr;
    }
}

bool BatteryAdcImpl::get_state(State &state)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(is_voltage_adc_valid_internal(), false, "Battery voltage ADC is not initialized");

    State next = {
        .is_present = true,
        .power_source = PowerSource::Unknown,
        .charge_state = ChargeState::Unknown,
        .level_source = LevelSource::VoltageCurve,
    };

    uint32_t voltage_mv = 0;
    BROOKESIA_CHECK_FALSE_RETURN(read_voltage_adc_mv(voltage_mv), false, "Failed to read battery voltage");

    next.voltage_mv = voltage_mv;
    next.percentage = estimate_percentage(voltage_mv);

    if (is_charge_adc_valid_internal()) {
        uint32_t charge_voltage_mv = 0;
        if (read_charge_adc_mv(charge_voltage_mv)) {
            next.charge_state = (charge_voltage_mv <= BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_ADC_CHARGE_DETECT_MV) ?
                                ChargeState::Charging : ChargeState::NotCharging;
        }
    }

    apply_low_critical_state(next);

    state = next;

    return true;
}

bool BatteryAdcImpl::get_charge_config(ChargeConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    config = {};

    return false;
}

bool BatteryAdcImpl::set_charge_config(const ChargeConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", config);

    return false;
}

bool BatteryAdcImpl::set_charging_enabled(bool enabled)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: enabled(%1%)", enabled);

    return false;
}

bool BatteryAdcImpl::setup_voltage_adc()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto ret = esp_board_periph_ref_handle(ADC_BATTERY_VOLTAGE_NAME, &voltage_adc_handle_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to ref battery voltage ADC");
    BROOKESIA_CHECK_NULL_RETURN(voltage_adc_handle_, false, "Failed to get battery voltage ADC handle");

    ret = esp_board_periph_get_config(ADC_BATTERY_VOLTAGE_NAME, &voltage_adc_config_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get battery voltage ADC config");
    BROOKESIA_CHECK_NULL_RETURN(voltage_adc_config_, false, "Failed to get battery voltage ADC config");

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_oneshot_adc_config(get_adc_config(voltage_adc_config_)), false,
                                 "Battery voltage ADC only supports oneshot role");

    if (!create_adc_calibration(voltage_adc_config_, &voltage_adc_cali_handle_, voltage_adc_cali_scheme_)) {
        BROOKESIA_LOGW("Battery voltage ADC calibration is unavailable, fallback to raw conversion");
    }

    return true;
}

bool BatteryAdcImpl::setup_charge_adc()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto ret = esp_board_periph_ref_handle(ADC_BATTERY_CHARGE_NAME, &charge_adc_handle_);
    if (ret != ESP_OK) {
        return false;
    }
    BROOKESIA_CHECK_NULL_RETURN(charge_adc_handle_, false, "Failed to get battery charge ADC handle");

    ret = esp_board_periph_get_config(ADC_BATTERY_CHARGE_NAME, &charge_adc_config_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get battery charge ADC config");
    BROOKESIA_CHECK_NULL_RETURN(charge_adc_config_, false, "Failed to get battery charge ADC config");

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_oneshot_adc_config(get_adc_config(charge_adc_config_)), false,
                                 "Battery charge ADC only supports oneshot role");

    if (!create_adc_calibration(charge_adc_config_, &charge_adc_cali_handle_, charge_adc_cali_scheme_)) {
        BROOKESIA_LOGW("Battery charge ADC calibration is unavailable, fallback to raw conversion");
    }

    return true;
}

bool BatteryAdcImpl::read_voltage_adc_mv(uint32_t &voltage_mv)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    uint32_t adc_voltage_mv = 0;
    BROOKESIA_CHECK_FALSE_RETURN(
        read_adc_mv(voltage_adc_handle_, voltage_adc_config_, voltage_adc_cali_handle_, adc_voltage_mv), false,
        "Failed to read battery voltage ADC"
    );

    voltage_mv = (adc_voltage_mv * BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_ADC_VOLTAGE_SCALE_MUL) /
                 BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_ADC_VOLTAGE_SCALE_DIV;

    return true;
}

bool BatteryAdcImpl::read_charge_adc_mv(uint32_t &voltage_mv)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return read_adc_mv(charge_adc_handle_, charge_adc_config_, charge_adc_cali_handle_, voltage_mv);
}

bool BatteryAdcImpl::read_adc_mv(void *adc_handle, void *adc_config, void *adc_cali_handle, uint32_t &voltage_mv)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_NULL_RETURN(adc_handle, false, "ADC handle is null");
    BROOKESIA_CHECK_NULL_RETURN(adc_config, false, "ADC config is null");

    auto *handle = get_adc_handle(adc_handle);
    auto *config = get_adc_config(adc_config);
    BROOKESIA_CHECK_NULL_RETURN(handle, false, "ADC handle is null");
    BROOKESIA_CHECK_NULL_RETURN(handle->oneshot, false, "ADC oneshot handle is null");
    BROOKESIA_CHECK_FALSE_RETURN(is_valid_oneshot_adc_config(config), false, "ADC config is invalid");

    int64_t raw_sum = 0;
    for (int i = 0; i < ADC_SAMPLE_COUNT; ++i) {
        int raw = 0;
        auto ret = adc_oneshot_read(handle->oneshot, config->cfg.oneshot.channel_id, &raw);
        BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to read ADC raw value");
        raw_sum += raw;
    }

    const int raw_avg = static_cast<int>(raw_sum / ADC_SAMPLE_COUNT);
    auto cali_handle = get_adc_cali_handle(adc_cali_handle);
    if (cali_handle != nullptr) {
        int calibrated_mv = 0;
        auto ret = adc_cali_raw_to_voltage(cali_handle, raw_avg, &calibrated_mv);
        if ((ret == ESP_OK) && (calibrated_mv >= 0)) {
            voltage_mv = static_cast<uint32_t>(calibrated_mv);
            return true;
        }
        BROOKESIA_LOGW("ADC calibration conversion failed, fallback to raw conversion: %1%", esp_err_to_name(ret));
    }

    voltage_mv = static_cast<uint32_t>((raw_avg * ADC_REF_VOLTAGE_MV) / ADC_RAW_MAX);

    return true;
}

bool BatteryAdcImpl::create_adc_calibration(
    void *adc_config, void **adc_cali_handle, CalibrationScheme &scheme
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_NULL_RETURN(adc_config, false, "ADC config is null");
    BROOKESIA_CHECK_NULL_RETURN(adc_cali_handle, false, "ADC calibration output is null");

    *adc_cali_handle = nullptr;
    scheme = CalibrationScheme::None;

    auto *config = get_adc_config(adc_config);

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = config->cfg.oneshot.unit_cfg.unit_id,
        .chan = config->cfg.oneshot.channel_id,
        .atten = config->cfg.oneshot.chan_cfg.atten,
        .bitwidth = config->cfg.oneshot.chan_cfg.bitwidth,
    };
    auto ret = adc_cali_create_scheme_curve_fitting(
                   &cali_config, reinterpret_cast<adc_cali_handle_t *>(adc_cali_handle)
               );
    if (ret == ESP_OK) {
        scheme = CalibrationScheme::CurveFitting;
        return true;
    }
    BROOKESIA_LOGW("Failed to create curve fitting ADC calibration: %1%", esp_err_to_name(ret));
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = config->cfg.oneshot.unit_cfg.unit_id,
        .atten = config->cfg.oneshot.chan_cfg.atten,
        .bitwidth = config->cfg.oneshot.chan_cfg.bitwidth,
        .default_vref = 0,
    };
    auto ret = adc_cali_create_scheme_line_fitting(&cali_config, reinterpret_cast<adc_cali_handle_t *>(adc_cali_handle));
    if (ret == ESP_OK) {
        scheme = CalibrationScheme::LineFitting;
        return true;
    }
    BROOKESIA_LOGW("Failed to create line fitting ADC calibration: %1%", esp_err_to_name(ret));
#else
    BROOKESIA_LOGW("No supported ADC calibration scheme");
#endif

    return false;
}

void BatteryAdcImpl::delete_adc_calibration(void *adc_cali_handle, CalibrationScheme scheme)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (adc_cali_handle == nullptr) {
        return;
    }

    switch (scheme) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    case CalibrationScheme::CurveFitting:
        adc_cali_delete_scheme_curve_fitting(reinterpret_cast<adc_cali_handle_t>(adc_cali_handle));
        break;
#endif
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    case CalibrationScheme::LineFitting:
        adc_cali_delete_scheme_line_fitting(reinterpret_cast<adc_cali_handle_t>(adc_cali_handle));
        break;
#endif
    case CalibrationScheme::None:
    default:
        break;
    }
}

} // namespace esp_brookesia::hal
#endif // BROOKESIA_HAL_ADAPTOR_POWER_ENABLE_BATTERY && BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_IMPL_ADC && CONFIG_ESP_BOARD_PERIPH_ADC_SUPPORT
