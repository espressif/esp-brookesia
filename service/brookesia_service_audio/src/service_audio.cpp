/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cmath>
#include <chrono>
#include <algorithm>
#include "boost/format.hpp"
#include "boost/thread.hpp"
#include "esp_bit_defs.h"
#include "esp_codec_dev.h"
#include "esp_gmf_afe.h"
#include "brookesia/service_audio/macro_configs.h"
#if !BROOKESIA_SERVICE_AUDIO_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/type_converter.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/service_manager/macro_configs.h"
#include "brookesia/service_helper/nvs.hpp"
#include "brookesia/service_audio/service_audio.hpp"
#include "audio_processor.h"

constexpr size_t ENCODER_FETCH_DATA_SIZE_MORE = 100;

namespace esp_brookesia::service {

using NVSHelper = helper::NVS;

namespace {
// Helper function to get current time in milliseconds using std::chrono
inline int64_t get_current_time_ms()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
} // namespace

constexpr uint32_t NVS_SAVE_DATA_TIMEOUT_MS = 20;
constexpr uint32_t NVS_ERASE_DATA_TIMEOUT_MS = 20;

bool Audio::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_AUDIO_VER_MAJOR, BROOKESIA_SERVICE_AUDIO_VER_MINOR,
        BROOKESIA_SERVICE_AUDIO_VER_PATCH
    );

    return true;
}

void Audio::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI("Deinitialized");
}

bool Audio::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Try to load data from NVS
    try_load_data();

    BROOKESIA_LOGI(
        "Start peripheral with config:\n%1%",
        BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(peripheral_config_, BROOKESIA_DESCRIBE_FORMAT_VERBOSE)
    );

    // Open audio dac and audio_adc
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = static_cast<uint8_t>(peripheral_config_.board_bits),
        .channel = static_cast<uint8_t>(peripheral_config_.board_channels),
        .sample_rate = static_cast<uint32_t>(peripheral_config_.board_sample_rate),
    };
    BROOKESIA_LOGI(
        "Board sample info: sample_rate(%1%) channel(%2%) bits_per_sample(%3%)", fs.sample_rate, fs.channel,
        fs.bits_per_sample
    );

    esp_codec_dev_handle_t play_dev = static_cast<esp_codec_dev_handle_t>(peripheral_config_.play_dev);
    esp_codec_dev_handle_t rec_dev = static_cast<esp_codec_dev_handle_t>(peripheral_config_.rec_dev);
    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_codec_dev_open(play_dev, &fs), false, "Failed to open audio dac");
    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_codec_dev_open(rec_dev, &fs), false, "Failed to open audio_adc");

    // Initialize audio manager
    auto manager_config = TypeConverter::convert(peripheral_config_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        audio_manager_init(&manager_config), false, "Failed to initialize audio manager"
    );

    // Open playback
    BROOKESIA_LOGI(
        "Start playback with config:\n%1%",
        BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(playback_config_, BROOKESIA_DESCRIBE_FORMAT_VERBOSE)
    );
    auto playback_event_cb = +[](audio_player_state_t state, void *ctx) {
        auto self = static_cast<Audio *>(ctx);
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");
        self->on_playback_event(state);
    };
    auto playback_config = TypeConverter::convert(
                               playback_config_, reinterpret_cast<const void *>(playback_event_cb), this
                           );
    BROOKESIA_CHECK_ESP_ERR_RETURN(audio_playback_open(&playback_config), false, "Failed to open playback");
    play_state_ = AudioPlayState::Idle;

    // Set player volume
    auto init_volume = playback_config_.volume_default;
    if (is_data_loaded_ && data_player_volume_ > 0) {
        init_volume = std::clamp(
                          static_cast<uint8_t>(data_player_volume_), playback_config_.volume_min,
                          playback_config_.volume_max
                      );
        if (init_volume != data_player_volume_) {
            set_data<DataType::PlayerVolume>(init_volume);
            try_save_data(DataType::PlayerVolume);
        }
    }
    set_data<DataType::PlayerVolume>(init_volume);
    BROOKESIA_CHECK_FALSE_RETURN(
        esp_codec_dev_set_out_vol(play_dev, init_volume) == ESP_CODEC_DEV_OK,
        false, "Failed to set play volume"
    );

    // Set recorder gain
    BROOKESIA_CHECK_FALSE_RETURN(
        esp_codec_dev_set_in_gain(rec_dev, peripheral_config_.recorder_gain) == ESP_CODEC_DEV_OK,
        false, "Failed to set recorder gain"
    );
    for (const auto &[channel, gain] : peripheral_config_.recorder_channel_gains) {
        esp_codec_dev_set_in_channel_gain(rec_dev, BIT(channel), gain);
    }

    return true;
}

