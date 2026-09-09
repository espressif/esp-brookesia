/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include <atomic>
#include <chrono>
#include <cerrno>
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

#include "sdk_stubs.hpp"
#include "brookesia/hal_adaptor/macro_configs.h"
#include "brookesia/hal_adaptor/board_manager.h"
#include "brookesia/hal_custom/expansion/mosaico_module_manager.h"
#include "expansion/runtime.hpp"
#include "video/camera_impl.hpp"
#include "video/encoder_impl.hpp"
#include "video/processor_type_converter.hpp"

using namespace esp_brookesia;
using namespace std::chrono_literals;

std::shared_ptr<hal::expansion::ModuleProvider> make_mosaico_provider_for_test();

namespace {

void require(bool value, const char *message)
{
    if (!value) {
        std::cerr << message << '\n';
        std::abort();
    }
}

class Signal {
public:
    void set()
    {
        std::lock_guard lock(mutex_);
        ready_ = true;
        condition_.notify_all();
    }

    void wait()
    {
        std::unique_lock lock(mutex_);
        require(condition_.wait_for(lock, 2s, [this]() {
            return ready_;
        }), "signal timed out");
    }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    bool ready_ = false;
};

bool wait_until(const std::function<bool()> &predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::yield();
    }
    return true;
}

std::recursive_mutex lifecycle_mutex;
thread_local size_t lifecycle_depth = 0;
std::atomic<int> camera_refs = 0;
std::atomic<int> camera_init_calls = 0;
std::atomic<int> camera_deinit_calls = 0;
std::atomic<bool> camera_cleanup_fails = false;
std::atomic<bool> camera_node_present = true;
std::atomic<bool> probe_close_fails = false;
std::atomic<int> probe_close_calls = 0;
std::atomic<bool> runtime_pin_live = false;
std::atomic<bool> runtime_pin_cleanup_pending = false;
std::atomic<bool> runtime_pin_cleanup_fails = false;
std::atomic<bool> runtime_pin_failure_consumes_handle = true;
std::atomic<int> runtime_pin_init_calls = 0;
std::atomic<int> frame_release_calls = 0;
std::atomic<int> capture_stop_calls = 0;
std::atomic<bool> capture_is_started = false;
std::atomic<bool> capture_is_open = false;
video_capture_config_t capture_config;
uint8_t frame_pixels[4] {};
dev_camera_handle_t camera_handle{ESP_VIDEO_DVP_DEVICE_NAME};
dev_camera_config_t camera_config{"dvp"};

class Provider: public hal::expansion::ModuleProvider {
public:
    std::atomic<int> scans = 0;
    std::atomic<int> starts = 0;
    std::atomic<int> stops = 0;

    std::string_view get_name() const override
    {
        return "host";
    }
    std::vector<std::string> get_slots() const override
    {
        return {"camera"};
    }
    std::expected<void, std::string> start() override
    {
        require(lifecycle_depth == 0, "runtime retain entered with BM gate held");
        ++starts;
        return {};
    }
    std::expected<void, std::string> stop() override
    {
        require(lifecycle_depth == 0, "runtime release entered with BM gate held");
        ++stops;
        return {};
    }
    std::expected<hal::expansion::ScanInfo, std::string> scan(std::string_view) override
    {
        ++scans;
        return hal::expansion::ScanInfo{.state = hal::expansion::ModuleState::Ready, .type = "camera"};
    }
    std::expected<hal::expansion::ClaimOptions, std::string> claim(
        const hal::expansion::ModuleInfo &, uint64_t
    ) override
    {
        return hal::expansion::ClaimOptions{.pause_all_scanning = true};
    }
    std::expected<void, std::string> release(const hal::expansion::ModuleInfo &) override
    {
        return {};
    }
};

