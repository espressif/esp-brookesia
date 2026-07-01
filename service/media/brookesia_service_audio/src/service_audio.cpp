/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <chrono>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "boost/format.hpp"
#include "brookesia/service_audio/macro_configs.h"
#if !BROOKESIA_SERVICE_AUDIO_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/audio/processor.hpp"
#include "brookesia/service_audio/service_audio.hpp"

namespace esp_brookesia::service {

constexpr size_t ENCODER_FETCH_DATA_SIZE_MORE = 100;
constexpr const char *DECODER_OUTPUT_NAME = "Speaker0";
constexpr const char *DECODER_OUTPUT_ROLE = "speaker";
constexpr uint32_t DECODER_STREAM_DRAIN_INTERVAL_MS = 1;
constexpr uint32_t DECODER_STREAM_QUEUE_SIZE_DEFAULT = 32 * 1024;

namespace {

inline int64_t get_current_time_ms()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// The registries below are intentionally leaked (heap-allocated, never deleted) to
// avoid a static destruction order fiasco: AudioDecoder/AudioEncoder instances are
// owned by the ServiceBase PluginRegistry (constructed during static init, destroyed
// late), while these function-local statistics are constructed at runtime and would
// otherwise be destroyed first. Their destructors would then access already-freed
// registries at program exit (heap-use-after-free).
std::mutex &get_decoder_registry_mutex()
{
    static std::mutex *mutex = new std::mutex();
    return *mutex;
}

std::vector<AudioDecoder *> &get_decoder_registry()
{
    static std::vector<AudioDecoder *> *registry = new std::vector<AudioDecoder *>();
    return *registry;
}

std::mutex &get_encoder_registry_mutex()
{
    static std::mutex *mutex = new std::mutex();
    return *mutex;
}

std::vector<AudioEncoder *> &get_encoder_registry()
{
    static std::vector<AudioEncoder *> *registry = new std::vector<AudioEncoder *>();
    return *registry;
}

} // namespace

bool AudioPlayback::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_AUDIO_VER_MAJOR, BROOKESIA_SERVICE_AUDIO_VER_MINOR,
        BROOKESIA_SERVICE_AUDIO_VER_PATCH
    );

    return true;
}

bool AudioPlayback::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    initialize_control_default_state();

    auto playback_handle = hal::acquire_interface<hal::audio::PlaybackIface>(
                               hal::audio::PlaybackIface::get_default_instance_name()
                           );
    auto playback_iface = playback_handle.get();
    BROOKESIA_CHECK_NULL_RETURN(playback_iface, false, "Failed to get audio playback interface");
    BROOKESIA_CHECK_FALSE_RETURN(
    playback_iface->open([this](AudioPlayState state) {
        on_playback_event(state);
    }),
    false, "Failed to open audio playback"
    );

    play_state_ = AudioPlayState::Idle;
    pause_requested_ = false;
    clear_pending_interrupt_playback();
    playback_iface_ = std::move(playback_handle);
#if BROOKESIA_SERVICE_AUDIO_PLAYBACK_ENABLE_AUTO_LOAD_DATA
    load_control_data_from_storage();
#endif

    return true;
}

void AudioPlayback::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::queue<PlaybackRequest>().swap(playback_queue_);
    is_processing_queue_ = false;
    pause_requested_ = false;
    clear_pending_interrupt_playback();
    cancel_playlist_scheduled_task();

    if (playback_iface_) {
        playback_iface_->close();
    }
    playback_iface_.reset();
    player_iface_.reset();
    control_data_loaded_ = false;
}

std::expected<void, std::string> AudioPlayback::function_play(
    const std::string &url, const boost::json::object &config
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: url(%1%), config(%2%)", url, BROOKESIA_DESCRIBE_TO_STR(config));

    AudioPlayUrlConfig playback_config = {};
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config, playback_config)) {
        return std::unexpected("Invalid play URL config");
    }

    return submit_playback_request(PlaybackRequest{
        .urls = {url},
        .config = playback_config,
    });
}

std::expected<void, std::string> AudioPlayback::function_play_list(
    const boost::json::array &urls, const boost::json::object &config
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: urls(%1%), config(%2%)", urls.size(), BROOKESIA_DESCRIBE_TO_STR(config));

    if (urls.empty()) {
        return std::unexpected("URLs array is empty");
    }

    std::vector<std::string> url_list;
    url_list.reserve(urls.size());
    for (const auto &url_value : urls) {
        if (!url_value.is_string()) {
            return std::unexpected("Invalid URL in array: expected string");
        }
        url_list.push_back(std::string(url_value.as_string()));
    }

    AudioPlayUrlConfig playback_config = {};
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config, playback_config)) {
        return std::unexpected("Invalid play URL config");
    }

    return submit_playback_request(PlaybackRequest{
        .urls = std::move(url_list),
        .config = playback_config,
    });
}

std::expected<void, std::string> AudioPlayback::submit_playback_request(PlaybackRequest request)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (request.config.interrupt) {
        return submit_interrupt_playback_request(std::move(request));
    }

    playback_queue_.push(std::move(request));
    is_processing_queue_ = true;
    BROOKESIA_LOGD("Queued playback request, queue size: %1%", playback_queue_.size());

    if ((play_state_ == AudioPlayState::Idle) && (playlist_state_.phase == PlaylistPhase::Inactive)) {
        process_playback_queue();
    }

    return {};
}

std::expected<void, std::string> AudioPlayback::submit_interrupt_playback_request(PlaybackRequest request)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    bool should_stop_paused_playback = (play_state_ == AudioPlayState::Paused) || pause_requested_;
    bool allow_gap_idle_before_start = (play_state_ != AudioPlayState::Idle) && (request.config.delay_ms > 0);

    std::queue<PlaybackRequest>().swap(playback_queue_);
    is_processing_queue_ = false;
    clear_pending_interrupt_playback();
    cancel_playlist_scheduled_task();

    if (should_stop_paused_playback) {
        pending_interrupt_request_ = std::move(request);
        stop_for_pending_interrupt_ = true;
        pause_requested_ = false;

        if (!playback_iface_ || !playback_iface_->stop()) {
            clear_pending_interrupt_playback();
            return std::unexpected("Failed to stop paused playback before interrupt");
        }
        return {};
    }

    auto config = request.config;
    start_playlist(std::move(request.urls), config, allow_gap_idle_before_start);

    return {};
}

std::expected<void, std::string> AudioPlayback::function_pause()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    pause_requested_ = true;
    const bool pause_ok = playback_iface_ && playback_iface_->pause();
    if (!pause_ok) {
        pause_requested_ = false;
        return std::unexpected("Failed to pause playback");
    }
    suspend_playlist_scheduled_task();

    return {};
}

std::expected<void, std::string> AudioPlayback::function_resume()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    pause_requested_ = false;
    if (!playback_iface_ || !playback_iface_->resume()) {
        return std::unexpected("Failed to resume playback");
    }
    if (!stop_for_pending_interrupt_) {
        pending_interrupt_request_.reset();
    }
    resume_playlist_scheduled_task();

    return {};
}

std::expected<void, std::string> AudioPlayback::function_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    pause_requested_ = false;
    clear_pending_interrupt_playback();
    cancel_playlist_scheduled_task();
    if (!playback_iface_ || !playback_iface_->stop()) {
        return std::unexpected("Failed to stop playback");
    }

    return {};
}

