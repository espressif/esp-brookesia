/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

/**
 * @file ota_updater.hpp
 * @brief Declares the system OTA HAL interface.
 */

namespace esp_brookesia::hal::system {

/**
 * @brief Platform OTA primitives for firmware and board target identity.
 */
class OtaUpdaterIface: public Interface {
public:
    static constexpr const char *NAME = "SystemOtaUpdater";  ///< Interface registry name.

    /**
     * @brief OTA manifest target selector.
     */
    struct TargetInfo {
        std::vector<std::string> systems = {}; ///< Compatible product/system names.
        std::vector<std::string> chips = {};   ///< Compatible chips.
        std::vector<std::string> boards = {};  ///< Compatible board names.
    };

    /**
     * @brief Current platform target identity.
     */
    struct TargetIdentity {
        std::string system; ///< Current product/system name.
        std::string chip;   ///< Current chip name.
        std::string board;  ///< Current board name.
    };

    /**
     * @brief Firmware write progress callback.
     *
     * Return true to continue writing, or false to cancel.
     */
    using FirmwareProgressCallback = std::function<bool(uint64_t written, uint64_t total)>;

    /**
     * @brief Construct a system OTA interface.
     */
    OtaUpdaterIface()
        : Interface(NAME)
    {
    }

    /**
     * @brief Virtual destructor for polymorphic OTA interfaces.
     */
    ~OtaUpdaterIface() override = default;

    /**
     * @brief Get the current target identity.
     *
     * @param[in] system Product/system name provided by the caller.
     * @return Target identity.
     */
    virtual TargetIdentity get_target_identity(const std::string &system) = 0;

    /**
     * @brief Get the currently running firmware version.
     *
     * @return Firmware version string, or empty if unavailable.
     */
    virtual std::string get_running_firmware_version() = 0;

    /**
     * @brief Get the currently running boot partition label.
     *
     * @return Partition label, or empty if unavailable.
     */
    virtual std::string get_running_boot_partition_label() = 0;

    /**
     * @brief Get the next OTA update partition label.
     *
     * @param[out] label Update partition label.
     * @return true on success, otherwise false.
     */
    virtual bool get_next_update_partition_label(std::string &label) = 0;

    /**
     * @brief Write a staged firmware image to the platform OTA partition.
     *
     * @param[in] staged_path Source firmware image path.
     * @param[in] progress_cb Progress callback. Return false from the callback to cancel.
     * @param[out] boot_partition Boot partition label selected by the platform.
     * @return true on success, otherwise false.
     */
    virtual bool write_firmware(
        const std::string &staged_path, FirmwareProgressCallback progress_cb, std::string &boot_partition
    ) = 0;

    /**
     * @brief Confirm a pending OTA boot if the platform requires explicit confirmation.
     *
     * @return true on success, otherwise false.
     */
    virtual bool confirm_boot() = 0;

    /**
     * @brief Get the most recent backend error.
     *
     * @return Human-readable error string.
     */
    const std::string &get_last_error() const
    {
        return last_error_;
    }

protected:
    std::string last_error_;
};

BROOKESIA_DESCRIBE_STRUCT(OtaUpdaterIface::TargetInfo, (), (systems, chips, boards));
BROOKESIA_DESCRIBE_STRUCT(OtaUpdaterIface::TargetIdentity, (), (system, chip, board));

} // namespace esp_brookesia::hal::system
