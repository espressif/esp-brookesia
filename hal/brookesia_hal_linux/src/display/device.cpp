/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "brookesia/hal_linux/macro_configs.h"

#if !BROOKESIA_HAL_LINUX_DISPLAY_BACKEND_STUB
#include "SDL2/SDL.h"
#endif

#if !BROOKESIA_HAL_LINUX_DISPLAY_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_interface/interfaces/display/backlight.hpp"
#include "brookesia/hal_interface/interfaces/display/panel.hpp"
#include "brookesia/hal_interface/interfaces/display/touch.hpp"
#include "brookesia/hal_linux/display/device.hpp"

namespace esp_brookesia::hal {

namespace {

constexpr uint8_t LINUX_DISPLAY_TOUCH_MAX_POINTS = 10;
constexpr const char *LINUX_DISPLAY_GROUP_ID = "linux_display";

#if !BROOKESIA_HAL_LINUX_DISPLAY_BACKEND_STUB
uint8_t make_sdl_touch_track_id(SDL_FingerID finger_id)
{
    return static_cast<uint8_t>(
               (static_cast<uint64_t>(finger_id) % (LINUX_DISPLAY_TOUCH_MAX_POINTS - 1)) + 1
           );
}
#endif

display::PanelIface::Info make_panel_info(const DisplayLinuxDevice::Config &config)
{
    return display::PanelIface::Info{
        .h_res = config.width_px,
        .v_res = config.height_px,
        .pixel_format = display::PanelIface::PixelFormat::RGB565,
        .group_id = LINUX_DISPLAY_GROUP_ID,
    };
}

display::TouchIface::Info make_touch_info(const DisplayLinuxDevice::Config &config)
{
    return display::TouchIface::Info{
        .x_max = config.width_px,
        .y_max = config.height_px,
        .max_points = LINUX_DISPLAY_TOUCH_MAX_POINTS,
        .operation_mode = display::TouchIface::OperationMode::Polling,
        .group_id = LINUX_DISPLAY_GROUP_ID,
    };
}

uint32_t rgb565_to_argb8888(uint16_t pixel)
{
    const uint8_t r = static_cast<uint8_t>(((pixel >> 11) & 0x1F) * 255 / 31);
    const uint8_t g = static_cast<uint8_t>(((pixel >> 5) & 0x3F) * 255 / 63);
    const uint8_t b = static_cast<uint8_t>((pixel & 0x1F) * 255 / 31);
    return (0xFFU << 24) | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

} // namespace

class DisplayLinuxBackend {
public:
    explicit DisplayLinuxBackend(DisplayLinuxDevice::Config config)
        : config_(std::move(config))
    {
    }

    ~DisplayLinuxBackend()
    {
        deinit();
    }

    bool init()
    {
#if !BROOKESIA_HAL_LINUX_DISPLAY_BACKEND_STUB
        std::unique_lock lock(mutex_);
        if (sdl_available_) {
            return true;
        }
        if (sdl_failed_) {
            return false;
        }

        render_stop_requested_ = false;
        render_init_done_ = false;
        render_dirty_ = false;
        try {
            render_thread_ = std::thread([this]() {
                render_thread_loop();
            });
        } catch (const std::exception &e) {
            BROOKESIA_LOGW("Failed to create SDL2 render thread, falling back to stub: %1%", e.what());
            sdl_failed_ = true;
            return false;
        }

        init_cv_.wait(lock, [this]() {
            return render_init_done_;
        });
        const bool available = sdl_available_;
        lock.unlock();
        if (!available && render_thread_.joinable()) {
            render_thread_.join();
        }
        return available;
#else
        return false;
#endif
    }

    void deinit()
    {
#if !BROOKESIA_HAL_LINUX_DISPLAY_BACKEND_STUB
        {
            std::lock_guard lock(mutex_);
            render_stop_requested_ = true;
        }
        render_cv_.notify_all();
        if (render_thread_.joinable()) {
            render_thread_.join();
        }
        std::lock_guard lock(mutex_);
        deinit_locked();
#endif
    }