void Audio::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Clear play URL queue and loop state
    {
        std::lock_guard<std::mutex> lock(play_url_queue_mutex_);
        std::queue<PlayUrlRequest>().swap(play_url_queue_);
        is_processing_queue_ = false;
    }
    // Cancel any scheduled loop task before clearing loop state
    cancel_loop_scheduled_task();
    {
        std::lock_guard<std::mutex> lock(loop_state_mutex_);
        loop_state_.is_active = false;
    }

    BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_playback_close(), {}, {
        BROOKESIA_LOGE("Failed to close playback");
    });

    stop_encoder();
    stop_decoder();

    BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_manager_deinit(), {}, {
        BROOKESIA_LOGE("Failed to deinitialize audio manager");
    });
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_codec_dev_close(
                                        static_cast<esp_codec_dev_handle_t>(peripheral_config_.play_dev)
    ), {}, {
        BROOKESIA_LOGE("Failed to close playback device");
    });
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_codec_dev_close(
                                        static_cast<esp_codec_dev_handle_t>(peripheral_config_.rec_dev)
    ), {}, {
        BROOKESIA_LOGE("Failed to close recorder device");
    });
}

std::expected<void, std::string> Audio::function_set_peripheral(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    AudioPeripheralConfig peripheral_config = {};
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config, peripheral_config)) {
        return std::unexpected("Invalid peripheral config");
    }

    peripheral_config_ = peripheral_config;

    return {};
}

std::expected<void, std::string> Audio::function_set_playback_config(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    AudioPlaybackConfig playback_config = {};
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config, playback_config)) {
        return std::unexpected("Invalid playback config");
    }

    playback_config_ = playback_config;

    return {};
}

std::expected<void, std::string> Audio::function_set_encoder_static_config(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    AudioEncoderStaticConfig encoder_static_config = {};
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config, encoder_static_config)) {
        return std::unexpected("Invalid encoder static config");
    }

    encoder_static_config_ = encoder_static_config;

    return {};
}

std::expected<void, std::string> Audio::function_set_decoder_static_config(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    AudioDecoderStaticConfig decoder_static_config = {};
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config, decoder_static_config)) {
        return std::unexpected("Invalid decoder static config");
    }

    decoder_static_config_ = decoder_static_config;

    return {};
}

std::expected<void, std::string> Audio::function_set_afe_config(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    AudioAFE_Config afe_config = {};
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config, afe_config)) {
        return std::unexpected("Invalid AFE config");
    }

    afe_config_ = afe_config;

    return {};
}

std::expected<void, std::string> Audio::function_play_url(const std::string &url, const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: url(%1%), config(%2%)", url, BROOKESIA_DESCRIBE_TO_STR(config));

    AudioPlayUrlConfig play_url_config = {};
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config, play_url_config)) {
        return std::unexpected("Invalid play URL config");
    }

    if (play_url_config.interrupt) {
        // Interrupt mode: play immediately, clear queue and loop state
        {
            std::lock_guard<std::mutex> lock(play_url_queue_mutex_);
            // Clear the queue
            std::queue<PlayUrlRequest>().swap(play_url_queue_);
        }
        // Cancel any scheduled loop task before clearing loop state
        cancel_loop_scheduled_task();
        {
            std::lock_guard<std::mutex> lock(loop_state_mutex_);
            loop_state_.is_active = false;
        }

        if (!play_url_internal(url, play_url_config)) {
            return std::unexpected("Failed to play URL: " + url);
        }
    } else {
        // Non-interrupt mode: add to queue
        {
            std::lock_guard<std::mutex> lock(play_url_queue_mutex_);
            play_url_queue_.push({url, play_url_config});
            // Mark as processing so on_playback_event will continue processing
            is_processing_queue_ = true;
            BROOKESIA_LOGD("Added URL to queue, queue size: %1%", play_url_queue_.size());
        }

        // If not currently playing, start processing the queue immediately
        if (play_state_ == AudioPlayState::Idle) {
            process_play_url_queue();
        }
    }

    return {};
}