void AudioPlayback::start_playlist(
    std::vector<std::string> urls, const AudioPlayUrlConfig &config, bool allow_gap_idle_before_start
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    uint32_t total_loops = (config.loop_count == 0) ? 1 : config.loop_count;
    uint32_t new_session_id = 0;

    playlist_state_.urls = std::move(urls);
    playlist_state_.config = config;
    playlist_state_.current_url_index = 0;
    playlist_state_.current_loop = 0;
    playlist_state_.total_loops = total_loops;
    playlist_state_.phase = PlaylistPhase::WaitingToStart;
    playlist_state_.allow_gap_idle_before_start = allow_gap_idle_before_start;
    playlist_state_.gap_idle_published_before_start = false;
    playlist_state_.scheduled_task_id = 0;
    playlist_state_.pause_start_ms = 0;
    playlist_state_.paused_duration_ms = 0;
    playlist_state_.start_time_ms = 0;
    new_session_id = ++playlist_session_counter_;
    playlist_state_.session_id = new_session_id;

    BROOKESIA_LOGD("Starting playlist: %1% URLs x %2% loops, session %3%", urls.size(), total_loops, new_session_id);

    play_playlist_url_at_index(0);
}

void AudioPlayback::clear_pending_interrupt_playback()
{
    pending_interrupt_request_.reset();
    stop_for_pending_interrupt_ = false;
}

void AudioPlayback::start_pending_interrupt_playback_after_idle()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!pending_interrupt_request_) {
        clear_pending_interrupt_playback();
        return;
    }

    auto request = std::move(*pending_interrupt_request_);
    clear_pending_interrupt_playback();

    if (request.config.delay_ms > 0) {
        auto result = publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::PlayStateChanged), {
            BROOKESIA_DESCRIBE_TO_STR(AudioPlayState::Idle)
        });
        BROOKESIA_CHECK_FALSE_EXECUTE(result, {}, {
            BROOKESIA_LOGE("Failed to publish play state changed event");
        });
    }

    auto config = request.config;
    start_playlist(std::move(request.urls), config, false);
}

void AudioPlayback::play_playlist_url_at_index(size_t index)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::string url;
    uint32_t delay_ms = 0;
    uint32_t session_id = 0;
    uint32_t current_loop = 0;
    uint32_t total_loops = 0;

    if (index >= playlist_state_.urls.size()) {
        return;
    }
    url = playlist_state_.urls[index];
    session_id = playlist_state_.session_id;
    current_loop = playlist_state_.current_loop;
    total_loops = playlist_state_.total_loops;

    if (index == 0) {
        delay_ms = (current_loop == 0) ? playlist_state_.config.delay_ms : playlist_state_.config.loop_interval_ms;
    }

    size_t url_count = playlist_state_.urls.size();
    auto do_play = [this, url, session_id, index, current_loop, total_loops, url_count]() {
        if (playlist_state_.session_id != session_id) {
            BROOKESIA_LOGD(
                "Playlist playback cancelled (session %1% vs current %2%)", session_id, playlist_state_.session_id
            );
            return;
        }
        if (index == 0 && current_loop == 0) {
            playlist_state_.start_time_ms = get_current_time_ms();
        }
        playlist_state_.scheduled_task_id = 0;
        playlist_state_.phase = PlaylistPhase::StartingItem;

        BROOKESIA_LOGI(
            "Playing [loop %1%/%2%, url %3%/%4%]: %5%", current_loop + 1, total_loops, index + 1, url_count, url
        );

        if (!playback_iface_ || !playback_iface_->play(url)) {
            BROOKESIA_LOGE("Failed to play URL: %1%", url);
            playlist_state_.phase = PlaylistPhase::Inactive;
            playlist_state_.allow_gap_idle_before_start = false;
            playlist_state_.gap_idle_published_before_start = false;
        }
    };

    auto scheduler = get_task_scheduler();
    if (scheduler) {
        lib_utils::TaskScheduler::TaskId task_id = 0;
        if (delay_ms > 0) {
            BROOKESIA_LOGD("Scheduling playback after %1% ms delay", delay_ms);
            scheduler->post_delayed(std::move(do_play), delay_ms, &task_id, get_call_task_group());
        } else {
            scheduler->post(std::move(do_play), &task_id, get_call_task_group());
        }
        if (playlist_state_.session_id == session_id) {
            playlist_state_.scheduled_task_id = task_id;
        }
    } else {
        BROOKESIA_LOGW("TaskScheduler not available, executing directly");
        do_play();
    }
}

void AudioPlayback::advance_playlist()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    bool play_next_url = false;
    bool start_next_loop = false;
    bool playlist_finished = false;
    size_t next_url_index = 0;

    if (playlist_state_.phase != PlaylistPhase::PlayingCurrentItem) {
        return;
    }

    playlist_state_.current_url_index++;
    if (playlist_state_.current_url_index < playlist_state_.urls.size()) {
        play_next_url = true;
        next_url_index = playlist_state_.current_url_index;
    } else {
        playlist_state_.current_loop++;
        if (playlist_state_.config.timeout_ms > 0) {
            int64_t total_elapsed = get_current_time_ms() - playlist_state_.start_time_ms;
            int64_t effective_elapsed = total_elapsed - playlist_state_.paused_duration_ms;
            if (effective_elapsed >= static_cast<int64_t>(playlist_state_.config.timeout_ms)) {
                BROOKESIA_LOGI("Playlist timeout after %1% ms (effective)", effective_elapsed);
                playlist_state_.phase = PlaylistPhase::Inactive;
                playlist_finished = true;
            }
        }

        if (!playlist_finished) {
            bool more_loops = (playlist_state_.total_loops == UINT32_MAX) ||
                              (playlist_state_.current_loop < playlist_state_.total_loops);
            if (more_loops) {
                playlist_state_.current_url_index = 0;
                start_next_loop = true;
            } else {
                playlist_state_.phase = PlaylistPhase::Inactive;
                playlist_finished = true;
            }
        }
    }

    if (play_next_url) {
        playlist_state_.phase = PlaylistPhase::WaitingToStart;
        playlist_state_.allow_gap_idle_before_start = false;
        playlist_state_.gap_idle_published_before_start = false;
        play_playlist_url_at_index(next_url_index);
    } else if (start_next_loop) {
        auto idle_result = publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::PlayStateChanged), {
            BROOKESIA_DESCRIBE_TO_STR(AudioPlayState::Idle)
        });
        BROOKESIA_CHECK_FALSE_EXECUTE(idle_result, {}, {
            BROOKESIA_LOGE("Failed to publish play state changed event");
        });
        playlist_state_.phase = PlaylistPhase::WaitingToStart;
        playlist_state_.allow_gap_idle_before_start = false;
        playlist_state_.gap_idle_published_before_start = false;
        play_playlist_url_at_index(0);
    } else if (playlist_finished) {
        auto result = publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::PlayStateChanged), {
            BROOKESIA_DESCRIBE_TO_STR(AudioPlayState::Idle)
        });
        BROOKESIA_CHECK_FALSE_EXECUTE(result, {}, {
            BROOKESIA_LOGE("Failed to publish play state changed event");
        });

        if (is_processing_queue_) {
            process_playback_queue();
        }
    }
}

void AudioPlayback::cancel_playlist_scheduled_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    lib_utils::TaskScheduler::TaskId task_id = playlist_state_.scheduled_task_id;
    playlist_state_.scheduled_task_id = 0;
    playlist_state_.phase = PlaylistPhase::Inactive;
    playlist_state_.allow_gap_idle_before_start = false;
    playlist_state_.gap_idle_published_before_start = false;

    if (task_id != 0) {
        auto scheduler = get_task_scheduler();
        if (scheduler) {
            scheduler->cancel(task_id);
        }
    }
}