    bool draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data)
    {
        {
            std::lock_guard lock(mutex_);
            if ((data == nullptr) || (x2 <= x1) || (y2 <= y1) || (x2 > config_.width_px) ||
                    (y2 > config_.height_px)) {
                BROOKESIA_LOGE("Invalid panel draw region: (%1%, %2%) -> (%3%, %4%)", x1, y1, x2, y2);
                return false;
            }

#if !BROOKESIA_HAL_LINUX_DISPLAY_BACKEND_STUB
            if (sdl_available_) {
                const uint32_t width = x2 - x1;
                const uint32_t height = y2 - y1;
                const auto *pixels = reinterpret_cast<const uint16_t *>(data);
                for (uint32_t y = 0; y < height; y++) {
                    auto *destination = framebuffer_.data() + static_cast<size_t>(y1 + y) * config_.width_px + x1;
                    const auto *source = pixels + static_cast<size_t>(y) * width;
                    for (uint32_t x = 0; x < width; x++) {
                        destination[x] = rgb565_to_argb8888(source[x]);
                    }
                }
                render_dirty_ = true;
            }
#endif
        }
#if !BROOKESIA_HAL_LINUX_DISPLAY_BACKEND_STUB
        render_cv_.notify_one();
#endif
        return true;
    }

    bool read_points(std::vector<display::TouchIface::Point> &points)
    {
        display::TouchIface::InterruptHandler interrupt_handler = nullptr;
        void *interrupt_handler_ctx = nullptr;
        {
            std::lock_guard lock(mutex_);
            points.clear();
            if (!active_touch_points_.empty()) {
                points.reserve(active_touch_points_.size());
                for (const auto &[_, point] : active_touch_points_) {
                    points.push_back(point);
                }
            } else if (!sdl_available()) {
                points = {
                    display::TouchIface::Point{.x = 12, .y = 34, .pressure = 56, .track_id = 0},
                    display::TouchIface::Point{.x = 78, .y = 90, .pressure = 12, .track_id = 1},
                };
            }
            if (touch_changed_) {
                interrupt_handler = interrupt_handler_;
                interrupt_handler_ctx = interrupt_handler_ctx_;
                touch_changed_ = false;
            }
        }
        if (interrupt_handler) {
            (void)interrupt_handler(interrupt_handler_ctx);
        }
        return true;
    }

    bool register_interrupt_handler(display::TouchIface::InterruptHandler handler, void *ctx)
    {
        std::lock_guard lock(mutex_);
        interrupt_handler_ = handler;
        interrupt_handler_ctx_ = ctx;
        return true;
    }

    bool set_brightness(uint8_t percent)
    {
        {
            std::lock_guard lock(mutex_);
            brightness_ = std::min<uint8_t>(percent, 100);
            brightness_dirty_ = true;
            render_dirty_ = true;
        }
        render_cv_.notify_one();
        return true;
    }

    bool get_brightness(uint8_t &percent)
    {
        std::lock_guard lock(mutex_);
        percent = brightness_;
        return true;
    }

    bool set_light_on_off(bool on)
    {
        {
            std::lock_guard lock(mutex_);
            is_light_on_ = on;
            brightness_dirty_ = true;
            render_dirty_ = true;
        }
        render_cv_.notify_one();
        return true;
    }

    bool is_light_on() const
    {
        std::lock_guard lock(mutex_);
        return is_light_on_;
    }

    bool is_quit_requested() const
    {
        std::lock_guard lock(mutex_);
        return quit_requested_;
    }

    void *window_handle() const
    {
#if !BROOKESIA_HAL_LINUX_DISPLAY_BACKEND_STUB
        std::lock_guard lock(mutex_);
        return window_;
#else
        return nullptr;
#endif
    }

