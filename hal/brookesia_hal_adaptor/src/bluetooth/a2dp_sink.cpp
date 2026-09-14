/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sdkconfig.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include "brookesia/hal_interface/interfaces/bluetooth/a2dp_sink.hpp"
#include "private/utils.hpp"
#include "host_coordinator.hpp"

#if CONFIG_BT_CONTROLLER_ENABLED && CONFIG_BT_CLASSIC_ENABLED && CONFIG_BT_BLUEDROID_ENABLED && CONFIG_BT_A2DP_ENABLE && \
    CONFIG_ESP_BT_AUDIO_GMF_IO_SUPPORT

#include "esp_bt.h"
#include "esp_log.h"
#include "esp_bt_audio.h"
#include "esp_bt_audio_classic.h"
#include "esp_bt_audio_defs.h"
#include "esp_bt_audio_event.h"
#include "esp_bt_audio_host.h"
#include "esp_bt_audio_playback.h"
#include "esp_bt_audio_stream.h"
#include "esp_bt_audio_vol.h"
#include "esp_codec_dev.h"
#include "esp_gmf_audio_dec.h"
#include "esp_gmf_bit_cvt.h"
#include "esp_gmf_ch_cvt.h"
#include "esp_gmf_err.h"
#include "esp_gmf_element.h"
#include "esp_gmf_io_bt.h"
#include "esp_gmf_io_codec_dev.h"
#include "esp_gmf_pipeline.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_rate_cvt.h"
#include "esp_gmf_task.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_simple_dec_default.h"
#include "esp_board_manager.h"
#include "esp_board_manager_defs.h"
#include "brookesia/hal_adaptor/board_manager.h"
#include "dev_audio_codec.h"

#if CONFIG_AUDIO_SIMPLE_PLAYER_RESAMPLE_DEST_RATE
#   define BROOKESIA_A2DP_OUTPUT_RATE CONFIG_AUDIO_SIMPLE_PLAYER_RESAMPLE_DEST_RATE
#else
#   define BROOKESIA_A2DP_OUTPUT_RATE 48000
#endif
#if CONFIG_AUDIO_SIMPLE_PLAYER_CH_CVT_DEST
#   define BROOKESIA_A2DP_OUTPUT_CHANNELS CONFIG_AUDIO_SIMPLE_PLAYER_CH_CVT_DEST
#else
#   define BROOKESIA_A2DP_OUTPUT_CHANNELS 2
#endif
#if CONFIG_AUDIO_SIMPLE_PLAYER_BIT_CVT_DEST_32BIT
#   define BROOKESIA_A2DP_OUTPUT_BITS 32
#elif CONFIG_AUDIO_SIMPLE_PLAYER_BIT_CVT_DEST_24BIT
#   define BROOKESIA_A2DP_OUTPUT_BITS 24
#else
#   define BROOKESIA_A2DP_OUTPUT_BITS 16
#endif

namespace esp_brookesia::hal {

namespace {

constexpr uint8_t BTA_ID_AV_PROFILE = 18;
constexpr uint8_t BTA_ID_AVK_PROFILE = 19;
constexpr uint16_t HCI_ENABLE_MASTER_SLAVE_SWITCH_POLICY = 0x0001;
constexpr uint16_t HCI_ENABLE_SNIFF_MODE_POLICY = 0x0004;
constexpr uint16_t DISABLED_A2DP_LINK_POLICY =
    HCI_ENABLE_MASTER_SLAVE_SWITCH_POLICY | HCI_ENABLE_SNIFF_MODE_POLICY;

extern "C" void bta_sys_clear_policy(uint8_t id, uint8_t policy, uint8_t *peer_addr);
extern "C" void bta_sys_clear_default_policy(uint8_t id, uint8_t policy);

void clear_default_a2dp_link_policy()
{
    bta_sys_clear_default_policy(BTA_ID_AV_PROFILE, DISABLED_A2DP_LINK_POLICY);
    bta_sys_clear_default_policy(BTA_ID_AVK_PROFILE, DISABLED_A2DP_LINK_POLICY);
}

void clear_peer_a2dp_link_policy(uint8_t *peer_addr)
{
    if (peer_addr == nullptr) {
        return;
    }
    bta_sys_clear_policy(BTA_ID_AV_PROFILE, DISABLED_A2DP_LINK_POLICY, peer_addr);
    bta_sys_clear_policy(BTA_ID_AVK_PROFILE, DISABLED_A2DP_LINK_POLICY, peer_addr);
}

class EspA2dpSink final: public bluetooth::A2dpSinkIface {
public:
    ~EspA2dpSink() override
    {
        deinit();
    }

