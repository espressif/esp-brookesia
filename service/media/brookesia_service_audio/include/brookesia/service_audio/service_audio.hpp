/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "brookesia/lib_utils/signal.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_player.hpp"
#include "brookesia/hal_interface/interfaces/audio/processor.hpp"
#include "brookesia/service_helper/media/audio.hpp"
#include "brookesia/service_helper/system/storage.hpp"
#include "brookesia/service_manager/macro_configs.h"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_audio/macro_configs.h"

namespace esp_brookesia::service {

using AudioCodecFormat = helper::Audio::CodecFormat;
using AudioCodecGeneralConfig = helper::Audio::CodecGeneralConfig;
using AudioEncoderExtraConfigOpus = helper::Audio::EncoderExtraConfigOpus;
using AudioEncoderDynamicConfig = helper::Audio::EncoderDynamicConfig;
using AudioDecoderDynamicConfig = helper::Audio::DecoderDynamicConfig;
using AudioAFE_Event = helper::Audio::AFE_Event;
using AudioPlayState = helper::Audio::PlayState;
using AudioPlayUrlConfig = helper::Audio::PlayUrlConfig;
using AudioOutputInfo = helper::Audio::OutputInfo;
using AudioSourceInfo = helper::Audio::SourceInfo;
using AudioSourceState = helper::Audio::SourceState;
using AudioStreamConfig = helper::Audio::StreamConfig;
using AudioWriteResult = helper::Audio::WriteResult;

class AudioPlayback : public ServiceBase {
public:
    static AudioPlayback &get_instance()
    {
        static AudioPlayback instance;
        return instance;
    }

private:
    using Helper = helper::AudioPlayback;
    using StorageHelper = helper::Storage;
    enum class PlaylistPhase : uint8_t {
        Inactive,
        WaitingToStart,
        StartingItem,
        PlayingCurrentItem,
    };
    struct PlaybackRequest {
        std::vector<std::string> urls;
        AudioPlayUrlConfig config;
    };

    AudioPlayback()
        : ServiceBase({
        .name = Helper::get_name().data(),
        .dependencies = {
            StorageHelper::get_name().data(),
        },
#if BROOKESIA_SERVICE_AUDIO_ENABLE_WORKER
        .task_scheduler_config = lib_utils::TaskScheduler::StartConfig{
            .worker_configs = {
                lib_utils::ThreadConfig{
                    .name = BROOKESIA_SERVICE_AUDIO_WORKER_NAME,
                    .core_id = BROOKESIA_SERVICE_AUDIO_WORKER_CORE_ID,
                    .priority = BROOKESIA_SERVICE_AUDIO_WORKER_PRIORITY,
                    .stack_size = BROOKESIA_SERVICE_AUDIO_WORKER_STACK_SIZE,
                    .stack_in_ext = BROOKESIA_SERVICE_AUDIO_WORKER_STACK_IN_EXT,
                }
            },
            .worker_poll_interval_ms = BROOKESIA_SERVICE_AUDIO_WORKER_POLL_INTERVAL_MS,
        }
#endif
    })
    {}
    ~AudioPlayback() = default;

    bool on_init() override;
    bool on_start() override;
    void on_stop() override;

    std::expected<void, std::string> function_play(const std::string &url, const boost::json::object &config);
    std::expected<void, std::string> function_play_list(
        const boost::json::array &urls, const boost::json::object &config
    );
    std::expected<void, std::string> function_pause();
    std::expected<void, std::string> function_resume();
    std::expected<void, std::string> function_stop();
    std::expected<void, std::string> submit_playback_request(PlaybackRequest request);
    std::expected<void, std::string> submit_interrupt_playback_request(PlaybackRequest request);
    std::expected<void, std::string> function_set_volume(double volume);
    std::expected<double, std::string> function_get_volume();
    std::expected<void, std::string> function_set_mute(bool enable);
    std::expected<bool, std::string> function_get_mute();
    std::expected<void, std::string> function_load_data();
    std::expected<void, std::string> function_reset_data();

    std::vector<FunctionSchema> get_function_schemas() override
    {
        auto function_schemas = Helper::get_function_schemas();
        return std::vector<FunctionSchema>(function_schemas.begin(), function_schemas.end());
    }

