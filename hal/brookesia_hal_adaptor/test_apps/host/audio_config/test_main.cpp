/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include "audio_processor.h"
#include "codec_shared.h"

#define BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_RECORDING_IMPL 1
#define BROOKESIA_LOG_TRACE_GUARD_WITH_THIS() ((void)0)
#define BROOKESIA_LOGE(...) ((void)0)
#define BROOKESIA_LOGW(...) ((void)0)
#define BROOKESIA_CHECK_NULL_RETURN(value, result, ...) if (!(value)) { return result; }
#define BROOKESIA_CHECK_FALSE_RETURN(value, result, ...) if (!(value)) { return result; }
#define BROOKESIA_CHECK_ESP_ERR_RETURN(value, result, ...) if ((value) != ESP_OK) { return result; }
#define BROOKESIA_CHECK_NULL_EXIT(value, ...) if (!(value)) { return; }
#define BROOKESIA_CHECK_ESP_ERR_EXECUTE(value, unused, action) if ((value) != ESP_OK) action

namespace lib_utils {
struct ThreadConfig {
    unsigned stack_size = 8192;
    unsigned priority = 5;
    int core_id = 1;
    bool stack_in_ext = true;
};
class FunctionGuard {
public:
    explicit FunctionGuard(std::function<void()> callback) : callback_(std::move(callback)) {}
    ~FunctionGuard()
    {
        if (callback_) {
            callback_();
        }
    }
    void release()
    {
        callback_ = nullptr;
    }
private:
    std::function<void()> callback_;
};
}

namespace audio {
struct Format {
    uint8_t bits = 16;
    uint8_t channels = 1;
    uint32_t sample_rate = 48000;
};
struct PlaybackIface {
    using EventCallback = std::function<void()>;
};
struct CodecPlayerIface {
    Format opened;
    bool fail_open = false;
    bool fail_write = false;
    int closes = 0;
    int writes = 0;
    bool open(const Format &config)
    {
        opened = config;
        return !fail_open && shared_codec_open(true, config.bits, config.channels, config.sample_rate) == ESP_OK;
    }
    void close()
    {
        ++closes;
        shared_codec_close(true);
    }
    bool write_data(uint8_t *, int)
    {
        ++writes;
        return !fail_write;
    }
};
struct Recorder {
    struct Info : Format {
        std::string mic_layout = "MR";
    } info;
    bool fail_open = false;
    bool fail_read = false;
    int closes = 0;
    int reads = 0;
    const Info &get_info()
    {
        return info;
    }
    bool open()
    {
        return !fail_open && shared_codec_open(false, info.bits, info.channels, info.sample_rate) == ESP_OK;
    }
    void close()
    {
        ++closes;
        shared_codec_close(false);
    }
    bool read_data(uint8_t *, int)
    {
        ++reads;
        return !fail_read;
    }
};
struct DecoderDynamicConfig {};
}
struct AudioProcessorMixerGainConfig {
    float initial_gain = 0.25f;
    float target_gain = 0.75f;
    int transition_time = 42;
};
struct AudioProcessorPlaybackConfig {
    audio::Format player;
    lib_utils::ThreadConfig player_task;
    AudioProcessorMixerGainConfig mixer_gain;
};
struct AudioProcessorDecoderConfig {
    lib_utils::ThreadConfig feeder_task;
    AudioProcessorMixerGainConfig mixer_gain;
};
using audio_playback_config_t = audio_play_config_t;
using audio_playback_handle_t = audio_play_handle_t;
class AudioProcessorTypeConverter {
public:
    static audio_task_config_t convert(const lib_utils::ThreadConfig &);
    static audio_mixer_gain_config_t convert(const AudioProcessorMixerGainConfig &);
    static audio_playback_config_t convert(const AudioProcessorPlaybackConfig &, const void *, void *);
    static audio_feeder_config_t convert(const AudioProcessorDecoderConfig &, const audio::DecoderDynamicConfig &);
    // Decoder codec mapping is outside this suite's configuration boundary.
    static av_processor_decoder_config_t convert(const audio::DecoderDynamicConfig &)
    {
        return {};
    }
};
class AudioProcessorCore {
public:
    std::shared_ptr<audio::CodecPlayerIface> player_iface_ = std::make_shared<audio::CodecPlayerIface>();
    std::shared_ptr<audio::Recorder> recorder_iface_ = std::make_shared<audio::Recorder>();
    struct {
        AudioProcessorPlaybackConfig playback;
    } config_;
    audio::PlaybackIface::EventCallback playback_callback_;
    bool is_opened_ = false;
    void *playback_handle_ = nullptr;
    unsigned input_events = 0;
    uint8_t last_state = 255;
    void on_recorder_input_data(uint8_t *, int)
    {
        ++input_events;
    }
    void on_playback_event(uint8_t state)
    {
        last_state = state;
    }
    bool open_common(audio::PlaybackIface::EventCallback);
};

