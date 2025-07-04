/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <atomic>
#include <vector>
#include <queue>
#include <condition_variable>
#include "boost/signals2/signal.hpp"
#include "boost/thread.hpp"
#include "esp_mmap_assets.h"
#include "anim_player.h"

namespace esp_brookesia::gui {

struct AnimPlayerCanvasConfig {
    int coord_x;
    int coord_y;
    int width;
    int height;
};

struct AnimPlayerAnimConfig {
    const void *data_address;
    size_t data_length;
    int fps;
};

struct AnimPlayerPartitionConfig {
    const char *partition_label;
    int max_files;
    uint32_t checksum;
};

struct AnimPlayerData {
    AnimPlayerCanvasConfig canvas;
    struct {
        int task_priority;
        int task_stack;
        int task_affinity;
        bool task_stack_in_ext;
    } task;
    struct {
        int animation_num;
        const gui::AnimPlayerAnimConfig *animation_configs;
        gui::AnimPlayerPartitionConfig partition_config;
    } source;
    struct {
        int enable_source_partition: 1;
        int enable_data_swap_bytes: 1;
    } flags;
};

class AnimPlayer {
public:
    enum class Operation {
        Stop,
        PlayLoop,
        PlayOnceStop,
        PlayOncePause,
        Pause,
    };

    enum class OperationState {
        Stop,
        Play,
        Pause,
    };

    struct Event {
        int index;
        Operation operation;
        struct {
            int enable_interrupt: 1;
            int force: 1;
        } flags;
    };

    using FlushReadySignal = boost::signals2::signal <
                             void(int x_start, int y_start, int x_end, int y_end, const void *data, AnimPlayer *player)
                             >;
    using AnimationStopSignal = boost::signals2::signal <
                                void(int x_start, int y_start, int x_end, int y_end, AnimPlayer *player)
                                >;
    using AnimationEndSignal = boost::signals2::signal<void(AnimPlayer *player)>;

    static constexpr int INDEX_NONE = -1;

    AnimPlayer() = default;
    ~AnimPlayer();

    AnimPlayer(const AnimPlayer &) = delete;
    AnimPlayer &operator=(const AnimPlayer &) = delete;

    bool begin(const AnimPlayerData &data);
    bool del();

    bool sendEvent(const Event &event, bool clear_queue);

    bool waitAnimationStop();
    bool notifyFlushFinished() const;

    static FlushReadySignal flush_ready_signal;
    static AnimationStopSignal animation_stop_signal;

private:
    bool processEvent(const Event &event);

    bool _is_begun = false;
    AnimPlayerCanvasConfig _canvas_config = {};
    std::vector<AnimPlayerAnimConfig> _animation_configs;

    std::atomic<bool> _event_thread_need_exit = false;
    boost::thread _event_thread;
    std::queue<Event> _event_queue;
    std::mutex _event_mutex;
    std::condition_variable _event_cv;

    std::mutex _player_mutex;
    struct {
        int is_started: 1;
        int is_starting: 1;
        int is_end: 1;
    } _player_flags = {};
    int _player_index = -1;
    OperationState _player_state = OperationState::Stop;
    Operation _player_operation = Operation::Stop;
    std::condition_variable _player_condition;
    anim_player_handle_t _player_handle = nullptr;
    mmap_assets_handle_t _assets_handle = nullptr;
};

} // namespace esp_brookesia::speaker
