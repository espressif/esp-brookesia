/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cmath>
#include <chrono>
#include <algorithm>
#include <string>
#include "boost/format.hpp"
#include "boost/thread.hpp"
#include "esp_bit_defs.h"
#include "esp_gmf_afe.h"
#include "model_path.h"
#include "brookesia/service_audio/macro_configs.h"
#if !BROOKESIA_SERVICE_AUDIO_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/type_converter.hpp"
#include "audio_processor.h"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/service_manager/macro_configs.h"
#include "brookesia/service_helper/nvs.hpp"
#include "brookesia/hal_interface/audio/codec_player.hpp"
#include "brookesia/hal_interface/audio/codec_recorder.hpp"
#include "brookesia/hal_interface/device.hpp"
#include "brookesia/service_audio/service_audio.hpp"

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

inline hal::AudioCodecPlayerIface::Config get_player_config()
{
    hal::AudioCodecPlayerIface::Config config{
#if CONFIG_AUDIO_SIMPLE_PLAYER_BIT_CVT_DEST_24BIT
        .bits = 24,
#elif CONFIG_AUDIO_SIMPLE_PLAYER_BIT_CVT_DEST_32BIT
        .bits = 32,
#else
        .bits = 16,
#endif
        .channels = CONFIG_AUDIO_SIMPLE_PLAYER_CH_CVT_DEST,
        .sample_rate = CONFIG_AUDIO_SIMPLE_PLAYER_RESAMPLE_DEST_RATE,
    };
    return config;
}

template<typename T>
void try_save_data(const std::string &nvs_namespace, Audio::DataType type, const T &data)
{
    if (!NVSHelper::is_available()) {
        return;
    }

    auto key = BROOKESIA_DESCRIBE_TO_STR(type);
    auto result = NVSHelper::save_key_value(nvs_namespace, key, data);
    if (!result) {
        BROOKESIA_LOGW("Failed to save '%1%' to NVS: %2%", key, result.error());
    } else {
        BROOKESIA_LOGD("Saved '%1%' to NVS: %2%", key, data);
    }
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

    auto [player_name, player_iface] = hal::get_first_interface<hal::AudioCodecPlayerIface>();
    BROOKESIA_CHECK_NULL_RETURN(player_iface, false, "Failed to get audio player interface");
    BROOKESIA_CHECK_FALSE_RETURN(player_iface->open(get_player_config()), false, "Failed to open audio dac");
    lib_utils::FunctionGuard close_player_guard([this, player_iface]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        player_iface->close();
    });

    auto [recorder_name, recorder_iface] = hal::get_first_interface<hal::AudioCodecRecorderIface>();
    BROOKESIA_CHECK_NULL_RETURN(recorder_iface, false, "Failed to get audio recorder interface");
    BROOKESIA_CHECK_FALSE_RETURN(recorder_iface->open(), false, "Failed to open audio adc");
    lib_utils::FunctionGuard close_recorder_guard([this, recorder_iface]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        recorder_iface->close();
    });

    auto &recorder_info = recorder_iface->get_info();
    // Initialize audio manager
    auto player_write_cb = +[](uint8_t *data, int size, void *ctx) {
        BROOKESIA_LOG_TRACE_GUARD();

        auto player_iface = static_cast<hal::AudioCodecPlayerIface *>(ctx);
        BROOKESIA_CHECK_NULL_RETURN(player_iface, ESP_FAIL, "Invalid context");

        return player_iface->write_data(data, size) ? ESP_OK : ESP_FAIL;
    };
    auto recorder_read_cb = +[](uint8_t *data, int size, void *ctx) {
        BROOKESIA_LOG_TRACE_GUARD();

        auto recorder_iface = static_cast<hal::AudioCodecRecorderIface *>(ctx);
        BROOKESIA_CHECK_NULL_RETURN(recorder_iface, ESP_FAIL, "Invalid context");

        auto result = recorder_iface->read_data(data, size);
        if (result) {
            Audio::get_instance().on_recorder_input_data(data, size);
        }

        return result ? ESP_OK : ESP_FAIL;
    };
    audio_manager_config_t manager_config = {
        .play_io = {
            .read_cb = nullptr,
            .read_ctx = nullptr,
            .write_cb = player_write_cb,
            .write_ctx = player_iface.get(),
        },
        .rec_io = {
            .read_cb = recorder_read_cb,
            .read_ctx = recorder_iface.get(),
            .write_cb = nullptr,
            .write_ctx = nullptr,
        },
        .mic_layout = {0},
        .board_sample_rate = static_cast<int>(recorder_info.sample_rate),
        .board_bits = static_cast<int>(recorder_info.bits),
        .board_channels = static_cast<int>(recorder_info.channels),
    };
    // Copy mic_layout string safely
    if (!recorder_info.mic_layout.empty()) {
        std::strncpy(
            manager_config.mic_layout, recorder_info.mic_layout.c_str(), sizeof(manager_config.mic_layout) - 1
        );
        manager_config.mic_layout[sizeof(manager_config.mic_layout) - 1] = '\0';
    }
    BROOKESIA_CHECK_ESP_ERR_RETURN(audio_manager_init(&manager_config), false, "Failed to initialize audio manager");

    // Open playback
    auto playback_event_cb = +[](audio_play_state_t state, void *ctx) {
        auto self = static_cast<Audio *>(ctx);
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");
        self->on_playback_event(state);
    };
    auto playback_config = TypeConverter::convert(
                               playback_config_, reinterpret_cast<const void *>(playback_event_cb), this
                           );
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        audio_playback_open(&playback_config, reinterpret_cast<audio_playback_handle_t *>(&playback_handle_)),
        false, "Failed to open playback"
    );

    play_state_ = AudioPlayState::Idle;
    player_iface_ = player_iface;
    recorder_iface_ = recorder_iface;

    close_player_guard.release();
    close_recorder_guard.release();

    // Try to load data from NVS
    try_load_data();

    return true;
}

