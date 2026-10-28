/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include "brookesia/service_bt/macro_configs.h"
#if !BROOKESIA_SERVICE_BT_SERVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_manager/dataflow/registry.hpp"
#include "brookesia/service_manager/service/manager.hpp"
#include "brookesia/service_bt.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::service::bt {

std::string Bt::get_component_version()
{
    return "0.8.0";
}

bool Bt::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    ble_iface_ = hal::acquire_first_interface<hal::bluetooth::ble::PeripheralIface>();
    a2dp_iface_ = hal::acquire_first_interface<hal::bluetooth::A2dpSinkIface>();
    capabilities_.ble_supported = static_cast<bool>(ble_iface_);
    if (capabilities_.ble_supported) {
        capabilities_.profiles.push_back(Profile::BleGattPeripheral);
    }
    if (a2dp_iface_) {
        capabilities_.classic_supported = a2dp_iface_->is_supported();
        if (capabilities_.classic_supported) {
            capabilities_.profiles = {Profile::A2dpSink, Profile::AvrcpController};
            if (capabilities_.ble_supported) {
                capabilities_.profiles.insert(capabilities_.profiles.begin(), Profile::BleGattPeripheral);
            }
        }
    } else {
        BROOKESIA_LOGW("Bluetooth A2DP HAL interface is unavailable");
        capabilities_.profiles = capabilities_.ble_supported ?
                                 std::vector<Profile> {Profile::BleGattPeripheral} : std::vector<Profile> {};
        capabilities_.classic_supported = false;
    }
    set_host_state(HostState::Idle, false);
    return true;
}

void Bt::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_a2dp(false);
    a2dp_iface_.reset();
    ble_iface_.reset();
    std::lock_guard<std::mutex> lock(mutex_);
    connection_.reset();
    capabilities_ = {};
    host_state_ = HostState::Idle;
}

bool Bt::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    set_host_state(HostState::Idle, false);
    publish_event(
        BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::ProfileAvailabilityChanged),
        std::vector<EventItem> {BROOKESIA_DESCRIBE_TO_STR(Profile::BleGattPeripheral), capabilities_.ble_supported}
    );
    const bool classic_supported = capabilities_.classic_supported;
    publish_event(
        BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::ProfileAvailabilityChanged),
        std::vector<EventItem> {BROOKESIA_DESCRIBE_TO_STR(Profile::A2dpSink), classic_supported}
    );
    publish_event(
        BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::ProfileAvailabilityChanged),
        std::vector<EventItem> {BROOKESIA_DESCRIBE_TO_STR(Profile::AvrcpController), classic_supported}
    );
    return true;
}

void Bt::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    stop_a2dp(false);
    set_host_state(HostState::Idle, false);
}

std::expected<boost::json::object, std::string> Bt::function_get_capabilities()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return BROOKESIA_DESCRIBE_TO_JSON(capabilities_).as_object();
}

std::expected<void, std::string> Bt::function_set_device_config(const boost::json::object &config_json)
{
    DeviceConfig config;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config_json, config) || config.device_name.empty()) {
        return std::unexpected("Invalid Bluetooth device configuration");
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (hal_started_) {
            return std::unexpected("Bluetooth device configuration cannot change while started");
        }
        device_config_ = std::move(config);
    }
    return {};
}

std::expected<boost::json::object, std::string> Bt::function_get_device_config()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return BROOKESIA_DESCRIBE_TO_JSON(device_config_).as_object();
}

std::expected<void, std::string> Bt::function_start()
{
    return start_a2dp() ? std::expected<void, std::string> {} :
           std::unexpected("Bluetooth A2DP Sink is unavailable or failed to start");
}

std::expected<void, std::string> Bt::function_stop()
{
    return stop_a2dp(true) ? std::expected<void, std::string> {} :
           std::unexpected("Failed to stop Bluetooth A2DP Sink");
}

std::expected<boost::json::object, std::string> Bt::function_get_state()
{
    Helper::State state;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state.host_state = host_state_;
        state.is_supported = capabilities_.classic_supported;
        state.is_started = hal_started_;
        state.profiles = capabilities_.profiles;
    }
    return BROOKESIA_DESCRIBE_TO_JSON(state).as_object();
}

std::expected<boost::json::array, std::string> Bt::function_list_profiles()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return BROOKESIA_DESCRIBE_TO_JSON(capabilities_.profiles).as_array();
}

std::expected<boost::json::array, std::string> Bt::function_get_connections()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connection_) {
        return boost::json::array{};
    }
    return boost::json::array{BROOKESIA_DESCRIBE_TO_JSON(*connection_).as_object()};
}

std::expected<void, std::string> Bt::function_disconnect(double connection_id)
{
    if (!std::isfinite(connection_id) || connection_id < 0 || std::floor(connection_id) != connection_id) {
        return std::unexpected("ConnectionId must be a non-negative integer");
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connection_ || connection_->connection_id != static_cast<uint16_t>(connection_id)) {
            return std::unexpected("Bluetooth connection does not exist");
        }
    }
    if (!a2dp_iface_ || !a2dp_iface_->disconnect()) {
        return std::unexpected("Bluetooth connection does not exist");
    }
    return {};
}

