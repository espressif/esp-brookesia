/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_AUDIO_PROCESSOR_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif

#include <cstring>
#include <memory>
#include <new>
#include <utility>
#include "boost/thread.hpp"

#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_RECORDING_IMPL
#   include <cstdlib>
#   include "esp_gmf_afe.h"
#   include "model_path.h"
#endif
#include "brookesia/lib_utils/function_guard.hpp"
#include "private/utils.hpp"
#include "processor_impl.hpp"
#include "processor_type_converter.hpp"

#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_IMPL

namespace esp_brookesia::hal {

constexpr size_t PLAYBACK_CLOSE_THREAD_STACK_SIZE = 8 * 1024;

#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_RECORDING_IMPL
constexpr size_t GET_WAKE_WORDS_THREAD_STACK_SIZE = 5 * 1024;
constexpr size_t RECORDER_THREAD_STACK_SIZE = 5 * 1024;

namespace {

bool audio_recorder_open_wrapper(const audio_recorder_config_t *config, audio_recorder_handle_t *recorder_handle)
{
    BROOKESIA_LOG_TRACE_GUARD();

    esp_err_t recorder_open_result = ESP_OK;
    auto recorder_open_func = [&]() {
        BROOKESIA_LOG_TRACE_GUARD();
        recorder_open_result = audio_recorder_open(const_cast<audio_recorder_config_t *>(config), recorder_handle);
    };
    if (!lib_utils::ThreadConfig::check_stack_cache_safe()) {
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_size = RECORDER_THREAD_STACK_SIZE,
            .stack_in_ext = false,
        });
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            boost::thread(recorder_open_func).join(), false, "Failed to open recorder in new thread"
        );
    } else {
        recorder_open_func();
    }
    BROOKESIA_CHECK_ESP_ERR_RETURN(recorder_open_result, false, "Failed to open recorder");

    return true;
}

bool audio_recorder_close_wrapper(audio_recorder_handle_t recorder_handle)
{
    BROOKESIA_LOG_TRACE_GUARD();

    esp_err_t recorder_close_result = ESP_OK;
    auto recorder_close_func = [&]() {
        BROOKESIA_LOG_TRACE_GUARD();
        recorder_close_result = audio_recorder_close(recorder_handle);
    };
    if (!lib_utils::ThreadConfig::check_stack_cache_safe()) {
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_size = RECORDER_THREAD_STACK_SIZE,
            .stack_in_ext = false,
        });
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            boost::thread(recorder_close_func).join(), false, "Failed to close recorder in new thread"
        );
    } else {
        recorder_close_func();
    }
    BROOKESIA_CHECK_ESP_ERR_RETURN(recorder_close_result, false, "Failed to close recorder");

    return true;
}

} // namespace
#endif

AudioProcessorCore::AudioProcessorCore(
    std::shared_ptr<audio::CodecPlayerIface> player_iface,
    std::shared_ptr<audio::CodecRecorderIface> recorder_iface,
    AudioProcessorConfig config
)
    : player_iface_(std::move(player_iface))
    , recorder_iface_(std::move(recorder_iface))
    , config_(std::move(config))
{
}

AudioProcessorCore::~AudioProcessorCore()
{
    clear_playback_callback();
#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_RECORDING_IMPL
    stop_encoder();
    stop_decoder();
#endif

    std::lock_guard lock(mutex_);
    open_ref_count_ = 0;
    close_common();
}

bool AudioProcessorCore::acquire(audio::PlaybackIface::EventCallback callback)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(mutex_);
    if (is_opened_) {
        if (callback) {
            std::lock_guard callback_lock(playback_callback_mutex_);
            playback_callback_ = std::move(callback);
        }
        open_ref_count_++;
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(open_common(std::move(callback)), false, "Failed to open audio processor core");
    open_ref_count_ = 1;

    return true;
}

void AudioProcessorCore::release()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(mutex_);
    if (open_ref_count_ == 0) {
        return;
    }

    open_ref_count_--;
    if (open_ref_count_ == 0) {
        close_common();
    }
}

void AudioProcessorCore::clear_playback_callback()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    disable_playback_callback();
    wait_playback_callbacks();
}

