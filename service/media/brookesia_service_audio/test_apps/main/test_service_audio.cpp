/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#if defined(ESP_PLATFORM)
#include "sdkconfig.h"
#include "brookesia/hal_adaptor.hpp"
#include "brookesia/hal_interface.hpp"
#else
#include "brookesia/hal_interface/interfaces/storage/file_system.hpp"
#include "brookesia/hal_linux.hpp"
#endif
#include "brookesia/lib_utils.hpp"
#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/service_audio/macro_configs.h"
#include "brookesia/service_audio/service_audio.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_manager/service/local_runner.hpp"
#include "brookesia/service_helper/media/audio.hpp"

using namespace esp_brookesia;
using AudioHelper = service::helper::Audio;
using AudioPlaybackHelper = service::helper::AudioPlayback;
using AudioEncoderHelper = service::helper::AudioEncoder<0>;
using AudioDecoderHelper = service::helper::AudioDecoder<0>;

static auto &service_manager = service::ServiceManager::get_instance();
static service::ServiceBinding audio_playback_binding;
static service::ServiceBinding audio_encoder_binding;
static service::ServiceBinding audio_decoder_binding;

#ifndef CONFIG_IDF_TARGET_ESP32C5
#define CONFIG_IDF_TARGET_ESP32C5 0
#endif

constexpr uint32_t AUDIO_WAIT_EVENT_TIMEOUT_MS = 3000;
#if defined(ESP_PLATFORM)
constexpr uint32_t AUDIO_PLAYBACK_FINISH_TIMEOUT_MS = 20000;
constexpr uint32_t AUDIO_PLAYBACK_AUDITION_WINDOW_MS = 1500;
constexpr uint32_t AUDIO_PLAYBACK_PAUSE_WINDOW_MS = 1200;
constexpr uint32_t AUDIO_CODEC_CAPTURE_DURATION_MS = 5000;
constexpr uint32_t AUDIO_CODEC_START_TIMEOUT_MS = 5000;
constexpr uint32_t AUDIO_AFE_RUN_TIME_MS = 10000;
constexpr uint32_t AUDIO_LOCAL_SINGLE_RUN_DURATION_MS = 3000;
constexpr uint32_t AUDIO_LOCAL_LIST_RUN_DURATION_MS = 5000;
constexpr uint32_t AUDIO_LOCAL_EXTRA_TIMEOUT_MS = 5000;
#else
constexpr uint32_t AUDIO_PLAYBACK_FINISH_TIMEOUT_MS = 15000;
constexpr uint32_t AUDIO_PLAYBACK_AUDITION_WINDOW_MS = 1200;
constexpr uint32_t AUDIO_PLAYBACK_PAUSE_WINDOW_MS = 1000;
constexpr uint32_t AUDIO_CODEC_CAPTURE_DURATION_MS = 3000;
constexpr uint32_t AUDIO_CODEC_START_TIMEOUT_MS = 3000;
constexpr uint32_t AUDIO_AFE_RUN_TIME_MS = 500;
constexpr uint32_t AUDIO_LOCAL_SINGLE_RUN_DURATION_MS = 200;
constexpr uint32_t AUDIO_LOCAL_LIST_RUN_DURATION_MS = 300;
constexpr uint32_t AUDIO_LOCAL_EXTRA_TIMEOUT_MS = 1000;
#endif

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
static bool configure_audio_processor();
static void configure_linux_hal_environment();
static std::string get_audio_file_path(const std::string &filename);
static bool test_codec_loopback(AudioHelper::CodecFormat format);
static bool wait_for_playback_state(
    AudioPlaybackHelper::EventMonitor<AudioPlaybackHelper::EventId::PlayStateChanged> &monitor,
    AudioHelper::PlayState state, uint32_t timeout_ms, const char *message
);
static bool has_playback_state(
    const AudioPlaybackHelper::EventMonitor<AudioPlaybackHelper::EventId::PlayStateChanged> &monitor,
    AudioHelper::PlayState state
);

