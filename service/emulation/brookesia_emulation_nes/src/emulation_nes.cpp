/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_audio.hpp"
#include "brookesia/service_display.hpp"
#include "brookesia/emulation_nes/emulation_nes.hpp"

extern "C" {
#include "nofrendo.h"
#include "nes/input.h"
#include "nes/state.h"
}

#if !BROOKESIA_EMULATION_NES_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

namespace esp_brookesia::emulation {

namespace {

constexpr std::string_view DEFAULT_DISPLAY_SOURCE_ROLE = "game";
constexpr uint32_t NES_NATIVE_WIDTH = 256;
constexpr uint32_t NES_NATIVE_HEIGHT = 240;
constexpr uint32_t NES_VISIBLE_HEIGHT = 224;
constexpr uint32_t NES_VISIBLE_Y_OFFSET = 8;
constexpr uint32_t RGB565_BYTES_PER_PIXEL = 2;
constexpr size_t NES_VIDEO_BUFFER_COUNT = 2;
constexpr size_t NES_PRESENT_BUFFER_COUNT = 3;
constexpr uint8_t NES_AUDIO_SAMPLE_BITS = 16;
constexpr uint8_t NES_AUDIO_FRAME_DURATION_MS = 16;
constexpr uint32_t NES_AUDIO_FEED_INTERVAL_MS = 4;
constexpr const char *NES_AUDIO_SOURCE_NAME = "NES";
constexpr const char *NES_AUDIO_SOURCE_ROLE = "game";
constexpr const char *NES_AUDIO_OUTPUT_NAME = "Speaker0";
constexpr size_t NES_ROM_MAX_SIZE = 0x200000;
constexpr int NES_TASK_STOP_TIMEOUT_MS = 3000;
constexpr int NES_PRESENT_DRAIN_EXTRA_TIMEOUT_MS = 1000;
const lib_utils::TaskScheduler::Group NES_FRAME_TASK_GROUP = "Frame";
const lib_utils::TaskScheduler::Group NES_AUDIO_TASK_GROUP = "Audio";

constexpr uint32_t get_auto_frame_skip_max()
{
    return std::max<uint32_t>(
               BROOKESIA_EMULATION_NES_AUTO_FRAME_SKIP_MAX,
               BROOKESIA_EMULATION_NES_MAX_CONSECUTIVE_FRAME_SKIP
           );
}

std::string to_string(Nes::State state)
{
    return std::string(BROOKESIA_DESCRIBE_ENUM_TO_STR(state));
}

inline int64_t get_current_time_ms()
{
    using namespace std::chrono;
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()
           ).count();
}

std::expected<void, std::string> ensure_path_exists(const std::string &path)
{
    if (path.empty()) {
        return std::unexpected("ROM path is empty");
    }

    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return std::unexpected("ROM path does not exist: " + path);
    }
    if (!std::filesystem::is_regular_file(path, ec) || ec) {
        return std::unexpected("ROM path is not a regular file: " + path);
    }
    return {};
}

std::string map_rom_load_error(int error)
{
    switch (error) {
    case -1:
        return "Failed to load NES ROM";
    case -2:
        return "Unsupported NES mapper";
    case -3:
        return "NES BIOS file is required";
    default:
        return "Unsupported NES ROM";
    }
}

int gamepad_to_nes_buttons(Nes::GamepadState state)
{
    int buttons = 0;
    if (state.a) {
        buttons |= NES_PAD_A;
    }
    if (state.b) {
        buttons |= NES_PAD_B;
    }
    if (state.select) {
        buttons |= NES_PAD_SELECT;
    }
    if (state.start) {
        buttons |= NES_PAD_START;
    }

    if (state.up != state.down) {
        buttons |= state.up ? NES_PAD_UP : NES_PAD_DOWN;
    }
    if (state.left != state.right) {
        buttons |= state.left ? NES_PAD_LEFT : NES_PAD_RIGHT;
    }
    return buttons;
}

struct FreeDeleter {
    void operator()(void *ptr) const
    {
        std::free(ptr);
    }
};

using OwnedBuffer = std::unique_ptr<uint8_t, FreeDeleter>;

void log_buffer_allocation(std::string_view label, const void *ptr, size_t size)
{
#if BROOKESIA_EMULATION_NES_LOG_BUFFER_MEMORY
    BROOKESIA_LOGI(
        "NES buffer: name=%1%, size=%2%, ptr=%3%",
        label, size, ptr
    );
#else
    (void)label;
    (void)ptr;
    (void)size;
#endif
}

std::expected<OwnedBuffer, std::string> allocate_buffer(size_t size, std::string_view label)
{
    if (size == 0) {
        return std::unexpected("Buffer size is zero");
    }

    auto *ptr = std::malloc(size);
    if (ptr == nullptr) {
        return std::unexpected("Failed to allocate buffer: " + std::string(label));
    }

    OwnedBuffer buffer(static_cast<uint8_t *>(ptr));
    log_buffer_allocation(label, buffer.get(), size);
    return buffer;
}

class HeapBuffer {
public:
    HeapBuffer() = default;
    HeapBuffer(HeapBuffer &&) noexcept = default;
    HeapBuffer &operator=(HeapBuffer &&) noexcept = default;

    HeapBuffer(const HeapBuffer &) = delete;
    HeapBuffer &operator=(const HeapBuffer &) = delete;

    std::expected<void, std::string> resize(size_t size, std::string_view label)
    {
        if ((size_ == size) && data_) {
            return {};
        }

        auto buffer_result = allocate_buffer(size, label);
        if (!buffer_result) {
            return std::unexpected(buffer_result.error());
        }
        data_ = std::move(buffer_result.value());
        size_ = size;
        return {};
    }

    void clear()
    {
        data_.reset();
        size_ = 0;
    }

    void swap(HeapBuffer &other) noexcept
    {
        data_.swap(other.data_);
        std::swap(size_, other.size_);
    }

    uint8_t *data()
    {
        return data_.get();
    }

    const uint8_t *data() const
    {
        return data_.get();
    }

    size_t size() const
    {
        return size_;
    }

    bool empty() const
    {
        return (data_ == nullptr) || (size_ == 0);
    }

private:
    OwnedBuffer data_;
    size_t size_ = 0;
};

std::expected<std::pair<OwnedBuffer, size_t>, std::string> load_file_to_memory(const std::string &path)
{
    auto path_result = ensure_path_exists(path);
    if (!path_result) {
        return std::unexpected(path_result.error());
    }

    std::error_code ec;
    const auto file_size = std::filesystem::file_size(path, ec);
    if (ec) {
        return std::unexpected("Failed to get ROM file size: " + ec.message());
    }
    if (file_size < 16 || file_size > NES_ROM_MAX_SIZE) {
        return std::unexpected("Invalid ROM file size: " + std::to_string(file_size));
    }

    auto buffer_result = allocate_buffer(static_cast<size_t>(file_size), "rom");
    if (!buffer_result) {
        return std::unexpected(buffer_result.error() + ": " + path);
    }
    auto buffer = std::move(buffer_result.value());

    auto *fp = std::fopen(path.c_str(), "rb");
    if (fp == nullptr) {
        return std::unexpected("Failed to open ROM path: " + path);
    }
    lib_utils::FunctionGuard close_guard([fp]() {
        std::fclose(fp);
    });

    const auto file_size_bytes = static_cast<size_t>(file_size);
    const auto read_count = std::fread(buffer.get(), 1, file_size_bytes, fp);
    if (read_count != file_size_bytes) {
        return std::unexpected("Failed to read complete ROM file: " + path);
    }

    return std::make_pair(std::move(buffer), file_size_bytes);
}

} // namespace

class Nes::Runtime: public std::enable_shared_from_this<Nes::Runtime> {
public:
    struct PerfSnapshot {
        Nes::AudioMode audio_mode = Nes::AudioMode::Disabled;
        bool audio_started = false;
        size_t audio_queue_depth = 0;
        uint32_t audio_drop_count = 0;
        uint32_t render_width = 0;
        uint32_t render_height = 0;
        Nes::VideoMode video_mode = Nes::VideoMode::Fit;
        uint32_t display_backpressure_count = 0;
        uint32_t present_drop_count = 0;
        uint32_t present_error_count = 0;
    };

    std::expected<void, std::string> load(const Config &config)
    {
        std::lock_guard lock(runtime_mutex_);

        auto path_result = ensure_path_exists(config.rom_path);
        if (!path_result) {
            return path_result;
        }

        config_ = config;
        unload_locked();

        auto display_result = setup_display();
        if (!display_result) {
            return display_result;
        }

        auto emulator_result = setup_emulator();
        if (!emulator_result) {
            unload_locked();
            return emulator_result;
        }

        auto audio_result = setup_audio();
        if (!audio_result) {
            unload_locked();
            return audio_result;
        }

        BROOKESIA_LOGI(
            "Loaded NES ROM path=%1%, output=%2%, source=%3%", config_.rom_path, output_name_,
            config_.display_source_name
        );
        return {};
    }

    void unload()
    {
        std::lock_guard lock(runtime_mutex_);
        unload_locked();
    }

    std::expected<void, std::string> reset()
    {
        std::lock_guard lock(emulator_mutex_);
        if (nes_ == nullptr) {
            return std::unexpected("NES runtime is not loaded");
        }
        nes_reset(true);
        return {};
    }