void AudioPlayback::suspend_playlist_scheduled_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    lib_utils::TaskScheduler::TaskId task_id = playlist_state_.scheduled_task_id;
    if ((playlist_state_.phase != PlaylistPhase::Inactive) && (playlist_state_.pause_start_ms == 0)) {
        playlist_state_.pause_start_ms = get_current_time_ms();
    }

    if (task_id != 0) {
        auto scheduler = get_task_scheduler();
        if (scheduler) {
            scheduler->suspend(task_id);
        }
    }
}

void AudioPlayback::resume_playlist_scheduled_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    lib_utils::TaskScheduler::TaskId task_id = playlist_state_.scheduled_task_id;
    if ((playlist_state_.phase != PlaylistPhase::Inactive) && (playlist_state_.pause_start_ms != 0)) {
        int64_t current_time_ms = get_current_time_ms();
        playlist_state_.paused_duration_ms += current_time_ms - playlist_state_.pause_start_ms;
        playlist_state_.pause_start_ms = 0;
    }

    if (task_id != 0) {
        auto scheduler = get_task_scheduler();
        if (scheduler) {
            scheduler->resume(task_id);
        }
    }
}

void AudioPlayback::process_playback_queue()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (playlist_state_.phase != PlaylistPhase::Inactive) {
        return;
    }

    if (playback_queue_.empty()) {
        is_processing_queue_ = false;
        return;
    }

    auto request = std::move(playback_queue_.front());
    playback_queue_.pop();
    is_processing_queue_ = true;
    start_playlist(std::move(request.urls), request.config, false);
}

void AudioPlayback::on_playback_event(AudioPlayState new_state)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    bool should_process_same_state_playing = (
                (new_state == AudioPlayState::Playing) &&
                ((playlist_state_.phase == PlaylistPhase::WaitingToStart) ||
                 (playlist_state_.phase == PlaylistPhase::StartingItem))
            );
    bool should_process_pending_interrupt_idle = (
                (new_state == AudioPlayState::Idle) &&
                stop_for_pending_interrupt_ &&
                pending_interrupt_request_.has_value()
            );
    if ((new_state == play_state_) && !should_process_same_state_playing && !should_process_pending_interrupt_idle) {
        return;
    }

    auto task_func = [this, new_state]() {
        PlaylistPhase playlist_phase = playlist_state_.phase;
        bool is_first_url_of_loop = (playlist_state_.current_url_index == 0);

        play_state_ = new_state;
        if (new_state == AudioPlayState::Paused) {
            pause_requested_ = false;
            if (stop_for_pending_interrupt_) {
                return;
            }
        } else if (new_state == AudioPlayState::Idle) {
            pause_requested_ = false;
            if (stop_for_pending_interrupt_) {
                start_pending_interrupt_playback_after_idle();
                return;
            }
        } else if (new_state == AudioPlayState::Playing) {
            pause_requested_ = false;
        }

        bool should_publish_event = true;
        if (new_state == AudioPlayState::Playing) {
            if ((playlist_phase == PlaylistPhase::WaitingToStart) ||
                    (playlist_phase == PlaylistPhase::StartingItem)) {
                playlist_state_.phase = PlaylistPhase::PlayingCurrentItem;
                playlist_state_.allow_gap_idle_before_start = false;
                playlist_state_.gap_idle_published_before_start = false;
            }
            if (!is_first_url_of_loop) {
                should_publish_event = false;
            }
        } else if (new_state == AudioPlayState::Idle) {
            if (playlist_phase == PlaylistPhase::WaitingToStart) {
                if (playlist_state_.allow_gap_idle_before_start &&
                        !playlist_state_.gap_idle_published_before_start) {
                    playlist_state_.gap_idle_published_before_start = true;
                    should_publish_event = true;
                } else {
                    should_publish_event = false;
                }
            } else if (playlist_phase == PlaylistPhase::StartingItem) {
                should_publish_event = false;
            } else if (playlist_phase == PlaylistPhase::PlayingCurrentItem) {
                should_publish_event = false;
            }
        }

        if (should_publish_event) {
            auto result = publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::PlayStateChanged), {
                BROOKESIA_DESCRIBE_TO_STR(new_state)
            });
            BROOKESIA_CHECK_FALSE_EXECUTE(result, {}, {
                BROOKESIA_LOGE("Failed to publish play state changed event");
            });
        }

        if (new_state == AudioPlayState::Idle) {
            if (playlist_phase == PlaylistPhase::PlayingCurrentItem) {
                advance_playlist();
            } else if ((playlist_state_.phase == PlaylistPhase::Inactive) && is_processing_queue_) {
                process_playback_queue();
            }
        }
    };
    auto task_scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_EXIT(task_scheduler, "Task scheduler is not available");
    auto result = task_scheduler->post(task_func, nullptr, get_call_task_group());
    BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to post play state changed task");
}

bool AudioEncoder::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    {
        std::lock_guard lock(get_encoder_registry_mutex());
        auto &registry = get_encoder_registry();
        if (id_ >= 0 && static_cast<size_t>(id_) >= registry.size()) {
            registry.resize(static_cast<size_t>(id_) + 1, nullptr);
        }
        if (id_ >= 0) {
            registry[static_cast<size_t>(id_)] = this;
        }
    }

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_AUDIO_VER_MAJOR, BROOKESIA_SERVICE_AUDIO_VER_MINOR,
        BROOKESIA_SERVICE_AUDIO_VER_PATCH
    );

    return true;
}

AudioEncoder *AudioEncoder::get_instance(int id)
{
    std::lock_guard lock(get_encoder_registry_mutex());
    auto &registry = get_encoder_registry();
    if (id < 0 || static_cast<size_t>(id) >= registry.size()) {
        return nullptr;
    }
    return registry[static_cast<size_t>(id)];
}

bool AudioEncoder::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto encoder_handle = hal::acquire_interface<hal::audio::EncoderIface>(
                              hal::audio::EncoderIface::get_default_instance_name(id_)
                          );
    auto encoder_iface = encoder_handle.get();
    BROOKESIA_CHECK_NULL_RETURN(encoder_iface, false, "Failed to get audio encoder interface");
    encoder_iface_ = std::move(encoder_handle);

    return true;
}

void AudioEncoder::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_encoder();
    encoder_iface_.reset();
}

std::expected<void, std::string> AudioEncoder::function_start(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    AudioEncoderDynamicConfig encoder_config;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config, encoder_config)) {
        return std::unexpected("Failed to parse encoder dynamic config");
    }
    if (encoder_config.type == BaseHelper::CodecFormat::PCM) {
        auto &general = encoder_config.general;
        auto target_fetch_data_size = general.frame_duration * general.sample_rate * general.channels *
                                      general.sample_bits / 8 / 1000 + ENCODER_FETCH_DATA_SIZE_MORE;
        if (encoder_config.fetch_data_size != target_fetch_data_size) {
            BROOKESIA_LOGW(
                "Detected different fetch data size for PCM type, adjusted from %1% to %2%",
                encoder_config.fetch_data_size, target_fetch_data_size
            );
            encoder_config.fetch_data_size = target_fetch_data_size;
        }
    }

    if (!start_encoder(encoder_config)) {
        return std::unexpected("Failed to start encoder");
    }

    return {};
}

std::expected<void, std::string> AudioEncoder::function_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_encoder();

    return {};
}

