/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <cstdint>
#include <expected>
#include <mutex>
#include <string>
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_board_periph.h"
#include "periph_adc.h"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_battery/service_battery.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::service {

namespace {
constexpr const char *ADC_CHARGE_STATE_PERIPH_NAME = "adc_charge_state";
constexpr const char *ADC_BATTERY_VOLTAGE_PERIPH_NAME = "adc_battery_voltage";

constexpr int ADC_SAMPLE_COUNT = 8;
constexpr int ADC_RAW_MAX = 4095;
constexpr int ADC_REF_VOLTAGE_MV = 3300;
constexpr int CHARGING_THRESHOLD_MV = 1000;
constexpr float BATTERY_ADC_CALIBRATION_FACTOR = 1.058F;
constexpr float BATTERY_DIVIDER_RATIO = 2.0F;
constexpr int BATTERY_EMPTY_MV = 3000;
constexpr int BATTERY_FULL_MV = 4100;

using BatteryStatus = helper::Battery::Status;

periph_adc_handle_t *get_adc_handle(void *handle)
{
    return reinterpret_cast<periph_adc_handle_t *>(handle);
}

periph_adc_config_t *get_adc_config(void *config)
{
    return reinterpret_cast<periph_adc_config_t *>(config);
}

adc_cali_handle_t get_cali_handle(void *handle)
{
    return reinterpret_cast<adc_cali_handle_t>(handle);
}

bool is_valid_oneshot_adc_config(periph_adc_config_t *config)
{
    return (config != nullptr) && (config->role == ESP_BOARD_PERIPH_ROLE_ONESHOT);
}

std::expected<int, std::string> read_average_raw_mv(void *handle_ptr, void *config_ptr, void *cali_handle_ptr)
{
    auto *handle = get_adc_handle(handle_ptr);
    auto *config = get_adc_config(config_ptr);
    BROOKESIA_CHECK_FALSE_RETURN(handle != nullptr, std::unexpected("ADC handle is null"), "ADC handle is null");
    BROOKESIA_CHECK_FALSE_RETURN(
        is_valid_oneshot_adc_config(config), std::unexpected("ADC config is invalid"), "ADC config is invalid"
    );

    int64_t raw_sum = 0;
    for (int i = 0; i < ADC_SAMPLE_COUNT; ++i) {
        int raw = 0;
        auto ret = adc_oneshot_read(handle->oneshot, config->cfg.oneshot.channel_id, &raw);
        BROOKESIA_CHECK_ESP_ERR_RETURN(ret, std::unexpected("Failed to read ADC oneshot value"), "Failed to read ADC");
        raw_sum += raw;
    }

    const int raw_avg = static_cast<int>(raw_sum / ADC_SAMPLE_COUNT);
    auto cali_handle = get_cali_handle(cali_handle_ptr);
    if (cali_handle != nullptr) {
        int voltage_mv = 0;
        auto ret = adc_cali_raw_to_voltage(cali_handle, raw_avg, &voltage_mv);
        if (ret == ESP_OK) {
            return voltage_mv;
        }
        BROOKESIA_LOGW("ADC calibration conversion failed, fallback to raw conversion: %1%", esp_err_to_name(ret));
    }

    return (raw_avg * ADC_REF_VOLTAGE_MV) / ADC_RAW_MAX;
}

int battery_level_from_voltage_mv(int voltage_mv)
{
    if (voltage_mv >= BATTERY_FULL_MV) {
        return 100;
    }
    if (voltage_mv <= BATTERY_EMPTY_MV) {
        return 0;
    }

    const int range_mv = BATTERY_FULL_MV - BATTERY_EMPTY_MV;
    return std::clamp(((voltage_mv - BATTERY_EMPTY_MV) * 100) / range_mv, 0, 100);
}

class BatteryProvider {
public:
    static BatteryProvider &get_instance()
    {
        static BatteryProvider instance;
        return instance;
    }

    bool is_supported() const
    {
        void *charge_config = nullptr;
        void *battery_config = nullptr;

        if (esp_board_periph_get_config(ADC_CHARGE_STATE_PERIPH_NAME, &charge_config) != ESP_OK) {
            return false;
        }
        if (esp_board_periph_get_config(ADC_BATTERY_VOLTAGE_PERIPH_NAME, &battery_config) != ESP_OK) {
            return false;
        }

        return is_valid_oneshot_adc_config(get_adc_config(charge_config)) &&
               is_valid_oneshot_adc_config(get_adc_config(battery_config));
    }

    bool init()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return init_locked();
    }

    void deinit()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        deinit_locked();
    }

    std::expected<BatteryStatus, std::string> get_status()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!init_locked()) {
            return std::unexpected(last_error_message_.empty() ? "Battery monitor is unavailable" : last_error_message_);
        }

        return get_status_locked();
    }

    std::string get_last_error() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_error_message_;
    }

