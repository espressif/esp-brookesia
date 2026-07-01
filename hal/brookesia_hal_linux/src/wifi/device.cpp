/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <sstream>
#include <span>
#include <string>
#include <utility>

#include "boost/thread/lock_guard.hpp"
#include "boost/thread/mutex.hpp"

#include "brookesia/hal_linux/macro_configs.h"
#if !BROOKESIA_HAL_LINUX_WIFI_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_interface/interfaces/network/connectivity.hpp"
#include "brookesia/hal_interface/interfaces/wifi/basic.hpp"
#include "brookesia/hal_interface/interfaces/wifi/softap.hpp"
#include "brookesia/hal_interface/interfaces/wifi/station.hpp"
#include "brookesia/hal_linux/wifi/device.hpp"

namespace esp_brookesia::hal {

namespace {

#if !BROOKESIA_HAL_LINUX_WIFI_BACKEND_STUB
constexpr const char *SOFTAP_CONNECTION_NAME = "BrookesiaHALSoftAP";

std::string shell_quote(const std::string &value)
{
    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

bool command_exists(const char *command)
{
    const std::string check_command = std::string("command -v ") + command + " >/dev/null 2>&1";
    return std::system(check_command.c_str()) == 0;
}

std::string run_command(const std::string &command, int *exit_code = nullptr)
{
    std::array<char, 256> buffer = {};
    std::string output;
    FILE *pipe = popen((command + " 2>/dev/null").c_str(), "r");
    if (pipe == nullptr) {
        if (exit_code != nullptr) {
            *exit_code = -1;
        }
        return output;
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
    const int status = pclose(pipe);
    if (exit_code != nullptr) {
        *exit_code = status;
    }
    return output;
}

std::vector<std::string> split_nmcli_line(const std::string &line)
{
    std::vector<std::string> fields;
    std::string field;
    bool escaped = false;
    for (char ch : line) {
        if (escaped) {
            field.push_back(ch);
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == ':') {
            fields.push_back(field);
            field.clear();
            continue;
        }
        field.push_back(ch);
    }
    fields.push_back(field);
    return fields;
}

std::vector<std::string> split_lines(const std::string &text)
{
    std::vector<std::string> lines;
    std::stringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

std::string get_first_wifi_device()
{
    int exit_code = 0;
    const auto output = run_command("nmcli -t -f DEVICE,TYPE device status", &exit_code);
    if (exit_code != 0) {
        return {};
    }
    for (const auto &line : split_lines(output)) {
        auto fields = split_nmcli_line(line);
        if ((fields.size() >= 2) && (fields[1] == "wifi")) {
            return fields[0];
        }
    }
    return {};
}

int signal_percent_to_rssi(int signal_percent)
{
    return std::clamp(signal_percent / 2 - 100, -100, -30);
}

int parse_int_or_default(const std::string &value, int default_value)
{
    try {
        return std::stoi(value);
    } catch (const std::exception &) {
        return default_value;
    }
}
#endif

} // namespace

class WifiLinuxBackend {
public:
    bool configure(wifi::BasicIface::RuntimeContext runtime, wifi::BasicIface::Callbacks callbacks)
    {
        boost::lock_guard lock(callbacks_mutex_);
        task_scheduler_ = std::move(runtime.task_scheduler);
        task_group_ = runtime.task_group;
        basic_callbacks_ = std::move(callbacks);
        invalidate_callbacks();
        return true;
    }

    bool configure(wifi::StationIface::Callbacks callbacks)
    {
        boost::lock_guard lock(callbacks_mutex_);
        station_callbacks_ = std::move(callbacks);
        invalidate_callbacks();
        return true;
    }

    bool configure(wifi::SoftApIface::Callbacks callbacks)
    {
        boost::lock_guard lock(callbacks_mutex_);
        softap_callbacks_ = std::move(callbacks);
        invalidate_callbacks();
        return true;
    }

    void clear_basic_callbacks()
    {
        boost::lock_guard lock(callbacks_mutex_);
        basic_callbacks_ = {};
        invalidate_callbacks();
    }

    void clear_station_callbacks()
    {
        boost::lock_guard lock(callbacks_mutex_);
        station_callbacks_ = {};
        invalidate_callbacks();
    }

    void clear_softap_callbacks()
    {
        boost::lock_guard lock(callbacks_mutex_);
        softap_callbacks_ = {};
        invalidate_callbacks();
    }

    void clear_callbacks()
    {
        clear_basic_callbacks();
        clear_station_callbacks();
        clear_softap_callbacks();
    }

    bool init()
    {
#if !BROOKESIA_HAL_LINUX_WIFI_BACKEND_STUB
        if (!network_manager_checked_) {
            network_manager_available_ = command_exists("nmcli");
            network_manager_checked_ = true;
            if (network_manager_available_) {
                BROOKESIA_LOGI("NetworkManager Wi-Fi backend initialized through nmcli");
            } else {
                BROOKESIA_LOGW("NetworkManager nmcli command is unavailable, using Wi-Fi stub fallback");
            }
        }
#endif
        backend_initialized_ = true;
        return true;
    }

    void deinit()
    {
        connected_ = false;
        started_ = false;
        initialized_ = false;
        backend_initialized_ = false;
        running_basic_action_ = wifi::BasicAction::Max;
        running_station_action_ = wifi::StationAction::Max;
        clear_callbacks();
    }

    bool start()
    {
        if (!backend_initialized_) {
            return false;
        }
        is_running_ = true;
        return true;
    }

    void stop()
    {
        stop_scan();
        stop_soft_ap();
        is_running_ = false;
        started_ = false;
        connected_ = false;
    }

    void reset_data()
    {
        target_ap_ = {};
        last_ap_ = {};
        connected_aps_.clear();
        scan_params_ = {};
        scan_ap_infos_.clear();
        softap_params_ = {};
    }

    bool do_action(wifi::BasicAction action, bool is_force = false)
    {
        if (!is_running_) {
            return false;
        }
        if ((running_basic_action_ == action) || (!is_force && is_event_ready(get_target_event(action)))) {
            return true;
        }

        running_basic_action_ = action;
        notify_basic_action(action);

        switch (action) {
        case wifi::BasicAction::Init:
            initialized_ = true;
            notify_basic_event(wifi::BasicEvent::Inited, false);
            break;
        case wifi::BasicAction::Deinit:
            stop();
            initialized_ = false;
            notify_basic_event(wifi::BasicEvent::Deinited, false);
            break;
        case wifi::BasicAction::Start:
            initialized_ = true;
            started_ = true;
            notify_basic_event(wifi::BasicEvent::Started, false);
            break;
        case wifi::BasicAction::Stop:
            connected_ = false;
            started_ = false;
            notify_basic_event(wifi::BasicEvent::Stopped, false);
            break;
        default:
            running_basic_action_ = wifi::BasicAction::Max;
            return false;
        }

        running_basic_action_ = wifi::BasicAction::Max;
        return true;
    }

    bool is_action_running(wifi::BasicAction action)
    {
        return running_basic_action_ == action;
    }

    bool is_event_ready(wifi::BasicEvent event)
    {
        switch (event) {
        case wifi::BasicEvent::Inited:
            return initialized_;
        case wifi::BasicEvent::Deinited:
            return !initialized_;
        case wifi::BasicEvent::Started:
            return started_;
        case wifi::BasicEvent::Stopped:
            return !started_;
        default:
            return false;
        }
    }

    bool do_station_action(wifi::StationAction action, bool is_force = false)
    {
        if (!is_running_) {
            return false;
        }
        if ((running_station_action_ == action) || (!is_force && is_station_event_ready(get_target_event(action)))) {
            return true;
        }

        running_station_action_ = action;
        notify_station_action(action);

        switch (action) {
        case wifi::StationAction::Connect:
            if (!target_ap_.is_valid()) {
                notify_station_error();
                running_station_action_ = wifi::StationAction::Max;
                return false;
            }
#if !BROOKESIA_HAL_LINUX_WIFI_BACKEND_STUB
            if (network_manager_available_) {
                if (!connect_station_with_network_manager()) {
                    notify_station_error();
                    running_station_action_ = wifi::StationAction::Max;
                    return false;
                }
                break;
            }
#endif
            initialized_ = true;
            started_ = true;
            connected_ = true;
            last_ap_ = target_ap_;
            last_ap_.is_connectable = true;
            add_connected_ap(last_ap_);
            notify_last_connected_ap_info_updated(last_ap_);
            notify_station_event(wifi::StationEvent::Connected, false);
            break;
        case wifi::StationAction::Disconnect:
#if !BROOKESIA_HAL_LINUX_WIFI_BACKEND_STUB
            if (network_manager_available_) {
                disconnect_station_with_network_manager();
                break;
            }
#endif
            connected_ = false;
            notify_station_event(wifi::StationEvent::Disconnected, false);
            break;
        default:
            running_station_action_ = wifi::StationAction::Max;
            return false;
        }

        running_station_action_ = wifi::StationAction::Max;
        return true;
    }

    bool is_station_action_running(wifi::StationAction action)
    {
        return running_station_action_ == action;
    }

    bool is_station_event_ready(wifi::StationEvent event)
    {
#if !BROOKESIA_HAL_LINUX_WIFI_BACKEND_STUB
        if (network_manager_available_) {
            const bool connected = get_network_status().is_local_network_ready();
            switch (event) {
            case wifi::StationEvent::Connected:
                return connected;
            case wifi::StationEvent::Disconnected:
                return !connected;
            default:
                return false;
            }
        }
#endif
        switch (event) {
        case wifi::StationEvent::Connected:
            return connected_;
        case wifi::StationEvent::Disconnected:
            return !connected_;
        default:
            return false;
        }
    }

    void mark_target_connect_ap_info_connectable(bool connectable)
    {
        target_ap_.is_connectable = connectable;
    }

    bool set_target_connect_ap_info(const wifi::ConnectApInfo &ap_info)
    {
        target_ap_ = ap_info;
        return true;
    }

    bool set_last_connected_ap_info(const wifi::ConnectApInfo &ap_info)
    {
        last_ap_ = ap_info;
        return true;
    }

    bool set_connected_ap_infos(const wifi::ConnectApInfoList &ap_infos)
    {
        connected_aps_ = ap_infos;
        return true;
    }

    const wifi::ConnectApInfo &get_target_connect_ap_info() const
    {
        return target_ap_;
    }

    const wifi::ConnectApInfo &get_last_connected_ap_info() const
    {
        return last_ap_;
    }

    const wifi::ConnectApInfoList &get_connected_ap_infos() const
    {
        return connected_aps_;
    }

    bool set_scan_params(const wifi::ScanParams &params)
    {
        scan_params_ = params;
        return true;
    }

    const wifi::ScanParams &get_scan_params() const
    {
        return scan_params_;
    }

    bool start_scan()
    {
        if (is_scanning_) {
            return true;
        }

        is_scanning_ = true;
        notify_scan_state_changed(true);

#if !BROOKESIA_HAL_LINUX_WIFI_BACKEND_STUB
        if (network_manager_available_ && scan_with_network_manager()) {
            is_scanning_ = false;
            notify_scan_state_changed(false);
            return true;
        }
#endif

        scan_ap_infos_ = {
            wifi::ScanApInfo{
                .ssid = target_ap_.is_valid() ? target_ap_.ssid : std::string("Brookesia-Linux-AP"),
                .is_locked = true,
                .rssi = -42,
                .signal_level = wifi::ScanApInfo::get_signal_level(-42),
                .channel = 6,
            },
            wifi::ScanApInfo{
                .ssid = "Brookesia-Guest",
                .is_locked = false,
                .rssi = -67,
                .signal_level = wifi::ScanApInfo::get_signal_level(-67),
                .channel = 11,
            },
        };
        auto publish_count = std::min(scan_ap_infos_.size(), scan_params_.ap_count);
        std::span<const wifi::ScanApInfo> ap_infos(scan_ap_infos_.data(), publish_count);
        notify_scan_ap_infos_updated(ap_infos);

        is_scanning_ = false;
        notify_scan_state_changed(false);

        return true;
    }

    void stop_scan()
    {
        if (!is_scanning_) {
            return;
        }
        is_scanning_ = false;
        notify_scan_state_changed(false);
    }

    bool set_soft_ap_params(const wifi::SoftApParams &params)
    {
        softap_params_ = params;
        return true;
    }

    const wifi::SoftApParams &get_soft_ap_params() const
    {
        return softap_params_;
    }

    bool start_soft_ap()
    {
#if !BROOKESIA_HAL_LINUX_WIFI_BACKEND_STUB
        if (network_manager_available_) {
            if (!start_soft_ap_with_network_manager()) {
                return false;
            }
            return true;
        }
#endif
        softap_started_ = true;
        notify_softap_event(wifi::SoftApEvent::Started);
        return true;
    }

    void stop_soft_ap()
    {
#if !BROOKESIA_HAL_LINUX_WIFI_BACKEND_STUB
        if (network_manager_available_ && softap_started_) {
            stop_soft_ap_with_network_manager();
            return;
        }
#endif
        if (!softap_started_) {
            return;
        }
        softap_started_ = false;
        notify_softap_event(wifi::SoftApEvent::Stopped);
    }

    bool start_soft_ap_provision()
    {
        provision_running_ = true;
        return start_soft_ap();
    }

    bool stop_soft_ap_provision()
    {
        provision_running_ = false;
        stop_soft_ap();
        return true;
    }

    network::NetworkStatus get_network_status() const
    {
#if !BROOKESIA_HAL_LINUX_WIFI_BACKEND_STUB
        if (network_manager_available_) {
            return get_network_manager_status();
        }
#endif
        network::NetworkStatus status = {
            .interface_type = network::InterfaceType::WifiStation,
            .link_state = connected_ ? network::LinkState::Up : network::LinkState::Down,
            .ip_state = connected_ ? network::IpState::Ready : network::IpState::None,
            .reachability = connected_ ? network::Reachability::LocalOnly : network::Reachability::Unreachable,
        };
        if (connected_) {
            status.ip_info = network::IpInfo{
                .ip = "192.168.4.2",
                .netmask = "255.255.255.0",
                .gateway = "192.168.4.1",
                .dns = "192.168.4.1",
            };
            status.signal_dbm = -42;
            status.connected_duration_ms = 0;
        }
        return status;
    }

private:
    void invalidate_callbacks()
    {
        callback_generation_.fetch_add(1, std::memory_order_relaxed);
    }

    uint64_t get_callback_generation() const
    {
        return callback_generation_.load(std::memory_order_relaxed);
    }

    bool is_callback_generation_valid(uint64_t generation) const
    {
        return generation == get_callback_generation();
    }

    template <typename Callback>
    void post_callback(const char *name, uint64_t generation, Callback callback)
    {
        std::shared_ptr<lib_utils::TaskScheduler> task_scheduler;
        lib_utils::TaskScheduler::Group task_group;
        {
            boost::lock_guard lock(callbacks_mutex_);
            task_scheduler = task_scheduler_;
            task_group = task_group_;
        }

        if (task_scheduler && task_scheduler->is_running()) {
            auto task = [this, name, generation, callback = std::move(callback)]() mutable {
                if (!is_callback_generation_valid(generation))
                {
                    BROOKESIA_LOGD("Drop stale Wi-Fi Linux callback: %1%", name);
                    return;
                }
                BROOKESIA_CHECK_EXCEPTION_EXECUTE(callback(), {}, {
                    BROOKESIA_LOGE("Wi-Fi Linux callback '%1%' raised exception: %2%", name, e.what());
                });
            };
            if (!task_scheduler->post(std::move(task), nullptr, task_group)) {
                BROOKESIA_LOGW("Failed to post Wi-Fi Linux callback: %1%", name);
            }
            return;
        }

        if (!is_callback_generation_valid(generation)) {
            BROOKESIA_LOGD("Drop stale Wi-Fi Linux callback without scheduler: %1%", name);
            return;
        }
        BROOKESIA_CHECK_EXCEPTION_EXECUTE(callback(), {}, {
            BROOKESIA_LOGE("Wi-Fi Linux callback '%1%' raised exception: %2%", name, e.what());
        });
    }

    void notify_basic_action(wifi::BasicAction action)
    {
        std::function<void(wifi::BasicAction)> callback;
        auto generation = uint64_t(0);
        {
            boost::lock_guard lock(callbacks_mutex_);
            callback = basic_callbacks_.on_action;
            generation = get_callback_generation();
        }
        if (!callback) {
            return;
        }
        post_callback("basic action", generation, [callback = std::move(callback), action]() {
            callback(action);
        });
    }

    void notify_station_action(wifi::StationAction action)
    {
        std::function<void(wifi::StationAction)> callback;
        auto generation = uint64_t(0);
        {
            boost::lock_guard lock(callbacks_mutex_);
            callback = station_callbacks_.on_action;
            generation = get_callback_generation();
        }
        if (!callback) {
            return;
        }
        post_callback("station action", generation, [callback = std::move(callback), action]() {
            callback(action);
        });
    }

    static wifi::BasicEvent get_target_event(wifi::BasicAction action)
    {
        switch (action) {
        case wifi::BasicAction::Init:
            return wifi::BasicEvent::Inited;
        case wifi::BasicAction::Deinit:
            return wifi::BasicEvent::Deinited;
        case wifi::BasicAction::Start:
            return wifi::BasicEvent::Started;
        case wifi::BasicAction::Stop:
            return wifi::BasicEvent::Stopped;
        default:
            return wifi::BasicEvent::Max;
        }
    }

    static wifi::StationEvent get_target_event(wifi::StationAction action)
    {
        switch (action) {
        case wifi::StationAction::Connect:
            return wifi::StationEvent::Connected;
        case wifi::StationAction::Disconnect:
            return wifi::StationEvent::Disconnected;
        default:
            return wifi::StationEvent::Max;
        }
    }

    void notify_basic_event(wifi::BasicEvent event, bool is_unexpected)
    {
        std::function<void(wifi::BasicEvent, bool)> callback;
        auto generation = uint64_t(0);
        {
            boost::lock_guard lock(callbacks_mutex_);
            callback = basic_callbacks_.on_event;
            generation = get_callback_generation();
        }
        if (!callback) {
            return;
        }
        post_callback("basic event", generation, [callback = std::move(callback), event, is_unexpected]() {
            callback(event, is_unexpected);
        });
    }

    void notify_station_event(wifi::StationEvent event, bool is_unexpected)
    {
        std::function<void(wifi::StationEvent, bool)> callback;
        auto generation = uint64_t(0);
        {
            boost::lock_guard lock(callbacks_mutex_);
            callback = station_callbacks_.on_event;
            generation = get_callback_generation();
        }
        if (!callback) {
            return;
        }
        post_callback("station event", generation, [callback = std::move(callback), event, is_unexpected]() {
            callback(event, is_unexpected);
        });
    }

    void notify_station_error()
    {
        std::function<void()> callback;
        auto generation = uint64_t(0);
        {
            boost::lock_guard lock(callbacks_mutex_);
            callback = station_callbacks_.on_error;
            generation = get_callback_generation();
        }
        if (!callback) {
            return;
        }
        post_callback("station error", generation, [callback = std::move(callback)]() {
            callback();
        });
    }

    void notify_scan_state_changed(bool is_scanning)
    {
        std::function<void(bool)> callback;
        auto generation = uint64_t(0);
        {
            boost::lock_guard lock(callbacks_mutex_);
            callback = station_callbacks_.on_scan_state_changed;
            generation = get_callback_generation();
        }
        if (!callback) {
            return;
        }
        post_callback("scan state", generation, [callback = std::move(callback), is_scanning]() {
            callback(is_scanning);
        });
    }

    void notify_scan_ap_infos_updated(std::span<const wifi::ScanApInfo> ap_infos)
    {
        std::function<void(std::span<const wifi::ScanApInfo>)> callback;
        auto generation = uint64_t(0);
        {
            boost::lock_guard lock(callbacks_mutex_);
            callback = station_callbacks_.on_scan_ap_infos_updated;
            generation = get_callback_generation();
        }
        if (!callback) {
            return;
        }
        auto ap_infos_copy = wifi::ScanApInfoList(ap_infos.begin(), ap_infos.end());
        post_callback(
            "scan AP infos",
            generation,
        [callback = std::move(callback), ap_infos_copy = std::move(ap_infos_copy)]() mutable {
            callback(std::span<const wifi::ScanApInfo>(ap_infos_copy.data(), ap_infos_copy.size()));
        }
        );
    }

    void notify_last_connected_ap_info_updated(const wifi::ConnectApInfo &ap_info)
    {
        std::function<void(const wifi::ConnectApInfo &)> callback;
        auto generation = uint64_t(0);
        {
            boost::lock_guard lock(callbacks_mutex_);
            callback = station_callbacks_.on_last_connected_ap_info_updated;
            generation = get_callback_generation();
        }
        if (!callback) {
            return;
        }
        post_callback("last connected AP", generation, [callback = std::move(callback), ap_info]() {
            callback(ap_info);
        });
    }

    void notify_connected_ap_infos_updated(const wifi::ConnectApInfoList &ap_infos)
    {
        std::function<void(const wifi::ConnectApInfoList &)> callback;
        auto generation = uint64_t(0);
        {
            boost::lock_guard lock(callbacks_mutex_);
            callback = station_callbacks_.on_connected_ap_infos_updated;
            generation = get_callback_generation();
        }
        if (!callback) {
            return;
        }
        post_callback("connected AP infos", generation, [callback = std::move(callback), ap_infos]() {
            callback(ap_infos);
        });
    }

    void notify_softap_event(wifi::SoftApEvent event)
    {
        std::function<void(wifi::SoftApEvent)> callback;
        auto generation = uint64_t(0);
        {
            boost::lock_guard lock(callbacks_mutex_);
            callback = softap_callbacks_.on_event;
            generation = get_callback_generation();
        }
        if (!callback) {
            return;
        }
        post_callback("softap event", generation, [callback = std::move(callback), event]() {
            callback(event);
        });
    }

    void add_connected_ap(const wifi::ConnectApInfo &ap_info)
    {
        for (auto &stored_ap_info : connected_aps_) {
            if (stored_ap_info.ssid == ap_info.ssid) {
                stored_ap_info = ap_info;
                notify_connected_ap_infos_updated(connected_aps_);
                return;
            }
        }

        connected_aps_.push_back(ap_info);
        notify_connected_ap_infos_updated(connected_aps_);
    }

#if !BROOKESIA_HAL_LINUX_WIFI_BACKEND_STUB
    bool scan_with_network_manager()
    {
        int exit_code = 0;
        const auto output = run_command(
                                "nmcli -t -f SSID,SECURITY,SIGNAL,CHAN device wifi list --rescan yes", &exit_code
                            );
        if (exit_code != 0) {
            BROOKESIA_LOGW("NetworkManager Wi-Fi scan failed, using stub scan results");
            return false;
        }

        wifi::ScanApInfoList ap_infos;
        for (const auto &line : split_lines(output)) {
            auto fields = split_nmcli_line(line);
            if (fields.size() < 4 || fields[0].empty()) {
                continue;
            }
            const int signal = parse_int_or_default(fields[2], 0);
            const int rssi = signal_percent_to_rssi(signal);
            ap_infos.push_back(wifi::ScanApInfo{
                .ssid = fields[0],
                .is_locked = !fields[1].empty(),
                .rssi = rssi,
                .signal_level = wifi::ScanApInfo::get_signal_level(rssi),
                .channel = static_cast<uint8_t>(std::clamp(parse_int_or_default(fields[3], 0), 0, 255)),
            });
            if (ap_infos.size() >= scan_params_.ap_count) {
                break;
            }
        }
        scan_ap_infos_ = std::move(ap_infos);
        std::span<const wifi::ScanApInfo> ap_infos_span(scan_ap_infos_.data(), scan_ap_infos_.size());
        notify_scan_ap_infos_updated(ap_infos_span);
        return true;
    }

    bool connect_station_with_network_manager()
    {
        const std::string password_arg = target_ap_.password.empty() ? "" :
                                         (" password " + shell_quote(target_ap_.password));
        const std::string command = "nmcli device wifi connect " + shell_quote(target_ap_.ssid) + password_arg;
        int exit_code = 0;
        run_command(command, &exit_code);
        if (exit_code != 0) {
            BROOKESIA_LOGE("NetworkManager failed to connect Wi-Fi station to SSID '%1%'", target_ap_.ssid);
            target_ap_.is_connectable = false;
            return false;
        }

        initialized_ = true;
        started_ = true;
        connected_ = true;
        connected_since_ = std::chrono::steady_clock::now();
        last_ap_ = target_ap_;
        last_ap_.is_connectable = true;
        add_connected_ap(last_ap_);
        notify_last_connected_ap_info_updated(last_ap_);
        notify_station_event(wifi::StationEvent::Connected, false);
        return true;
    }

    void disconnect_station_with_network_manager()
    {
        const auto device = get_first_wifi_device();
        if (!device.empty()) {
            int exit_code = 0;
            run_command("nmcli device disconnect " + shell_quote(device), &exit_code);
            if (exit_code != 0) {
                BROOKESIA_LOGW("NetworkManager failed to disconnect Wi-Fi device '%1%'", device);
            }
        }
        connected_ = false;
        notify_station_event(wifi::StationEvent::Disconnected, false);
    }

    bool start_soft_ap_with_network_manager()
    {
        const auto device = get_first_wifi_device();
        if (device.empty()) {
            BROOKESIA_LOGE("NetworkManager SoftAP start failed: Wi-Fi device not found");
            return false;
        }
        if (softap_params_.ssid.empty()) {
            BROOKESIA_LOGE("NetworkManager SoftAP start failed: SSID is empty");
            return false;
        }

        run_command("nmcli connection delete " + shell_quote(SOFTAP_CONNECTION_NAME));
        int exit_code = 0;
        run_command(
            "nmcli connection add type wifi ifname " + shell_quote(device) + " con-name " +
            shell_quote(SOFTAP_CONNECTION_NAME) + " autoconnect no ssid " + shell_quote(softap_params_.ssid),
            &exit_code
        );
        if (exit_code != 0) {
            BROOKESIA_LOGE("NetworkManager SoftAP connection create failed");
            return false;
        }

        std::string modify_command = "nmcli connection modify " + shell_quote(SOFTAP_CONNECTION_NAME) +
                                     " 802-11-wireless.mode ap ipv4.method shared";
        if (softap_params_.has_channel()) {
            modify_command += " 802-11-wireless.channel " + std::to_string(softap_params_.get_channel());
        }
        if (!softap_params_.password.empty()) {
            modify_command += " wifi-sec.key-mgmt wpa-psk wifi-sec.psk " + shell_quote(softap_params_.password);
        }
        run_command(modify_command, &exit_code);
        if (exit_code != 0) {
            BROOKESIA_LOGE("NetworkManager SoftAP connection configure failed");
            return false;
        }

        run_command("nmcli connection up " + shell_quote(SOFTAP_CONNECTION_NAME), &exit_code);
        if (exit_code != 0) {
            BROOKESIA_LOGE("NetworkManager SoftAP start failed; hardware or permissions may not support AP mode");
            return false;
        }

        softap_started_ = true;
        notify_softap_event(wifi::SoftApEvent::Started);
        return true;
    }

    void stop_soft_ap_with_network_manager()
    {
        int exit_code = 0;
        run_command("nmcli connection down " + shell_quote(SOFTAP_CONNECTION_NAME), &exit_code);
        if (exit_code != 0) {
            BROOKESIA_LOGW("NetworkManager SoftAP stop command returned an error");
        }
        softap_started_ = false;
        notify_softap_event(wifi::SoftApEvent::Stopped);
    }

    network::NetworkStatus get_network_manager_status() const
    {
        network::NetworkStatus status = {
            .interface_type = network::InterfaceType::WifiStation,
            .link_state = network::LinkState::Down,
            .ip_state = network::IpState::None,
            .reachability = network::Reachability::Unreachable,
        };

        int exit_code = 0;
        const auto device_output = run_command("nmcli -t -f DEVICE,TYPE,STATE,CONNECTION device status", &exit_code);
        if (exit_code != 0) {
            return status;
        }

        std::string wifi_device;
        bool connected = false;
        for (const auto &line : split_lines(device_output)) {
            auto fields = split_nmcli_line(line);
            if (fields.size() < 3 || (fields[1] != "wifi")) {
                continue;
            }
            wifi_device = fields[0];
            connected = (fields[2] == "connected");
            break;
        }
        if (wifi_device.empty()) {
            return status;
        }

        status.link_state = connected ? network::LinkState::Up : network::LinkState::Down;
        if (!connected) {
            return status;
        }

        status.ip_state = network::IpState::Acquiring;
        const auto info_output = run_command("nmcli -t -f IP4.ADDRESS,IP4.GATEWAY,IP4.DNS device show " +
                                             shell_quote(wifi_device), &exit_code);
        if (exit_code != 0) {
            return status;
        }

        network::IpInfo ip_info;
        for (const auto &line : split_lines(info_output)) {
            auto fields = split_nmcli_line(line);
            if (fields.size() < 2) {
                continue;
            }
            if (fields[0].starts_with("IP4.ADDRESS")) {
                ip_info.ip = fields[1];
                const auto slash = ip_info.ip.find('/');
                if (slash != std::string::npos) {
                    ip_info.ip = ip_info.ip.substr(0, slash);
                }
            } else if (fields[0].starts_with("IP4.GATEWAY")) {
                ip_info.gateway = fields[1];
            } else if (fields[0].starts_with("IP4.DNS") && ip_info.dns.empty()) {
                ip_info.dns = fields[1];
            }
        }
        if (!ip_info.ip.empty()) {
            ip_info.netmask = "255.255.255.0";
            status.ip_state = network::IpState::Ready;
            status.reachability = network::Reachability::LocalOnly;
            status.ip_info = ip_info;
        }

        const auto signal_output = run_command("nmcli -t -f IN-USE,SIGNAL device wifi list", &exit_code);
        if (exit_code == 0) {
            for (const auto &line : split_lines(signal_output)) {
                auto fields = split_nmcli_line(line);
                if (fields.size() >= 2 && fields[0] == "*") {
                    status.signal_dbm = signal_percent_to_rssi(parse_int_or_default(fields[1], 0));
                    break;
                }
            }
        }
        if (connected_) {
            const auto duration = std::chrono::steady_clock::now() - connected_since_;
            status.connected_duration_ms =
                static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
        }
        return status;
    }
#endif

    bool initialized_ = false;
    bool backend_initialized_ = false;
    bool is_running_ = false;
    bool started_ = false;
    bool connected_ = false;
    bool is_scanning_ = false;
    bool softap_started_ = false;
    bool provision_running_ = false;
    wifi::BasicAction running_basic_action_ = wifi::BasicAction::Max;
    wifi::StationAction running_station_action_ = wifi::StationAction::Max;
    wifi::ConnectApInfo target_ap_{};
    wifi::ConnectApInfo last_ap_{};
    wifi::ConnectApInfoList connected_aps_;
    wifi::ScanParams scan_params_{};
    wifi::ScanApInfoList scan_ap_infos_;
    wifi::SoftApParams softap_params_{};
    mutable boost::mutex callbacks_mutex_;
    std::atomic<uint64_t> callback_generation_{0};
    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler_;
    lib_utils::TaskScheduler::Group task_group_;
    wifi::BasicIface::Callbacks basic_callbacks_;
    wifi::StationIface::Callbacks station_callbacks_;
    wifi::SoftApIface::Callbacks softap_callbacks_;
#if !BROOKESIA_HAL_LINUX_WIFI_BACKEND_STUB
    bool network_manager_checked_ = false;
    bool network_manager_available_ = false;
    std::chrono::steady_clock::time_point connected_since_ = std::chrono::steady_clock::now();
#endif
};

namespace {

class WifiLinuxBasicIface: public wifi::BasicIface {
public:
    explicit WifiLinuxBasicIface(std::shared_ptr<WifiLinuxBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(RuntimeContext runtime, Callbacks callbacks) override
    {
        return backend_->configure(std::move(runtime), std::move(callbacks));
    }

    void clear_callbacks() override
    {
        backend_->clear_basic_callbacks();
    }

    bool init() override
    {
        return backend_->init();
    }

    void deinit() override
    {
        backend_->deinit();
    }

    bool start() override
    {
        return backend_->start();
    }

    void stop() override
    {
        backend_->stop();
    }

    void reset_data() override
    {
        backend_->reset_data();
    }

    bool do_action(wifi::BasicAction action, bool is_force = false) override
    {
        return backend_->do_action(action, is_force);
    }

    bool is_action_running(wifi::BasicAction action) override
    {
        return backend_->is_action_running(action);
    }

    bool is_event_ready(wifi::BasicEvent event) override
    {
        return backend_->is_event_ready(event);
    }

private:
    std::shared_ptr<WifiLinuxBackend> backend_;
};

class WifiLinuxStationIface: public wifi::StationIface {
public:
    explicit WifiLinuxStationIface(std::shared_ptr<WifiLinuxBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(Callbacks callbacks) override
    {
        return backend_->configure(std::move(callbacks));
    }

    void clear_callbacks() override
    {
        backend_->clear_station_callbacks();
    }

    bool do_action(wifi::StationAction action, bool is_force = false) override
    {
        return backend_->do_station_action(action, is_force);
    }

    bool is_action_running(wifi::StationAction action) override
    {
        return backend_->is_station_action_running(action);
    }

    bool is_event_ready(wifi::StationEvent event) override
    {
        return backend_->is_station_event_ready(event);
    }

    void mark_target_connect_ap_info_connectable(bool connectable) override
    {
        backend_->mark_target_connect_ap_info_connectable(connectable);
    }

    bool set_target_connect_ap_info(const wifi::ConnectApInfo &ap_info) override
    {
        return backend_->set_target_connect_ap_info(ap_info);
    }

    bool set_last_connected_ap_info(const wifi::ConnectApInfo &ap_info) override
    {
        return backend_->set_last_connected_ap_info(ap_info);
    }

    bool set_connected_ap_infos(const wifi::ConnectApInfoList &ap_infos) override
    {
        return backend_->set_connected_ap_infos(ap_infos);
    }

    const wifi::ConnectApInfo &get_target_connect_ap_info() const override
    {
        return backend_->get_target_connect_ap_info();
    }

    const wifi::ConnectApInfo &get_last_connected_ap_info() const override
    {
        return backend_->get_last_connected_ap_info();
    }

    const wifi::ConnectApInfoList &get_connected_ap_infos() const override
    {
        return backend_->get_connected_ap_infos();
    }

    bool set_scan_params(const wifi::ScanParams &params) override
    {
        return backend_->set_scan_params(params);
    }

    const wifi::ScanParams &get_scan_params() const override
    {
        return backend_->get_scan_params();
    }

    bool start_scan() override
    {
        return backend_->start_scan();
    }

    void stop_scan() override
    {
        backend_->stop_scan();
    }

private:
    std::shared_ptr<WifiLinuxBackend> backend_;
};

class WifiLinuxSoftApIface: public wifi::SoftApIface {
public:
    explicit WifiLinuxSoftApIface(std::shared_ptr<WifiLinuxBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(Callbacks callbacks) override
    {
        return backend_->configure(std::move(callbacks));
    }

    void clear_callbacks() override
    {
        backend_->clear_softap_callbacks();
    }

    bool set_params(const wifi::SoftApParams &params) override
    {
        return backend_->set_soft_ap_params(params);
    }

    const wifi::SoftApParams &get_params() const override
    {
        return backend_->get_soft_ap_params();
    }

    bool start() override
    {
        return backend_->start_soft_ap();
    }

    void stop() override
    {
        backend_->stop_soft_ap();
    }

    bool start_provision() override
    {
        return backend_->start_soft_ap_provision();
    }

    bool stop_provision() override
    {
        return backend_->stop_soft_ap_provision();
    }

private:
    std::shared_ptr<WifiLinuxBackend> backend_;
};

class WifiLinuxConnectivityIface: public network::ConnectivityIface {
public:
    explicit WifiLinuxConnectivityIface(std::shared_ptr<WifiLinuxBackend> backend)
        : backend_(std::move(backend))
    {
    }

    network::NetworkStatus get_status() const override
    {
        return backend_->get_network_status();
    }

private:
    std::shared_ptr<WifiLinuxBackend> backend_;
};

} // namespace

bool WifiLinuxDevice::probe()
{
    return true;
}

std::vector<InterfaceSpec> WifiLinuxDevice::get_interface_specs() const
{
    return {
        {wifi::BasicIface::NAME, BASIC_IFACE_NAME},
        {wifi::StationIface::NAME, STATION_IFACE_NAME},
        {wifi::SoftApIface::NAME, SOFTAP_IFACE_NAME},
        {network::ConnectivityIface::NAME, CONNECTIVITY_IFACE_NAME},
    };
}

bool WifiLinuxDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        backend_ = std::make_shared<WifiLinuxBackend>(), false, "Failed to create Wi-Fi linux backend"
    );

    interfaces_.emplace(BASIC_IFACE_NAME, std::make_shared<WifiLinuxBasicIface>(backend_));
    interfaces_.emplace(STATION_IFACE_NAME, std::make_shared<WifiLinuxStationIface>(backend_));
    interfaces_.emplace(SOFTAP_IFACE_NAME, std::make_shared<WifiLinuxSoftApIface>(backend_));
    interfaces_.emplace(CONNECTIVITY_IFACE_NAME, std::make_shared<WifiLinuxConnectivityIface>(backend_));

    return true;
}

void WifiLinuxDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(BASIC_IFACE_NAME);
    interfaces_.erase(STATION_IFACE_NAME);
    interfaces_.erase(SOFTAP_IFACE_NAME);
    interfaces_.erase(CONNECTIVITY_IFACE_NAME);
    backend_.reset();
}

#if BROOKESIA_HAL_LINUX_ENABLE_WIFI_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, WifiLinuxDevice, WifiLinuxDevice::DEVICE_NAME, WifiLinuxDevice::get_instance(),
    BROOKESIA_HAL_LINUX_WIFI_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