std::expected<void, std::string> Audio::function_play_urls(const boost::json::array &urls, const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: urls(%1%), config(%2%)", urls.size(), BROOKESIA_DESCRIBE_TO_STR(config));

    if (urls.empty()) {
        return std::unexpected("URLs array is empty");
    }

    // Parse URLs from JSON array
    std::vector<std::string> url_list;
    url_list.reserve(urls.size());
    for (const auto &url_value : urls) {
        if (url_value.is_string()) {
            url_list.push_back(std::string(url_value.as_string()));
        } else {
            return std::unexpected("Invalid URL in array: expected string");
        }
    }

    AudioPlayUrlConfig play_url_config = {};
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config, play_url_config)) {
        return std::unexpected("Invalid play URL config");
    }

    if (play_url_config.interrupt) {
        // Interrupt mode: clear queue and loop state, then add all URLs
        {
            std::lock_guard<std::mutex> lock(play_url_queue_mutex_);
            std::queue<PlayUrlRequest>().swap(play_url_queue_);
        }
        cancel_loop_scheduled_task();
        {
            std::lock_guard<std::mutex> lock(loop_state_mutex_);
            loop_state_.is_active = false;
        }

        // Play first URL immediately, add rest to queue
        if (!play_url_internal(url_list[0], play_url_config)) {
            return std::unexpected("Failed to play URL: " + url_list[0]);
        }

        // Add remaining URLs to queue (without loop/delay for subsequent URLs)
        if (url_list.size() > 1) {
            AudioPlayUrlConfig subsequent_config = play_url_config;
            subsequent_config.delay_ms = 0;       // No delay for queued items
            subsequent_config.loop_count = 0;     // No loop for queued items
            subsequent_config.timeout_ms = 0;     // No timeout for queued items

            std::lock_guard<std::mutex> lock(play_url_queue_mutex_);
            for (size_t i = 1; i < url_list.size(); ++i) {
                play_url_queue_.push({url_list[i], subsequent_config});
            }
            is_processing_queue_ = true;
            BROOKESIA_LOGD("Added %1% URLs to queue", url_list.size() - 1);
        }
    } else {
        // Non-interrupt mode: add all URLs to queue
        AudioPlayUrlConfig first_config = play_url_config;
        AudioPlayUrlConfig subsequent_config = play_url_config;
        subsequent_config.delay_ms = 0;       // No delay for subsequent items
        subsequent_config.loop_count = 0;     // No loop for subsequent items
        subsequent_config.timeout_ms = 0;     // No timeout for subsequent items

        {
            std::lock_guard<std::mutex> lock(play_url_queue_mutex_);
            // First URL keeps original config (with delay, loop, timeout)
            play_url_queue_.push({url_list[0], first_config});
            // Subsequent URLs use simplified config
            for (size_t i = 1; i < url_list.size(); ++i) {
                play_url_queue_.push({url_list[i], subsequent_config});
            }
            is_processing_queue_ = true;
            BROOKESIA_LOGD("Added %1% URLs to queue, queue size: %2%", url_list.size(), play_url_queue_.size());
        }

        // If not currently playing, start processing the queue immediately
        if (play_state_ == AudioPlayState::Idle) {
            process_play_url_queue();
        }
    }

    return {};
}

bool Audio::play_url_internal(const std::string &url, const AudioPlayUrlConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: url(%1%), config(%2%)", url, BROOKESIA_DESCRIBE_TO_STR(config));

    uint32_t play_count = (config.loop_count == 0) ? 1 : config.loop_count;
    bool need_loop = (play_count > 1);

    // Setup loop state for multi-iteration playback
    // First, disable loop to prevent old playback's stop event from triggering loop continuation
    uint32_t new_session_id = 0;
    {
        std::lock_guard<std::mutex> lock(loop_state_mutex_);
        loop_state_.is_active = false;  // Disable first
        loop_state_.url = url;
        loop_state_.config = config;
        loop_state_.current_iteration = 0;
        loop_state_.total_iterations = play_count;
        loop_state_.scheduled_task_id = 0;
        loop_state_.pause_start_ms = 0;      // Reset pause tracking
        loop_state_.paused_duration_ms = 0;  // Reset accumulated pause duration
        // Assign a new session ID to distinguish this loop from previous ones
        new_session_id = ++loop_session_counter_;
        loop_state_.session_id = new_session_id;
        BROOKESIA_LOGD("New loop session: %1%", new_session_id);
    }

    // Lambda to perform the actual playback
    auto do_play = [this, url, play_count, need_loop, new_session_id]() {
        // Check if this task still belongs to the current session
        {
            std::lock_guard<std::mutex> lock(loop_state_mutex_);
            if (loop_state_.session_id != new_session_id) {
                BROOKESIA_LOGD("Delayed playback cancelled (session %1% vs current %2%)",
                               new_session_id, loop_state_.session_id);
                return;
            }
            // Set start time when playback actually begins
            loop_state_.start_time_ms = get_current_time_ms();
        }

        BROOKESIA_LOGD("Playing URL (iteration 1/%1%): %2%", play_count, url);

        if (audio_playback_play(url.c_str()) != ESP_OK) {
            BROOKESIA_LOGE("Failed to play URL: %1%", url);
            return;
        }

        // Enable loop after playback starts successfully
        if (need_loop) {
            std::lock_guard<std::mutex> lock(loop_state_mutex_);
            // Only enable if session ID still matches (not superseded by another call)
            if (loop_state_.session_id == new_session_id) {
                loop_state_.is_active = true;
            }
        }
    };

    // Check if delay is needed
    if (config.delay_ms > 0) {
        auto scheduler = get_task_scheduler();
        if (scheduler) {
            lib_utils::TaskScheduler::TaskId task_id = 0;
            BROOKESIA_LOGD("Scheduling playback after %1% ms delay", config.delay_ms);
            scheduler->post_delayed(std::move(do_play), config.delay_ms, &task_id);
            // Save task ID for potential cancellation
            {
                std::lock_guard<std::mutex> lock(loop_state_mutex_);
                if (loop_state_.session_id == new_session_id) {
                    loop_state_.scheduled_task_id = task_id;
                }
            }
        } else {
            BROOKESIA_LOGW("TaskScheduler not available, ignoring delay");
            do_play();
        }
    } else {
        // No delay, play immediately
        do_play();
    }

    return true;
}