void test_event_worker(const std::shared_ptr<Provider> &provider)
{
    auto &runtime = hal::expansion::detail::ExpansionRuntime::get_instance();
    Signal callback_entered;
    Signal allow_callback_query;
    Signal callback_done;
    Signal allow_callback_exit;
    std::atomic<bool> blocked_once = false;
    std::atomic<int> queued_calls = 0;
    const auto listener = runtime.add_event_listener([&](const auto &) {
        if (blocked_once.exchange(true)) {
            return;
        }
        callback_entered.set();
        allow_callback_query.wait();
        // This executes on the event worker while the scan worker progresses.
        const auto before = provider->scans.load();
        require(runtime.request_rescan(), "callback rescan request rejected");
        require(wait_until([&]() {
            return provider->scans > before;
        }), "event worker blocked scanning");
        callback_done.set();
        allow_callback_exit.wait();
    });
    const auto suppressed = runtime.add_event_listener([&](const auto &) {
        ++queued_calls;
    });
    require(runtime.retain_consumer(), "runtime retain failed");
    callback_entered.wait();
    // remove must suppress a queued invocation without waiting for the preceding callback.
    require(runtime.remove_event_listener(suppressed), "queued listener removal failed");
    auto info = runtime.get_module_infos().front();
    auto lease = runtime.claim_module(info.provider, info.slot, info.generation, nullptr);
    require(static_cast<bool>(lease), "claim failed while listener was blocked");
    require(lease.reset(), "release failed while listener was blocked");
    allow_callback_query.set();
    callback_done.wait();

    // Last-consumer release must not wait on a callback that may need caller locks.
    const auto before_stop = provider->stops.load();
    runtime.release_consumer();
    require(provider->stops == before_stop + 1, "scan shutdown waited for a user callback");
    Signal remove_started;
    std::atomic<bool> remove_done = false;
    std::thread remover([&]() {
        remove_started.set();
        require(runtime.remove_event_listener(listener), "active listener removal failed");
        remove_done = true;
    });
    remove_started.wait();
    require(!remove_done, "remove did not wait for entered callback");
    allow_callback_exit.set();
    remover.join();
    require(queued_calls == 0, "removed queued listener was invoked");

    Signal self_removed;
    hal::expansion::ModuleManagerIface::EventListenerId self_id = 0;
    self_id = runtime.add_event_listener([&](const auto &) {
        require(runtime.remove_event_listener(self_id), "self removal failed");
        self_removed.set();
    });
    require(runtime.retain_consumer(), "restart failed");
    self_removed.wait();
    runtime.release_consumer();
    std::cout << "expansion: async claim/release, independent scan, remove, self-remove passed\n";
}

hal::video::EncoderConfig encoder_config(bool stream)
{
    return {.sinks = {{hal::video::EncoderSinkFormat::RGB565, 2, 1, 7}}, .enable_stream_mode = stream};
}

void test_encoder_shutdown(bool stream, bool close)
{
    hal::VideoEncoderImpl encoder(0, ESP_VIDEO_DVP_DEVICE_NAME);
    Signal entered;
    Signal query;
    std::string error;
    const auto callback = [&](size_t, const auto &, const uint8_t *, size_t) {
        entered.set();
        query.wait();
        require(encoder.is_opened(), "capture closed before callback released its frame");
        require(!encoder.is_started(), "stop state was not visible inside callback");
    };
    require(encoder.open(encoder_config(stream), callback, &error), "encoder open failed");
    require(encoder.start(&error), "encoder start failed");
    const auto before_release = frame_release_calls.load();
    const auto before_stop = capture_stop_calls.load();
    std::thread frame([&]() {
        if (stream) {
            esp_capture_stream_frame_t frame{ESP_CAPTURE_STREAM_TYPE_VIDEO, 0, frame_pixels, sizeof(frame_pixels)};
            capture_config.callback(capture_config.context, 0, &frame);
        } else {
            require(encoder.fetch_frame(0, callback, nullptr), "fetch failed");
        }
    });
    entered.wait();
    std::thread shutdown([&]() {
        if (close) {
            encoder.close();
        } else {
            require(encoder.stop(nullptr), "stop failed");
        }
    });
    require(wait_until([&]() {
        return !encoder.is_started();
    }), "shutdown did not publish stopping state");
    require(capture_stop_calls == before_stop, "capture stopped before outstanding callback finished");
    require(!encoder.start(nullptr), "start raced through stop transition");
    require(!encoder.fetch_frame(0, nullptr, nullptr), "fetch raced through stop transition");
    query.set();
    frame.join();
    shutdown.join();
    require(capture_stop_calls == before_stop + 1, "capture did not stop exactly once");
    if (!stream) {
        require(frame_release_calls == before_release + 1, "borrowed frame was not released exactly once");
    }
    if (close) {
        require(!encoder.is_opened(), "close retained capture");
    } else {
        require(encoder.is_opened(), "stop unexpectedly closed capture");
        require(encoder.start(nullptr), "restart after stop failed");
        encoder.close();
    }
}