BROOKESIA_TEST_CASE(
    playback_volume_mute_reset,
    "Test ServiceAudio - playback volume/mute/reset",
    "[service][audio][playback_control]"
)
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    if (!hal::has_interface(hal::audio::CodecPlayerIface::NAME)) {
        TEST_IGNORE_MESSAGE("Audio player interface is not available on this board");
    }

    constexpr double test_volume = 55.0;
    auto set_volume_result = AudioPlaybackHelper::call_function_sync(
                                 AudioPlaybackHelper::FunctionId::SetVolume, test_volume
                             );
    TEST_ASSERT_TRUE_MESSAGE(set_volume_result, "Failed to set playback volume");

    auto volume_result = AudioPlaybackHelper::call_function_sync<double>(AudioPlaybackHelper::FunctionId::GetVolume);
    TEST_ASSERT_TRUE_MESSAGE(volume_result.has_value(), "Failed to get playback volume");
    TEST_ASSERT_EQUAL_DOUBLE(test_volume, volume_result.value());

    auto set_mute_result = AudioPlaybackHelper::call_function_sync(AudioPlaybackHelper::FunctionId::SetMute, true);
    TEST_ASSERT_TRUE_MESSAGE(set_mute_result, "Failed to set playback mute");

    auto mute_result = AudioPlaybackHelper::call_function_sync<bool>(AudioPlaybackHelper::FunctionId::GetMute);
    TEST_ASSERT_TRUE_MESSAGE(mute_result.has_value(), "Failed to get playback mute");
    TEST_ASSERT_TRUE(mute_result.value());

    auto reset_result = AudioPlaybackHelper::call_function_sync(AudioPlaybackHelper::FunctionId::ResetData);
    TEST_ASSERT_TRUE_MESSAGE(reset_result, "Failed to reset audio playback data");

    volume_result = AudioPlaybackHelper::call_function_sync<double>(AudioPlaybackHelper::FunctionId::GetVolume);
    TEST_ASSERT_TRUE_MESSAGE(volume_result.has_value(), "Failed to get playback volume after reset");
    TEST_ASSERT_EQUAL_DOUBLE(BROOKESIA_SERVICE_AUDIO_PLAYBACK_VOLUME_DEFAULT, volume_result.value());

    mute_result = AudioPlaybackHelper::call_function_sync<bool>(AudioPlaybackHelper::FunctionId::GetMute);
    TEST_ASSERT_TRUE_MESSAGE(mute_result.has_value(), "Failed to get playback mute after reset");
    TEST_ASSERT_EQUAL(BROOKESIA_SERVICE_AUDIO_PLAYBACK_DEFAULT_MUTE, mute_result.value());
}

BROOKESIA_TEST_CASE(play_single_url, "Test ServiceAudio - play single url", "[service][audio][play_single]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    AudioPlaybackHelper::EventMonitor<AudioPlaybackHelper::EventId::PlayStateChanged> play_state_monitor;
    TEST_ASSERT_TRUE_MESSAGE(play_state_monitor.start(), "Failed to monitor PlayStateChanged event");

    auto result = AudioPlaybackHelper::call_function_sync(
                      AudioPlaybackHelper::FunctionId::Play, get_audio_file_path("example.mp3")
                  );
    TEST_ASSERT_TRUE_MESSAGE(result, "Failed to play single URL");

    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Playing, AUDIO_WAIT_EVENT_TIMEOUT_MS,
            "Expected Playing state after starting playback"
        ),
        "Expected Playing state after starting playback"
    );
    play_state_monitor.clear();
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Idle, AUDIO_PLAYBACK_FINISH_TIMEOUT_MS,
            "Expected Idle state after natural playback completion"
        ),
        "Expected Idle state after natural playback completion"
    );
}

BROOKESIA_TEST_CASE(
    play_single_url_with_config, "Test ServiceAudio - play single url with config", "[service][audio][play_single_config]"
)
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    AudioPlaybackHelper::EventMonitor<AudioPlaybackHelper::EventId::PlayStateChanged> play_state_monitor;
    TEST_ASSERT_TRUE_MESSAGE(play_state_monitor.start(), "Failed to monitor PlayStateChanged event");

    AudioHelper::PlayUrlConfig config{
        .interrupt = true,
        .loop_count = 1,
    };
    auto result = AudioPlaybackHelper::call_function_sync(
                      AudioPlaybackHelper::FunctionId::Play, get_audio_file_path("example.mp3"),
                      BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
                  );
    TEST_ASSERT_TRUE_MESSAGE(result, "Failed to play single URL with config");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Playing, AUDIO_WAIT_EVENT_TIMEOUT_MS,
            "Expected Playing state after configured playback"
        ),
        "Expected Playing state after configured playback"
    );

    lib_utils::test_adapter::delay_ms(AUDIO_PLAYBACK_AUDITION_WINDOW_MS);

    play_state_monitor.clear();
    auto interrupt_result = AudioPlaybackHelper::call_function_sync(
                                AudioPlaybackHelper::FunctionId::Play, get_audio_file_path("2.mp3")
                            );
    TEST_ASSERT_TRUE_MESSAGE(interrupt_result, "Failed to interrupt with new URL");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Playing, AUDIO_WAIT_EVENT_TIMEOUT_MS,
            "Expected Playing state after immediate interrupt starts replacement playback"
        ),
        "Expected Playing state after immediate interrupt starts replacement playback"
    );
    TEST_ASSERT_TRUE_MESSAGE(
        !has_playback_state(play_state_monitor, AudioHelper::PlayState::Idle),
        "Unexpected transient Idle state during immediate interrupt"
    );
    play_state_monitor.clear();
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Idle, AUDIO_PLAYBACK_FINISH_TIMEOUT_MS,
            "Expected only final Idle state after interrupted playback finishes"
        ),
        "Expected only final Idle state after interrupted playback finishes"
    );
}