void AudioProcessorCore::disable_playback_callback()
{
    audio::PlaybackIface::EventCallback previous;
    {
        boost::lock_guard lock(playback_callback_mutex_);
        previous = std::move(playback_callback_);
    }
}

void AudioProcessorCore::wait_playback_callbacks()
{
    boost::unique_lock lock(playback_callback_mutex_);
    // Drain before taking the lifecycle lock: a callback may still use the HAL.
    playback_callback_condition_.wait(lock, [this]() {
        return playback_callbacks_in_flight_ == 0;
    });
}

bool AudioProcessorCore::inside_playback_callback() const
{
    for (auto *frame = playback_callback_frame_; frame != nullptr; frame = frame->previous) {
        if (frame->owner == this) {
            return true;
        }
    }
    return false;
}

void AudioProcessorCore::retain_after_deferred_close_failure(std::shared_ptr<AudioProcessorCore> self)
{
    boost::lock_guard lock(playback_callback_mutex_);
    retained_after_close_failure_ = std::move(self);
}

bool AudioProcessorCore::open_common(audio::PlaybackIface::EventCallback callback)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_NULL_RETURN(player_iface_, false, "Invalid player interface");
    BROOKESIA_CHECK_NULL_RETURN(recorder_iface_, false, "Invalid recorder interface");
    const auto &recorder_info = recorder_iface_->get_info();
    auto player_config = config_.playback.player;
    // Player and recorder can share an I2S clock. The recorder has no dynamic
    // format API, so keep both endpoints on its declared format.
    if ((player_config.bits != recorder_info.bits) ||
            (player_config.channels != recorder_info.channels) ||
            (player_config.sample_rate != recorder_info.sample_rate)) {
        BROOKESIA_LOGW(
            "Playback format differs from recorder; using recorder format(bits=%1%, channels=%2%, rate=%3%) "
            "for shared-clock compatibility",
            recorder_info.bits, recorder_info.channels, recorder_info.sample_rate
        );
        player_config.bits = recorder_info.bits;
        player_config.channels = recorder_info.channels;
        player_config.sample_rate = recorder_info.sample_rate;
    }

    BROOKESIA_CHECK_FALSE_RETURN(player_iface_->open(player_config), false, "Failed to open audio DAC");
    lib_utils::FunctionGuard close_player_guard([this]() {
        player_iface_->close();
    });

    BROOKESIA_CHECK_FALSE_RETURN(recorder_iface_->open(), false, "Failed to open audio ADC");
    lib_utils::FunctionGuard close_recorder_guard([this]() {
        recorder_iface_->close();
    });

    auto player_write_cb = +[](uint8_t *data, int size, void *ctx) {
        auto player_iface = static_cast<audio::CodecPlayerIface *>(ctx);
        BROOKESIA_CHECK_NULL_RETURN(player_iface, ESP_FAIL, "Invalid player context");
        return player_iface->write_data(data, size) ? ESP_OK : ESP_FAIL;
    };
    auto recorder_read_cb = +[](uint8_t *data, int size, void *ctx) {
        auto self = static_cast<AudioProcessorCore *>(ctx);
        BROOKESIA_CHECK_NULL_RETURN(self, ESP_FAIL, "Invalid processor context");
        BROOKESIA_CHECK_NULL_RETURN(self->recorder_iface_, ESP_FAIL, "Invalid recorder interface");

        auto result = self->recorder_iface_->read_data(data, size);
#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_RECORDING_IMPL
        if (result) {
            self->on_recorder_input_data(data, size);
        }
#endif

        return result ? ESP_OK : ESP_FAIL;
    };

    audio_manager_config_t manager_config = {
        .play_io = {
            .audio_info = {
                .sample_rate = player_config.sample_rate,
                .sample_bits = player_config.bits,
                .channels = player_config.channels,
                .frame_duration = 0,
            },
            .read_cb = nullptr,
            .read_ctx = nullptr,
            .write_cb = player_write_cb,
            .write_ctx = player_iface_.get(),
            .drain_cb = nullptr,
            .drain_ctx = nullptr,
        },
        .rec_io = {
            .audio_info = {
                .sample_rate = recorder_info.sample_rate,
                .sample_bits = recorder_info.bits,
                .channels = recorder_info.channels,
                .frame_duration = 0,
            },
            .read_cb = recorder_read_cb,
            .read_ctx = this,
            .write_cb = nullptr,
            .write_ctx = nullptr,
            .drain_cb = nullptr,
            .drain_ctx = nullptr,
        },
        .mic_layout = {0},
    };
    if (!recorder_info.mic_layout.empty()) {
        std::strncpy(
            manager_config.mic_layout, recorder_info.mic_layout.c_str(), sizeof(manager_config.mic_layout) - 1
        );
        manager_config.mic_layout[sizeof(manager_config.mic_layout) - 1] = '\0';
    }
    BROOKESIA_CHECK_ESP_ERR_RETURN(audio_manager_init(&manager_config), false, "Failed to initialize audio manager");
    lib_utils::FunctionGuard manager_guard([]() {
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_manager_deinit(), {}, {
            BROOKESIA_LOGE("Failed to deinitialize audio manager");
        });
    });

    auto playback_event_cb = +[](audio_play_state_t state, void *ctx) {
        auto self = static_cast<AudioProcessorCore *>(ctx);
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid processor context");
        self->on_playback_event(static_cast<uint8_t>(state));
    };
    auto playback_config = AudioProcessorTypeConverter::convert(
                               config_.playback, reinterpret_cast<const void *>(playback_event_cb), this
                           );
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        audio_playback_open(&playback_config, reinterpret_cast<audio_playback_handle_t *>(&playback_handle_)),
        false, "Failed to open playback"
    );

    {
        std::lock_guard callback_lock(playback_callback_mutex_);
        playback_callback_ = std::move(callback);
    }
    is_opened_ = true;

    manager_guard.release();
    close_player_guard.release();
    close_recorder_guard.release();

    return true;
}