void test_camera_pending_owner(const std::shared_ptr<Provider> &provider)
{
    std::string error;
    const auto before_stop = provider->stops.load();
    const auto before_deinit = camera_deinit_calls.load();
    {
        hal::VideoCameraDeviceSession session;
        require(session.open(error), "camera session open failed");
        camera_cleanup_fails = true;
        require(!session.close(&error), "injected close error disappeared");
        require(!session.close(&error), "repeat close hid pending cleanup");
        require(camera_deinit_calls == before_deinit + 1, "repeat close retried cleanup");
    }
    require(camera_refs == 1, "failed close dropped board ownership");
    require(camera_deinit_calls == before_deinit + 1, "destructor retried failed close immediately");
    require(provider->stops == before_stop, "failed close dropped runtime pin");
    const auto before_init = camera_init_calls.load();
    {
        hal::VideoCameraDeviceSession next;
        require(!next.open(error), "opened another camera while cleanup still failed");
    }
    require(camera_init_calls == before_init, "failed cleanup allowed a new board init");
    require(camera_deinit_calls == before_deinit + 2, "new interface did not attempt exactly one cleanup");
    camera_cleanup_fails = false;
    {
        hal::VideoCameraDeviceSession next;
        require(next.open(error), "new interface did not recover previous cleanup");
        require(next.close(), "recovered camera close failed");
    }
    require(camera_refs == 0, "camera recovery leaked board reference");
    require(provider->stops == before_stop + 1, "runtime pin did not survive until final cleanup");

    // Missing sensor needs no esp_video detect-required patch: validate the node,
    // roll back, and allow a later insertion through the same public session path.
    camera_node_present = false;
    {
        hal::VideoCameraImpl discovery;
        require(discovery.get_device_infos().empty(), "discovery accepted absent video node");
    }
    require(camera_refs == 0, "absent node rollback leaked board reference");
    camera_node_present = true;
    {
        hal::VideoCameraImpl discovery;
        require(discovery.get_device_infos().size() == 1, "reinserted camera was not discovered");
    }
    require(camera_refs == 0, "discovery leaked board reference");

    const auto before_discovery_deinit = camera_deinit_calls.load();
    camera_cleanup_fails = true;
    {
        hal::VideoCameraImpl discovery;
        require(discovery.get_device_infos().empty(), "discovery hid its cleanup failure");
    }
    require(camera_deinit_calls == before_discovery_deinit + 1, "discovery destructor retried failed cleanup");
    require(camera_refs == 1, "destroyed discovery lost its pending hardware owner");
    camera_cleanup_fails = false;
    {
        hal::VideoEncoderImpl encoder(0, ESP_VIDEO_DVP_DEVICE_NAME);
        require(encoder.open(encoder_config(false), nullptr, &error), "encoder failed to recover discovery cleanup");
        encoder.close();
    }
    require(camera_refs == 0, "cross-interface cleanup recovery leaked a reference");

    // An old session may observe completion but must never release a new session's reference.
    hal::VideoCameraDeviceSession old;
    require(old.open(error), "old camera session open failed");
    camera_cleanup_fails = true;
    require(!old.close(&error), "old camera session close failure disappeared");
    camera_cleanup_fails = false;
    {
        hal::VideoCameraDeviceSession next;
        require(next.open(error), "pending session recovery failed");
        require(old.close(&error), "resolved pending cleanup still reported failure");
        require(camera_refs == 1, "resolved session released the new session's ownership");
        camera_cleanup_fails = true;
        require(!next.close(&error), "second pending cleanup failure disappeared");
        require(old.close(&error), "old session confused an unrelated cleanup with its own");
    }
    camera_cleanup_fails = false;
    {
        hal::VideoCameraDeviceSession next;
        require(next.open(error), "second pending generation recovery failed");
        require(next.close(), "second pending generation close failed");
    }

    const auto before_probe_close = probe_close_calls.load();
    const auto before_probe_deinit = camera_deinit_calls.load();
    probe_close_fails = true;
    {
        hal::VideoCameraDeviceSession session;
        require(!session.open(error), "probe close error was reported as a successful open");
        require(error.find("Failed to close board camera probe") != std::string::npos,
                "probe close failure lost its error");
    }
    require(probe_close_calls == before_probe_close + 1, "failed probe descriptor was retried");
    require(camera_deinit_calls == before_probe_deinit + 1 && camera_refs == 0,
            "probe close failure did not roll back board ownership");
    probe_close_fails = false;
    std::cout << "camera: persistent owner, repeated close, probe failure, absent node rollback/reinsert passed\n";
}

