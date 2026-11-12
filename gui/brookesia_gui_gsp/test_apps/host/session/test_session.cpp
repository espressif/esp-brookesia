/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <cassert>
#include <future>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

#include <exception>

#include "zlib.h"

#include "esp_heap_caps.h"
#include "sdk_stubs.hpp"

struct FakeGsp {};

#include "brookesia/gui_gsp/bundle.hpp"
#include "brookesia/gui_gsp/app_host.hpp"
#include "brookesia/gui_gsp/session.hpp"

using esp_brookesia::gui::gsp::Bundle;
using esp_brookesia::gui::gsp::AppHost;
using esp_brookesia::gui::gsp::DisplayLease;
using esp_brookesia::gui::gsp::Session;
using esp_brookesia::hal::display::PanelIface;
using esp_brookesia::hal::display::TouchIface;

namespace {

std::atomic_int stop_timeouts_left = 0;
std::atomic_int stop_failure = ESP_GSP_OK;
std::atomic_bool event_inflight = false;
bool fail_start = false;
bool fail_event = false;
std::function<void()> registration_hook;
int starts = 0;
std::atomic_int stops = 0;
std::atomic<esp_gsp_event_cb_t> event_callback = nullptr;
std::atomic<void *> event_ctx = nullptr;
esp_gsp_esp_lcd_config_t last_lcd{};
size_t last_bundle_size = 0;
const void *last_bundle_ptr = nullptr;
const void *last_directories = nullptr;
const void *last_ttf = nullptr;
std::atomic_int allocations = 0;
std::atomic_int frees = 0;

void reset()
{
    stop_timeouts_left = 0;
    stop_failure = ESP_GSP_OK;
    event_inflight = false;
    fail_start = false;
    fail_event = false;
    registration_hook = {};
    starts = 0;
    stops = 0;
    event_callback = nullptr;
    event_ctx = nullptr;
    last_lcd = {};
    last_bundle_size = 0;
    last_bundle_ptr = nullptr;
    last_directories = nullptr;
    last_ttf = nullptr;
    allocations = 0;
    frees = 0;
}

void write_u32(std::vector<std::byte> &buffer, size_t offset, uint32_t value)
{
    for (size_t byte = 0; byte < 4; ++byte) {
        buffer[offset + byte] = static_cast<std::byte>((value >> (byte * 8)) & 0xFF);
    }
}

std::vector<std::byte> compressed_bundle(std::span<const std::byte> raw)
{
    uLongf compressed_size = compressBound(raw.size());
    std::vector<std::byte> result(16 + compressed_size);
    result[0] = std::byte{'B'};
    result[1] = std::byte{'G'};
    result[2] = std::byte{'Z'};
    result[3] = std::byte{'1'};
    assert(compress2(reinterpret_cast<Bytef *>(result.data() + 16), &compressed_size,
                     reinterpret_cast<const Bytef *>(raw.data()), raw.size(), 6) == Z_OK);
    result.resize(16 + compressed_size);
    write_u32(result, 4, raw.size());
    write_u32(result, 8, compressed_size);
    write_u32(result, 12, crc32(0, reinterpret_cast<const Bytef *>(raw.data()), raw.size()));
    return result;
}

std::shared_ptr<const void> borrowed_config(const esp_gsp_config_t &config)
{
    return {&config, [](const void *) {}};
}

DisplayLease lease_for(PanelIface &panel, TouchIface &touch)
{
    return DisplayLease{.panel = &panel, .touch = &touch, .lifetime = {&panel, [](void *) {}}};
}

bool wait_until(const std::function<bool()> &predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}

void test_start_stop_round_trip()
{
    reset();
    PanelIface panel;
    TouchIface touch;
    Session session;
    esp_gsp_config_t config{};
    config.bundle = "scene";
    config.bundle_size = 5;
    int seen = 0;
    auto on_event = [](esp_gsp_handle_t, const esp_gsp_event_t *event, void *ctx) {
        *static_cast<int *>(ctx) = event->marker;
    };
    assert(session.start(config, lease_for(panel, touch), on_event, &seen,
    std::shared_ptr<void>(&seen, [](void *) {}), borrowed_config(config)));
    assert(session.running());
    assert(session.handle() != nullptr);
    assert(starts == 1);
    assert(last_bundle_size == 5);
    assert(last_lcd.display.hw.panel_type == ESP_DISPLAY_PRESENT_PANEL_IO);
    assert(last_lcd.touch_input_mode == ESP_GSP_TOUCH_INPUT_POLLING);
    // Dynamic fonts must not select a smaller renderer stack.
    assert(last_lcd.task_stack_size > 0);
    assert(last_lcd.task_stack_size_freetype >= last_lcd.task_stack_size);
    esp_gsp_event_t event{.marker = 7};
    event_callback.load()(session.handle(), &event, event_ctx.load());
    assert(seen == 7);
    assert(session.stop());
    assert(!session.running());
    assert(session.handle() == nullptr);
    assert(event_callback.load() == nullptr);
    assert(session.stop());
}

void test_rejects_missing_display_and_second_start()
{
    reset();
    PanelIface panel;
    TouchIface touch;
    Session session;
    esp_gsp_config_t config{};
    DisplayLease missing_panel{.panel = nullptr, .touch = &touch};
    assert(!session.start(config, missing_panel, nullptr, nullptr, {}, borrowed_config(config)));
    DisplayLease unowned_display{.panel = &panel, .touch = &touch};
    assert(!session.start(config, unowned_display, nullptr, nullptr, {}, borrowed_config(config)));
    int callback_context = 0;
    assert(!session.start(config, lease_for(panel, touch), nullptr, &callback_context, {}, borrowed_config(config)));
    assert(!session.start(config, lease_for(panel, touch), nullptr, nullptr, {}, {}));
    assert(starts == 0);
    assert(session.start(config, lease_for(panel, touch), nullptr, nullptr, {}, borrowed_config(config)));
    assert(!session.start(config, lease_for(panel, touch), nullptr, nullptr, {}, borrowed_config(config)));
    assert(starts == 1);
    assert(session.stop());
}

void test_cleanup_thread_creation_failure()
{
    reset();
    PanelIface panel;
    TouchIface touch;
    esp_gsp_config_t config{};
    Session session;
    assert(!session.start(config, lease_for(panel, touch), nullptr, nullptr, {}, borrowed_config(config)));
    assert(starts == 0 && !session.running());
    assert(session.start(config, lease_for(panel, touch), nullptr, nullptr, {}, borrowed_config(config)));
    assert(session.stop());
}

void test_event_registration_failure_stops()
{
    reset();
    fail_event = true;
    PanelIface panel;
    TouchIface touch;
    Session session;
    esp_gsp_config_t config{};
    auto ignored = [](esp_gsp_handle_t, const esp_gsp_event_t *, void *) {};
    assert(!session.start(config, lease_for(panel, touch), ignored, nullptr, {}, borrowed_config(config)));
    assert(!session.running());
    assert(stops == 1);
}

void test_stop_timeout_keeps_bundle_until_retry()
{
    reset();
    std::array<char, 4> bytes{'V', 'a', 'l', 'd'};
    auto loaded = Bundle::load(std::as_bytes(std::span(bytes)));
    assert(loaded);
    auto bundle = loaded.value();
    const long owners_before = bundle.use_count();
    PanelIface panel;
    TouchIface touch;
    Session session;
    assert(session.start(bundle, lease_for(panel, touch), nullptr, nullptr));
    assert(bundle.use_count() == owners_before + 1);

    stop_timeouts_left = 3;
    assert(!session.stop());
    assert(session.running());
    assert(bundle.use_count() == owners_before + 1);
    assert(session.handle() != nullptr);

    assert(session.stop());
    assert(!session.running());
    assert(bundle.use_count() == owners_before);
}

void test_destructor_defers_persistent_stop_failure()
{
    reset();
    struct Owners {
        PanelIface panel;
        TouchIface touch;
    };
    auto display = std::make_shared<Owners>();
    auto context = std::make_shared<int>(42);
    auto config = std::make_shared<esp_gsp_config_t>();
    std::weak_ptr<Owners> weak_display = display;
    std::weak_ptr<int> weak_context = context;
    std::weak_ptr<esp_gsp_config_t> weak_config = config;
    stop_failure = ESP_GSP_FAIL;
    {
        Session session;
        const DisplayLease lease{.panel = &display->panel, .touch = &display->touch, .lifetime = display};
        auto callback = [](esp_gsp_handle_t, const esp_gsp_event_t *, void *) {};
        assert(session.start(*config, lease, callback, context.get(), context, config));
        display.reset();
        context.reset();
        config.reset();
    }
    assert(wait_until([] { return stops >= 2; }));
    assert(!weak_display.expired() && !weak_context.expired() && !weak_config.expired());
    {
        auto held_display = weak_display.lock();
        auto held_config = weak_config.lock();
        const DisplayLease lease{.panel = &held_display->panel, .touch = &held_display->touch,
                                 .lifetime = held_display};
        Session replacement;
        assert(!replacement.start(*held_config, lease, nullptr, nullptr, {}, held_config));
    }
    stop_failure = ESP_GSP_OK;
    assert(wait_until([&] { return weak_display.expired() && weak_context.expired() && weak_config.expired(); }));
}

void test_destructor_retries_transient_stop_timeout()
{
    reset();
    struct Owners {
        PanelIface panel;
        TouchIface touch;
    };
    auto display = std::make_shared<Owners>();
    auto config = std::make_shared<esp_gsp_config_t>();
    std::weak_ptr<Owners> weak_display = display;
    {
        Session session;
        const DisplayLease lease{.panel = &display->panel, .touch = &display->touch, .lifetime = display};
        assert(session.start(*config, lease, nullptr, nullptr, {}, config));
        stop_timeouts_left = 1;
    }
    display.reset();
    config.reset();
    assert(wait_until([&] { return stops == 2 && weak_display.expired(); }));
}

void test_callback_can_destroy_session()
{
    reset();
    struct Owners {
        PanelIface panel;
        TouchIface touch;
    };
    struct CallbackOwner {
        std::unique_ptr<Session> session;
        std::promise<void> entered;
        std::shared_future<void> release;
        int marker = 42;
    };
    auto display = std::make_shared<Owners>();
    auto owner = std::make_shared<CallbackOwner>();
    auto config = std::make_shared<esp_gsp_config_t>();
    std::promise<void> release;
    owner->release = release.get_future().share();
    auto entered = owner->entered.get_future();
    owner->session = std::make_unique<Session>();
    std::weak_ptr<CallbackOwner> weak_owner = owner;
    const DisplayLease lease{.panel = &display->panel, .touch = &display->touch, .lifetime = display};
    auto callback = [](esp_gsp_handle_t, const esp_gsp_event_t *, void *ctx) {
        auto *owner = static_cast<CallbackOwner *>(ctx);
        owner->session.reset();
        owner->entered.set_value();
        owner->release.wait();
        assert(owner->marker == 42);
    };
    assert(owner->session->start(*config, lease, callback, owner.get(), owner, config));
    event_inflight = true;
    std::thread event([] {
        esp_gsp_event_t value{};
        event_callback.load()(nullptr, &value, event_ctx.load());
        event_inflight = false;
    });
    assert(entered.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    owner.reset();
    assert(wait_until([] { return stops > 0; }));
    assert(!weak_owner.expired());
    release.set_value();
    event.join();
    assert(wait_until([&] { return weak_owner.expired(); }));
}

void test_compressed_app_host_owns_one_expansion_until_stop()
{
    reset();
    std::array<char, 8> raw{'G', 'S', 'P', 'B', 'A', 'B', 'C', 'D'};
    auto encoded = compressed_bundle(std::as_bytes(std::span(raw)));
    PanelIface panel;
    TouchIface touch;
    AppHost host;
    gsp_component_directory_t directory{};
    const gsp_component_directory_t *directories[] {&directory};
    int font = 7;
    esp_gsp_config_t config{};
    config.bundle = encoded.data();
    config.bundle_size = encoded.size();
    config.directories = directories;
    config.ttf = &font;
    config.ttf_size = sizeof(font);
    assert(host.start(config, lease_for(panel, touch), nullptr, nullptr, {}, borrowed_config(config)));
    assert(host.running() && host.bundle() == nullptr);
    assert(allocations == 1 && frees == 0);
    assert(last_bundle_ptr != encoded.data() && last_bundle_size == raw.size());
    assert(std::memcmp(last_bundle_ptr, raw.data(), raw.size()) == 0);
    assert(last_directories == directories && last_ttf == &font);

    stop_timeouts_left = 3;
    assert(!host.stop());
    assert(host.running() && frees == 0);
    assert(host.stop());
    assert(!host.running() && frees == 1);
}

void test_compressed_app_host_rejects_invalid_envelopes()
{
    std::array<char, 8> raw{'G', 'S', 'P', 'B', 'A', 'B', 'C', 'D'};
    auto encoded = compressed_bundle(std::as_bytes(std::span(raw)));
    PanelIface panel;
    TouchIface touch;
    auto try_invalid = [&](std::vector<std::byte> input, bool allocated) {
        reset();
        AppHost host;
        esp_gsp_config_t config{};
        config.bundle = input.data();
        config.bundle_size = input.size();
        assert(!host.start(config, lease_for(panel, touch), nullptr, nullptr, {}, borrowed_config(config)));
        assert(!host.running() && starts == 0);
        assert(allocations == static_cast<int>(allocated));
        assert(frees == allocations);
    };
    auto bad_magic = encoded;
    bad_magic[0] = std::byte{'X'};
    try_invalid(bad_magic, false);
    auto bad_size = encoded;
    write_u32(bad_size, 4, 9 * 1024 * 1024);
    try_invalid(bad_size, false);
    auto bad_crc = encoded;
    bad_crc[12] ^= std::byte{1};
    try_invalid(bad_crc, true);
    auto trailing = encoded;
    trailing.push_back(std::byte{0});
    try_invalid(trailing, false);
    auto truncated = encoded;
    truncated.pop_back();
    write_u32(truncated, 8, truncated.size() - 16);
    try_invalid(truncated, true);
}

void test_compressed_app_host_failed_start_cleanup()
{
    reset();
    std::array<char, 4> raw{'G', 'S', 'P', 'B'};
    auto encoded = compressed_bundle(std::as_bytes(std::span(raw)));
    PanelIface panel;
    TouchIface touch;
    esp_gsp_config_t config{};
    config.bundle = encoded.data();
    config.bundle_size = encoded.size();
    AppHost host;
    fail_start = true;
    assert(!host.start(config, lease_for(panel, touch), nullptr, nullptr, {}, borrowed_config(config)));
    assert(!host.running() && allocations == 1 && frees == 1);

    reset();
    fail_event = true;
    stop_failure = ESP_GSP_FAIL;
    AppHost retained;
    auto ignored = [](esp_gsp_handle_t, const esp_gsp_event_t *, void *) {};
    assert(!retained.start(config, lease_for(panel, touch), ignored, nullptr, {}, borrowed_config(config)));
    assert(retained.running() && allocations == 1 && frees == 0);
    stop_failure = ESP_GSP_OK;
    assert(retained.stop() && frees == 1);
}

void test_app_host_destructor_retries_stop()
{
    reset();
    std::array<char, 4> raw{'G', 'S', 'P', 'B'};
    auto encoded = compressed_bundle(std::as_bytes(std::span(raw)));
    PanelIface panel;
    TouchIface touch;
    esp_gsp_config_t config{};
    config.bundle = encoded.data();
    config.bundle_size = encoded.size();
    {
        AppHost host;
        assert(host.start(config, lease_for(panel, touch), nullptr, nullptr, {}, borrowed_config(config)));
        stop_timeouts_left = 3;
        assert(!host.stop() && host.running() && frees == 0);
    }
    assert(wait_until([] { return stops == 4 && frees == 1; }));
}

void test_current_published_before_callback_registration()
{
    reset();
    PanelIface panel;
    TouchIface touch;
    std::array<char, 4> raw{'G', 'S', 'P', 'B'};
    esp_gsp_config_t config{};
    config.bundle = raw.data();
    config.bundle_size = raw.size();
    AppHost host;
    bool observed = false;
    registration_hook = [&] {
        // Registration runs before start() returns, potentially on another core.
        std::thread reader([&] { observed = AppHost::current() == &host; });
        reader.join();
    };
    const auto callback = [](esp_gsp_handle_t, const esp_gsp_event_t *, void *) {};
    assert(host.start(config, lease_for(panel, touch), callback, nullptr, {}, borrowed_config(config)));
    assert(observed && AppHost::current() == &host);
    assert(host.stop() && AppHost::current() == nullptr);
    fail_start = true;
    assert(!host.start(config, lease_for(panel, touch), callback, nullptr, {}, borrowed_config(config)));
    assert(AppHost::current() == nullptr);
}

} // namespace

extern "C" void *heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps)
{
    assert(alignment == 64 && size > 0 && (caps & MALLOC_CAP_8BIT));
    ++allocations;
    void *ptr = nullptr;
    assert(posix_memalign(&ptr, alignment, size) == 0);
    return ptr;
}