std::expected<void, std::string> AudioEncoder::function_pause()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!encoder_iface_) {
        return std::unexpected("Audio encoder is not available");
    }
    encoder_iface_->pause();

    return {};
}

std::expected<void, std::string> AudioEncoder::function_resume()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!encoder_iface_) {
        return std::unexpected("Audio encoder is not available");
    }
    encoder_iface_->resume();

    return {};
}

std::expected<void, std::string> AudioEncoder::function_pause_wake_end()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(encoder_state_mutex_);
    wake_end_paused_ = true;
    if (wake_end_task_id_ != 0) {
        auto scheduler = get_task_scheduler();
        if (scheduler) {
            scheduler->cancel(wake_end_task_id_);
        }
        wake_end_task_id_ = 0;
        wake_end_remaining_ms_ = std::max<int64_t>(1, wake_end_deadline_ms_ - get_current_time_ms());
        wake_end_deadline_ms_ = 0;
    }

    return {};
}

std::expected<void, std::string> AudioEncoder::function_resume_wake_end()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(encoder_state_mutex_);
    wake_end_paused_ = false;
    if (wake_end_pending_ && (wake_end_task_id_ == 0) && (wake_end_remaining_ms_ > 0)) {
        if (!schedule_wake_end_locked(static_cast<uint32_t>(wake_end_remaining_ms_))) {
            return std::unexpected("Failed to resume WakeEnd timer");
        }
    }

    return {};
}

std::expected<boost::json::array, std::string> AudioEncoder::function_get_afe_wake_words()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!encoder_iface_) {
        return std::unexpected("Audio encoder is not available");
    }

    auto wake_words_array = encoder_iface_->get_afe_wake_words();
    return BROOKESIA_DESCRIBE_TO_JSON(wake_words_array).as_array();
}

bool AudioEncoder::start_encoder(const AudioEncoderDynamicConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(static_cast<bool>(encoder_iface_), false, "Audio encoder is not available");
    if (encoder_iface_->is_started()) {
        return true;
    }

    {
        std::lock_guard lock(encoder_state_mutex_);
        encoder_config_ = config;
        wake_end_paused_ = false;
        wake_end_pending_ = false;
        wake_end_task_id_ = 0;
        wake_end_deadline_ms_ = 0;
        wake_end_remaining_ms_ = 0;
        wake_end_session_id_++;
    }

    hal::audio::EncoderIface::Callbacks callbacks{
        .afe_event = [this](AudioAFE_Event event)
        {
            on_afe_event(event);
        },
        .recorder_data = [this](const uint8_t *data, size_t size)
        {
            on_recorder_input_data(data, size);
        },
    };
    BROOKESIA_CHECK_FALSE_RETURN(
        encoder_iface_->start(config, std::move(callbacks)), false, "Failed to start encoder"
    );
    if (!start_encoder_fetch_task(config)) {
        encoder_iface_->stop();
        return false;
    }

    BROOKESIA_LOGI("Encoder started");

    return true;
}

void AudioEncoder::stop_encoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_encoder_fetch_task();
    cancel_wake_end();

    if (!encoder_iface_ || !encoder_iface_->is_started()) {
        return;
    }

    encoder_iface_->stop();
    BROOKESIA_LOGI("Encoder stopped");
}

bool AudioEncoder::start_encoder_fetch_task(const AudioEncoderDynamicConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Task scheduler is not available");

    {
        std::lock_guard lock(encoder_state_mutex_);
        encoder_fetch_buffer_.resize(config.fetch_data_size);
    }

    auto task = [this, fetch_data_size = config.fetch_data_size, fetch_interval_ms = config.fetch_interval_ms]() {
        return encoder_fetch_task(fetch_data_size, fetch_interval_ms);
    };
    auto result = scheduler->post_periodic(
                      task, static_cast<int>(std::max<uint32_t>(1, config.fetch_interval_ms)),
                      &encoder_fetch_task_id_, get_call_task_group()
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to schedule encoder fetch task");

    return true;
}

void AudioEncoder::stop_encoder_fetch_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (encoder_fetch_task_id_ != 0) {
        auto scheduler = get_task_scheduler();
        if (scheduler) {
            scheduler->cancel(encoder_fetch_task_id_);
        }
        encoder_fetch_task_id_ = 0;
    }
    std::lock_guard lock(encoder_state_mutex_);
    encoder_fetch_buffer_.clear();
}

bool AudioEncoder::encoder_fetch_task(uint32_t fetch_data_size, uint32_t fetch_interval_ms)
{
    if (fetch_data_size == 0) {
        BROOKESIA_LOGE("Invalid encoder fetch data size");
        return false;
    }

    (void)fetch_interval_ms;

    // Snapshot the encoder interface under the state lock, then run the blocking read WITHOUT
    // holding encoder_state_mutex_. Holding the lock across read_encoded_data() deadlocks with the
    // AFE event path: on_afe_event()/schedule_wake_end() block acquiring encoder_state_mutex_ from
    // the AFE dispatch context, while this task waits inside read_encoded_data() for the AFE to
    // produce a frame, which it cannot until that dispatch completes -> AFE(FEED) ringbuffer overflow.
    std::shared_ptr<hal::audio::EncoderIface> iface;
    {
        std::lock_guard lock(encoder_state_mutex_);
        if (!encoder_iface_ || !encoder_iface_->is_started()) {
            return false;
        }
        iface = encoder_iface_.get();
    }

    std::vector<uint8_t> data(fetch_data_size);
    int ret_size = iface->read_encoded_data(data.data(), data.size());
    if (ret_size <= 0) {
        if (ret_size < 0) {
            BROOKESIA_LOGW("Failed to read encoded data");
        }
        return true;
    }
    data.resize(static_cast<size_t>(ret_size));

    RawBuffer buffer(data.data(), data.size());
    encoded_data_signal_(buffer);
    return true;
}

void AudioEncoder::on_afe_event(AudioAFE_Event afe_event)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (afe_event == BaseHelper::AFE_Event::Max) {
        return;
    }

    publish_afe_event(afe_event);
    AudioEncoderDynamicConfig encoder_config = {};
    {
        std::lock_guard lock(encoder_state_mutex_);
        encoder_config = encoder_config_;
    }

    switch (afe_event) {
    case BaseHelper::AFE_Event::WakeStart:
        schedule_wake_end(encoder_config.afe_wake_start_timeout_ms);
        break;
    case BaseHelper::AFE_Event::VAD_Start:
        cancel_wake_end();
        break;
    case BaseHelper::AFE_Event::VAD_End:
        schedule_wake_end(encoder_config.afe_wake_end_timeout_ms);
        break;
    case BaseHelper::AFE_Event::WakeEnd:
    case BaseHelper::AFE_Event::Max:
        break;
    }
}

void AudioEncoder::publish_afe_event(AudioAFE_Event afe_event)
{
    auto result = publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::AFEEventHappened), {
        BROOKESIA_DESCRIBE_TO_STR(afe_event)
    });
    BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to publish afe event happened event");
}

void AudioEncoder::on_recorder_input_data(const uint8_t *data, size_t size)
{
    RawBuffer buffer(data, size);
    recorder_data_signal_(buffer);
}

void AudioEncoder::schedule_wake_end(uint32_t timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(encoder_state_mutex_);
    cancel_wake_end_locked();
    if (!encoder_config_.enable_afe || (timeout_ms == 0)) {
        return;
    }

    wake_end_pending_ = true;
    wake_end_remaining_ms_ = timeout_ms;
    if (wake_end_paused_) {
        return;
    }

    BROOKESIA_CHECK_FALSE_EXECUTE(schedule_wake_end_locked(timeout_ms), {}, {
        BROOKESIA_LOGE("Failed to schedule WakeEnd event");
    });
}