    bool is_supported() const override
    {
        return true;
    }

    bool configure(const Config &config, Callbacks callbacks) override
    {
        std::lock_guard lock(mutex_);
        if (initialized_ || started_ || config.device.device_name.empty()) {
            return false;
        }
        config_ = config;
        callbacks_ = std::move(callbacks);
        return true;
    }

    void clear_callbacks() override
    {
        std::lock_guard lock(mutex_);
        callbacks_ = {};
    }

    bool init() override
    {
        std::lock_guard lock(mutex_);
        if (initialized_) {
            return true;
        }
        if (!init_output_locked()) {
            emit_error_locked("Audio output is not available");
            destroy_output_locked();
            return false;
        }
        host_token_ = bluetooth::detail::BluetoothHostCoordinator::get_instance().acquire(
                          bluetooth::detail::BluetoothHostCoordinator::Profile::Classic
                      );
        if (!init_controller_locked()) {
            host_token_.reset();
            destroy_output_locked();
            return false;
        }

        active_instance_ = this;
        esp_bt_audio_host_bluedroid_cfg_t host_cfg = ESP_BT_AUDIO_HOST_BLUEDROID_CFG_DEFAULT();
        std::snprintf(host_cfg.dev_name, sizeof(host_cfg.dev_name), "%s", config_.device.device_name.c_str());
        esp_bt_audio_config_t audio_config = {};
        audio_config.host_config = &host_cfg;
        audio_config.event_cb = &EspA2dpSink::on_bt_event;
        audio_config.event_user_ctx = this;
        audio_config.classic.roles = ESP_BT_AUDIO_CLASSIC_ROLE_A2DP_SNK;
#if CONFIG_BT_AVRCP_ENABLED
        audio_config.classic.roles |= ESP_BT_AUDIO_CLASSIC_ROLE_AVRC_CT;
#endif
        const esp_err_t result = esp_bt_audio_init(&audio_config);
        if (result != ESP_OK) {
            emit_error_locked("esp_bt_audio_init failed");
            active_instance_ = nullptr;
            deinit_controller_locked();
            host_token_.reset();
            destroy_output_locked();
            return false;
        }
        clear_default_a2dp_link_policy();
        audio_initialized_ = true;
        initialized_ = true;
        return true;
    }

    void deinit() override
    {
        std::lock_guard lock(mutex_);
        stop_locked();
        active_instance_ = nullptr;
        if (audio_initialized_) {
            esp_bt_audio_deinit();
            audio_initialized_ = false;
        }
        deinit_controller_locked();
        host_token_.reset();
        destroy_output_locked();
        initialized_ = false;
        stream_ = nullptr;
        peer_.reset();
    }

    bool start() override
    {
        std::lock_guard lock(mutex_);
        if (!initialized_) {
            return false;
        }
        if (started_) {
            return true;
        }
        if (esp_bt_audio_classic_set_scan_mode(config_.device.connectable, config_.device.discoverable) != ESP_OK) {
            return false;
        }
        const uint32_t mask = ESP_BT_AUDIO_PLAYBACK_EVENT_PLAY_STATUS_CHANGE |
                              ESP_BT_AUDIO_PLAYBACK_EVENT_TRACK_CHANGE |
                              ESP_BT_AUDIO_PLAYBACK_EVENT_PLAY_POS_CHANGED |
                              ESP_BT_AUDIO_PLAYBACK_EVENT_NOW_PLAYING_CHANGE;
        if (esp_bt_audio_playback_reg_notifications(mask) != ESP_OK) {
            emit_error_locked("Failed to register AVRCP notifications");
        }
        started_ = true;
        return true;
    }

