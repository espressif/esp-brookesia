/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include "unity.h"
#include "boost/thread.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_manager/service/local_runner.hpp"
#include "brookesia/service_helper/audio.hpp"
#include "brookesia/hal_interface.hpp"

using namespace esp_brookesia;
using AudioHelper = service::helper::Audio;

static auto &service_manager = service::ServiceManager::get_instance();
static auto &time_profiler = lib_utils::TimeProfiler::get_instance();
static service::ServiceBinding audio_service_binding;

static AudioHelper::CodecGeneralConfig codec_general_config{
#if CONFIG_IDF_TARGET_ESP32C5
    .channels = 1,
    .sample_bits = 16,
    .sample_rate = 8000,
    .frame_duration = 60,
#else
    .channels = 1,
    .sample_bits = 16,
    .sample_rate = 16000,
    .frame_duration = 60,
#endif
};

static bool startup();
static void shutdown();
static std::string get_audio_file_path(const std::string &filename);
static bool test_codec_loopback(AudioHelper::CodecFormat format, size_t run_time_s = 5);

TEST_CASE("Test ServiceAudio - play single url", "[service][audio][play_single]")
{
    BROOKESIA_LOGI("=== Test ServiceAudio - play single url ===");

    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    std::atomic<int> play_state_event_count{0};
    auto on_play_state_changed = [&](const std::string & event_name, const std::string & play_state) {
        BROOKESIA_LOGI("[Event: %1%] %2%", event_name, play_state);
        play_state_event_count++;
    };
    auto player_connection = AudioHelper::subscribe_event(AudioHelper::EventId::PlayStateChanged, on_play_state_changed);
    TEST_ASSERT_TRUE_MESSAGE(player_connection.connected(), "Failed to subscribe PlayStateChanged event");

    auto result = AudioHelper::call_function_sync(AudioHelper::FunctionId::PlayUrl, get_audio_file_path("0.mp3"));
    TEST_ASSERT_TRUE_MESSAGE(result, "Failed to play single URL");

    boost::this_thread::sleep_for(boost::chrono::seconds(3));

    TEST_ASSERT_GREATER_THAN_MESSAGE(0, play_state_event_count.load(), "No PlayStateChanged events received");

    BROOKESIA_LOGI("=== Test ServiceAudio - play single url Completed ===");
}

TEST_CASE("Test ServiceAudio - play single url with config", "[service][audio][play_single_config]")
{
    BROOKESIA_LOGI("=== Test ServiceAudio - play single url with config ===");

    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    AudioHelper::PlayUrlConfig config{
        .interrupt = false,
        .loop_count = 2,
    };
    auto result = AudioHelper::call_function_sync(
                      AudioHelper::FunctionId::PlayUrl, get_audio_file_path("1.mp3"),
                      BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
                  );
    TEST_ASSERT_TRUE_MESSAGE(result, "Failed to play single URL with config");

    boost::this_thread::sleep_for(boost::chrono::seconds(5));

    auto interrupt_result = AudioHelper::call_function_sync(AudioHelper::FunctionId::PlayUrl, get_audio_file_path("2.mp3"));
    TEST_ASSERT_TRUE_MESSAGE(interrupt_result, "Failed to interrupt with new URL");

    boost::this_thread::sleep_for(boost::chrono::seconds(3));

    BROOKESIA_LOGI("=== Test ServiceAudio - play single url with config Completed ===");
}