void Audio::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Clear play URL queue and playlist state
    std::queue<PlayUrlRequest>().swap(play_url_queue_);
    is_processing_queue_ = false;

    cancel_playlist_scheduled_task();

    BROOKESIA_CHECK_ESP_ERR_EXECUTE(
    audio_playback_close(TypeConverter::to_playback_handle(playback_handle_)), {}, {
        BROOKESIA_LOGE("Failed to close playback");
    });
    playback_handle_ = nullptr;

    stop_encoder();
    stop_decoder();

    BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_manager_deinit(), {}, {
        BROOKESIA_LOGE("Failed to deinitialize audio manager");
    });

    if (player_iface_) {
        player_iface_->close();
    }
    if (recorder_iface_) {
        recorder_iface_->close();
    }

    player_iface_.reset();
    recorder_iface_.reset();
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

std::expected<boost::json::array, std::string> Audio::function_get_afe_wake_words()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!afe_config_.wakenet.has_value()) {
        return boost::json::array();
    }

    auto partition_label = afe_config_.wakenet.value().model_partition_label;
    if (partition_label.empty()) {
        return boost::json::array();
    }

    std::vector<std::string> wake_words_array;
    auto get_wake_words = [&wake_words_array, partition_label]() {
        auto models = esp_srmodel_init(partition_label.c_str());
        BROOKESIA_CHECK_NULL_EXIT(models, "Failed to get models");

        for (size_t i = 0; i < models->num; i++) {
            auto wake_words_str = esp_srmodel_get_wake_words(models, models->model_name[i]);
            if (!wake_words_str) {
                BROOKESIA_LOGE("Failed to get wake words for model: %1%", models->model_name[i]);
                continue;
            }

            BROOKESIA_LOGD("Wake words for model: %1%: %2%", models->model_name[i], wake_words_str);

            // Split wake words by semicolon if multiple wake words exist in one model
            std::string wake_words(wake_words_str);
            free(wake_words_str);

            // Split by semicolon and add each wake word to the array
            size_t pos = 0;
            while ((pos = wake_words.find(';')) != std::string::npos) {
                std::string wake_word = wake_words.substr(0, pos);
                // Trim whitespace
                wake_word.erase(0, wake_word.find_first_not_of(" \t"));
                wake_word.erase(wake_word.find_last_not_of(" \t") + 1);
                if (!wake_word.empty()) {
                    wake_words_array.push_back(wake_word);
                }
                wake_words.erase(0, pos + 1);
            }
            // Add the last wake word (or the only one if no semicolon found)
            wake_words.erase(0, wake_words.find_first_not_of(" \t"));
            wake_words.erase(wake_words.find_last_not_of(" \t") + 1);
            if (!wake_words.empty()) {
                wake_words_array.push_back(wake_words);
            }
        }
    };

    auto thrread_config = BROOKESIA_THREAD_GET_CURRENT_CONFIG();
    if (!thrread_config.stack_in_ext || (get_static_srmodels() != nullptr)) {
        BROOKESIA_LOGD("Getting wake words in current thread");
        get_wake_words();
    } else {
        BROOKESIA_LOGD("Getting wake words in new thread");
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_in_ext = false,
        });
        boost::thread(get_wake_words).join();
    }

    return BROOKESIA_DESCRIBE_TO_JSON(wake_words_array).as_array();
}

std::expected<void, std::string> Audio::function_pause_afe_wakeup_end()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_afe_wakeup_end_task_running()) {
        BROOKESIA_LOGD("Not running, skip");
        return {};
    }

    if (!pause_afe_wakeup_end_task()) {
        return std::unexpected("Pause afe wakeup end task failed");
    }

    return {};
}