    std::expected<void, std::string> save()
    {
        if (config_.save_path.empty()) {
            return {};
        }

        std::filesystem::path save_path(config_.save_path);
        std::error_code ec;
        if (save_path.has_parent_path()) {
            std::filesystem::create_directories(save_path.parent_path(), ec);
            if (ec) {
                return std::unexpected("Failed to create save directory: " + ec.message());
            }
        }

        {
            std::lock_guard lock(emulator_mutex_);
            if (state_save(config_.save_path.c_str()) != 0) {
                return std::unexpected("Failed to write save path: " + config_.save_path);
            }
        }
        return {};
    }

    std::expected<void, std::string> set_gamepad_state(GamepadState state)
    {
        std::lock_guard lock(gamepad_mutex_);
        gamepad_state_ = state;
        return {};
    }

    PerfSnapshot get_perf_snapshot()
    {
        PerfSnapshot snapshot;
        {
            std::lock_guard lock(runtime_mutex_);
            snapshot.audio_mode = config_.audio_mode;
            snapshot.render_width = render_width_;
            snapshot.render_height = render_height_;
            snapshot.video_mode = config_.video_mode;
        }
        {
            std::lock_guard lock(audio_queue_mutex_);
            snapshot.audio_started = audio_started_;
            snapshot.audio_queue_depth = audio_queue_count_;
            snapshot.audio_drop_count = audio_drop_count_;
        }
        {
            std::lock_guard lock(present_mutex_);
            snapshot.display_backpressure_count = display_backpressure_count_;
            snapshot.present_drop_count = present_drop_count_;
            snapshot.present_error_count = present_error_count_;
        }
        return snapshot;
    }

    bool is_audio_started() const
    {
        std::lock_guard lock(audio_queue_mutex_);
        return audio_started_;
    }

    bool consume_display_backpressure()
    {
        std::lock_guard lock(present_mutex_);
        const bool pending = display_backpressure_pending_;
        display_backpressure_pending_ = false;
        return pending;
    }

    bool step_frame(bool draw_video, Nes::FrameStepStats *stats)
    {
        if (stats != nullptr) {
            *stats = {};
            stats->drew_video = draw_video;
        }

        uint32_t source_id = 0;
        std::string output_name;
        uint32_t output_x = 0;
        uint32_t output_y = 0;
        uint32_t render_width = 0;
        uint32_t render_height = 0;
        bool output_is_buffer = false;
        std::optional<size_t> present_buffer_index;
        {
            std::lock_guard lock(runtime_mutex_);
            if (nes_ == nullptr) {
                return false;
            }
            if (draw_video) {
                if (source_id_ == 0) {
                    return false;
                }
                source_id = source_id_;
                output_name = output_name_;
                output_x = output_x_;
                output_y = output_y_;
                render_width = render_width_;
                render_height = render_height_;
                output_is_buffer = output_is_buffer_;
            }
        }
        if (draw_video && !output_is_buffer) {
            present_buffer_index = reserve_present_buffer(
                                       static_cast<size_t>(render_width) * render_height * RGB565_BYTES_PER_PIXEL
                                   );
            if (!present_buffer_index.has_value()) {
                draw_video = false;
            }
        }
        if (stats != nullptr) {
            stats->drew_video = draw_video;
        }

        const uint8_t *frame_vidbuf = nullptr;
        {
            std::lock_guard lock(emulator_mutex_);
            if (nes_ == nullptr) {
                if (present_buffer_index.has_value()) {
                    release_present_buffer(present_buffer_index.value(), service::display::PresentResult::Error);
                }
                return false;
            }
            if (draw_video) {
                nes_vidbuf_index_ = (nes_vidbuf_index_ + 1) % nes_vidbufs_.size();
            }
            frame_vidbuf = nes_vidbufs_[nes_vidbuf_index_].data();
            nes_setvidbuf(const_cast<uint8_t *>(frame_vidbuf));
            {
                std::lock_guard gamepad_lock(gamepad_mutex_);
                input_update(0, gamepad_to_nes_buttons(gamepad_state_));
                input_update(1, 0);
            }
            const auto emulate_start_ms = get_current_time_ms();
            nes_emulate(draw_video);
            if (stats != nullptr) {
                stats->emulate_ms = static_cast<uint32_t>(get_current_time_ms() - emulate_start_ms);
            }
            enqueue_audio_frame();
        }

        if (!draw_video) {
            return true;
        }
        if (frame_vidbuf == nullptr) {
            if (present_buffer_index.has_value()) {
                release_present_buffer(present_buffer_index.value(), service::display::PresentResult::Error);
            }
            return false;
        }

        service::display::FrameInfo frame{
            .x = output_x,
            .y = output_y,
            .width = render_width,
            .height = render_height,
            .pixel_format = service::display::PixelFormat::RGB565,
        };
        service::display::PresentResult result = service::display::PresentResult::Error;
        if (output_is_buffer) {
            const auto present_start_ms = get_current_time_ms();
            auto buffer_writer = [this, frame_vidbuf, &frame, stats](service::Display::BufferOutputView & view) {
                auto *output_data = view.buffer.to_ptr<uint8_t>();
                if (output_data == nullptr) {
                    return false;
                }
                const auto convert_start_ms = get_current_time_ms();
                const size_t dst_offset = static_cast<size_t>(frame.y) * view.stride_bytes +
                                          static_cast<size_t>(frame.x) * RGB565_BYTES_PER_PIXEL;
                convert_frame_to(frame_vidbuf, output_data + dst_offset, view.stride_bytes);
                if (stats != nullptr) {
                    stats->convert_ms = static_cast<uint32_t>(get_current_time_ms() - convert_start_ms);
                }
                return true;
            };
            result = service::Display::get_instance().present_buffer_frame_sync(source_id, output_name, frame, buffer_writer);
            if (stats != nullptr) {
                stats->present_ms = static_cast<uint32_t>(get_current_time_ms() - present_start_ms);
            }
        } else {
            if (!present_buffer_index.has_value()) {
                return true;
            }
            auto &present_buffer = present_buffers_[present_buffer_index.value()];
            const auto convert_start_ms = get_current_time_ms();
            convert_frame_to(
                frame_vidbuf, present_buffer.data.data(),
                static_cast<size_t>(render_width_) * RGB565_BYTES_PER_PIXEL
            );
            if (stats != nullptr) {
                stats->convert_ms = static_cast<uint32_t>(get_current_time_ms() - convert_start_ms);
            }
            const auto present_start_ms = get_current_time_ms();
            service::RawBuffer data(present_buffer.data.data(), present_buffer.data.size());
            auto weak_runtime = weak_from_this();
            auto on_complete = [weak_runtime, buffer_index = present_buffer_index.value()](
                                   uint32_t, service::display::PresentResult complete_result
            ) {
                if (auto runtime = weak_runtime.lock()) {
                    runtime->release_present_buffer(buffer_index, complete_result);
                }
            };
            auto submit_result = service::Display::get_instance().present_frame_async(
                                     source_id, output_name, frame, data, std::move(on_complete),
                                     BROOKESIA_EMULATION_NES_DRAW_TIMEOUT_MS
                                 );
            switch (submit_result.state) {
            case service::display::PresentSubmitState::Queued:
                result = service::display::PresentResult::Presented;
                break;
            case service::display::PresentSubmitState::DroppedNotActive:
                release_present_buffer(present_buffer_index.value(), service::display::PresentResult::DroppedNotActive);
                result = service::display::PresentResult::DroppedNotActive;
                break;
            case service::display::PresentSubmitState::DroppedInvalidFrame:
                release_present_buffer(present_buffer_index.value(), service::display::PresentResult::DroppedInvalidFrame);
                result = service::display::PresentResult::DroppedInvalidFrame;
                break;
            case service::display::PresentSubmitState::Error:
            default:
                release_present_buffer(present_buffer_index.value(), service::display::PresentResult::Error);
                result = service::display::PresentResult::Error;
                break;
            }
            if (stats != nullptr) {
                stats->present_ms = static_cast<uint32_t>(get_current_time_ms() - present_start_ms);
            }
        }
        if ((result != service::display::PresentResult::Presented) &&
                (result != service::display::PresentResult::DroppedNotActive)) {
            BROOKESIA_LOGW("Failed to present NES frame: %1%", BROOKESIA_DESCRIBE_ENUM_TO_STR(result));
        }
        return true;
    }

