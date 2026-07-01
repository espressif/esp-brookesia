/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include "esp_log.h"
#define BROOKESIA_LOG_TAG "Main"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper.hpp"
#include "brookesia/service_audio/service_audio.hpp"
#include "brookesia/hal_interface.hpp"
#include "brookesia/hal_adaptor.hpp"

using namespace esp_brookesia;
using AudioHelper = service::helper::Audio;
using AudioPlaybackHelper = service::helper::AudioPlayback;
using AudioEncoderHelper = service::helper::AudioEncoder<0>;
using AudioDecoderHelper = service::helper::AudioDecoder<0>;
using PlaybackStateEventMonitor =
    AudioPlaybackHelper::EventMonitor<AudioPlaybackHelper::EventId::PlayStateChanged>;

constexpr uint32_t PLAYBACK_WAIT_EVENT_TIMEOUT_MS = 30000;

static AudioHelper::CodecGeneralConfig codec_general_config{
#if CONFIG_IDF_TARGET_ESP32C5
    // Codec general parameters (8kHz mono 16-bit, 60ms frame)
    .channels = 1,
    .sample_bits = 16,
    .sample_rate = 8000,
    .frame_duration = 60,
#else
    // Codec general parameters (16kHz mono 16-bit, 60ms frame)
    .channels = 1,
    .sample_bits = 16,
    .sample_rate = 16000,
    .frame_duration = 60,
#endif
};

static std::string get_audio_file_path(const std::string &filename);

static bool configure_audio_processor();
static bool demo_play_single_url();
static bool demo_play_multiple_urls();
static bool demo_play_control();
static bool demo_codec(AudioHelper::CodecFormat codec_format);
static bool demo_afe();
static bool wait_for_play_state(
    PlaybackStateEventMonitor &monitor, AudioHelper::PlayState state, uint32_t timeout_ms
);