bool AudioEncoder::schedule_wake_end_locked(uint32_t timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Task scheduler is not available");

    const uint32_t session_id = ++wake_end_session_id_;
    lib_utils::TaskScheduler::TaskId task_id = 0;
    auto result = scheduler->post_delayed([this, session_id]() {
        on_wake_end_timeout(session_id);
    }, static_cast<int>(timeout_ms), &task_id, get_call_task_group());
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to post WakeEnd delayed task");

    wake_end_task_id_ = task_id;
    wake_end_deadline_ms_ = get_current_time_ms() + timeout_ms;
    wake_end_remaining_ms_ = timeout_ms;
    wake_end_pending_ = true;

    return true;
}

void AudioEncoder::cancel_wake_end()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(encoder_state_mutex_);
    cancel_wake_end_locked();
}

void AudioEncoder::cancel_wake_end_locked()
{
    if (wake_end_task_id_ != 0) {
        auto scheduler = get_task_scheduler();
        if (scheduler) {
            scheduler->cancel(wake_end_task_id_);
        }
    }
    wake_end_task_id_ = 0;
    wake_end_deadline_ms_ = 0;
    wake_end_remaining_ms_ = 0;
    wake_end_pending_ = false;
    wake_end_session_id_++;
}

void AudioEncoder::on_wake_end_timeout(uint32_t session_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    {
        std::lock_guard lock(encoder_state_mutex_);
        if ((session_id != wake_end_session_id_) || wake_end_paused_ || !wake_end_pending_) {
            return;
        }
        wake_end_task_id_ = 0;
        wake_end_deadline_ms_ = 0;
        wake_end_remaining_ms_ = 0;
        wake_end_pending_ = false;
    }

    publish_afe_event(BaseHelper::AFE_Event::WakeEnd);
}