void Audio::handle_loop_playback_complete()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    bool should_continue = false;
    bool loop_finished = false;

    {
        std::lock_guard<std::mutex> lock(loop_state_mutex_);
        if (!loop_state_.is_active) {
            return;
        }

        loop_state_.current_iteration++;
        const auto &loop_config = loop_state_.config;

        // Check timeout (excluding paused duration)
        if (loop_config.timeout_ms > 0) {
            int64_t total_elapsed_ms = get_current_time_ms() - loop_state_.start_time_ms;
            int64_t effective_elapsed_ms = total_elapsed_ms - loop_state_.paused_duration_ms;
            if (effective_elapsed_ms >= static_cast<int64_t>(loop_config.timeout_ms)) {
                BROOKESIA_LOGI("Loop playback timeout after %1% ms (effective), total: %2% ms, paused: %3% ms",
                               effective_elapsed_ms, total_elapsed_ms, loop_state_.paused_duration_ms);
                loop_state_.is_active = false;
                loop_finished = true;
            }
        }

        // Check if more iterations needed
        if (!loop_finished) {
            if (loop_state_.current_iteration < loop_state_.total_iterations) {
                should_continue = true;
            } else {
                loop_state_.is_active = false;
                loop_finished = true;
                BROOKESIA_LOGD("Loop playback completed all iterations");
            }
        }
    }

    if (should_continue) {
        schedule_next_loop_iteration();
    } else if (loop_finished) {
        // Loop completely finished, now publish the Idle event
        auto result = publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::PlayStateChanged), {
            BROOKESIA_DESCRIBE_TO_STR(AudioPlayState::Idle)
        });
        BROOKESIA_CHECK_FALSE_EXECUTE(result, {}, {
            BROOKESIA_LOGE("Failed to publish play state changed event");
        });

        // Process next item in queue if any
        if (is_processing_queue_) {
            process_play_url_queue();
        }
    }
}

void Audio::schedule_next_loop_iteration()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::string url;
    uint32_t current_iteration = 0;
    uint32_t total_iterations = 0;
    uint32_t interval_ms = 0;
    uint32_t session_id = 0;

    {
        std::lock_guard<std::mutex> lock(loop_state_mutex_);
        if (!loop_state_.is_active) {
            return;
        }
        url = loop_state_.url;
        current_iteration = loop_state_.current_iteration;
        total_iterations = loop_state_.total_iterations;
        interval_ms = loop_state_.config.loop_interval_ms;
        session_id = loop_state_.session_id;
    }

    auto play_next = [this, url, current_iteration, total_iterations, session_id]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        // Check if this task belongs to the current loop session
        {
            std::lock_guard<std::mutex> lock(loop_state_mutex_);
            if (!loop_state_.is_active || loop_state_.session_id != session_id) {
                BROOKESIA_LOGD("Loop playback was cancelled or superseded (session %1% vs current %2%)",
                               session_id, loop_state_.session_id);
                return;
            }
        }

        BROOKESIA_LOGI("Playing URL (iteration %1%/%2%): %3%", current_iteration + 1, total_iterations, url);

        if (audio_playback_play(url.c_str()) != ESP_OK) {
            BROOKESIA_LOGE("Failed to play URL in loop: %1%", url);
            std::lock_guard<std::mutex> lock(loop_state_mutex_);
            loop_state_.is_active = false;
        }
    };

    auto scheduler = get_task_scheduler();
    if (scheduler) {
        lib_utils::TaskScheduler::TaskId task_id = 0;
        if (interval_ms > 0) {
            BROOKESIA_LOGD("Scheduling next loop iteration after %1% ms", interval_ms);
            scheduler->post_delayed(std::move(play_next), interval_ms, &task_id);
        } else {
            scheduler->post(std::move(play_next), &task_id);
        }
        // Save task ID for potential cancellation
        {
            std::lock_guard<std::mutex> lock(loop_state_mutex_);
            loop_state_.scheduled_task_id = task_id;
        }
    } else {
        // Fallback: execute directly (not recommended)
        BROOKESIA_LOGW("TaskScheduler not available, executing directly");
        play_next();
    }
}

void Audio::cancel_loop_scheduled_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    lib_utils::TaskScheduler::TaskId task_id = 0;
    {
        std::lock_guard<std::mutex> lock(loop_state_mutex_);
        task_id = loop_state_.scheduled_task_id;
        loop_state_.scheduled_task_id = 0;
    }

    if (task_id != 0) {
        auto scheduler = get_task_scheduler();
        if (scheduler) {
            scheduler->cancel(task_id);
            BROOKESIA_LOGD("Cancelled loop scheduled task: %1%", task_id);
        }
    }
}

