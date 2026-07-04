/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/service_device/macro_configs.h"
#if !BROOKESIA_SERVICE_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/network/connectivity.hpp"
#include "brookesia/hal_interface/interfaces/system/board_info.hpp"
#include "brookesia/service_device/service_device.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::service {

std::expected<boost::json::array, std::string> Device::function_get_capabilities()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return BROOKESIA_DESCRIBE_TO_JSON(hal::get_device_infos()).as_array();
}

std::expected<boost::json::object, std::string> Device::function_get_board_info()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!hal::has_interface(hal::system::BoardInfoIface::NAME)) {
        return std::unexpected("Board info interface is not available");
    }

    auto board_info_iface = hal::acquire_first_interface<hal::system::BoardInfoIface>();
    if (!board_info_iface) {
        return std::unexpected("Failed to acquire board info interface");
    }

    return BROOKESIA_DESCRIBE_TO_JSON(board_info_iface->get_info()).as_object();
}

std::expected<boost::json::array, std::string> Device::function_get_network_connectivity_info()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!hal::has_interface(hal::network::ConnectivityIface::NAME)) {
        return std::unexpected("Network connectivity interface is not available");
    }

    auto connectivity_ifaces = hal::acquire_interfaces<hal::network::ConnectivityIface>();
    NetworkConnectivityInfos infos;
    infos.reserve(connectivity_ifaces.size());
    for (const auto &connectivity_iface : connectivity_ifaces) {
        if (!connectivity_iface) {
            continue;
        }
        const auto status = connectivity_iface->get_status();
        infos.emplace_back(NetworkConnectivityInfo{
            .instance_name = connectivity_iface.instance_name(),
            .status = status,
            .state = status.state(),
            .network_ready = status.is_local_network_ready(),
            .internet_ready = status.is_internet_ready(),
        });
    }

    if (infos.empty()) {
        return std::unexpected("Failed to acquire network connectivity interface");
    }

    return BROOKESIA_DESCRIBE_TO_JSON(infos).as_array();
}

helper::Device::Capabilities Device::get_capabilities() const
{
    return hal::get_device_infos();
}

} // namespace esp_brookesia::service