    void stop() override
    {
        std::lock_guard lock(mutex_);
        stop_locked();
    }

    bool pause() override
    {
        return send_playback_command(esp_bt_audio_playback_pause);
    }
    bool resume() override
    {
        return send_playback_command(esp_bt_audio_playback_play);
    }
    bool next() override
    {
        return send_playback_command(esp_bt_audio_playback_next);
    }
    bool previous() override
    {
        return send_playback_command(esp_bt_audio_playback_prev);
    }

    bool set_volume(uint8_t volume) override
    {
        std::lock_guard lock(mutex_);
        if (!initialized_ || esp_bt_audio_vol_set_absolute(volume) != ESP_OK) {
            return false;
        }
        volume_ = volume;
        if (dac_ != nullptr) {
            (void)esp_codec_dev_set_out_vol(dac_, volume_);
        }
        return true;
    }

    uint8_t get_volume() const override
    {
        std::lock_guard lock(mutex_);
        return volume_;
    }

    std::optional<bluetooth::PeerInfo> get_connection() const override
    {
        std::lock_guard lock(mutex_);
        return peer_;
    }

    bool disconnect() override
    {
        std::lock_guard lock(mutex_);
        return peer_ && esp_bt_audio_classic_disconnect(
                   ESP_BT_AUDIO_CLASSIC_ROLE_A2DP_SNK, peer_address_.data()) == ESP_OK;
    }

private:
    static EspA2dpSink *active_instance_;

    static void on_bt_event(esp_bt_audio_event_t event, void *event_data, void *user_data)
    {
        auto *self = static_cast<EspA2dpSink *>(user_data);
        if (self == nullptr) {
            self = active_instance_;
        }
        if (self != nullptr) {
            self->handle_event(event, event_data);
        }
    }

    void emit_error_locked(const std::string &message)
    {
        auto callback = callbacks_.on_error;
        if (callback) {
            callback(message);
        }
    }

    bool send_playback_command(esp_err_t (*command)())
    {
        std::lock_guard lock(mutex_);
        return initialized_ && command() == ESP_OK;
    }