    bool flush_audio_queue(Nes::AudioStepStats *stats)
    {
        if (stats != nullptr) {
            *stats = {};
        }

        const auto flush_start_ms = get_current_time_ms();
        for (uint32_t fed_chunks = 0; fed_chunks < BROOKESIA_EMULATION_NES_AUDIO_FEED_MAX_CHUNKS_PER_TICK;
                ++fed_chunks) {
            size_t feed_size = 0;
            {
                std::lock_guard lock(audio_queue_mutex_);
                if (!audio_started_ || (audio_queue_count_ == 0)) {
                    if (stats != nullptr) {
                        stats->elapsed_ms = static_cast<uint32_t>(get_current_time_ms() - flush_start_ms);
                    }
                    return true;
                }
                auto &chunk = audio_queue_[audio_queue_head_];
                audio_feed_buffer_.swap(chunk.data);
                feed_size = chunk.size;
                chunk.size = 0;
                audio_queue_head_ = (audio_queue_head_ + 1) % audio_queue_.size();
                audio_queue_count_--;
            }

            if (audio_decoder_ == nullptr) {
                if (stats != nullptr) {
                    stats->elapsed_ms = static_cast<uint32_t>(get_current_time_ms() - flush_start_ms);
                }
                return true;
            }

            struct WaitContext {
                std::mutex mutex;
                std::condition_variable cv;
                service::AudioWriteResult result = service::AudioWriteResult::Error;
                bool done = false;
            };
            WaitContext wait_context;
            auto release_callback = [&wait_context](service::AudioWriteResult result) {
                std::lock_guard lock(wait_context.mutex);
                wait_context.result = result;
                wait_context.done = true;
                wait_context.cv.notify_one();
            };
            service::RawBuffer data(audio_feed_buffer_.data(), feed_size);
            auto write_result = audio_decoder_->write_stream_borrowed(
                                    audio_source_id_, audio_output_name_, data, std::move(release_callback), 0
                                );
            if (write_result == service::AudioWriteResult::Written) {
                std::unique_lock lock(wait_context.mutex);
                wait_context.cv.wait(lock, [&wait_context]() {
                    return wait_context.done;
                });
                write_result = wait_context.result;
                if (write_result == service::AudioWriteResult::Written) {
                    if (stats != nullptr) {
                        stats->fed_chunks++;
                    }
                }
            }
            if (write_result == service::AudioWriteResult::DroppedQueueFull) {
                std::lock_guard lock(audio_queue_mutex_);
                audio_drop_count_++;
                if (!audio_queue_drop_logged_) {
                    audio_queue_drop_logged_ = true;
                    BROOKESIA_LOGW("NES audio stream queue is full; dropping audio chunks");
                }
            } else if (write_result != service::AudioWriteResult::DroppedNotActive) {
                bool should_log_error = false;
                {
                    std::lock_guard lock(audio_queue_mutex_);
                    if (!audio_error_logged_) {
                        audio_error_logged_ = true;
                        should_log_error = true;
                    }
                }
                if (should_log_error) {
                    BROOKESIA_LOGW("Failed to write NES audio stream data: %1%", BROOKESIA_DESCRIBE_TO_STR(write_result));
                }
            }

            if constexpr (BROOKESIA_EMULATION_NES_AUDIO_FEED_BUDGET_MS > 0) {
                if ((get_current_time_ms() - flush_start_ms) >= BROOKESIA_EMULATION_NES_AUDIO_FEED_BUDGET_MS) {
                    break;
                }
            }
        }
        if (stats != nullptr) {
            stats->elapsed_ms = static_cast<uint32_t>(get_current_time_ms() - flush_start_ms);
        }
        return true;
    }

private:
    struct AudioChunk {
        HeapBuffer data;
        size_t size = 0;
    };
    struct PresentBuffer {
        HeapBuffer data;
        bool busy = false;
    };

    void unload_locked()
    {
        if (!wait_for_present_buffers_idle()) {
            BROOKESIA_LOGW("Timed out waiting for NES Display frames to complete");
        }
        stop_audio();
        shutdown_emulator();
        release_display();
        clear_present_buffers();
        for (auto &vidbuf : nes_vidbufs_) {
            vidbuf.clear();
        }
        nes_vidbuf_index_ = 0;
        x_src_map_.clear();
        row_src_offset_map_.clear();
        rom_buffer_.reset();
        rom_size_ = 0;
        palette_.clear();
        source_id_ = 0;
    }

    std::expected<void, std::string> setup_display()
    {
        auto &display = service::Display::get_instance();
        auto outputs = display.get_outputs();
        if (outputs.empty()) {
            return std::unexpected("No Display output is available");
        }

        auto output_it = outputs.begin();
        if (!config_.display_output_name.empty()) {
            output_it = std::find_if(outputs.begin(), outputs.end(), [this](const auto & output) {
                return output.name == config_.display_output_name;
            });
            if (output_it == outputs.end()) {
                return std::unexpected("Display output is not found: " + config_.display_output_name);
            }
        }

        if (output_it->pixel_format != service::display::PixelFormat::RGB565) {
            return std::unexpected("NES v1 requires RGB565 Display output");
        }

        output_name_ = output_it->name;
        output_width_ = output_it->width;
        output_height_ = output_it->height;
        output_is_buffer_ = output_it->slot == service::display::OutputSlot::Buffer;
        auto area_result = resolve_render_area();
        if (!area_result) {
            return area_result;
        }

        service::display::SourceInfo source{
            .name = config_.display_source_name.empty() ? std::string("NES") : config_.display_source_name,
            .role = std::string(DEFAULT_DISPLAY_SOURCE_ROLE),
            .preferred_outputs = {output_name_},
            .priority = 0,
        };
        auto register_result = display.register_source(source);
        if (!register_result) {
            return std::unexpected("Failed to register NES Display source: " + register_result.error());
        }
        source_id_ = register_result.value();

        auto request_result = display.request_output(source_id_, output_name_);
        if (!request_result) {
            auto unregister_result = display.unregister_source(source_id_);
            if (!unregister_result) {
                BROOKESIA_LOGW(
                    "Failed to unregister NES Display source after output request failure: %1%",
                    unregister_result.error()
                );
            }
            source_id_ = 0;
            return std::unexpected("Failed to request NES Display output: " + request_result.error());
        }

        if (config_.auto_activate_display) {
            auto active_result = display.set_active_source(output_name_, source.name);
            if (!active_result) {
                BROOKESIA_LOGW("Failed to activate NES Display source: %1%", active_result.error());
            }
        }

        if (!output_is_buffer_) {
            auto present_result = setup_present_buffers(render_width_ * render_height_ * RGB565_BYTES_PER_PIXEL);
            if (!present_result) {
                auto release_result = display.release_output(source_id_, output_name_);
                if (!release_result) {
                    BROOKESIA_LOGW(
                        "Failed to release NES Display output after present buffer setup failure: %1%",
                        release_result.error()
                    );
                }
                auto unregister_result = display.unregister_source(source_id_);
                if (!unregister_result) {
                    BROOKESIA_LOGW(
                        "Failed to unregister NES Display source after present buffer setup failure: %1%",
                        unregister_result.error()
                    );
                }
                source_id_ = 0;
                return present_result;
            }
        }
        return {};
    }

    std::expected<void, std::string> setup_emulator()
    {
        const auto load_start_ms = get_current_time_ms();
        auto rom_result = load_file_to_memory(config_.rom_path);
        if (!rom_result) {
            return std::unexpected(rom_result.error());
        }
        auto rom_image = std::move(rom_result.value());
        rom_buffer_ = std::move(rom_image.first);
        rom_size_ = rom_image.second;
        BROOKESIA_LOGI(
            "Preloaded NES ROM into memory: path=%1%, size=%2% bytes, elapsed=%3% ms", config_.rom_path,
            rom_size_, get_current_time_ms() - load_start_ms
        );

        nes_ = nes_init(
                   SYS_DETECT, BROOKESIA_EMULATION_NES_AUDIO_SAMPLE_RATE,
                   BROOKESIA_EMULATION_NES_AUDIO_STEREO != 0, nullptr
               );
        if (nes_ == nullptr) {
            return std::unexpected("Failed to initialize nofrendo");
        }

        const int load_result = nes_loadmem(rom_buffer_.get(), rom_size_, config_.rom_path.c_str());
        if (load_result < 0) {
            return std::unexpected(map_rom_load_error(load_result) + ": " + config_.rom_path);
        }

        input_connect(0, NES_JOYPAD);
        input_connect(1, NES_JOYPAD);
        ppu_setopt(PPU_LIMIT_SPRITES, 1);

        std::unique_ptr<uint8_t, FreeDeleter> built_palette(
            static_cast<uint8_t *>(nofrendo_buildpalette(NES_PALETTE_PVM, 16))
        );
        if (!built_palette) {
            return std::unexpected("Failed to build NES RGB565 palette");
        }
        constexpr size_t palette_bytes = 256 * sizeof(uint16_t);
        auto palette_result = palette_.resize(palette_bytes, "palette_rgb565");
        if (!palette_result) {
            return palette_result;
        }
        std::memcpy(palette_.data(), built_palette.get(), palette_bytes);

        for (size_t i = 0; i < nes_vidbufs_.size(); ++i) {
            auto label = "nes_vidbuf[" + std::to_string(i) + "]";
            auto resize_result = nes_vidbufs_[i].resize(NES_SCREEN_PITCH * NES_NATIVE_HEIGHT, label);
            if (!resize_result) {
                return resize_result;
            }
        }
        nes_vidbuf_index_ = 0;
        nes_setvidbuf(nes_vidbufs_[nes_vidbuf_index_].data());

        // Nofrendo needs a couple of dry frames after ROM load to settle mapper/PPU state, matching retro-go.
        nes_emulate(false);
        nes_emulate(false);
        return {};
    }

    void shutdown_emulator()
    {
        if (nes_ == nullptr) {
            return;
        }
        nes_shutdown();
        nes_ = nullptr;
    }