AudioDecoder::AudioDecoder(int id)
    : ServiceBase({
    .name = std::string(helper::Audio::DECODER_NAME_PREFIX) + std::to_string(id),
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
{
    std::lock_guard lock(get_decoder_registry_mutex());
    auto &registry = get_decoder_registry();
    if (id_ >= 0 && static_cast<size_t>(id_) >= registry.size()) {
        registry.resize(static_cast<size_t>(id_) + 1, nullptr);
    }
    if (id_ >= 0) {
        registry[static_cast<size_t>(id_)] = this;
    }
}

AudioDecoder::~AudioDecoder()
{
    std::lock_guard lock(get_decoder_registry_mutex());
    auto &registry = get_decoder_registry();
    if (id_ >= 0 && static_cast<size_t>(id_) < registry.size() && registry[static_cast<size_t>(id_)] == this) {
        registry[static_cast<size_t>(id_)] = nullptr;
    }
}

AudioDecoder *AudioDecoder::get_instance(int id)
{
    std::lock_guard lock(get_decoder_registry_mutex());
    auto &registry = get_decoder_registry();
    if (id < 0 || static_cast<size_t>(id) >= registry.size()) {
        return nullptr;
    }
    return registry[static_cast<size_t>(id)];
}

std::vector<AudioOutputInfo> AudioDecoder::get_outputs() const
{
    std::lock_guard lock(decoder_state_mutex_);
    std::vector<AudioOutputInfo> outputs;
    outputs.reserve(outputs_.size());
    for (const auto &output : outputs_) {
        outputs.push_back(output.info);
    }
    return outputs;
}

std::vector<AudioSourceInfo> AudioDecoder::get_sources() const
{
    std::lock_guard lock(decoder_state_mutex_);
    std::vector<AudioSourceInfo> sources;
    sources.reserve(sources_.size());
    for (const auto &[_, source] : sources_) {
        sources.push_back(source.info);
    }
    return sources;
}

std::expected<uint32_t, std::string> AudioDecoder::register_source(AudioSourceInfo source)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (source.name.empty()) {
        return std::unexpected("Audio source name is empty");
    }

    std::lock_guard lock(decoder_state_mutex_);
    if (find_source_by_name_locked(source.name) != nullptr) {
        return std::unexpected("Audio source already registered: " + source.name);
    }

    if (source.id == 0) {
        source.id = next_source_id_++;
    } else if (sources_.contains(source.id)) {
        return std::unexpected("Audio source id already registered");
    }

    const uint32_t source_id = source.id;
    sources_.emplace(source_id, SourceContext{
        .info = std::move(source),
        .requested_outputs = {},
        .streams = {},
    });
    return source_id;
}

std::expected<void, std::string> AudioDecoder::unregister_source(uint32_t source_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(decoder_state_mutex_);
    auto source_it = sources_.find(source_id);
    if (source_it == sources_.end()) {
        return std::unexpected("Audio source is not registered");
    }

    for (auto &output : outputs_) {
        if (output.active_source_id == source_id) {
            stop_hal_decoder_locked();
            output.active_source_id = 0;
            emit_active_source_changed(output.info.name, "");
        }
    }

    for (auto &[output_name, stream] : source_it->second.streams) {
        clear_stream_queue_locked(stream);
        emit_source_state_changed(source_it->second.info.name, output_name, AudioSourceState::Released);
    }
    sources_.erase(source_it);
    return {};
}

std::expected<void, std::string> AudioDecoder::unregister_source(std::string_view source_name)
{
    uint32_t source_id = 0;
    {
        std::lock_guard lock(decoder_state_mutex_);
        const auto *source = find_source_by_name_locked(source_name);
        if (source == nullptr) {
            return std::unexpected("Audio source is not registered");
        }
        source_id = source->info.id;
    }
    return unregister_source(source_id);
}

std::expected<void, std::string> AudioDecoder::request_output(uint32_t source_id, std::string_view output_name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(decoder_state_mutex_);
    auto *source = find_source_by_id_locked(source_id);
    if (source == nullptr) {
        return std::unexpected("Audio source is not registered");
    }
    if (find_output_locked(output_name) == nullptr) {
        return std::unexpected("Audio output is not registered");
    }

    source->requested_outputs.insert(std::string(output_name));
    emit_source_state_changed(source->info.name, std::string(output_name), AudioSourceState::Requested);
    return {};
}

std::expected<void, std::string> AudioDecoder::request_output(
    std::string_view source_name, std::string_view output_name
)
{
    uint32_t source_id = 0;
    {
        std::lock_guard lock(decoder_state_mutex_);
        const auto *source = find_source_by_name_locked(source_name);
        if (source == nullptr) {
            return std::unexpected("Audio source is not registered");
        }
        source_id = source->info.id;
    }
    return request_output(source_id, output_name);
}

std::expected<void, std::string> AudioDecoder::release_output(uint32_t source_id, std::string_view output_name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(decoder_state_mutex_);
    auto *source = find_source_by_id_locked(source_id);
    auto *output = find_output_locked(output_name);
    if (source == nullptr) {
        return std::unexpected("Audio source is not registered");
    }
    if (output == nullptr) {
        return std::unexpected("Audio output is not registered");
    }

    if (output->active_source_id == source_id) {
        stop_hal_decoder_locked();
        output->active_source_id = 0;
        emit_active_source_changed(output->info.name, "");
    }
    auto stream_it = source->streams.find(std::string(output_name));
    if (stream_it != source->streams.end()) {
        clear_stream_queue_locked(stream_it->second);
        source->streams.erase(stream_it);
    }
    source->requested_outputs.erase(std::string(output_name));
    emit_source_state_changed(source->info.name, std::string(output_name), AudioSourceState::Released);
    return {};
}

std::expected<void, std::string> AudioDecoder::release_output(
    std::string_view source_name, std::string_view output_name
)
{
    uint32_t source_id = 0;
    {
        std::lock_guard lock(decoder_state_mutex_);
        const auto *source = find_source_by_name_locked(source_name);
        if (source == nullptr) {
            return std::unexpected("Audio source is not registered");
        }
        source_id = source->info.id;
    }
    return release_output(source_id, output_name);
}

std::expected<void, std::string> AudioDecoder::set_active_source(
    std::string_view output_name, std::string_view source_name
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(decoder_state_mutex_);
    auto *output = find_output_locked(output_name);
    auto *source = find_source_by_name_locked(source_name);
    if (output == nullptr) {
        return std::unexpected("Audio output is not registered");
    }
    if (source == nullptr) {
        return std::unexpected("Audio source is not registered");
    }
    if (!source->requested_outputs.contains(std::string(output_name))) {
        return std::unexpected("Audio source has not requested output: " + std::string(output_name));
    }
    if (output->active_source_id == source->info.id) {
        return {};
    }

    if (output->active_source_id != 0) {
        auto *old_source = find_source_by_id_locked(output->active_source_id);
        if (old_source != nullptr) {
            emit_source_state_changed(old_source->info.name, output->info.name, AudioSourceState::Requested);
        }
    }
    stop_hal_decoder_locked();
    clear_all_queues_locked();
    output->active_source_id = source->info.id;
    emit_source_state_changed(source->info.name, output->info.name, AudioSourceState::Granted);
    emit_active_source_changed(output->info.name, source->info.name);
    auto stream_it = source->streams.find(output->info.name);
    if (stream_it != source->streams.end() && stream_it->second.opened) {
        ensure_hal_decoder_for_active_stream_locked(*source, output->info.name);
    }
    return {};
}

std::expected<std::string, std::string> AudioDecoder::get_active_source(std::string_view output_name) const
{
    std::lock_guard lock(decoder_state_mutex_);
    const auto *output = find_output_locked(output_name);
    if (output == nullptr) {
        return std::unexpected("Audio output is not registered");
    }
    const auto *source = find_source_by_id_locked(output->active_source_id);
    return source != nullptr ? source->info.name : std::string();
}

std::expected<void, std::string> AudioDecoder::open_stream(
    uint32_t source_id, std::string_view output_name, const AudioStreamConfig &config
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(decoder_state_mutex_);
    auto *source = find_source_by_id_locked(source_id);
    if (source == nullptr) {
        return std::unexpected("Audio source is not registered");
    }
    if (find_output_locked(output_name) == nullptr) {
        return std::unexpected("Audio output is not registered");
    }
    if (!source->requested_outputs.contains(std::string(output_name))) {
        return std::unexpected("Audio source has not requested output: " + std::string(output_name));
    }

    auto &stream = source->streams[std::string(output_name)];
    clear_stream_queue_locked(stream);
    stream.config = config;
    if (stream.config.queue_size_bytes == 0) {
        stream.config.queue_size_bytes = DECODER_STREAM_QUEUE_SIZE_DEFAULT;
    }
    stream.opened = true;
    if (is_source_active_locked(source_id, output_name)) {
        if (!ensure_hal_decoder_for_active_stream_locked(*source, std::string(output_name))) {
            return std::unexpected("Failed to start audio decoder stream");
        }
    }
    if (!start_decoder_stream_task_locked()) {
        return std::unexpected("Failed to start decoder stream task");
    }
    return {};
}

std::expected<void, std::string> AudioDecoder::open_stream(
    std::string_view source_name, std::string_view output_name, const AudioStreamConfig &config
)
{
    uint32_t source_id = 0;
    {
        std::lock_guard lock(decoder_state_mutex_);
        const auto *source = find_source_by_name_locked(source_name);
        if (source == nullptr) {
            return std::unexpected("Audio source is not registered");
        }
        source_id = source->info.id;
    }
    return open_stream(source_id, output_name, config);
}

std::expected<void, std::string> AudioDecoder::close_stream(uint32_t source_id, std::string_view output_name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(decoder_state_mutex_);
    auto *source = find_source_by_id_locked(source_id);
    if (source == nullptr) {
        return std::unexpected("Audio source is not registered");
    }
    auto stream_it = source->streams.find(std::string(output_name));
    if (stream_it == source->streams.end()) {
        return {};
    }
    if (is_source_active_locked(source_id, output_name)) {
        stop_hal_decoder_locked();
    }
    clear_stream_queue_locked(stream_it->second);
    source->streams.erase(stream_it);
    return {};
}

std::expected<void, std::string> AudioDecoder::close_stream(
    std::string_view source_name, std::string_view output_name
)
{
    uint32_t source_id = 0;
    {
        std::lock_guard lock(decoder_state_mutex_);
        const auto *source = find_source_by_name_locked(source_name);
        if (source == nullptr) {
            return std::unexpected("Audio source is not registered");
        }
        source_id = source->info.id;
    }
    return close_stream(source_id, output_name);
}

AudioWriteResult AudioDecoder::write_stream(
    uint32_t source_id, std::string_view output_name, const RawBuffer &data, uint32_t timeout_ms
)
{
    if ((data.data_ptr == nullptr) || (data.data_size == 0)) {
        return AudioWriteResult::DroppedInvalidData;
    }

    std::unique_lock lock(decoder_state_mutex_);
    StreamContext *stream = nullptr;
    auto reserve_result = reserve_stream_queue_space_locked(
                              lock, source_id, output_name, data.data_size, timeout_ms, stream
                          );
    if (reserve_result != AudioWriteResult::Written) {
        return reserve_result;
    }

    StreamContext::Chunk chunk = {};
    chunk.owned_data.assign(data.data_ptr, data.data_ptr + data.data_size);
    stream->queued_bytes += chunk.owned_data.size();
    stream->queue.emplace_back(std::move(chunk));
    return AudioWriteResult::Written;
}

AudioWriteResult AudioDecoder::write_stream(
    std::string_view source_name, std::string_view output_name, const RawBuffer &data, uint32_t timeout_ms
)
{
    uint32_t source_id = 0;
    {
        std::lock_guard lock(decoder_state_mutex_);
        const auto *source = find_source_by_name_locked(source_name);
        if (source == nullptr) {
            return AudioWriteResult::Error;
        }
        source_id = source->info.id;
    }
    return write_stream(source_id, output_name, data, timeout_ms);
}

AudioWriteResult AudioDecoder::write_stream_borrowed(
    uint32_t source_id, std::string_view output_name, const RawBuffer &data,
    StreamBufferReleaseCallback release_callback, uint32_t timeout_ms
)
{
    if ((data.data_ptr == nullptr) || (data.data_size == 0)) {
        return AudioWriteResult::DroppedInvalidData;
    }
    if (!release_callback) {
        return AudioWriteResult::DroppedInvalidData;
    }

    StreamContext::Chunk chunk = {};
    chunk.borrowed_data = data;
    chunk.release_callback = std::move(release_callback);
    chunk.borrowed = true;
    return enqueue_stream_chunk_locked(source_id, output_name, std::move(chunk), timeout_ms);
}

AudioWriteResult AudioDecoder::write_stream_borrowed(
    std::string_view source_name, std::string_view output_name, const RawBuffer &data,
    StreamBufferReleaseCallback release_callback, uint32_t timeout_ms
)
{
    uint32_t source_id = 0;
    {
        std::lock_guard lock(decoder_state_mutex_);
        const auto *source = find_source_by_name_locked(source_name);
        if (source == nullptr) {
            return AudioWriteResult::Error;
        }
        source_id = source->info.id;
    }
    return write_stream_borrowed(source_id, output_name, data, std::move(release_callback), timeout_ms);
}

bool AudioDecoder::is_stream_drained(uint32_t source_id, std::string_view output_name) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(decoder_state_mutex_);
    const auto *source = find_source_by_id_locked(source_id);
    if (source == nullptr) {
        return false;
    }
    auto stream_it = source->streams.find(std::string(output_name));
    if (stream_it == source->streams.end()) {
        return false;
    }

    return stream_it->second.opened && stream_it->second.queue.empty() && (stream_it->second.queued_bytes == 0);
}

