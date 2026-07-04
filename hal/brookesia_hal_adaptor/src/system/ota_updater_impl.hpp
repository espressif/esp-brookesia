/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>

#include "brookesia/hal_interface/interfaces/system/ota_updater.hpp"

namespace esp_brookesia::hal {

class OtaUpdaterAdaptorIface: public system::OtaUpdaterIface {
public:
    OtaUpdaterAdaptorIface() = default;
    ~OtaUpdaterAdaptorIface() override = default;

    TargetIdentity get_target_identity(const std::string &system) override;
    std::string get_running_firmware_version() override;
    std::string get_running_boot_partition_label() override;
    bool get_next_update_partition_label(std::string &label) override;
    bool write_firmware(
        const std::string &staged_path, FirmwareProgressCallback progress_cb, std::string &boot_partition
    ) override;
    bool confirm_boot() override;

private:
    bool write_firmware_internal(
        const std::string &staged_path, FirmwareProgressCallback progress_cb, std::string &boot_partition
    );
    bool confirm_boot_internal();
};

} // namespace esp_brookesia::hal