    std::expected<void, std::string> setup_audio()
    {
        if (config_.audio_mode == AudioMode::Disabled) {
            return {};
        }
        if (!AudioDecoderHelper::is_available()) {
            if (config_.audio_mode == AudioMode::Required) {
                return std::unexpected("AudioDecoder0 service is not available");
            }
            BROOKESIA_LOGW("AudioDecoder0 is not available; NES continues muted");
            return {};
        }

        auto binding = service::ServiceManager::get_instance().bind(AudioDecoderHelper::get_name().data());
        if (!binding.is_valid()) {
            if (config_.audio_mode == AudioMode::Required) {
                return std::unexpected("Failed to bind AudioDecoder0 service");
            }
            BROOKESIA_LOGW("Failed to bind AudioDecoder0; NES continues muted");
            return {};
        }
        auto *decoder = service::AudioDecoder::get_instance(0);
        if (decoder == nullptr) {
            if (config_.audio_mode == AudioMode::Required) {
                return std::unexpected("AudioDecoder0 direct instance is not available");
            }
            BROOKESIA_LOGW("AudioDecoder0 direct instance is not available; NES continues muted");
            return {};
        }

        const uint8_t channels = (nes_ != nullptr && nes_->apu != nullptr && nes_->apu->stereo) ? 2 : 1;
        service::AudioStreamConfig stream_config{
            .type = AudioHelper::CodecFormat::PCM,
            .general = AudioHelper::CodecGeneralConfig{
                .channels = channels,
                .sample_bits = NES_AUDIO_SAMPLE_BITS,
                .sample_rate = BROOKESIA_EMULATION_NES_AUDIO_SAMPLE_RATE,
                .frame_duration = NES_AUDIO_FRAME_DURATION_MS,
            },
        };

        audio_decoder_binding_ = std::move(binding);
        audio_decoder_ = decoder;
        lib_utils::FunctionGuard stop_decoder_guard([this]() {
            if (audio_decoder_ != nullptr) {
                auto close_result = audio_decoder_->close_stream(NES_AUDIO_SOURCE_NAME, NES_AUDIO_OUTPUT_NAME);
                if (!close_result) {
                    BROOKESIA_LOGD(
                        "Failed to close partially configured NES audio stream: %1%", close_result.error()
                    );
                }
                auto release_result = audio_decoder_->release_output(NES_AUDIO_SOURCE_NAME, NES_AUDIO_OUTPUT_NAME);
                if (!release_result) {
                    BROOKESIA_LOGD(
                        "Failed to release partially configured NES audio output: %1%", release_result.error()
                    );
                }
                auto unregister_result = audio_decoder_->unregister_source(NES_AUDIO_SOURCE_NAME);
                if (!unregister_result) {
                    BROOKESIA_LOGD(
                        "Failed to unregister partially configured NES audio source: %1%", unregister_result.error()
                    );
                }
                audio_decoder_ = nullptr;
            }
            if (audio_decoder_binding_.is_valid()) {
                audio_decoder_binding_.release();
            }
        });
        auto fail_audio_setup = [this](const std::string & message) -> std::expected<void, std::string> {
            if (config_.audio_mode == AudioMode::Required)
            {
                return std::unexpected(message);
            }
            BROOKESIA_LOGW("%1%; NES continues muted", message);
            return {};
        };

        auto unregister_result = audio_decoder_->unregister_source(NES_AUDIO_SOURCE_NAME);
        if (!unregister_result) {
            BROOKESIA_LOGD("NES audio source was not registered before setup: %1%", unregister_result.error());
        }
        service::AudioSourceInfo source_info = {
            .name = NES_AUDIO_SOURCE_NAME,
            .role = NES_AUDIO_SOURCE_ROLE,
            .preferred_outputs = {NES_AUDIO_OUTPUT_NAME},
            .priority = 0,
        };
        auto register_result = audio_decoder_->register_source(std::move(source_info));
        if (!register_result) {
            return fail_audio_setup("Failed to register NES audio source: " + register_result.error());
        }
        audio_source_id_ = register_result.value();
        audio_output_name_ = NES_AUDIO_OUTPUT_NAME;

        auto request_result = audio_decoder_->request_output(audio_source_id_, audio_output_name_);
        if (!request_result) {
            return fail_audio_setup("Failed to request NES audio output: " + request_result.error());
        }
        auto active_result = audio_decoder_->set_active_source(audio_output_name_, NES_AUDIO_SOURCE_NAME);
        if (!active_result) {
            return fail_audio_setup("Failed to set NES audio source active: " + active_result.error());
        }
        auto open_result = audio_decoder_->open_stream(audio_source_id_, audio_output_name_, stream_config);
        if (!open_result) {
            return fail_audio_setup("Failed to open NES audio stream: " + open_result.error());
        }

        {
            std::lock_guard lock(audio_queue_mutex_);
            audio_chunk_bytes_ = get_audio_frame_byte_count();
            audio_queue_.clear();
            audio_queue_.resize(BROOKESIA_EMULATION_NES_AUDIO_QUEUE_MAX_FRAMES);
            for (size_t i = 0; i < audio_queue_.size(); ++i) {
                auto label = "audio_chunk[" + std::to_string(i) + "]";
                auto resize_result = audio_queue_[i].data.resize(audio_chunk_bytes_, label);
                if (!resize_result) {
                    audio_queue_.clear();
                    audio_chunk_bytes_ = 0;
                    audio_started_ = false;
                    return fail_audio_setup("Failed to allocate NES audio queue: " + resize_result.error());
                }
                auto &chunk = audio_queue_[i];
                chunk.size = 0;
            }
            auto feed_resize_result = audio_feed_buffer_.resize(audio_chunk_bytes_, "audio_feed");
            if (!feed_resize_result) {
                audio_queue_.clear();
                audio_chunk_bytes_ = 0;
                audio_started_ = false;
                return fail_audio_setup("Failed to allocate NES audio feed buffer: " + feed_resize_result.error());
            }
            audio_queue_head_ = 0;
            audio_queue_count_ = 0;
            audio_started_ = true;
            audio_error_logged_ = false;
            audio_queue_drop_logged_ = false;
            audio_drop_count_ = 0;
        }
        BROOKESIA_LOGI("NES audio started: %1% Hz, %2% channels", BROOKESIA_EMULATION_NES_AUDIO_SAMPLE_RATE, channels);
        stop_decoder_guard.release();
        return {};
    }

    void stop_audio()
    {
        {
            std::lock_guard lock(audio_queue_mutex_);
            audio_started_ = false;
            audio_queue_.clear();
            audio_feed_buffer_.clear();
            audio_queue_head_ = 0;
            audio_queue_count_ = 0;
            audio_chunk_bytes_ = 0;
            audio_error_logged_ = false;
            audio_queue_drop_logged_ = false;
        }
        if (audio_decoder_ != nullptr) {
            auto close_result = audio_decoder_->close_stream(NES_AUDIO_SOURCE_NAME, NES_AUDIO_OUTPUT_NAME);
            if (!close_result) {
                BROOKESIA_LOGW("Failed to close NES audio stream: %1%", close_result.error());
            }
            auto release_result = audio_decoder_->release_output(NES_AUDIO_SOURCE_NAME, NES_AUDIO_OUTPUT_NAME);
            if (!release_result) {
                BROOKESIA_LOGW("Failed to release NES audio output: %1%", release_result.error());
            }
            auto unregister_result = audio_decoder_->unregister_source(NES_AUDIO_SOURCE_NAME);
            if (!unregister_result) {
                BROOKESIA_LOGW("Failed to unregister NES audio source: %1%", unregister_result.error());
            }
        }
        audio_decoder_ = nullptr;
        audio_source_id_ = 0;
        audio_output_name_.clear();
        if (audio_decoder_binding_.is_valid()) {
            audio_decoder_binding_.release();
        }
    }

    size_t get_audio_frame_byte_count() const
    {
        if ((nes_ == nullptr) || (nes_->apu == nullptr)) {
            return 0;
        }
        const size_t channels = nes_->apu->stereo ? 2 : 1;
        return static_cast<size_t>(nes_->apu->samples_per_frame) * channels * sizeof(int16_t);
    }

    void enqueue_audio_frame()
    {
        if ((nes_ == nullptr) || (nes_->apu == nullptr) || (nes_->apu->buffer == nullptr)) {
            return;
        }
        const size_t byte_count = get_audio_frame_byte_count();
        if (byte_count == 0) {
            return;
        }

        std::lock_guard lock(audio_queue_mutex_);
        if (!audio_started_ || audio_queue_.empty()) {
            return;
        }
        if (byte_count > audio_chunk_bytes_) {
            return;
        }
        while (audio_queue_count_ >= audio_queue_.size()) {
            audio_queue_head_ = (audio_queue_head_ + 1) % audio_queue_.size();
            audio_queue_count_--;
            audio_drop_count_++;
            if (!audio_queue_drop_logged_) {
                BROOKESIA_LOGW("NES audio queue is full; dropping stale audio to keep video smooth");
                audio_queue_drop_logged_ = true;
            }
        }
        const size_t write_index = (audio_queue_head_ + audio_queue_count_) % audio_queue_.size();
        auto &chunk = audio_queue_[write_index];
        if (chunk.data.size() < byte_count) {
            auto resize_result = chunk.data.resize(byte_count, "audio_chunk_grow");
            if (!resize_result) {
                audio_drop_count_++;
                if (!audio_queue_drop_logged_) {
                    BROOKESIA_LOGW("Failed to grow NES audio queue chunk: %1%", resize_result.error());
                    audio_queue_drop_logged_ = true;
                }
                return;
            }
        }
        std::memcpy(chunk.data.data(), nes_->apu->buffer, byte_count);
        chunk.size = byte_count;
        audio_queue_count_++;
    }