BROOKESIA_TEST_CASE(
    play_interrupt_with_delay_gap,
    "Test ServiceAudio - interrupt with delay gap",
    "[service][audio][interrupt][gap]"
)
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    AudioPlaybackHelper::EventMonitor<AudioPlaybackHelper::EventId::PlayStateChanged> play_state_monitor;
    TEST_ASSERT_TRUE_MESSAGE(play_state_monitor.start(), "Failed to monitor PlayStateChanged event");

    auto first_result = AudioPlaybackHelper::call_function_sync(
                            AudioPlaybackHelper::FunctionId::Play, get_audio_file_path("0.mp3")
                        );
    TEST_ASSERT_TRUE_MESSAGE(first_result, "Failed to start first playback");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Playing, AUDIO_WAIT_EVENT_TIMEOUT_MS,
            "Expected Playing state after starting first playback"
        ),
        "Expected Playing state after starting first playback"
    );

    AudioHelper::PlayUrlConfig delayed_interrupt_config{
        .interrupt = true,
        .delay_ms = 2000,
        .loop_count = 1,
    };
    play_state_monitor.clear();
    auto second_result = AudioPlaybackHelper::call_function_sync(
                             AudioPlaybackHelper::FunctionId::Play, get_audio_file_path("2.mp3"),
                             BROOKESIA_DESCRIBE_TO_JSON(delayed_interrupt_config).as_object()
                         );
    TEST_ASSERT_TRUE_MESSAGE(second_result, "Failed to submit delayed interrupt playback");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Idle, AUDIO_PLAYBACK_FINISH_TIMEOUT_MS,
            "Expected real Idle gap before delayed replacement playback"
        ),
        "Expected real Idle gap before delayed replacement playback"
    );
    play_state_monitor.clear();
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Playing, AUDIO_PLAYBACK_FINISH_TIMEOUT_MS,
            "Expected Playing state after delayed replacement playback starts"
        ),
        "Expected Playing state after delayed replacement playback starts"
    );
    play_state_monitor.clear();
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Idle, AUDIO_PLAYBACK_FINISH_TIMEOUT_MS,
            "Expected final Idle state after delayed replacement playback finishes"
        ),
        "Expected final Idle state after delayed replacement playback finishes"
    );
}

BROOKESIA_TEST_CASE(
    play_interrupt_after_pause,
    "Test ServiceAudio - interrupt after pause",
    "[service][audio][interrupt][pause]"
)
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    AudioPlaybackHelper::EventMonitor<AudioPlaybackHelper::EventId::PlayStateChanged> play_state_monitor;
    TEST_ASSERT_TRUE_MESSAGE(play_state_monitor.start(), "Failed to monitor PlayStateChanged event");

    auto first_result = AudioPlaybackHelper::call_function_sync(
                            AudioPlaybackHelper::FunctionId::Play, get_audio_file_path("example.mp3")
                        );
    TEST_ASSERT_TRUE_MESSAGE(first_result, "Failed to start first playback");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Playing, AUDIO_WAIT_EVENT_TIMEOUT_MS,
            "Expected Playing state after starting first playback"
        ),
        "Expected Playing state after starting first playback"
    );
    lib_utils::test_adapter::delay_ms(AUDIO_PLAYBACK_AUDITION_WINDOW_MS);

    play_state_monitor.clear();
    auto pause_result = AudioPlaybackHelper::call_function_sync(AudioPlaybackHelper::FunctionId::Pause);
    TEST_ASSERT_TRUE_MESSAGE(pause_result, "Failed to pause playback");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Paused, AUDIO_WAIT_EVENT_TIMEOUT_MS,
            "Expected Paused state after pausing first playback"
        ),
        "Expected Paused state after pausing first playback"
    );

    play_state_monitor.clear();
    auto interrupt_result = AudioPlaybackHelper::call_function_sync(
                                AudioPlaybackHelper::FunctionId::Play, get_audio_file_path("2.mp3")
                            );
    TEST_ASSERT_TRUE_MESSAGE(interrupt_result, "Failed to interrupt paused playback with new URL");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Playing, AUDIO_WAIT_EVENT_TIMEOUT_MS,
            "Expected Playing state after interrupting paused playback"
        ),
        "Expected Playing state after interrupting paused playback"
    );
    TEST_ASSERT_TRUE_MESSAGE(
        !has_playback_state(play_state_monitor, AudioHelper::PlayState::Idle),
        "Unexpected transient Idle state during paused interrupt"
    );

    play_state_monitor.clear();
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Idle, AUDIO_PLAYBACK_FINISH_TIMEOUT_MS,
            "Expected final Idle state after paused interrupt playback finishes"
        ),
        "Expected final Idle state after paused interrupt playback finishes"
    );
}