    std::vector<EventSchema> get_event_schemas() override
    {
        auto event_schemas = Helper::get_event_schemas();
        return std::vector<EventSchema>(event_schemas.begin(), event_schemas.end());
    }

    FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::Play, std::string, boost::json::object,
                function_play(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::PlayUrls, boost::json::array, boost::json::object,
                function_play_list(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::Pause, function_pause()),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::Resume, function_resume()),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::Stop, function_stop()),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetVolume, double,
                function_set_volume(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::GetVolume, function_get_volume()),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetMute, bool,
                function_set_mute(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::GetMute, function_get_mute()),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::LoadData, function_load_data()),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::ResetData, function_reset_data()),
        };
    }

    void on_playback_event(AudioPlayState state);
    void process_playback_queue();
    void start_playlist(
        std::vector<std::string> urls, const AudioPlayUrlConfig &config, bool allow_gap_idle_before_start
    );
    void clear_pending_interrupt_playback();
    void start_pending_interrupt_playback_after_idle();
    void play_playlist_url_at_index(size_t index);
    void advance_playlist();

    void cancel_playlist_scheduled_task();
    void suspend_playlist_scheduled_task();
    void resume_playlist_scheduled_task();

    void initialize_control_default_state();
    void load_control_data_from_storage();
    void save_volume_data();
    void save_mute_data();
    void reset_control_data(uint8_t old_volume, bool old_mute);
    bool ensure_player_iface();
    bool apply_control_state_to_hal();
    bool publish_volume_changed();
    bool publish_mute_changed();
    static std::expected<uint8_t, std::string> validate_percentage(double value, const char *name);
    static uint8_t map_percentage_to_hardware(uint8_t percentage, uint8_t min, uint8_t max);

    hal::InterfaceHandle<hal::audio::PlaybackIface> playback_iface_;
    hal::InterfaceHandle<hal::audio::CodecPlayerIface> player_iface_;
    uint8_t desired_volume_ = 0;
    bool desired_mute_ = false;
    bool control_data_loaded_ = false;

    AudioPlayState play_state_ = AudioPlayState::Idle;
    std::queue<PlaybackRequest> playback_queue_;
    bool is_processing_queue_ = false;
    bool pause_requested_ = false;
    bool stop_for_pending_interrupt_ = false;
    std::optional<PlaybackRequest> pending_interrupt_request_;

    struct PlaylistState {
        std::vector<std::string> urls;
        AudioPlayUrlConfig config;
        size_t current_url_index = 0;
        uint32_t current_loop = 0;
        uint32_t total_loops = 0;
        uint32_t session_id = 0;
        PlaylistPhase phase = PlaylistPhase::Inactive;
        bool allow_gap_idle_before_start = false;
        bool gap_idle_published_before_start = false;
        int64_t start_time_ms = 0;
        int64_t pause_start_ms = 0;
        int64_t paused_duration_ms = 0;
        lib_utils::TaskScheduler::TaskId scheduled_task_id = 0;
    };
    PlaylistState playlist_state_;
    std::mutex playlist_state_mutex_;
    uint32_t playlist_session_counter_ = 0;
};

class AudioEncoder : public ServiceBase {
public:
    explicit AudioEncoder(int id)
        : ServiceBase({
        .name = std::string(helper::Audio::ENCODER_NAME_PREFIX) + std::to_string(id),
#if BROOKESIA_SERVICE_AUDIO_ENABLE_WORKER
        .task_scheduler_config = lib_utils::TaskScheduler::StartConfig{
            .worker_configs = {
                lib_utils::ThreadConfig{
                    .name = BROOKESIA_SERVICE_AUDIO_WORKER_NAME,
                    .core_id = BROOKESIA_SERVICE_AUDIO_WORKER_CORE_ID,
                    .priority = BROOKESIA_SERVICE_AUDIO_WORKER_PRIORITY,
                    .stack_size = BROOKESIA_SERVICE_AUDIO_WORKER_STACK_SIZE,
                    .stack_in_ext = BROOKESIA_SERVICE_AUDIO_WORKER_STACK_IN_EXT,
                }
            },
            .worker_poll_interval_ms = BROOKESIA_SERVICE_AUDIO_WORKER_POLL_INTERVAL_MS,
        }
#endif
    })
    , id_(id)
    {}
    ~AudioEncoder() = default;

