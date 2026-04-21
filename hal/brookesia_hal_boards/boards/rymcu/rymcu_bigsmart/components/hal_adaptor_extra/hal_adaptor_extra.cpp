#undef BROOKESIA_LOG_TAG
#define BROOKESIA_LOG_TAG "HalAdptExtra"
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/check.hpp"
#include "brookesia/hal_adaptor.hpp"
#include "brookesia/hal_interface/device.hpp"
#include "esp_board_manager.h"

using namespace esp_brookesia;

constexpr const char *GPIO_EXPANDER_DEVICE_NAME = "gpio_expander";

ESP_BROOKESIA_HAL_DEVICE_REGISTER_PRE_INIT_CALLBACKS(
    HAL_ADAPTOR_EXTRA_PRE_INIT_CALLBACKS_SYMBOL,
    {
        hal::AudioDevice::get_instance().get_name(),
        []() {
            BROOKESIA_LOG_TRACE_GUARD();
            auto ret = esp_board_manager_init_device_by_name(GPIO_EXPANDER_DEVICE_NAME);
            BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to init codec DAC");
            return true;
        }
    },
    {
        hal::DisplayDevice::get_instance().get_name(),
        []() {
            BROOKESIA_LOG_TRACE_GUARD();
            auto ret = esp_board_manager_init_device_by_name(GPIO_EXPANDER_DEVICE_NAME);
            BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to init codec DAC");
            return true;
        }
    }
)

ESP_BROOKESIA_HAL_DEVICE_REGISTER_ALL_POST_DEINIT_CALLBACK(
    HAL_ADAPTOR_EXTRA_ALL_POST_DEINIT_CALLBACKS_SYMBOL,
    []() {
        BROOKESIA_LOG_TRACE_GUARD();
        auto ret = esp_board_manager_deinit_device_by_name(GPIO_EXPANDER_DEVICE_NAME);
        BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to deinit codec DAC");
        return true;
    }
)