void Audio::suspend_loop_scheduled_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    lib_utils::TaskScheduler::TaskId task_id = 0;
    {
        std::lock_guard<std::mutex> lock(loop_state_mutex_);
        task_id = loop_state_.scheduled_task_id;
        // Record pause start time for timeout calculation
        if (loop_state_.is_active && loop_state_.pause_start_ms == 0) {
            loop_state_.pause_start_ms = get_current_time_ms();
            BROOKESIA_LOGD("Loop timeout paused at %1% ms", loop_state_.pause_start_ms);
        }
    }

    if (task_id != 0) {
        auto scheduler = get_task_scheduler();
        if (scheduler) {
            if (scheduler->suspend(task_id)) {
                BROOKESIA_LOGD("Suspended loop scheduled task: %1%", task_id);
            }
        }
    }
}

void Audio::resume_loop_scheduled_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    lib_utils::TaskScheduler::TaskId task_id = 0;
    {
        std::lock_guard<std::mutex> lock(loop_state_mutex_);
        task_id = loop_state_.scheduled_task_id;
        // Calculate and accumulate pause duration for timeout calculation
        if (loop_state_.is_active && loop_state_.pause_start_ms != 0) {
            int64_t current_time_ms = get_current_time_ms();
            int64_t pause_duration = current_time_ms - loop_state_.pause_start_ms;
            loop_state_.paused_duration_ms += pause_duration;
            loop_state_.pause_start_ms = 0;  // Reset pause start
            BROOKESIA_LOGD("Loop timeout resumed, paused for %1% ms, total paused: %2% ms",
                           pause_duration, loop_state_.paused_duration_ms);
        }
    }

    if (task_id != 0) {
        auto scheduler = get_task_scheduler();
        if (scheduler) {
            if (scheduler->resume(task_id)) {
                BROOKESIA_LOGD("Resumed loop scheduled task: %1%", task_id);
            }
        }
    }
}

void Audio::process_play_url_queue()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    PlayUrlRequest request;
    {
        std::lock_guard<std::mutex> lock(play_url_queue_mutex_);
        if (play_url_queue_.empty()) {
            BROOKESIA_LOGD("Play URL queue is empty");
            is_processing_queue_ = false;
            return;
        }
        request = std::move(play_url_queue_.front());
        play_url_queue_.pop();
        is_processing_queue_ = true;
        BROOKESIA_LOGD("Processing URL from queue, remaining: %1%", play_url_queue_.size());
    }

    // Post the playback task to avoid blocking
    auto scheduler = get_task_scheduler();
    if (scheduler) {
        auto task = [this, request = std::move(request)]() mutable {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            play_url_internal(request.url, request.config);
        };
        scheduler->post(std::move(task));
    } else {
        // Fallback: play directly
        play_url_internal(request.url, request.config);
    }
}

std::expected<void, std::string> Audio::function_play_control(const std::string &action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: action(%1%)", action);

    AudioPlayControlAction action_enum;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM(action, action_enum)) {
        return std::unexpected("Invalid action: " + action);
    }

    switch (action_enum) {
    case AudioPlayControlAction::Pause:
        if (audio_playback_pause() != ESP_OK) {
            return std::unexpected("Failed to pause playback");
        }
        // Also suspend any scheduled loop task
        suspend_loop_scheduled_task();
        break;
    case AudioPlayControlAction::Resume:
        if (audio_playback_resume() != ESP_OK) {
            return std::unexpected("Failed to resume playback");
        }
        // Also resume any suspended loop task
        resume_loop_scheduled_task();
        break;
    case AudioPlayControlAction::Stop:
        // Cancel any scheduled loop task
        cancel_loop_scheduled_task();
        {
            std::lock_guard<std::mutex> lock(loop_state_mutex_);
            loop_state_.is_active = false;
        }
        if (audio_playback_stop() != ESP_OK) {
            return std::unexpected("Failed to stop playback");
        }
        break;
    }

    return {};
}

std::expected<void, std::string> Audio::function_set_volume(double volume)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: volume(%1%)", volume);

    int volume_int = static_cast<int>(volume);
    volume_int = std::clamp(
                     volume_int, static_cast<int>(playback_config_.volume_min),
                     static_cast<int>(playback_config_.volume_max)
                 );
    if (data_player_volume_ != volume_int) {
        auto result = esp_codec_dev_set_out_vol(
                          static_cast<esp_codec_dev_handle_t>(peripheral_config_.play_dev), volume_int
                      );
        if (result != ESP_CODEC_DEV_OK) {
            return std::unexpected("Failed to set codec dev out volume");
        }
        set_data<DataType::PlayerVolume>(volume_int);
        try_save_data(DataType::PlayerVolume);
    } else {
        BROOKESIA_LOGD("Volume is the same, skip");
    }

    return {};
}

std::expected<double, std::string> Audio::function_get_volume()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return static_cast<double>(get_data<DataType::PlayerVolume>());
}

