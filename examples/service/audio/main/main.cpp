/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <vector>
#include <string>
#include "esp_log.h"
#define BROOKESIA_LOG_TAG "Main"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper.hpp"
#include "brookesia/hal_interface.hpp"
#include "brookesia/hal_adaptor.hpp"

using namespace esp_brookesia;
using AudioHelper = esp_brookesia::service::helper::Audio;

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

static bool demo_set_configs();
static bool demo_play_single_url();
static bool demo_play_multiple_urls();
static bool demo_play_control();
static bool demo_codec(AudioHelper::CodecFormat codec_format);
static bool demo_afe();

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

    // Initialize required devices from HAL adaptor
    BROOKESIA_CHECK_FALSE_EXIT(hal::init_device(hal::StorageDevice::DEVICE_NAME), "Failed to initialize storage device");
    BROOKESIA_CHECK_FALSE_EXIT(hal::init_device(hal::AudioDevice::DEVICE_NAME), "Failed to initialize audio device");

    // Initialize ServiceManager
    auto &service_manager = service::ServiceManager::get_instance();
    BROOKESIA_CHECK_FALSE_EXIT(service_manager.start(), "Failed to start ServiceManager");

    // Check if Audio service is available
    BROOKESIA_CHECK_FALSE_EXIT(AudioHelper::is_available(), "Audio service is not available");

    // Should set configs before starting the service
    BROOKESIA_CHECK_FALSE_EXIT(demo_set_configs(), "Failed to demo set configs");

    // Bind Audio service to start it and its dependencies (RAII - service stays alive while binding exists)
    auto binding = service_manager.bind(AudioHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_EXIT(binding.is_valid(), "Failed to bind Audio service");

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

static bool demo_set_configs()
{
    BROOKESIA_LOGI("\n\n--- Demo: Set Configs ---\n");

    AudioHelper::PlaybackConfig playback_config{
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
        }
    };
    auto playback_result = AudioHelper::call_function_sync(
                               AudioHelper::FunctionId::SetPlaybackConfig, BROOKESIA_DESCRIBE_TO_JSON(playback_config).as_object()
                           );
    BROOKESIA_CHECK_FALSE_RETURN(playback_result, false, "Failed to set playback config: %1%", playback_result.error());

    AudioHelper::EncoderStaticConfig encoder_static_config{};
    auto encoder_static_result = AudioHelper::call_function_sync(
                                     AudioHelper::FunctionId::SetEncoderStaticConfig, BROOKESIA_DESCRIBE_TO_JSON(encoder_static_config).as_object()
                                 );
    BROOKESIA_CHECK_FALSE_RETURN(
        encoder_static_result, false, "Failed to set encoder static config: %1%", encoder_static_result.error()
    );

    AudioHelper::DecoderStaticConfig decoder_static_config{};
    auto decoder_static_result = AudioHelper::call_function_sync(
                                     AudioHelper::FunctionId::SetDecoderStaticConfig, BROOKESIA_DESCRIBE_TO_JSON(decoder_static_config).as_object()
                                 );
    BROOKESIA_CHECK_FALSE_RETURN(
        decoder_static_result, false, "Failed to set decoder static config: %1%", decoder_static_result.error()
    );

    BROOKESIA_LOGI("\n\n--- Demo: Set Configs Completed ---\n");

    return true;
}

static std::string get_audio_file_path(const std::string &filename)
{
    auto [name, fs_iface] = hal::get_first_interface<hal::StorageFsIface>();
    BROOKESIA_CHECK_NULL_RETURN(fs_iface, filename, "Failed to get storage file system interface");

    auto infos = fs_iface->get_all_info();
    for (const auto &info : infos) {
        if (info.fs_type == hal::StorageFsIface::FileSystemType::SPIFFS) {
            return "file:/" + std::string(info.mount_point) + "/" + filename;
        }
    }
    return filename;
}