std::expected<void, std::string> Audio::function_resume_afe_wakeup_end()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_afe_wakeup_end_task_running()) {
        BROOKESIA_LOGD("Not running, skip");
        return {};
    }

    if (!update_afe_wakeup_end_task()) {
        return std::unexpected("Update afe wakeup end task failed");
    }

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
        std::queue<PlayUrlRequest>().swap(play_url_queue_);
        is_processing_queue_ = false;

        cancel_playlist_scheduled_task();
        start_playlist({url}, play_url_config);
    } else {
        play_url_queue_.push({{url}, play_url_config});
        is_processing_queue_ = true;
        BROOKESIA_LOGD("Added URL to queue, queue size: %1%", play_url_queue_.size());

        if (play_state_ == AudioPlayState::Idle && !playlist_state_.is_active) {
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
        std::queue<PlayUrlRequest>().swap(play_url_queue_);
        is_processing_queue_ = false;

        cancel_playlist_scheduled_task();
        start_playlist(std::move(url_list), play_url_config);
    } else {
        play_url_queue_.push({std::move(url_list), play_url_config});
        is_processing_queue_ = true;
        BROOKESIA_LOGD("Queued playlist request, queue size: %1%", play_url_queue_.size());

        if (play_state_ == AudioPlayState::Idle && !playlist_state_.is_active) {
            process_play_url_queue();
        }
    }

    return {};
}

void Audio::start_playlist(std::vector<std::string> urls, const AudioPlayUrlConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    uint32_t total_loops = (config.loop_count == 0) ? 1 : config.loop_count;
    uint32_t new_session_id = 0;

    playlist_state_.is_active = false;
    playlist_state_.urls = std::move(urls);
    playlist_state_.config = config;
    playlist_state_.current_url_index = 0;
    playlist_state_.current_loop = 0;
    playlist_state_.total_loops = total_loops;
    playlist_state_.scheduled_task_id = 0;
    playlist_state_.pause_start_ms = 0;
    playlist_state_.paused_duration_ms = 0;
    playlist_state_.start_time_ms = 0;
    new_session_id = ++playlist_session_counter_;
    playlist_state_.session_id = new_session_id;

    BROOKESIA_LOGD("Starting playlist: %1% URLs x %2% loops, session %3%", urls.size(), total_loops, new_session_id);

    play_playlist_url_at_index(0);
}

void Audio::play_playlist_url_at_index(size_t index)
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
        delay_ms = (current_loop == 0)
                   ? playlist_state_.config.delay_ms
                   : playlist_state_.config.loop_interval_ms;
    }

    size_t url_count = playlist_state_.urls.size();

    auto do_play = [this, url, session_id, index, current_loop, total_loops, url_count]() {
        if (playlist_state_.session_id != session_id) {
            BROOKESIA_LOGD("Playlist playback cancelled (session %1% vs current %2%)",
                           session_id, playlist_state_.session_id);
            return;
        }
        if (index == 0 && current_loop == 0) {
            playlist_state_.start_time_ms = get_current_time_ms();
        }
        playlist_state_.is_active = true;
        playlist_state_.scheduled_task_id = 0;

        BROOKESIA_LOGI(
            "Playing [loop %1%/%2%, url %3%/%4%]: %5%", current_loop + 1, total_loops, index + 1, url_count, url
        );

        if (audio_playback_play(TypeConverter::to_playback_handle(playback_handle_), url.c_str()) != ESP_OK) {
            BROOKESIA_LOGE("Failed to play URL: %1%", url);
            playlist_state_.is_active = false;
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

void Audio::advance_playlist()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    bool play_next_url = false;
    bool start_next_loop = false;
    bool playlist_finished = false;
    size_t next_url_index = 0;

    if (!playlist_state_.is_active) {
        return;
    }

    playlist_state_.current_url_index++;

    if (playlist_state_.current_url_index < playlist_state_.urls.size()) {
        play_next_url = true;
        next_url_index = playlist_state_.current_url_index;
    } else {
        // Current loop iteration complete
        playlist_state_.current_loop++;

        // Check timeout
        if (playlist_state_.config.timeout_ms > 0) {
            int64_t total_elapsed = get_current_time_ms() - playlist_state_.start_time_ms;
            int64_t effective_elapsed = total_elapsed - playlist_state_.paused_duration_ms;
            if (effective_elapsed >= static_cast<int64_t>(playlist_state_.config.timeout_ms)) {
                BROOKESIA_LOGI("Playlist timeout after %1% ms (effective)", effective_elapsed);
                playlist_state_.is_active = false;
                playlist_finished = true;
            }
        }

        if (!playlist_finished) {
            bool more_loops = (playlist_state_.total_loops == UINT32_MAX) ||
                              (playlist_state_.current_loop < playlist_state_.total_loops);
            if (more_loops) {
                playlist_state_.current_url_index = 0;
                start_next_loop = true;
                BROOKESIA_LOGD("Loop %1%/%2% complete, starting next",
                               playlist_state_.current_loop, playlist_state_.total_loops);
            } else {
                playlist_state_.is_active = false;
                playlist_finished = true;
                BROOKESIA_LOGD("Playlist completed all %1% loops", playlist_state_.total_loops);
            }
        }
    }

    if (play_next_url) {
        play_playlist_url_at_index(next_url_index);
    } else if (start_next_loop) {
        // Publish Idle at loop boundary before starting next loop
        auto idle_result = publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::PlayStateChanged), {
            BROOKESIA_DESCRIBE_TO_STR(AudioPlayState::Idle)
        });
        BROOKESIA_CHECK_FALSE_EXECUTE(idle_result, {}, {
            BROOKESIA_LOGE("Failed to publish play state changed event");
        });
        play_playlist_url_at_index(0);
    } else if (playlist_finished) {
        auto result = publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::PlayStateChanged), {
            BROOKESIA_DESCRIBE_TO_STR(AudioPlayState::Idle)
        });
        BROOKESIA_CHECK_FALSE_EXECUTE(result, {}, {
            BROOKESIA_LOGE("Failed to publish play state changed event");
        });

        if (is_processing_queue_) {
            process_play_url_queue();
        }
    }
}