    bool init_output_locked()
    {
        dev_audio_codec_handles_t *handles = nullptr;
        if (brookesia_hal_board_manager_get_device_handle(
                    ESP_BOARD_DEVICE_NAME_AUDIO_DAC, reinterpret_cast<void **>(&handles)) != ESP_OK ||
                handles == nullptr || handles->codec_dev == nullptr) {
            return false;
        }
        dac_ = handles->codec_dev;
        if (esp_gmf_pool_init(&pool_) != ESP_GMF_ERR_OK) {
            return false;
        }
        esp_audio_dec_register_default();
        esp_audio_simple_dec_register_default();

        esp_gmf_element_handle_t element = nullptr;
        esp_audio_simple_dec_cfg_t decoder = DEFAULT_ESP_GMF_AUDIO_DEC_CONFIG();
        if (esp_gmf_audio_dec_init(&decoder, &element) != ESP_GMF_ERR_OK ||
                esp_gmf_pool_register_element(pool_, element, nullptr) != ESP_GMF_ERR_OK) {
            return false;
        }
        esp_ae_rate_cvt_cfg_t rate = DEFAULT_ESP_GMF_RATE_CVT_CONFIG();
        rate.complexity = 1;
        if (esp_gmf_rate_cvt_init(&rate, &element) != ESP_GMF_ERR_OK ||
                esp_gmf_pool_register_element(pool_, element, nullptr) != ESP_GMF_ERR_OK) {
            return false;
        }
        esp_ae_ch_cvt_cfg_t channel = DEFAULT_ESP_GMF_CH_CVT_CONFIG();
        if (esp_gmf_ch_cvt_init(&channel, &element) != ESP_GMF_ERR_OK ||
                esp_gmf_pool_register_element(pool_, element, nullptr) != ESP_GMF_ERR_OK) {
            return false;
        }
        esp_ae_bit_cvt_cfg_t bits = DEFAULT_ESP_GMF_BIT_CVT_CONFIG();
        if (esp_gmf_bit_cvt_init(&bits, &element) != ESP_GMF_ERR_OK ||
                esp_gmf_pool_register_element(pool_, element, nullptr) != ESP_GMF_ERR_OK) {
            return false;
        }

        esp_gmf_io_handle_t io = nullptr;
        codec_dev_io_cfg_t codec = ESP_GMF_IO_CODEC_DEV_CFG_DEFAULT();
        codec.dir = ESP_GMF_IO_DIR_WRITER;
        codec.dev = dac_;
        if (esp_gmf_io_codec_dev_init(&codec, &io) != ESP_GMF_ERR_OK ||
                esp_gmf_pool_register_io(pool_, io, nullptr) != ESP_GMF_ERR_OK) {
            return false;
        }
        bt_io_cfg_t bt = ESP_GMF_BT_IO_CFG_DEFAULT();
        bt.dir = ESP_GMF_IO_DIR_READER;
        if (esp_gmf_io_bt_init(&bt, &io) != ESP_GMF_ERR_OK ||
                esp_gmf_pool_register_io(pool_, io, nullptr) != ESP_GMF_ERR_OK) {
            return false;
        }
        const char *elements[] = {"aud_dec", "aud_rate_cvt", "aud_ch_cvt", "aud_bit_cvt"};
        if (esp_gmf_pool_new_pipeline(
                    pool_, "io_bt", elements, sizeof(elements) / sizeof(elements[0]), "io_codec_dev", &pipeline_) !=
                ESP_GMF_ERR_OK) {
            return false;
        }
        esp_gmf_task_cfg_t task = DEFAULT_ESP_GMF_TASK_CONFIG();
        task.thread.stack = 8192;
        task.thread.prio = 4;
        task.thread.core = 0;
        task.thread.stack_in_ext = false;
        task.name = "bt2codec_task";
        if (esp_gmf_task_init(&task, &task_) != ESP_GMF_ERR_OK ||
                esp_gmf_pipeline_bind_task(pipeline_, task_) != ESP_GMF_ERR_OK) {
            return false;
        }
        return true;
    }

    bool init_controller_locked()
    {
        const esp_bt_mode_t requested_mode =
#if CONFIG_BT_BLE_ENABLED
            ESP_BT_MODE_BTDM;
#else
            ESP_BT_MODE_CLASSIC_BT;
#endif
        const auto status = esp_bt_controller_get_status();
        if (status == ESP_BT_CONTROLLER_STATUS_ENABLED) {
            return true;
        }
        if (status == ESP_BT_CONTROLLER_STATUS_INITED) {
            if (esp_bt_controller_enable(requested_mode) != ESP_OK) {
                return false;
            }
            controller_enabled_ = true;
            return true;
        }
#if !CONFIG_BT_BLE_ENABLED
        (void)esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
        esp_bt_controller_config_t controller = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
#pragma GCC diagnostic pop
        if (esp_bt_controller_init(&controller) != ESP_OK) {
            return false;
        }
        controller_initialized_ = true;
        if (esp_bt_controller_enable(requested_mode) != ESP_OK) {
            (void)esp_bt_controller_deinit();
            controller_initialized_ = false;
            return false;
        }
        controller_enabled_ = true;
        return true;
    }

    void deinit_controller_locked()
    {
        if (controller_initialized_) {
            if (controller_enabled_) {
                (void)esp_bt_controller_disable();
            }
            (void)esp_bt_controller_deinit();
        }
        controller_enabled_ = false;
        controller_initialized_ = false;
    }

    void stop_pipeline_locked()
    {
        if (pipeline_ == nullptr || !pipeline_running_) {
            return;
        }
        pipeline_running_ = false;
        (void)esp_gmf_pipeline_stop(pipeline_);
        (void)esp_gmf_pipeline_reset(pipeline_);
    }

    void stop_locked()
    {
        if (started_) {
            (void)esp_bt_audio_classic_set_scan_mode(false, false);
            started_ = false;
        }
        stop_pipeline_locked();
    }