extern "C" void heap_caps_free(void *ptr)
{
    ++frees;
    std::free(ptr);
}

extern "C" esp_gsp_err_t esp_gsp_deployable_bundle_open(
    const void *data, size_t size, bool verify_crc, esp_gsp_deployable_bundle_t **out_bundle
)
{
    assert(verify_crc && data != nullptr && size >= 4 && out_bundle != nullptr);
    *out_bundle = new esp_gsp_deployable_bundle{data, size};
    return ESP_GSP_OK;
}

extern "C" esp_gsp_err_t esp_gsp_deployable_bundle_get_info(
    const esp_gsp_deployable_bundle_t *, esp_gsp_deployable_info_t *out_info
)
{
    out_info->scene_count = 1;
    return ESP_GSP_OK;
}

extern "C" esp_gsp_err_t esp_gsp_deployable_bundle_make_config(
    const esp_gsp_deployable_bundle_t *bundle, esp_gsp_config_t *out_config
)
{
    *out_config = ESP_GSP_CONFIG_INIT();
    out_config->bundle = bundle->data;
    out_config->bundle_size = bundle->size;
    return ESP_GSP_OK;
}

extern "C" esp_gsp_err_t esp_gsp_deployable_bundle_action_key(
    const esp_gsp_deployable_bundle_t *, uint16_t, uint16_t, uint32_t *out_key
)
{
    *out_key = 1;
    return ESP_GSP_OK;
}

