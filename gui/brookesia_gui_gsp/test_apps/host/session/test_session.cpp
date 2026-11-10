/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <array>
#include <cassert>
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

int stop_timeouts_left = 0;
int stop_failure = ESP_GSP_OK;
bool fail_start = false;
bool fail_event = false;
int starts = 0;
int stops = 0;
esp_gsp_event_cb_t event_callback = nullptr;
void *event_ctx = nullptr;
esp_gsp_esp_lcd_config_t last_lcd{};
size_t last_bundle_size = 0;
const void *last_bundle_ptr = nullptr;
const void *last_directories = nullptr;
const void *last_ttf = nullptr;
int allocations = 0;
int frees = 0;

void reset()
{
    stop_timeouts_left = 0;
    stop_failure = ESP_GSP_OK;
    fail_start = false;
    fail_event = false;
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

DisplayLease lease_for(PanelIface &panel, TouchIface &touch)
{
    return DisplayLease{.panel = &panel, .touch = &touch};
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
    assert(session.start(config, lease_for(panel, touch), on_event, &seen));
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
    event_callback(session.handle(), &event, event_ctx);
    assert(seen == 7);
    assert(session.stop());
    assert(!session.running());
    assert(session.handle() == nullptr);
    assert(event_callback == nullptr);
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
    assert(!session.start(config, missing_panel, nullptr, nullptr));
    assert(starts == 0);
    assert(session.start(config, lease_for(panel, touch), nullptr, nullptr));
    assert(!session.start(config, lease_for(panel, touch), nullptr, nullptr));
    assert(starts == 1);
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
    assert(!session.start(config, lease_for(panel, touch), ignored, nullptr));
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

void test_destructor_terminates_when_stop_fails()
{
    const pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        std::set_terminate([]() {
            _exit(77);
        });
        reset();
        stop_failure = ESP_GSP_FAIL;
        PanelIface panel;
        TouchIface touch;
        esp_gsp_config_t config{};
        {
            Session session;
            assert(session.start(config, lease_for(panel, touch), nullptr, nullptr));
        }
        _exit(0);
    }
    int status = 0;
    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 77);
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
    assert(host.start(config, lease_for(panel, touch), nullptr, nullptr));
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
        assert(!host.start(config, lease_for(panel, touch), nullptr, nullptr));
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
    assert(!host.start(config, lease_for(panel, touch), nullptr, nullptr));
    assert(!host.running() && allocations == 1 && frees == 1);

    reset();
    fail_event = true;
    stop_failure = ESP_GSP_FAIL;
    AppHost retained;
    auto ignored = [](esp_gsp_handle_t, const esp_gsp_event_t *, void *) {};
    assert(!retained.start(config, lease_for(panel, touch), ignored, nullptr));
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
        assert(host.start(config, lease_for(panel, touch), nullptr, nullptr));
        stop_timeouts_left = 3;
        assert(!host.stop() && host.running() && frees == 0);
    }
    assert(stops == 4 && frees == 1);
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
    test_start_stop_round_trip();
    test_rejects_missing_display_and_second_start();
    test_event_registration_failure_stops();
    test_stop_timeout_keeps_bundle_until_retry();
    test_destructor_terminates_when_stop_fails();
    test_compressed_app_host_owns_one_expansion_until_stop();
    test_compressed_app_host_rejects_invalid_envelopes();
    test_compressed_app_host_failed_start_cleanup();
    test_app_host_destructor_retries_stop();
    std::cout << "GSP session lifecycle tests passed\n";
}