extern "C" void app_main(void)
{
    BROOKESIA_LOGI("\n\n=== Audio Service Example ===\n");

    // Set log level to warning for some components to reduce log output
    esp_log_level_set("ESP_GMF_TASK", ESP_LOG_WARN);
    esp_log_level_set("ESP_GMF_FILE", ESP_LOG_WARN);
    esp_log_level_set("ESP_GMF_PORT", ESP_LOG_WARN);
    esp_log_level_set("ESP_GMF_POOL", ESP_LOG_WARN);
    esp_log_level_set("ESP_ES_PARSER", ESP_LOG_WARN);
    esp_log_level_set("GMF_AFE", ESP_LOG_WARN);
    esp_log_level_set("ESP_GMF_AENC", ESP_LOG_WARN);
    esp_log_level_set("AFE_MANAGER", ESP_LOG_WARN);
    esp_log_level_set("AUD_RENDER", ESP_LOG_WARN);
    esp_log_level_set("AUDIO_PROCESSOR", ESP_LOG_WARN);
    esp_log_level_set("AUD_SIMP_PLAYER", ESP_LOG_WARN);
    esp_log_level_set("AUD_SDEC", ESP_LOG_WARN);
    esp_log_level_set("AFE_CONFIG", ESP_LOG_WARN);
    esp_log_level_set("NEW_DATA_BUS", ESP_LOG_WARN);

    BROOKESIA_CHECK_FALSE_EXIT(configure_audio_processor(), "Failed to configure audio processor");

    // Initialize ServiceManager
    auto &service_manager = service::ServiceManager::get_instance();
    BROOKESIA_CHECK_FALSE_EXIT(service_manager.start(), "Failed to start ServiceManager");

    // Check and bind split Audio services. RAII keeps the services alive while the bindings exist.
    BROOKESIA_CHECK_FALSE_EXIT(AudioPlaybackHelper::is_available(), "Audio playback service is not available");
    BROOKESIA_CHECK_FALSE_EXIT(AudioEncoderHelper::is_available(), "Audio encoder service is not available");
    BROOKESIA_CHECK_FALSE_EXIT(AudioDecoderHelper::is_available(), "Audio decoder service is not available");
    auto playback_binding = service_manager.bind(AudioPlaybackHelper::get_name().data());
    auto encoder_binding = service_manager.bind(AudioEncoderHelper::get_name().data());
    auto decoder_binding = service_manager.bind(AudioDecoderHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_EXIT(playback_binding.is_valid(), "Failed to bind audio playback service");
    BROOKESIA_CHECK_FALSE_EXIT(encoder_binding.is_valid(), "Failed to bind audio encoder service");
    BROOKESIA_CHECK_FALSE_EXIT(decoder_binding.is_valid(), "Failed to bind audio decoder service");

    // Play a single URL
    BROOKESIA_CHECK_FALSE_EXIT(demo_play_single_url(), "Failed to play single URL");
    // Play multiple URLs
    BROOKESIA_CHECK_FALSE_EXIT(demo_play_multiple_urls(), "Failed to play multiple URLs");
    // Play control (pause/resume/stop)
    BROOKESIA_CHECK_FALSE_EXIT(demo_play_control(), "Failed to demo play control");
    // Encode -> Decode loopback
    BROOKESIA_CHECK_FALSE_EXIT(demo_codec(AudioHelper::CodecFormat::PCM), "Failed to demo encode -> decode (PCM)");
    BROOKESIA_CHECK_FALSE_EXIT(demo_codec(AudioHelper::CodecFormat::OPUS), "Failed to demo encode -> decode (OPUS)");
#if !CONFIG_IDF_TARGET_ESP32C5
    BROOKESIA_CHECK_FALSE_EXIT(demo_codec(AudioHelper::CodecFormat::G711A), "Failed to demo encode -> decode (G711A)");
#endif
    // AFE
    BROOKESIA_CHECK_FALSE_EXIT(demo_afe(), "Failed to demo AFE");

    BROOKESIA_LOGI("\n\n=== Audio Service Example Completed ===\n");
}

static bool configure_audio_processor()
{
    BROOKESIA_LOGI("\n\n--- Configure Audio Processor ---\n");

    hal::AudioProcessorConfig processor_config{
        .playback = {
            .player_task = {
                .core_id = 0,
                .priority = 5,
                .stack_size = 4 * 1024,
                // When using PSRAM but not using XIP, set stack_in_ext to true to prevent crashes caused by tasks
                // operating on Flash.
#if CONFIG_SPIRAM_XIP_FROM_PSRAM
                .stack_in_ext = true,
#else
                .stack_in_ext = false,
#endif
            },
        },
        .encoder = {},
        .decoder = {},
        .afe = {
            .vad = hal::AudioProcessorAFE_VAD_Config{},
            .wakenet = hal::AudioProcessorAFE_WakeNetConfig{},
        },
    };

    BROOKESIA_CHECK_FALSE_RETURN(
        hal::AudioDevice::get_instance().set_processor_config(std::move(processor_config)), false,
        "Failed to configure audio processor"
    );
    BROOKESIA_LOGI("\n\n--- Configure Audio Processor Completed ---\n");

    return true;
}

static std::string get_audio_file_path(const std::string &filename)
{
    auto fs_iface = hal::acquire_first_interface<hal::storage::FileSystemIface>();
    BROOKESIA_CHECK_NULL_RETURN(fs_iface, filename, "Failed to get storage file system interface");

    auto infos = fs_iface->get_all_info();
    for (const auto &info : infos) {
        if (info.fs_type == hal::storage::FileSystemIface::FileSystemType::LittleFS) {
            return "file:/" + std::string(info.mount_point) + "/" + filename;
        }
    }
    return filename;
}

static bool demo_play_single_url()
{
    BROOKESIA_LOGI("\n\n--- Demo: Play Single URL ---\n");

    PlaybackStateEventMonitor play_state_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(play_state_monitor.start(), false, "Failed to monitor play state changed event");

    // Play single URL
    play_state_monitor.clear();
    auto first_result = AudioPlaybackHelper::call_function_sync(
                            AudioPlaybackHelper::FunctionId::Play, get_audio_file_path("1.mp3")
                        );
    BROOKESIA_CHECK_FALSE_RETURN(first_result, false, "Failed to play single URL: %1%", first_result.error());
    BROOKESIA_CHECK_FALSE_RETURN(
        wait_for_play_state(play_state_monitor, AudioHelper::PlayState::Playing, PLAYBACK_WAIT_EVENT_TIMEOUT_MS),
        false, "Failed to wait for the first playback to start"
    );

    // Play single URL with configuration
    // It will not interrupt the current playback, and will be added to the playback queue and played after the current
    // playback completes.
    // But it will be interrupted by the third time playback.
    AudioHelper::PlayUrlConfig config{
        .interrupt = false,
        .loop_count = 5,
    };
    auto second_result = AudioPlaybackHelper::call_function_sync(
                             AudioPlaybackHelper::FunctionId::Play, get_audio_file_path("2.mp3"),
                             BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
                         );
    BROOKESIA_CHECK_FALSE_RETURN(
        second_result, false, "Failed to play single URL with configuration: %1%", second_result.error()
    );

    // Play single URL again and interrupt the previous playback
    play_state_monitor.clear();
    auto third_result = AudioPlaybackHelper::call_function_sync(
                            AudioPlaybackHelper::FunctionId::Play, get_audio_file_path("3.mp3")
                        );
    BROOKESIA_CHECK_FALSE_RETURN(third_result, false, "Failed to play single URL: %1%", third_result.error());
    BROOKESIA_CHECK_FALSE_RETURN(
        wait_for_play_state(play_state_monitor, AudioHelper::PlayState::Playing, PLAYBACK_WAIT_EVENT_TIMEOUT_MS),
        false, "Failed to wait for interrupted playback to start"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        !play_state_monitor.has({BROOKESIA_DESCRIBE_TO_STR(AudioHelper::PlayState::Idle)}),
        false, "Unexpected transient Idle state during immediate interrupt"
    );
    play_state_monitor.clear();
    BROOKESIA_CHECK_FALSE_RETURN(
        wait_for_play_state(play_state_monitor, AudioHelper::PlayState::Idle, PLAYBACK_WAIT_EVENT_TIMEOUT_MS),
        false, "Failed to wait for interrupted playback to finish"
    );

    BROOKESIA_LOGI("\n\n--- Demo: Play Single URL Completed ---\n");

    return true;
}

static bool demo_play_multiple_urls()
{
    BROOKESIA_LOGI("\n\n--- Demo: Play Multiple URLs ---\n");

    PlaybackStateEventMonitor play_state_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(play_state_monitor.start(), false, "Failed to monitor play state changed event");

    // Play the first time with a short clip so the delayed interrupt below creates a visible Idle gap.
    std::vector<std::string> first_urls{
        get_audio_file_path("0.mp3"),
    };
    play_state_monitor.clear();
    auto first_result = AudioPlaybackHelper::call_function_sync(
                            AudioPlaybackHelper::FunctionId::PlayUrls, BROOKESIA_DESCRIBE_TO_JSON(first_urls).as_array()
                        );
    BROOKESIA_CHECK_FALSE_RETURN(
        first_result, false, "Failed to play multiple URLs for first time: %1%", first_result.error()
    );
    if (!first_result) {
        BROOKESIA_LOGE("Failed to play multiple URLs for first time: %1%", first_result.error());
        return false;
    }
    BROOKESIA_CHECK_FALSE_RETURN(
        wait_for_play_state(play_state_monitor, AudioHelper::PlayState::Playing, PLAYBACK_WAIT_EVENT_TIMEOUT_MS),
        false, "Failed to wait for the first playlist to start"
    );

    // Then play the second time with a delayed interrupt. The first clip should finish, publish Idle for the
    // real gap, then the replacement playlist should start.
    std::vector<std::string> second_urls{
        get_audio_file_path("5.mp3"),
        get_audio_file_path("6.mp3"),
    };
    AudioHelper::PlayUrlConfig second_config{
        .interrupt = true,
        .delay_ms = 2000,
        .loop_count = 2,
        .loop_interval_ms = 1000,
        .timeout_ms = 0
    };
    play_state_monitor.clear();
    auto second_result = AudioPlaybackHelper::call_function_sync(
                             AudioPlaybackHelper::FunctionId::PlayUrls,
                             BROOKESIA_DESCRIBE_TO_JSON(second_urls).as_array(),
                             BROOKESIA_DESCRIBE_TO_JSON(second_config).as_object()
                         );
    BROOKESIA_CHECK_FALSE_RETURN(
        second_result, false, "Failed to play multiple URLs for second time: %1%", second_result.error()
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        wait_for_play_state(play_state_monitor, AudioHelper::PlayState::Idle, PLAYBACK_WAIT_EVENT_TIMEOUT_MS),
        false, "Failed to wait for the real Idle gap before the second playlist"
    );
    play_state_monitor.clear();
    BROOKESIA_CHECK_FALSE_RETURN(
        wait_for_play_state(play_state_monitor, AudioHelper::PlayState::Playing, PLAYBACK_WAIT_EVENT_TIMEOUT_MS),
        false, "Failed to wait for the second playlist to start"
    );
    play_state_monitor.clear();
    BROOKESIA_CHECK_FALSE_RETURN(
        wait_for_play_state(play_state_monitor, AudioHelper::PlayState::Idle, PLAYBACK_WAIT_EVENT_TIMEOUT_MS),
        false, "Failed to wait for the second playlist to finish"
    );

    BROOKESIA_LOGI("\n\n--- Demo: Play Multiple URLs Completed ---\n");

    return true;
}

static bool demo_play_control()
{
    BROOKESIA_LOGI("\n\n--- Demo: Play Control ---\n");

    PlaybackStateEventMonitor play_state_monitor;
    BROOKESIA_CHECK_FALSE_RETURN(play_state_monitor.start(), false, "Failed to monitor play state changed event");

    // Start playing a longer file
    AudioHelper::PlayUrlConfig config{
        .loop_count = 10
    };
    play_state_monitor.clear();
    auto play_result = AudioPlaybackHelper::call_function_sync(
                           AudioPlaybackHelper::FunctionId::Play, get_audio_file_path("7.mp3"),
                           BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
                       );
    BROOKESIA_CHECK_FALSE_RETURN(play_result, false, "Failed to start playback: %1%", play_result.error());
    BROOKESIA_CHECK_FALSE_RETURN(
        wait_for_play_state(play_state_monitor, AudioHelper::PlayState::Playing, PLAYBACK_WAIT_EVENT_TIMEOUT_MS),
        false, "Failed to wait for playback to start"
    );

    BROOKESIA_LOGI("Pausing playback...");
    play_state_monitor.clear();
    auto pause_result = AudioPlaybackHelper::call_function_sync(AudioPlaybackHelper::FunctionId::Pause);
    BROOKESIA_CHECK_FALSE_RETURN(pause_result, false, "Failed to pause: %1%", pause_result.error());
    BROOKESIA_CHECK_FALSE_RETURN(
        wait_for_play_state(play_state_monitor, AudioHelper::PlayState::Paused, PLAYBACK_WAIT_EVENT_TIMEOUT_MS),
        false, "Failed to wait for playback to pause"
    );

    // Resume playback
    BROOKESIA_LOGI("Resuming playback...");
    play_state_monitor.clear();
    auto resume_result = AudioPlaybackHelper::call_function_sync(AudioPlaybackHelper::FunctionId::Resume);
    BROOKESIA_CHECK_FALSE_RETURN(resume_result, false, "Failed to resume: %1%", resume_result.error());
    BROOKESIA_CHECK_FALSE_RETURN(
        wait_for_play_state(play_state_monitor, AudioHelper::PlayState::Playing, PLAYBACK_WAIT_EVENT_TIMEOUT_MS),
        false, "Failed to wait for playback to resume"
    );

    BROOKESIA_LOGI("Stopping playback...");
    play_state_monitor.clear();
    auto stop_result = AudioPlaybackHelper::call_function_sync(AudioPlaybackHelper::FunctionId::Stop);
    BROOKESIA_CHECK_FALSE_RETURN(stop_result, false, "Failed to stop: %1%", stop_result.error());
    BROOKESIA_CHECK_FALSE_RETURN(
        wait_for_play_state(play_state_monitor, AudioHelper::PlayState::Idle, PLAYBACK_WAIT_EVENT_TIMEOUT_MS),
        false, "Failed to wait for playback to stop"
    );

    BROOKESIA_LOGI("\n\n--- Demo: Play Control Completed ---\n");

    return true;
}

static bool demo_codec(AudioHelper::CodecFormat codec_format)
{
    BROOKESIA_LOGI("\n\n--- Demo: Encode -> Decode (Format: %1%) ---\n", BROOKESIA_DESCRIBE_TO_STR(codec_format));

    size_t run_time_s = 10;
    size_t decoder_feed_data_size = 1024;

    // Collect encoded data from encoder events
    std::vector<uint8_t> recorded_data;
    auto *encoder = service::AudioEncoder::get_instance(0);
    BROOKESIA_CHECK_NULL_RETURN(encoder, false, "AudioEncoder0 instance is not available");
    auto on_encoder_data_ready = [&](service::RawBuffer buf) {
        if (buf.data_ptr && buf.data_size > 0) {
            BROOKESIA_LOGD("[Direct] %1%", buf);
            recorded_data.insert(recorded_data.end(), buf.data_ptr, buf.data_ptr + buf.data_size);
            // Update decoder feed data size
            decoder_feed_data_size = std::min(decoder_feed_data_size, buf.data_size);
        }
    };
    // RAII - the scoped connection auto-disconnects when `encoder_connection` goes out of scope.
    // A plain esp_brookesia::lib_utils::connection does NOT disconnect on destruction, which would leave the
    // slot (and its dangling capture of `recorded_data`) connected across later demos.
    esp_brookesia::lib_utils::scoped_connection encoder_connection = encoder->connect_encoded_data(on_encoder_data_ready);
    BROOKESIA_CHECK_FALSE_RETURN(encoder_connection.connected(), false, "Failed to connect encoder data signal");

    auto timeout = service::helper::Timeout(1000);

    // Start encoder
    AudioHelper::EncoderDynamicConfig encoder_config{
        .type = codec_format,
        .general = codec_general_config,
    };
    if (codec_format == AudioHelper::CodecFormat::OPUS) {
        encoder_config.extra = AudioHelper::EncoderExtraConfigOpus{
            .enable_vbr = false,
            .bitrate = 24000,
        };
    }
    auto enc_result = AudioEncoderHelper::call_function_sync(
                          AudioEncoderHelper::FunctionId::Start,
                          BROOKESIA_DESCRIBE_TO_JSON(encoder_config).as_object(), timeout
                      );
    BROOKESIA_CHECK_FALSE_RETURN(enc_result, false, "Failed to start encoder: %1%", enc_result.error());
    BROOKESIA_LOGI("Encoder started, collecting data for %1% seconds...", run_time_s);
    BROOKESIA_LOGI("Please speak or make noise to record audio...");

    boost::this_thread::sleep_for(boost::chrono::seconds(run_time_s));

    // Stop encoder
    auto stop_enc_result = AudioEncoderHelper::call_function_sync(AudioEncoderHelper::FunctionId::Stop, timeout);
    BROOKESIA_CHECK_FALSE_RETURN(stop_enc_result, false, "Failed to stop encoder: %1%", stop_enc_result.error());

    size_t total_size = recorded_data.size();
    BROOKESIA_LOGI("Encoder stopped, recorded %1% bytes", total_size);

    if (total_size == 0) {
        BROOKESIA_LOGW("No data recorded, skipping decoder playback");
        return true;
    }

    // Start decoder
    auto *decoder = service::AudioDecoder::get_instance(0);
    BROOKESIA_CHECK_NULL_RETURN(decoder, false, "AudioDecoder0 instance is not available");
    constexpr const char *source_name = "AudioExample";
    constexpr const char *output_name = "Speaker0";
    std::mutex decoder_drain_mutex;
    std::condition_variable decoder_drain_cv;
    bool decoder_stream_drained = false;
    auto on_decoder_stream_drained =
    [&](const std::string & drained_source_name, const std::string & drained_output_name) {
        if ((drained_source_name != source_name) || (drained_output_name != output_name)) {
            return;
        }
        {
            std::lock_guard lock(decoder_drain_mutex);
            decoder_stream_drained = true;
        }
        decoder_drain_cv.notify_all();
        BROOKESIA_LOGI("Decoder stream drained: source=%1%, output=%2%", drained_source_name, drained_output_name);
    };
    esp_brookesia::lib_utils::scoped_connection decoder_drain_connection =
        decoder->connect_stream_drained(on_decoder_stream_drained);
    BROOKESIA_CHECK_FALSE_RETURN(decoder_drain_connection.connected(), false, "Failed to connect decoder drain signal");

    auto unregister_result = decoder->unregister_source(source_name);
    if (!unregister_result) {
        BROOKESIA_LOGD("Decoder source was not registered before start: %1%", unregister_result.error());
    }
    service::AudioSourceInfo source_info = {
        .name = source_name,
        .role = "example",
        .preferred_outputs = {output_name},
    };
    auto register_result = decoder->register_source(std::move(source_info));
    BROOKESIA_CHECK_FALSE_RETURN(
        register_result, false, "Failed to register decoder source: %1%", register_result.error()
    );
    auto request_result = decoder->request_output(register_result.value(), output_name);
    BROOKESIA_CHECK_FALSE_RETURN(
        request_result, false, "Failed to request decoder output: %1%", request_result.error()
    );
    auto active_result = decoder->set_active_source(output_name, source_name);
    BROOKESIA_CHECK_FALSE_RETURN(
        active_result, false, "Failed to set active decoder source: %1%", active_result.error()
    );

    service::AudioStreamConfig decoder_config{
        .type = codec_format,
        .general = codec_general_config,
    };
    auto dec_result = decoder->open_stream(register_result.value(), output_name, decoder_config);
    BROOKESIA_CHECK_FALSE_RETURN(dec_result, false, "Failed to start decoder: %1%", dec_result.error());
    BROOKESIA_LOGI("Decoder started, feeding %1% bytes...", total_size);
    BROOKESIA_LOGI("Playing recorded audio...");

    // Feed recorded data to decoder in chunks
    for (size_t offset = 0; offset < recorded_data.size(); offset += decoder_feed_data_size) {
        size_t chunk_size = std::min(decoder_feed_data_size, recorded_data.size() - offset);
        service::RawBuffer chunk(recorded_data.data() + offset, chunk_size);
        BROOKESIA_LOGD("Feeding data(%1%) to decoder...", chunk);
        auto feed_result = decoder->write_stream(register_result.value(), output_name, chunk, 1000);
        BROOKESIA_CHECK_FALSE_RETURN(
            feed_result == service::AudioWriteResult::Written, false, "Failed to feed decoder data: %1%",
            BROOKESIA_DESCRIBE_TO_STR(feed_result)
        );
    }

    BROOKESIA_LOGI("All data fed to decoder");

    BROOKESIA_LOGI("Waiting for decoder stream to drain...");
    {
        std::unique_lock<std::mutex> lock(decoder_drain_mutex);
        decoder_stream_drained = decoder->is_stream_drained(register_result.value(), output_name);
        auto drained = decoder_drain_cv.wait_for(
        lock, std::chrono::milliseconds(PLAYBACK_WAIT_EVENT_TIMEOUT_MS), [&decoder_stream_drained]() {
            return decoder_stream_drained;
        }
                       );
        BROOKESIA_CHECK_FALSE_RETURN(drained, false, "Timed out waiting for decoder stream to drain");
    }

    // Stop decoder
    auto stop_dec_result = decoder->close_stream(register_result.value(), output_name);
    BROOKESIA_CHECK_FALSE_RETURN(stop_dec_result, false, "Failed to stop decoder: %1%", stop_dec_result.error());
    auto release_result = decoder->release_output(register_result.value(), output_name);
    BROOKESIA_CHECK_FALSE_RETURN(release_result, false, "Failed to release decoder output: %1%", release_result.error());
    unregister_result = decoder->unregister_source(register_result.value());
    BROOKESIA_CHECK_FALSE_RETURN(unregister_result, false, "Failed to unregister decoder source: %1%", unregister_result.error());

    BROOKESIA_LOGI(
        "\n\n--- Demo: Encode -> Decode (Format: %1%) Completed ---\n", BROOKESIA_DESCRIBE_TO_STR(codec_format)
    );

    return true;
}

static bool demo_afe()
{
    BROOKESIA_LOGI("\n\n--- Demo: AFE (Audio Front-End) ---\n");

    size_t monitor_time_s = 120;

    std::atomic<uint32_t> afe_event_count = 0;
    auto on_afe_event_happened = [&afe_event_count](const std::string & event_name, const std::string & afe_event) {
        auto count = ++afe_event_count;
        BROOKESIA_LOGI("AFE event received: name=%1%, event=%2%, count=%3%", event_name, afe_event, count);
    };
    auto afe_event_connection = AudioEncoderHelper::subscribe_event(
                                    AudioEncoderHelper::EventId::AFEEventHappened, on_afe_event_happened
                                );
    BROOKESIA_CHECK_FALSE_RETURN(afe_event_connection.connected(), false, "Failed to subscribe AFE event");

    // Get wakeup words
    auto get_wake_words_result = AudioEncoderHelper::call_function_sync<boost::json::array>(
                                     AudioEncoderHelper::FunctionId::GetAFEWakeWords
                                 );
    BROOKESIA_CHECK_FALSE_RETURN(
        get_wake_words_result, false, "Failed to get wakeup words: %1%", get_wake_words_result.error()
    );
    if (get_wake_words_result.value().empty()) {
        BROOKESIA_LOGW("No wakeup words detected, skipping demo");
        return true;
    }
    BROOKESIA_LOGI("Detected wakeup words: %1%", get_wake_words_result.value());

    // Start encoder to enable AFE processing
    BROOKESIA_LOGI("Start encoder to enable AFE processing...");
    AudioHelper::EncoderDynamicConfig encoder_config{
        .type = AudioHelper::CodecFormat::PCM,
        .general = codec_general_config,
        .enable_afe = true,
    };
    auto timeout = service::helper::Timeout(1000);
    auto start_enc_result = AudioEncoderHelper::call_function_sync(
                                AudioEncoderHelper::FunctionId::Start,
                                BROOKESIA_DESCRIBE_TO_JSON(encoder_config).as_object(), timeout
                            );
    BROOKESIA_CHECK_FALSE_RETURN(start_enc_result, false, "Failed to start encoder: %1%", start_enc_result.error());
    BROOKESIA_LOGI("Encoder started, AFE is now processing audio...");
    BROOKESIA_LOGI(
        "Please speak wakeup words, then make noise to trigger VAD events (wakeup words: %1%)",
        get_wake_words_result.value()
    );

    // Monitor AFE events for a period of time
    BROOKESIA_LOGI("Monitoring AFE events for %1% seconds...", monitor_time_s);
    boost::this_thread::sleep_for(boost::chrono::seconds(monitor_time_s));
    BROOKESIA_LOGI("AFE event count: %1%", afe_event_count.load());

    // Stop encoder
    BROOKESIA_LOGI("Stopping encoder...");
    auto stop_enc_result = AudioEncoderHelper::call_function_sync(AudioEncoderHelper::FunctionId::Stop, timeout);
    BROOKESIA_CHECK_FALSE_RETURN(stop_enc_result, false, "Failed to stop encoder: %1%", stop_enc_result.error());

    BROOKESIA_LOGI("\n\n--- Demo: AFE Completed ---\n");

    return true;
}

static bool wait_for_play_state(
    PlaybackStateEventMonitor &monitor, AudioHelper::PlayState state, uint32_t timeout_ms
)
{
    const auto state_str = BROOKESIA_DESCRIBE_TO_STR(state);
    auto got_state = monitor.wait_for(std::vector<service::EventItem> {
        state_str
    }, timeout_ms);
    if (got_state) {
        BROOKESIA_LOGI("[Monitor] Play state changed: %1%", state_str);
    }
    return got_state;
}