private:
    BatteryProvider() = default;
    ~BatteryProvider()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        deinit_locked();
    }

    BatteryProvider(const BatteryProvider &) = delete;
    BatteryProvider &operator=(const BatteryProvider &) = delete;

    bool init_locked()
    {
        if (!is_supported()) {
            last_error_message_ = "Battery monitor is not supported on current board";
            BROOKESIA_LOGD("Battery monitor is not supported on current board");
            return false;
        }

        if (is_initialized_) {
            return true;
        }

        last_error_message_.clear();

        auto charge_ret = esp_board_periph_ref_handle(ADC_CHARGE_STATE_PERIPH_NAME, &charge_handle_);
        if (charge_ret != ESP_OK) {
            last_error_message_ = "Failed to initialize charging ADC";
            BROOKESIA_LOGE("Failed to initialize charging ADC handle: %1%", esp_err_to_name(charge_ret));
            return false;
        }

        auto battery_ret = esp_board_periph_ref_handle(ADC_BATTERY_VOLTAGE_PERIPH_NAME, &battery_handle_);
        if (battery_ret != ESP_OK) {
            last_error_message_ = "Failed to initialize battery voltage ADC";
            BROOKESIA_LOGE("Failed to initialize battery ADC handle: %1%", esp_err_to_name(battery_ret));
            deinit_locked();
            return false;
        }

        auto charge_cfg_ret = esp_board_periph_get_config(ADC_CHARGE_STATE_PERIPH_NAME, &charge_config_);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(charge_cfg_ret, {
            last_error_message_ = "Failed to get charging ADC config";
            deinit_locked();
            return false;
        }, {
            BROOKESIA_LOGE("Failed to get charging ADC config: %1%", esp_err_to_name(charge_cfg_ret));
        });

        auto battery_cfg_ret = esp_board_periph_get_config(ADC_BATTERY_VOLTAGE_PERIPH_NAME, &battery_config_);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(battery_cfg_ret, {
            last_error_message_ = "Failed to get battery ADC config";
            deinit_locked();
            return false;
        }, {
            BROOKESIA_LOGE("Failed to get battery ADC config: %1%", esp_err_to_name(battery_cfg_ret));
        });

        BROOKESIA_CHECK_FALSE_EXECUTE(is_valid_oneshot_adc_config(get_adc_config(charge_config_)), {
            last_error_message_ = "Charging ADC config is invalid";
            deinit_locked();
            return false;
        }, {
            BROOKESIA_LOGE("Charging ADC config is not oneshot");
        });
        BROOKESIA_CHECK_FALSE_EXECUTE(is_valid_oneshot_adc_config(get_adc_config(battery_config_)), {
            last_error_message_ = "Battery ADC config is invalid";
            deinit_locked();
            return false;
        }, {
            BROOKESIA_LOGE("Battery ADC config is not oneshot");
        });

        create_cali_handle_locked(charge_config_, &charge_cali_handle_);
        create_cali_handle_locked(battery_config_, &battery_cali_handle_);

        is_initialized_ = true;
        last_error_message_.clear();

        auto status_result = get_status_locked();
        if (status_result) {
            const auto &status = status_result.value();
            BROOKESIA_LOGI(
                "Battery provider initialized: level(%1%), charging(%2%), voltage_mv(%3%)",
                static_cast<int>(status.level), status.charging, status.voltage_mv
            );
        } else {
            BROOKESIA_LOGW("Battery provider initialized, but first read failed: %1%", status_result.error());
        }

        return true;
    }

    void deinit_locked()
    {
        if (charge_cali_handle_ != nullptr) {
            destroy_cali_handle_locked(charge_cali_handle_);
            charge_cali_handle_ = nullptr;
        }
        if (battery_cali_handle_ != nullptr) {
            destroy_cali_handle_locked(battery_cali_handle_);
            battery_cali_handle_ = nullptr;
        }
        if (charge_handle_ != nullptr) {
            esp_board_periph_unref_handle(ADC_CHARGE_STATE_PERIPH_NAME);
            charge_handle_ = nullptr;
        }
        if (battery_handle_ != nullptr) {
            esp_board_periph_unref_handle(ADC_BATTERY_VOLTAGE_PERIPH_NAME);
            battery_handle_ = nullptr;
        }
        charge_config_ = nullptr;
        battery_config_ = nullptr;
        is_initialized_ = false;
    }

    bool create_cali_handle_locked(void *config_ptr, void **handle_out)
    {
        if ((config_ptr == nullptr) || (handle_out == nullptr)) {
            return false;
        }

        auto *config = get_adc_config(config_ptr);
        if (!is_valid_oneshot_adc_config(config)) {
            return false;
        }

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_handle_t cali_handle = nullptr;
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = config->cfg.oneshot.unit_cfg.unit_id,
            .chan = config->cfg.oneshot.channel_id,
            .atten = config->cfg.oneshot.chan_cfg.atten,
            .bitwidth = config->cfg.oneshot.chan_cfg.bitwidth,
        };
        auto ret = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
        if (ret == ESP_OK) {
            *handle_out = reinterpret_cast<void *>(cali_handle);
            return true;
        }
        BROOKESIA_LOGW("Failed to create ADC curve fitting calibration: %1%", esp_err_to_name(ret));
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        adc_cali_handle_t cali_handle = nullptr;
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = config->cfg.oneshot.unit_cfg.unit_id,
            .atten = config->cfg.oneshot.chan_cfg.atten,
            .bitwidth = config->cfg.oneshot.chan_cfg.bitwidth,
            .default_vref = 0,
        };
        auto ret = adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle);
        if (ret == ESP_OK) {
            *handle_out = reinterpret_cast<void *>(cali_handle);
            return true;
        }
        BROOKESIA_LOGW("Failed to create ADC line fitting calibration: %1%", esp_err_to_name(ret));
