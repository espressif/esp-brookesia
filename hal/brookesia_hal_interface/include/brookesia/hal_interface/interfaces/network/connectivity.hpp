/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

/**
 * @file connectivity.hpp
 * @brief Declares the network connectivity status HAL interface.
 */

namespace esp_brookesia::hal::network {

enum class InterfaceType {
    Unknown,
    WifiStation,
    Ethernet,
    Cellular,
};

enum class LinkState {
    Unknown,
    Down,
    Connecting,
    Up,
};

enum class IpState {
    Unknown,
    None,
    Acquiring,
    Ready,
    Failed,
};

enum class Reachability {
    Unknown,
    Unreachable,
    LocalOnly,
    Internet,
};

enum class ConnectivityState {
    Unknown,
    Offline,
    Connecting,
    ConnectedNoIp,
    LocalNetworkReady,
    InternetReady,
};

struct IpInfo {
    std::string ip;
    std::string netmask;
    std::string gateway;
    std::string dns;
};

struct NetworkStatus {
    InterfaceType interface_type = InterfaceType::Unknown;
    LinkState link_state = LinkState::Unknown;
    IpState ip_state = IpState::Unknown;
    Reachability reachability = Reachability::Unknown;
    std::optional<IpInfo> ip_info = std::nullopt;
    std::optional<int> signal_dbm = std::nullopt;
    std::optional<uint64_t> connected_duration_ms = std::nullopt;

    bool is_link_up() const
    {
        return link_state == LinkState::Up;
    }

    bool has_ip() const
    {
        return ip_state == IpState::Ready;
    }

    bool is_local_network_ready() const
    {
        return is_link_up() && has_ip();
    }

    bool is_internet_ready() const
    {
        return is_local_network_ready() && (reachability == Reachability::Internet);
    }

    ConnectivityState state() const
    {
        if (link_state == LinkState::Down) {
            return ConnectivityState::Offline;
        }
        if ((link_state == LinkState::Connecting) || (ip_state == IpState::Acquiring)) {
            return ConnectivityState::Connecting;
        }
        if ((link_state == LinkState::Up) && (ip_state != IpState::Ready)) {
            return ConnectivityState::ConnectedNoIp;
        }
        if ((link_state == LinkState::Up) && (ip_state == IpState::Ready) &&
                (reachability == Reachability::Internet)) {
            return ConnectivityState::InternetReady;
        }
        if ((link_state == LinkState::Up) && (ip_state == IpState::Ready)) {
            return ConnectivityState::LocalNetworkReady;
        }
        return ConnectivityState::Unknown;
    }
};

/**
 * @brief Network connectivity status provider.
 */
class ConnectivityIface: public Interface {
public:
    static constexpr const char *NAME = "NetworkConnectivity";

    ConnectivityIface()
        : Interface(NAME)
    {
    }

    ~ConnectivityIface() override = default;

    virtual NetworkStatus get_status() const = 0;

    virtual bool is_network_ready() const
    {
        return get_status().is_local_network_ready();
    }

    virtual bool is_internet_ready() const
    {
        return get_status().is_internet_ready();
    }
};

BROOKESIA_DESCRIBE_ENUM(InterfaceType, Unknown, WifiStation, Ethernet, Cellular);
BROOKESIA_DESCRIBE_ENUM(LinkState, Unknown, Down, Connecting, Up);
BROOKESIA_DESCRIBE_ENUM(IpState, Unknown, None, Acquiring, Ready, Failed);
BROOKESIA_DESCRIBE_ENUM(Reachability, Unknown, Unreachable, LocalOnly, Internet);
BROOKESIA_DESCRIBE_ENUM(
    ConnectivityState, Unknown, Offline, Connecting, ConnectedNoIp, LocalNetworkReady, InternetReady
);
BROOKESIA_DESCRIBE_STRUCT(IpInfo, (), (ip, netmask, gateway, dns));
BROOKESIA_DESCRIBE_STRUCT(
    NetworkStatus, (),
    (interface_type, link_state, ip_state, reachability, ip_info, signal_dbm, connected_duration_ms)
);

} // namespace esp_brookesia::hal::network