BROOKESIA_TEST_CASE(play_multiple_urls, "Test ServiceAudio - play multiple urls", "[service][audio][play_multiple]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    AudioPlaybackHelper::EventMonitor<AudioPlaybackHelper::EventId::PlayStateChanged> play_state_monitor;
    TEST_ASSERT_TRUE_MESSAGE(play_state_monitor.start(), "Failed to monitor PlayStateChanged event");

    std::vector<std::string> urls{
        get_audio_file_path("0.mp3"),
        get_audio_file_path("1.mp3"),
        get_audio_file_path("2.mp3"),
        get_audio_file_path("3.mp3"),
    };
    auto result = AudioPlaybackHelper::call_function_sync(
                      AudioPlaybackHelper::FunctionId::PlayUrls, BROOKESIA_DESCRIBE_TO_JSON(urls).as_array()
                  );
    TEST_ASSERT_TRUE_MESSAGE(result, "Failed to play multiple URLs");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Playing, AUDIO_WAIT_EVENT_TIMEOUT_MS,
            "Expected Playing state after starting playlist"
        ),
        "Expected Playing state after starting playlist"
    );

    play_state_monitor.clear();
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Idle, AUDIO_PLAYBACK_FINISH_TIMEOUT_MS,
            "Expected Idle state after playlist playback completes"
        ),
        "Expected Idle state after playlist playback completes"
    );
}

BROOKESIA_TEST_CASE(play_control, "Test ServiceAudio - play control", "[service][audio][play_control]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    AudioPlaybackHelper::EventMonitor<AudioPlaybackHelper::EventId::PlayStateChanged> play_state_monitor;
    TEST_ASSERT_TRUE_MESSAGE(play_state_monitor.start(), "Failed to monitor PlayStateChanged event");

    play_state_monitor.clear();
    auto play_result = AudioPlaybackHelper::call_function_sync(
                           AudioPlaybackHelper::FunctionId::Play, get_audio_file_path("example.mp3")
                       );
    TEST_ASSERT_TRUE_MESSAGE(play_result, "Failed to start playback");

    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Playing, AUDIO_WAIT_EVENT_TIMEOUT_MS,
            "Expected Playing state after starting playback"
        ),
        "Expected Playing state after starting playback"
    );
    lib_utils::test_adapter::delay_ms(AUDIO_PLAYBACK_AUDITION_WINDOW_MS);

    play_state_monitor.clear();
    auto pause_result = AudioPlaybackHelper::call_function_sync(AudioPlaybackHelper::FunctionId::Pause);
    TEST_ASSERT_TRUE_MESSAGE(pause_result, "Failed to pause playback");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Paused, AUDIO_WAIT_EVENT_TIMEOUT_MS,
            "Expected Paused state after pausing"
        ),
        "Expected Paused state after pausing"
    );
    lib_utils::test_adapter::delay_ms(AUDIO_PLAYBACK_PAUSE_WINDOW_MS);

    play_state_monitor.clear();
    auto resume_result = AudioPlaybackHelper::call_function_sync(AudioPlaybackHelper::FunctionId::Resume);
    TEST_ASSERT_TRUE_MESSAGE(resume_result, "Failed to resume playback");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Playing, AUDIO_WAIT_EVENT_TIMEOUT_MS,
            "Expected Playing state after resuming"
        ),
        "Expected Playing state after resuming"
    );
    play_state_monitor.clear();
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Idle, AUDIO_PLAYBACK_FINISH_TIMEOUT_MS,
            "Expected Idle state after resumed playback finishes"
        ),
        "Expected Idle state after resumed playback finishes"
    );

    play_state_monitor.clear();
    play_result = AudioPlaybackHelper::call_function_sync(
                      AudioPlaybackHelper::FunctionId::Play, get_audio_file_path("example.mp3")
                  );
    TEST_ASSERT_TRUE_MESSAGE(play_result, "Failed to restart playback for stop control");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Playing, AUDIO_WAIT_EVENT_TIMEOUT_MS,
            "Expected Playing state after restarting playback"
        ),
        "Expected Playing state after restarting playback"
    );
    lib_utils::test_adapter::delay_ms(AUDIO_PLAYBACK_AUDITION_WINDOW_MS);

    play_state_monitor.clear();
    auto stop_result = AudioPlaybackHelper::call_function_sync(AudioPlaybackHelper::FunctionId::Stop);
    TEST_ASSERT_TRUE_MESSAGE(stop_result, "Failed to stop playback");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_playback_state(
            play_state_monitor, AudioHelper::PlayState::Idle, AUDIO_WAIT_EVENT_TIMEOUT_MS,
            "Expected Idle state after stopping"
        ),
        "Expected Idle state after stopping"
    );
}

BROOKESIA_TEST_CASE(codec_pcm, "Test ServiceAudio - codec PCM", "[service][audio][codec][pcm]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    TEST_ASSERT_TRUE_MESSAGE(test_codec_loopback(AudioHelper::CodecFormat::PCM), "Codec PCM loopback failed");
}

BROOKESIA_TEST_CASE(codec_opus, "Test ServiceAudio - codec OPUS", "[service][audio][codec][opus]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    TEST_ASSERT_TRUE_MESSAGE(test_codec_loopback(AudioHelper::CodecFormat::OPUS), "Codec OPUS loopback failed");
}