void AudioProcessorCore::close_common()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_opened_) {
        return;
    }

    {
        std::lock_guard callback_lock(playback_callback_mutex_);
        playback_callback_ = nullptr;
    }
    if (playback_handle_ != nullptr) {
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(
        audio_playback_close(AudioProcessorTypeConverter::to_playback_handle(playback_handle_)), {}, {
            BROOKESIA_LOGE("Failed to close playback");
        }
        );
        playback_handle_ = nullptr;
    }

    BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_manager_deinit(), {}, {
        BROOKESIA_LOGE("Failed to deinitialize audio manager");
    });

    if (player_iface_) {
        player_iface_->close();
    }
    if (recorder_iface_) {
        recorder_iface_->close();
    }

    is_opened_ = false;
}

bool AudioProcessorCore::play(const std::string &url)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_opened_, false, "Audio processor core is not opened");
    auto ret = audio_playback_play(AudioProcessorTypeConverter::to_playback_handle(playback_handle_), url.c_str());
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to play URL: %1%", url);

    return true;
}

bool AudioProcessorCore::pause()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_opened_, false, "Audio processor core is not opened");
    auto ret = audio_playback_pause(AudioProcessorTypeConverter::to_playback_handle(playback_handle_));
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to pause playback");

    return true;
}

bool AudioProcessorCore::resume()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_opened_, false, "Audio processor core is not opened");
    auto ret = audio_playback_resume(AudioProcessorTypeConverter::to_playback_handle(playback_handle_));
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to resume playback");

    return true;
}

bool AudioProcessorCore::stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_opened_, false, "Audio processor core is not opened");
    auto ret = audio_playback_stop(AudioProcessorTypeConverter::to_playback_handle(playback_handle_));
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to stop playback");

    return true;
}

