/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "boost/json/object.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/service_helper/emulation/nes.hpp"
#include "brookesia/service_manager/function/registry.hpp"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/emulation_nes/macro_configs.h"

namespace esp_brookesia::emulation {

class Nes: public service::ServiceBase {
public:
    using Helper = service::helper::Nes;
    using State = Helper::State;
    using VideoMode = Helper::VideoMode;
    using AudioMode = Helper::AudioMode;
    using GamepadState = Helper::GamepadState;
    using Config = Helper::Config;

    static Nes &get_instance()
    {
        static Nes instance;
        return instance;
    }

    std::expected<void, std::string> load(Config config);
    std::expected<void, std::string> start();
    std::expected<void, std::string> pause();
    std::expected<void, std::string> resume();
    std::expected<void, std::string> stop();
    std::expected<void, std::string> reset();
    std::expected<void, std::string> save();
    std::expected<void, std::string> set_gamepad_state(GamepadState state);
    State get_state() const;

private:
    struct FrameStepStats {
        uint32_t emulate_ms = 0;
        uint32_t convert_ms = 0;
        uint32_t present_ms = 0;
        bool drew_video = false;
    };
    struct AudioStepStats {
        uint32_t fed_chunks = 0;
        uint32_t elapsed_ms = 0;
    };
    class Runtime;

    Nes();
    ~Nes() override;

    bool on_init() override;
    bool on_start() override;
    void on_stop() override;
    void on_deinit() override;

    std::expected<void, std::string> function_load(const boost::json::object &config_json);
    std::expected<void, std::string> function_start();
    std::expected<void, std::string> function_pause();
    std::expected<void, std::string> function_resume();
    std::expected<void, std::string> function_stop();
    std::expected<void, std::string> function_reset();
    std::expected<void, std::string> function_save();
    std::expected<void, std::string> function_set_gamepad_state(const boost::json::object &state_json);
    std::expected<std::string, std::string> function_get_state();

    std::vector<service::FunctionSchema> get_function_schemas() override;
    std::vector<service::EventSchema> get_event_schemas() override;
    service::ServiceBase::FunctionHandlerMap get_function_handlers() override;

    void set_state(State state);
    void publish_error(const std::string &message);
    bool start_frame_task_locked();
    bool start_audio_task_locked();
    void take_task_ids_locked(std::vector<lib_utils::TaskScheduler::TaskId> &task_ids);
    void cancel_and_wait_task_ids(const std::vector<lib_utils::TaskScheduler::TaskId> &task_ids);
    bool frame_task();
    bool audio_task();
    bool frame_step(bool draw_video, FrameStepStats *stats);
    bool audio_step(AudioStepStats *stats);

    mutable std::mutex mutex_;
    mutable std::mutex perf_mutex_;
    std::shared_ptr<Runtime> runtime_;
    Config config_;
    State state_ = State::Idle;
    lib_utils::TaskScheduler::TaskId frame_task_id_ = 0;
    lib_utils::TaskScheduler::TaskId audio_task_id_ = 0;
    int64_t next_frame_deadline_ms_ = 0;
    bool skip_next_video_frame_ = false;
    uint32_t base_frame_skip_ = 0;
    uint32_t current_frame_skip_ = 0;
    uint32_t frame_skip_remaining_ = 0;
    uint32_t consecutive_frame_skip_count_ = 0;
    uint32_t frame_late_count_ = 0;
    uint32_t frame_count_since_log_ = 0;
    uint32_t frame_draw_count_since_log_ = 0;
    uint32_t frame_skip_count_since_log_ = 0;
    uint32_t frame_backpressure_count_since_log_ = 0;
    uint64_t frame_total_step_ms_ = 0;
    uint64_t frame_draw_step_ms_ = 0;
    uint32_t frame_max_step_ms_ = 0;
    uint32_t frame_max_emulate_ms_ = 0;
    uint32_t frame_max_convert_ms_ = 0;
    uint32_t frame_max_present_ms_ = 0;
    uint32_t frame_max_late_by_ms_ = 0;
    uint32_t audio_chunks_since_log_ = 0;
    uint32_t audio_max_feed_ms_ = 0;
    int64_t next_perf_log_ms_ = 0;
};

} // namespace esp_brookesia::emulation