TEST_CASE("Test ServiceAudio - play multiple urls", "[service][audio][play_multiple]")
{
    BROOKESIA_LOGI("=== Test ServiceAudio - play multiple urls ===");

    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    std::atomic<int> play_state_event_count{0};
    auto on_play_state_changed = [&](const std::string & event_name, const std::string & play_state) {
        BROOKESIA_LOGI("[Event: %1%] %2%", event_name, play_state);
        play_state_event_count++;
    };
    auto player_connection = AudioHelper::subscribe_event(AudioHelper::EventId::PlayStateChanged, on_play_state_changed);
    TEST_ASSERT_TRUE_MESSAGE(player_connection.connected(), "Failed to subscribe PlayStateChanged event");

    std::vector<std::string> urls{
        get_audio_file_path("3.mp3"),
        get_audio_file_path("4.mp3"),
    };
    auto result = AudioHelper::call_function_sync(
                      AudioHelper::FunctionId::PlayUrls, BROOKESIA_DESCRIBE_TO_JSON(urls).as_array()
                  );
    TEST_ASSERT_TRUE_MESSAGE(result, "Failed to play multiple URLs");

    boost::this_thread::sleep_for(boost::chrono::seconds(5));

    std::vector<std::string> second_urls{
        get_audio_file_path("5.mp3"),
        get_audio_file_path("6.mp3"),
    };
    AudioHelper::PlayUrlConfig config{
        .interrupt = true,
        .delay_ms = 1000,
        .loop_count = 1,
        .loop_interval_ms = 500,
    };
    auto second_result = AudioHelper::call_function_sync(
                             AudioHelper::FunctionId::PlayUrls, BROOKESIA_DESCRIBE_TO_JSON(second_urls).as_array(),
                             BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
                         );
    TEST_ASSERT_TRUE_MESSAGE(second_result, "Failed to play multiple URLs with config");

    boost::this_thread::sleep_for(boost::chrono::seconds(8));

    TEST_ASSERT_GREATER_THAN_MESSAGE(0, play_state_event_count.load(), "No PlayStateChanged events received");

    BROOKESIA_LOGI("=== Test ServiceAudio - play multiple urls Completed ===");
}

TEST_CASE("Test ServiceAudio - play control", "[service][audio][play_control]")
{
    BROOKESIA_LOGI("=== Test ServiceAudio - play control ===");

    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Track state transitions to verify control actions
    std::mutex state_mutex;
    std::vector<std::string> state_history;
    std::string last_state;
    auto on_play_state_changed = [&](const std::string & event_name, const std::string & play_state) {
        BROOKESIA_LOGI("[Event: %1%] %2%", event_name, play_state);
        std::lock_guard<std::mutex> lock(state_mutex);
        state_history.push_back(play_state);
        last_state = play_state;
    };
    auto player_connection = AudioHelper::subscribe_event(AudioHelper::EventId::PlayStateChanged, on_play_state_changed);
    TEST_ASSERT_TRUE_MESSAGE(player_connection.connected(), "Failed to subscribe PlayStateChanged event");

    auto play_result = AudioHelper::call_function_sync(AudioHelper::FunctionId::PlayUrl, get_audio_file_path("example.mp3"));
    TEST_ASSERT_TRUE_MESSAGE(play_result, "Failed to start playback");

    boost::this_thread::sleep_for(boost::chrono::seconds(2));

    {
        std::lock_guard<std::mutex> lock(state_mutex);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(
            BROOKESIA_DESCRIBE_TO_STR(AudioHelper::PlayState::Playing).c_str(),
            last_state.c_str(), "Expected Playing state after starting playback"
        );
    }

    // Pause
    BROOKESIA_LOGI("Pausing playback...");
    auto pause_result = AudioHelper::call_function_sync(
                            AudioHelper::FunctionId::PlayControl,
                            BROOKESIA_DESCRIBE_TO_STR(AudioHelper::PlayControlAction::Pause)
                        );
    TEST_ASSERT_TRUE_MESSAGE(pause_result, "Failed to pause playback");

    boost::this_thread::sleep_for(boost::chrono::seconds(2));

    {
        std::lock_guard<std::mutex> lock(state_mutex);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(
            BROOKESIA_DESCRIBE_TO_STR(AudioHelper::PlayState::Paused).c_str(),
            last_state.c_str(), "Expected Paused state after pausing"
        );
    }

    // Resume
    BROOKESIA_LOGI("Resuming playback...");
    auto resume_result = AudioHelper::call_function_sync(
                             AudioHelper::FunctionId::PlayControl,
                             BROOKESIA_DESCRIBE_TO_STR(AudioHelper::PlayControlAction::Resume)
                         );
    TEST_ASSERT_TRUE_MESSAGE(resume_result, "Failed to resume playback");

    boost::this_thread::sleep_for(boost::chrono::seconds(2));

    {
        std::lock_guard<std::mutex> lock(state_mutex);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(
            BROOKESIA_DESCRIBE_TO_STR(AudioHelper::PlayState::Playing).c_str(),
            last_state.c_str(), "Expected Playing state after resuming"
        );
    }

    // Stop
    BROOKESIA_LOGI("Stopping playback...");
    auto stop_result = AudioHelper::call_function_sync(
                           AudioHelper::FunctionId::PlayControl,
                           BROOKESIA_DESCRIBE_TO_STR(AudioHelper::PlayControlAction::Stop)
                       );
    TEST_ASSERT_TRUE_MESSAGE(stop_result, "Failed to stop playback");

    boost::this_thread::sleep_for(boost::chrono::seconds(1));

    {
        std::lock_guard<std::mutex> lock(state_mutex);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(
            BROOKESIA_DESCRIBE_TO_STR(AudioHelper::PlayState::Idle).c_str(),
            last_state.c_str(), "Expected Idle state after stopping"
        );
    }

    // Verify the full state transition sequence: Playing -> Paused -> Playing -> Idle
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        BROOKESIA_LOGI("State transition history (%1% events):", state_history.size());
        for (size_t i = 0; i < state_history.size(); i++) {
            BROOKESIA_LOGI("  [%1%] %2%", i, state_history[i]);
        }
        TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(4, state_history.size(), "Expected at least 4 state transitions");
    }

    BROOKESIA_LOGI("=== Test ServiceAudio - play control Completed ===");
}