    void *renderer_handle() const
    {
#if !BROOKESIA_HAL_LINUX_DISPLAY_BACKEND_STUB
        std::lock_guard lock(mutex_);
        return renderer_;
#else
        return nullptr;
#endif
    }

private:
    bool sdl_available() const
    {
#if !BROOKESIA_HAL_LINUX_DISPLAY_BACKEND_STUB
        return sdl_available_;
#else
        return false;
#endif
    }

    bool init_sdl_locked()
    {
#if !BROOKESIA_HAL_LINUX_DISPLAY_BACKEND_STUB
        if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
            BROOKESIA_LOGW("SDL2 display backend unavailable, falling back to stub: %1%", SDL_GetError());
            sdl_failed_ = true;
            return false;
        }
        sdl_subsystem_initialized_ = true;

        if (!config_.render_driver.empty()) {
            SDL_SetHint(SDL_HINT_RENDER_DRIVER, config_.render_driver.c_str());
        }

        window_ = SDL_CreateWindow(
                      config_.window_title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                      config_.width_px, config_.height_px, SDL_WINDOW_SHOWN
                  );
        if (window_ == nullptr) {
            BROOKESIA_LOGW("Failed to create SDL2 window, falling back to stub: %1%", SDL_GetError());
            deinit_locked();
            sdl_failed_ = true;
            return false;
        }
        const uint32_t renderer_flags = (config_.render_driver == "software") ?
                                        SDL_RENDERER_SOFTWARE :
                                        (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        renderer_ = SDL_CreateRenderer(window_, -1, renderer_flags);
        if (renderer_ == nullptr) {
            renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
        }
        if (renderer_ == nullptr) {
            BROOKESIA_LOGW("Failed to create SDL2 renderer, falling back to stub: %1%", SDL_GetError());
            deinit_locked();
            sdl_failed_ = true;
            return false;
        }
        texture_ = SDL_CreateTexture(
                       renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, config_.width_px,
                       config_.height_px
                   );
        if (texture_ == nullptr) {
            BROOKESIA_LOGW("Failed to create SDL2 texture, falling back to stub: %1%", SDL_GetError());
            deinit_locked();
            sdl_failed_ = true;
            return false;
        }

        framebuffer_.assign(static_cast<size_t>(config_.width_px) * config_.height_px, 0xFF000000U);
        present_locked();
        sdl_available_ = true;
        BROOKESIA_LOGI("SDL2 display backend initialized: size(%1%x%2%)", config_.width_px, config_.height_px);
        return true;
#else
        return false;
#endif
    }

    void render_thread_loop()
    {
#if !BROOKESIA_HAL_LINUX_DISPLAY_BACKEND_STUB
        bool initialized = false;
        {
            std::lock_guard lock(mutex_);
            (void)init_sdl_locked();
            initialized = sdl_available_;
            render_init_done_ = true;
            render_dirty_ = sdl_available_;
            brightness_dirty_ = sdl_available_;
        }
        init_cv_.notify_all();
        if (!initialized) {
            return;
        }
        render_cv_.notify_all();

        while (true) {
            std::unique_lock lock(mutex_);
            render_cv_.wait_for(lock, std::chrono::milliseconds(10), [this]() {
                return render_stop_requested_ || render_dirty_ || brightness_dirty_;
            });
            if (render_stop_requested_) {
                break;
            }
            if (!sdl_available_) {
                continue;
            }
            pump_events_locked();
            if (brightness_dirty_) {
                apply_brightness_locked();
                brightness_dirty_ = false;
            }
            if (render_dirty_) {
                present_locked();
                render_dirty_ = false;
            }
        }
        {
            std::lock_guard lock(mutex_);
            deinit_locked();
        }
#endif
    }

