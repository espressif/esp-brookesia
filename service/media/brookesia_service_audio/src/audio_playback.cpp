/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/audio_impl.hpp"

namespace esp_brookesia::service {

std::string AudioPlayback::get_component_version()
{
    return make_version(
               BROOKESIA_SERVICE_AUDIO_VER_MAJOR, BROOKESIA_SERVICE_AUDIO_VER_MINOR, BROOKESIA_SERVICE_AUDIO_VER_PATCH
           );
}


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

    if (request.config.only_if_idle) {
        if (request.config.interrupt) {
            return std::unexpected("Only-if-idle playback cannot interrupt");
        }
        if ((play_state_ != AudioPlayState::Idle) || (playlist_state_.phase != PlaylistPhase::Inactive) ||
                !playback_queue_.empty() || is_processing_queue_ || pause_requested_ || stop_for_pending_interrupt_ ||
                pending_interrupt_request_.has_value()) {
            return std::unexpected("Audio playback is busy");
        }
    }

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


std::string AudioEncoder::get_component_version()
{
    return make_version(
               BROOKESIA_SERVICE_AUDIO_VER_MAJOR, BROOKESIA_SERVICE_AUDIO_VER_MINOR, BROOKESIA_SERVICE_AUDIO_VER_PATCH
           );
}
}