static audio_manager_config_t captured_manager;
static audio_play_config_t captured_playback;
static bool fail_manager;
static bool fail_playback;
static int manager_opens;
static int manager_closes;
static int playback_opens;

extern "C" esp_err_t audio_manager_init(audio_manager_config_t *config)
{
    captured_manager = *config;
    ++manager_opens;
    return fail_manager ? ESP_FAIL : ESP_OK;
}

extern "C" esp_err_t audio_manager_deinit()
{
    ++manager_closes;
    return ESP_OK;
}

extern "C" esp_err_t audio_playback_open(audio_play_config_t *config, audio_play_handle_t *handle)
{
    ++playback_opens;
    captured_playback = *config;
    if (fail_playback) {
        return ESP_FAIL;
    }
    *handle = reinterpret_cast<audio_play_handle_t>(uintptr_t{1});
    return ESP_OK;
}

#include "production.inc"

static void reset()
{
    shared_codec_reset();
    fail_manager = false;
    fail_playback = false;
    manager_opens = 0;
    manager_closes = 0;
    playback_opens = 0;
}

static void check_success()
{
    reset();
    AudioProcessorCore core;
    core.recorder_iface_->info.bits = 32;
    core.recorder_iface_->info.channels = 2;
    core.recorder_iface_->info.sample_rate = 16000;
    core.recorder_iface_->info.mic_layout = "MRabcdefgh";
    bool event = false;
    assert(core.open_common([&event]() {
        event = true;
    }));
    assert(core.is_opened_ && core.playback_handle_ != nullptr);
    assert(manager_opens == 1 && manager_closes == 0 && playback_opens == 1);
    assert(core.player_iface_->closes == 0 && core.recorder_iface_->closes == 0);
    const auto &play = captured_manager.play_io;
    const auto &rec = captured_manager.rec_io;
    assert(core.player_iface_->opened.sample_rate == 16000);
    assert(core.player_iface_->opened.channels == 2 && core.player_iface_->opened.bits == 32);
    assert(play.audio_info.sample_rate == 16000 && play.audio_info.sample_bits == 32);
    assert(play.audio_info.channels == 2 && play.audio_info.frame_duration == 0);
    assert(core.config_.playback.player.sample_rate == 48000);
    assert(core.config_.playback.player.channels == 1 && core.config_.playback.player.bits == 16);
    assert(shared_codec_format_rejections() == 0);
    assert(rec.audio_info.sample_rate == 16000 && rec.audio_info.sample_bits == 32);
    assert(rec.audio_info.channels == 2 && rec.audio_info.frame_duration == 0);
    assert(std::strcmp(captured_manager.mic_layout, "MRabcde") == 0);
    assert(play.read_cb == nullptr && rec.write_cb == nullptr);
    assert(play.drain_cb == nullptr && rec.drain_cb == nullptr);
    assert(play.write_ctx == core.player_iface_.get() && rec.read_ctx == &core);
    uint8_t data[8] = {};
    assert(play.write_cb(data, sizeof(data), play.write_ctx) == ESP_OK);
    core.player_iface_->fail_write = true;
    assert(play.write_cb(data, sizeof(data), play.write_ctx) == ESP_FAIL);
    assert(core.player_iface_->writes == 2);
    assert(rec.read_cb(data, sizeof(data), rec.read_ctx) == ESP_OK);
    assert(core.input_events == 1);
    core.recorder_iface_->fail_read = true;
    assert(rec.read_cb(data, sizeof(data), rec.read_ctx) == ESP_FAIL);
    assert(core.input_events == 1 && core.recorder_iface_->reads == 2);
    captured_playback.event_cb(AUDIO_PLAY_STATE_PLAYING, captured_playback.event_cb_ctx);
    assert(core.last_state == AUDIO_PLAY_STATE_PLAYING);
    core.playback_callback_();
    assert(event);
    auto feeder = AudioProcessorTypeConverter::convert(AudioProcessorDecoderConfig{}, audio::DecoderDynamicConfig{});
    assert(captured_playback.priority == AUDIO_POLICY_PRIORITY_DEFAULT);
    assert(feeder.priority == captured_playback.priority);
    assert(!captured_playback.block_until_done);
    assert(captured_playback.task_config.task_stack == 8192 && captured_playback.task_config.task_core == 1);
    assert(captured_playback.mixer_gain.initial_gain == 0.25f);
    assert(feeder.feeder_mixer_gain.target_gain == 0.75f);
}