    void destroy_output_locked()
    {
        stop_pipeline_locked();
        if (task_ != nullptr) {
            (void)esp_gmf_task_deinit(task_);
            task_ = nullptr;
        }
        if (pipeline_ != nullptr) {
            (void)esp_gmf_pipeline_destroy(pipeline_);
            pipeline_ = nullptr;
        }
        if (pool_ != nullptr) {
            (void)esp_gmf_pool_deinit(pool_);
            pool_ = nullptr;
        }
        dac_ = nullptr;
    }

    void prepare_stream_locked(esp_bt_audio_stream_handle_t stream)
    {
        if (pipeline_ == nullptr) {
            emit_error_locked("A2DP decoder pipeline is unavailable");
            return;
        }
        esp_bt_audio_stream_codec_info_t info = {};
        if (esp_bt_audio_stream_get_codec_info(stream, &info) != ESP_OK) {
            emit_error_locked("Failed to query A2DP codec");
            return;
        }
        auto *input = ESP_GMF_PIPELINE_GET_IN_INSTANCE(pipeline_);
        const auto set_stream_result = esp_gmf_io_bt_set_stream(input, stream);
        if (set_stream_result != ESP_GMF_ERR_OK) {
            emit_error_locked("Failed to attach A2DP stream");
            return;
        }
        esp_audio_simple_dec_cfg_t decoder = {};
        decoder.dec_type = static_cast<esp_audio_simple_dec_type_t>(
                               info.codec_type == ESP_BT_AUDIO_STREAM_CODEC_SBC ? ESP_AUDIO_TYPE_SBC : ESP_AUDIO_TYPE_LC3);
        decoder.dec_cfg = info.codec_cfg;
        decoder.cfg_size = info.cfg_size;
        (void)esp_gmf_audio_dec_reconfig(ESP_GMF_PIPELINE_GET_FIRST_ELEMENT(pipeline_), &decoder);
        esp_gmf_element_handle_t element = nullptr;
        if (esp_gmf_pipeline_get_el_by_name(pipeline_, "aud_rate_cvt", &element) == ESP_GMF_ERR_OK) {
            (void)esp_gmf_rate_cvt_set_dest_rate(element, BROOKESIA_A2DP_OUTPUT_RATE);
        }
        if (esp_gmf_pipeline_get_el_by_name(pipeline_, "aud_ch_cvt", &element) == ESP_GMF_ERR_OK) {
            (void)esp_gmf_ch_cvt_set_dest_channel(element, BROOKESIA_A2DP_OUTPUT_CHANNELS);
        }
        if (esp_gmf_pipeline_get_el_by_name(pipeline_, "aud_bit_cvt", &element) == ESP_GMF_ERR_OK) {
            (void)esp_gmf_bit_cvt_set_dest_bits(element, BROOKESIA_A2DP_OUTPUT_BITS);
        }
        (void)esp_gmf_pipeline_loading_jobs(pipeline_);
        stream_ = stream;
    }