std::expected<void, std::string> Bt::function_a2dp_sink_start()
{
    return function_start();
}

std::expected<void, std::string> Bt::function_a2dp_sink_stop()
{
    return function_stop();
}

std::expected<void, std::string> Bt::function_a2dp_pause()
{
    return (a2dp_iface_ && a2dp_iface_->pause()) ? std::expected<void, std::string> {} :
           std::unexpected("A2DP pause is unavailable");
}

std::expected<void, std::string> Bt::function_a2dp_resume()
{
    return (a2dp_iface_ && a2dp_iface_->resume()) ? std::expected<void, std::string> {} :
           std::unexpected("A2DP resume is unavailable");
}

std::expected<void, std::string> Bt::function_a2dp_next()
{
    return (a2dp_iface_ && a2dp_iface_->next()) ? std::expected<void, std::string> {} :
           std::unexpected("A2DP next is unavailable");
}

std::expected<void, std::string> Bt::function_a2dp_previous()
{
    return (a2dp_iface_ && a2dp_iface_->previous()) ? std::expected<void, std::string> {} :
           std::unexpected("A2DP previous is unavailable");
}

std::expected<void, std::string> Bt::function_a2dp_set_volume(double volume)
{
    if (!std::isfinite(volume) || volume < 0 || volume > 100 || std::floor(volume) != volume) {
        return std::unexpected("Volume must be an integer in the range 0..100");
    }
    return (a2dp_iface_ && a2dp_iface_->set_volume(static_cast<uint8_t>(volume))) ?
           std::expected<void, std::string> {} : std::unexpected("A2DP volume control is unavailable");
}

std::expected<double, std::string> Bt::function_a2dp_get_volume()
{
    if (!a2dp_iface_) {
        return std::unexpected("A2DP interface is unavailable");
    }
    return static_cast<double>(a2dp_iface_->get_volume());
}

bool Bt::start_a2dp()
{
    if (!a2dp_iface_ || !a2dp_iface_->is_supported()) {
        set_host_state(HostState::Error);
        publish_error("Start", -1, "A2DP unavailable on this Bluetooth controller");
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (hal_started_) {
            return true;
        }
    }

    set_host_state(HostState::Starting);
    const auto generation = callback_generation_.fetch_add(1) + 1;
    DeviceConfig device_config;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        device_config = device_config_;
    }
    hal::bluetooth::A2dpSinkIface::Config config{
        .device = std::move(device_config),
        .output_format = {.sample_rate = 48000, .channels = 2, .bits = 16},
    };
    if (!a2dp_iface_->configure(config, make_hal_callbacks(generation))) {
        publish_error("Configure", -1, "A2DP HAL rejected configuration");
        set_host_state(HostState::Error);
        return false;
    }
    hal_configured_ = true;
    if (!a2dp_iface_->init()) {
        publish_error("Start", -1, "A2DP HAL failed to start");
        stop_a2dp(false);
        set_host_state(HostState::Error);
        return false;
    }
    hal_initialized_ = true;
    if (!a2dp_iface_->start()) {
        publish_error("Start", -1, "A2DP HAL failed to start");
        stop_a2dp(false);
        set_host_state(HostState::Error);
        return false;
    }
    hal_started_ = true;
    set_host_state(HostState::Started);
    return true;
}

bool Bt::stop_a2dp(bool publish_state)
{
    callback_generation_.fetch_add(1);
    close_playback_operation();
    if (!a2dp_iface_) {
        return true;
    }

    bool result = true;
    if (publish_state) {
        set_host_state(HostState::Stopping);
    }
    if (hal_started_) {
        a2dp_iface_->stop();
    }
    if (hal_initialized_) {
        a2dp_iface_->deinit();
    }
    a2dp_iface_->clear_callbacks();
    hal_started_ = false;
    hal_initialized_ = false;
    hal_configured_ = false;
    connection_.reset();
    connection_state_ = ConnectionState::Disconnected;
    stream_state_ = StreamState::Idle;
    set_host_state(publish_state ? HostState::Idle : HostState::Idle, publish_state);
    return result;
}