#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_RECORDING_IMPL
std::vector<std::string> AudioProcessorCore::get_afe_wake_words()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    const auto &afe_config = config_.afe;
    if (!afe_config.wakenet.has_value()) {
        return {};
    }

    auto partition_label = afe_config.wakenet.value().model_partition_label;
    if (partition_label.empty()) {
        return {};
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

            std::string wake_words(wake_words_str);
            free(wake_words_str);

            size_t pos = 0;
            while ((pos = wake_words.find(';')) != std::string::npos) {
                std::string wake_word = wake_words.substr(0, pos);
                wake_word.erase(0, wake_word.find_first_not_of(" \t"));
                wake_word.erase(wake_word.find_last_not_of(" \t") + 1);
                if (!wake_word.empty()) {
                    wake_words_array.push_back(wake_word);
                }
                wake_words.erase(0, pos + 1);
            }
            wake_words.erase(0, wake_words.find_first_not_of(" \t"));
            wake_words.erase(wake_words.find_last_not_of(" \t") + 1);
            if (!wake_words.empty()) {
                wake_words_array.push_back(wake_words);
            }
        }
    };

    if (lib_utils::ThreadConfig::check_stack_cache_safe() || (get_static_srmodels() != nullptr)) {
        get_wake_words();
    } else {
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_size = GET_WAKE_WORDS_THREAD_STACK_SIZE,
            .stack_in_ext = false,
        });
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            boost::thread(get_wake_words).join(), std::vector<std::string>(), "Failed to get wake words in new thread"
        );
    }

    return wake_words_array;
}

bool AudioProcessorCore::start_encoder(
    const audio::EncoderDynamicConfig &dynamic_config, audio::EncoderIface::Callbacks callbacks
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_encoder_started()) {
        BROOKESIA_LOGD("Encoder is already running");
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(acquire(), false, "Failed to acquire audio processor core");
    lib_utils::FunctionGuard release_guard([this]() {
        release();
    });

    auto event_cb = +[](audio_recorder_handle_t recorder, void *event, void *ctx) {
        (void)recorder;
        auto self = static_cast<AudioProcessorCore *>(ctx);
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid processor context");
        self->on_recorder_event(event);
    };
    const auto afe_config = dynamic_config.enable_afe ? config_.afe : AudioProcessorAFE_Config{};
    auto recorder_config = AudioProcessorTypeConverter::convert(
                               config_.encoder, dynamic_config, afe_config, afe_model_partition_label_,
                               afe_mn_language_, reinterpret_cast<const void *>(event_cb), this
                           );
    auto *recorder_handle_ptr = reinterpret_cast<audio_recorder_handle_t *>(&recorder_handle_);

    BROOKESIA_CHECK_FALSE_RETURN(
        audio_recorder_open_wrapper(&recorder_config, recorder_handle_ptr), false, "Failed to open recorder"
    );
    auto recorder_handle = AudioProcessorTypeConverter::to_recorder_handle(recorder_handle_);
    lib_utils::FunctionGuard close_recorder_guard([recorder_handle]() {
        BROOKESIA_CHECK_FALSE_EXECUTE(audio_recorder_close_wrapper(recorder_handle), {}, {
            BROOKESIA_LOGE("Failed to close recorder");
        });
    });

    encoder_callbacks_ = std::move(callbacks);

    close_recorder_guard.release();
    release_guard.release();
    is_encoder_started_ = true;
    is_encoder_paused_ = false;

    return true;
}

int AudioProcessorCore::read_encoded_data(uint8_t *data, size_t size)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_encoder_started(), -1, "Encoder is not running");
    BROOKESIA_CHECK_NULL_RETURN(data, -1, "Invalid encoder data buffer");
    if (size == 0) {
        return 0;
    }
    if (is_encoder_paused()) {
        return 0;
    }

    auto recorder_handle = AudioProcessorTypeConverter::to_recorder_handle(recorder_handle_);
    BROOKESIA_CHECK_NULL_RETURN(recorder_handle, -1, "Invalid recorder handle");

    int read_ret = audio_recorder_read_data(recorder_handle, data, size);
    return read_ret;
}

void AudioProcessorCore::stop_encoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_encoder_started()) {
        return;
    }

    auto recorder_handle = AudioProcessorTypeConverter::to_recorder_handle(recorder_handle_);
    BROOKESIA_CHECK_FALSE_EXECUTE(audio_recorder_close_wrapper(recorder_handle), {}, {
        BROOKESIA_LOGE("Failed to close recorder");
    });
    recorder_handle_ = nullptr;
    encoder_callbacks_ = {};
    is_encoder_started_ = false;
    is_encoder_paused_ = false;
    release();
}

void AudioProcessorCore::pause_encoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    is_encoder_paused_ = true;
}

void AudioProcessorCore::resume_encoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    is_encoder_paused_ = false;
}