void test_mosaico_provider_cleanup()
{
    auto &runtime = hal::expansion::detail::ExpansionRuntime::get_instance();
    require(runtime.register_provider(make_mosaico_provider_for_test()), "Mosaico provider registration failed");
    require(runtime.retain_consumer(), "Mosaico initial retain failed");
    require(runtime_pin_live, "Mosaico provider did not acquire the board runtime pin");
    runtime_pin_cleanup_fails = true;
    runtime.release_consumer();
    require(!runtime_pin_live && runtime_pin_cleanup_pending, "failed cleanup did not consume the runtime pin");
    std::string error;
    require(!runtime.retain_consumer(&error), "provider restart skipped consumed pin cleanup and falsely succeeded");
    require(!runtime_pin_live, "failed restart recreated runtime pin before cleanup");
    runtime_pin_cleanup_fails = false;
    require(runtime.retain_consumer(&error), "Mosaico provider did not recover consumed pin cleanup");
    require(wait_until([&]() {
        const auto modules = runtime.get_module_infos();
        return !modules.empty() && modules.front().state == hal::expansion::ModuleState::Ready;
    }), "Mosaico scan did not recover after cleanup");
    const auto before_init = runtime_pin_init_calls.load();
    runtime_pin_cleanup_fails = true;
    runtime_pin_failure_consumes_handle = false;
    runtime.release_consumer();
    require(runtime_pin_live, "pre-release failure lost the live pin");
    runtime_pin_cleanup_fails = false;
    require(runtime.retain_consumer(&error), "live pin could not restart");
    require(runtime_pin_init_calls == before_init, "restart duplicated a surviving runtime pin reference");
    runtime.release_consumer();
    require(!runtime_pin_live && !runtime_pin_cleanup_pending, "Mosaico final release retained a pin");
    require(runtime.unregister_provider("mosaico"), "Mosaico provider remained pinned");
    std::cout << "mosaico: real provider distinguishes consumed and surviving BM pins after cleanup errors passed\n";
}

} // namespace