    void release_display()
    {
        if (source_id_ == 0) {
            return;
        }
        auto &display = service::Display::get_instance();
        auto release_result = display.release_output(source_id_, output_name_);
        if (!release_result) {
            BROOKESIA_LOGW("Failed to release NES Display output: %1%", release_result.error());
        }
        auto unregister_result = display.unregister_source(source_id_);
        if (!unregister_result) {
            BROOKESIA_LOGW("Failed to unregister NES Display source: %1%", unregister_result.error());
        }
        source_id_ = 0;
        output_name_.clear();
        output_is_buffer_ = false;
    }

    std::expected<void, std::string> setup_present_buffers(size_t buffer_size)
    {
        if (buffer_size == 0) {
            return std::unexpected("NES present buffer size is zero");
        }

        for (size_t i = 0; i < present_buffers_.size(); ++i) {
            auto label = "present_buffer[" + std::to_string(i) + "]";
            auto resize_result = present_buffers_[i].data.resize(buffer_size, label);
            if (!resize_result) {
                return resize_result;
            }
            present_buffers_[i].busy = false;
        }
        present_error_logged_ = false;
        display_backpressure_pending_ = false;
        display_backpressure_count_ = 0;
        present_drop_count_ = 0;
        present_error_count_ = 0;
        return {};
    }

    void clear_present_buffers()
    {
        std::lock_guard lock(present_mutex_);
        for (auto &buffer : present_buffers_) {
            buffer.data.clear();
            buffer.busy = false;
        }
        display_backpressure_pending_ = false;
        display_backpressure_count_ = 0;
        present_drop_count_ = 0;
        present_error_count_ = 0;
        present_cv_.notify_all();
    }

    std::optional<size_t> reserve_present_buffer(size_t buffer_size)
    {
        std::lock_guard lock(present_mutex_);
        for (size_t i = 0; i < present_buffers_.size(); ++i) {
            auto &buffer = present_buffers_[i];
            if (buffer.busy) {
                continue;
            }
            if (buffer.data.size() != buffer_size) {
                auto label = "present_buffer[" + std::to_string(i) + "]";
                auto resize_result = buffer.data.resize(buffer_size, label);
                if (!resize_result) {
                    BROOKESIA_LOGW("Failed to resize NES present buffer: %1%", resize_result.error());
                    mark_display_backpressure_locked();
                    return std::nullopt;
                }
            }
            buffer.busy = true;
            return i;
        }
        mark_display_backpressure_locked();
        return std::nullopt;
    }

    void release_present_buffer(size_t buffer_index, service::display::PresentResult result)
    {
        if (buffer_index >= present_buffers_.size()) {
            return;
        }

        bool should_log_error = false;
        {
            std::lock_guard lock(present_mutex_);
            present_buffers_[buffer_index].busy = false;
            if (result == service::display::PresentResult::DroppedQueueFull) {
                present_drop_count_++;
                mark_display_backpressure_locked();
            } else if (result == service::display::PresentResult::Error) {
                present_error_count_++;
                mark_display_backpressure_locked();
            }
            should_log_error = (result == service::display::PresentResult::Error) && !present_error_logged_;
            if (should_log_error) {
                present_error_logged_ = true;
            }
        }
        present_cv_.notify_all();
        if (should_log_error) {
            BROOKESIA_LOGW("NES Display async present failed; future errors will be suppressed");
        }
    }

    bool wait_for_present_buffers_idle()
    {
        std::unique_lock lock(present_mutex_);
        const auto timeout = std::chrono::milliseconds(
                                 static_cast<int>(BROOKESIA_EMULATION_NES_DRAW_TIMEOUT_MS) +
                                 NES_PRESENT_DRAIN_EXTRA_TIMEOUT_MS
                             );
        return present_cv_.wait_for(lock, timeout, [this]() {
            return std::none_of(present_buffers_.begin(), present_buffers_.end(), [](const auto & buffer) {
                return buffer.busy;
            });
        });
    }

    void mark_display_backpressure_locked()
    {
        display_backpressure_pending_ = true;
        display_backpressure_count_++;
    }

    std::expected<void, std::string> resolve_render_area()
    {
        const auto &area = config_.video_area;
        if ((area.x >= output_width_) || (area.y >= output_height_)) {
            return std::unexpected("NES video area origin is outside Display output");
        }

        const uint32_t max_width = output_width_ - area.x;
        const uint32_t max_height = output_height_ - area.y;
        const uint32_t viewport_width = (area.width == 0) ? max_width : area.width;
        const uint32_t viewport_height = (area.height == 0) ? max_height : area.height;
        if ((viewport_width == 0) || (viewport_height == 0) ||
                (viewport_width > max_width) || (viewport_height > max_height)) {
            return std::unexpected("NES video area is outside Display output");
        }

        uint32_t target_width = NES_NATIVE_WIDTH;
        uint32_t target_height = NES_VISIBLE_HEIGHT;
        if (config_.video_mode == VideoMode::Native) {
            if ((target_width > viewport_width) || (target_height > viewport_height)) {
                return std::unexpected("NES native frame is larger than configured video area");
            }
        } else {
            const float scale_x = static_cast<float>(viewport_width) / static_cast<float>(NES_NATIVE_WIDTH);
            const float scale_y = static_cast<float>(viewport_height) / static_cast<float>(NES_VISIBLE_HEIGHT);
            const float scale = (config_.video_mode == VideoMode::Fill) ? std::max(scale_x, scale_y) :
                                std::min(scale_x, scale_y);
            target_width = std::max<uint32_t>(1, static_cast<uint32_t>(NES_NATIVE_WIDTH * scale));
            target_height = std::max<uint32_t>(1, static_cast<uint32_t>(NES_VISIBLE_HEIGHT * scale));
            target_width = std::min(target_width, viewport_width);
            target_height = std::min(target_height, viewport_height);
        }
        render_width_ = target_width;
        render_height_ = target_height;
        output_x_ = area.x + ((viewport_width > render_width_) ? ((viewport_width - render_width_) / 2) : 0);
        output_y_ = area.y + ((viewport_height > render_height_) ? ((viewport_height - render_height_) / 2) : 0);
        auto x_map_result = x_src_map_.resize(static_cast<size_t>(render_width_) * sizeof(uint16_t), "x_src_map");
        if (!x_map_result) {
            return x_map_result;
        }
        auto row_map_result = row_src_offset_map_.resize(
                                  static_cast<size_t>(render_height_) * sizeof(uint16_t), "row_src_offset_map"
                              );
        if (!row_map_result) {
            return row_map_result;
        }
        auto *x_src_map = reinterpret_cast<uint16_t *>(x_src_map_.data());
        auto *row_src_offset_map = reinterpret_cast<uint16_t *>(row_src_offset_map_.data());
        for (uint32_t x = 0; x < render_width_; x++) {
            x_src_map[x] = static_cast<uint16_t>((x * NES_NATIVE_WIDTH) / render_width_);
        }
        for (uint32_t y = 0; y < render_height_; y++) {
            const uint32_t src_y = NES_VISIBLE_Y_OFFSET + ((y * NES_VISIBLE_HEIGHT) / render_height_);
            row_src_offset_map[y] = static_cast<uint16_t>((src_y * NES_SCREEN_PITCH) + NES_SCREEN_OVERDRAW);
        }
        BROOKESIA_LOGI(
            "NES render area: output=%1% %2%x%3%, viewport=(%4%,%5%) %6%x%7%, rect=(%8%,%9%) %10%x%11%",
            output_name_, output_width_, output_height_, area.x, area.y, viewport_width, viewport_height, output_x_,
            output_y_, render_width_, render_height_
        );
        return {};
    }