#if !CONFIG_IDF_TARGET_ESP32C5
BROOKESIA_TEST_CASE(codec_g711a, "Test ServiceAudio - codec G711A", "[service][audio][codec][g711a]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    TEST_ASSERT_TRUE_MESSAGE(test_codec_loopback(AudioHelper::CodecFormat::G711A), "Codec G711A loopback failed");
}
#endif // !CONFIG_IDF_TARGET_ESP32C5

BROOKESIA_TEST_CASE(afe, "Test ServiceAudio - AFE", "[service][audio][afe]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    AudioEncoderHelper::EventMonitor<AudioEncoderHelper::EventId::AFEEventHappened> afe_event_monitor;
    TEST_ASSERT_TRUE_MESSAGE(afe_event_monitor.start(), "Failed to monitor AFE event");

    auto get_wake_words_result = AudioEncoderHelper::call_function_sync<boost::json::array>(
                                     AudioEncoderHelper::FunctionId::GetAFEWakeWords
                                 );
    TEST_ASSERT_TRUE_MESSAGE(get_wake_words_result, "Failed to get wakeup words");
    if (get_wake_words_result.value().empty()) {
        BROOKESIA_LOGW("No wakeup words detected, skip active AFE event check");
        return;
    }
    BROOKESIA_LOGI("Detected wakeup words: %1%", get_wake_words_result.value());

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
    TEST_ASSERT_TRUE_MESSAGE(start_enc_result, "Failed to start encoder for AFE");

    lib_utils::test_adapter::delay_ms(AUDIO_AFE_RUN_TIME_MS);

    auto stop_enc_result = AudioEncoderHelper::call_function_sync(AudioEncoderHelper::FunctionId::Stop, timeout);
    TEST_ASSERT_TRUE_MESSAGE(stop_enc_result, "Failed to stop encoder");

    BROOKESIA_LOGI("AFE event count: %1%", afe_event_monitor.get_count());
}

BROOKESIA_TEST_CASE(
    decoder_borrowed_stream, "Test ServiceAudio - decoder borrowed stream", "[service][audio][decoder][borrowed]"
)
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto *decoder = service::AudioDecoder::get_instance(0);
    TEST_ASSERT_NOT_NULL_MESSAGE(decoder, "AudioDecoder0 instance is not available");

    constexpr const char *source_name = "BorrowedTest";
    constexpr const char *output_name = "Speaker0";
    auto remove_existing_result = decoder->unregister_source(source_name);
    (void)remove_existing_result;
    service::AudioSourceInfo source_info = {
        .name = source_name,
        .role = "test",
        .preferred_outputs = {output_name},
    };
    auto register_result = decoder->register_source(std::move(source_info));
    TEST_ASSERT_TRUE_MESSAGE(register_result.has_value(), "Failed to register borrowed decoder source");
    lib_utils::FunctionGuard cleanup_guard([decoder, source_id = register_result.value()]() {
        auto close_result = decoder->close_stream(source_id, output_name);
        if (!close_result) {
            BROOKESIA_LOGW("Failed to close borrowed decoder stream during cleanup: %1%", close_result.error());
        }
        auto release_result = decoder->release_output(source_id, output_name);
        if (!release_result) {
            BROOKESIA_LOGW("Failed to release borrowed decoder output during cleanup: %1%", release_result.error());
        }
        auto unregister_result = decoder->unregister_source(source_id);
        if (!unregister_result) {
            BROOKESIA_LOGW("Failed to unregister borrowed decoder source during cleanup: %1%", unregister_result.error());
        }
    });

    auto request_result = decoder->request_output(register_result.value(), output_name);
    TEST_ASSERT_TRUE_MESSAGE(request_result.has_value(), "Failed to request borrowed decoder output");
    auto active_result = decoder->set_active_source(output_name, source_name);
    TEST_ASSERT_TRUE_MESSAGE(active_result.has_value(), "Failed to set borrowed decoder source active");

    service::AudioStreamConfig decoder_config{
        .type = AudioHelper::CodecFormat::PCM,
        .general = codec_general_config,
        .queue_size_bytes = 1024,
    };
    auto open_result = decoder->open_stream(register_result.value(), output_name, decoder_config);
    TEST_ASSERT_TRUE_MESSAGE(open_result.has_value(), "Failed to open borrowed decoder stream");

    std::array<uint8_t, 160> sample = {};
    std::mutex callback_mutex;
    std::condition_variable callback_cv;
    service::AudioWriteResult callback_result = service::AudioWriteResult::Error;
    int callback_count = 0;
    auto callback = [&](service::AudioWriteResult result) {
        std::lock_guard lock(callback_mutex);
        callback_result = result;
        callback_count++;
        callback_cv.notify_one();
    };

    auto write_result = decoder->write_stream_borrowed(
                            register_result.value(), output_name, service::RawBuffer(sample.data(), sample.size()),
                            callback, AUDIO_CODEC_START_TIMEOUT_MS
                        );
    TEST_ASSERT_EQUAL(
        static_cast<int>(service::AudioWriteResult::Written), static_cast<int>(write_result)
    );
    {
        std::unique_lock lock(callback_mutex);
        const bool completed = callback_cv.wait_for(
        lock, std::chrono::milliseconds(AUDIO_CODEC_START_TIMEOUT_MS), [&]() {
            return callback_count == 1;
        }
                               );
        TEST_ASSERT_TRUE_MESSAGE(completed, "Timed out waiting for borrowed decoder callback");
        TEST_ASSERT_EQUAL(static_cast<int>(service::AudioWriteResult::Written), static_cast<int>(callback_result));
    }

    bool invalid_callback_called = false;
    auto invalid_result = decoder->write_stream_borrowed(
                              register_result.value(), output_name,
                              service::RawBuffer(static_cast<const uint8_t *>(nullptr), 0),
    [&](service::AudioWriteResult) {
        invalid_callback_called = true;
    }, 0
                          );
    TEST_ASSERT_EQUAL(
        static_cast<int>(service::AudioWriteResult::DroppedInvalidData), static_cast<int>(invalid_result)
    );
    TEST_ASSERT_FALSE_MESSAGE(invalid_callback_called, "Invalid borrowed submit should not call callback");

    callback_count = 0;
    callback_result = service::AudioWriteResult::Error;
    write_result = decoder->write_stream_borrowed(
                       register_result.value(), output_name, service::RawBuffer(sample.data(), sample.size()),
                       callback, AUDIO_CODEC_START_TIMEOUT_MS
                   );
    TEST_ASSERT_EQUAL(
        static_cast<int>(service::AudioWriteResult::Written), static_cast<int>(write_result)
    );
    auto close_result = decoder->close_stream(register_result.value(), output_name);
    TEST_ASSERT_TRUE_MESSAGE(close_result.has_value(), "Failed to close borrowed decoder stream");
    {
        std::unique_lock lock(callback_mutex);
        const bool released = callback_cv.wait_for(
        lock, std::chrono::milliseconds(AUDIO_CODEC_START_TIMEOUT_MS), [&]() {
            return callback_count == 1;
        }
                              );
        TEST_ASSERT_TRUE_MESSAGE(released, "Borrowed chunk was not released by close stream");
    }
    auto release_result = decoder->release_output(register_result.value(), output_name);
    TEST_ASSERT_TRUE_MESSAGE(release_result.has_value(), "Failed to release borrowed decoder output");
    auto unregister_result = decoder->unregister_source(register_result.value());
    TEST_ASSERT_TRUE_MESSAGE(unregister_result.has_value(), "Failed to unregister borrowed decoder source");
    cleanup_guard.release();
}