extern "C" void brookesia_hal_lifecycle_lock()
{
    lifecycle_mutex.lock();
    ++lifecycle_depth;
}
extern "C" void brookesia_hal_lifecycle_unlock()
{
    --lifecycle_depth;
    lifecycle_mutex.unlock();
}
extern "C" bool brookesia_hal_board_manager_check_name(const char *)
{
    require(lifecycle_depth > 0, "board declaration lookup lacks lifecycle guard");
    return true;
}
extern "C" esp_err_t brookesia_hal_board_manager_get_device_config(const char *, void **config)
{
    require(lifecycle_depth > 0, "config lookup lacks lifecycle guard");
    *config = &camera_config;
    return ESP_OK;
}
extern "C" esp_err_t brookesia_hal_board_manager_init_device_by_name(const char *name)
{
    require(lifecycle_depth > 0, "board init lacks lifecycle guard");
    if (std::string_view(name) == "expansion_runtime_pin") {
        ++runtime_pin_init_calls;
        if (runtime_pin_cleanup_pending && runtime_pin_cleanup_fails) {
            return ESP_ERR_INVALID_STATE;
        }
        require(!runtime_pin_live, "runtime pin initialized twice");
        runtime_pin_cleanup_pending = false;
        runtime_pin_live = true;
        return ESP_OK;
    }
    ++camera_refs;
    ++camera_init_calls;
    return ESP_OK;
}
extern "C" esp_err_t brookesia_hal_board_manager_get_device_handle(const char *name, void **handle)
{
    if (std::string_view(name) == "expansion_runtime_pin") {
        require(lifecycle_depth > 0, "runtime pin lookup lacks lifecycle guard");
        *handle = runtime_pin_live ? &camera_handle : nullptr;
        return runtime_pin_live ? ESP_OK : ESP_ERR_INVALID_STATE;
    }
    require(lifecycle_depth > 0 && camera_refs > 0, "board handle lookup lacks init transaction");
    *handle = &camera_handle;
    return ESP_OK;
}
extern "C" esp_err_t brookesia_hal_board_manager_deinit_device_by_name(const char *name)
{
    if (std::string_view(name) == "expansion_runtime_pin") {
        require(lifecycle_depth > 0 && runtime_pin_live, "runtime pin deinit lacks ownership or lifecycle guard");
        if (runtime_pin_cleanup_fails) {
            if (runtime_pin_failure_consumes_handle) {
                runtime_pin_live = false;
                runtime_pin_cleanup_pending = true;
            }
            return ESP_ERR_INVALID_STATE;
        }
        runtime_pin_live = false;
        runtime_pin_cleanup_pending = false;
        return ESP_OK;
    }
    require(lifecycle_depth > 0 && camera_refs > 0, "board deinit lacks ownership or lifecycle guard");
    ++camera_deinit_calls;
    if (camera_cleanup_fails) {
        return ESP_ERR_INVALID_STATE;
    }
    --camera_refs;
    return ESP_OK;
}
extern "C" int __wrap_open(const char *, int, ...)
{
    return camera_node_present ? 1000 : -1;
}
extern "C" int __real_close(int fd);
extern "C" int __wrap_close(int fd)
{
    if (fd != 1000) {
        return __real_close(fd);
    }
    ++probe_close_calls;
    if (probe_close_fails) {
        errno = EIO;
        return -1;
    }
    return 0;
}
extern "C" esp_err_t esp_mosaico_expansion_scan(
    esp_mosaico_expansion_slot_t slot, esp_mosaico_expansion_info_t *info
)
{
    if (!runtime_pin_live || runtime_pin_cleanup_pending) {
        return ESP_ERR_INVALID_STATE;
    }
    *info = {};
    info->slot = slot;
    info->state = ESP_MOSAICO_EXPANSION_STATE_READY;
    info->eeprom.board_type = ESP_MOSAICO_BOARD_TYPE_CAMERA;
    info->eeprom.board_id = 7;
    info->generation = 1;
    return ESP_OK;
}
extern "C" const char *esp_mosaico_board_type_to_name(uint8_t)
{
    return "camera";
}
extern "C" esp_err_t esp_mosaico_camera_slot_acquire(const esp_mosaico_camera_slot_config_t *)
{
    return ESP_OK;
}
extern "C" esp_err_t esp_mosaico_camera_slot_release(esp_mosaico_expansion_slot_t)
{
    return ESP_OK;
}
extern "C" int __wrap_ioctl(int, unsigned long, ...)
{
    return -1;
}
video_capture_handle_t video_capture_open(const video_capture_config_t *config)
{
    capture_config = *config;
    capture_is_open = true;
    return &capture_config;
}
void video_capture_close(video_capture_handle_t)
{
    require(!capture_is_started, "capture close before stop");
    capture_is_open = false;
}
int video_capture_start(video_capture_handle_t)
{
    capture_is_started = true;
    return ESP_OK;
}
int video_capture_stop(video_capture_handle_t)
{
    ++capture_stop_calls;
    capture_is_started = false;
    return ESP_OK;
}
int video_capture_fetch_frame_acquire(video_capture_handle_t, int, esp_capture_stream_frame_t *frame)
{
    frame->data = frame_pixels;
    frame->size = sizeof(frame_pixels);
    return ESP_OK;
}
int video_capture_fetch_frame_release(video_capture_handle_t, int, esp_capture_stream_frame_t *)
{
    require(capture_is_open && capture_is_started, "borrowed frame outlived capture resources");
    ++frame_release_calls;
    return ESP_OK;
}
video_capture_config_t hal::VideoProcessorTypeConverter::convert(
    const video::EncoderConfig &, void *, video_capture_frame_callback_t callback, void *context
)
{
    return {callback, context};
}

int main()
{
    try {
        static_assert(BROOKESIA_HAL_ADAPTOR_VIDEO_KEEP_DEVICE_INITIALIZED_ON_RELEASE == 0);
        auto provider = std::make_shared<Provider>();
        auto &runtime = hal::expansion::detail::ExpansionRuntime::get_instance();
        require(runtime.register_provider(provider), "provider registration failed");
        test_event_worker(provider);
        for (bool stream : {
                    false, true
                }) {
            for (bool close : {
                        false, true
                    }) {
                test_encoder_shutdown(stream, close);
            }
        }
        std::cout << "encoder: fetch/stream callbacks query state during stop/close passed\n";
        test_camera_pending_owner(provider);
        require(runtime.unregister_provider("host"), "provider remained pinned after tests");
        test_mosaico_provider_cleanup();
        std::cout << "all lifecycle regressions passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