    void convert_frame_to(const uint8_t *src_vidbuf, uint8_t *dst_data, size_t dst_stride_bytes)
    {
        if ((src_vidbuf == nullptr) || (dst_data == nullptr) || (dst_stride_bytes == 0)) {
            return;
        }
        const auto *palette = reinterpret_cast<const uint16_t *>(palette_.data());
        if (palette == nullptr) {
            return;
        }
        if ((render_width_ == NES_NATIVE_WIDTH) && (render_height_ == NES_VISIBLE_HEIGHT)) {
            for (uint32_t y = 0; y < NES_VISIBLE_HEIGHT; ++y) {
                const auto *src_row = src_vidbuf + ((NES_VISIBLE_Y_OFFSET + y) * NES_SCREEN_PITCH) +
                                      NES_SCREEN_OVERDRAW;
                auto *dst_row = reinterpret_cast<uint16_t *>(dst_data + static_cast<size_t>(y) * dst_stride_bytes);
                uint32_t x = 0;
                for (; x + 7 < NES_NATIVE_WIDTH; x += 8) {
                    dst_row[x] = palette[src_row[x]];
                    dst_row[x + 1] = palette[src_row[x + 1]];
                    dst_row[x + 2] = palette[src_row[x + 2]];
                    dst_row[x + 3] = palette[src_row[x + 3]];
                    dst_row[x + 4] = palette[src_row[x + 4]];
                    dst_row[x + 5] = palette[src_row[x + 5]];
                    dst_row[x + 6] = palette[src_row[x + 6]];
                    dst_row[x + 7] = palette[src_row[x + 7]];
                }
                for (; x < NES_NATIVE_WIDTH; ++x) {
                    dst_row[x] = palette[src_row[x]];
                }
            }
            return;
        }

        const auto *x_src_map = reinterpret_cast<const uint16_t *>(x_src_map_.data());
        const auto *row_src_offset_map = reinterpret_cast<const uint16_t *>(row_src_offset_map_.data());
        if ((x_src_map == nullptr) || (row_src_offset_map == nullptr)) {
            return;
        }
        const size_t row_bytes = static_cast<size_t>(render_width_) * RGB565_BYTES_PER_PIXEL;
        uint32_t previous_src_offset = std::numeric_limits<uint32_t>::max();
        uint16_t *previous_dst_row = nullptr;
        for (uint32_t y = 0; y < render_height_; ++y) {
            const uint32_t src_offset = row_src_offset_map[y];
            auto *dst_row = reinterpret_cast<uint16_t *>(dst_data + static_cast<size_t>(y) * dst_stride_bytes);
            if ((src_offset == previous_src_offset) && (previous_dst_row != nullptr)) {
                std::memcpy(dst_row, previous_dst_row, row_bytes);
                continue;
            }

            const auto *src_row = src_vidbuf + src_offset;
            uint32_t x = 0;
            for (; x + 7 < render_width_; x += 8) {
                dst_row[x] = palette[src_row[x_src_map[x]]];
                dst_row[x + 1] = palette[src_row[x_src_map[x + 1]]];
                dst_row[x + 2] = palette[src_row[x_src_map[x + 2]]];
                dst_row[x + 3] = palette[src_row[x_src_map[x + 3]]];
                dst_row[x + 4] = palette[src_row[x_src_map[x + 4]]];
                dst_row[x + 5] = palette[src_row[x_src_map[x + 5]]];
                dst_row[x + 6] = palette[src_row[x_src_map[x + 6]]];
                dst_row[x + 7] = palette[src_row[x_src_map[x + 7]]];
            }
            for (; x < render_width_; ++x) {
                dst_row[x] = palette[src_row[x_src_map[x]]];
            }
            previous_src_offset = src_offset;
            previous_dst_row = dst_row;
        }
    }

    using AudioHelper = service::helper::Audio;
    using AudioDecoderHelper = service::helper::AudioDecoder<0>;

    std::mutex runtime_mutex_;
    Config config_;
    uint32_t source_id_ = 0;
    std::string output_name_;
    uint32_t output_width_ = 0;
    uint32_t output_height_ = 0;
    bool output_is_buffer_ = false;
    uint32_t output_x_ = 0;
    uint32_t output_y_ = 0;
    uint32_t render_width_ = NES_NATIVE_WIDTH;
    uint32_t render_height_ = NES_VISIBLE_HEIGHT;
    std::array<PresentBuffer, NES_PRESENT_BUFFER_COUNT> present_buffers_;
    std::mutex present_mutex_;
    std::condition_variable present_cv_;
    bool present_error_logged_ = false;
    bool display_backpressure_pending_ = false;
    uint32_t display_backpressure_count_ = 0;
    uint32_t present_drop_count_ = 0;
    uint32_t present_error_count_ = 0;
    std::array<HeapBuffer, NES_VIDEO_BUFFER_COUNT> nes_vidbufs_;
    size_t nes_vidbuf_index_ = 0;
    HeapBuffer x_src_map_;
    HeapBuffer row_src_offset_map_;
    OwnedBuffer rom_buffer_;
    size_t rom_size_ = 0;
    HeapBuffer palette_;
    nes_t *nes_ = nullptr;
    std::mutex emulator_mutex_;
    service::ServiceBinding audio_decoder_binding_;
    service::AudioDecoder *audio_decoder_ = nullptr;
    uint32_t audio_source_id_ = 0;
    std::string audio_output_name_;
    bool audio_started_ = false;
    bool audio_error_logged_ = false;
    bool audio_queue_drop_logged_ = false;
    mutable std::mutex audio_queue_mutex_;
    std::vector<AudioChunk> audio_queue_;
    HeapBuffer audio_feed_buffer_;
    size_t audio_queue_head_ = 0;
    size_t audio_queue_count_ = 0;
    size_t audio_chunk_bytes_ = 0;
    uint32_t audio_drop_count_ = 0;
    std::mutex gamepad_mutex_;
    GamepadState gamepad_state_;
};

Nes::Nes()
    : service::ServiceBase({
    .name = Helper::get_name().data(),
#if BROOKESIA_EMULATION_NES_ENABLE_WORKER
    .task_scheduler_config = lib_utils::TaskScheduler::StartConfig{
        .worker_configs = {
            lib_utils::ThreadConfig{
                .name = BROOKESIA_EMULATION_NES_WORKER_NAME "0",
                .core_id = BROOKESIA_EMULATION_NES_WORKER_0_CORE_ID,
                .priority = BROOKESIA_EMULATION_NES_WORKER_0_PRIORITY,
                .stack_size = BROOKESIA_EMULATION_NES_WORKER_0_STACK_SIZE,
                .stack_in_ext = false,
            },
            lib_utils::ThreadConfig{
                .name = BROOKESIA_EMULATION_NES_WORKER_NAME "1",
                .core_id = BROOKESIA_EMULATION_NES_WORKER_1_CORE_ID,
                .priority = BROOKESIA_EMULATION_NES_WORKER_1_PRIORITY,
                .stack_size = BROOKESIA_EMULATION_NES_WORKER_1_STACK_SIZE,
                .stack_in_ext = false,
            },
        },
        .worker_poll_interval_ms = BROOKESIA_EMULATION_NES_WORKER_POLL_INTERVAL_MS,
    },
#endif
})
{
}

Nes::~Nes() = default;

bool Nes::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_EMULATION_NES_VER_MAJOR, BROOKESIA_EMULATION_NES_VER_MINOR,
        BROOKESIA_EMULATION_NES_VER_PATCH
    );

    runtime_ = std::make_shared<Runtime>();
    return true;
}

bool Nes::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    auto scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Task scheduler is not available");
    BROOKESIA_CHECK_FALSE_RETURN(scheduler->is_running(), false, "Task scheduler is not running");
    BROOKESIA_CHECK_FALSE_RETURN(
    scheduler->configure_group(NES_FRAME_TASK_GROUP, {
        .enable_serial_execution = true,
    }), false, "Failed to configure NES frame task group"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
    scheduler->configure_group(NES_AUDIO_TASK_GROUP, {
        .enable_serial_execution = true,
    }), false, "Failed to configure NES audio task group"
    );
    return true;
}

void Nes::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::vector<lib_utils::TaskScheduler::TaskId> task_ids;
    {
        std::lock_guard lock(mutex_);
        take_task_ids_locked(task_ids);
        if ((state_ == State::Running) || (state_ == State::Paused)) {
            set_state(State::Stopped);
        }
    }
    cancel_and_wait_task_ids(task_ids);
}

void Nes::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::vector<lib_utils::TaskScheduler::TaskId> task_ids;
    {
        std::lock_guard lock(mutex_);
        take_task_ids_locked(task_ids);
        if ((state_ == State::Running) || (state_ == State::Paused)) {
            set_state(State::Stopped);
        }
    }
    cancel_and_wait_task_ids(task_ids);

    {
        std::lock_guard lock(mutex_);
        if (runtime_) {
            runtime_->unload();
        }
        runtime_.reset();
        config_ = {};
        state_ = State::Idle;
    }
    BROOKESIA_LOGI("Deinitialized");
}

std::expected<void, std::string> Nes::load(Config config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: config(%1%)", config);

    std::vector<lib_utils::TaskScheduler::TaskId> task_ids;
    {
        std::lock_guard lock(mutex_);
        if (!runtime_) {
            return std::unexpected("NES runtime is not initialized");
        }
        take_task_ids_locked(task_ids);
        if ((state_ == State::Running) || (state_ == State::Paused)) {
            set_state(State::Stopped);
        }
    }

    cancel_and_wait_task_ids(task_ids);

    std::lock_guard lock(mutex_);
    runtime_->unload();
    auto result = runtime_->load(config);
    if (!result) {
        set_state(State::Error);
        publish_error(result.error());
        return result;
    }
    config_ = std::move(config);
    set_state(State::Loaded);
    return {};
}

std::expected<void, std::string> Nes::start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::vector<lib_utils::TaskScheduler::TaskId> task_ids;
    bool failed_to_start_frame = false;
    {
        std::lock_guard lock(mutex_);
        if ((state_ != State::Loaded) && (state_ != State::Paused) && (state_ != State::Stopped)) {
            return std::unexpected("NES runtime is not loaded");
        }
        set_state(State::Running);
        if (runtime_ && runtime_->is_audio_started() && !start_audio_task_locked()) {
            BROOKESIA_LOGW("Failed to start NES audio task; continuing without decoupled audio feed");
        }
        if (!start_frame_task_locked()) {
            take_task_ids_locked(task_ids);
            set_state(State::Error);
            failed_to_start_frame = true;
        }
    }
    cancel_and_wait_task_ids(task_ids);
    if (failed_to_start_frame) {
        return std::unexpected("Failed to start NES frame task");
    }
    return {};
}