bool AudioProcessorCore::start_decoder(const audio::DecoderDynamicConfig &dynamic_config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_decoder_started()) {
        BROOKESIA_LOGD("Decoder is already running");
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(acquire(), false, "Failed to acquire audio processor core");
    lib_utils::FunctionGuard release_guard([this]() {
        release();
    });

    auto feeder_config = AudioProcessorTypeConverter::convert(config_.decoder, dynamic_config);
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        audio_feeder_open(&feeder_config, reinterpret_cast<audio_feeder_handle_t *>(&feeder_handle_)), false,
        "Failed to open feeder"
    );
    auto feeder_handle = AudioProcessorTypeConverter::to_feeder_handle(feeder_handle_);
    lib_utils::FunctionGuard close_feeder_guard([feeder_handle]() {
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_feeder_close(feeder_handle), {}, {
            BROOKESIA_LOGE("Failed to close feeder");
        });
    });

    auto playback_handle = AudioProcessorTypeConverter::to_playback_handle(playback_handle_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        audio_processor_mixer_open(playback_handle, feeder_handle), false, "Failed to open mixer"
    );
    lib_utils::FunctionGuard close_mixer_guard([]() {
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_processor_mixer_close(), {}, {
            BROOKESIA_LOGE("Failed to close mixer");
        });
    });

    BROOKESIA_CHECK_ESP_ERR_RETURN(audio_feeder_start(feeder_handle), false, "Failed to start feeder");

    close_feeder_guard.release();
    close_mixer_guard.release();
    release_guard.release();
    is_decoder_started_ = true;

    return true;
}

void AudioProcessorCore::stop_decoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_decoder_started()) {
        return;
    }

    BROOKESIA_CHECK_ESP_ERR_EXECUTE(
    audio_feeder_close(AudioProcessorTypeConverter::to_feeder_handle(feeder_handle_)), {}, {
        BROOKESIA_LOGE("Failed to close feeder");
    }
    );
    feeder_handle_ = nullptr;
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_processor_mixer_close(), {}, {
        BROOKESIA_LOGE("Failed to close mixer");
    });
    is_decoder_started_ = false;
    release();
}

bool AudioProcessorCore::feed_decoder_data(const uint8_t *data, size_t size)
{
    BROOKESIA_CHECK_FALSE_RETURN(is_decoder_started_, false, "Decoder is not running");
    BROOKESIA_CHECK_NULL_RETURN(data, false, "Invalid decoder data");

    auto feeder_handle = AudioProcessorTypeConverter::to_feeder_handle(feeder_handle_);
    auto result = audio_feeder_feed_data(feeder_handle, const_cast<uint8_t *>(data), size);
    BROOKESIA_CHECK_ESP_ERR_RETURN(result, false, "Failed to feed decoder data");

    return true;
}
#endif

void AudioProcessorCore::on_playback_event(uint8_t state)
{
    audio::PlayState new_state = audio::PlayState::Idle;
    switch (static_cast<audio_play_state_t>(state)) {
    case AUDIO_PLAY_STATE_IDLE:
    case AUDIO_PLAY_STATE_FINISHED:
    case AUDIO_PLAY_STATE_STOPPED:
        new_state = audio::PlayState::Idle;
        break;
    case AUDIO_PLAY_STATE_PLAYING:
        new_state = audio::PlayState::Playing;
        break;
    case AUDIO_PLAY_STATE_PAUSED:
        new_state = audio::PlayState::Paused;
        break;
    default:
        BROOKESIA_LOGE("Invalid playback state: %1%", state);
        return;
    }

    audio::PlaybackIface::EventCallback callback;
    {
        std::lock_guard lock(playback_callback_mutex_);
        callback = playback_callback_;
        if (callback) {
            ++playback_callbacks_in_flight_;
        }
    }
    if (callback) {
        PlaybackCallbackFrame frame{this, playback_callback_frame_};
        playback_callback_frame_ = &frame;
        lib_utils::FunctionGuard callback_guard([this, &frame]() {
            playback_callback_frame_ = frame.previous;
            std::lock_guard lock(playback_callback_mutex_);
            if (--playback_callbacks_in_flight_ == 0) {
                playback_callback_condition_.notify_all();
            }
        });
        callback(new_state);
    }
}

