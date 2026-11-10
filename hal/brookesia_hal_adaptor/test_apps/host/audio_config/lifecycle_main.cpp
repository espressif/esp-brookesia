/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lifecycle_fixture.inc"

static std::function<void()> before_close;
static std::atomic<int> playback_closes;

extern "C" esp_err_t audio_playback_close(audio_play_handle_t)
{
    ++playback_closes;
    if (before_close) {
        before_close();
    }
    // GMF sends STOPPED on its worker before signalling stop completion.
    auto callback = captured_playback.event_cb;
    auto context = captured_playback.event_cb_ctx;
    std::thread worker([callback, context]() {
        callback(AUDIO_PLAY_STATE_STOPPED, context);
    });
    worker.join();
    return ESP_OK;
}

#include "production.inc"

static auto make_core()
{
    return std::make_unique<AudioProcessorCore>(
               std::make_shared<audio::CodecPlayerIface>(), std::make_shared<audio::Recorder>(),
               AudioProcessorConfig{}
           );
}

static void check_close_and_reopen()
{
    auto core = make_core();
    assert(core->acquire());
    assert(core->acquire());
    core->release();
    assert(core->is_opened());
    core->release();
    assert(!core->is_opened());
    assert(core->acquire());
    core->release();
    assert(!core->is_opened());
}

static void check_concurrent_reopen()
{
    auto core = make_core();
    assert(core->acquire());
    std::promise<void> closing;
    std::promise<void> finish_close;
    auto finish = finish_close.get_future().share();
    before_close = [&]() {
        closing.set_value();
        finish.wait();
    };
    auto release = std::async(std::launch::async, [&]() {
        core->release();
    });
    closing.get_future().wait();
    auto acquire = std::async(std::launch::async, [&]() {
        return core->acquire();
    });
    assert(acquire.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout);
    finish_close.set_value();
    release.get();
    assert(acquire.get());
    before_close = nullptr;
    core->release();
}

static void check_callback_drain()
{
    auto core = make_core();
    std::promise<void> entered;
    std::promise<void> finish_callback;
    auto finish = finish_callback.get_future().share();
    std::atomic_bool callback_used_hal = false;
    assert(core->acquire([&](audio::PlayState) {
        entered.set_value();
        finish.wait();
        // clear must not hold the lifecycle lock while waiting for this call.
        assert(core->is_opened());
        assert(core->acquire());
        core->release();
        callback_used_hal = true;
    }));
    std::thread event([]() {
        captured_playback.event_cb(AUDIO_PLAY_STATE_PLAYING, captured_playback.event_cb_ctx);
    });
    entered.get_future().wait();
    auto clear = std::async(std::launch::async, [&]() {
        core->clear_playback_callback();
    });
    assert(clear.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout);
    finish_callback.set_value();
    event.join();
    clear.get();
    assert(callback_used_hal);
    // No further business callbacks may run after clear has returned.
    captured_playback.event_cb(AUDIO_PLAY_STATE_PLAYING, captured_playback.event_cb_ctx);
    core->release();
}

static void check_failure_and_destruction()
{
    for (int stage = 0; stage < 4; ++stage) {
        auto core = make_core();
        core->player_iface_->fail_open = stage == 0;
        core->recorder_iface_->fail_open = stage == 1;
        fail_manager = stage == 2;
        fail_playback = stage == 3;
        assert(!core->acquire());
        assert(!core->is_opened());
        core->player_iface_->fail_open = false;
        core->recorder_iface_->fail_open = false;
        fail_manager = false;
        fail_playback = false;
        assert(core->acquire());
        // The destructor also closes an active SDK player.
    }
}

static void check_close_from_callback()
{
    auto core = std::make_shared<AudioProcessorCore>(
                    std::make_shared<audio::CodecPlayerIface>(), std::make_shared<audio::Recorder>(),
                    AudioProcessorConfig{}
                );
    AudioPlaybackImpl playback(core);
    std::promise<void> closed_in_callback;
    std::promise<void> finish_callback;
    auto finish = finish_callback.get_future().share();
    const int closes_before = playback_closes.load();
    assert(playback.open([&](audio::PlayState) {
        playback.close();
        closed_in_callback.set_value();
        finish.wait();
    }));
    std::thread event([]() {
        captured_playback.event_cb(AUDIO_PLAY_STATE_PLAYING, captured_playback.event_cb_ctx);
    });
    assert(closed_in_callback.get_future().wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    assert(!playback.is_opened());
    assert(!playback.open(nullptr));
    assert(playback_closes.load() == closes_before);
    finish_callback.set_value();
    event.join();
    for (int i = 0; i < 100 && core->is_opened(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(!core->is_opened());
    // A close from another task waits for the deferred cleanup, including its lifecycle state update.
    playback.close();
    assert(playback.open(nullptr));
    playback.close();
    assert(playback_closes.load() == closes_before + 2);
}

static void check_close_from_another_thread()
{
    auto core = std::make_shared<AudioProcessorCore>(
                    std::make_shared<audio::CodecPlayerIface>(), std::make_shared<audio::Recorder>(),
                    AudioProcessorConfig{}
                );
    AudioPlaybackImpl playback(core);
    std::promise<void> entered;
    std::promise<void> finish_callback;
    auto finish = finish_callback.get_future().share();
    assert(playback.open([&](audio::PlayState) {
        entered.set_value();
        finish.wait();
    }));
    std::thread event([]() {
        captured_playback.event_cb(AUDIO_PLAY_STATE_PLAYING, captured_playback.event_cb_ctx);
    });
    entered.get_future().wait();
    auto close = std::async(std::launch::async, [&]() {
        playback.close();
    });
    assert(close.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout);
    finish_callback.set_value();
    event.join();
    close.get();
    assert(!playback.is_opened() && !core->is_opened());
}

static void check_close_reentrant_callback_destruction()
{
    struct CloseOnDestroy {
        AudioPlaybackImpl *playback;
        ~CloseOnDestroy()
        {
            playback->close();
        }
    };
    auto core = std::make_shared<AudioProcessorCore>(
                    std::make_shared<audio::CodecPlayerIface>(), std::make_shared<audio::Recorder>(),
                    AudioProcessorConfig{}
                );
    AudioPlaybackImpl playback(core);
    auto captured = std::make_shared<CloseOnDestroy>(&playback);
    assert(playback.open([captured](audio::PlayState) {}));
    captured.reset();
    playback.close();
    assert(!playback.is_opened() && !core->is_opened());
}

static int run_main()
{
    shared_codec_reset();
    check_close_and_reopen();
    check_concurrent_reopen();
    check_callback_drain();
    check_failure_and_destruction();
    assert(playback_closes == 9);
    check_close_from_callback();
    check_close_from_another_thread();
    check_close_reentrant_callback_destruction();
    std::cout << "PASS: production audio lifecycle, synchronous stop callback, concurrent reopen, "
              "callback drain with HAL access, open rollback and destruction (fake SDK I/O)\n";
    return 0;
}

int main()
{
    try {
        return run_main();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