static bool demo_play_single_url()
{
    BROOKESIA_LOGI("\n\n--- Demo: Play Single URL ---\n");

    // Subscribe play state changed event
    auto on_play_state_changed = [](const std::string & event_name, const std::string & play_state) {
        BROOKESIA_LOGI("[Event: %1%] %2%", event_name, play_state);
    };
    // RAII - connection stays alive while `player_connection` exists
    auto player_connection = AudioHelper::subscribe_event(AudioHelper::EventId::PlayStateChanged, on_play_state_changed);
    BROOKESIA_CHECK_FALSE_RETURN(player_connection.connected(), false, "Failed to subscribe play state changed event");

    // Play single URL
    auto first_result = AudioHelper::call_function_sync(AudioHelper::FunctionId::PlayUrl, get_audio_file_path("1.mp3"));
    BROOKESIA_CHECK_FALSE_RETURN(first_result, false, "Failed to play single URL: %1%", first_result.error());

    // Play single URL with configuration
    // It will not interrupt the current playback, and will be added to the playback queue and played after the current
    // playback completes.
    // But it will be interrupted by the third time playback.
    AudioHelper::PlayUrlConfig config{
        .interrupt = false,
        .loop_count = 5,
    };
    auto second_result = AudioHelper::call_function_sync(
                             AudioHelper::FunctionId::PlayUrl, get_audio_file_path("2.mp3"),
                             BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
                         );
    BROOKESIA_CHECK_FALSE_RETURN(
        second_result, false, "Failed to play single URL with configuration: %1%", second_result.error()
    );

    boost::this_thread::sleep_for(boost::chrono::seconds(3));

    // Play single URL again and interrupt the previous playback
    auto third_result = AudioHelper::call_function_sync(AudioHelper::FunctionId::PlayUrl, get_audio_file_path("3.mp3"));
    BROOKESIA_CHECK_FALSE_RETURN(third_result, false, "Failed to play single URL: %1%", third_result.error());

    boost::this_thread::sleep_for(boost::chrono::seconds(3));

    BROOKESIA_LOGI("\n\n--- Demo: Play Single URL Completed ---\n");

    return true;
}

static bool demo_play_multiple_urls()
{
    BROOKESIA_LOGI("\n\n--- Demo: Play Multiple URLs ---\n");

    // Subscribe play state changed event
    auto on_play_state_changed = [](const std::string & event_name, const std::string & play_state) {
        BROOKESIA_LOGI("[Event: %1%] %2%", event_name, play_state);
    };
    // RAII - connection stays alive while `player_connection` exists
    auto player_connection = AudioHelper::subscribe_event(AudioHelper::EventId::PlayStateChanged, on_play_state_changed);
    BROOKESIA_CHECK_FALSE_RETURN(player_connection.connected(), false, "Failed to subscribe play state changed event");

    // Play the first time
    // The `non_exist.mp3` will not be played because it will be interrupted by the second time playback.
    std::vector<std::string> first_urls{
        get_audio_file_path("4.mp3"),
        get_audio_file_path("non_exist.mp3"),
    };
    auto first_result = AudioHelper::call_function_sync(
                            AudioHelper::FunctionId::PlayUrls, BROOKESIA_DESCRIBE_TO_JSON(first_urls).as_array()
                        );
    BROOKESIA_CHECK_FALSE_RETURN(
        first_result, false, "Failed to play multiple URLs for first time: %1%", first_result.error()
    );
    if (!first_result) {
        BROOKESIA_LOGE("Failed to play multiple URLs for first time: %1%", first_result.error());
        return false;
    }

    // Then we play the second time with interrupting the current playback
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
    auto second_result = AudioHelper::call_function_sync(
                             AudioHelper::FunctionId::PlayUrls, BROOKESIA_DESCRIBE_TO_JSON(second_urls).as_array(),
                             BROOKESIA_DESCRIBE_TO_JSON(second_config).as_object()
                         );
    BROOKESIA_CHECK_FALSE_RETURN(
        second_result, false, "Failed to play multiple URLs for second time: %1%", second_result.error()
    );

    boost::this_thread::sleep_for(boost::chrono::seconds(10));

    BROOKESIA_LOGI("\n\n--- Demo: Play Multiple URLs Completed ---\n");

    return true;
}