std::expected<void, std::string> Audio::function_start_encoder(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    AudioEncoderDynamicConfig encoder_config;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config, encoder_config)) {
        return std::unexpected("Failed to parse encoder dynamic config");
    }
    if (encoder_config.type == Helper::CodecFormat::PCM) {
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

std::expected<void, std::string> Audio::function_stop_encoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_encoder();

    return {};
}

std::expected<void, std::string> Audio::function_start_decoder(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    AudioDecoderDynamicConfig decoder_config;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config, decoder_config)) {
        return std::unexpected("Failed to parse decoder dynamic config");
    }

    if (!start_decoder(decoder_config)) {
        return std::unexpected("Failed to start decoder");
    }

    return {};
}

std::expected<void, std::string> Audio::function_stop_decoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_decoder();

    return {};
}

std::expected<void, std::string> Audio::function_feed_decoder_data(const RawBuffer &data)
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // BROOKESIA_LOGD("Params: data(%1%)", BROOKESIA_DESCRIBE_TO_STR(data));

    if (!is_decoder_started_) {
        return std::unexpected("Decoder is not running");
    }

    auto result = audio_feeder_feed_data(const_cast<uint8_t *>(data.data_ptr), data.data_size);
    if (result != ESP_OK) {
        return std::unexpected((boost::format("Failed to feed decoder data: %1%") % esp_err_to_name(result)).str());
    }

    return {};
}

void Audio::try_load_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_data_loaded_) {
        BROOKESIA_LOGD("Data is already loaded, skip");
        return;
    }

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto binding = service::ServiceManager::get_instance().bind(NVSHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_EXIT(binding.is_valid(), "Failed to bind NVS service");

    auto nvs_namespace = get_attributes().name;

    {
        auto key = BROOKESIA_DESCRIBE_TO_STR(DataType::PlayerVolume);
        auto result = NVSHelper::get_key_value<int>(nvs_namespace, key);
        if (!result) {
            BROOKESIA_LOGD("Failed to load '%1%' from NVS: %2%", key, result.error());
        } else {
            set_data<DataType::PlayerVolume>(result.value());
            BROOKESIA_LOGD("Loaded '%1%' from NVS", key);
        }
    }

    is_data_loaded_ = true;

    BROOKESIA_LOGI("Loaded all data from NVS");
}

void Audio::try_save_data(DataType type)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto key = BROOKESIA_DESCRIBE_TO_STR(type);
    BROOKESIA_LOGD("Params: type(%1%)", key);

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto nvs_namespace = get_attributes().name;

    auto save_function = [this, &nvs_namespace, &key](const auto & data_value) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        auto result = NVSHelper::save_key_value(nvs_namespace, key, data_value, NVS_SAVE_DATA_TIMEOUT_MS);
        if (!result) {
            BROOKESIA_LOGE("Failed to save '%1%' to NVS: %2%", key, result.error());
        } else {
            BROOKESIA_LOGI("Saved '%1%' to NVS", key);
        }
    };

    if (type == DataType::PlayerVolume) {
        save_function(get_data<DataType::PlayerVolume>());
    } else {
        BROOKESIA_LOGE("Invalid data type for saving to NVS");
    }
}

void Audio::try_erase_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto result = NVSHelper::erase_keys(get_attributes().name, {}, NVS_ERASE_DATA_TIMEOUT_MS);
    if (!result) {
        BROOKESIA_LOGE("Failed to erase NVS data: %1%", result.error());
    } else {
        BROOKESIA_LOGI("Erased NVS data");
    }
}

bool Audio::start_encoder(const AudioEncoderDynamicConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_encoder_started()) {
        BROOKESIA_LOGD("Encoder is already running");
        return true;
    }

    BROOKESIA_LOGI(
        "Start encoder with AFE config:\n%1% \n and static config:\n%2% \n and dynamic config:\n%3%",
        BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(afe_config_, BROOKESIA_DESCRIBE_FORMAT_VERBOSE),
        BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(encoder_static_config_, BROOKESIA_DESCRIBE_FORMAT_VERBOSE),
        BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(config, BROOKESIA_DESCRIBE_FORMAT_VERBOSE)
    );

    auto event_cb = +[](void *event, void *ctx) {
        // BROOKESIA_LOG_TRACE_GUARD();
        auto self = static_cast<Audio *>(ctx);
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");
        self->on_recorder_event(event);
    };
    auto raw_data_cb = +[](const uint8_t *data, size_t size, void *ctx) {
        // BROOKESIA_LOG_TRACE_GUARD();
        auto self = static_cast<Audio *>(ctx);
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");
        self->on_recorder_input_data(data, size);
    };
    auto recorder_config = TypeConverter::convert(
                               encoder_static_config_, config, afe_config_, afe_model_partition_label_,
                               afe_mn_language_, reinterpret_cast<const void *>(event_cb), this,
                               reinterpret_cast<const void *>(raw_data_cb), this
                           );

#if (BROOKESIA_SERVICE_AUDIO_ENABLE_WORKER && BROOKESIA_SERVICE_AUDIO_WORKER_STACK_IN_EXT) || \
    (!BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT)
    BROOKESIA_CHECK_ESP_ERR_RETURN(audio_recorder_open(&recorder_config), false, "Failed to open recorder");
