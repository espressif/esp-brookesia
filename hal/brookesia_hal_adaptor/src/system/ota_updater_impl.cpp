/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <array>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>
#include <thread>
#include <utility>

#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "sdkconfig.h"

#if defined(CONFIG_ESP_BOARD_NAME)
#   include "esp_board_manager_includes.h"
#endif

#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_SYSTEM_OTA_UPDATER_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "ota_updater_impl.hpp"

#if BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_OTA_UPDATER_IMPL

namespace esp_brookesia::hal {
namespace {

constexpr const char *WRITE_FIRMWARE_THREAD_NAME = "HalOtaWriteFw";
constexpr const char *CONFIRM_BOOT_THREAD_NAME = "HalOtaCfmBoot";

#if defined(CONFIG_ESP_BOARD_NAME)
std::string to_string_or_empty(const char *value)
{
    return (value != nullptr) ? value : "";
}
#endif

system::OtaUpdaterIface::TargetIdentity make_default_target_identity(const std::string &system)
{
    return {
        .system = system,
#if defined(CONFIG_IDF_TARGET)
        .chip = CONFIG_IDF_TARGET,
#else
        .chip = {},
#endif
#if defined(CONFIG_ESP_BOARD_NAME)
        .board = CONFIG_ESP_BOARD_NAME,
#else
        .board = {},
#endif
    };
}

} // namespace

system::OtaUpdaterIface::TargetIdentity OtaUpdaterAdaptorIface::get_target_identity(const std::string &system)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if defined(CONFIG_ESP_BOARD_NAME)
    esp_board_info_t board_info = {};
    auto ret = esp_board_manager_get_board_info(&board_info);
    if (ret == ESP_OK) {
        last_error_.clear();
        return {
            .system = system,
            .chip = to_string_or_empty(board_info.chip),
            .board = to_string_or_empty(board_info.name),
        };
    }

    last_error_ = esp_err_to_name(ret);
    BROOKESIA_LOGW("Failed to get board info, fallback to sdkconfig target identity: %1%", last_error_);
#else
    last_error_.clear();
#endif
    return make_default_target_identity(system);
}

std::string OtaUpdaterAdaptorIface::get_running_firmware_version()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    const esp_app_desc_t *description = esp_app_get_description();
    last_error_.clear();
    return description == nullptr ? std::string() : std::string(description->version);
}

std::string OtaUpdaterAdaptorIface::get_running_boot_partition_label()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    const esp_partition_t *partition = esp_ota_get_running_partition();
    last_error_.clear();
    return partition == nullptr ? std::string() : std::string(partition->label);
}

bool OtaUpdaterAdaptorIface::get_next_update_partition_label(std::string &label)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    const esp_partition_t *partition = esp_ota_get_next_update_partition(nullptr);
    if (partition == nullptr) {
        last_error_ = "No OTA update partition is available";
        return false;
    }

    label = partition->label;
    last_error_.clear();
    return true;
}

bool OtaUpdaterAdaptorIface::write_firmware(
    const std::string &staged_path, FirmwareProgressCallback progress_cb, std::string &boot_partition
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (lib_utils::ThreadConfig::check_stack_cache_safe()) {
        BROOKESIA_LOGD("Writing firmware OTA in current thread");
        return write_firmware_internal(staged_path, std::move(progress_cb), boot_partition);
    }

    bool result = false;
    auto worker = [&]() {
        BROOKESIA_LOG_TRACE_GUARD();
        result = write_firmware_internal(staged_path, std::move(progress_cb), boot_partition);
    };

    BROOKESIA_LOGD("Writing firmware OTA on internal-stack thread");
    BROOKESIA_THREAD_CONFIG_GUARD({
        .name = WRITE_FIRMWARE_THREAD_NAME,
        .stack_size = static_cast<size_t>(BROOKESIA_HAL_ADAPTOR_SYSTEM_OTA_UPDATER_THREAD_STACK_SIZE),
        .stack_in_ext = false,
    });
    try {
        std::thread(worker).join();
    } catch (const std::exception &e) {
        last_error_ = std::string("Failed to write firmware OTA on safe-stack thread: ") + e.what();
        return false;
    } catch (...) {
        last_error_ = "Failed to write firmware OTA on safe-stack thread";
        return false;
    }

    return result;
}