static bool demo_play_control()
{
    BROOKESIA_LOGI("\n\n--- Demo: Play Control ---\n");

    // Subscribe play state changed event
    auto on_play_state_changed = [](const std::string & event_name, const std::string & play_state) {
        BROOKESIA_LOGI("[Event: %1%] %2%", event_name, play_state);
    };
    // RAII - connection stays alive while `player_connection` exists
    auto player_connection = AudioHelper::subscribe_event(AudioHelper::EventId::PlayStateChanged, on_play_state_changed);
    BROOKESIA_CHECK_FALSE_RETURN(player_connection.connected(), false, "Failed to subscribe play state changed event");

    // Start playing a longer file
    AudioHelper::PlayUrlConfig config{
        .loop_count = 10
    };
    auto play_result = AudioHelper::call_function_sync(
                           AudioHelper::FunctionId::PlayUrl, get_audio_file_path("7.mp3"),
                           BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
                       );
    BROOKESIA_CHECK_FALSE_RETURN(play_result, false, "Failed to start playback: %1%", play_result.error());

    // Wait a bit then pause
    boost::this_thread::sleep_for(boost::chrono::seconds(3));
    BROOKESIA_LOGI("Pausing playback...");
    auto pause_result = AudioHelper::call_function_sync(
                            AudioHelper::FunctionId::PlayControl,
                            BROOKESIA_DESCRIBE_TO_STR(AudioHelper::PlayControlAction::Pause)
                        );
    BROOKESIA_CHECK_FALSE_RETURN(pause_result, false, "Failed to pause: %1%", pause_result.error());

    // Stay paused for a while
    boost::this_thread::sleep_for(boost::chrono::seconds(3));

    // Resume playback
    BROOKESIA_LOGI("Resuming playback...");
    auto resume_result = AudioHelper::call_function_sync(
                             AudioHelper::FunctionId::PlayControl,
                             BROOKESIA_DESCRIBE_TO_STR(AudioHelper::PlayControlAction::Resume)
                         );
    BROOKESIA_CHECK_FALSE_RETURN(resume_result, false, "Failed to resume: %1%", resume_result.error());

    // Let it play for a while then stop
    boost::this_thread::sleep_for(boost::chrono::seconds(3));

    BROOKESIA_LOGI("Stopping playback...");
    auto stop_result = AudioHelper::call_function_sync(
                           AudioHelper::FunctionId::PlayControl,
                           BROOKESIA_DESCRIBE_TO_STR(AudioHelper::PlayControlAction::Stop)
                       );
    BROOKESIA_CHECK_FALSE_RETURN(stop_result, false, "Failed to stop: %1%", stop_result.error());

    boost::this_thread::sleep_for(boost::chrono::seconds(3));

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
    auto on_encoder_data_ready = [&](const std::string & event_name, service::RawBuffer buf) {
        if (buf.data_ptr && buf.data_size > 0) {
            BROOKESIA_LOGD("[Event: %1%] %2%", event_name, buf);
            recorded_data.insert(recorded_data.end(), buf.data_ptr, buf.data_ptr + buf.data_size);
            // Update decoder feed data size
            decoder_feed_data_size = std::min(decoder_feed_data_size, buf.data_size);
        }
    };
    // RAII - connection stays alive while `encoder_connection` exists
    auto encoder_connection = AudioHelper::subscribe_event(AudioHelper::EventId::EncoderDataReady, on_encoder_data_ready);
    BROOKESIA_CHECK_FALSE_RETURN(encoder_connection.connected(), false, "Failed to subscribe EncoderDataReady");

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
    auto enc_result = AudioHelper::call_function_sync(
                          AudioHelper::FunctionId::StartEncoder,
                          BROOKESIA_DESCRIBE_TO_JSON(encoder_config).as_object(), timeout
                      );
    BROOKESIA_CHECK_FALSE_RETURN(enc_result, false, "Failed to start encoder: %1%", enc_result.error());
    BROOKESIA_LOGI("Encoder started, collecting data for %1% seconds...", run_time_s);
    BROOKESIA_LOGI("Please speak or make noise to record audio...");

    boost::this_thread::sleep_for(boost::chrono::seconds(run_time_s));

    // Stop encoder
    auto stop_enc_result = AudioHelper::call_function_sync(AudioHelper::FunctionId::StopEncoder, timeout);
    BROOKESIA_CHECK_FALSE_RETURN(stop_enc_result, false, "Failed to stop encoder: %1%", stop_enc_result.error());

    size_t total_size = recorded_data.size();
    BROOKESIA_LOGI("Encoder stopped, recorded %1% bytes", total_size);

    if (total_size == 0) {
        BROOKESIA_LOGW("No data recorded, skipping decoder playback");
        return true;
    }

    // Start decoder
    AudioHelper::DecoderDynamicConfig decoder_config{
        .type = codec_format,
        .general = codec_general_config,
    };
    auto dec_result = AudioHelper::call_function_sync(
                          AudioHelper::FunctionId::StartDecoder,
                          BROOKESIA_DESCRIBE_TO_JSON(decoder_config).as_object(), timeout
                      );
    BROOKESIA_CHECK_FALSE_RETURN(dec_result, false, "Failed to start decoder: %1%", dec_result.error());
    BROOKESIA_LOGI("Decoder started, feeding %1% bytes...", total_size);
    BROOKESIA_LOGI("Playing recorded audio...");

    // Feed recorded data to decoder in chunks
    for (size_t offset = 0; offset < recorded_data.size(); offset += decoder_feed_data_size) {
        size_t chunk_size = std::min(decoder_feed_data_size, recorded_data.size() - offset);
        service::RawBuffer chunk(recorded_data.data() + offset, chunk_size);
        BROOKESIA_LOGD("Feeding data(%1%) to decoder...", chunk);
        // This function does not require task scheduler, so we don't need to pass timeout
        auto feed_result = AudioHelper::call_function_sync(AudioHelper::FunctionId::FeedDecoderData, chunk);
        BROOKESIA_CHECK_FALSE_RETURN(feed_result, false, "Failed to feed decoder data: %1%", feed_result.error());
    }

    BROOKESIA_LOGI("All data fed to decoder");

    // Stop decoder
    auto stop_dec_result = AudioHelper::call_function_sync(AudioHelper::FunctionId::StopDecoder, timeout);
    BROOKESIA_CHECK_FALSE_RETURN(stop_dec_result, false, "Failed to stop decoder: %1%", stop_dec_result.error());

    BROOKESIA_LOGI(
        "\n\n--- Demo: Encode -> Decode (Format: %1%) Completed ---\n", BROOKESIA_DESCRIBE_TO_STR(codec_format)
    );

    return true;
}