hal::bluetooth::A2dpSinkIface::Callbacks Bt::make_hal_callbacks(uint64_t generation)
{
    return {
        .on_connection_changed = [this, generation](const PeerInfo & peer, ConnectionState state)
        {
            post_hal_callback(generation, [this, peer, state]() {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    connection_state_ = state;
                    if (state == ConnectionState::Disconnected) {
                        connection_.reset();
                    } else {
                        connection_ = peer;
                    }
                }
                if (state == ConnectionState::Disconnected) {
                    close_playback_operation();
                }
                publish_event(
                    BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::ConnectionStateChanged),
                {BROOKESIA_DESCRIBE_TO_STR(state), BROOKESIA_DESCRIBE_TO_JSON(peer).as_object()}
                );
            });
        },
        .on_stream_changed = [this, generation](StreamState state)
        {
            post_hal_callback(generation, [this, state]() {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    stream_state_ = state;
                }
                if (state == StreamState::Stopped || state == StreamState::Error) {
                    close_playback_operation();
                }
                publish_event(BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::A2dpStreamStateChanged), {BROOKESIA_DESCRIBE_TO_STR(state)});
            });
        },
        .on_playback_status_changed = [this, generation](PlaybackStatus status)
        {
            post_hal_callback(generation, [this, status]() {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    playback_status_ = status;
                }
                publish_event(BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::PlaybackStatusChanged), {BROOKESIA_DESCRIBE_TO_STR(status)});
            });
        },
        .on_metadata_changed = [this, generation](const TrackMetadata & metadata)
        {
            post_hal_callback(generation, [this, metadata]() {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    metadata_ = metadata;
                }
                publish_event(
                    BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::MetadataChanged),
                    std::vector<EventItem> {BROOKESIA_DESCRIBE_TO_JSON(metadata).as_object()}
                );
            });
        },
        .on_volume_changed = [this, generation](uint8_t volume)
        {
            post_hal_callback(generation, [this, volume]() {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    volume_ = volume;
                }
                publish_event(
                    BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::VolumeChanged),
                    std::vector<EventItem> {static_cast<double>(volume)}
                );
            });
        },
        .on_pcm = [this, generation](PcmFrame frame)
        {
            post_hal_callback(generation, [this, frame = std::move(frame)]() mutable { handle_pcm(std::move(frame)); });
        },
        .on_error = [this, generation](std::string message)
        {
            post_hal_callback(generation, [this, message = std::move(message)]() {
                publish_error("HAL", -1, message);
            });
        },
    };
}

void Bt::post_hal_callback(uint64_t generation, std::function<void()> callback)
{
    if (generation != callback_generation_.load()) {
        return;
    }
    auto scheduler = get_task_scheduler();
    if (!scheduler) {
        return;
    }
    scheduler->post([this, generation, callback = std::move(callback)]() mutable {
        if (generation == callback_generation_.load() && is_running())
        {
            callback();
        }
    }, nullptr, get_call_task_group());
}

bool Bt::ensure_playback_operation(const hal::bluetooth::PcmFormat &format)
{
    if (playback_operation_) {
        return true;
    }
    auto &registry = ServiceManager::get_instance().get_dataflow_registry();
    const auto outputs = registry.list_outputs(dataflow::Model::AudioPlayback);
    if (outputs.empty()) {
        publish_error("DataFlow", -1, "No AudioPlayback output is available");
        return false;
    }
    dataflow::AudioPlaybackOperationConfig config;
    config.owner = "service:Bt";
    config.source = {.name = "BluetoothA2dp", .role = "music", .preferred_outputs = {outputs.front().name}, .priority = 20};
    config.output_name = outputs.front().name;
    config.request_output = true;
    config.activate_source = true;
    config.open_stream = true;
    config.stream = {
        .type = dataflow::AudioCodecFormat::PCM,
        .general = {.channels = format.channels, .sample_bits = format.bits, .sample_rate = format.sample_rate},
    };
    auto result = registry.open_audio_playback_operation(std::move(config));
    if (!result) {
        publish_error("DataFlow", -1, result.error());
        return false;
    }
    playback_output_name_ = outputs.front().name;
    playback_operation_ = std::move(result.value());
    return true;
}

void Bt::close_playback_operation()
{
    if (playback_operation_) {
        playback_operation_->close();
        playback_operation_.reset();
    }
    playback_output_name_.clear();
}

void Bt::handle_pcm(PcmFrame frame)
{
    if (!ensure_playback_operation(frame.format)) {
        return;
    }
    const auto result = playback_operation_->write_copy(playback_output_name_, frame.data, 0);
    if (result != dataflow::AudioWriteResult::Written) {
        publish_error("DataFlowWrite", static_cast<int>(result), "PCM frame was not accepted");
        // A revoked/closed output must be remounted before the next frame. Keep
        // the operation for transient queue pressure, but drop it on terminal
        // route failures so the next frame can select a fresh output.
        if (result == dataflow::AudioWriteResult::DroppedNotActive ||
                result == dataflow::AudioWriteResult::Closed ||
                result == dataflow::AudioWriteResult::Error) {
            close_playback_operation();
        }
    }
}

void Bt::set_host_state(HostState state, bool publish)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        host_state_ = state;
    }
    if (publish) {
        auto result = function_get_state();
        if (result) {
            publish_event(
                BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::StateChanged),
                std::vector<EventItem> {result.value()}
            );
        }
    }
}

void Bt::publish_error(const std::string &operation, int code, const std::string &message)
{
    publish_event(
        BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::ErrorHappened),
    {operation, static_cast<double>(code), message}
    );
}

#if BROOKESIA_SERVICE_BT_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    ServiceBase, Bt, Bt::get_instance().get_attributes().name, Bt::get_instance(),
    BROOKESIA_SERVICE_BT_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::service::bt