BROOKESIA_TEST_CASE(
    play_using_local_test_runner, "Test ServiceAudio - play using LocalTestRunner", "[service][audio][local_runner]"
)
{
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
            .method = BROOKESIA_DESCRIBE_ENUM_TO_STR(AudioPlaybackHelper::FunctionId::Play),
            .params = boost::json::object{
                {BROOKESIA_DESCRIBE_TO_STR(AudioHelper::PlaybackFunctionPlayParam::Url), get_audio_file_path("0.mp3")},
                {
                    BROOKESIA_DESCRIBE_TO_STR(AudioHelper::PlaybackFunctionPlayParam::Config),
                    BROOKESIA_DESCRIBE_TO_JSON(play_config)
                }
            },
            .start_delay_ms = 100,
            .call_timeout_ms = 1000,
            .run_duration_ms = AUDIO_LOCAL_SINGLE_RUN_DURATION_MS,
        },
        {
            .name = "Play multiple URLs",
            .method = BROOKESIA_DESCRIBE_ENUM_TO_STR(AudioPlaybackHelper::FunctionId::PlayUrls),
            .params = boost::json::object{
                {
                    BROOKESIA_DESCRIBE_TO_STR(AudioHelper::PlaybackFunctionPlayUrlsParam::Urls),
                    BROOKESIA_DESCRIBE_TO_JSON(urls)
                },
                {
                    BROOKESIA_DESCRIBE_TO_STR(AudioHelper::PlaybackFunctionPlayUrlsParam::Config),
                    BROOKESIA_DESCRIBE_TO_JSON(play_config)
                }
            },
            .start_delay_ms = 100,
            .call_timeout_ms = 1000,
            .run_duration_ms = AUDIO_LOCAL_LIST_RUN_DURATION_MS,
        },
    };

    service::LocalTestRunner runner;
    service::LocalTestRunner::RunTestsConfig config(std::string(AudioPlaybackHelper::get_name()));
    config.scheduler_config.worker_configs = {{.stack_size = 10 * 1024}};
    config.extra_timeout_ms = AUDIO_LOCAL_EXTRA_TIMEOUT_MS;
    bool all_passed = runner.run_tests(config, test_items);

    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all LocalTestRunner tests passed");

    const auto &results = runner.get_results();
    TEST_ASSERT_EQUAL(test_items.size(), results.size());
    for (size_t i = 0; i < results.size(); i++) {
        TEST_ASSERT_TRUE_MESSAGE(results[i], (std::string("Test failed: ") + test_items[i].name).c_str());
    }
}