    void pump_events_locked()
    {
#if !BROOKESIA_HAL_LINUX_DISPLAY_BACKEND_STUB
        if (!sdl_available_) {
            return;
        }

        SDL_Event event = {};
        while (SDL_PollEvent(&event) != 0) {
            switch (event.type) {
            case SDL_QUIT:
                quit_requested_ = true;
                is_light_on_ = false;
                brightness_dirty_ = true;
                render_dirty_ = true;
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEMOTION:
                if ((event.type == SDL_MOUSEBUTTONDOWN) || ((event.motion.state & SDL_BUTTON_LMASK) != 0)) {
                    int x = 0;
                    int y = 0;
                    SDL_GetMouseState(&x, &y);
                    active_touch_points_[0] = display::TouchIface::Point{
                        .x = static_cast<int16_t>(std::clamp(x, 0, static_cast<int>(config_.width_px - 1))),
                        .y = static_cast<int16_t>(std::clamp(y, 0, static_cast<int>(config_.height_px - 1))),
                        .pressure = 1,
                        .track_id = 0,
                    };
                    touch_changed_ = true;
                }
                break;
            case SDL_MOUSEBUTTONUP:
                active_touch_points_.erase(0);
                touch_changed_ = true;
                break;
            case SDL_FINGERDOWN:
            case SDL_FINGERMOTION: {
                const auto track_id = make_sdl_touch_track_id(event.tfinger.fingerId);
                active_touch_points_[track_id] = display::TouchIface::Point{
                    .x = static_cast<int16_t>(std::clamp(
                                                  static_cast<int>(event.tfinger.x * config_.width_px), 0,
                                                  static_cast<int>(config_.width_px - 1)
                                              )),
                    .y = static_cast<int16_t>(std::clamp(
                                                  static_cast<int>(event.tfinger.y * config_.height_px), 0,
                                                  static_cast<int>(config_.height_px - 1)
                                              )),
                    .pressure = static_cast<uint16_t>(std::max(1.0f, event.tfinger.pressure * 1024.0f)),
                    .track_id = track_id,
                };
                touch_changed_ = true;
                break;
            }
            case SDL_FINGERUP:
                active_touch_points_.erase(make_sdl_touch_track_id(event.tfinger.fingerId));
                touch_changed_ = true;
                break;
            default:
                break;
            }
        }
#endif
    }

    void apply_brightness_locked()
    {
#if !BROOKESIA_HAL_LINUX_DISPLAY_BACKEND_STUB
        if (window_ == nullptr) {
            return;
        }
        const float brightness = is_light_on_ ? static_cast<float>(brightness_) / 100.0f : 0.0f;
        SDL_SetWindowBrightness(window_, brightness);
        SDL_SetWindowOpacity(window_, 1.0f);
#endif
    }

    void present_locked()
    {
#if !BROOKESIA_HAL_LINUX_DISPLAY_BACKEND_STUB
        if ((renderer_ == nullptr) || (texture_ == nullptr)) {
            return;
        }
        if (SDL_UpdateTexture(texture_, nullptr, framebuffer_.data(), config_.width_px * sizeof(uint32_t)) != 0) {
            BROOKESIA_LOGW("Failed to update SDL2 display texture: %1%", SDL_GetError());
            return;
        }
        if (SDL_RenderClear(renderer_) != 0) {
            BROOKESIA_LOGW("Failed to clear SDL2 display renderer: %1%", SDL_GetError());
            return;
        }
        if (SDL_RenderCopy(renderer_, texture_, nullptr, nullptr) != 0) {
            BROOKESIA_LOGW("Failed to copy SDL2 display texture: %1%", SDL_GetError());
            return;
        }
        const uint8_t overlay_alpha = get_backlight_overlay_alpha();
        if (overlay_alpha > 0) {
            if (SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND) != 0) {
                BROOKESIA_LOGW("Failed to set SDL2 display blend mode: %1%", SDL_GetError());
                return;
            }
            if (SDL_SetRenderDrawColor(renderer_, 0, 0, 0, overlay_alpha) != 0) {
                BROOKESIA_LOGW("Failed to set SDL2 display overlay color: %1%", SDL_GetError());
                return;
            }
            if (SDL_RenderFillRect(renderer_, nullptr) != 0) {
                BROOKESIA_LOGW("Failed to render SDL2 display backlight overlay: %1%", SDL_GetError());
                return;
            }
        }
        SDL_RenderPresent(renderer_);
#endif
    }

