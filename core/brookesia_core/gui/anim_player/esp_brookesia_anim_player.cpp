/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <vector>
#include "esp_heap_caps.h"
#include "private/esp_brookesia_anim_player_utils.hpp"
#include "esp_brookesia_anim_player.hpp"

#define THREAD_EXIT_CHECK_INTERVAL_MS       100

#define ANIM_EVENT_THREAD_NAME              "anim_event"
#define ANIM_EVENT_THREAD_STACK_SIZE        (10 * 1024)
#define ANIM_EVENT_THREAD_STACK_CAPS_EXT    (true)

namespace esp_brookesia::gui {

AnimPlayer::FlushReadySignal AnimPlayer::flush_ready_signal;
AnimPlayer::AnimationStopSignal AnimPlayer::animation_stop_signal;

AnimPlayer::~AnimPlayer()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (_is_begun && !del()) {
        ESP_UTILS_LOGE("Delete failed");
    }
}

bool AnimPlayer::begin(const AnimPlayerData &data)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (_is_begun) {
        ESP_UTILS_LOGW("Already begun");
        return true;
    }

    _canvas_config = data.canvas;
    _animation_configs = std::vector<AnimPlayerAnimConfig>(
                             data.source.animation_configs, data.source.animation_configs + data.source.animation_num
                         );

    // Update animation source
    if (data.flags.enable_source_partition) {
        ESP_UTILS_LOGD("Enable source partition");

        auto &partition_config = data.source.partition_config;
        const mmap_assets_config_t asset_config = {
            .partition_label = partition_config.partition_label,
            .max_files = partition_config.max_files,
            .checksum = partition_config.checksum,
            .flags = {
                .mmap_enable = true,
                .full_check = true,
            },
        };
        ESP_UTILS_CHECK_FALSE_RETURN(
            mmap_assets_new(&asset_config, &_assets_handle) == ESP_OK, false, "Failed to create mmap assets"
        );
        ESP_UTILS_CHECK_VALUE_GOTO(
            data.source.animation_num, 1, mmap_assets_get_stored_files(_assets_handle), err,
            "Animation num out of range"
        );

        for (int i = 0; i < data.source.animation_num; i++) {
            ESP_UTILS_LOGD("Animation %d: %s", i, mmap_assets_get_name(_assets_handle, i));
            _animation_configs[i].data_address = mmap_assets_get_mem(_assets_handle, i);
            _animation_configs[i].data_length = mmap_assets_get_size(_assets_handle, i);
        }
    } else {
        ESP_UTILS_LOGD("Disable source partition");
        for (int i = 0; i < data.source.animation_num; i++) {
            ESP_UTILS_LOGD(
                "Animation %d: address(%p), length(%d)", i, data.source.animation_configs[i].data_address,
                data.source.animation_configs[i].data_length
            );
            ESP_UTILS_CHECK_NULL_RETURN(
                data.source.animation_configs[i].data_address, false, "Invalid data address"
            );

            _animation_configs[i].data_address = data.source.animation_configs[i].data_address;
            _animation_configs[i].data_length = data.source.animation_configs[i].data_length;
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

                auto *self = static_cast<AnimPlayer *>(anim_player_get_user_data(handle));
                ESP_UTILS_CHECK_NULL_EXIT(self, "Invalid user data");

                std::unique_lock<std::mutex> lock(self->_player_mutex);
                if (event == PLAYER_EVENT_ALL_FRAME_DONE) {
                    if (self->_player_operation == Operation::PlayOnceStop) {
                        ESP_UTILS_LOGD("Animation play once stop: %d", self->_player_index);
                        if ((self->_event_queue.empty()) && !self->_player_flags.is_starting) {
                            self->sendEvent({-1, Operation::Stop, {true, true}}, false);
                        }
                    } else if (self->_player_operation == Operation::PlayOncePause) {
                        ESP_UTILS_LOGD("Animation play once pause: %d", self->_player_index);
                        self->_player_state = OperationState::Pause;
                    }
                    self->_player_flags.is_end = true;
                } else if (event == PLAYER_EVENT_IDLE) {
                    ESP_UTILS_LOGD("Animation idle: %d", self->_player_index);
                    self->_player_state = OperationState::Stop;
                    self->_player_flags.is_end = true;
                    self->_player_index = -1;
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
        ESP_UTILS_CHECK_NULL_GOTO(_player_handle, err, "Failed to create anim player");
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
                    auto event = _event_queue.front();
                    _event_queue.pop();

                    lock.unlock();
                    if (!processEvent(event)) {
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

    _is_begun = true;

    return true;

err:
    if (del()) {
        ESP_UTILS_LOGE("Failed to delete anim player");
    }

    return false;
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
    _is_begun = false;

    return true;
}

bool AnimPlayer::sendEvent(const Event &event, bool clear_queue)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD(
        "Param: event(%d,%d,%d,%d)", event.index, static_cast<int>(event.operation), event.flags.enable_interrupt,
        event.flags.force
    );

    std::lock_guard lock(_event_mutex);
    if (clear_queue) {
        while (!_event_queue.empty()) {
            ESP_UTILS_LOGD("Pop event: %d", _event_queue.front().index);
            _event_queue.pop();
        }
    }
    _event_queue.push(event);
    _event_cv.notify_all();

    return true;
}

bool AnimPlayer::waitAnimationStop()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::unique_lock<std::mutex> lock(_player_mutex);
    _player_flags.is_started = false;
    _player_condition.wait(lock, [this] {
        return (_player_state == OperationState::Stop) && _player_flags.is_started;
    });

    return true;
}

bool AnimPlayer::notifyFlushFinished() const
{
    // ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_NULL_RETURN(_player_handle, false, "Invalid handle");

    anim_player_flush_ready(_player_handle);

    return true;
}

bool AnimPlayer::processEvent(const Event &event)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD(
        "Param: event(%d,%d,%d,%d)", event.index, static_cast<int>(event.operation), event.flags.enable_interrupt,
        event.flags.force
    );

    if (!event.flags.force && (_player_index == event.index) && (_player_operation == event.operation)) {
        ESP_UTILS_LOGD("Animation already in index & operation");
        return true;
    }

    {
        ESP_UTILS_LOGD("Try to lock");
        std::unique_lock<std::mutex> lock(_player_mutex);
        ESP_UTILS_LOGD("Get lock");

        _player_flags.is_starting = true;

        if (!event.flags.enable_interrupt) {
            ESP_UTILS_LOGD("Do not enable interrupt");
            _player_flags.is_end = false;
            ESP_UTILS_LOGD("Wait for animation[%d] stop start", _player_index);
            while ((_player_state != OperationState::Stop) && !_event_thread_need_exit && !_player_flags.is_end) {
                _player_condition.wait_for(lock, std::chrono::milliseconds(THREAD_EXIT_CHECK_INTERVAL_MS));
            }
            ESP_UTILS_LOGD("Wait for animation[%d] stop end", _player_index);
            if (_event_thread_need_exit) {
                ESP_UTILS_LOGD("Event thread need exit, return true");
                return true;
            }
        }

        ESP_UTILS_LOGD("Update animation[%d] to stop", _player_index);
        anim_player_update(_player_handle, PLAYER_ACTION_STOP);

        ESP_UTILS_LOGD("Wait for animation[%d] stop start", _player_index);
        while ((_player_state != OperationState::Stop) && !_event_thread_need_exit) {
            _player_condition.wait_for(lock, std::chrono::milliseconds(THREAD_EXIT_CHECK_INTERVAL_MS));
        }
        ESP_UTILS_LOGD("Wait for animation[%d] stop end", _player_index);
        if (_event_thread_need_exit) {
            ESP_UTILS_LOGD("Event thread need exit, return false");
            return true;
        }
        _player_index = event.index;
        _player_operation = event.operation;

        // Then update animation
        switch (event.operation) {
        case Operation::PlayLoop:
        case Operation::PlayOnceStop:
        case Operation::PlayOncePause: {
            ESP_UTILS_CHECK_FALSE_RETURN(
                (_player_index >= 0) && (_player_index < static_cast<int>(_animation_configs.size())),
                false, "Invalid index: %d", _player_index
            );

            auto &config = _animation_configs[_player_index];
            uint32_t start = 0;
            uint32_t end = 0;
            bool is_repeat = (event.operation == Operation::PlayLoop);

            ESP_UTILS_LOGD("Animation[%d] set src data start", _player_index);
            lock.unlock();
            anim_player_set_src_data(_player_handle, config.data_address, config.data_length);
            lock.lock();
            ESP_UTILS_LOGD("Animation[%d] set src data end", _player_index);

            _player_flags.is_started = true;
            _player_state = OperationState::Play;
            anim_player_get_segment(_player_handle, &start, &end);
            anim_player_set_segment(_player_handle, start, end, config.fps, is_repeat);
            anim_player_update(_player_handle, PLAYER_ACTION_START);
            ESP_UTILS_LOGI(
                "Update animation: %d, start(%d), end(%d), fps(%d), is_repeat(%d)", _player_index,
                static_cast<int>(start), static_cast<int>(end), config.fps, is_repeat
            );
            break;
        }
        case Operation::Pause: {
            break;
        }
        case Operation::Stop:
            ESP_UTILS_LOGD("Release lock");
            lock.unlock();
            animation_stop_signal(
                _canvas_config.coord_x, _canvas_config.coord_y, _canvas_config.coord_x + _canvas_config.width,
                _canvas_config.coord_y + _canvas_config.height, this
            );
            break;
        default:
            ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Invalid operation: %d", static_cast<int>(event.operation));
            break;
        }
    }

    _player_flags.is_starting = false;

    return true;
}

} // namespace esp_brookesia::speaker