static void check_failures()
{
    for (int stage = 0; stage < 4; ++stage) {
        reset();
        AudioProcessorCore core;
        core.player_iface_->fail_open = stage == 0;
        core.recorder_iface_->fail_open = stage == 1;
        fail_manager = stage == 2;
        fail_playback = stage == 3;
        assert(!core.open_common([]() {}));
        assert(!core.is_opened_ && core.playback_handle_ == nullptr && !core.playback_callback_);
        assert(core.player_iface_->closes == (stage > 0 ? 1 : 0));
        assert(core.recorder_iface_->closes == (stage > 1 ? 1 : 0));
        assert(manager_opens == (stage > 1 ? 1 : 0));
        assert(manager_closes == (stage == 3 ? 1 : 0));
        assert(playback_opens == (stage == 3 ? 1 : 0));
    }
}

static bool check_shared_clock()
{
    const audio::Format requests[] = {
        {.bits = 16, .channels = 2, .sample_rate = 16000},
        {.bits = 16, .channels = 2, .sample_rate = 16000},
        {.bits = 16, .channels = 1, .sample_rate = 48000},
        {.bits = 16, .channels = 2, .sample_rate = 48000},
    };
    const audio::Format inputs[] = {
        {.bits = 16, .channels = 2, .sample_rate = 48000},
        {.bits = 16, .channels = 2, .sample_rate = 16000},
        {.bits = 16, .channels = 2, .sample_rate = 48000},
        {.bits = 32, .channels = 2, .sample_rate = 48000},
    };
    for (size_t index = 0; index < std::size(requests); ++index) {
        reset();
        AudioProcessorCore core;
        core.config_.playback.player = requests[index];
        static_cast<audio::Format &>(core.recorder_iface_->info) = inputs[index];
        if (!core.open_common([]() {})) {
            return false;
        }
        const auto tx = shared_codec_driver_format(true);
        const auto rx = shared_codec_driver_format(false);
        const auto &input = inputs[index];
        const auto &player = core.player_iface_->opened;
        const auto &metadata = captured_manager.play_io.audio_info;
        const auto &record_metadata = captured_manager.rec_io.audio_info;
        const auto &configured = core.config_.playback.player;
        if (shared_codec_format_rejections() != 0 || tx.sample_rate != input.sample_rate ||
                rx.sample_rate != input.sample_rate || tx.bits_per_sample != input.bits ||
                rx.bits_per_sample != input.bits || tx.channel != input.channels || rx.channel != input.channels ||
                player.sample_rate != input.sample_rate || player.bits != input.bits ||
                player.channels != input.channels ||
                metadata.sample_rate != input.sample_rate || metadata.sample_bits != input.bits ||
                record_metadata.sample_rate != input.sample_rate || record_metadata.sample_bits != input.bits ||
                record_metadata.channels != input.channels ||
                metadata.channels != input.channels || configured.sample_rate != requests[index].sample_rate ||
                configured.bits != requests[index].bits || configured.channels != requests[index].channels) {
            std::cout << "REJECT: shared-I2S driver format or manager metadata mismatch, case " << index << '\n';
            return false;
        }
    }
    return true;
}

static void reproduce_unfixed()
{
    reset();
    AudioProcessorCore core;
    core.config_.playback.player = {.bits = 16, .channels = 2, .sample_rate = 16000};
    core.recorder_iface_->info.bits = 16;
    core.recorder_iface_->info.channels = 2;
    core.recorder_iface_->info.sample_rate = 48000;
    // Real esp_codec_dev_open ignores the actual set_fmt rejection and returns OK.
    assert(core.open_common([]() {}));
    assert(shared_codec_format_rejections() == 1);
    assert(shared_codec_last_format_result() == ESP_CODEC_DEV_NOT_SUPPORT);
    assert(shared_codec_driver_format(true).sample_rate == 16000);
    assert(shared_codec_driver_format(false).sample_rate == 16000);
    assert(captured_manager.rec_io.audio_info.sample_rate == 48000);
    std::cout << "REPRODUCED: codec open succeeds despite shared-I2S 16/48 kHz format rejection\n";
}

int main(int argc, char **argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--reproduce-unfixed") == 0) {
        reproduce_unfixed();
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--clock-regression") == 0) {
        return check_shared_clock() ? 0 : 1;
    }
    assert(check_shared_clock());
    check_success();
    check_failures();
    std::cout << "PASS: HAL audio configuration, callback wiring, priorities and rollback "
              "(real codec format/open slices, fake driver I/O)\n";
}
