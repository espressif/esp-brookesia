/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define BROOKESIA_LOG_TAG "GspBasic"
#include "brookesia/lib_utils/log.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include "demo_gsp.h"
#include "demo_ui.hpp"
#include "esp_gsp.h"

#ifndef BROOKESIA_GSP_BASIC_BUILD_ID
#   define BROOKESIA_GSP_BASIC_BUILD_ID "unknown"
#endif

namespace esp_brookesia::gsp_basic {

namespace {

struct State {
    esp_gsp_handle_t ui = nullptr;
    // Touch callbacks write this on the render task. keep_alive() reads it on the application task.
    std::atomic<int> count{0};
    int activity = 0;
    void *timer = nullptr;
};

State s_state{};

void publish_count(esp_gsp_handle_t ui)
{
    char text[32];
    const int count = s_state.count.load(std::memory_order_relaxed);
    std::snprintf(text, sizeof(text), "Count %d", count);
    const esp_gsp_err_t result = gsp_home_count_set_text(ui, text);
    if (result != ESP_GSP_OK) {
        BROOKESIA_LOGE("Failed to set count text: %1%", static_cast<int>(result));
    }
}

void publish_activity(esp_gsp_handle_t ui)
{
    const esp_gsp_err_t result = gsp_home_activity_set_value(ui, s_state.activity);
    if (result != ESP_GSP_OK) {
        BROOKESIA_LOGE("Failed to set activity: %1%", static_cast<int>(result));
    }
}

void on_tick(esp_gsp_handle_t ui, void *ctx)
{
    (void)ctx;
    s_state.activity = (s_state.activity + 4) % 101;
    publish_activity(ui);
}

void on_event(esp_gsp_handle_t ui, const esp_gsp_event_t *event, void *ctx)
{
    (void)ctx;
    if ((event == nullptr) || (event->type != ESP_GSP_EVENT_CALL)) {
        return;
    }
    if (gsp_home_event_is_count_up(event)) {
        s_state.count.fetch_add(1, std::memory_order_relaxed);
        publish_count(ui);
        BROOKESIA_LOGI("count_up -> %1%", s_state.count);
        return;
    }
    if (gsp_home_event_is_count_reset(event)) {
        s_state.count.store(0, std::memory_order_relaxed);
        publish_count(ui);
        BROOKESIA_LOGI("count_reset");
    }
}

bool arm_demo(esp_gsp_handle_t ui)
{
    char build_text[64];
    std::snprintf(build_text, sizeof(build_text), "build %s", BROOKESIA_GSP_BASIC_BUILD_ID);
    s_state.ui = ui;
    s_state.count.store(0, std::memory_order_relaxed);
    s_state.activity = 0;

    if (gsp_home_build_id_set_text(ui, build_text) != ESP_GSP_OK) {
        BROOKESIA_LOGE("Failed to set build id");
        return false;
    }
    publish_count(ui);
    publish_activity(ui);
    s_state.timer = esp_gsp_timer_create(ui, 250, on_tick, nullptr);
    if (s_state.timer == nullptr) {
        BROOKESIA_LOGE("Failed to create activity timer");
        return false;
    }
    BROOKESIA_LOGI("Demo armed, %1%", build_text);
    return true;
}

} // namespace

bool start_demo(gui::gsp::Session &session, const gui::gsp::DisplayLease &display)
{
    auto config = std::make_shared<esp_gsp_config_t>(gsp_demo_config());
    if (!session.start(*config, display, on_event, nullptr, {}, config)) {
        return false;
    }
    if (!arm_demo(session.handle())) {
        session.stop();
        return false;
    }
    return true;
}

bool restart_demo(gui::gsp::Session &session, const gui::gsp::DisplayLease &display)
{
    s_state.timer = nullptr;
    s_state.ui = nullptr;
    if (!session.stop()) {
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    if (!start_demo(session, display)) {
        return false;
    }
    BROOKESIA_LOGI("GSP session restart check: ok");
    return true;
}

void keep_alive()
{
    const auto started = std::chrono::steady_clock::now();
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(60));
        const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(
                                 std::chrono::steady_clock::now() - started
                             ).count();
        const int count = s_state.count.load(std::memory_order_relaxed);
        BROOKESIA_LOGI("demo alive, uptime %1% min, count %2%", static_cast<long>(minutes), count);
    }
}

} // namespace esp_brookesia::gsp_basic