void Audio::cancel_playlist_scheduled_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    lib_utils::TaskScheduler::TaskId task_id = 0;
    task_id = playlist_state_.scheduled_task_id;
    playlist_state_.scheduled_task_id = 0;
    playlist_state_.is_active = false;

    if (task_id != 0) {
        auto scheduler = get_task_scheduler();
        if (scheduler) {
            scheduler->cancel(task_id);
            BROOKESIA_LOGD("Cancelled playlist scheduled task: %1%", task_id);
        }
    }
}

void Audio::suspend_playlist_scheduled_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    lib_utils::TaskScheduler::TaskId task_id = 0;
    task_id = playlist_state_.scheduled_task_id;
    if (playlist_state_.is_active && playlist_state_.pause_start_ms == 0) {
        playlist_state_.pause_start_ms = get_current_time_ms();
        BROOKESIA_LOGD("Playlist timeout paused at %1% ms", playlist_state_.pause_start_ms);
    }

    if (task_id != 0) {
        auto scheduler = get_task_scheduler();
        if (scheduler) {
            if (scheduler->suspend(task_id)) {
                BROOKESIA_LOGD("Suspended playlist scheduled task: %1%", task_id);
            }
        }
    }
}

void Audio::resume_playlist_scheduled_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    lib_utils::TaskScheduler::TaskId task_id = 0;
    task_id = playlist_state_.scheduled_task_id;
    if (playlist_state_.is_active && playlist_state_.pause_start_ms != 0) {
        int64_t current_time_ms = get_current_time_ms();
        int64_t pause_duration = current_time_ms - playlist_state_.pause_start_ms;
        playlist_state_.paused_duration_ms += pause_duration;
        playlist_state_.pause_start_ms = 0;
        BROOKESIA_LOGD("Playlist timeout resumed, paused for %1% ms, total paused: %2% ms",
                       pause_duration, playlist_state_.paused_duration_ms);
    }

    if (task_id != 0) {
        auto scheduler = get_task_scheduler();
        if (scheduler) {
            if (scheduler->resume(task_id)) {
                BROOKESIA_LOGD("Resumed playlist scheduled task: %1%", task_id);
            }
        }
    }
}

void Audio::process_play_url_queue()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    PlayUrlRequest request;
    if (play_url_queue_.empty()) {
        BROOKESIA_LOGD("Play URL queue is empty");
        is_processing_queue_ = false;
        return;
    }
    request = std::move(play_url_queue_.front());
    play_url_queue_.pop();
    is_processing_queue_ = true;
    BROOKESIA_LOGD("Processing request from queue, remaining: %1%", play_url_queue_.size());

    start_playlist(std::move(request.urls), request.config);
}

std::expected<void, std::string> Audio::function_play_control(const std::string &action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: action(%1%)", action);

    AudioPlayControlAction action_enum;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM(action, action_enum)) {
        return std::unexpected("Invalid action: " + action);
    }

    auto playback_handle = TypeConverter::to_playback_handle(playback_handle_);

    switch (action_enum) {
    case AudioPlayControlAction::Pause:
        if (audio_playback_pause(playback_handle) != ESP_OK) {
            return std::unexpected("Failed to pause playback");
        }
        suspend_playlist_scheduled_task();
        break;
    case AudioPlayControlAction::Resume:
        if (audio_playback_resume(playback_handle) != ESP_OK) {
            return std::unexpected("Failed to resume playback");
        }
        resume_playlist_scheduled_task();
        break;
    case AudioPlayControlAction::Stop:
        cancel_playlist_scheduled_task();
        if (audio_playback_stop(playback_handle) != ESP_OK) {
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

    if (!player_iface_) {
        return std::unexpected("Player interface is not available");
    }

    uint8_t old_volume = 0;
    if (!player_iface_->get_volume(old_volume)) {
        return std::unexpected("Failed to get volume");
    }

    if (old_volume == static_cast<uint8_t>(volume)) {
        BROOKESIA_LOGD("Volume not changed, skip");
        return {};
    }

    if (!player_iface_->set_volume(static_cast<uint8_t>(volume))) {
        return std::unexpected("Failed to set volume");
    }

    uint8_t new_volume = 0;
    if (!player_iface_->get_volume(new_volume)) {
        return std::unexpected("Failed to get volume");
    }

    if (old_volume != new_volume) {
        try_save_data(get_attributes().name, DataType::PlayerVolume, new_volume);
    }

    return {};
}

std::expected<double, std::string> Audio::function_get_volume()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!player_iface_) {
        return std::unexpected("Player interface is not available");
    }

    uint8_t volume = 0;
    if (!player_iface_->get_volume(volume)) {
        return std::unexpected("Failed to get volume");
    }

    return static_cast<double>(volume);
}