std::expected<void, std::string> Nes::pause()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::vector<lib_utils::TaskScheduler::TaskId> task_ids;
    {
        std::lock_guard lock(mutex_);
        if (state_ != State::Running) {
            return {};
        }
        take_task_ids_locked(task_ids);
        set_state(State::Paused);
    }
    cancel_and_wait_task_ids(task_ids);
    return {};
}

std::expected<void, std::string> Nes::resume()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return start();
}

std::expected<void, std::string> Nes::stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::vector<lib_utils::TaskScheduler::TaskId> task_ids;
    {
        std::lock_guard lock(mutex_);
        take_task_ids_locked(task_ids);
        if ((state_ == State::Running) || (state_ == State::Paused)) {
            set_state(State::Stopped);
        }
    }
    cancel_and_wait_task_ids(task_ids);

    std::lock_guard lock(mutex_);
    if (runtime_) {
        runtime_->unload();
    }
    config_ = {};
    set_state(State::Stopped);
    return {};
}

std::expected<void, std::string> Nes::reset()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(mutex_);
    if (!runtime_) {
        return std::unexpected("NES runtime is not initialized");
    }
    return runtime_->reset();
}

std::expected<void, std::string> Nes::save()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(mutex_);
    if (!runtime_) {
        return std::unexpected("NES runtime is not initialized");
    }
    auto result = runtime_->save();
    if (!result) {
        publish_error(result.error());
        return result;
    }
    if (!config_.save_path.empty()) {
        publish_event(
            BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::SaveCompleted),
        {service::EventItem(config_.save_path)}
        );
    }
    return {};
}

std::expected<void, std::string> Nes::set_gamepad_state(GamepadState state)
{
    std::lock_guard lock(mutex_);
    if (!runtime_) {
        return std::unexpected("NES runtime is not initialized");
    }
    return runtime_->set_gamepad_state(state);
}

Nes::State Nes::get_state() const
{
    std::lock_guard lock(mutex_);
    return state_;
}

std::expected<void, std::string> Nes::function_load(const boost::json::object &config_json)
{
    Config config;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config_json, config)) {
        return std::unexpected("Failed to parse NES config");
    }
    return load(std::move(config));
}

std::expected<void, std::string> Nes::function_start()
{
    return start();
}

std::expected<void, std::string> Nes::function_pause()
{
    return pause();
}

std::expected<void, std::string> Nes::function_resume()
{
    return resume();
}

std::expected<void, std::string> Nes::function_stop()
{
    return stop();
}

std::expected<void, std::string> Nes::function_reset()
{
    return reset();
}

std::expected<void, std::string> Nes::function_save()
{
    return save();
}

std::expected<void, std::string> Nes::function_set_gamepad_state(const boost::json::object &state_json)
{
    GamepadState state;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(state_json, state)) {
        return std::unexpected("Failed to parse NES gamepad state");
    }
    return set_gamepad_state(state);
}

std::expected<std::string, std::string> Nes::function_get_state()
{
    return to_string(get_state());
}

std::vector<service::FunctionSchema> Nes::get_function_schemas()
{
    auto schemas = Helper::get_function_schemas();
    return std::vector<service::FunctionSchema>(schemas.begin(), schemas.end());
}

std::vector<service::EventSchema> Nes::get_event_schemas()
{
    auto schemas = Helper::get_event_schemas();
    return std::vector<service::EventSchema>(schemas.begin(), schemas.end());
}

service::ServiceBase::FunctionHandlerMap Nes::get_function_handlers()
{
    return {
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
            Helper, Helper::FunctionId::Load, boost::json::object, function_load(PARAM)
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::Start, function_start()),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::Pause, function_pause()),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::Resume, function_resume()),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::Stop, function_stop()),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::Reset, function_reset()),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::Save, function_save()),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
            Helper, Helper::FunctionId::SetGamepadState, boost::json::object, function_set_gamepad_state(PARAM)
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(Helper, Helper::FunctionId::GetState, function_get_state()),
    };
}

void Nes::set_state(State state)
{
    if (state_ == state) {
        return;
    }
    state_ = state;
    publish_event(
        BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::StateChanged),
    {service::EventItem(to_string(state))}
    );
}

void Nes::publish_error(const std::string &message)
{
    publish_event(
        BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::Error),
    {service::EventItem(message)}
    );
}

bool Nes::start_frame_task_locked()
{
    if (frame_task_id_ != 0) {
        return true;
    }
    auto scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Task scheduler is not available");
    BROOKESIA_CHECK_FALSE_RETURN(scheduler->is_running(), false, "Task scheduler is not running");

    frame_late_count_ = 0;
    frame_count_since_log_ = 0;
    frame_draw_count_since_log_ = 0;
    frame_skip_count_since_log_ = 0;
    frame_max_step_ms_ = 0;
    frame_total_step_ms_ = 0;
    frame_draw_step_ms_ = 0;
    frame_max_emulate_ms_ = 0;
    frame_max_convert_ms_ = 0;
    frame_max_present_ms_ = 0;
    frame_max_late_by_ms_ = 0;
    audio_chunks_since_log_ = 0;
    audio_max_feed_ms_ = 0;
    consecutive_frame_skip_count_ = 0;
    skip_next_video_frame_ = false;
    base_frame_skip_ = std::min<uint32_t>(BROOKESIA_EMULATION_NES_BASE_FRAME_SKIP, get_auto_frame_skip_max());
    current_frame_skip_ = base_frame_skip_;
    frame_skip_remaining_ = 0;
    frame_backpressure_count_since_log_ = 0;
    next_frame_deadline_ms_ = get_current_time_ms();
    next_perf_log_ms_ = get_current_time_ms() + BROOKESIA_EMULATION_NES_PERF_LOG_INTERVAL_MS;

    lib_utils::TaskScheduler::TaskId task_id = 0;
    auto task = [this]() -> bool {
        return frame_task();
    };
    if (!scheduler->post_periodic(
                std::move(task), static_cast<int>(BROOKESIA_EMULATION_NES_FRAME_INTERVAL_MS), &task_id,
                NES_FRAME_TASK_GROUP
            )) {
        BROOKESIA_LOGE("Failed to schedule NES frame task");
        return false;
    }
    frame_task_id_ = task_id;
    return true;
}

bool Nes::start_audio_task_locked()
{
    if (audio_task_id_ != 0) {
        return true;
    }
    auto scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Task scheduler is not available");
    BROOKESIA_CHECK_FALSE_RETURN(scheduler->is_running(), false, "Task scheduler is not running");

    lib_utils::TaskScheduler::TaskId task_id = 0;
    auto task = [this]() -> bool {
        return audio_task();
    };
    if (!scheduler->post_periodic(
                std::move(task), static_cast<int>(NES_AUDIO_FEED_INTERVAL_MS), &task_id, NES_AUDIO_TASK_GROUP
            )) {
        BROOKESIA_LOGE("Failed to schedule NES audio task");
        return false;
    }
    audio_task_id_ = task_id;
    return true;
}

void Nes::take_task_ids_locked(std::vector<lib_utils::TaskScheduler::TaskId> &task_ids)
{
    if (audio_task_id_ != 0) {
        task_ids.push_back(audio_task_id_);
        audio_task_id_ = 0;
    }
    if (frame_task_id_ != 0) {
        task_ids.push_back(frame_task_id_);
        frame_task_id_ = 0;
    }
}

void Nes::cancel_and_wait_task_ids(const std::vector<lib_utils::TaskScheduler::TaskId> &task_ids)
{
    if (task_ids.empty()) {
        return;
    }
    auto scheduler = get_task_scheduler();
    if (!scheduler || !scheduler->is_running()) {
        return;
    }
    for (const auto task_id : task_ids) {
        scheduler->cancel(task_id);
    }
    for (const auto task_id : task_ids) {
        if (!scheduler->wait(task_id, NES_TASK_STOP_TIMEOUT_MS)) {
            BROOKESIA_LOGW("Timed out waiting for NES task %1% to stop", task_id);
        }
    }
}