extern "C" void esp_gsp_deployable_bundle_close(esp_gsp_deployable_bundle_t *bundle)
{
    delete bundle;
}

extern "C" esp_err_t esp_gsp_esp_lcd_start(
    const esp_gsp_config_t *config, const esp_gsp_esp_lcd_config_t *lcd, esp_gsp_handle_t *out_gsp
)
{
    assert(config != nullptr && lcd != nullptr && out_gsp != nullptr);
    ++starts;
    last_bundle_size = config->bundle_size;
    last_bundle_ptr = config->bundle;
    last_directories = config->directories;
    last_ttf = config->ttf;
    last_lcd = *lcd;
    if (fail_start) {
        return ESP_GSP_FAIL;
    }
    *out_gsp = new FakeGsp{};
    return ESP_OK;
}

extern "C" esp_gsp_err_t esp_gsp_on_event(esp_gsp_handle_t, esp_gsp_event_cb_t callback, void *user_ctx)
{
    if (callback && registration_hook) {
        registration_hook();
    }
    if (fail_event) {
        return ESP_GSP_FAIL;
    }
    event_callback = callback;
    event_ctx = user_ctx;
    return ESP_GSP_OK;
}

extern "C" esp_gsp_err_t esp_gsp_stop(esp_gsp_handle_t gsp)
{
    ++stops;
    if (event_inflight) {
        return ESP_GSP_ERR_TIMEOUT;
    }
    if (stop_timeouts_left > 0) {
        --stop_timeouts_left;
        return ESP_GSP_ERR_TIMEOUT;
    }
    if (stop_failure != ESP_GSP_OK) {
        return stop_failure;
    }
    delete gsp;
    event_callback = nullptr;
    event_ctx = nullptr;
    return ESP_GSP_OK;
}

int main()
{
    if (std::getenv("GSP_TEST_FAIL_CLEANUP_THREAD") != nullptr) {
        test_cleanup_thread_creation_failure();
        std::cout << "GSP cleanup task creation failure test passed\n";
        return 0;
    }
    test_start_stop_round_trip();
    test_rejects_missing_display_and_second_start();
    test_event_registration_failure_stops();
    test_stop_timeout_keeps_bundle_until_retry();
    test_destructor_retries_transient_stop_timeout();
    test_destructor_defers_persistent_stop_failure();
    test_callback_can_destroy_session();
    test_compressed_app_host_owns_one_expansion_until_stop();
    test_compressed_app_host_rejects_invalid_envelopes();
    test_compressed_app_host_failed_start_cleanup();
    test_app_host_destructor_retries_stop();
    test_current_published_before_callback_registration();
    std::cout << "GSP session lifecycle tests passed\n";
}