    uint8_t get_backlight_overlay_alpha() const
    {
        if (!is_light_on_) {
            return 255;
        }
        return static_cast<uint8_t>(255 - ((static_cast<uint16_t>(brightness_) * 255 + 50) / 100));
    }

#if !BROOKESIA_HAL_LINUX_DISPLAY_BACKEND_STUB
    void deinit_locked()
    {
        if (texture_ != nullptr) {
            SDL_DestroyTexture(texture_);
            texture_ = nullptr;
        }
        if (renderer_ != nullptr) {
            SDL_DestroyRenderer(renderer_);
            renderer_ = nullptr;
        }
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }
        if (sdl_subsystem_initialized_) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
            sdl_subsystem_initialized_ = false;
        }
        sdl_available_ = false;
        framebuffer_.clear();
        active_touch_points_.clear();
        render_dirty_ = false;
        brightness_dirty_ = false;
    }
#endif

    mutable std::mutex mutex_;
    std::condition_variable init_cv_;
    std::condition_variable render_cv_;
    DisplayLinuxDevice::Config config_;
    uint8_t brightness_ = 100;
    bool is_light_on_ = true;
    bool quit_requested_ = false;
    std::map<uint8_t, display::TouchIface::Point> active_touch_points_;
    bool touch_changed_ = false;
    bool render_init_done_ = false;
    bool render_stop_requested_ = false;
    bool render_dirty_ = false;
    bool brightness_dirty_ = false;
    std::thread render_thread_;
    display::TouchIface::InterruptHandler interrupt_handler_ = nullptr;
    void *interrupt_handler_ctx_ = nullptr;

#if !BROOKESIA_HAL_LINUX_DISPLAY_BACKEND_STUB
    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
    SDL_Texture *texture_ = nullptr;
    bool sdl_subsystem_initialized_ = false;
    bool sdl_available_ = false;
    bool sdl_failed_ = false;
    std::vector<uint32_t> framebuffer_;
#endif
};

class DisplayPanelLinuxImpl: public display::PanelIface {
public:
    explicit DisplayPanelLinuxImpl(std::shared_ptr<DisplayLinuxBackend> backend)
        : display::PanelIface(make_panel_info(DisplayLinuxDevice::get_instance().get_config()))
        , backend_(std::move(backend))
    {
    }

    bool draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data) override
    {
        return backend_->draw_bitmap(x1, y1, x2, y2, data);
    }

    bool get_driver_specific(DriverSpecific &specific) override
    {
        specific = DriverSpecific{
            .io_handle = backend_->window_handle(),
            .panel_handle = backend_->renderer_handle(),
            .bus_type = BusType::Generic,
            .draw_x_align_bytes = 1,
            .draw_y_align_bytes = 1,
            .event_dispatcher = {},
        };
        return true;
    }

private:
    std::shared_ptr<DisplayLinuxBackend> backend_;
};

class DisplayTouchLinuxImpl: public display::TouchIface {
public:
    explicit DisplayTouchLinuxImpl(std::shared_ptr<DisplayLinuxBackend> backend)
        : display::TouchIface(make_touch_info(DisplayLinuxDevice::get_instance().get_config()))
        , backend_(std::move(backend))
    {
    }

    bool read_points(std::vector<Point> &points) override
    {
        return backend_->read_points(points);
    }

    bool register_interrupt_handler(InterruptHandler handler, void *ctx) override
    {
        return backend_->register_interrupt_handler(handler, ctx);
    }

    bool get_driver_specific(DriverSpecific &specific) override
    {
        specific = DriverSpecific{
            .io_handle = backend_->window_handle(),
            .touch_handle = nullptr,
        };
        return true;
    }

private:
    std::shared_ptr<DisplayLinuxBackend> backend_;
};