std::expected<void, std::string> Audio::function_set_mute(bool enable)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: enable(%1%)", enable);

    if (!player_iface_) {
        return std::unexpected("Player interface is not available");
    }

    if (enable) {
        if (!player_iface_->mute()) {
            return std::unexpected("Failed to mute player");
        }
    } else {
        if (!player_iface_->unmute()) {
            return std::unexpected("Failed to unmute player");
        }
    }

    return {};
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

std::expected<void, std::string> Audio::function_pause_encoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    set_encoder_paused(true);

    return {};
}

std::expected<void, std::string> Audio::function_resume_encoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    set_encoder_paused(false);

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

    auto feeder_handle = TypeConverter::to_feeder_handle(feeder_handle_);
    auto result = audio_feeder_feed_data(feeder_handle, const_cast<uint8_t *>(data.data_ptr), data.data_size);
    if (result != ESP_OK) {
        return std::unexpected((boost::format("Failed to feed decoder data: %1%") % esp_err_to_name(result)).str());
    }

    return {};
}

std::expected<void, std::string> Audio::function_reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    try_erase_data();

    if (player_iface_) {
        auto &player_info = player_iface_->get_info();
        BROOKESIA_CHECK_FALSE_EXECUTE(player_iface_->set_volume(static_cast<uint8_t>(player_info.volume_default)), {}, {
            BROOKESIA_LOGE("Failed to reset player volume");
        });
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
        auto result = NVSHelper::get_key_value<uint8_t>(nvs_namespace, key);
        if (!result) {
            BROOKESIA_LOGW("Failed to load '%1%' from NVS: %2%", key, result.error());
            if (player_iface_) {
                BROOKESIA_CHECK_FALSE_EXECUTE(player_iface_->unmute(), {}, {
                    BROOKESIA_LOGE("Failed to unmute player");
                });
            }
        } else {
            auto &volume = result.value();
            BROOKESIA_LOGD("Loaded '%1%' from NVS: %2%", key, volume);
            BROOKESIA_CHECK_FALSE_EXECUTE(player_iface_->set_volume(volume), {}, {
                BROOKESIA_LOGE("Failed to set player volume");
            });
        }
    }

    is_data_loaded_ = true;

    BROOKESIA_LOGI("Loaded all data from NVS");
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

    BROOKESIA_LOGD(
        "Start encoder with AFE config:\n%1% \n and static config:\n%2% \n and dynamic config:\n%3%",
        BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(afe_config_, BROOKESIA_DESCRIBE_FORMAT_VERBOSE),
        BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(encoder_static_config_, BROOKESIA_DESCRIBE_FORMAT_VERBOSE),
        BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(config, BROOKESIA_DESCRIBE_FORMAT_VERBOSE)
    );

    auto event_cb = +[](audio_recorder_handle_t recorder, void *event, void *ctx) {
        // BROOKESIA_LOG_TRACE_GUARD();
        auto self = static_cast<Audio *>(ctx);
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");
        self->on_recorder_event(event);
    };
    auto recorder_config = TypeConverter::convert(
                               encoder_static_config_, config, afe_config_, afe_model_partition_label_,
                               afe_mn_language_, reinterpret_cast<const void *>(event_cb), this
                           );
    auto *recorder_handle_ptr = reinterpret_cast<audio_recorder_handle_t *>(&recorder_handle_);

#if (BROOKESIA_SERVICE_AUDIO_ENABLE_WORKER && !BROOKESIA_SERVICE_AUDIO_WORKER_STACK_IN_EXT) || \
    !BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        audio_recorder_open(&recorder_config, recorder_handle_ptr), false, "Failed to open recorder"
    );
#else
    {
        // Since initializing SR in `audio_recorder_open()` operates on flash,
        // a separate thread with its stack located in SRAM needs to be created to prevent a crash.
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_in_ext = false,
        });
        auto recorder_open_future =
        std::async(std::launch::async, [this, recorder_config_ptr = &recorder_config, recorder_handle_ptr]() mutable {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            BROOKESIA_CHECK_ESP_ERR_RETURN(
                audio_recorder_open(recorder_config_ptr, recorder_handle_ptr), false, "Failed to open recorder"
            );
            return true;
        });
        BROOKESIA_CHECK_FALSE_RETURN(recorder_open_future.get(), false, "Failed to open recorder");
    }
#endif

    auto recorder_handle = TypeConverter::to_recorder_handle(recorder_handle_);
    auto close_recorder_safely = [this, recorder_handle]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