    void handle_event(esp_bt_audio_event_t event, void *event_data)
    {
        std::lock_guard lock(mutex_);
        if (!initialized_ || event_data == nullptr) {
            return;
        }
        switch (event) {
        case ESP_BT_AUDIO_EVENT_CONNECTION_STATE_CHG: {
            auto *connection = static_cast<esp_bt_audio_event_connection_st_t *>(event_data);
            if (connection->connected) {
                std::copy_n(connection->addr, peer_address_.size(), peer_address_.begin());
                clear_peer_a2dp_link_policy(peer_address_.data());
                peer_ = bluetooth::PeerInfo{1, format_address(connection->addr), {}};
                metadata_ = {};
                notify_connection_locked(bluetooth::ConnectionState::Connected);
            } else {
                stop_pipeline_locked();
                stream_ = nullptr;
                notify_connection_locked(bluetooth::ConnectionState::Disconnected);
                peer_.reset();
            }
            break;
        }
        case ESP_BT_AUDIO_EVENT_STREAM_STATE_CHG: {
            auto *stream = static_cast<esp_bt_audio_event_stream_st_t *>(event_data);
            esp_bt_audio_stream_dir_t direction = ESP_BT_AUDIO_STREAM_DIR_UNKNOWN;
            if (esp_bt_audio_stream_get_dir(stream->stream_handle, &direction) != ESP_OK ||
                    direction != ESP_BT_AUDIO_STREAM_DIR_SINK) {
                break;
            }
            switch (stream->state) {
            case ESP_BT_AUDIO_STREAM_STATE_ALLOCATED:
                prepare_stream_locked(stream->stream_handle);
                break;
            case ESP_BT_AUDIO_STREAM_STATE_STARTED:
                clear_peer_a2dp_link_policy(peer_address_.data());
                if (pipeline_ != nullptr) {
                    if (stream_ != nullptr) {
                        (void)esp_gmf_io_bt_set_stream(
                            ESP_GMF_PIPELINE_GET_IN_INSTANCE(pipeline_), stream_);
                        (void)esp_gmf_pipeline_loading_jobs(pipeline_);
                    }
                    const auto run_result = esp_gmf_pipeline_run(pipeline_);
                    pipeline_running_ = run_result == ESP_GMF_ERR_OK;
                }
                (void)esp_bt_audio_playback_request_metadata(
                    ESP_BT_AUDIO_PLAYBACK_METADATA_TITLE |
                    ESP_BT_AUDIO_PLAYBACK_METADATA_ARTIST |
                    ESP_BT_AUDIO_PLAYBACK_METADATA_ALBUM |
                    ESP_BT_AUDIO_PLAYBACK_METADATA_PLAYING_TIME
                );
                notify_stream_locked(bluetooth::StreamState::Started);
                break;
            case ESP_BT_AUDIO_STREAM_STATE_STOPPED:
                // Bluedroid re-enables Sniff/role-switch policy while handling
                // stream suspension, so clear it again after that transition.
                clear_peer_a2dp_link_policy(peer_address_.data());
                stop_pipeline_locked();
                notify_stream_locked(bluetooth::StreamState::Stopped);
                break;
            case ESP_BT_AUDIO_STREAM_STATE_RELEASED:
                clear_peer_a2dp_link_policy(peer_address_.data());
                stream_ = nullptr;
                notify_stream_locked(bluetooth::StreamState::Idle);
                break;
            }
            break;
        }
        case ESP_BT_AUDIO_EVENT_PLAYBACK_STATUS_CHG: {
            auto *status = static_cast<esp_bt_audio_event_playback_st_t *>(event_data);
            if (status->event == ESP_BT_AUDIO_PLAYBACK_EVENT_PLAY_STATUS_CHANGE) {
                notify_playback_locked(status->evt_param.play_status);
            }
            break;
        }
        case ESP_BT_AUDIO_EVENT_PLAYBACK_METADATA: {
            auto *metadata = static_cast<esp_bt_audio_event_playback_metadata_t *>(event_data);
            if (metadata->value == nullptr || metadata->length == 0) {
                break;
            }
            std::string value(reinterpret_cast<const char *>(metadata->value), metadata->length);
            while (!value.empty() && value.back() == '\0') {
                value.pop_back();
            }
            switch (metadata->type) {
            case ESP_BT_AUDIO_PLAYBACK_METADATA_TITLE: metadata_.title = value; break;
            case ESP_BT_AUDIO_PLAYBACK_METADATA_ARTIST: metadata_.artist = value; break;
            case ESP_BT_AUDIO_PLAYBACK_METADATA_ALBUM: metadata_.album = value; break;
            case ESP_BT_AUDIO_PLAYBACK_METADATA_PLAYING_TIME:
                metadata_.duration_ms = static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
                break;
            default: return;
            }
            auto callback = callbacks_.on_metadata_changed;
            if (callback) {
                callback(metadata_);
            }
            break;
        }
        case ESP_BT_AUDIO_EVENT_VOL_ABSOLUTE: {
            auto *volume = static_cast<esp_bt_audio_event_vol_absolute_t *>(event_data);
            volume_ = volume->mute ? 0 : volume->vol;
            if (dac_ != nullptr) {
                (void)esp_codec_dev_set_out_vol(dac_, volume_);
            }
            auto callback = callbacks_.on_volume_changed;
            if (callback) {
                callback(volume_);
            }
            break;
        }
        default:
            break;
        }
    }

