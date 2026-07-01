/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "brookesia/hal_linux/macro_configs.h"
#if !BROOKESIA_HAL_LINUX_NETWORK_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_interface/interfaces/network/connectivity.hpp"
#include "brookesia/hal_linux/network/device.hpp"
#include "http_client_impl.hpp"
#include "sntp_client_impl.hpp"

namespace esp_brookesia::hal {
namespace {

constexpr unsigned long ROUTE_FLAG_UP = 0x0001;

struct DefaultRoute {
    std::string interface_name;
    std::string gateway;
    int metric = std::numeric_limits<int>::max();
};

std::optional<unsigned long> parse_hex_ulong(const std::string &value)
{
    try {
        size_t parsed = 0;
        const auto result = std::stoul(value, &parsed, 16);
        if (parsed != value.size()) {
            return std::nullopt;
        }
        return result;
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

std::optional<int> parse_int(const std::string &value)
{
    try {
        size_t parsed = 0;
        const auto result = std::stoi(value, &parsed);
        if (parsed != value.size()) {
            return std::nullopt;
        }
        return result;
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

std::string route_ipv4_hex_to_string(const std::string &value)
{
    const auto raw = parse_hex_ulong(value);
    if (!raw.has_value() || *raw == 0) {
        return {};
    }

    in_addr address = {};
    address.s_addr = static_cast<in_addr_t>(*raw);
    std::array<char, INET_ADDRSTRLEN> buffer = {};
    if (inet_ntop(AF_INET, &address, buffer.data(), buffer.size()) == nullptr) {
        return {};
    }
    return buffer.data();
}

std::optional<DefaultRoute> find_default_ipv4_route()
{
    std::ifstream route_file("/proc/net/route");
    if (!route_file) {
        return std::nullopt;
    }

    std::string line;
    std::getline(route_file, line);
    std::optional<DefaultRoute> best_route;
    while (std::getline(route_file, line)) {
        std::istringstream stream(line);
        std::string interface_name;
        std::string destination;
        std::string gateway;
        std::string flags_text;
        std::string ref_count;
        std::string use;
        std::string metric_text;
        std::string mask;
        if (!(stream >> interface_name >> destination >> gateway >> flags_text >> ref_count >> use >> metric_text >>
                mask)) {
            continue;
        }
        if (interface_name == "lo" || destination != "00000000") {
            continue;
        }

        const auto flags = parse_hex_ulong(flags_text);
        if (!flags.has_value() || ((*flags & ROUTE_FLAG_UP) == 0)) {
            continue;
        }

        const auto metric = parse_int(metric_text).value_or(std::numeric_limits<int>::max());
        if (best_route.has_value() && metric >= best_route->metric) {
            continue;
        }

        best_route = DefaultRoute{
            .interface_name = interface_name,
            .gateway = route_ipv4_hex_to_string(gateway),
            .metric = metric,
        };
    }
    return best_route;
}

std::string sockaddr_ipv4_to_string(const sockaddr *address)
{
    if (address == nullptr || address->sa_family != AF_INET) {
        return {};
    }

    const auto *ipv4 = reinterpret_cast<const sockaddr_in *>(address);
    std::array<char, INET_ADDRSTRLEN> buffer = {};
    if (inet_ntop(AF_INET, &ipv4->sin_addr, buffer.data(), buffer.size()) == nullptr) {
        return {};
    }
    return buffer.data();
}

std::string read_first_ipv4_dns()
{
    std::ifstream resolv_file("/etc/resolv.conf");
    if (!resolv_file) {
        return {};
    }

    std::string line;
    while (std::getline(resolv_file, line)) {
        std::istringstream stream(line);
        std::string key;
        std::string value;
        if (!(stream >> key >> value) || key != "nameserver") {
            continue;
        }
        if (value.find(':') == std::string::npos) {
            return value;
        }
    }
    return {};
}

network::InterfaceType detect_interface_type(const std::string &interface_name)
{
    std::error_code error;
    if (std::filesystem::exists(std::filesystem::path("/sys/class/net") / interface_name / "wireless", error)) {
        return network::InterfaceType::WifiStation;
    }
    if (interface_name.starts_with("wwan") || interface_name.starts_with("ppp") ||
            interface_name.starts_with("cell")) {
        return network::InterfaceType::Cellular;
    }
    if (interface_name.starts_with("en") || interface_name.starts_with("eth")) {
        return network::InterfaceType::Ethernet;
    }
    return network::InterfaceType::Unknown;
}

class NetworkConnectivityLinuxIface: public network::ConnectivityIface {
public:
    network::NetworkStatus get_status() const override
    {
        network::NetworkStatus status = {
            .interface_type = network::InterfaceType::Unknown,
            .link_state = network::LinkState::Down,
            .ip_state = network::IpState::None,
            .reachability = network::Reachability::Unreachable,
        };

        const auto route = find_default_ipv4_route();
        if (!route.has_value()) {
            return status;
        }

        status.interface_type = detect_interface_type(route->interface_name);

        ifaddrs *addresses = nullptr;
        if (getifaddrs(&addresses) != 0 || addresses == nullptr) {
            return status;
        }

        std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> guard(addresses, freeifaddrs);
        bool found_interface = false;
        bool link_up = false;
        bool link_running = false;
        std::string ip;
        std::string netmask;
        for (const auto *current = addresses; current != nullptr; current = current->ifa_next) {
            if (current->ifa_name == nullptr || route->interface_name != current->ifa_name) {
                continue;
            }

            found_interface = true;
            link_up = link_up || ((current->ifa_flags & IFF_UP) != 0);
            link_running = link_running || ((current->ifa_flags & IFF_RUNNING) != 0);
            if (current->ifa_addr == nullptr || current->ifa_addr->sa_family != AF_INET) {
                continue;
            }

            ip = sockaddr_ipv4_to_string(current->ifa_addr);
            netmask = sockaddr_ipv4_to_string(current->ifa_netmask);
        }

        if (!found_interface) {
            return status;
        }

        status.link_state = link_up ? network::LinkState::Up : network::LinkState::Down;
        status.ip_state = link_up ? network::IpState::Acquiring : network::IpState::None;
        if (link_up && link_running && !ip.empty()) {
            status.ip_state = network::IpState::Ready;
            status.reachability = network::Reachability::LocalOnly;
            status.ip_info = network::IpInfo{
                .ip = ip,
                .netmask = netmask,
                .gateway = route->gateway,
                .dns = read_first_ipv4_dns(),
            };
        }

        return status;
    }
};

} // namespace

bool NetworkLinuxDevice::probe()
{
    return true;
}

std::vector<InterfaceSpec> NetworkLinuxDevice::get_interface_specs() const
{
    std::vector<InterfaceSpec> specs;
    specs.push_back({network::ConnectivityIface::NAME, CONNECTIVITY_IFACE_NAME});
#if BROOKESIA_HAL_LINUX_NETWORK_ENABLE_SNTP_CLIENT_IMPL
    specs.push_back({network::SntpClientIface::NAME, SNTP_CLIENT_IFACE_NAME});
#endif
#if BROOKESIA_HAL_LINUX_NETWORK_ENABLE_HTTP_CLIENT_IMPL
    specs.push_back({network::HttpClientIface::NAME, HTTP_CLIENT_IFACE_NAME});
#endif
    return specs;
}

bool NetworkLinuxDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        connectivity_iface_ = std::make_shared<NetworkConnectivityLinuxIface>(), false,
        "Failed to create connectivity linux"
    );
    interfaces_.emplace(CONNECTIVITY_IFACE_NAME, connectivity_iface_);
#if BROOKESIA_HAL_LINUX_NETWORK_ENABLE_SNTP_CLIENT_IMPL
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        sntp_client_iface_ = std::make_shared<SntpClientLinuxIface>(), false,
        "Failed to create SNTP client linux"
    );
    interfaces_.emplace(SNTP_CLIENT_IFACE_NAME, sntp_client_iface_);
#endif
#if BROOKESIA_HAL_LINUX_NETWORK_ENABLE_HTTP_CLIENT_IMPL
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        http_client_iface_ = std::make_shared<HttpClientLinuxIface>(), false,
        "Failed to create HTTP client linux"
    );
    interfaces_.emplace(HTTP_CLIENT_IFACE_NAME, http_client_iface_);
#endif

    return !interfaces_.empty();
}

void NetworkLinuxDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(CONNECTIVITY_IFACE_NAME);
    connectivity_iface_.reset();
#if BROOKESIA_HAL_LINUX_NETWORK_ENABLE_SNTP_CLIENT_IMPL
    interfaces_.erase(SNTP_CLIENT_IFACE_NAME);
    sntp_client_iface_.reset();
#endif
#if BROOKESIA_HAL_LINUX_NETWORK_ENABLE_HTTP_CLIENT_IMPL
    interfaces_.erase(HTTP_CLIENT_IFACE_NAME);
    http_client_iface_.reset();
#endif
}

#if BROOKESIA_HAL_LINUX_ENABLE_NETWORK_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, NetworkLinuxDevice, NetworkLinuxDevice::DEVICE_NAME, NetworkLinuxDevice::get_instance(),
    BROOKESIA_HAL_LINUX_NETWORK_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