#endif

        *handle_out = nullptr;
        return false;
    }

    void destroy_cali_handle_locked(void *handle)
    {
        auto cali_handle = get_cali_handle(handle);
        if (cali_handle == nullptr) {
            return;
        }

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        auto ret = adc_cali_delete_scheme_curve_fitting(cali_handle);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, {
            BROOKESIA_LOGW("Failed to delete ADC curve fitting calibration: %1%", esp_err_to_name(ret));
        });
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        auto ret = adc_cali_delete_scheme_line_fitting(cali_handle);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, {
            BROOKESIA_LOGW("Failed to delete ADC line fitting calibration: %1%", esp_err_to_name(ret));
        });
#else
        (void)cali_handle;
#endif
    }

    std::expected<BatteryStatus, std::string> get_status_locked() const
    {
        BROOKESIA_CHECK_FALSE_RETURN(is_initialized_, std::unexpected("Battery monitor is not initialized"), "Battery monitor is not initialized");

        auto charge_voltage_result = read_average_raw_mv(charge_handle_, charge_config_, charge_cali_handle_);
        BROOKESIA_CHECK_FALSE_RETURN(
            charge_voltage_result.has_value(), std::unexpected(charge_voltage_result.error()),
            "Failed to read charging ADC voltage: %1%", charge_voltage_result.error()
        );

        auto battery_adc_voltage_result = read_average_raw_mv(battery_handle_, battery_config_, battery_cali_handle_);
        BROOKESIA_CHECK_FALSE_RETURN(
            battery_adc_voltage_result.has_value(), std::unexpected(battery_adc_voltage_result.error()),
            "Failed to read battery ADC voltage: %1%", battery_adc_voltage_result.error()
        );

        const int charge_voltage_mv = charge_voltage_result.value();
        const int battery_adc_voltage_mv = battery_adc_voltage_result.value();
        const int battery_voltage_mv = static_cast<int>(
                                           battery_adc_voltage_mv * BATTERY_ADC_CALIBRATION_FACTOR * BATTERY_DIVIDER_RATIO
                                       );
        const int level = battery_level_from_voltage_mv(battery_voltage_mv);
        const bool raw_charging = charge_voltage_mv < CHARGING_THRESHOLD_MV;

        BROOKESIA_LOGD(
            "Battery status: charge_voltage_mv(%1%), battery_adc_voltage_mv(%2%), battery_voltage_mv(%3%), level(%4%), charging(%5%)",
            charge_voltage_mv, battery_adc_voltage_mv, battery_voltage_mv, level, raw_charging
        );

        BatteryStatus status = {
            .level = static_cast<uint8_t>(level),
            .charging = (level < 100) && raw_charging,
            .discharging = (level < 100) && !raw_charging,
            .voltage_mv = battery_voltage_mv,
        };

        return status;
    }

    mutable std::mutex mutex_;
    bool is_initialized_ = false;
    void *charge_handle_ = nullptr;
    void *battery_handle_ = nullptr;
    void *charge_config_ = nullptr;
    void *battery_config_ = nullptr;
    void *charge_cali_handle_ = nullptr;
    void *battery_cali_handle_ = nullptr;
    std::string last_error_message_;
};
} // namespace

bool Battery::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGI("Initialized");
    return true;
}

void Battery::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGI("Deinitialized");
}

bool Battery::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto &provider = BatteryProvider::get_instance();
    if (!provider.init()) {
        BROOKESIA_LOGW("Battery service unavailable: %1%", provider.get_last_error());
        return false;
    }

    return true;
}

void Battery::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BatteryProvider::get_instance().deinit();
}

std::expected<boost::json::object, std::string> Battery::function_get_status()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto status_result = BatteryProvider::get_instance().get_status();
    if (!status_result) {
        return std::unexpected(status_result.error());
    }

    return BROOKESIA_DESCRIBE_TO_JSON(status_result.value()).as_object();
}

#if BROOKESIA_SERVICE_BATTERY_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    ServiceBase, Battery, Battery::get_instance().get_attributes().name, Battery::get_instance(),
    BROOKESIA_SERVICE_BATTERY_PLUGIN_SYMBOL
);
#endif // BROOKESIA_SERVICE_BATTERY_ENABLE_AUTO_REGISTER

} // namespace esp_brookesia::service
