/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <variant>
#include <vector>
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

struct AnimPlayerAnimAddress {
    const void *data_address;
    size_t data_length;
    int fps;
};

struct AnimPlayerAnimPath {
    const char *path;
    int fps;
};

struct AnimPlayerResourcesConfig {
    int num;
    std::variant<const AnimPlayerAnimAddress *, const AnimPlayerAnimPath *> resources;
};

struct AnimPlayerPartitionConfig {
    const char *partition_label;
    int max_files;
    const int *fps;
    uint32_t checksum;
};

struct AnimPlayerData {
    int getAnimationNum() const
    {
        if (std::holds_alternative<AnimPlayerResourcesConfig>(source)) {
            return std::get<AnimPlayerResourcesConfig>(source).num;
        } else if (std::holds_alternative<AnimPlayerPartitionConfig>(source)) {
            return std::get<AnimPlayerPartitionConfig>(source).max_files;
        }
        return 0;
    }

    AnimPlayerCanvasConfig canvas;
    std::variant<AnimPlayerResourcesConfig, AnimPlayerPartitionConfig> source;
    struct {
        int task_priority;
        int task_stack;
        int task_affinity;
        bool task_stack_in_ext;
    } task;
    struct {
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
        Stop  = (1U << 0),
        Play  = (1U << 1),
        Pause = (1U << 2),
    };

    friend bool operator|(OperationState a, OperationState b)
    {
        return static_cast<bool>(static_cast<OperationState>(static_cast<int>(a) | static_cast<int>(b)));
    }
    friend bool operator&(OperationState a, OperationState b)
    {
        return static_cast<bool>(static_cast<OperationState>(static_cast<int>(a) & static_cast<int>(b)));
    }

    struct Event {
        int index;
        Operation operation;
        struct {
            int enable_interrupt: 1;
            int force: 1;
        } flags;
    };

    using EventFuture = std::future<void>;

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

    bool sendEvent(const Event &event, bool clear_queue, EventFuture *future = nullptr);

    bool notifyFlushFinished() const;

    static FlushReadySignal flush_ready_signal;
    static AnimationStopSignal animation_stop_signal;

private:
    using EventPromise = std::promise<void>;
    struct EventWrapper {
        Event event;
        std::shared_ptr<EventPromise> promise;
    };

    bool loadAnimationConfig(const AnimPlayerPartitionConfig &partition_config);
    bool loadAnimationConfig(const AnimPlayerAnimAddress *anim_address, int num);
    bool loadAnimationConfig(const AnimPlayerAnimPath *anim_path, int num);
    bool waitPlayerFrameDone();
    bool waitPlayerIdle();
    bool waitPlayerState(OperationState state);
    bool processEvent(std::shared_ptr<EventWrapper> event_wrapper);

    bool _is_begun = false;
    AnimPlayerCanvasConfig _canvas_config = {};
    std::vector<AnimPlayerAnimAddress> _animation_configs;
    std::vector<std::vector<uint8_t>> _animation_data;

    std::atomic<bool> _event_thread_need_exit = false;
    boost::thread _event_thread;
    std::queue<std::shared_ptr<EventWrapper>> _event_queue;
    std::shared_ptr<EventWrapper> _current_event;
    std::mutex _event_mutex;
    std::condition_variable _event_cv;

    std::mutex _player_mutex;
    struct {
        int is_starting: 1;
        int is_frame_done: 1;
    } _player_flags;
    OperationState _player_state = OperationState::Stop;
    std::condition_variable _player_condition;
    anim_player_handle_t _player_handle = nullptr;
    mmap_assets_handle_t _assets_handle = nullptr;
};

} // namespace esp_brookesia::gui