#else
    {
        // Since initializing SR in `audio_recorder_open()` operates on flash,
        // a separate thread with its stack located in SRAM needs to be created to prevent a crash.
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_in_ext = false,
        });
        auto recorder_open_future = std::async(std::launch::async, [this, recorder_config]() mutable {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            BROOKESIA_CHECK_ESP_ERR_RETURN(audio_recorder_open(&recorder_config), false, "Failed to open recorder");
            return true;
        });
        BROOKESIA_CHECK_FALSE_RETURN(recorder_open_future.get(), false, "Failed to open recorder");
    }
#endif

    lib_utils::FunctionGuard close_recorder_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_recorder_close(), {}, {
            BROOKESIA_LOGE("Failed to close recorder");
        });
    });

    auto recorder_fetch_thread_func =
    [this, fetch_data_size = config.fetch_data_size, fetch_interval_ms = config.fetch_interval_ms]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD("recorder fetch thread started (fetch data size: %1%)", fetch_data_size);

        std::unique_ptr<uint8_t[]> data(new (std::nothrow) uint8_t[fetch_data_size]);
        BROOKESIA_CHECK_NULL_EXIT(data, "Failed to allocate memory");

        int ret_size = 0;
        try {
            while (!boost::this_thread::interruption_requested()) {
                ret_size = audio_recorder_read_data(data.get(), fetch_data_size);
                // BROOKESIA_LOGD("Reading data from recorder (%1%)", ret_size);
                if (ret_size > 0) {
                    BROOKESIA_CHECK_FALSE_EXIT(
                    publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::EncoderDataReady), {
                        RawBuffer(static_cast<const uint8_t *>(data.get()), ret_size)
                    }), "Failed to publish recorder data ready event"
                    );
                } else {
                    boost::this_thread::sleep_for(boost::chrono::milliseconds(fetch_interval_ms));
                }
            }
        } catch (const boost::thread_interrupted &e) {
            BROOKESIA_LOGI("recorder fetch thread interrupted");
        }

        BROOKESIA_LOGI("recorder fetch thread stopped");
    };
    {
        BROOKESIA_THREAD_CONFIG_GUARD(encoder_static_config_.fetcher_task);
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            recorder_fetcher_thread_ = boost::thread(recorder_fetch_thread_func), false,
            "Failed to create recorder fetch thread"
        );
    }

    close_recorder_guard.release();

    is_encoder_started_ = true;

    BROOKESIA_LOGI("Encoder started");

    return true;
}

void Audio::stop_encoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_encoder_started()) {
        BROOKESIA_LOGD("Encoder is not running");
        return;
    }

    if (recorder_fetcher_thread_.joinable()) {
        recorder_fetcher_thread_.interrupt();
        recorder_fetcher_thread_.join();
    }

    BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_recorder_close(), {}, {
        BROOKESIA_LOGE("Failed to close recorder");
    });
    is_encoder_started_ = false;

    BROOKESIA_LOGI("Encoder stopped");
}

bool Audio::start_decoder(const AudioDecoderDynamicConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_decoder_started()) {
        BROOKESIA_LOGD("Decoder is already running");
        return true;
    }

    BROOKESIA_LOGI(
        "Start decoder with static config:\n%1% \n and dynamic config:\n%2%",
        BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(decoder_static_config_, BROOKESIA_DESCRIBE_FORMAT_VERBOSE),
        BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(config, BROOKESIA_DESCRIBE_FORMAT_VERBOSE)
    );

    auto feeder_config = TypeConverter::convert(decoder_static_config_, config);

    BROOKESIA_CHECK_ESP_ERR_RETURN(audio_feeder_open(&feeder_config), false, "Failed to open feeder");
    lib_utils::FunctionGuard close_feeder_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_feeder_close(), {}, {
            BROOKESIA_LOGE("Failed to close feeder");
        });
    });

    BROOKESIA_CHECK_ESP_ERR_RETURN(audio_processor_mixer_open(), false, "Failed to open mixer");
    lib_utils::FunctionGuard close_mixer_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_processor_mixer_close(), {}, {
            BROOKESIA_LOGE("Failed to close mixer");
        });
    });

    BROOKESIA_CHECK_ESP_ERR_RETURN(audio_feeder_run(), false, "Failed to run feeder");

    close_feeder_guard.release();
    close_mixer_guard.release();

    is_decoder_started_ = true;

    BROOKESIA_LOGI("Decoder started");

    return true;
}

void Audio::stop_decoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_decoder_started()) {
        BROOKESIA_LOGD("Decoder is not running");
        return;
    }

    // Feeder must be closed before mixer
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_feeder_close(), {}, {
        BROOKESIA_LOGE("Failed to close feeder");
    });
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_processor_mixer_close(), {}, {
        BROOKESIA_LOGE("Failed to close mixer");
    });
    is_decoder_started_ = false;

    BROOKESIA_LOGI("Decoder stopped");
}