    static std::string format_address(const uint8_t *address)
    {
        char value[18] = {};
        std::snprintf(value, sizeof(value), "%02X:%02X:%02X:%02X:%02X:%02X",
                      address[0], address[1], address[2], address[3], address[4], address[5]);
        return value;
    }

    void notify_connection_locked(bluetooth::ConnectionState state)
    {
        auto callback = callbacks_.on_connection_changed;
        if (callback && peer_) {
            callback(*peer_, state);
        }
    }

    void notify_stream_locked(bluetooth::StreamState state)
    {
        auto callback = callbacks_.on_stream_changed;
        if (callback) {
            callback(state);
        }
    }

    void notify_playback_locked(uint32_t status)
    {
        bluetooth::PlaybackStatus mapped = bluetooth::PlaybackStatus::Unknown;
        switch (status) {
        case ESP_BT_AUDIO_PLAYBACK_STATUS_STOPPED: mapped = bluetooth::PlaybackStatus::Stopped; break;
        case ESP_BT_AUDIO_PLAYBACK_STATUS_PLAYING: mapped = bluetooth::PlaybackStatus::Playing; break;
        case ESP_BT_AUDIO_PLAYBACK_STATUS_PAUSED: mapped = bluetooth::PlaybackStatus::Paused; break;
        default: break;
        }
        auto callback = callbacks_.on_playback_status_changed;
        if (callback) {
            callback(mapped);
        }
    }

    mutable std::mutex mutex_;
    Config config_;
    Callbacks callbacks_;
    std::optional<bluetooth::PeerInfo> peer_;
    std::array<uint8_t, 6> peer_address_ = {};
    bluetooth::TrackMetadata metadata_;
    uint8_t volume_ = 100;
    esp_bt_audio_stream_handle_t stream_ = nullptr;
    esp_codec_dev_handle_t dac_ = nullptr;
    esp_gmf_pool_handle_t pool_ = nullptr;
    esp_gmf_pipeline_handle_t pipeline_ = nullptr;
    esp_gmf_task_handle_t task_ = nullptr;
    bool pipeline_running_ = false;
    bluetooth::detail::BluetoothHostCoordinator::Token host_token_;
    bool initialized_ = false;
    bool started_ = false;
    bool audio_initialized_ = false;
    bool controller_initialized_ = false;
    bool controller_enabled_ = false;
};

EspA2dpSink *EspA2dpSink::active_instance_ = nullptr;

} // namespace

std::shared_ptr<bluetooth::A2dpSinkIface> create_a2dp_sink_iface()
{
    return std::make_shared<EspA2dpSink>();
}

} // namespace esp_brookesia::hal

#else

namespace esp_brookesia::hal {

namespace {
class UnavailableA2dpSink final: public bluetooth::A2dpSinkIface {
public:
    bool is_supported() const override
    {
        return false;
    }
    bool configure(const Config &, Callbacks callbacks) override
    {
        callbacks_ = std::move(callbacks);
        return true;
    }
    void clear_callbacks() override
    {
        callbacks_ = {};
    }
    bool init() override
    {
        return true;
    }
    void deinit() override {}
    bool start() override
    {
        return false;
    }
    void stop() override {}
    bool pause() override
    {
        return false;
    }
    bool resume() override
    {
        return false;
    }
    bool next() override
    {
        return false;
    }
    bool previous() override
    {
        return false;
    }
    bool set_volume(uint8_t) override
    {
        return false;
    }
    uint8_t get_volume() const override
    {
        return 100;
    }
    std::optional<bluetooth::PeerInfo> get_connection() const override
    {
        return std::nullopt;
    }
    bool disconnect() override
    {
        return false;
    }
private:
    Callbacks callbacks_;
};
} // namespace

std::shared_ptr<bluetooth::A2dpSinkIface> create_a2dp_sink_iface()
{
    return std::make_shared<UnavailableA2dpSink>();
}

} // namespace esp_brookesia::hal

#endif