#if (BROOKESIA_SERVICE_AUDIO_ENABLE_WORKER && !BROOKESIA_SERVICE_AUDIO_WORKER_STACK_IN_EXT) || \
    !BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT
        BROOKESIA_CHECK_ESP_ERR_RETURN(audio_recorder_close(recorder_handle), false, "Failed to close recorder");
#else
        auto current_thread_config = BROOKESIA_THREAD_GET_CURRENT_CONFIG();
        if (!current_thread_config.stack_in_ext) {
            BROOKESIA_CHECK_ESP_ERR_RETURN(audio_recorder_close(recorder_handle), false, "Failed to close recorder");
        } else {
            // Closing the recorder unmaps SR models from flash, so use an internal-memory stack.
            BROOKESIA_THREAD_CONFIG_GUARD({
                .stack_in_ext = false,
            });
            auto recorder_close_future = std::async(std::launch::async, [this, recorder_handle]() mutable {
                BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
                BROOKESIA_CHECK_ESP_ERR_RETURN(audio_recorder_close(recorder_handle), false, "Failed to close recorder");
                return true;
            });
            BROOKESIA_CHECK_FALSE_RETURN(recorder_close_future.get(), false, "Failed to close recorder");
        }
#endif
        return true;
    };
    lib_utils::FunctionGuard close_recorder_guard([this, close_recorder_safely]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_CHECK_FALSE_EXIT(close_recorder_safely(), "Failed to close recorder");
    });

    auto recorder_fetch_thread_func =
    [this, recorder_handle, fetch_data_size = config.fetch_data_size, fetch_interval_ms = config.fetch_interval_ms]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD("recorder fetch thread started (fetch data size: %1%)", fetch_data_size);

        std::unique_ptr<uint8_t[]> data(new (std::nothrow) uint8_t[fetch_data_size]);
        BROOKESIA_CHECK_NULL_EXIT(data, "Failed to allocate memory");

        int ret_size = 0;
        try {
            while (!boost::this_thread::interruption_requested()) {
                if (!is_encoder_paused()) {
                    ret_size = audio_recorder_read_data(recorder_handle, data.get(), fetch_data_size);
                }
                // BROOKESIA_LOGD("Reading data from recorder (%1%)", ret_size);
                if (ret_size > 0) {
                    BROOKESIA_CHECK_FALSE_EXIT(
                    publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::EncoderDataReady), {
                        RawBuffer(static_cast<const uint8_t *>(data.get()), ret_size)
                    }), "Failed to publish recorder data ready event"
                    );
                    ret_size = 0;
                } else {
                    boost::this_thread::sleep_for(boost::chrono::milliseconds(fetch_interval_ms));
                }
            }
        } catch (const boost::thread_interrupted &e) {
            BROOKESIA_LOGW("recorder fetch thread interrupted");
        }

        BROOKESIA_LOGD("recorder fetch thread stopped");
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

    auto recorder_handle = TypeConverter::to_recorder_handle(recorder_handle_);
#if (BROOKESIA_SERVICE_AUDIO_ENABLE_WORKER && !BROOKESIA_SERVICE_AUDIO_WORKER_STACK_IN_EXT) || \
    !BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_recorder_close(recorder_handle), {}, {
        BROOKESIA_LOGE("Failed to close recorder");
    });
#else
    {
        // Since deinitializing SR in `audio_recorder_close()` operates on flash,
        // a separate thread with its stack located in SRAM needs to be created to prevent a crash.
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_in_ext = false,
        });
        auto recorder_close_future = std::async(std::launch::async, [this, recorder_handle]() mutable {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_recorder_close(recorder_handle), {}, {
                BROOKESIA_LOGE("Failed to close recorder");
            });
            return true;
        });
        recorder_close_future.get();
    }
#endif

    recorder_handle_ = nullptr;
    is_encoder_started_ = false;

    set_encoder_paused(false);

    BROOKESIA_LOGI("Encoder stopped");
}

void Audio::set_encoder_paused(bool paused)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: paused(%1%)", BROOKESIA_DESCRIBE_TO_STR(paused));

    is_encoder_paused_ = paused;
}