TEST_CASE("Test ServiceAudio - volume control", "[service][audio][volume]")
{
    BROOKESIA_LOGI("=== Test ServiceAudio - volume control ===");

    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Get original volume
    BROOKESIA_LOGI("Getting original volume...");
    auto get_volume_result = AudioHelper::call_function_sync<double>(AudioHelper::FunctionId::GetVolume);
    TEST_ASSERT_TRUE_MESSAGE(get_volume_result, "Failed to get original volume");
    uint8_t original_volume = static_cast<uint8_t>(get_volume_result.value());
    BROOKESIA_LOGI("Original volume: %1%", original_volume);

    // Set new volume
    uint8_t new_volume = 90;
    BROOKESIA_LOGI("Setting volume to %1%...", new_volume);
    auto set_volume_result = AudioHelper::call_function_sync(AudioHelper::FunctionId::SetVolume, new_volume);
    TEST_ASSERT_TRUE_MESSAGE(set_volume_result, "Failed to set volume");

    // Verify new volume
    BROOKESIA_LOGI("Verifying new volume...");
    auto verify_result = AudioHelper::call_function_sync<double>(AudioHelper::FunctionId::GetVolume);
    TEST_ASSERT_TRUE_MESSAGE(verify_result, "Failed to get volume for verification");
    TEST_ASSERT_EQUAL_MESSAGE(new_volume, static_cast<uint8_t>(verify_result.value()), "Volume mismatch after set");
    BROOKESIA_LOGI("Volume verified: %1%", static_cast<uint8_t>(verify_result.value()));

    // Play with new volume
    std::vector<std::string> urls{
        get_audio_file_path("8.mp3"),
        get_audio_file_path("9.mp3"),
    };
    auto play_result = AudioHelper::call_function_sync(
                           AudioHelper::FunctionId::PlayUrls, BROOKESIA_DESCRIBE_TO_JSON(urls).as_array()
                       );
    TEST_ASSERT_TRUE_MESSAGE(play_result, "Failed to play files with new volume");

    boost::this_thread::sleep_for(boost::chrono::seconds(3));

    // Restore original volume
    BROOKESIA_LOGI("Restoring original volume to %1%...", original_volume);
    auto restore_result = AudioHelper::call_function_sync(AudioHelper::FunctionId::SetVolume, original_volume);
    TEST_ASSERT_TRUE_MESSAGE(restore_result, "Failed to restore original volume");

    BROOKESIA_LOGI("=== Test ServiceAudio - volume control Completed ===");
}

TEST_CASE("Test ServiceAudio - codec PCM", "[service][audio][codec][pcm]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    TEST_ASSERT_TRUE_MESSAGE(test_codec_loopback(AudioHelper::CodecFormat::PCM), "Codec PCM loopback failed");
}

TEST_CASE("Test ServiceAudio - codec OPUS", "[service][audio][codec][opus]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    TEST_ASSERT_TRUE_MESSAGE(test_codec_loopback(AudioHelper::CodecFormat::OPUS), "Codec OPUS loopback failed");
}