    using DataSignal = esp_brookesia::lib_utils::signal<void(const RawBuffer &data)>;

    static AudioEncoder *get_instance(int id);

    esp_brookesia::lib_utils::connection connect_encoded_data(const DataSignal::slot_type &slot)
    {
        return encoded_data_signal_.connect(slot);
    }

    esp_brookesia::lib_utils::connection connect_recorder_data(const DataSignal::slot_type &slot)
    {
        return recorder_data_signal_.connect(slot);
    }

private:
    using BaseHelper = helper::Audio;
    using Helper = helper::AudioEncoder<0>;

    bool on_init() override;
    bool on_start() override;
    void on_stop() override;

    std::expected<void, std::string> function_start(const boost::json::object &config);
    std::expected<void, std::string> function_stop();
    std::expected<void, std::string> function_pause();
    std::expected<void, std::string> function_resume();
    std::expected<void, std::string> function_pause_wake_end();
    std::expected<void, std::string> function_resume_wake_end();
    std::expected<boost::json::array, std::string> function_get_afe_wake_words();

    std::vector<FunctionSchema> get_function_schemas() override
    {
        auto function_schemas = Helper::get_function_schemas();
        return std::vector<FunctionSchema>(function_schemas.begin(), function_schemas.end());
    }

    std::vector<EventSchema> get_event_schemas() override
    {
        auto event_schemas = Helper::get_event_schemas();
        return std::vector<EventSchema>(event_schemas.begin(), event_schemas.end());
    }

    FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::Start, boost::json::object,
                function_start(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::Stop, function_stop()),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::Pause, function_pause()),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::Resume, function_resume()),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::PauseWakeEnd,
                function_pause_wake_end()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::ResumeWakeEnd,
                function_resume_wake_end()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetAFEWakeWords,
                function_get_afe_wake_words()
            ),
        };
    }

    bool start_encoder(const AudioEncoderDynamicConfig &config);
    void stop_encoder();
    bool start_encoder_fetch_task(const AudioEncoderDynamicConfig &config);
    void stop_encoder_fetch_task();
    bool encoder_fetch_task(uint32_t fetch_data_size, uint32_t fetch_interval_ms);
    void on_afe_event(AudioAFE_Event event);
    void on_recorder_input_data(const uint8_t *data, size_t size);
    void publish_afe_event(AudioAFE_Event event);
    void schedule_wake_end(uint32_t timeout_ms);
    bool schedule_wake_end_locked(uint32_t timeout_ms);
    void cancel_wake_end();
    void cancel_wake_end_locked();
    void on_wake_end_timeout(uint32_t session_id);

    int id_ = 0;
    hal::InterfaceHandle<hal::audio::EncoderIface> encoder_iface_;
    lib_utils::TaskScheduler::TaskId encoder_fetch_task_id_ = 0;
    AudioEncoderDynamicConfig encoder_config_{};
    std::mutex encoder_state_mutex_;
    std::vector<uint8_t> encoder_fetch_buffer_;
    DataSignal encoded_data_signal_;
    DataSignal recorder_data_signal_;
    lib_utils::TaskScheduler::TaskId wake_end_task_id_ = 0;
    int64_t wake_end_deadline_ms_ = 0;
    int64_t wake_end_remaining_ms_ = 0;
    uint32_t wake_end_session_id_ = 0;
    bool wake_end_paused_ = false;
    bool wake_end_pending_ = false;
};

class AudioDecoder : public ServiceBase {
public:
    explicit AudioDecoder(int id);
    ~AudioDecoder() override;

    using SourceStateChangedSignal = esp_brookesia::lib_utils::signal<void(
                                         const std::string &source_name, const std::string &output_name, AudioSourceState state
                                     )>;
    using ActiveSourceChangedSignal = esp_brookesia::lib_utils::signal<void(
                                          const std::string &output_name, const std::string &source_name
                                      )>;
    using StreamDrainedSignal = esp_brookesia::lib_utils::signal<void(
                                    const std::string &source_name, const std::string &output_name
                                )>;
    using StreamBufferReleaseCallback = std::function<void(AudioWriteResult result)>;

    static AudioDecoder *get_instance(int id);