bool Audio::start_decoder(const AudioDecoderDynamicConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_decoder_started()) {
        BROOKESIA_LOGD("Decoder is already running");
        return true;
    }

    BROOKESIA_LOGD(
        "Start decoder with static config:\n%1% \n and dynamic config:\n%2%",
        BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(decoder_static_config_, BROOKESIA_DESCRIBE_FORMAT_VERBOSE),
        BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(config, BROOKESIA_DESCRIBE_FORMAT_VERBOSE)
    );

    auto feeder_config = TypeConverter::convert(decoder_static_config_, config);

    BROOKESIA_CHECK_ESP_ERR_RETURN(audio_feeder_open(
                                       &feeder_config, reinterpret_cast<audio_feeder_handle_t *>(&feeder_handle_)), false, "Failed to open feeder"
                                  );
    auto feeder_handle = TypeConverter::to_feeder_handle(feeder_handle_);
    lib_utils::FunctionGuard close_feeder_guard([this, feeder_handle]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_feeder_close(feeder_handle), {}, {
            BROOKESIA_LOGE("Failed to close feeder");
        });
    });

    auto playback_handle = TypeConverter::to_playback_handle(playback_handle_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        audio_processor_mixer_open(playback_handle, feeder_handle), false, "Failed to open mixer"
    );
    lib_utils::FunctionGuard close_mixer_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_processor_mixer_close(), {}, {
            BROOKESIA_LOGE("Failed to close mixer");
        });
    });

    BROOKESIA_CHECK_ESP_ERR_RETURN(audio_feeder_start(feeder_handle), false, "Failed to start feeder");

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
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_feeder_close(TypeConverter::to_feeder_handle(feeder_handle_)), {}, {
        BROOKESIA_LOGE("Failed to close feeder");
    });
    feeder_handle_ = nullptr;
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

    audio_play_state_t state_enum = static_cast<audio_play_state_t>(state);
    AudioPlayState new_state;
    switch (state_enum) {
    case AUDIO_PLAY_STATE_IDLE:
        new_state = AudioPlayState::Idle;
        break;
    case AUDIO_PLAY_STATE_PLAYING:
        new_state = AudioPlayState::Playing;
        break;
    case AUDIO_PLAY_STATE_PAUSED:
        new_state = AudioPlayState::Paused;
        break;
    case AUDIO_PLAY_STATE_FINISHED:
    case AUDIO_PLAY_STATE_STOPPED:
        new_state = AudioPlayState::Idle;
        break;
    default:
        BROOKESIA_LOGE("Invalid playback state: %1%", state_enum);
        return;
    }

    if (new_state == play_state_) {
        BROOKESIA_LOGD("Play state not changed, skip");
        return;
    }

    auto task_func = [this, new_state]() {
        bool playlist_active = false;
        bool is_first_url_of_loop = false;
        {
            playlist_active = playlist_state_.is_active;
            is_first_url_of_loop = (playlist_state_.current_url_index == 0);
        }

        play_state_ = new_state;
        BROOKESIA_LOGD("Play state changed to: %1%", BROOKESIA_DESCRIBE_TO_STR(play_state_));

        // Event suppression: publish Playing at loop start, suppress Idle (advance_playlist decides)
        bool should_publish_event = true;
        if (playlist_active) {
            if (new_state == AudioPlayState::Playing && !is_first_url_of_loop) {
                should_publish_event = false;
            } else if (new_state == AudioPlayState::Idle) {
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
            if (playlist_active) {
                advance_playlist();
            } else if (is_processing_queue_) {
                process_play_url_queue();
            }
        }
    };
    auto task_scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_EXIT(task_scheduler, "Task scheduler is not available");
    auto result = task_scheduler->post(task_func, nullptr, get_call_task_group());
    BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to post play state changed task");
}

void Audio::on_recorder_event(void *event)
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // BROOKESIA_LOGD("Params: event(%1%)", BROOKESIA_DESCRIBE_TO_STR(event));

    auto afe_evt = static_cast<esp_gmf_afe_evt_t *>(event);
    BROOKESIA_CHECK_NULL_EXIT(afe_evt, "AFE event is null");

    lib_utils::TaskScheduler::OnceTask task_func = nullptr;

    Helper::AFE_Event afe_event = Helper::AFE_Event::Max;
    switch (afe_evt->type) {
    case ESP_GMF_AFE_EVT_WAKEUP_START: {
        BROOKESIA_LOGD("AFE wakeup start");
        afe_event = Helper::AFE_Event::WakeStart;
        task_func = [this]() {
            // Start the afe wakeup end task with the `start_timeout_ms` timeout
            BROOKESIA_CHECK_FALSE_EXIT(
                start_afe_wakeup_end_task(afe_config_.wakenet->start_timeout_ms), "Failed to start afe wakeup end task"
            );
        };
        break;
    }
    case ESP_GMF_AFE_EVT_VAD_START:
        BROOKESIA_LOGD("AFE VAD start");
        afe_event = Helper::AFE_Event::VAD_Start;
        task_func = [this]() {
            if (is_afe_wakeup_end_task_running()) {
                BROOKESIA_CHECK_FALSE_EXIT(pause_afe_wakeup_end_task(), "Failed to pause afe wakeup end task");
            }
        };
        break;
    case ESP_GMF_AFE_EVT_VAD_END:
        BROOKESIA_LOGD("AFE VAD end");
        afe_event = Helper::AFE_Event::VAD_End;
        task_func = [this]() {
            if (is_afe_wakeup_end_task_running()) {
                if (!is_afe_vad_detected_) {
                    // Detect the first VAD event after wakeup start, start the afe wakeup end task with the
                    // `end_timeout_ms` timeout
                    BROOKESIA_CHECK_FALSE_EXIT(
                        start_afe_wakeup_end_task(afe_config_.wakenet->end_timeout_ms),
                        "Failed to start afe wakeup end task"
                    );
                    is_afe_vad_detected_ = true;
                } else {
                    BROOKESIA_CHECK_FALSE_EXIT(update_afe_wakeup_end_task(), "Failed to update afe wakeup end task");
                }
            }
        };
        break;
    case ESP_GMF_AFE_EVT_VCMD_DECTECTED: {
        BROOKESIA_LOGD("AFE VCMD detected");
#if !AV_PROCESSOR_AFE_USE_CUSTOM
        afe_event = Helper::AFE_Event::Max;
#else
        // In the current mode, wakeup is triggered by the this event, not the start event, so handle it as a start event.
        // And since no end event is supported, the task function is not needed.
        afe_event = Helper::AFE_Event::WakeStart;
#endif
        break;
    }
    case ESP_GMF_AFE_EVT_VCMD_DECT_TIMEOUT:
        BROOKESIA_LOGD("AFE VCMD detection timeout");
        afe_event = Helper::AFE_Event::Max;
        break;
    default:
        break;
    }

    if (afe_event == Helper::AFE_Event::Max) {
        BROOKESIA_LOGD("Unsupported AFE event: %1%", afe_evt->type);
        return;
    }

    auto result = publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::AFE_EventHappened), {
        BROOKESIA_DESCRIBE_TO_STR(afe_event)
    });
    BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to publish afe event happened event");

    if (task_func) {
        auto task_scheduler = get_task_scheduler();
        BROOKESIA_CHECK_NULL_EXIT(task_scheduler, "Task scheduler is not available");
        auto result = task_scheduler->post(task_func, nullptr, get_call_task_group());
        BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to post afe event task");
    }
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