#if !CONFIG_IDF_TARGET_ESP32C5
TEST_CASE("Test ServiceAudio - codec G711A", "[service][audio][codec][g711a]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    TEST_ASSERT_TRUE_MESSAGE(test_codec_loopback(AudioHelper::CodecFormat::G711A), "Codec G711A loopback failed");
}
#endif // !CONFIG_IDF_TARGET_ESP32C5

TEST_CASE("Test ServiceAudio - AFE", "[service][audio][afe]")
{
    BROOKESIA_LOGI("=== Test ServiceAudio - AFE ===");

    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    size_t monitor_time_s = 10;

    // Configure AFE with VAD and WakeNet
    AudioHelper::AFE_Config afe_config{
        .vad = AudioHelper::AFE_VAD_Config{},
        .wakenet = AudioHelper::AFE_WakeNetConfig{},
    };
    auto set_afe_result = AudioHelper::call_function_sync(
                              AudioHelper::FunctionId::SetAFE_Config, BROOKESIA_DESCRIBE_TO_JSON(afe_config).as_object()
                          );
    TEST_ASSERT_TRUE_MESSAGE(set_afe_result, "Failed to set AFE config");

    // Subscribe to AFE events
    std::atomic<int> afe_event_count{0};
    auto on_afe_event = [&](const std::string & event_name, const std::string & afe_event_str) {
        BROOKESIA_LOGI("[Event: %1%] %2%", event_name, afe_event_str);
        afe_event_count++;
    };
    auto afe_connection = AudioHelper::subscribe_event(AudioHelper::EventId::AFE_EventHappened, on_afe_event);
    TEST_ASSERT_TRUE_MESSAGE(afe_connection.connected(), "Failed to subscribe AFE event");

    // Get wakeup words
    auto get_wake_words_result = AudioHelper::call_function_sync<boost::json::array>(
                                     AudioHelper::FunctionId::GetAFE_WakeWords
                                 );
    TEST_ASSERT_TRUE_MESSAGE(get_wake_words_result, "Failed to get wakeup words");
    TEST_ASSERT_TRUE_MESSAGE(!get_wake_words_result.value().empty(), "No wakeup words detected");
    BROOKESIA_LOGI("Detected wakeup words: %1%", get_wake_words_result.value());

    // Start encoder to enable AFE processing
    AudioHelper::EncoderDynamicConfig encoder_config{
        .type = AudioHelper::CodecFormat::PCM,
        .general = codec_general_config,
    };
    auto timeout = service::helper::Timeout(1000);
    auto start_enc_result = AudioHelper::call_function_sync(
                                AudioHelper::FunctionId::StartEncoder,
                                BROOKESIA_DESCRIBE_TO_JSON(encoder_config).as_object(), timeout
                            );
    TEST_ASSERT_TRUE_MESSAGE(start_enc_result, "Failed to start encoder for AFE");
    BROOKESIA_LOGI("Encoder started, monitoring AFE events for %1% seconds...", monitor_time_s);

    boost::this_thread::sleep_for(boost::chrono::seconds(monitor_time_s));

    // Stop encoder
    auto stop_enc_result = AudioHelper::call_function_sync(AudioHelper::FunctionId::StopEncoder, timeout);
    TEST_ASSERT_TRUE_MESSAGE(stop_enc_result, "Failed to stop encoder");

    BROOKESIA_LOGI("AFE event count: %1%", afe_event_count.load());
    BROOKESIA_LOGI("=== Test ServiceAudio - AFE Completed ===");
}

