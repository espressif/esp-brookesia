/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <iostream>
#include <filesystem>
#include <vector>
#include <fstream>
#include "esp_heap_caps.h"
#include "esp_brookesia_gui_internal.h"
#if !ESP_BROOKESIA_ANIM_PLAYER_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_anim_player_utils.hpp"
#include "esp_brookesia_anim_player.hpp"

#define THREAD_EXIT_CHECK_INTERVAL_MS       100

#define ANIM_EVENT_THREAD_NAME              "anim_event"
#define ANIM_EVENT_THREAD_STACK_SIZE        (10 * 1024)
#define ANIM_EVENT_THREAD_STACK_CAPS_EXT    (true)

namespace fs = std::filesystem;

namespace esp_brookesia::gui {

AnimPlayer::FlushReadySignal AnimPlayer::flush_ready_signal;
AnimPlayer::AnimationStopSignal AnimPlayer::animation_stop_signal;

AnimPlayer::~AnimPlayer()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (_is_begun) {
        ESP_UTILS_CHECK_FALSE_EXIT(del(), "Failed to delete anim player");
    }
}

bool AnimPlayer::begin(const AnimPlayerData &data)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (_is_begun) {
        ESP_UTILS_LOGW("Already begun");
        return true;
    }

    esp_utils::function_guard del_guard([this] {
        ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

        ESP_UTILS_CHECK_FALSE_EXIT(del(), "Failed to delete anim player");
    });

    // Update animation source
    if (std::holds_alternative<AnimPlayerPartitionConfig>(data.source)) {
        ESP_UTILS_LOGD("Enable source partition");

        auto &partition_config = std::get<AnimPlayerPartitionConfig>(data.source);
        ESP_UTILS_CHECK_FALSE_RETURN(
            loadAnimationConfig(partition_config), false, "Failed to load animation config"
        );
    } else if (std::holds_alternative<AnimPlayerResourcesConfig>(data.source)) {
        auto &resources_config = std::get<AnimPlayerResourcesConfig>(data.source);

        if (std::holds_alternative<const AnimPlayerAnimAddress *>(resources_config.resources)) {
            ESP_UTILS_LOGD("Enable source address");

            auto &anim_addresses = std::get<const AnimPlayerAnimAddress *>(resources_config.resources);
            ESP_UTILS_CHECK_FALSE_RETURN(
                loadAnimationConfig(anim_addresses, resources_config.num), false, "Failed to load animation config"
            );
        } else if (std::holds_alternative<const AnimPlayerAnimPath *>(resources_config.resources)) {
            ESP_UTILS_LOGD("Enable source path");

            auto &anim_paths = std::get<const AnimPlayerAnimPath *>(resources_config.resources);
            ESP_UTILS_CHECK_FALSE_RETURN(
                loadAnimationConfig(anim_paths, resources_config.num), false, "Failed to load animation config"
            );
        }
    }

    {
        anim_player_config_t config = {
            .flush_cb = [](anim_player_handle_t handle, int x1, int y1, int x2, int y2, const void *data)
            {
                // ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

                // ESP_UTILS_LOGD("Param: x1(%03d), y1(%03d), x2(%03d), y2(%03d), data(%p)", x1, y1, x2, y2, data);

                auto *self = static_cast<AnimPlayer *>(anim_player_get_user_data(handle));
                ESP_UTILS_CHECK_NULL_EXIT(self, "Invalid user data");
                auto &canvas_config = self->_canvas_config;

                ESP_UTILS_CHECK_FALSE_EXIT(
                    (x1 > 0 || y1 > 0 || x2 <= canvas_config.width),
                    "Invalid coordinates: (%03d,%03d)-(%03d,%03d)", x1, y1, x2, y2
                );

                int x_start = x1 + canvas_config.coord_x;
                int y_start = y1 + canvas_config.coord_y;
                int width = std::min(x2 - x1, canvas_config.width);
                int height = std::min(y2 - y1, canvas_config.height);
                int x_end = std::min(x_start + width, canvas_config.coord_x + canvas_config.width);
                int y_end = std::min(y_start + height, canvas_config.coord_y + canvas_config.height);

                flush_ready_signal(x_start, y_start, x_end, y_end, data, self);
            },
            .update_cb = [](anim_player_handle_t handle, player_event_t event)
            {
                // ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

                // ESP_UTILS_LOGD("Param: handle(%p), event(%d)", handle, static_cast<int>(event));

                if ((event != PLAYER_EVENT_ALL_FRAME_DONE) && (event != PLAYER_EVENT_IDLE)) {
                    return;
                }

                auto *self = static_cast<AnimPlayer *>(anim_player_get_user_data(handle));
                ESP_UTILS_CHECK_NULL_EXIT(self, "Invalid user data");

                std::unique_lock<std::mutex> lock(self->_player_mutex);

                if (event == PLAYER_EVENT_ALL_FRAME_DONE) {
                    self->_player_flags.is_frame_done = true;
                } else if (event == PLAYER_EVENT_IDLE) {
                    self->_player_state = OperationState::Stop;

                    auto &event_wrapper = self->_current_event;
                    ESP_UTILS_CHECK_NULL_EXIT(event_wrapper, "Invalid current event");

                    if (event_wrapper->event.operation == Operation::PlayOnceStop) {
                        ESP_UTILS_LOGD("Animation play once stop: %d", event_wrapper->event.index);

                        if (self->_event_queue.empty() && !self->_player_flags.is_starting) {
                            self->sendEvent({-1, Operation::Stop, {true, true}}, false);
                        } else {
                            if (event_wrapper->promise != nullptr) {
                                event_wrapper->promise->set_value();
                            }
                            event_wrapper.reset();
                        }
                    } else {
                        if (event_wrapper->event.operation == Operation::PlayOncePause) {
                            ESP_UTILS_LOGD("Animation play once pause: %d", event_wrapper->event.index);

                            self->_player_state = OperationState::Pause;
                        } else {
                            ESP_UTILS_LOGD("Animation stop: %d", event_wrapper->event.index);
                        }

                        if (event_wrapper->promise != nullptr) {
                            event_wrapper->promise->set_value();
                        }
                        event_wrapper.reset();
                    }
                }

                self->_player_condition.notify_all();
            },
            .user_data = this,
            .flags = {
                .swap = static_cast<unsigned char>(data.flags.enable_data_swap_bytes),
            },
            .task = {
                .task_priority = data.task.task_priority,
                .task_stack = data.task.task_stack,
                .task_affinity = data.task.task_affinity,
                .task_stack_caps = static_cast<unsigned>(
                    (data.task.task_stack_in_ext ? MALLOC_CAP_SPIRAM : MALLOC_CAP_DEFAULT) | MALLOC_CAP_8BIT
                ),
            },
        };
        _player_handle = anim_player_init(&config);
        ESP_UTILS_CHECK_NULL_RETURN(_player_handle, false, "Failed to create anim player");
    }

    _event_thread_need_exit = false;
    {
        esp_utils::thread_config_guard thread_config(esp_utils::ThreadConfig{
            .name = ANIM_EVENT_THREAD_NAME,
            .stack_size = ANIM_EVENT_THREAD_STACK_SIZE,
            .stack_in_ext = ANIM_EVENT_THREAD_STACK_CAPS_EXT,
        });
        _event_thread = boost::thread([this] {
            ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

            while (!_event_thread_need_exit)
            {
                std::unique_lock<std::mutex> lock(_event_mutex);
                while (_event_queue.empty() && !_event_thread_need_exit) {
                    _event_cv.wait_for(lock, std::chrono::milliseconds(THREAD_EXIT_CHECK_INTERVAL_MS));
                }
                if (_event_thread_need_exit) {
                    ESP_UTILS_LOGD("Event thread not running, exit");
                    break;
                }

                while (!_event_queue.empty() && !_event_thread_need_exit) {
                    auto event_wrapper = _event_queue.front();
                    _event_queue.pop();

                    lock.unlock();
                    if (!processEvent(event_wrapper)) {
                        ESP_UTILS_LOGE("Failed to process event");
                    }
                    lock.lock();
                }
                if (_event_thread_need_exit) {
                    ESP_UTILS_LOGD("Event thread not running, exit");
                    break;
                }
            }
        });
    }

    del_guard.release();
    _is_begun = true;
    _canvas_config = data.canvas;

    return true;
}