static bool demo_afe()
{
    BROOKESIA_LOGI("\n\n--- Demo: AFE (Audio Front-End) ---\n");

    size_t monitor_time_s = 120;

    // Configure AFE with VAD and WakeNet (default config)
    AudioHelper::AFE_Config afe_config{
        .vad = AudioHelper::AFE_VAD_Config{},
        .wakenet = AudioHelper::AFE_WakeNetConfig{},
    };
    auto set_afe_result = AudioHelper::call_function_sync(
                              AudioHelper::FunctionId::SetAFE_Config, BROOKESIA_DESCRIBE_TO_JSON(afe_config).as_object()
                          );
    BROOKESIA_CHECK_FALSE_RETURN(set_afe_result, false, "Failed to set AFE config: %1%", set_afe_result.error());
    BROOKESIA_LOGI("AFE config set successfully");

    // Subscribe to AFE events
    auto on_afe_event = [&](const std::string & event_name, const std::string & afe_event_str) {
        AudioHelper::AFE_Event afe_event;
        BROOKESIA_CHECK_FALSE_EXIT(
            BROOKESIA_DESCRIBE_STR_TO_ENUM(afe_event_str, afe_event), "Failed to parse AFE event: %1%", afe_event_str
        );
        BROOKESIA_LOGI("[Event: %1%] %2%", event_name, afe_event_str);
    };
    // RAII - connection stays alive while `afe_connection` exists
    auto afe_connection = AudioHelper::subscribe_event(
                              AudioHelper::EventId::AFE_EventHappened, on_afe_event
                          );
    BROOKESIA_CHECK_FALSE_RETURN(afe_connection.connected(), false, "Failed to subscribe AFE event");

    // Get wakeup words
    auto get_wake_words_result = AudioHelper::call_function_sync<boost::json::array>(
                                     AudioHelper::FunctionId::GetAFE_WakeWords
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
    };
    auto timeout = service::helper::Timeout(1000);
    auto start_enc_result = AudioHelper::call_function_sync(
                                AudioHelper::FunctionId::StartEncoder,
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

    // Stop encoder
    BROOKESIA_LOGI("Stopping encoder...");
    auto stop_enc_result = AudioHelper::call_function_sync(AudioHelper::FunctionId::StopEncoder, timeout);
    BROOKESIA_CHECK_FALSE_RETURN(stop_enc_result, false, "Failed to stop encoder: %1%", stop_enc_result.error());

    BROOKESIA_LOGI("\n\n--- Demo: AFE Completed ---\n");

    return true;
}