TEST_CASE("Test ServiceAudio - play using LocalTestRunner", "[service][audio][local_runner]")
{
    BROOKESIA_LOGI("=== Test ServiceAudio - play using LocalTestRunner ===");

    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    AudioHelper::PlayUrlConfig play_config{
        .interrupt = true,
        .loop_count = 1,
    };

    std::vector<std::string> urls{
        get_audio_file_path("0.mp3"),
        get_audio_file_path("1.mp3"),
    };

    std::vector<service::LocalTestItem> test_items = {
        {
            .name = "Play single URL",
            .method = BROOKESIA_DESCRIBE_ENUM_TO_STR(AudioHelper::FunctionId::PlayUrl),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(AudioHelper::FunctionPlayUrlParam::Url), get_audio_file_path("0.mp3")},
                {
                    BROOKESIA_DESCRIBE_TO_STR(AudioHelper::FunctionPlayUrlParam::Config),
                    BROOKESIA_DESCRIBE_TO_JSON(play_config)
                }
            },
            .start_delay_ms = 100,
            .call_timeout_ms = 1000,
            .run_duration_ms = 3000,
        },
        {
            .name = "Play multiple URLs",
            .method = BROOKESIA_DESCRIBE_ENUM_TO_STR(AudioHelper::FunctionId::PlayUrls),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(AudioHelper::FunctionPlayUrlsParam::Urls), BROOKESIA_DESCRIBE_TO_JSON(urls)},
                {
                    BROOKESIA_DESCRIBE_TO_STR(AudioHelper::FunctionPlayUrlsParam::Config),
                    BROOKESIA_DESCRIBE_TO_JSON(play_config)
                }
            },
            .start_delay_ms = 100,
            .call_timeout_ms = 1000,
            .run_duration_ms = 5000,
        },
        {
            .name = "Get volume",
            .method = BROOKESIA_DESCRIBE_ENUM_TO_STR(AudioHelper::FunctionId::GetVolume),
            .params = boost::json::object{},
            .validator = [](const service::FunctionValue & value)
            {
                auto num_ptr = std::get_if<double>(&value);
                if (!num_ptr) {
                    BROOKESIA_LOGE("GetVolume: value is not a number");
                    return false;
                }
                BROOKESIA_LOGI("Volume: %1%", *num_ptr);
                return *num_ptr >= 0 && *num_ptr <= 100;
            },
            .start_delay_ms = 100,
            .call_timeout_ms = 200,
            .run_duration_ms = 500,
        },
        {
            .name = "Set volume to 80",
            .method = BROOKESIA_DESCRIBE_ENUM_TO_STR(AudioHelper::FunctionId::SetVolume),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(AudioHelper::FunctionSetVolumeParam::Volume), 80}
            },
            .start_delay_ms = 100,
            .call_timeout_ms = 1000,
            .run_duration_ms = 1500,
        },
        {
            .name = "Verify volume is 80",
            .method = BROOKESIA_DESCRIBE_ENUM_TO_STR(AudioHelper::FunctionId::GetVolume),
            .params = boost::json::object{},
            .validator = [](const service::FunctionValue & value)
            {
                auto num_ptr = std::get_if<double>(&value);
                if (!num_ptr) {
                    BROOKESIA_LOGE("Verify volume: value is not a number");
                    return false;
                }
                if (static_cast<uint8_t>(*num_ptr) != 80) {
                    BROOKESIA_LOGE("Verify volume: expected 80 but got %1%", *num_ptr);
                    return false;
                }
                return true;
            },
            .start_delay_ms = 100,
            .call_timeout_ms = 200,
            .run_duration_ms = 500,
        },
    };

    service::LocalTestRunner runner;
    service::LocalTestRunner::RunTestsConfig config(std::string(AudioHelper::get_name()));
    config.scheduler_config.worker_configs = {{.stack_size = 10 * 1024}};
    config.extra_timeout_ms = 5000;
    bool all_passed = runner.run_tests(config, test_items);

    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all LocalTestRunner tests passed");

    const auto &results = runner.get_results();
    TEST_ASSERT_EQUAL(test_items.size(), results.size());
    for (size_t i = 0; i < results.size(); i++) {
        TEST_ASSERT_TRUE_MESSAGE(results[i], (std::string("Test failed: ") + test_items[i].name).c_str());
    }

    BROOKESIA_LOGI("=== Test ServiceAudio - play using LocalTestRunner Completed ===");
}

static bool startup()
{
    BROOKESIA_CHECK_FALSE_RETURN(service_manager.start(), false, "Failed to start service manager");

    audio_service_binding = service_manager.bind(AudioHelper::get_name().data());
    TEST_ASSERT_TRUE_MESSAGE(audio_service_binding.is_valid(), "Failed to bind Audio service");

    return true;
}

