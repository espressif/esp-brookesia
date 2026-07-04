/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <netdb.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include "brookesia/hal_linux/macro_configs.h"
#if !BROOKESIA_HAL_LINUX_NETWORK_SNTP_CLIENT_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "sntp_client_impl.hpp"

namespace esp_brookesia::hal {

namespace {

constexpr std::size_t MAX_SNTP_SERVERS = 4;
constexpr const char *DEFAULT_SERVER = "pool.ntp.org";
constexpr const char *DEFAULT_TIMEZONE = "CST-8";
constexpr const char *NTP_PORT = "123";
constexpr uint32_t DEFAULT_WAIT_TIME_SYNC_TIMEOUT_MS = 1000;
constexpr uint64_t NTP_UNIX_EPOCH_OFFSET = 2208988800ULL;
constexpr std::size_t NTP_PACKET_SIZE = 48;

std::string make_errno_error(const char *operation)
{
    return std::string(operation) + ": " + std::strerror(errno);
}

uint32_t read_be32(const std::array<uint8_t, NTP_PACKET_SIZE> &packet, std::size_t offset)
{
    return (static_cast<uint32_t>(packet[offset]) << 24) |
           (static_cast<uint32_t>(packet[offset + 1]) << 16) |
           (static_cast<uint32_t>(packet[offset + 2]) << 8) |
           static_cast<uint32_t>(packet[offset + 3]);
}

bool decode_ntp_response(const std::array<uint8_t, NTP_PACKET_SIZE> &packet, uint64_t &unix_time)
{
    const uint8_t mode = packet[0] & 0x07;
    const uint8_t stratum = packet[1];
    const uint64_t ntp_seconds = read_be32(packet, 40);

    if ((mode != 4) || (stratum == 0) || (ntp_seconds <= NTP_UNIX_EPOCH_OFFSET)) {
        return false;
    }

    unix_time = ntp_seconds - NTP_UNIX_EPOCH_OFFSET;
    return true;
}

bool query_ntp_address(const addrinfo &address, uint32_t timeout_ms, uint64_t &unix_time, std::string &error)
{
    const int sock = socket(address.ai_family, address.ai_socktype, address.ai_protocol);
    if (sock < 0) {
        error = make_errno_error("socket");
        return false;
    }
    auto close_socket = lib_utils::FunctionGuard([sock]() {
        close(sock);
    });

    if (connect(sock, address.ai_addr, address.ai_addrlen) != 0) {
        error = make_errno_error("connect");
        return false;
    }

    std::array<uint8_t, NTP_PACKET_SIZE> request = {};
    request[0] = 0x1b; // LI = 0, version = 3, mode = client.
    const ssize_t sent = send(sock, request.data(), request.size(), 0);
    if (sent != static_cast<ssize_t>(request.size())) {
        error = make_errno_error("send");
        return false;
    }

    pollfd descriptor = {
        .fd = sock,
        .events = POLLIN,
        .revents = 0,
    };
    const int poll_result = poll(&descriptor, 1, static_cast<int>(timeout_ms));
    if (poll_result == 0) {
        error = "NTP receive timeout";
        return false;
    }
    if (poll_result < 0) {
        error = make_errno_error("poll");
        return false;
    }

    std::array<uint8_t, NTP_PACKET_SIZE> response = {};
    const ssize_t received = recv(sock, response.data(), response.size(), 0);
    if (received < static_cast<ssize_t>(response.size())) {
        error = make_errno_error("recv");
        return false;
    }
    if (!decode_ntp_response(response, unix_time)) {
        error = "Invalid NTP response";
        return false;
    }

    return true;
}

bool query_ntp_server(const std::string &server, uint32_t timeout_ms, uint64_t &unix_time, std::string &error)
{
    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    addrinfo *addresses = nullptr;
    const int ret = getaddrinfo(server.c_str(), NTP_PORT, &hints, &addresses);
    if (ret != 0) {
        error = std::string("getaddrinfo: ") + gai_strerror(ret);
        return false;
    }
    auto free_addresses = lib_utils::FunctionGuard([addresses]() {
        freeaddrinfo(addresses);
    });

    for (auto *address = addresses; address != nullptr; address = address->ai_next) {
        if (query_ntp_address(*address, timeout_ms, unix_time, error)) {
            return true;
        }
    }

    return false;
}

} // namespace

bool SntpClientLinuxIface::init(const Config &config)
{
    if (!set_servers(config.servers)) {
        return false;
    }
    if (!set_timezone(config.timezone.empty() ? DEFAULT_TIMEZONE : config.timezone)) {
        return false;
    }
    is_initialized_ = true;
    is_running_ = false;
    is_time_synced_ = false;
    return true;
}

void SntpClientLinuxIface::deinit()
{
    is_initialized_ = false;
    is_running_ = false;
    is_time_synced_ = false;
}

bool SntpClientLinuxIface::start()
{
    if (!is_initialized_) {
        last_error_ = "SNTP linux is not initialized";
        return false;
    }
    is_running_ = true;
    is_time_synced_ = false;
    last_error_.clear();
    return true;
}

void SntpClientLinuxIface::stop()
{
    is_running_ = false;
    is_time_synced_ = false;
}

bool SntpClientLinuxIface::is_initialized() const
{
    return is_initialized_;
}

bool SntpClientLinuxIface::is_running() const
{
    return is_running_;
}

bool SntpClientLinuxIface::wait_time_sync(uint32_t timeout_ms)
{
    if (!is_running_) {
        last_error_ = "SNTP linux is not running";
        return false;
    }

    const uint32_t effective_timeout_ms = (timeout_ms == 0) ? DEFAULT_WAIT_TIME_SYNC_TIMEOUT_MS : timeout_ms;
    for (const auto &server : servers_) {
        if (server.empty()) {
            continue;
        }

        uint64_t unix_time = 0;
        std::string error;
        if (query_ntp_server(server, effective_timeout_ms, unix_time, error)) {
            BROOKESIA_LOGI("SNTP linux synchronized with %1%, unix_time=%2%", server, unix_time);
            is_time_synced_ = true;
            last_error_.clear();
            return true;
        }
        last_error_ = "Failed to sync with " + server + ": " + error;
        BROOKESIA_LOGW("%1%", last_error_);
    }

    if (last_error_.empty()) {
        last_error_ = "No SNTP server is configured";
    }
    return false;
}

bool SntpClientLinuxIface::is_time_synced() const
{
    return is_time_synced_;
}

bool SntpClientLinuxIface::set_servers(const std::vector<std::string> &servers)
{
    if (servers.size() > MAX_SNTP_SERVERS) {
        last_error_ = "Too many SNTP servers";
        return false;
    }
    servers_ = servers;
    if (servers_.empty()) {
        servers_.push_back(DEFAULT_SERVER);
    }
    last_error_.clear();
    return true;
}

std::vector<std::string> SntpClientLinuxIface::get_servers() const
{
    return servers_;
}

std::size_t SntpClientLinuxIface::get_max_servers() const
{
    return MAX_SNTP_SERVERS;
}

bool SntpClientLinuxIface::set_timezone(const std::string &timezone)
{
    if (timezone.empty()) {
        last_error_ = "Timezone is empty";
        return false;
    }
    timezone_ = timezone;
    setenv("TZ", timezone_.c_str(), 1);
    tzset();
    last_error_.clear();
    return true;
}

std::string SntpClientLinuxIface::get_timezone() const
{
    return timezone_;
}

} // namespace esp_brookesia::hal