bool Nes::frame_task()
{
    using Clock = std::chrono::steady_clock;

    const auto now_ms = get_current_time_ms();
    if ((next_frame_deadline_ms_ > 0) && (now_ms < next_frame_deadline_ms_)) {
        return true;
    }

    const uint32_t late_by_ms = (next_frame_deadline_ms_ > 0 && now_ms > next_frame_deadline_ms_) ?
                                static_cast<uint32_t>(now_ms - next_frame_deadline_ms_) : 0;
    std::shared_ptr<Runtime> runtime;
    {
        std::lock_guard lock(mutex_);
        runtime = runtime_;
    }
    const bool display_backpressure = runtime && runtime->consume_display_backpressure();
    const uint32_t max_frame_skip = get_auto_frame_skip_max();
    const bool scheduled_skip = frame_skip_remaining_ > 0;
    const bool can_skip_video = max_frame_skip > 0 && consecutive_frame_skip_count_ < max_frame_skip;
    const bool draw_video = !(can_skip_video &&
                              (scheduled_skip || skip_next_video_frame_ || display_backpressure ||
                               (late_by_ms > BROOKESIA_EMULATION_NES_FRAME_PERIOD_MS)));

    const auto step_start = Clock::now();
    FrameStepStats stats;
    if (!frame_step(draw_video, &stats)) {
        return false;
    }
    const bool drew_video = stats.drew_video;

    const auto step_elapsed_ms = static_cast<uint32_t>(
                                     std::chrono::duration_cast<std::chrono::milliseconds>(
                                         Clock::now() - step_start
                                     ).count()
                                 );
    if (drew_video) {
        consecutive_frame_skip_count_ = 0;
        frame_skip_remaining_ = current_frame_skip_;
    } else {
        consecutive_frame_skip_count_++;
        if (frame_skip_remaining_ > 0) {
            frame_skip_remaining_--;
        }
    }
    skip_next_video_frame_ = drew_video && (step_elapsed_ms > BROOKESIA_EMULATION_NES_FRAME_PERIOD_MS);
    next_frame_deadline_ms_ = (next_frame_deadline_ms_ > 0) ?
                              (next_frame_deadline_ms_ + BROOKESIA_EMULATION_NES_FRAME_PERIOD_MS) :
                              (now_ms + BROOKESIA_EMULATION_NES_FRAME_PERIOD_MS);
    const auto after_step_ms = get_current_time_ms();
    if (next_frame_deadline_ms_ + BROOKESIA_EMULATION_NES_FRAME_PERIOD_MS < after_step_ms) {
        next_frame_deadline_ms_ = after_step_ms + BROOKESIA_EMULATION_NES_FRAME_PERIOD_MS;
    }

    {
        std::lock_guard lock(perf_mutex_);
        frame_max_step_ms_ = std::max(frame_max_step_ms_, step_elapsed_ms);
        frame_max_emulate_ms_ = std::max(frame_max_emulate_ms_, stats.emulate_ms);
        frame_max_convert_ms_ = std::max(frame_max_convert_ms_, stats.convert_ms);
        frame_max_present_ms_ = std::max(frame_max_present_ms_, stats.present_ms);
        frame_max_late_by_ms_ = std::max(frame_max_late_by_ms_, late_by_ms);
        frame_count_since_log_++;
        frame_total_step_ms_ += step_elapsed_ms;
        if (drew_video) {
            frame_draw_count_since_log_++;
            frame_draw_step_ms_ += step_elapsed_ms;
        } else {
            frame_skip_count_since_log_++;
        }
        if (display_backpressure) {
            frame_backpressure_count_since_log_++;
        }
        if ((step_elapsed_ms > BROOKESIA_EMULATION_NES_FRAME_PERIOD_MS) ||
                (late_by_ms > BROOKESIA_EMULATION_NES_FRAME_PERIOD_MS)) {
            frame_late_count_++;
        }
    }

    if constexpr (BROOKESIA_EMULATION_NES_PERF_LOG_INTERVAL_MS > 0) {
        const auto perf_now_ms = get_current_time_ms();
        if (perf_now_ms >= next_perf_log_ms_) {
            std::shared_ptr<Runtime> runtime;
            {
                std::lock_guard lock(mutex_);
                runtime = runtime_;
            }
            auto perf = runtime ? runtime->get_perf_snapshot() : Runtime::PerfSnapshot{};
            uint32_t frames = 0;
            uint32_t max_step = 0;
            uint32_t max_emulate = 0;
            uint32_t max_convert = 0;
            uint32_t max_present = 0;
            uint32_t max_late_by = 0;
            uint32_t late = 0;
            uint32_t drawn = 0;
            uint32_t skipped = 0;
            uint32_t avg_step = 0;
            uint32_t avg_draw_step = 0;
            uint32_t audio_chunks = 0;
            uint32_t audio_max_feed = 0;
            uint32_t backpressure = 0;
            uint32_t frame_skip_level = 0;
            {
                std::lock_guard lock(perf_mutex_);
                frames = frame_count_since_log_;
                max_step = frame_max_step_ms_;
                max_emulate = frame_max_emulate_ms_;
                max_convert = frame_max_convert_ms_;
                max_present = frame_max_present_ms_;
                max_late_by = frame_max_late_by_ms_;
                late = frame_late_count_;
                drawn = frame_draw_count_since_log_;
                skipped = frame_skip_count_since_log_;
                avg_step = frames > 0 ? static_cast<uint32_t>(frame_total_step_ms_ / frames) : 0;
                avg_draw_step = frame_draw_count_since_log_ > 0 ?
                                static_cast<uint32_t>(frame_draw_step_ms_ / frame_draw_count_since_log_) : 0;
                audio_chunks = audio_chunks_since_log_;
                audio_max_feed = audio_max_feed_ms_;
                backpressure = frame_backpressure_count_since_log_;
                frame_skip_level = current_frame_skip_;
                frame_count_since_log_ = 0;
                frame_max_step_ms_ = 0;
                frame_max_emulate_ms_ = 0;
                frame_max_convert_ms_ = 0;
                frame_max_present_ms_ = 0;
                frame_max_late_by_ms_ = 0;
                frame_late_count_ = 0;
                frame_draw_count_since_log_ = 0;
                frame_skip_count_since_log_ = 0;
                frame_total_step_ms_ = 0;
                frame_draw_step_ms_ = 0;
                frame_backpressure_count_since_log_ = 0;
                audio_chunks_since_log_ = 0;
                audio_max_feed_ms_ = 0;
            }
            const bool should_raise_frame_skip = frames > 0 &&
                                                 ((late * 10 >= frames) ||
                                                  (backpressure * 10 >= frames) ||
                                                  (avg_draw_step > (BROOKESIA_EMULATION_NES_FRAME_PERIOD_MS + 2)));
            const bool should_lower_frame_skip = frames > 0 &&
                                                 (late * 100 <= frames * 2) &&
                                                 (backpressure * 100 <= frames * 2) &&
                                                 (avg_draw_step <= BROOKESIA_EMULATION_NES_FRAME_PERIOD_MS);
            if (should_raise_frame_skip && (current_frame_skip_ < max_frame_skip)) {
                current_frame_skip_++;
            } else if (should_lower_frame_skip && (current_frame_skip_ > base_frame_skip_)) {
                current_frame_skip_--;
            }
            std::string audio_state = "muted";
            if (perf.audio_mode == AudioMode::Disabled) {
                audio_state = "disabled";
            } else if (perf.audio_started) {
                audio_state = "enabled";
            }
            BROOKESIA_LOGI(
                "NES perf: frames=%1%, drawn=%2%, skip=%3%, late=%4%, avg_step=%5% ms, avg_draw=%6% ms, "
                "max_step=%7% ms, max_emulate=%8% ms, max_convert=%9% ms, max_present=%10% ms, "
                "max_late_by=%11% ms, audio=%12%, audio_chunks=%13%, audio_max_feed=%14% ms, "
                "audio_depth=%15%, audio_drop=%16%, render=%17%x%18%, video_mode=%19%, frame_skip=%20%->%21%, "
                "backpressure=%22%, present_drop=%23%, present_error=%24%",
                frames, drawn, skipped, late, avg_step, avg_draw_step, max_step, max_emulate, max_convert,
                max_present, max_late_by, audio_state, audio_chunks, audio_max_feed, perf.audio_queue_depth,
                perf.audio_drop_count, perf.render_width, perf.render_height,
                BROOKESIA_DESCRIBE_ENUM_TO_STR(perf.video_mode), frame_skip_level, current_frame_skip_, backpressure,
                perf.present_drop_count, perf.present_error_count
            );
            next_perf_log_ms_ = perf_now_ms + BROOKESIA_EMULATION_NES_PERF_LOG_INTERVAL_MS;
        }
    }
    return true;
}

bool Nes::audio_task()
{
    AudioStepStats stats;
    return audio_step(&stats);
}

bool Nes::frame_step(bool draw_video, FrameStepStats *stats)
{
    std::shared_ptr<Runtime> runtime;
    {
        std::lock_guard lock(mutex_);
        if ((state_ != State::Running) || !runtime_) {
            return false;
        }
        runtime = runtime_;
    }

    if (!runtime->step_frame(draw_video, stats)) {
        std::lock_guard lock(mutex_);
        set_state(State::Error);
        publish_error("NES frame step failed");
        return false;
    }
    return true;
}

bool Nes::audio_step(AudioStepStats *stats)
{
    std::shared_ptr<Runtime> runtime;
    {
        std::lock_guard lock(mutex_);
        if ((state_ != State::Running) || !runtime_) {
            return false;
        }
        runtime = runtime_;
    }

    const bool result = runtime->flush_audio_queue(stats);
    if (stats != nullptr) {
        std::lock_guard lock(perf_mutex_);
        audio_chunks_since_log_ += stats->fed_chunks;
        audio_max_feed_ms_ = std::max(audio_max_feed_ms_, stats->elapsed_ms);
    }
    return result;
}

#if BROOKESIA_EMULATION_NES_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    service::ServiceBase, Nes, Nes::get_instance().get_attributes().name, Nes::get_instance(),
    BROOKESIA_EMULATION_NES_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::emulation