class DisplayBacklightLinuxImpl: public display::BacklightIface {
public:
    explicit DisplayBacklightLinuxImpl(std::shared_ptr<DisplayLinuxBackend> backend)
        : display::BacklightIface(display::BacklightIface::Info{
        .group_id = LINUX_DISPLAY_GROUP_ID,
    })
    , backend_(std::move(backend))
    {
    }

    bool set_brightness(uint8_t percent) override
    {
        return backend_->set_brightness(percent);
    }

    bool get_brightness(uint8_t &percent) override
    {
        return backend_->get_brightness(percent);
    }

    bool is_light_on_off_supported() override
    {
        return true;
    }

    bool set_light_on_off(bool on) override
    {
        return backend_->set_light_on_off(on);
    }

    bool is_light_on() const override
    {
        return backend_->is_light_on();
    }

private:
    std::shared_ptr<DisplayLinuxBackend> backend_;
};

bool DisplayLinuxDevice::probe()
{
    return true;
}

bool DisplayLinuxDevice::configure(Config config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if ((config.width_px == 0) || (config.height_px == 0)) {
        BROOKESIA_LOGW("Reject invalid Linux display size: %1%x%2%", config.width_px, config.height_px);
        return false;
    }
    if (config.window_title.empty()) {
        config.window_title = Config{}.window_title;
    }
    if (config.render_driver.empty()) {
        config.render_driver = Config{}.render_driver;
    }

    std::lock_guard lock(config_mutex_);
    if (initialized_) {
        BROOKESIA_LOGW("Linux display configuration must be set before device initialization");
        return false;
    }

    config_ = std::move(config);
    return true;
}

DisplayLinuxDevice::Config DisplayLinuxDevice::get_config() const
{
    std::lock_guard lock(config_mutex_);
    return config_;
}

bool DisplayLinuxDevice::is_quit_requested() const
{
    return (backend_ != nullptr) && backend_->is_quit_requested();
}

std::vector<InterfaceSpec> DisplayLinuxDevice::get_interface_specs() const
{
    return {
        {display::PanelIface::NAME, PANEL_IFACE_NAME},
        {display::TouchIface::NAME, TOUCH_IFACE_NAME},
        {display::BacklightIface::NAME, BACKLIGHT_IFACE_NAME},
    };
}

bool DisplayLinuxDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto config = get_config();
    backend_ = std::make_shared<DisplayLinuxBackend>(config);
    if (!backend_->init()) {
        BROOKESIA_LOGW("Using display stub fallback");
    }

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        panel_iface_ = std::make_shared<DisplayPanelLinuxImpl>(backend_), false, "Failed to create display panel linux"
    );
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        touch_iface_ = std::make_shared<DisplayTouchLinuxImpl>(backend_), false, "Failed to create display touch linux"
    );
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        backlight_iface_ = std::make_shared<DisplayBacklightLinuxImpl>(backend_), false,
        "Failed to create display backlight linux"
    );

    interfaces_.emplace(PANEL_IFACE_NAME, panel_iface_);
    interfaces_.emplace(TOUCH_IFACE_NAME, touch_iface_);
    interfaces_.emplace(BACKLIGHT_IFACE_NAME, backlight_iface_);

    {
        std::lock_guard lock(config_mutex_);
        initialized_ = true;
    }
    return true;
}

void DisplayLinuxDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(BACKLIGHT_IFACE_NAME);
    interfaces_.erase(TOUCH_IFACE_NAME);
    interfaces_.erase(PANEL_IFACE_NAME);
    backlight_iface_.reset();
    touch_iface_.reset();
    panel_iface_.reset();
    backend_.reset();
    {
        std::lock_guard lock(config_mutex_);
        initialized_ = false;
    }
}

#if BROOKESIA_HAL_LINUX_ENABLE_DISPLAY_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, DisplayLinuxDevice, DisplayLinuxDevice::DEVICE_NAME, DisplayLinuxDevice::get_instance(),
    BROOKESIA_HAL_LINUX_DISPLAY_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