bool OtaUpdaterAdaptorIface::confirm_boot()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (lib_utils::ThreadConfig::check_stack_cache_safe()) {
        BROOKESIA_LOGD("Confirming OTA boot in current thread");
        return confirm_boot_internal();
    }

    bool result = false;
    auto worker = [&]() {
        BROOKESIA_LOG_TRACE_GUARD();
        result = confirm_boot_internal();
    };

    BROOKESIA_LOGD("Confirming OTA boot on internal-stack thread");
    BROOKESIA_THREAD_CONFIG_GUARD({
        .name = CONFIRM_BOOT_THREAD_NAME,
        .stack_size = static_cast<size_t>(BROOKESIA_HAL_ADAPTOR_SYSTEM_OTA_UPDATER_THREAD_STACK_SIZE),
        .stack_in_ext = false,
    });
    try {
        std::thread(worker).join();
    } catch (const std::exception &e) {
        last_error_ = std::string("Failed to confirm OTA boot on safe-stack thread: ") + e.what();
        return false;
    } catch (...) {
        last_error_ = "Failed to confirm OTA boot on safe-stack thread";
        return false;
    }

    return result;
}

bool OtaUpdaterAdaptorIface::write_firmware_internal(
    const std::string &staged_path, FirmwareProgressCallback progress_cb, std::string &boot_partition
)
{
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(nullptr);
    if (update_partition == nullptr) {
        last_error_ = "No OTA update partition is available";
        return false;
    }
    BROOKESIA_LOGI(
        "Writing firmware OTA: staged_path(%1%), partition(%2%), address(%3%), size(%4%)",
        staged_path, update_partition->label, update_partition->address, update_partition->size
    );

    std::error_code file_size_error;
    const auto firmware_size = std::filesystem::file_size(staged_path, file_size_error);
    const uint64_t firmware_total = file_size_error ? 0 : static_cast<uint64_t>(firmware_size);

    FILE *file = std::fopen(staged_path.c_str(), "rb");
    if (file == nullptr) {
        last_error_ = "Failed to open staged firmware image";
        return false;
    }
    lib_utils::FunctionGuard file_guard([file]() {
        std::fclose(file);
    });

    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        last_error_ = std::string("esp_ota_begin failed: ") + esp_err_to_name(err);
        return false;
    }
    lib_utils::FunctionGuard ota_guard([ota_handle]() {
        esp_ota_abort(ota_handle);
    });

    std::array<uint8_t, 4096> buffer = {};
    uint64_t written = 0;
    if (progress_cb && !progress_cb(written, firmware_total)) {
        last_error_ = "OTA canceled";
        return false;
    }

    while (true) {
        const size_t read_len = std::fread(buffer.data(), 1, buffer.size(), file);
        if (read_len > 0) {
            err = esp_ota_write(ota_handle, buffer.data(), read_len);
            if (err != ESP_OK) {
                last_error_ = std::string("esp_ota_write failed: ") + esp_err_to_name(err);
                return false;
            }
            written += static_cast<uint64_t>(read_len);
            if (progress_cb && !progress_cb(written, firmware_total)) {
                last_error_ = "OTA canceled";
                return false;
            }
        }
        if (read_len < buffer.size()) {
            if (std::ferror(file) != 0) {
                last_error_ = "Failed to read staged firmware image";
                return false;
            }
            break;
        }
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        last_error_ = std::string("esp_ota_end failed: ") + esp_err_to_name(err);
        return false;
    }
    ota_guard.release();

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        last_error_ = std::string("esp_ota_set_boot_partition failed: ") + esp_err_to_name(err);
        return false;
    }

    boot_partition = update_partition->label;
    last_error_.clear();
    BROOKESIA_LOGI("Firmware OTA written: boot_partition(%1%)", boot_partition);
    return true;
}

bool OtaUpdaterAdaptorIface::confirm_boot_internal()
{
    const auto *ota_data_partition = esp_partition_find_first(
                                         ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr
                                     );
    if (ota_data_partition == nullptr) {
        BROOKESIA_LOGI("OTA boot confirmation skipped: OTA data partition is not available");
        last_error_.clear();
        return true;
    }

    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    const bool running_from_ota_partition =
        (running_partition != nullptr) &&
        (running_partition->type == ESP_PARTITION_TYPE_APP) &&
        (running_partition->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_0) &&
        (running_partition->subtype <= ESP_PARTITION_SUBTYPE_APP_OTA_15);
    if (!running_from_ota_partition) {
        BROOKESIA_LOGI(
            "OTA boot confirmation skipped: running partition is not OTA app: label(%1%), subtype(%2%)",
            running_partition != nullptr ? running_partition->label : "null",
            running_partition != nullptr ? static_cast<int>(running_partition->subtype) : -1
        );
        last_error_.clear();
        return true;
    }

    const esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        last_error_ = std::string("esp_ota_mark_app_valid_cancel_rollback failed: ") + esp_err_to_name(err);
        return false;
    }

    BROOKESIA_LOGI("OTA boot confirmation checked: result(%1%)", esp_err_to_name(err));
    last_error_.clear();
    return true;
}

} // namespace esp_brookesia::hal

#endif // BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_OTA_UPDATER_IMPL