bool AnimPlayer::del()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    {
        std::lock_guard lock(_event_mutex);
        _event_thread_need_exit = true;
        _event_cv.notify_all();
    }
    if (_event_thread.joinable()) {
        _event_thread.join();
    }

    if (_player_handle != nullptr) {
        anim_player_deinit(_player_handle);
        _player_handle = nullptr;
    }

    if (_assets_handle != nullptr) {
        mmap_assets_del(_assets_handle);
        _assets_handle = nullptr;
    }

    _animation_configs.clear();
    _animation_data.clear();
    _is_begun = false;

    return true;
}

bool AnimPlayer::sendEvent(const Event &event, bool clear_queue, EventFuture *future)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD(
        "Param: event(%d,%d,%d,%d)", event.index, static_cast<int>(event.operation), event.flags.enable_interrupt,
        event.flags.force
    );

    std::lock_guard lock(_event_mutex);
    if (clear_queue) {
        while (!_event_queue.empty()) {
            auto event_wrapper = _event_queue.front();
            _event_queue.pop();

            ESP_UTILS_LOGD("Pop event: %d", event_wrapper->event.index);
            if (event_wrapper->promise != nullptr) {
                event_wrapper->promise->set_value();
            }
        }
    }

    std::shared_ptr<EventPromise> promise = nullptr;
    if (future != nullptr) {
        ESP_UTILS_CHECK_EXCEPTION_RETURN(
            promise = std::make_shared<EventPromise>(), false, "Failed to create event promise"
        );
    }

    std::shared_ptr<EventWrapper> event_wrapper = nullptr;
    ESP_UTILS_CHECK_EXCEPTION_RETURN(
        event_wrapper = std::make_shared<EventWrapper>(EventWrapper{event, promise}), false,
        "Failed to create event wrapper"
    );

    _event_queue.emplace(event_wrapper);
    _event_cv.notify_all();

    if (future != nullptr) {
        *future = promise->get_future();
    }

    return true;
}

