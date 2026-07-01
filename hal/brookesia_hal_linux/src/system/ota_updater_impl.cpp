/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <filesystem>

#include "brookesia/hal_linux/macro_configs.h"
#if !BROOKESIA_HAL_LINUX_SYSTEM_OTA_UPDATER_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "ota_updater_impl.hpp"

#if BROOKESIA_HAL_LINUX_SYSTEM_ENABLE_OTA_UPDATER_IMPL

namespace esp_brookesia::hal {

system::OtaUpdaterIface::TargetIdentity OtaUpdaterLinuxIface::get_target_identity(const std::string &system)
{
    last_error_.clear();
    return {
        .system = system,
        .chip = "PC",
        .board = "ESP-Brookesia Linux",
    };
}

std::string OtaUpdaterLinuxIface::get_running_firmware_version()
{
    last_error_.clear();
    return "pc";
}

std::string OtaUpdaterLinuxIface::get_running_boot_partition_label()
{
    last_error_.clear();
    return "pc";
}

bool OtaUpdaterLinuxIface::get_next_update_partition_label(std::string &label)
{
    label = "pc";
    last_error_.clear();
    return true;
}

bool OtaUpdaterLinuxIface::write_firmware(
    const std::string &staged_path, FirmwareProgressCallback progress_cb, std::string &boot_partition
)
{
    std::error_code error_code;
    const auto file_size = std::filesystem::file_size(staged_path, error_code);
    if (error_code) {
        last_error_ = "Failed to stat staged firmware image: " + error_code.message();
        return false;
    }

    const auto total = static_cast<uint64_t>(file_size);
    if (progress_cb && (!progress_cb(0, total) || !progress_cb(total, total))) {
        last_error_ = "OTA canceled";
        return false;
    }

    boot_partition = "pc";
    last_error_.clear();
    return true;
}

bool OtaUpdaterLinuxIface::confirm_boot()
{
    last_error_.clear();
    return true;
}

} // namespace esp_brookesia::hal

#endif // BROOKESIA_HAL_LINUX_SYSTEM_ENABLE_OTA_UPDATER_IMPL