static void shutdown()
{
    audio_service_binding.release();
    service_manager.deinit();
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

static bool test_codec_loopback(AudioHelper::CodecFormat format, size_t run_time_s)
{
    auto format_str = BROOKESIA_DESCRIBE_TO_STR(format);
    BROOKESIA_LOGI("=== Test codec loopback: %1% ===", format_str);

    size_t decoder_feed_data_size = 1024;
    std::vector<uint8_t> recorded_data;
    auto on_encoder_data_ready = [&](const std::string & event_name, service::RawBuffer buf) {
        if (buf.data_ptr && buf.data_size > 0) {
            recorded_data.insert(recorded_data.end(), buf.data_ptr, buf.data_ptr + buf.data_size);
            decoder_feed_data_size = std::min(decoder_feed_data_size, buf.data_size);
        }
    };
    auto encoder_conn = AudioHelper::subscribe_event(AudioHelper::EventId::EncoderDataReady, on_encoder_data_ready);
    BROOKESIA_CHECK_FALSE_RETURN(encoder_conn.connected(), false, "Failed to subscribe EncoderDataReady");

    auto timeout = service::helper::Timeout(1000);

    AudioHelper::EncoderDynamicConfig encoder_config{
        .type = format,
        .general = codec_general_config,
    };
    if (format == AudioHelper::CodecFormat::OPUS) {
        encoder_config.extra = AudioHelper::EncoderExtraConfigOpus{
            .enable_vbr = false,
            .bitrate = 24000,
        };
    }
    auto enc_result = AudioHelper::call_function_sync(
                          AudioHelper::FunctionId::StartEncoder,
                          BROOKESIA_DESCRIBE_TO_JSON(encoder_config).as_object(), timeout
                      );
    BROOKESIA_CHECK_FALSE_RETURN(enc_result, false, "Failed to start %1% encoder: %2%", format_str, enc_result.error());
    BROOKESIA_LOGI("%1% encoder started, recording for %2% seconds...", format_str, run_time_s);

    boost::this_thread::sleep_for(boost::chrono::seconds(run_time_s));

    auto stop_enc_result = AudioHelper::call_function_sync(AudioHelper::FunctionId::StopEncoder, timeout);
    BROOKESIA_CHECK_FALSE_RETURN(stop_enc_result, false, "Failed to stop %1% encoder: %2%", format_str, stop_enc_result.error());

    size_t total_size = recorded_data.size();
    BROOKESIA_LOGI("%1% encoder stopped, recorded %2% bytes", format_str, total_size);

    if (total_size == 0) {
        BROOKESIA_LOGW("No data recorded, skipping decoder playback");
        return true;
    }

    AudioHelper::DecoderDynamicConfig decoder_config{
        .type = format,
        .general = codec_general_config,
    };
    auto dec_result = AudioHelper::call_function_sync(
                          AudioHelper::FunctionId::StartDecoder,
                          BROOKESIA_DESCRIBE_TO_JSON(decoder_config).as_object(), timeout
                      );
    BROOKESIA_CHECK_FALSE_RETURN(dec_result, false, "Failed to start %1% decoder: %2%", format_str, dec_result.error());
    BROOKESIA_LOGI("%1% decoder started, feeding %2% bytes...", format_str, total_size);

    for (size_t offset = 0; offset < recorded_data.size(); offset += decoder_feed_data_size) {
        size_t chunk_size = std::min(decoder_feed_data_size, recorded_data.size() - offset);
        service::RawBuffer chunk(recorded_data.data() + offset, chunk_size);
        auto feed_result = AudioHelper::call_function_sync(AudioHelper::FunctionId::FeedDecoderData, chunk);
        BROOKESIA_CHECK_FALSE_RETURN(feed_result, false, "Failed to feed %1% decoder data: %2%", format_str, feed_result.error());
    }
    BROOKESIA_LOGI("All data fed to %1% decoder", format_str);

    auto stop_dec_result = AudioHelper::call_function_sync(AudioHelper::FunctionId::StopDecoder, timeout);
    BROOKESIA_CHECK_FALSE_RETURN(stop_dec_result, false, "Failed to stop %1% decoder: %2%", format_str, stop_dec_result.error());

    BROOKESIA_LOGI("=== Test codec loopback: %1% Completed ===", format_str);

    return true;
}