#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_RECORDING_IMPL
void AudioProcessorCore::on_recorder_event(void *event)
{
    auto afe_evt = static_cast<esp_gmf_afe_evt_t *>(event);
    BROOKESIA_CHECK_NULL_EXIT(afe_evt, "AFE event is null");

    switch (afe_evt->type) {
    case ESP_GMF_AFE_EVT_WAKEUP_START:
        emit_afe_event(audio::AfeEvent::WakeStart);
        break;
    case ESP_GMF_AFE_EVT_VAD_START:
        emit_afe_event(audio::AfeEvent::VAD_Start);
        break;
    case ESP_GMF_AFE_EVT_VAD_END:
        emit_afe_event(audio::AfeEvent::VAD_End);
        break;
    case ESP_GMF_AFE_EVT_VCMD_DECTECTED:
#if BROOKESIA_HAL_ADAPTOR_AUDIO_PROCESSOR_AFE_USE_CUSTOM
        emit_afe_event(audio::AfeEvent::WakeStart);
#endif
        break;
    case ESP_GMF_AFE_EVT_VCMD_DECT_TIMEOUT:
    default:
        BROOKESIA_LOGD("Unsupported AFE event: %1%", afe_evt->type);
        break;
    }
}

void AudioProcessorCore::on_recorder_input_data(const uint8_t *data, size_t size)
{
    audio::EncoderIface::DataCallback callback;
    {
        std::lock_guard lock(mutex_);
        callback = encoder_callbacks_.recorder_data;
    }
    if (callback) {
        callback(data, size);
    }
}

void AudioProcessorCore::emit_afe_event(audio::AfeEvent event)
{
    audio::EncoderIface::AfeEventCallback callback;
    {
        std::lock_guard lock(mutex_);
        callback = encoder_callbacks_.afe_event;
    }
    if (callback) {
        callback(event);
    }
}
#endif

AudioPlaybackImpl::AudioPlaybackImpl(std::shared_ptr<AudioProcessorCore> core)
    : core_(std::move(core))
{
}

AudioPlaybackImpl::~AudioPlaybackImpl()
{
    close();
}

bool AudioPlaybackImpl::open(EventCallback callback)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto lifecycle = lifecycle_;
    {
        boost::lock_guard lock(lifecycle->mutex);
        if (lifecycle->opened) {
            return true;
        }
        if (lifecycle->opening || lifecycle->closing || lifecycle->close_failed) {
            return false;
        }
        lifecycle->opening = true;
    }
    bool acquired = false;
    try {
        acquired = core_ && core_->acquire(std::move(callback));
    } catch (...) {
        boost::lock_guard lock(lifecycle->mutex);
        lifecycle->opening = false;
        lifecycle->condition.notify_all();
        throw;
    }
    boost::lock_guard lock(lifecycle->mutex);
    lifecycle->opening = false;
    lifecycle->acquired = acquired;
    lifecycle->opened = acquired && !lifecycle->closing && !lifecycle->close_failed;
    lifecycle->condition.notify_all();
    return lifecycle->opened;
}

void AudioPlaybackImpl::close()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto lifecycle = lifecycle_;
    auto core = core_;
    const bool from_callback = core && core->inside_playback_callback();
    {
        boost::unique_lock lock(lifecycle->mutex);
        if (lifecycle->closing) {
            if (!from_callback && active_close_lifecycle_ != lifecycle.get()) {
                lifecycle->condition.wait(lock, [&]() {
                    return !lifecycle->closing;
                });
            }
            return;
        }
        if (!lifecycle->opened && !lifecycle->opening) {
            return;
        }
        lifecycle->closing = true;
        lifecycle->opened = false;
    }
    auto finish_close = [lifecycle, core]() {
        auto *previous_close = active_close_lifecycle_;
        active_close_lifecycle_ = lifecycle.get();
        lib_utils::FunctionGuard close_guard([previous_close]() {
            active_close_lifecycle_ = previous_close;
        });
        bool acquired = false;
        {
            boost::unique_lock lock(lifecycle->mutex);
            lifecycle->condition.wait(lock, [&]() {
                return !lifecycle->opening;
            });
            acquired = lifecycle->acquired;
        }
        if (core && acquired) {
            core->clear_playback_callback();
            core->release();
        }
        boost::lock_guard lock(lifecycle->mutex);
        lifecycle->acquired = false;
        lifecycle->closing = false;
        lifecycle->condition.notify_all();
    };
    if (!from_callback) {
        finish_close();
        return;
    }
    // The SDK closes by waiting for its event task. Never join that task from its own callback.
    core->disable_playback_callback();
    try {
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_size = PLAYBACK_CLOSE_THREAD_STACK_SIZE,
            .stack_in_ext = false,
        });
        boost::thread(std::move(finish_close)).detach();
    } catch (const std::exception &error) {
        // Preserve the SDK callback context if no worker could be created.
        core->retain_after_deferred_close_failure(core);
        boost::lock_guard lock(lifecycle->mutex);
        lifecycle->close_failed = true;
        lifecycle->closing = false;
        lifecycle->condition.notify_all();
        BROOKESIA_LOGE("Failed to schedule playback close; retaining audio resources: %1%", error.what());
    }
}