bool AnimPlayer::notifyFlushFinished() const
{
    // ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_NULL_RETURN(_player_handle, false, "Invalid handle");

    anim_player_flush_ready(_player_handle);

    return true;
}

bool AnimPlayer::loadAnimationConfig(const AnimPlayerPartitionConfig &partition_config)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    mmap_assets_config_t asset_config = {
        .partition_label = partition_config.partition_label,
        .max_files = partition_config.max_files,
        .checksum = partition_config.checksum,
        .flags = {
            .mmap_enable = true,
            .full_check = true,
        },
    };
    ESP_UTILS_CHECK_ERROR_RETURN(mmap_assets_new(&asset_config, &_assets_handle), false, "Failed to create mmap assets");

    auto file_num = mmap_assets_get_stored_files(_assets_handle);
    ESP_UTILS_CHECK_FALSE_RETURN(file_num > 0, false, "No animation files");

    _animation_configs.resize(file_num);
    for (int i = 0; i < file_num; i++) {
        ESP_UTILS_LOGD("Load animation %d: %s, fps(%d)", i, mmap_assets_get_name(_assets_handle, i), partition_config.fps[i]);
        _animation_configs[i].data_address = mmap_assets_get_mem(_assets_handle, i);
        _animation_configs[i].data_length = mmap_assets_get_size(_assets_handle, i);
        _animation_configs[i].fps = partition_config.fps[i];
    }

    return true;
}

bool AnimPlayer::loadAnimationConfig(const AnimPlayerAnimAddress *anim_address, int num)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    _animation_configs.clear();
    for (int i = 0; i < num; i++) {
        ESP_UTILS_LOGD(
            "Load animation %d: address(%p), length(%d), fps(%d)", i, anim_address[i].data_address,
            anim_address[i].data_length, anim_address[i].fps
        );
        ESP_UTILS_CHECK_NULL_RETURN(anim_address[i].data_address, false, "Invalid data address");

        _animation_configs.emplace_back(anim_address[i]);
    }

    return true;
}

bool AnimPlayer::loadAnimationConfig(const AnimPlayerAnimPath *anim_path, int num)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    _animation_data.clear();
    for (int i = 0; i < num; i++) {
        ESP_UTILS_CHECK_FALSE_RETURN(fs::exists(anim_path[i].path), false, "File not exists: %s", anim_path[i].path);

        std::ifstream file(anim_path[i].path, std::ios::binary);
        ESP_UTILS_CHECK_FALSE_RETURN(file.is_open(), false, "Failed to open file: %s", anim_path[i].path);

        ESP_UTILS_LOGD("Load animation %d: %s, fps(%d)", i, anim_path[i].path, anim_path[i].fps);
        _animation_data.emplace_back(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        _animation_configs.emplace_back(AnimPlayerAnimAddress{
            .data_address = _animation_data.back().data(),
            .data_length = _animation_data.back().size(),
            .fps = anim_path[i].fps,
        });
    }

    return true;
}

bool AnimPlayer::waitPlayerFrameDone()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::unique_lock<std::mutex> lock(_player_mutex);
    _player_flags.is_frame_done = false;
    while (!_event_thread_need_exit && !_player_flags.is_frame_done && (_player_state != OperationState::Stop)) {
        _player_condition.wait_for(lock, std::chrono::milliseconds(THREAD_EXIT_CHECK_INTERVAL_MS));
    }

    return true;
}