bool AudioDecoder::is_stream_drained(std::string_view source_name, std::string_view output_name) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(decoder_state_mutex_);
    const auto *source = find_source_by_name_locked(source_name);
    if (source == nullptr) {
        return false;
    }
    auto stream_it = source->streams.find(std::string(output_name));
    if (stream_it == source->streams.end()) {
        return false;
    }

    return stream_it->second.opened && stream_it->second.queue.empty() && (stream_it->second.queued_bytes == 0);
}

bool AudioDecoder::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_AUDIO_VER_MAJOR, BROOKESIA_SERVICE_AUDIO_VER_MINOR,
        BROOKESIA_SERVICE_AUDIO_VER_PATCH
    );

    return true;
}

bool AudioDecoder::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto decoder_handle = hal::acquire_interface<hal::audio::DecoderIface>(
                              hal::audio::DecoderIface::get_default_instance_name(id_)
                          );
    auto decoder_iface = decoder_handle.get();
    BROOKESIA_CHECK_NULL_RETURN(decoder_iface, false, "Failed to get audio decoder interface");
    std::lock_guard lock(decoder_state_mutex_);
    decoder_iface_ = std::move(decoder_handle);
    outputs_ = {
        OutputContext{
            .info = AudioOutputInfo{
                .id = 0,
                .name = DECODER_OUTPUT_NAME,
                .role = DECODER_OUTPUT_ROLE,
                .sample_rates = {},
                .channels = {},
                .sample_bits = {16},
            },
        },
    };

    return true;
}

void AudioDecoder::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_decoder_stream_task();
    std::lock_guard lock(decoder_state_mutex_);
    stop_hal_decoder_locked();
    clear_all_queues_locked();
    sources_.clear();
    outputs_.clear();
    decoder_iface_.reset();
}

std::expected<boost::json::array, std::string> AudioDecoder::function_get_outputs()
{
    return BROOKESIA_DESCRIBE_TO_JSON(get_outputs()).as_array();
}

std::expected<boost::json::array, std::string> AudioDecoder::function_get_sources()
{
    return BROOKESIA_DESCRIBE_TO_JSON(get_sources()).as_array();
}

std::expected<double, std::string> AudioDecoder::function_register_source(const boost::json::object &source_json)
{
    AudioSourceInfo source;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(source_json, source)) {
        return std::unexpected("Failed to parse audio source info");
    }
    auto result = register_source(std::move(source));
    if (!result) {
        return std::unexpected(result.error());
    }
    return static_cast<double>(result.value());
}

std::expected<void, std::string> AudioDecoder::function_unregister_source(const std::string &source_name)
{
    return unregister_source(source_name);
}

std::expected<void, std::string> AudioDecoder::function_request_output(
    const std::string &source_name, const std::string &output_name
)
{
    return request_output(source_name, output_name);
}

std::expected<void, std::string> AudioDecoder::function_release_output(
    const std::string &source_name, const std::string &output_name
)
{
    return release_output(source_name, output_name);
}

std::expected<void, std::string> AudioDecoder::function_set_active_source(
    const std::string &output_name, const std::string &source_name
)
{
    return set_active_source(output_name, source_name);
}

std::expected<std::string, std::string> AudioDecoder::function_get_active_source(const std::string &output_name)
{
    return get_active_source(output_name);
}

std::expected<void, std::string> AudioDecoder::function_open_stream(
    const std::string &source_name, const std::string &output_name, const boost::json::object &config
)
{
    AudioStreamConfig stream_config;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config, stream_config)) {
        return std::unexpected("Failed to parse audio stream config");
    }
    return open_stream(source_name, output_name, stream_config);
}

std::expected<void, std::string> AudioDecoder::function_close_stream(
    const std::string &source_name, const std::string &output_name
)
{
    return close_stream(source_name, output_name);
}

AudioDecoder::SourceContext *AudioDecoder::find_source_by_id_locked(uint32_t source_id)
{
    auto it = sources_.find(source_id);
    return it != sources_.end() ? &it->second : nullptr;
}

const AudioDecoder::SourceContext *AudioDecoder::find_source_by_id_locked(uint32_t source_id) const
{
    auto it = sources_.find(source_id);
    return it != sources_.end() ? &it->second : nullptr;
}

AudioDecoder::SourceContext *AudioDecoder::find_source_by_name_locked(std::string_view source_name)
{
    for (auto &[_, source] : sources_) {
        if (source.info.name == source_name) {
            return &source;
        }
    }
    return nullptr;
}

const AudioDecoder::SourceContext *AudioDecoder::find_source_by_name_locked(std::string_view source_name) const
{
    for (const auto &[_, source] : sources_) {
        if (source.info.name == source_name) {
            return &source;
        }
    }
    return nullptr;
}

AudioDecoder::OutputContext *AudioDecoder::find_output_locked(std::string_view output_name)
{
    for (auto &output : outputs_) {
        if (output.info.name == output_name) {
            return &output;
        }
    }
    return nullptr;
}

const AudioDecoder::OutputContext *AudioDecoder::find_output_locked(std::string_view output_name) const
{
    for (const auto &output : outputs_) {
        if (output.info.name == output_name) {
            return &output;
        }
    }
    return nullptr;
}

bool AudioDecoder::is_source_active_locked(uint32_t source_id, std::string_view output_name) const
{
    const auto *output = find_output_locked(output_name);
    return (output != nullptr) && (output->active_source_id == source_id);
}

bool AudioDecoder::ensure_hal_decoder_for_active_stream_locked(SourceContext &source, const std::string &output_name)
{
    auto stream_it = source.streams.find(output_name);
    if (stream_it == source.streams.end() || !stream_it->second.opened) {
        return true;
    }

    std::lock_guard hal_lock(decoder_hal_mutex_);
    if ((active_hal_source_id_ == source.info.id) && (active_hal_output_name_ == output_name) &&
            decoder_iface_ && decoder_iface_->is_started()) {
        return true;
    }
    if (decoder_iface_ && decoder_iface_->is_started()) {
        decoder_iface_->stop();
    }
    if (!decoder_iface_) {
        return false;
    }
    const auto &config = stream_it->second.config;
    AudioDecoderDynamicConfig decoder_config{
        .type = config.type,
        .general = config.general,
    };
    if (!decoder_iface_->start(decoder_config)) {
        active_hal_source_id_ = 0;
        active_hal_output_name_.clear();
        return false;
    }
    active_hal_source_id_ = source.info.id;
    active_hal_output_name_ = output_name;
    active_hal_config_ = config;
    return true;
}

