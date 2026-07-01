/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_linux/macro_configs.h"
#if !BROOKESIA_HAL_LINUX_SYSTEM_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include <fstream>
#include <string>
#include <string_view>
#include <sys/utsname.h>
#include "private/utils.hpp"
#include "brookesia/hal_interface/interfaces/system/board_info.hpp"
#include "brookesia/hal_linux/system/device.hpp"
#include "ota_updater_impl.hpp"

namespace esp_brookesia::hal {

namespace {

std::string get_linux_release_name()
{
    std::ifstream os_release("/etc/os-release");
    std::string line;
    while (std::getline(os_release, line)) {
        constexpr std::string_view pretty_name_prefix = "PRETTY_NAME=";
        if (!line.starts_with(pretty_name_prefix)) {
            continue;
        }
        std::string value = line.substr(pretty_name_prefix.size());
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
        return value;
    }
    return "Linux";
}

system::BoardInfoIface::Info make_board_info()
{
    struct utsname uts = {};
    const bool has_uname = (uname(&uts) == 0);
    const std::string release_name = get_linux_release_name();
    const std::string machine = has_uname ? uts.machine : "unknown";
    const std::string release = has_uname ? uts.release : "unknown";

    return {
        .name = "ESP-Brookesia Linux",
        .chip = machine,
        .version = release,
        .description = release_name,
        .manufacturer = "Espressif",
    };
}

} // namespace

class BoardInfoLinuxImpl: public system::BoardInfoIface {
public:
    BoardInfoLinuxImpl()
        : system::BoardInfoIface(make_board_info())
    {
    }
};

bool SystemLinuxDevice::probe()
{
    return true;
}

std::vector<InterfaceSpec> SystemLinuxDevice::get_interface_specs() const
{
    std::vector<InterfaceSpec> specs = {
        {system::BoardInfoIface::NAME, BOARD_INFO_IFACE_NAME},
    };
#if BROOKESIA_HAL_LINUX_SYSTEM_ENABLE_OTA_UPDATER_IMPL
    specs.push_back({system::OtaUpdaterIface::NAME, OTA_UPDATER_IFACE_NAME});
#endif
    return specs;
}

bool SystemLinuxDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        board_info_iface_ = std::make_shared<BoardInfoLinuxImpl>(), false,
        "Failed to create board info linux"
    );
    BROOKESIA_CHECK_FALSE_RETURN(board_info_iface_->get_info().is_valid(), false, "Board info linux is invalid");

    interfaces_.emplace(BOARD_INFO_IFACE_NAME, board_info_iface_);

#if BROOKESIA_HAL_LINUX_SYSTEM_ENABLE_OTA_UPDATER_IMPL
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        ota_updater_iface_ = std::make_shared<OtaUpdaterLinuxIface>(), false,
        "Failed to create OTA updater linux"
    );
    interfaces_.emplace(OTA_UPDATER_IFACE_NAME, ota_updater_iface_);
#endif

    return true;
}

void SystemLinuxDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(BOARD_INFO_IFACE_NAME);
    board_info_iface_.reset();
#if BROOKESIA_HAL_LINUX_SYSTEM_ENABLE_OTA_UPDATER_IMPL
    interfaces_.erase(OTA_UPDATER_IFACE_NAME);
    ota_updater_iface_.reset();
#endif
}

#if BROOKESIA_HAL_LINUX_ENABLE_SYSTEM_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, SystemLinuxDevice, SystemLinuxDevice::DEVICE_NAME, SystemLinuxDevice::get_instance(),
    BROOKESIA_HAL_LINUX_SYSTEM_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