void Audio::on_playback_event(uint8_t state)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: state(%1%)", state);

    audio_player_state_t state_enum = static_cast<audio_player_state_t>(state);
    AudioPlayState new_state;
    switch (state_enum) {
    case AUDIO_PLAYER_STATE_IDLE:
        new_state = AudioPlayState::Idle;
        break;
    case AUDIO_PLAYER_STATE_PLAYING:
        new_state = AudioPlayState::Playing;
        break;
    case AUDIO_PLAYER_STATE_PAUSED:
        new_state = AudioPlayState::Paused;
        break;
    case AUDIO_PLAYER_STATE_FINISHED:
    case AUDIO_PLAYER_STATE_STOPPED:
        new_state = AudioPlayState::Idle;
        break;
    default:
        BROOKESIA_LOGE("Invalid playback state: %1%", state_enum);
        return;
    }

    if (new_state != play_state_) {
        // Check loop state before updating play_state_
        bool loop_active = false;
        bool is_loop_middle_iteration = false;
        {
            std::lock_guard<std::mutex> lock(loop_state_mutex_);
            loop_active = loop_state_.is_active;
            // Check if we're in the middle of a loop (not the first iteration entering Playing,
            // and not the last iteration exiting to Idle)
            if (loop_active && loop_state_.current_iteration > 0) {
                is_loop_middle_iteration = true;
            }
        }

        play_state_ = new_state;
        BROOKESIA_LOGI("Play state changed to: %1%", BROOKESIA_DESCRIBE_TO_STR(play_state_));

        // Only publish event if not in the middle of loop playback
        // For loop: publish Playing at start, publish Idle only when loop completely finishes
        bool should_publish_event = true;
        if (loop_active) {
            if (new_state == AudioPlayState::Playing && is_loop_middle_iteration) {
                // Don't publish Playing event for subsequent loop iterations
                should_publish_event = false;
            } else if (new_state == AudioPlayState::Idle) {
                // Don't publish Idle event during loop, will be published when loop finishes
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

        // Handle completion events
        if (new_state == AudioPlayState::Idle) {
            if (loop_active) {
                // Handle loop playback continuation
                handle_loop_playback_complete();
            } else if (is_processing_queue_) {
                // Process next item in queue when playback finishes
                process_play_url_queue();
            }
        }
    }
}

void Audio::on_recorder_event(void *event)
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // BROOKESIA_LOGD("Params: event(%1%)", BROOKESIA_DESCRIBE_TO_STR(event));

    auto afe_evt = static_cast<esp_gmf_afe_evt_t *>(event);
    BROOKESIA_CHECK_NULL_EXIT(afe_evt, "AFE event is null");

    Helper::AFE_Event afe_event = Helper::AFE_Event::Max;
    switch (afe_evt->type) {
    case ESP_GMF_AFE_EVT_WAKEUP_START: {
        BROOKESIA_LOGI("wakeup start");
        afe_event = Helper::AFE_Event::WakeStart;
        break;
    }
    case ESP_GMF_AFE_EVT_WAKEUP_END:
        BROOKESIA_LOGI("wakeup end");
        afe_event = Helper::AFE_Event::WakeEnd;
        break;
    case ESP_GMF_AFE_EVT_VAD_START:
        BROOKESIA_LOGI("vad start");
        afe_event = Helper::AFE_Event::VAD_Start;
        break;
    case ESP_GMF_AFE_EVT_VAD_END:
        BROOKESIA_LOGI("vad end");
        afe_event = Helper::AFE_Event::VAD_End;
        break;
    case ESP_GMF_AFE_EVT_VCMD_DECT_TIMEOUT:
        BROOKESIA_LOGI("vcmd detect timeout");
        afe_event = Helper::AFE_Event::Max;
        break;
    default:
        break;
    }

    if (afe_event == Helper::AFE_Event::Max) {
        BROOKESIA_LOGW("Unsupported afe event: %1%", afe_evt->type);
        return;
    }

    auto result = publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::AFE_EventHappened), {
        BROOKESIA_DESCRIBE_TO_STR(afe_event)
    });
    BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to publish afe event happened event");
}

void Audio::on_recorder_input_data(const uint8_t *data, size_t size)
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // BROOKESIA_LOGD("Params: data(%1%), size(%2%)", BROOKESIA_DESCRIBE_TO_STR(data), size);

    auto result = publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::RecorderDataReady), {
        RawBuffer(data, size)
    });
    BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to publish recorder data ready event");
}

#if BROOKESIA_SERVICE_AUDIO_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    ServiceBase, Audio, Audio::get_instance().get_attributes().name, Audio::get_instance(),
    BROOKESIA_SERVICE_AUDIO_PLUGIN_SYMBOL
);
#endif // BROOKESIA_SERVICE_AUDIO_ENABLE_AUTO_REGISTER

} // namespace esp_brookesia::service