AudioWriteResult AudioDecoder::reserve_stream_queue_space_locked(
    std::unique_lock<std::mutex> &lock, uint32_t source_id, std::string_view output_name, size_t data_size,
    uint32_t timeout_ms, StreamContext *&stream
)
{
    auto find_stream = [&]() -> AudioWriteResult {
        stream = nullptr;
        auto *source = find_source_by_id_locked(source_id);
        if (source == nullptr)
        {
            return AudioWriteResult::Error;
        }
        if (!is_source_active_locked(source_id, output_name))
        {
            return AudioWriteResult::DroppedNotActive;
        }
        auto stream_it = source->streams.find(std::string(output_name));
        if ((stream_it == source->streams.end()) || !stream_it->second.opened)
        {
            return AudioWriteResult::Error;
        }
        stream = &stream_it->second;
        if (data_size > stream->config.queue_size_bytes)
        {
            return AudioWriteResult::DroppedQueueFull;
        }
        return AudioWriteResult::Written;
    };

    auto has_space = [&]() {
        return (stream != nullptr) && ((stream->queued_bytes + data_size) <= stream->config.queue_size_bytes);
    };

    auto result = find_stream();
    if (result != AudioWriteResult::Written) {
        return result;
    }
    if (has_space()) {
        return AudioWriteResult::Written;
    }
    if (timeout_ms == 0) {
        return AudioWriteResult::DroppedQueueFull;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (true) {
        if (decoder_queue_cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            result = find_stream();
            if (result != AudioWriteResult::Written) {
                return result;
            }
            return has_space() ? AudioWriteResult::Written : AudioWriteResult::DroppedQueueFull;
        }

        result = find_stream();
        if (result != AudioWriteResult::Written) {
            return result;
        }
        if (has_space()) {
            return AudioWriteResult::Written;
        }
    }
}

AudioWriteResult AudioDecoder::enqueue_stream_chunk_locked(
    uint32_t source_id, std::string_view output_name, StreamContext::Chunk &&chunk, uint32_t timeout_ms
)
{
    const size_t data_size = chunk.size();
    if ((chunk.data() == nullptr) || (data_size == 0)) {
        return AudioWriteResult::DroppedInvalidData;
    }

    std::unique_lock lock(decoder_state_mutex_);
    StreamContext *stream = nullptr;
    auto reserve_result = reserve_stream_queue_space_locked(
                              lock, source_id, output_name, data_size, timeout_ms, stream
                          );
    if (reserve_result != AudioWriteResult::Written) {
        return reserve_result;
    }

    stream->queued_bytes += data_size;
    stream->queue.emplace_back(std::move(chunk));
    return AudioWriteResult::Written;
}

void AudioDecoder::clear_stream_queue_locked(StreamContext &stream)
{
    while (!stream.queue.empty()) {
        auto chunk = std::move(stream.queue.front());
        stream.queue.pop_front();
        chunk.release(AudioWriteResult::DroppedNotActive);
    }
    stream.queue.clear();
    stream.queued_bytes = 0;
    decoder_queue_cv_.notify_all();
}

void AudioDecoder::clear_all_queues_locked()
{
    for (auto &[_, source] : sources_) {
        for (auto &[__, stream] : source.streams) {
            clear_stream_queue_locked(stream);
        }
    }
}

void AudioDecoder::stop_hal_decoder_locked()
{
    std::lock_guard hal_lock(decoder_hal_mutex_);
    if (decoder_iface_ && decoder_iface_->is_started()) {
        decoder_iface_->stop();
    }
    active_hal_source_id_ = 0;
    active_hal_output_name_.clear();
    active_hal_config_ = {};
}

bool AudioDecoder::start_decoder_stream_task_locked()
{
    if (decoder_stream_task_id_ != 0) {
        return true;
    }
    auto scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Task scheduler is not available");
    auto result = scheduler->post_periodic([this]() {
        return decoder_stream_task();
    }, DECODER_STREAM_DRAIN_INTERVAL_MS, &decoder_stream_task_id_, get_call_task_group());
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to schedule decoder stream task");
    return true;
}

void AudioDecoder::stop_decoder_stream_task()
{
    if (decoder_stream_task_id_ == 0) {
        return;
    }
    auto scheduler = get_task_scheduler();
    if (scheduler) {
        scheduler->cancel(decoder_stream_task_id_);
    }
    decoder_stream_task_id_ = 0;
}

bool AudioDecoder::decoder_stream_task()
{
    StreamContext::Chunk chunk;
    bool stream_drained = false;
    std::string drained_source_name;
    std::string drained_output_name;
    {
        std::lock_guard lock(decoder_state_mutex_);
        for (auto &output : outputs_) {
            if (output.active_source_id == 0) {
                continue;
            }
            auto *source = find_source_by_id_locked(output.active_source_id);
            if (source == nullptr) {
                continue;
            }
            auto stream_it = source->streams.find(output.info.name);
            if ((stream_it == source->streams.end()) || stream_it->second.queue.empty()) {
                continue;
            }
            chunk = std::move(stream_it->second.queue.front());
            stream_it->second.queue.pop_front();
            stream_it->second.queued_bytes -= chunk.size();
            if (stream_it->second.queue.empty() && (stream_it->second.queued_bytes == 0)) {
                stream_drained = true;
                drained_source_name = source->info.name;
                drained_output_name = output.info.name;
            }
            decoder_queue_cv_.notify_all();
            break;
        }
    }

    if ((chunk.data() == nullptr) || (chunk.size() == 0)) {
        return true;
    }

    std::lock_guard hal_lock(decoder_hal_mutex_);
    if (!decoder_iface_ || !decoder_iface_->is_started()) {
        chunk.release(AudioWriteResult::Error);
        return true;
    }
    if (!decoder_iface_->feed_data(chunk.data(), chunk.size())) {
        BROOKESIA_LOGW("Failed to feed audio decoder stream data");
        chunk.release(AudioWriteResult::Error);
        return true;
    }
    chunk.release(AudioWriteResult::Written);
    if (stream_drained) {
        emit_stream_drained(drained_source_name, drained_output_name);
    }
    return true;
}

void AudioDecoder::emit_source_state_changed(
    const std::string &source_name, const std::string &output_name, AudioSourceState state
)
{
    source_state_changed_signal_(source_name, output_name, state);
}

void AudioDecoder::emit_active_source_changed(const std::string &output_name, const std::string &source_name)
{
    active_source_changed_signal_(output_name, source_name);
}

void AudioDecoder::emit_stream_drained(const std::string &source_name, const std::string &output_name)
{
    stream_drained_signal_(source_name, output_name);
}

#if BROOKESIA_SERVICE_AUDIO_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    ServiceBase, AudioPlayback, AudioPlayback::get_instance().get_attributes().name, AudioPlayback::get_instance(),
    BROOKESIA_SERVICE_AUDIO_PLAYBACK_PLUGIN_SYMBOL
);

BROOKESIA_PLUGIN_REGISTER_WITH_SYMBOL(
    ServiceBase, AudioEncoder, helper::AudioEncoder<0>::get_name().data(),
    BROOKESIA_SERVICE_AUDIO_ENCODER_PLUGIN_SYMBOL_0, 0
);

BROOKESIA_PLUGIN_REGISTER_WITH_SYMBOL(
    ServiceBase, AudioDecoder, helper::AudioDecoder<0>::get_name().data(),
    BROOKESIA_SERVICE_AUDIO_DECODER_PLUGIN_SYMBOL_0, 0
);
#endif // BROOKESIA_SERVICE_AUDIO_ENABLE_AUTO_REGISTER

} // namespace esp_brookesia::service