bool AudioPlaybackImpl::play(const std::string &url)
{
    BROOKESIA_CHECK_FALSE_RETURN(is_opened(), false, "Audio playback is not opened");
    return core_->play(url);
}

bool AudioPlaybackImpl::pause()
{
    BROOKESIA_CHECK_FALSE_RETURN(is_opened(), false, "Audio playback is not opened");
    return core_->pause();
}

bool AudioPlaybackImpl::resume()
{
    BROOKESIA_CHECK_FALSE_RETURN(is_opened(), false, "Audio playback is not opened");
    return core_->resume();
}

bool AudioPlaybackImpl::stop()
{
    BROOKESIA_CHECK_FALSE_RETURN(is_opened(), false, "Audio playback is not opened");
    return core_->stop();
}

#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_RECORDING_IMPL
AudioEncoderImpl::AudioEncoderImpl(std::shared_ptr<AudioProcessorCore> core)
    : core_(std::move(core))
{
}

AudioEncoderImpl::~AudioEncoderImpl()
{
    stop();
}

std::vector<std::string> AudioEncoderImpl::get_afe_wake_words()
{
    BROOKESIA_CHECK_NULL_RETURN(core_, std::vector<std::string>(), "Invalid audio processor core");
    return core_->get_afe_wake_words();
}

bool AudioEncoderImpl::start(const audio::EncoderDynamicConfig &config, Callbacks callbacks)
{
    BROOKESIA_CHECK_NULL_RETURN(core_, false, "Invalid audio processor core");
    return core_->start_encoder(config, std::move(callbacks));
}

int AudioEncoderImpl::read_encoded_data(uint8_t *data, size_t size)
{
    BROOKESIA_CHECK_NULL_RETURN(core_, -1, "Invalid audio processor core");
    return core_->read_encoded_data(data, size);
}

void AudioEncoderImpl::stop()
{
    if (core_) {
        core_->stop_encoder();
    }
}

void AudioEncoderImpl::pause()
{
    if (core_) {
        core_->pause_encoder();
    }
}

void AudioEncoderImpl::resume()
{
    if (core_) {
        core_->resume_encoder();
    }
}

bool AudioEncoderImpl::is_started() const
{
    return core_ && core_->is_encoder_started();
}

bool AudioEncoderImpl::is_paused() const
{
    return core_ && core_->is_encoder_paused();
}

AudioDecoderImpl::AudioDecoderImpl(std::shared_ptr<AudioProcessorCore> core)
    : core_(std::move(core))
{
}

AudioDecoderImpl::~AudioDecoderImpl()
{
    stop();
}

bool AudioDecoderImpl::start(const audio::DecoderDynamicConfig &config)
{
    BROOKESIA_CHECK_NULL_RETURN(core_, false, "Invalid audio processor core");
    return core_->start_decoder(config);
}

void AudioDecoderImpl::stop()
{
    if (core_) {
        core_->stop_decoder();
    }
}

bool AudioDecoderImpl::is_started() const
{
    return core_ && core_->is_decoder_started();
}

bool AudioDecoderImpl::feed_data(const uint8_t *data, size_t size)
{
    BROOKESIA_CHECK_NULL_RETURN(core_, false, "Invalid audio processor core");
    return core_->feed_decoder_data(data, size);
}
#endif

} // namespace esp_brookesia::hal
#endif // BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_IMPL