    std::vector<AudioOutputInfo> get_outputs() const;
    std::vector<AudioSourceInfo> get_sources() const;
    std::expected<uint32_t, std::string> register_source(AudioSourceInfo source);
    std::expected<void, std::string> unregister_source(uint32_t source_id);
    std::expected<void, std::string> unregister_source(std::string_view source_name);
    std::expected<void, std::string> request_output(uint32_t source_id, std::string_view output_name);
    std::expected<void, std::string> request_output(std::string_view source_name, std::string_view output_name);
    std::expected<void, std::string> release_output(uint32_t source_id, std::string_view output_name);
    std::expected<void, std::string> release_output(std::string_view source_name, std::string_view output_name);
    std::expected<void, std::string> set_active_source(std::string_view output_name, std::string_view source_name);
    std::expected<std::string, std::string> get_active_source(std::string_view output_name) const;
    std::expected<void, std::string> open_stream(
        uint32_t source_id, std::string_view output_name, const AudioStreamConfig &config
    );
    std::expected<void, std::string> open_stream(
        std::string_view source_name, std::string_view output_name, const AudioStreamConfig &config
    );
    std::expected<void, std::string> close_stream(uint32_t source_id, std::string_view output_name);
    std::expected<void, std::string> close_stream(std::string_view source_name, std::string_view output_name);
    AudioWriteResult write_stream(
        uint32_t source_id, std::string_view output_name, const RawBuffer &data, uint32_t timeout_ms
    );
    AudioWriteResult write_stream(
        std::string_view source_name, std::string_view output_name, const RawBuffer &data, uint32_t timeout_ms
    );
    AudioWriteResult write_stream_borrowed(
        uint32_t source_id, std::string_view output_name, const RawBuffer &data,
        StreamBufferReleaseCallback release_callback, uint32_t timeout_ms
    );
    AudioWriteResult write_stream_borrowed(
        std::string_view source_name, std::string_view output_name, const RawBuffer &data,
        StreamBufferReleaseCallback release_callback, uint32_t timeout_ms
    );
    bool is_stream_drained(uint32_t source_id, std::string_view output_name) const;
    bool is_stream_drained(std::string_view source_name, std::string_view output_name) const;

    esp_brookesia::lib_utils::connection connect_source_state_changed(const SourceStateChangedSignal::slot_type &slot)
    {
        return source_state_changed_signal_.connect(slot);
    }

    esp_brookesia::lib_utils::connection connect_active_source_changed(const ActiveSourceChangedSignal::slot_type &slot)
    {
        return active_source_changed_signal_.connect(slot);
    }

    esp_brookesia::lib_utils::connection connect_stream_drained(const StreamDrainedSignal::slot_type &slot)
    {
        return stream_drained_signal_.connect(slot);
    }

private:
    using BaseHelper = helper::Audio;
    using Helper = helper::AudioDecoder<0>;

    bool on_init() override;
    bool on_start() override;
    void on_stop() override;

    std::expected<boost::json::array, std::string> function_get_outputs();
    std::expected<boost::json::array, std::string> function_get_sources();
    std::expected<double, std::string> function_register_source(const boost::json::object &source_json);
    std::expected<void, std::string> function_unregister_source(const std::string &source_name);
    std::expected<void, std::string> function_request_output(
        const std::string &source_name, const std::string &output_name
    );
    std::expected<void, std::string> function_release_output(
        const std::string &source_name, const std::string &output_name
    );
    std::expected<void, std::string> function_set_active_source(
        const std::string &output_name, const std::string &source_name
    );
    std::expected<std::string, std::string> function_get_active_source(const std::string &output_name);
    std::expected<void, std::string> function_open_stream(
        const std::string &source_name, const std::string &output_name, const boost::json::object &config
    );
    std::expected<void, std::string> function_close_stream(
        const std::string &source_name, const std::string &output_name
    );

    std::vector<FunctionSchema> get_function_schemas() override
    {
        auto function_schemas = Helper::get_function_schemas();
        return std::vector<FunctionSchema>(function_schemas.begin(), function_schemas.end());
    }

    std::vector<EventSchema> get_event_schemas() override
    {
        auto event_schemas = Helper::get_event_schemas();
        return std::vector<EventSchema>(event_schemas.begin(), event_schemas.end());
    }

    FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::GetOutputs, function_get_outputs()),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::GetSources, function_get_sources()),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::RegisterSource, boost::json::object,
                function_register_source(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::UnregisterSource, std::string,
                function_unregister_source(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::RequestOutput, std::string, std::string,
                function_request_output(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::ReleaseOutput, std::string, std::string,
                function_release_output(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::SetActiveSource, std::string, std::string,
                function_set_active_source(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::GetActiveSource, std::string,
                function_get_active_source(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_3(
                Helper, Helper::FunctionId::OpenStream, std::string, std::string, boost::json::object,
                function_open_stream(PARAM1, PARAM2, PARAM3)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::CloseStream, std::string, std::string,
                function_close_stream(PARAM1, PARAM2)
            ),
        };
    }

    struct StreamContext {
        AudioStreamConfig config = {};
        struct Chunk {
            std::vector<uint8_t> owned_data;
            RawBuffer borrowed_data = {};
            StreamBufferReleaseCallback release_callback;
            bool borrowed = false;

            size_t size() const
            {
                return borrowed ? borrowed_data.data_size : owned_data.size();
            }

            const uint8_t *data() const
            {
                return borrowed ? borrowed_data.data_ptr : owned_data.data();
            }

            void release(AudioWriteResult result)
            {
                auto callback = std::move(release_callback);
                release_callback = nullptr;
                if (borrowed && callback) {
                    callback(result);
                }
            }
        };
        std::deque<Chunk> queue;
        size_t queued_bytes = 0;
        bool opened = false;
    };

    struct SourceContext {
        AudioSourceInfo info;
        std::unordered_set<std::string> requested_outputs;
        std::unordered_map<std::string, StreamContext> streams;
    };

    struct OutputContext {
        AudioOutputInfo info;
        uint32_t active_source_id = 0;
    };

    SourceContext *find_source_by_id_locked(uint32_t source_id);
    const SourceContext *find_source_by_id_locked(uint32_t source_id) const;
    SourceContext *find_source_by_name_locked(std::string_view source_name);
    const SourceContext *find_source_by_name_locked(std::string_view source_name) const;
    OutputContext *find_output_locked(std::string_view output_name);
    const OutputContext *find_output_locked(std::string_view output_name) const;
    bool is_source_active_locked(uint32_t source_id, std::string_view output_name) const;
    AudioWriteResult reserve_stream_queue_space_locked(
        std::unique_lock<std::mutex> &lock, uint32_t source_id, std::string_view output_name, size_t data_size,
        uint32_t timeout_ms, StreamContext *&stream
    );
    AudioWriteResult enqueue_stream_chunk_locked(
        uint32_t source_id, std::string_view output_name, StreamContext::Chunk &&chunk, uint32_t timeout_ms
    );
    bool ensure_hal_decoder_for_active_stream_locked(SourceContext &source, const std::string &output_name);
    void clear_stream_queue_locked(StreamContext &stream);
    void clear_all_queues_locked();
    void stop_hal_decoder_locked();
    bool start_decoder_stream_task_locked();
    void stop_decoder_stream_task();
    bool decoder_stream_task();
    void emit_source_state_changed(
        const std::string &source_name, const std::string &output_name, AudioSourceState state
    );
    void emit_active_source_changed(const std::string &output_name, const std::string &source_name);
    void emit_stream_drained(const std::string &source_name, const std::string &output_name);

    int id_ = 0;
    mutable std::mutex decoder_state_mutex_;
    std::mutex decoder_hal_mutex_;
    std::condition_variable decoder_queue_cv_;
    hal::InterfaceHandle<hal::audio::DecoderIface> decoder_iface_;
    std::vector<OutputContext> outputs_;
    std::unordered_map<uint32_t, SourceContext> sources_;
    uint32_t next_source_id_ = 1;
    uint32_t active_hal_source_id_ = 0;
    std::string active_hal_output_name_;
    AudioStreamConfig active_hal_config_{};
    lib_utils::TaskScheduler::TaskId decoder_stream_task_id_ = 0;
    SourceStateChangedSignal source_state_changed_signal_;
    ActiveSourceChangedSignal active_source_changed_signal_;
    StreamDrainedSignal stream_drained_signal_;
};

} // namespace esp_brookesia::service