static bool startup()
{
    configure_linux_hal_environment();
    BROOKESIA_CHECK_FALSE_RETURN(configure_audio_processor(), false, "Failed to configure audio processor");
    BROOKESIA_CHECK_FALSE_RETURN(service_manager.start(), false, "Failed to start service manager");

    audio_playback_binding = service_manager.bind(AudioPlaybackHelper::get_name().data());
    audio_encoder_binding = service_manager.bind(AudioEncoderHelper::get_name().data());
    audio_decoder_binding = service_manager.bind(AudioDecoderHelper::get_name().data());
    TEST_ASSERT_TRUE_MESSAGE(audio_playback_binding.is_valid(), "Failed to bind AudioPlayback service");
    TEST_ASSERT_TRUE_MESSAGE(audio_encoder_binding.is_valid(), "Failed to bind AudioEncoder service");
    TEST_ASSERT_TRUE_MESSAGE(audio_decoder_binding.is_valid(), "Failed to bind AudioDecoder service");

    return true;
}

static void shutdown()
{
    audio_decoder_binding.release();
    audio_encoder_binding.release();
    audio_playback_binding.release();
    service_manager.deinit();
}

static bool configure_audio_processor()
{
#if defined(ESP_PLATFORM)
    hal::AudioProcessorConfig processor_config {
        .playback = {
            .player_task = {
                .core_id = 0,
                .priority = 5,
                .stack_size = 4 * 1024,
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

    return hal::AudioDevice::get_instance().set_processor_config(std::move(processor_config));
#else
    return true;
#endif
}

static void configure_linux_hal_environment()
{
#if !defined(ESP_PLATFORM)
    setenv("BROOKESIA_HAL_LINUX_FS_ROOT", TEST_AUDIO_FIXTURE_ROOT, 0);
    setenv("BROOKESIA_HAL_LINUX_KV_DIR", TEST_AUDIO_PC_KV_DIR, 0);
#endif
}

static std::string get_audio_file_path(const std::string &filename)
{
    auto fs_handle = hal::acquire_first_interface<hal::storage::FileSystemIface>();
    auto fs_iface = fs_handle.get();
    BROOKESIA_CHECK_NULL_RETURN(fs_iface, filename, "Failed to get storage file system interface");

    auto infos = fs_iface->get_all_info();
    for (const auto &info : infos) {
        if (info.fs_type == hal::storage::FileSystemIface::FileSystemType::LittleFS) {
            return "file:/" + std::string(info.mount_point) + "/" + filename;
        }
    }
    return filename;
}

static bool wait_for_playback_state(
    AudioPlaybackHelper::EventMonitor<AudioPlaybackHelper::EventId::PlayStateChanged> &monitor,
    AudioHelper::PlayState state, uint32_t timeout_ms, const char *message
)
{
    (void)message;
    return monitor.wait_for({BROOKESIA_DESCRIBE_TO_STR(state)}, timeout_ms);
}

static bool has_playback_state(
    const AudioPlaybackHelper::EventMonitor<AudioPlaybackHelper::EventId::PlayStateChanged> &monitor,
    AudioHelper::PlayState state
)
{
    return monitor.has({BROOKESIA_DESCRIBE_TO_STR(state)});
}

static bool test_codec_loopback(AudioHelper::CodecFormat format)
{
    auto format_str = BROOKESIA_DESCRIBE_TO_STR(format);
    BROOKESIA_LOGI("=== Test codec loopback: %1% ===", format_str);

    std::vector<uint8_t> recorded_data;
    size_t decoder_feed_data_size = 1024;
    auto *encoder = service::AudioEncoder::get_instance(0);
    BROOKESIA_CHECK_NULL_RETURN(encoder, false, "AudioEncoder0 instance is not available");
    auto on_encoder_data_ready = [&](service::RawBuffer buf) {
        if (buf.data_ptr && buf.data_size > 0) {
            recorded_data.insert(recorded_data.end(), buf.data_ptr, buf.data_ptr + buf.data_size);
            decoder_feed_data_size = std::min(decoder_feed_data_size, buf.data_size);
        }
    };
    esp_brookesia::lib_utils::scoped_connection encoder_connection =
        encoder->connect_encoded_data(on_encoder_data_ready);
    BROOKESIA_CHECK_FALSE_RETURN(encoder_connection.connected(), false, "Failed to connect encoder data signal");

    auto timeout = service::helper::Timeout(AUDIO_CODEC_START_TIMEOUT_MS);
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

    auto enc_result = AudioEncoderHelper::call_function_sync(
                          AudioEncoderHelper::FunctionId::Start,
                          BROOKESIA_DESCRIBE_TO_JSON(encoder_config).as_object(), timeout
                      );
    BROOKESIA_CHECK_FALSE_RETURN(enc_result, false, "Failed to start %1% encoder: %2%", format_str, enc_result.error());
    BROOKESIA_LOGI("Codec capture started: %1% (please speak into the microphone)", format_str);
    const auto capture_start_time = std::chrono::steady_clock::now();
    lib_utils::test_adapter::delay_ms(AUDIO_CODEC_CAPTURE_DURATION_MS);
    const auto capture_duration_ms = static_cast<uint32_t>(
                                         std::chrono::duration_cast<std::chrono::milliseconds>(
                                                 std::chrono::steady_clock::now() - capture_start_time
                                         ).count()
                                     );

    auto stop_enc_result = AudioEncoderHelper::call_function_sync(AudioEncoderHelper::FunctionId::Stop, timeout);
    BROOKESIA_CHECK_FALSE_RETURN(
        stop_enc_result, false, "Failed to stop %1% encoder: %2%", format_str, stop_enc_result.error()
    );
    BROOKESIA_LOGI("Codec capture stopped: %1%", format_str);

    const size_t total_encoded_bytes = recorded_data.size();
    BROOKESIA_LOGI(
        "Captured %1% bytes in %2% ms (decoder chunk size: %3%)",
        total_encoded_bytes, capture_duration_ms, decoder_feed_data_size
    );

    if (recorded_data.empty()) {
        BROOKESIA_LOGW("No data recorded, skipping decoder playback");
        return true;
    }

    auto *decoder = service::AudioDecoder::get_instance(0);
    BROOKESIA_CHECK_NULL_RETURN(decoder, false, "AudioDecoder0 instance is not available");
    constexpr const char *source_name = "CodecLoopback";
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
    };
    esp_brookesia::lib_utils::scoped_connection decoder_drain_connection =
        decoder->connect_stream_drained(on_decoder_stream_drained);
    BROOKESIA_CHECK_FALSE_RETURN(decoder_drain_connection.connected(), false, "Failed to connect decoder drain signal");

    auto remove_existing_result = decoder->unregister_source(source_name);
    (void)remove_existing_result;
    service::AudioSourceInfo source_info = {
        .name = source_name,
        .role = "test",
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
        .type = format,
        .general = codec_general_config,
    };
    auto dec_result = decoder->open_stream(register_result.value(), output_name, decoder_config);
    BROOKESIA_CHECK_FALSE_RETURN(dec_result, false, "Failed to start %1% decoder: %2%", format_str, dec_result.error());
    BROOKESIA_LOGI("Codec replay started: %1%", format_str);

    for (size_t offset = 0; offset < recorded_data.size(); offset += decoder_feed_data_size) {
        const size_t chunk_size = std::min(decoder_feed_data_size, recorded_data.size() - offset);
        service::RawBuffer chunk(recorded_data.data() + offset, chunk_size);
        auto feed_result = decoder->write_stream(
                               register_result.value(), output_name, chunk, AUDIO_CODEC_START_TIMEOUT_MS
                           );
        BROOKESIA_CHECK_FALSE_RETURN(
            feed_result == service::AudioWriteResult::Written, false, "Failed to feed %1% decoder data: %2%",
            format_str, BROOKESIA_DESCRIBE_TO_STR(feed_result)
        );
    }

    BROOKESIA_LOGI("Codec replay waiting for stream drain");
    {
        std::unique_lock<std::mutex> lock(decoder_drain_mutex);
        decoder_stream_drained = decoder->is_stream_drained(register_result.value(), output_name);
        const auto drained = decoder_drain_cv.wait_for(
        lock, std::chrono::milliseconds(AUDIO_PLAYBACK_FINISH_TIMEOUT_MS), [&decoder_stream_drained]() {
            return decoder_stream_drained;
        }
                             );
        BROOKESIA_CHECK_FALSE_RETURN(drained, false, "Timed out waiting for %1% decoder stream to drain", format_str);
    }

    auto stop_dec_result = decoder->close_stream(register_result.value(), output_name);
    BROOKESIA_CHECK_FALSE_RETURN(
        stop_dec_result, false, "Failed to stop %1% decoder: %2%", format_str, stop_dec_result.error()
    );
    auto release_result = decoder->release_output(register_result.value(), output_name);
    BROOKESIA_CHECK_FALSE_RETURN(
        release_result, false, "Failed to release %1% decoder output: %2%", format_str, release_result.error()
    );
    auto unregister_result = decoder->unregister_source(register_result.value());
    BROOKESIA_CHECK_FALSE_RETURN(
        unregister_result, false, "Failed to unregister %1% decoder source: %2%", format_str, unregister_result.error()
    );
    BROOKESIA_LOGI("Codec replay finished: %1%", format_str);

    return true;
}