bool AnimPlayer::waitPlayerIdle()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::unique_lock<std::mutex> lock(_player_mutex);
    while (!_event_thread_need_exit && (_player_state != OperationState::Stop) && (_player_state != OperationState::Pause)) {
        _player_condition.wait_for(lock, std::chrono::milliseconds(THREAD_EXIT_CHECK_INTERVAL_MS));
    }

    return true;
}

bool AnimPlayer::waitPlayerState(OperationState state)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: state(%d)", static_cast<int>(state));

    if (_player_state & state) {
        ESP_UTILS_LOGD("Already meet target state");
        return true;
    }

    std::unique_lock<std::mutex> lock(_player_mutex);
    while (!_event_thread_need_exit && !(_player_state & state)) {
        _player_condition.wait_for(lock, std::chrono::milliseconds(THREAD_EXIT_CHECK_INTERVAL_MS));
        ESP_UTILS_LOGD("Not meet target state, current state: %d", static_cast<int>(_player_state));
    }

    return true;
}

bool AnimPlayer::processEvent(std::shared_ptr<EventWrapper> event_wrapper)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_NULL_RETURN(event_wrapper, false, "Invalid event wrapper");

    auto &event = event_wrapper->event;
    ESP_UTILS_LOGD(
        "Param: event(%d,%d,%d,%d)", event.index, static_cast<int>(event.operation), event.flags.enable_interrupt,
        event.flags.force
    );

    if (!event.flags.force && (_current_event != nullptr) && (_current_event->event.index == event.index) &&
            (_current_event->event.operation == event.operation)) {
        ESP_UTILS_LOGD("Animation already in index & operation");
        return true;
    }

    {
        _player_flags.is_starting = true;
        esp_utils::function_guard end_guard([this]() {
            ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();
            _player_flags.is_starting = false;
        });

        if (_current_event != nullptr) {
            if (!event.flags.enable_interrupt) {
                ESP_UTILS_LOGD("Do not enable interrupt, wait player frame done");
                ESP_UTILS_CHECK_FALSE_RETURN(waitPlayerFrameDone(), false, "Failed to wait player frame done");
                if (_event_thread_need_exit) {
                    ESP_UTILS_LOGD("Event thread need exit, return true");
                    return true;
                }
            }

            ESP_UTILS_LOGD("Update current event[%d] to stop", _current_event->event.index);
            anim_player_update(_player_handle, PLAYER_ACTION_STOP);

            ESP_UTILS_LOGD("Wait player idle");
            ESP_UTILS_CHECK_FALSE_RETURN(waitPlayerIdle(), false, "Failed to wait player idle");
            if (_event_thread_need_exit) {
                ESP_UTILS_LOGD("Event thread need exit, return true");
                return true;
            }
        }

        // Then update animation
        auto index = event.index;
        switch (event.operation) {
        case Operation::PlayLoop:
        case Operation::PlayOnceStop:
        case Operation::PlayOncePause: {
            _current_event = event_wrapper;

            ESP_UTILS_CHECK_FALSE_RETURN(
                (index >= 0) && (index < static_cast<int>(_animation_configs.size())), false, "Invalid index: %d", index
            );

            auto &config = _animation_configs[index];
            uint32_t start = 0;
            uint32_t end = 0;
            bool is_repeat = (event.operation == Operation::PlayLoop);

            ESP_UTILS_LOGD("Animation[%d] set src data start", index);
            ESP_UTILS_CHECK_ERROR_RETURN(
                anim_player_set_src_data(_player_handle, config.data_address, config.data_length), false,
                "Failed to set src data"
            );
            ESP_UTILS_LOGD("Animation[%d] set src data end", index);

            _player_state = OperationState::Play;
            anim_player_get_segment(_player_handle, &start, &end);
            anim_player_set_segment(_player_handle, start, end, config.fps, is_repeat);
            anim_player_update(_player_handle, PLAYER_ACTION_START);
            ESP_UTILS_LOGI(
                "Update animation: %d, start(%d), end(%d), fps(%d), is_repeat(%d)", index,
                static_cast<int>(start), static_cast<int>(end), config.fps, is_repeat
            );
            break;
        }
        case Operation::Pause: {
            break;
        }
        case Operation::Stop:
            animation_stop_signal(
                _canvas_config.coord_x, _canvas_config.coord_y, _canvas_config.coord_x + _canvas_config.width,
                _canvas_config.coord_y + _canvas_config.height, this
            );
            if (_current_event != nullptr) {
                // In this case, the current event type is PlayOnceStop, so we need to send value to the current event
                if (_current_event->promise != nullptr) {
                    _current_event->promise->set_value();
                }
                _current_event.reset();
            }
            break;
        default:
            ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Invalid operation: %d", static_cast<int>(event.operation));
            break;
        }
    }

    return true;
}

} // namespace esp_brookesia::gui