bool Audio::start_afe_wakeup_end_task(uint32_t timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_afe_wakeup_end_task_running()) {
        BROOKESIA_LOGD("Already running, stop it first");
        BROOKESIA_CHECK_FALSE_RETURN(stop_afe_wakeup_end_task(), false, "Failed to stop afe wakeup end task");
    }

    auto task_scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(task_scheduler, false, "Task scheduler is not available");

    auto task_func = [this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD("AFE wakeup end");

        auto result = publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::AFE_EventHappened), {
            BROOKESIA_DESCRIBE_TO_STR(Helper::AFE_Event::WakeEnd)
        });
        BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to publish afe event happened event");
        BROOKESIA_CHECK_FALSE_EXIT(stop_afe_wakeup_end_task(), "Failed to stop afe wakeup end task");
    };
    auto result = task_scheduler->post_delayed(
                      task_func, timeout_ms, &afe_wakeup_end_task_, get_call_task_group()
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to post delayed task");

    is_afe_vad_detected_ = false;
    BROOKESIA_LOGD("AFE wakeup end task created (timeout: %1%ms)", timeout_ms);

    return true;
}

bool Audio::stop_afe_wakeup_end_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_afe_wakeup_end_task_running()) {
        BROOKESIA_LOGD("Not running, skip");
        return true;
    }

    auto task_scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(task_scheduler, false, "Task scheduler is not available");

    task_scheduler->cancel(afe_wakeup_end_task_);
    afe_wakeup_end_task_ = 0;

    return true;
}

bool Audio::pause_afe_wakeup_end_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_afe_wakeup_end_task_running()) {
        BROOKESIA_LOGD("Not running, skip");
        return true;
    }

    auto task_scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(task_scheduler, false, "Task scheduler is not available");

    if (task_scheduler->get_state(afe_wakeup_end_task_) != lib_utils::TaskScheduler::TaskState::Running) {
        BROOKESIA_LOGD("Task is not running, skip");
        return true;
    }

    auto result = task_scheduler->suspend(afe_wakeup_end_task_);
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to suspend afe wakeup end task");

    BROOKESIA_LOGD("AFE wakeup end task paused");

    return true;
}

bool Audio::update_afe_wakeup_end_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_afe_wakeup_end_task_running()) {
        BROOKESIA_LOGD("Not running, skip");
        return true;
    }

    auto task_scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(task_scheduler, false, "Task scheduler is not available");

    if (task_scheduler->get_state(afe_wakeup_end_task_) == lib_utils::TaskScheduler::TaskState::Suspended) {
        task_scheduler->resume(afe_wakeup_end_task_);
    }
    task_scheduler->restart_timer(afe_wakeup_end_task_);

    BROOKESIA_LOGD("AFE wakeup end task updated (timeout: %1%ms)", afe_config_.wakenet->end_timeout_ms);

    return true;
}

#if BROOKESIA_SERVICE_AUDIO_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    ServiceBase, Audio, Audio::get_instance().get_attributes().name, Audio::get_instance(),
    BROOKESIA_SERVICE_AUDIO_PLUGIN_SYMBOL
);
#endif // BROOKESIA_SERVICE_AUDIO_ENABLE_AUTO_REGISTER

} // namespace esp_brookesia::service
