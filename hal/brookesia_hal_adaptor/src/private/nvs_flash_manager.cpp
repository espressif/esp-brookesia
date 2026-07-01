/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <functional>
#include <mutex>
#include <thread>
#include "boost/format.hpp"
#include "nvs_flash.h"
#include "private/utils.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "nvs_flash_manager.hpp"

namespace esp_brookesia::hal::nvs_flash_manager {

namespace {

std::mutex mutex;
size_t ref_count = 0;
std::string last_error;

std::string make_error(const char *operation, esp_err_t error)
{
    return (boost::format("%1% failed: %2%") % operation % esp_err_to_name(error)).str();
}

esp_err_t run_on_cache_safe_stack(std::function<esp_err_t()> operation)
{
    esp_err_t result = ESP_OK;
    auto operation_func = [&operation, &result]() {
        BROOKESIA_LOG_TRACE_GUARD();
        result = operation();
    };

    if (!lib_utils::ThreadConfig::check_stack_cache_safe()) {
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_in_ext = false,
        });
        std::thread(operation_func).join();
    } else {
        operation_func();
    }

    return result;
}

} // namespace

bool acquire()
{
    BROOKESIA_LOG_TRACE_GUARD();

    std::lock_guard lock(mutex);
    if (ref_count > 0) {
        ref_count++;
        return true;
    }

    auto init_result = run_on_cache_safe_stack([]() {
        auto result = nvs_flash_init();
        if ((result == ESP_ERR_NVS_NO_FREE_PAGES) || (result == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
            BROOKESIA_LOGI("NVS partition was truncated and needs to be erased");
            result = nvs_flash_erase();
            BROOKESIA_CHECK_ESP_ERR_RETURN(result, result, "Erase NVS flash failed");
            result = nvs_flash_init();
        }
        return result;
    });
    if (init_result != ESP_OK) {
        last_error = make_error("Initialize NVS flash", init_result);
        BROOKESIA_LOGE("%1%", last_error);
        return false;
    }

    last_error.clear();
    ref_count = 1;
    return true;
}

void release()
{
    BROOKESIA_LOG_TRACE_GUARD();

    std::lock_guard lock(mutex);
    if (ref_count == 0) {
        return;
    }

    ref_count--;
    if (ref_count > 0) {
        return;
    }

    auto deinit_result = run_on_cache_safe_stack([]() {
        return nvs_flash_deinit();
    });
    if (deinit_result != ESP_OK) {
        last_error = make_error("Deinitialize NVS flash", deinit_result);
        BROOKESIA_LOGE("%1%", last_error);
    } else {
        last_error.clear();
    }
}

std::string get_last_error()
{
    std::lock_guard lock(mutex);
    return last_error;
}

} // namespace esp_brookesia::hal::nvs_flash_manager
