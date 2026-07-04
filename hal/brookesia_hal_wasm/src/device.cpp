/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include "emscripten/emscripten.h"
#endif
#include "SDL2/SDL.h"

#include "boost/system/error_code.hpp"

#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_player.hpp"
#include "brookesia/hal_interface/interfaces/audio/processor.hpp"
#include "brookesia/hal_interface/interfaces/display/backlight.hpp"
#include "brookesia/hal_interface/interfaces/display/panel.hpp"
#include "brookesia/hal_interface/interfaces/display/touch.hpp"
#include "brookesia/hal_interface/interfaces/network/connectivity.hpp"
#include "brookesia/hal_interface/interfaces/network/http_client.hpp"
#include "brookesia/hal_interface/interfaces/storage/file_system.hpp"
#include "brookesia/hal_interface/interfaces/storage/key_value.hpp"
#include "brookesia/hal_interface/interfaces/system/board_info.hpp"
#include "brookesia/hal_interface/interfaces/video/camera.hpp"
#include "brookesia/hal_interface/interfaces/video/processor.hpp"
#include "brookesia/hal_interface/interfaces/wifi/basic.hpp"
#include "brookesia/hal_interface/interfaces/wifi/softap.hpp"
#include "brookesia/hal_interface/interfaces/wifi/station.hpp"
#include "brookesia/hal_wasm.hpp"

namespace esp_brookesia::hal {

namespace {

constexpr uint8_t WASM_DISPLAY_TOUCH_MAX_POINTS = 10;
constexpr const char *WASM_DISPLAY_GROUP_ID = "wasm_display";
constexpr const char *WASM_STORAGE_ROOT = "/brookesia";
constexpr const char *WASM_STORAGE_KV_ROOT = "/brookesia/kv";
constexpr uint64_t WASM_STORAGE_TOTAL_BYTES = 64ULL * 1024ULL * 1024ULL;

#if defined(__EMSCRIPTEN__)
EM_JS(int, brookesia_wasm_fetch_start_js, (
    const char *method_ptr,
    const char *url_ptr,
    const char *headers_json_ptr,
    const char *body_ptr,
    int body_size,
    int timeout_ms
), {
    const root = typeof Module !== 'undefined' ? Module : globalThis;
    if (!root.__brookesiaFetch) {
        root.__brookesiaFetch = {
            nextHandle: 1,
            items: new Map(),
        };
    }
    const state = root.__brookesiaFetch;
    const handle = state.nextHandle++;
    const item = {
        timeoutId: 0,
        status: 0,
        statusText: "",
        headers: {},
        body: new Uint8Array(),
        error: "",
        canceled: false,
        timedOut: false,
        done: false,
    };
    state.items.set(handle, item);

    const method = UTF8ToString(method_ptr);
    const url = UTF8ToString(url_ptr);
    let headers = {};
    try {
        headers = JSON.parse(UTF8ToString(headers_json_ptr) || '{}');
    } catch (error) {
        headers = {};
    }
    let body = undefined;
    if (body_ptr && body_size > 0) {
        body = HEAPU8.slice(body_ptr, body_ptr + body_size);
    }

    try {
        const xhr = new XMLHttpRequest();
        xhr.open(method, url, false);
        xhr.overrideMimeType('text/plain; charset=x-user-defined');
        Object.keys(headers).forEach((key) => {
            xhr.setRequestHeader(key, String(headers[key]));
        });
        if (timeout_ms > 0) {
            // Browsers disallow timeout on synchronous document XHR; the App Store watchdog covers missing events.
        }
        xhr.send(body !== undefined && method !== 'GET' && method !== 'HEAD' ? body : null);
        item.status = xhr.status;
        item.statusText = xhr.statusText || "";
        const rawHeaders = xhr.getAllResponseHeaders() || "";
        rawHeaders
            .trim()
            .split(String.fromCharCode(13))
            .join(String.fromCharCode(10))
            .split(String.fromCharCode(10))
            .forEach((line) => {
            const separator = line.indexOf(':');
            if (separator <= 0) {
                return;
            }
            const key = line.substring(0, separator).trim().toLowerCase();
            const value = line.substring(separator + 1).trim();
            if (key) {
                item.headers[key] = value;
            }
        });
        const text = xhr.responseText || "";
        const data = new Uint8Array(text.length);
        for (let i = 0; i < text.length; ++i) {
            data[i] = text.charCodeAt(i) & 0xff;
        }
        item.body = data;
    } catch (error) {
        if (item.canceled || (error && error.name === 'AbortError')) {
            item.canceled = true;
            item.error = 'Request canceled';
        } else {
            item.error = error && error.message ? error.message : String(error);
        }
        console.warn('Brookesia wasm XHR error:', handle, item.error);
    }
    item.done = true;
    return handle;
});

EM_JS(int, brookesia_wasm_fetch_is_done_js, (int handle), {
    const state = Module.__brookesiaFetch;
    const item = state && state.items ? state.items.get(handle) : null;
    return !item || item.done ? 1 : 0;
});

EM_JS(int, brookesia_wasm_fetch_get_status_js, (int handle), {
    const state = Module.__brookesiaFetch;
    const item = state && state.items ? state.items.get(handle) : null;
    return item ? item.status : 0;
});

EM_JS(int, brookesia_wasm_fetch_get_body_size_js, (int handle), {
    const state = Module.__brookesiaFetch;
    const item = state && state.items ? state.items.get(handle) : null;
    return item && item.body ? item.body.length : 0;
});

EM_JS(int, brookesia_wasm_fetch_copy_body_js, (int handle, char *buffer, int size), {
    const state = Module.__brookesiaFetch;
    const item = state && state.items ? state.items.get(handle) : null;
    if (!item || !item.body || !buffer || size <= 0) {
        return 0;
    }
    const copySize = Math.min(size, item.body.length);
    HEAPU8.set(item.body.subarray(0, copySize), buffer);
    return copySize;
});

EM_JS(int, brookesia_wasm_fetch_get_headers_size_js, (int handle), {
    const state = Module.__brookesiaFetch;
    const item = state && state.items ? state.items.get(handle) : null;
    const text = JSON.stringify(item && item.headers ? item.headers : {});
    return lengthBytesUTF8(text) + 1;
});

EM_JS(int, brookesia_wasm_fetch_copy_headers_js, (int handle, char *buffer, int size), {
    const state = Module.__brookesiaFetch;
    const item = state && state.items ? state.items.get(handle) : null;
    const text = JSON.stringify(item && item.headers ? item.headers : {});
    const required = lengthBytesUTF8(text) + 1;
    if (buffer && size > 0) {
        stringToUTF8(text, buffer, size);
    }
    return required;
});

EM_JS(int, brookesia_wasm_fetch_get_error_size_js, (int handle), {
    const state = Module.__brookesiaFetch;
    const item = state && state.items ? state.items.get(handle) : null;
    const text = item && item.error ? item.error : "";
    return lengthBytesUTF8(text) + 1;
});

EM_JS(int, brookesia_wasm_fetch_copy_error_js, (int handle, char *buffer, int size), {
    const state = Module.__brookesiaFetch;
    const item = state && state.items ? state.items.get(handle) : null;
    const text = item && item.error ? item.error : "";
    const required = lengthBytesUTF8(text) + 1;
    if (buffer && size > 0) {
        stringToUTF8(text, buffer, size);
    }
    return required;
});

EM_JS(int, brookesia_wasm_fetch_is_canceled_js, (int handle), {
    const state = Module.__brookesiaFetch;
    const item = state && state.items ? state.items.get(handle) : null;
    return item && item.canceled ? 1 : 0;
});

EM_JS(int, brookesia_wasm_fetch_is_timed_out_js, (int handle), {
    const state = Module.__brookesiaFetch;
    const item = state && state.items ? state.items.get(handle) : null;
    return item && item.timedOut ? 1 : 0;
});

EM_JS(void, brookesia_wasm_fetch_cancel_js, (int handle), {
    const state = Module.__brookesiaFetch;
    const item = state && state.items ? state.items.get(handle) : null;
    if (item) {
        item.canceled = true;
    }
});

EM_JS(void, brookesia_wasm_fetch_clear_js, (int handle), {
    const state = Module.__brookesiaFetch;
    const item = state && state.items ? state.items.get(handle) : null;
    if (!item) {
        return;
    }
    if (item.timeoutId) {
        clearTimeout(item.timeoutId);
    }
    state.items.delete(handle);
});
#endif

display::PanelIface::Info make_panel_info(const DisplayWasmDevice::Config &config)
{
    return display::PanelIface::Info{
        .h_res = config.width_px,
        .v_res = config.height_px,
        .pixel_format = display::PanelIface::PixelFormat::RGB565,
        .group_id = WASM_DISPLAY_GROUP_ID,
    };
}

display::TouchIface::Info make_touch_info(const DisplayWasmDevice::Config &config)
{
    return display::TouchIface::Info{
        .x_max = config.width_px,
        .y_max = config.height_px,
        .max_points = WASM_DISPLAY_TOUCH_MAX_POINTS,
        .operation_mode = display::TouchIface::OperationMode::Polling,
        .group_id = WASM_DISPLAY_GROUP_ID,
    };
}

uint32_t rgb565_to_argb8888(uint16_t pixel)
{
    const uint8_t r = static_cast<uint8_t>(((pixel >> 11) & 0x1F) * 255 / 31);
    const uint8_t g = static_cast<uint8_t>(((pixel >> 5) & 0x3F) * 255 / 63);
    const uint8_t b = static_cast<uint8_t>((pixel & 0x1F) * 255 / 31);
    return (0xFFU << 24) | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

uint8_t modulate_color_component(uint8_t component, uint8_t mod)
{
    return static_cast<uint8_t>((static_cast<uint16_t>(component) * mod) / 255);
}

uint8_t make_track_id(SDL_FingerID finger_id)
{
    return static_cast<uint8_t>((static_cast<uint64_t>(finger_id) % (WASM_DISPLAY_TOUCH_MAX_POINTS - 1)) + 1);
}

bool browser_is_online()
{
#if defined(__EMSCRIPTEN__)
    return EM_ASM_INT({
        if (typeof navigator === 'undefined') {
            return 1;
        }
        return navigator.onLine ? 1 : 0;
    }) != 0;
#else
    return true;
#endif
}

void ensure_storage_directories()
{
    std::error_code error_code;
    for (const auto *path : {
             "/brookesia",
             "/brookesia/kv",
             "/brookesia/fs",
             "/brookesia/fs/spiffs",
             "/brookesia/fs/littlefs",
             "/brookesia/fs/fatfs",
             "/brookesia/fs/sdcard",
         }) {
        std::filesystem::create_directories(path, error_code);
        error_code.clear();
    }
}

storage::KeyValueIface::ValueType get_value_type(const storage::KeyValueIface::Value &value)
{
    if (std::holds_alternative<bool>(value)) {
        return storage::KeyValueIface::ValueType::Bool;
    }
    if (std::holds_alternative<int32_t>(value)) {
        return storage::KeyValueIface::ValueType::Int;
    }
    if (std::holds_alternative<std::string>(value)) {
        return storage::KeyValueIface::ValueType::String;
    }
    return storage::KeyValueIface::ValueType::Max;
}

} // namespace

class DisplayWasmBackend {
public:
    explicit DisplayWasmBackend(DisplayWasmDevice::Config config)
        : config_(std::move(config))
    {
    }

    ~DisplayWasmBackend()
    {
        deinit();
    }

    bool init()
    {
        std::lock_guard lock(mutex_);
        if (sdl_available_) {
            return true;
        }
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
            BROOKESIA_LOGW("SDL video init failed in wasm HAL: %1%", SDL_GetError());
            return false;
        }
        window_ = SDL_CreateWindow(
            config_.window_title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            config_.width_px, config_.height_px, SDL_WINDOW_SHOWN
        );
        if (window_ == nullptr) {
            BROOKESIA_LOGW("SDL window create failed in wasm HAL: %1%", SDL_GetError());
            return false;
        }
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (renderer_ == nullptr) {
            renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
        }
        if (renderer_ == nullptr) {
            BROOKESIA_LOGW("SDL renderer create failed in wasm HAL: %1%", SDL_GetError());
            return false;
        }
        texture_ = SDL_CreateTexture(
            renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, config_.width_px, config_.height_px
        );
        if (texture_ == nullptr) {
            BROOKESIA_LOGW("SDL texture create failed in wasm HAL: %1%", SDL_GetError());
            return false;
        }
        framebuffer_.assign(static_cast<size_t>(config_.width_px) * config_.height_px, 0xFF000000U);
        SDL_RenderSetLogicalSize(renderer_, config_.width_px, config_.height_px);
        sdl_available_ = true;
        present_locked();
        return true;
    }

    void deinit()
    {
        std::lock_guard lock(mutex_);
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
        sdl_available_ = false;
    }

    void poll_events()
    {
        display::TouchIface::InterruptHandler interrupt_handler = nullptr;
        void *interrupt_handler_ctx = nullptr;
        {
            std::lock_guard lock(mutex_);
            SDL_Event event = {};
            while (SDL_PollEvent(&event) != 0) {
                switch (event.type) {
                case SDL_QUIT:
                    quit_requested_ = true;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEMOTION: {
                    if ((event.type == SDL_MOUSEMOTION) && ((event.motion.state & SDL_BUTTON_LMASK) == 0)) {
                        break;
                    }
                    const int x = (event.type == SDL_MOUSEMOTION) ? event.motion.x : event.button.x;
                    const int y = (event.type == SDL_MOUSEMOTION) ? event.motion.y : event.button.y;
                    active_touch_points_[0] = display::TouchIface::Point{
                        .x = static_cast<int16_t>(std::clamp(x, 0, static_cast<int>(config_.width_px - 1))),
                        .y = static_cast<int16_t>(std::clamp(y, 0, static_cast<int>(config_.height_px - 1))),
                        .pressure = 1,
                        .track_id = 0,
                    };
                    push_touch_snapshot_locked();
                    touch_changed_ = true;
                    break;
                }
                case SDL_MOUSEBUTTONUP:
                    active_touch_points_.erase(0);
                    push_touch_snapshot_locked();
                    touch_changed_ = true;
                    break;
                case SDL_FINGERDOWN:
                case SDL_FINGERMOTION: {
                    const uint8_t track_id = make_track_id(event.tfinger.fingerId);
                    active_touch_points_[track_id] = display::TouchIface::Point{
                        .x = static_cast<int16_t>(std::clamp(
                            static_cast<int>(event.tfinger.x * config_.width_px), 0,
                            static_cast<int>(config_.width_px - 1)
                        )),
                        .y = static_cast<int16_t>(std::clamp(
                            static_cast<int>(event.tfinger.y * config_.height_px), 0,
                            static_cast<int>(config_.height_px - 1)
                        )),
                        .pressure = static_cast<uint16_t>(std::max(1, static_cast<int>(event.tfinger.pressure * 255))),
                        .track_id = track_id,
                    };
                    push_touch_snapshot_locked();
                    touch_changed_ = true;
                    break;
                }
                case SDL_FINGERUP:
                    active_touch_points_.erase(make_track_id(event.tfinger.fingerId));
                    push_touch_snapshot_locked();
                    touch_changed_ = true;
                    break;
                default:
                    break;
                }
            }
            if (touch_changed_) {
                interrupt_handler = interrupt_handler_;
                interrupt_handler_ctx = interrupt_handler_ctx_;
                touch_changed_ = false;
            }
        }
        if (interrupt_handler != nullptr) {
            (void)interrupt_handler(interrupt_handler_ctx);
        }
    }

    bool draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data)
    {
        poll_events();
        std::lock_guard lock(mutex_);
        if ((data == nullptr) || (x2 <= x1) || (y2 <= y1) || (x2 > config_.width_px) || (y2 > config_.height_px)) {
            BROOKESIA_LOGE("Invalid wasm panel draw region: (%1%, %2%) -> (%3%, %4%)", x1, y1, x2, y2);
            return false;
        }
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
        present_locked();
        return true;
    }

    bool read_points(std::vector<display::TouchIface::Point> &points)
    {
        poll_events();
        std::lock_guard lock(mutex_);
        if (!pending_touch_snapshots_.empty()) {
            points = std::move(pending_touch_snapshots_.front());
            pending_touch_snapshots_.pop_front();
        } else {
            copy_active_touch_points_locked(points);
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
        std::lock_guard lock(mutex_);
        brightness_ = std::min<uint8_t>(percent, 100);
        present_locked();
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
        std::lock_guard lock(mutex_);
        is_light_on_ = on;
        present_locked();
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

    uint16_t width() const
    {
        std::lock_guard lock(mutex_);
        return config_.width_px;
    }

    uint16_t height() const
    {
        std::lock_guard lock(mutex_);
        return config_.height_px;
    }

    bool copy_snapshot_rgba(uint8_t *destination, size_t destination_size) const
    {
        std::lock_guard lock(mutex_);
        const size_t pixel_count = static_cast<size_t>(config_.width_px) * config_.height_px;
        const size_t required_size = pixel_count * 4;
        if (!sdl_available_ || (destination == nullptr) || (framebuffer_.size() != pixel_count) ||
                (destination_size < required_size)) {
            return false;
        }

        const uint8_t mod = brightness_mod_locked();
        for (size_t i = 0; i < pixel_count; i++) {
            const uint32_t pixel = framebuffer_[i];
            const size_t offset = i * 4;
            destination[offset] = modulate_color_component(static_cast<uint8_t>((pixel >> 16) & 0xFF), mod);
            destination[offset + 1] = modulate_color_component(static_cast<uint8_t>((pixel >> 8) & 0xFF), mod);
            destination[offset + 2] = modulate_color_component(static_cast<uint8_t>(pixel & 0xFF), mod);
            destination[offset + 3] = static_cast<uint8_t>((pixel >> 24) & 0xFF);
        }
        return true;
    }

private:
    void copy_active_touch_points_locked(std::vector<display::TouchIface::Point> &points) const
    {
        points.clear();
        points.reserve(active_touch_points_.size());
        for (const auto &[_, point] : active_touch_points_) {
            points.push_back(point);
        }
    }

    void push_touch_snapshot_locked()
    {
        std::vector<display::TouchIface::Point> points;
        copy_active_touch_points_locked(points);
        pending_touch_snapshots_.push_back(std::move(points));
        while (pending_touch_snapshots_.size() > 8) {
            pending_touch_snapshots_.pop_front();
        }
    }

    uint8_t brightness_mod_locked() const
    {
        return is_light_on_ ? static_cast<uint8_t>((static_cast<uint16_t>(brightness_) * 255 + 50) / 100) : 0;
    }

    void present_locked()
    {
        if (!sdl_available_ || (texture_ == nullptr) || (renderer_ == nullptr)) {
            return;
        }
        const uint8_t mod = brightness_mod_locked();
        SDL_SetTextureColorMod(texture_, mod, mod, mod);
        SDL_SetTextureAlphaMod(texture_, 255);
        SDL_UpdateTexture(
            texture_, nullptr, framebuffer_.data(), static_cast<int>(config_.width_px * sizeof(uint32_t))
        );
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(renderer_);
    }

    DisplayWasmDevice::Config config_;
    mutable std::mutex mutex_;
    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
    SDL_Texture *texture_ = nullptr;
    std::vector<uint32_t> framebuffer_;
    std::map<uint8_t, display::TouchIface::Point> active_touch_points_;
    std::deque<std::vector<display::TouchIface::Point>> pending_touch_snapshots_;
    display::TouchIface::InterruptHandler interrupt_handler_ = nullptr;
    void *interrupt_handler_ctx_ = nullptr;
    uint8_t brightness_ = 100;
    bool is_light_on_ = true;
    bool touch_changed_ = false;
    bool quit_requested_ = false;
    bool sdl_available_ = false;
};

namespace {

std::mutex active_display_backend_mutex;
std::weak_ptr<DisplayWasmBackend> active_display_backend;

std::shared_ptr<DisplayWasmBackend> get_active_display_backend()
{
    std::lock_guard lock(active_display_backend_mutex);
    return active_display_backend.lock();
}

void set_active_display_backend(const std::shared_ptr<DisplayWasmBackend> &backend)
{
    std::lock_guard lock(active_display_backend_mutex);
    active_display_backend = backend;
}

void clear_active_display_backend(const std::shared_ptr<DisplayWasmBackend> &backend)
{
    std::lock_guard lock(active_display_backend_mutex);
    if (active_display_backend.lock() == backend) {
        active_display_backend.reset();
    }
}

} // namespace

class DisplayPanelWasmImpl: public display::PanelIface {
public:
    explicit DisplayPanelWasmImpl(std::shared_ptr<DisplayWasmBackend> backend, DisplayWasmDevice::Config config)
        : display::PanelIface(make_panel_info(config))
        , backend_(std::move(backend))
    {
    }

    bool draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data) override
    {
        return backend_ != nullptr && backend_->draw_bitmap(x1, y1, x2, y2, data);
    }

private:
    std::shared_ptr<DisplayWasmBackend> backend_;
};

class DisplayTouchWasmImpl: public display::TouchIface {
public:
    explicit DisplayTouchWasmImpl(std::shared_ptr<DisplayWasmBackend> backend, DisplayWasmDevice::Config config)
        : display::TouchIface(make_touch_info(config))
        , backend_(std::move(backend))
    {
    }

    bool read_points(std::vector<Point> &points) override
    {
        return backend_ != nullptr && backend_->read_points(points);
    }

    bool register_interrupt_handler(InterruptHandler handler, void *ctx) override
    {
        return backend_ != nullptr && backend_->register_interrupt_handler(handler, ctx);
    }

private:
    std::shared_ptr<DisplayWasmBackend> backend_;
};

class DisplayBacklightWasmImpl: public display::BacklightIface {
public:
    explicit DisplayBacklightWasmImpl(std::shared_ptr<DisplayWasmBackend> backend)
        : display::BacklightIface(display::BacklightIface::Info{.group_id = WASM_DISPLAY_GROUP_ID})
        , backend_(std::move(backend))
    {
    }

    bool set_brightness(uint8_t percent) override
    {
        return backend_ != nullptr && backend_->set_brightness(percent);
    }

    bool get_brightness(uint8_t &percent) override
    {
        return backend_ != nullptr && backend_->get_brightness(percent);
    }

    bool is_light_on_off_supported() override
    {
        return true;
    }

    bool set_light_on_off(bool on) override
    {
        return backend_ != nullptr && backend_->set_light_on_off(on);
    }

    bool is_light_on() const override
    {
        return backend_ != nullptr && backend_->is_light_on();
    }

private:
    std::shared_ptr<DisplayWasmBackend> backend_;
};

bool DisplayWasmDevice::configure(Config config)
{
    if ((config.width_px == 0) || (config.height_px == 0)) {
        BROOKESIA_LOGW("Reject invalid wasm display size: %1%x%2%", config.width_px, config.height_px);
        return false;
    }
    if (config.window_title.empty()) {
        config.window_title = Config{}.window_title;
    }
    std::lock_guard lock(config_mutex_);
    if (initialized_) {
        BROOKESIA_LOGW("Wasm display configuration must be set before device initialization");
        return false;
    }
    config_ = std::move(config);
    return true;
}

DisplayWasmDevice::Config DisplayWasmDevice::get_config() const
{
    std::lock_guard lock(config_mutex_);
    return config_;
}

bool DisplayWasmDevice::is_quit_requested() const
{
    return (backend_ != nullptr) && backend_->is_quit_requested();
}

void DisplayWasmDevice::poll_events()
{
    if (backend_ != nullptr) {
        backend_->poll_events();
    }
}

bool DisplayWasmDevice::probe()
{
    return true;
}

std::vector<InterfaceSpec> DisplayWasmDevice::get_interface_specs() const
{
    return {
        {display::PanelIface::NAME, PANEL_IFACE_NAME},
        {display::TouchIface::NAME, TOUCH_IFACE_NAME},
        {display::BacklightIface::NAME, BACKLIGHT_IFACE_NAME},
    };
}

bool DisplayWasmDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto config = get_config();
    backend_ = std::make_shared<DisplayWasmBackend>(config);
    BROOKESIA_CHECK_FALSE_RETURN(backend_->init(), false, "Failed to initialize wasm display backend");
    set_active_display_backend(backend_);
    panel_iface_ = std::make_shared<DisplayPanelWasmImpl>(backend_, config);
    touch_iface_ = std::make_shared<DisplayTouchWasmImpl>(backend_, config);
    backlight_iface_ = std::make_shared<DisplayBacklightWasmImpl>(backend_);
    interfaces_.emplace(PANEL_IFACE_NAME, panel_iface_);
    interfaces_.emplace(TOUCH_IFACE_NAME, touch_iface_);
    interfaces_.emplace(BACKLIGHT_IFACE_NAME, backlight_iface_);
    std::lock_guard lock(config_mutex_);
    initialized_ = true;
    return true;
}

void DisplayWasmDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(BACKLIGHT_IFACE_NAME);
    interfaces_.erase(TOUCH_IFACE_NAME);
    interfaces_.erase(PANEL_IFACE_NAME);
    backlight_iface_.reset();
    touch_iface_.reset();
    panel_iface_.reset();
    clear_active_display_backend(backend_);
    backend_.reset();
    std::lock_guard lock(config_mutex_);
    initialized_ = false;
}

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE int brookesia_wasm_display_snapshot_width()
{
    auto backend = get_active_display_backend();
    if (backend == nullptr) {
        return 0;
    }
    return backend->width();
}

extern "C" EMSCRIPTEN_KEEPALIVE int brookesia_wasm_display_snapshot_height()
{
    auto backend = get_active_display_backend();
    if (backend == nullptr) {
        return 0;
    }
    return backend->height();
}

extern "C" EMSCRIPTEN_KEEPALIVE int brookesia_wasm_display_snapshot_copy_rgba(
    uint8_t *destination, int destination_size
)
{
    auto backend = get_active_display_backend();
    if ((backend == nullptr) || (destination_size <= 0)) {
        return 0;
    }

    const size_t width = backend->width();
    const size_t height = backend->height();
    const size_t required_size = width * height * 4;
    if ((required_size > static_cast<size_t>(destination_size)) ||
            (required_size > static_cast<size_t>(std::numeric_limits<int>::max()))) {
        return 0;
    }
    if (!backend->copy_snapshot_rgba(destination, static_cast<size_t>(destination_size))) {
        return 0;
    }
    return static_cast<int>(required_size);
}
#endif

class StorageKeyValueWasmImpl: public storage::KeyValueIface {
public:
    bool init() override
    {
        std::lock_guard lock(mutex_);
        ensure_storage_directories();
        root_dir_ = WASM_STORAGE_KV_ROOT;
        std::error_code error_code;
        std::filesystem::create_directories(root_dir_, error_code);
        if (error_code) {
            last_error_ = "Failed to create wasm key-value storage root: " + error_code.message();
            BROOKESIA_LOGE("%1%", last_error_);
            return false;
        }
        namespaces_.clear();
        load_all_locked();
        is_initialized_ = true;
        last_error_.clear();
        return true;
    }

    void deinit() override
    {
        std::lock_guard lock(mutex_);
        namespaces_.clear();
        is_initialized_ = false;
        last_error_.clear();
    }

    bool list(const std::string &nspace, std::vector<EntryInfo> &entries) override
    {
        std::lock_guard lock(mutex_);
        entries.clear();
        if (!check_initialized_locked()) {
            return false;
        }
        auto namespace_it = namespaces_.find(nspace);
        if (namespace_it == namespaces_.end()) {
            return true;
        }
        for (const auto &[key, value] : namespace_it->second) {
            entries.push_back(EntryInfo{.nspace = nspace, .key = key, .type = get_value_type(value)});
        }
        return true;
    }

    bool set(const std::string &nspace, const KeyValueMap &key_value_map) override
    {
        std::lock_guard lock(mutex_);
        if (!check_initialized_locked()) {
            return false;
        }
        auto &storage = namespaces_[nspace];
        for (const auto &[key, value] : key_value_map) {
            storage[key] = value;
        }
        if (!save_namespace_locked(nspace)) {
            return false;
        }
        last_error_.clear();
        return true;
    }

    bool get(
        const std::string &nspace, const std::vector<std::string> &keys, KeyValueMap &key_value_map
    ) override {
        std::lock_guard lock(mutex_);
        key_value_map.clear();
        if (!check_initialized_locked()) {
            return false;
        }
        auto namespace_it = namespaces_.find(nspace);
        if (namespace_it == namespaces_.end()) {
            return true;
        }
        if (keys.empty()) {
            key_value_map = namespace_it->second;
            return true;
        }
        for (const auto &key : keys) {
            auto value_it = namespace_it->second.find(key);
            if (value_it != namespace_it->second.end()) {
                key_value_map.emplace(key, value_it->second);
            }
        }
        return true;
    }

    bool erase(const std::string &nspace, const std::vector<std::string> &keys) override
    {
        std::lock_guard lock(mutex_);
        if (!check_initialized_locked()) {
            return false;
        }
        auto namespace_it = namespaces_.find(nspace);
        if (namespace_it == namespaces_.end()) {
            return true;
        }
        if (keys.empty()) {
            namespaces_.erase(namespace_it);
            remove_namespace_file_locked(nspace);
            return true;
        }
        for (const auto &key : keys) {
            namespace_it->second.erase(key);
        }
        if (namespace_it->second.empty()) {
            namespaces_.erase(namespace_it);
            remove_namespace_file_locked(nspace);
        } else if (!save_namespace_locked(nspace)) {
            return false;
        }
        return true;
    }

private:
    static std::string hex_encode(std::string_view input)
    {
        std::ostringstream stream;
        stream << std::hex << std::setfill('0');
        for (unsigned char ch : input) {
            stream << std::setw(2) << static_cast<int>(ch);
        }
        return stream.str();
    }

    static bool hex_decode(const std::string &input, std::string &output)
    {
        if ((input.size() % 2) != 0) {
            return false;
        }
        output.clear();
        output.reserve(input.size() / 2);
        for (size_t i = 0; i < input.size(); i += 2) {
            char *end = nullptr;
            const std::string byte = input.substr(i, 2);
            const long value = std::strtol(byte.c_str(), &end, 16);
            if ((end == nullptr) || (*end != '\0') || (value < 0) || (value > 0xFF)) {
                return false;
            }
            output.push_back(static_cast<char>(value));
        }
        return true;
    }

    std::filesystem::path get_namespace_path_locked(const std::string &nspace) const
    {
        return root_dir_ / (hex_encode(nspace) + ".kv");
    }

    bool save_namespace_locked(const std::string &nspace)
    {
        std::error_code error_code;
        std::filesystem::create_directories(root_dir_, error_code);
        if (error_code) {
            last_error_ = "Failed to create wasm key-value storage root: " + error_code.message();
            BROOKESIA_LOGE("%1%", last_error_);
            return false;
        }

        std::ofstream file(get_namespace_path_locked(nspace), std::ios::trunc);
        if (!file) {
            last_error_ = "Failed to open wasm key-value namespace file";
            BROOKESIA_LOGE("%1%", last_error_);
            return false;
        }

        const auto namespace_it = namespaces_.find(nspace);
        if (namespace_it == namespaces_.end()) {
            return true;
        }
        for (const auto &[key, value] : namespace_it->second) {
            file << hex_encode(key) << '\t';
            std::visit([&file](auto &&stored_value) {
                using T = std::decay_t<decltype(stored_value)>;
                if constexpr (std::is_same_v<T, bool>) {
                    file << "b\t" << (stored_value ? "1" : "0");
                } else if constexpr (std::is_same_v<T, int32_t>) {
                    file << "i\t" << stored_value;
                } else {
                    file << "s\t" << hex_encode(stored_value);
                }
            }, value);
            file << '\n';
        }
        return true;
    }

    void remove_namespace_file_locked(const std::string &nspace)
    {
        std::error_code error_code;
        std::filesystem::remove(get_namespace_path_locked(nspace), error_code);
    }

    void load_all_locked()
    {
        namespaces_.clear();
        std::error_code error_code;
        if (!std::filesystem::exists(root_dir_, error_code)) {
            return;
        }
        for (const auto &entry : std::filesystem::directory_iterator(root_dir_, error_code)) {
            if (error_code || !entry.is_regular_file() || entry.path().extension() != ".kv") {
                continue;
            }
            std::string nspace;
            if (!hex_decode(entry.path().stem().string(), nspace)) {
                continue;
            }
            std::ifstream file(entry.path());
            std::string line;
            while (std::getline(file, line)) {
                std::istringstream stream(line);
                std::string encoded_key;
                std::string type;
                std::string encoded_value;
                if (!std::getline(stream, encoded_key, '\t') || !std::getline(stream, type, '\t') ||
                        !std::getline(stream, encoded_value)) {
                    continue;
                }
                std::string key;
                if (!hex_decode(encoded_key, key)) {
                    continue;
                }
                if (type == "b") {
                    namespaces_[nspace][key] = (encoded_value == "1");
                } else if (type == "i") {
                    namespaces_[nspace][key] = static_cast<int32_t>(std::strtol(encoded_value.c_str(), nullptr, 10));
                } else if (type == "s") {
                    std::string value;
                    if (hex_decode(encoded_value, value)) {
                        namespaces_[nspace][key] = std::move(value);
                    }
                }
            }
        }
    }

    bool check_initialized_locked()
    {
        if (is_initialized_) {
            return true;
        }
        last_error_ = "Wasm key-value storage is not initialized";
        return false;
    }

    std::mutex mutex_;
    bool is_initialized_ = false;
    std::map<std::string, KeyValueMap> namespaces_;
    std::filesystem::path root_dir_;
};

class StorageFileSystemWasmImpl: public storage::FileSystemIface {
public:
    StorageFileSystemWasmImpl()
    {
        ensure_storage_directories();
        info_list_ = {
            {
                .fs_type = storage::FileSystemIface::FileSystemType::SPIFFS,
                .medium_type = storage::FileSystemIface::MediumType::Flash,
                .mount_point = "/spiffs",
                .root_path = std::string(WASM_STORAGE_ROOT) + "/fs/spiffs",
                .supports_directories = false,
            },
            {
                .fs_type = storage::FileSystemIface::FileSystemType::LittleFS,
                .medium_type = storage::FileSystemIface::MediumType::Flash,
                .mount_point = "/littlefs",
                .root_path = std::string(WASM_STORAGE_ROOT) + "/fs/littlefs",
                .supports_directories = true,
            },
            {
                .fs_type = storage::FileSystemIface::FileSystemType::FATFS,
                .medium_type = storage::FileSystemIface::MediumType::Flash,
                .mount_point = "/fatfs",
                .root_path = std::string(WASM_STORAGE_ROOT) + "/fs/fatfs",
                .supports_directories = true,
            },
            {
                .fs_type = storage::FileSystemIface::FileSystemType::FATFS,
                .medium_type = storage::FileSystemIface::MediumType::SDCard,
                .mount_point = "/sdcard",
                .root_path = std::string(WASM_STORAGE_ROOT) + "/fs/sdcard",
                .supports_directories = true,
            },
        };
    }

    bool get_capacity(const char *mount_point, Capacity &capacity) override
    {
        if (mount_point == nullptr) {
            return false;
        }
        for (const auto &info : info_list_) {
            if (std::string(info.mount_point) == mount_point) {
                capacity = {
                    .total_bytes = WASM_STORAGE_TOTAL_BYTES,
                    .used_bytes = 0,
                    .free_bytes = WASM_STORAGE_TOTAL_BYTES,
                };
                return true;
            }
        }
        return false;
    }
};

bool StorageWasmDevice::probe()
{
    return true;
}

std::vector<InterfaceSpec> StorageWasmDevice::get_interface_specs() const
{
    return {
        {storage::KeyValueIface::NAME, KEY_VALUE_IFACE_NAME},
        {storage::FileSystemIface::NAME, FILE_SYSTEM_IFACE_NAME},
    };
}

bool StorageWasmDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    key_value_iface_ = std::make_shared<StorageKeyValueWasmImpl>();
    BROOKESIA_CHECK_FALSE_RETURN(key_value_iface_->init(), false, "Failed to initialize wasm key-value storage");
    file_system_iface_ = std::make_shared<StorageFileSystemWasmImpl>();
    interfaces_.emplace(KEY_VALUE_IFACE_NAME, key_value_iface_);
    interfaces_.emplace(FILE_SYSTEM_IFACE_NAME, file_system_iface_);
    return true;
}

void StorageWasmDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (key_value_iface_) {
        key_value_iface_->deinit();
    }
    interfaces_.erase(FILE_SYSTEM_IFACE_NAME);
    interfaces_.erase(KEY_VALUE_IFACE_NAME);
    file_system_iface_.reset();
    key_value_iface_.reset();
}

class BoardInfoWasmImpl: public system::BoardInfoIface {
public:
    BoardInfoWasmImpl()
        : system::BoardInfoIface(system::BoardInfoIface::Info{
              .name = "ESP-Brookesia WebAssembly",
              .chip = "wasm32",
              .version = "Emscripten",
              .description = "Browser-hosted ESP-Brookesia simulator",
              .manufacturer = "Espressif",
          })
    {
    }
};

bool SystemWasmDevice::probe()
{
    return true;
}

std::vector<InterfaceSpec> SystemWasmDevice::get_interface_specs() const
{
    return {
        {system::BoardInfoIface::NAME, BOARD_INFO_IFACE_NAME},
    };
}

bool SystemWasmDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    board_info_iface_ = std::make_shared<BoardInfoWasmImpl>();
    interfaces_.emplace(BOARD_INFO_IFACE_NAME, board_info_iface_);
    return true;
}

void SystemWasmDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(BOARD_INFO_IFACE_NAME);
    board_info_iface_.reset();
}

class NetworkConnectivityWasmImpl: public network::ConnectivityIface {
public:
    network::NetworkStatus get_status() const override
    {
        const bool online = browser_is_online();
        return network::NetworkStatus{
            .interface_type = network::InterfaceType::WifiStation,
            .link_state = online ? network::LinkState::Up : network::LinkState::Down,
            .ip_state = online ? network::IpState::Ready : network::IpState::None,
            .reachability = online ? network::Reachability::Internet : network::Reachability::Unreachable,
            .ip_info = online ? std::make_optional(network::IpInfo{
                         .ip = "0.0.0.0",
                         .netmask = "",
                         .gateway = "",
                         .dns = "",
                     }) :
                     std::nullopt,
            .signal_dbm = online ? std::make_optional(-45) : std::nullopt,
            .connected_duration_ms = std::nullopt,
        };
    }
};

class NetworkHttpTransactionWasm: public network::HttpClientIface::Transaction {
public:
    using HttpClient = network::HttpClientIface;
    using ErrorCode = HttpClient::ErrorCode;

    ErrorCode open(const HttpClient::Request &request, HttpClient::Response &response, uint32_t &content_length) override
    {
        content_length = 0;
        data_.clear();
        offset_ = 0;
        complete_ = false;
        canceled_ = false;

#if defined(__EMSCRIPTEN__)
        if (request.url.empty()) {
            response.error_message = "URL is empty";
            return ErrorCode::InvalidArgument;
        }
        if (request.body.size() > static_cast<size_t>(INT32_MAX)) {
            response.error_message = "HTTP request body is too large for wasm fetch bridge";
            return ErrorCode::BodyTooLarge;
        }

        const auto headers_json = serialize_request_headers(request.headers);
        const auto body_size = static_cast<int>(request.body.size());
        fetch_handle_ = brookesia_wasm_fetch_start_js(
                            method_to_string(request.method),
                            request.url.c_str(),
                            headers_json.c_str(),
                            request.body.empty() ? nullptr : request.body.data(),
                            body_size,
                            static_cast<int>(request.timeout_ms)
                        );
        if (fetch_handle_ <= 0) {
            response.error_message = "Failed to start browser fetch";
            return ErrorCode::ClientInitFailed;
        }
        const lib_utils::FunctionGuard clear_fetch([this]() {
            clear_fetch_handle();
        });

        while (brookesia_wasm_fetch_is_done_js(fetch_handle_) == 0) {
            if (canceled_) {
                brookesia_wasm_fetch_cancel_js(fetch_handle_);
            }
            emscripten_sleep(1);
        }

        const auto fetch_error_message = copy_fetch_error(fetch_handle_);
        if (!fetch_error_message.empty()) {
            response.status_code = brookesia_wasm_fetch_get_status_js(fetch_handle_);
            response.error_message = fetch_error_message;
            if (canceled_ || brookesia_wasm_fetch_is_canceled_js(fetch_handle_) != 0) {
                return ErrorCode::Canceled;
            }
            if (brookesia_wasm_fetch_is_timed_out_js(fetch_handle_) != 0) {
                return ErrorCode::Timeout;
            }
            return ErrorCode::RequestFailed;
        }

        response.status_code = brookesia_wasm_fetch_get_status_js(fetch_handle_);
        parse_response_headers_json(copy_fetch_headers(fetch_handle_), response.headers);
        const int received_size = brookesia_wasm_fetch_get_body_size_js(fetch_handle_);
        if (received_size > 0) {
            data_.assign(static_cast<size_t>(received_size), '\0');
            const int copied_size = brookesia_wasm_fetch_copy_body_js(
                                        fetch_handle_,
                                        data_.data(),
                                        static_cast<int>(data_.size())
                                    );
            if (copied_size != received_size) {
                response.error_message = "Failed to copy browser fetch response body";
                data_.clear();
                return ErrorCode::RequestFailed;
            }
        }
        content_length = static_cast<uint32_t>(data_.size());
        complete_ = true;

        if (response.status_code == 0) {
            response.error_message = "Browser fetch returned status 0";
            return ErrorCode::RequestFailed;
        }

        return canceled_ ? ErrorCode::Canceled : ErrorCode::Ok;
#else
        (void)request;
        response.error_message = "Wasm fetch is only available under Emscripten";
        return ErrorCode::UnsupportedTransport;
#endif
    }

    int read(char *buffer, size_t size, std::string &error_message) override
    {
        (void)error_message;
        if (canceled_) {
            error_message = "Request canceled";
            return -1;
        }
        if (buffer == nullptr) {
            error_message = "Invalid read buffer";
            return -1;
        }
        if (offset_ >= data_.size()) {
            return 0;
        }
        const size_t read_size = std::min(size, data_.size() - offset_);
        std::memcpy(buffer, data_.data() + offset_, read_size);
        offset_ += read_size;
        return static_cast<int>(read_size);
    }

    bool is_complete() const override
    {
        return complete_ && (offset_ >= data_.size());
    }

    void cancel() override
    {
        canceled_ = true;
#if defined(__EMSCRIPTEN__)
        if (fetch_handle_ > 0) {
            brookesia_wasm_fetch_cancel_js(fetch_handle_);
        }
#endif
    }

private:
    static const char *method_to_string(HttpClient::Method method)
    {
        switch (method) {
        case HttpClient::Method::Post:
            return "POST";
        case HttpClient::Method::Put:
            return "PUT";
        case HttpClient::Method::Patch:
            return "PATCH";
        case HttpClient::Method::Delete:
            return "DELETE";
        case HttpClient::Method::Head:
            return "HEAD";
        case HttpClient::Method::Get:
        default:
            return "GET";
        }
    }

#if defined(__EMSCRIPTEN__)
    static std::string serialize_request_headers(const boost::json::object &headers)
    {
        boost::json::object serialized_headers;
        for (const auto &[key, value] : headers) {
            if (value.is_string()) {
                serialized_headers[key] = std::string(value.as_string().c_str());
            } else {
                serialized_headers[key] = boost::json::serialize(value);
            }
        }
        return boost::json::serialize(serialized_headers);
    }

    template<typename SizeFunc, typename CopyFunc>
    static std::string copy_fetch_string(int handle, SizeFunc size_func, CopyFunc copy_func)
    {
        const int required_size = size_func(handle);
        if (required_size <= 1) {
            return {};
        }
        std::string text(static_cast<size_t>(required_size), '\0');
        const int written_size = copy_func(handle, text.data(), required_size);
        if (written_size <= 0) {
            return {};
        }
        text.resize(static_cast<size_t>(std::min(written_size, required_size)));
        if (!text.empty() && text.back() == '\0') {
            text.pop_back();
        }
        return text;
    }

    static std::string copy_fetch_error(int handle)
    {
        return copy_fetch_string(
                   handle,
                   brookesia_wasm_fetch_get_error_size_js,
                   brookesia_wasm_fetch_copy_error_js
               );
    }

    static std::string copy_fetch_headers(int handle)
    {
        return copy_fetch_string(
                   handle,
                   brookesia_wasm_fetch_get_headers_size_js,
                   brookesia_wasm_fetch_copy_headers_js
               );
    }

    static void parse_response_headers_json(const std::string &headers_json, boost::json::object &headers)
    {
        headers.clear();
        boost::system::error_code error_code;
        auto parsed = boost::json::parse(headers_json, error_code);
        if (error_code || !parsed.is_object()) {
            return;
        }
        headers = boost::json::object(parsed.as_object());
    }

    void clear_fetch_handle()
    {
        if (fetch_handle_ <= 0) {
            return;
        }
        brookesia_wasm_fetch_clear_js(fetch_handle_);
        fetch_handle_ = 0;
    }
#endif

    std::string data_;
    size_t offset_ = 0;
    bool complete_ = false;
    bool canceled_ = false;
#if defined(__EMSCRIPTEN__)
    int fetch_handle_ = 0;
#endif
};

class NetworkHttpClientWasmImpl: public network::HttpClientIface {
public:
    std::shared_ptr<Transaction> create_transaction() override
    {
        return std::make_shared<NetworkHttpTransactionWasm>();
    }
};

bool NetworkWasmDevice::probe()
{
    return true;
}

std::vector<InterfaceSpec> NetworkWasmDevice::get_interface_specs() const
{
    return {
        {network::ConnectivityIface::NAME, CONNECTIVITY_IFACE_NAME},
        {network::HttpClientIface::NAME, HTTP_CLIENT_IFACE_NAME},
    };
}

bool NetworkWasmDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    connectivity_iface_ = std::make_shared<NetworkConnectivityWasmImpl>();
    http_client_iface_ = std::make_shared<NetworkHttpClientWasmImpl>();
    interfaces_.emplace(CONNECTIVITY_IFACE_NAME, connectivity_iface_);
    interfaces_.emplace(HTTP_CLIENT_IFACE_NAME, http_client_iface_);
    return true;
}

void NetworkWasmDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(HTTP_CLIENT_IFACE_NAME);
    interfaces_.erase(CONNECTIVITY_IFACE_NAME);
    http_client_iface_.reset();
    connectivity_iface_.reset();
}

class WifiWasmBackend {
public:
    bool configure(wifi::BasicIface::RuntimeContext runtime, wifi::BasicIface::Callbacks callbacks)
    {
        std::lock_guard lock(mutex_);
        runtime_ = std::move(runtime);
        basic_callbacks_ = std::move(callbacks);
        return true;
    }

    bool configure(wifi::StationIface::Callbacks callbacks)
    {
        std::lock_guard lock(mutex_);
        station_callbacks_ = std::move(callbacks);
        return true;
    }

    bool configure(wifi::SoftApIface::Callbacks callbacks)
    {
        std::lock_guard lock(mutex_);
        softap_callbacks_ = std::move(callbacks);
        return true;
    }

    void clear_callbacks()
    {
        std::lock_guard lock(mutex_);
        basic_callbacks_ = {};
        station_callbacks_ = {};
        softap_callbacks_ = {};
    }

    bool basic_action(wifi::BasicAction action)
    {
        wifi::BasicEvent event = wifi::BasicEvent::Max;
        {
            std::lock_guard lock(mutex_);
            if (basic_callbacks_.on_action) {
                basic_callbacks_.on_action(action);
            }
            switch (action) {
            case wifi::BasicAction::Init:
                initialized_ = true;
                event = wifi::BasicEvent::Inited;
                break;
            case wifi::BasicAction::Deinit:
                initialized_ = false;
                started_ = false;
                event = wifi::BasicEvent::Deinited;
                break;
            case wifi::BasicAction::Start:
                started_ = true;
                event = wifi::BasicEvent::Started;
                break;
            case wifi::BasicAction::Stop:
                started_ = false;
                connected_ = false;
                event = wifi::BasicEvent::Stopped;
                break;
            default:
                return false;
            }
            if (basic_callbacks_.on_event && event != wifi::BasicEvent::Max) {
                basic_callbacks_.on_event(event, true);
            }
        }
        return true;
    }

    bool station_action(wifi::StationAction action)
    {
        std::lock_guard lock(mutex_);
        if (station_callbacks_.on_action) {
            station_callbacks_.on_action(action);
        }
        switch (action) {
        case wifi::StationAction::Connect:
            if (!target_ap_.is_valid()) {
                target_ap_ = wifi::ConnectApInfo("Brookesia Fake AP", "brookesia");
            }
            connected_ = true;
            last_connected_ap_ = target_ap_;
            connected_aps_.clear();
            connected_aps_.push_back(target_ap_);
            if (station_callbacks_.on_last_connected_ap_info_updated) {
                station_callbacks_.on_last_connected_ap_info_updated(last_connected_ap_);
            }
            if (station_callbacks_.on_connected_ap_infos_updated) {
                station_callbacks_.on_connected_ap_infos_updated(connected_aps_);
            }
            if (station_callbacks_.on_event) {
                station_callbacks_.on_event(wifi::StationEvent::Connected, true);
            }
            return true;
        case wifi::StationAction::Disconnect:
            connected_ = false;
            if (station_callbacks_.on_event) {
                station_callbacks_.on_event(wifi::StationEvent::Disconnected, true);
            }
            return true;
        default:
            return false;
        }
    }

    network::NetworkStatus get_status() const
    {
        std::lock_guard lock(mutex_);
        const bool online = connected_;
        return network::NetworkStatus{
            .interface_type = network::InterfaceType::WifiStation,
            .link_state = online ? network::LinkState::Up : network::LinkState::Down,
            .ip_state = online ? network::IpState::Ready : network::IpState::None,
            .reachability = online ? network::Reachability::Internet : network::Reachability::Unreachable,
            .ip_info = online ? std::make_optional(network::IpInfo{
                         .ip = "192.0.2.10",
                         .netmask = "255.255.255.0",
                         .gateway = "192.0.2.1",
                         .dns = "192.0.2.1",
                     }) :
                     std::nullopt,
            .signal_dbm = online ? std::make_optional(-45) : std::nullopt,
            .connected_duration_ms = std::nullopt,
        };
    }

    wifi::BasicIface::RuntimeContext runtime_;
    wifi::BasicIface::Callbacks basic_callbacks_;
    wifi::StationIface::Callbacks station_callbacks_;
    wifi::SoftApIface::Callbacks softap_callbacks_;
    wifi::ConnectApInfo target_ap_;
    wifi::ConnectApInfo last_connected_ap_;
    wifi::ConnectApInfoList connected_aps_;
    wifi::ScanParams scan_params_;
    wifi::SoftApParams softap_params_{
        .ssid = "BrookesiaWasm",
        .password = "",
        .max_connection = 4,
        .channel = std::nullopt,
    };
    mutable std::mutex mutex_;
    bool initialized_ = false;
    bool started_ = false;
    bool connected_ = false;
    bool softap_started_ = false;
    bool scan_started_ = false;
};

class WifiWasmBasicIface: public wifi::BasicIface {
public:
    explicit WifiWasmBasicIface(std::shared_ptr<WifiWasmBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(RuntimeContext runtime, Callbacks callbacks) override
    {
        return backend_->configure(std::move(runtime), std::move(callbacks));
    }
    void clear_callbacks() override
    {
        backend_->clear_callbacks();
    }
    bool init() override
    {
        return backend_->basic_action(wifi::BasicAction::Init);
    }
    void deinit() override
    {
        (void)backend_->basic_action(wifi::BasicAction::Deinit);
    }
    bool start() override
    {
        return backend_->basic_action(wifi::BasicAction::Start);
    }
    void stop() override
    {
        (void)backend_->basic_action(wifi::BasicAction::Stop);
    }
    void reset_data() override
    {
    }
    bool do_action(wifi::BasicAction action, bool is_force = false) override
    {
        (void)is_force;
        return backend_->basic_action(action);
    }
    bool is_action_running(wifi::BasicAction action) override
    {
        (void)action;
        return false;
    }
    bool is_event_ready(wifi::BasicEvent event) override
    {
        std::lock_guard lock(backend_->mutex_);
        switch (event) {
        case wifi::BasicEvent::Inited:
            return backend_->initialized_;
        case wifi::BasicEvent::Started:
            return backend_->started_;
        case wifi::BasicEvent::Stopped:
            return !backend_->started_;
        case wifi::BasicEvent::Deinited:
            return !backend_->initialized_;
        default:
            return false;
        }
    }

private:
    std::shared_ptr<WifiWasmBackend> backend_;
};

class WifiWasmStationIface: public wifi::StationIface {
public:
    explicit WifiWasmStationIface(std::shared_ptr<WifiWasmBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(Callbacks callbacks) override
    {
        return backend_->configure(std::move(callbacks));
    }
    void clear_callbacks() override
    {
        std::lock_guard lock(backend_->mutex_);
        backend_->station_callbacks_ = {};
    }
    bool do_action(wifi::StationAction action, bool is_force = false) override
    {
        (void)is_force;
        return backend_->station_action(action);
    }
    bool is_action_running(wifi::StationAction action) override
    {
        (void)action;
        return false;
    }
    bool is_event_ready(wifi::StationEvent event) override
    {
        std::lock_guard lock(backend_->mutex_);
        switch (event) {
        case wifi::StationEvent::Connected:
            return backend_->connected_;
        case wifi::StationEvent::Disconnected:
            return !backend_->connected_;
        default:
            return false;
        }
    }
    void mark_target_connect_ap_info_connectable(bool connectable) override
    {
        std::lock_guard lock(backend_->mutex_);
        backend_->target_ap_.is_connectable = connectable;
    }
    bool set_target_connect_ap_info(const wifi::ConnectApInfo &ap_info) override
    {
        std::lock_guard lock(backend_->mutex_);
        backend_->target_ap_ = ap_info;
        return true;
    }
    bool set_last_connected_ap_info(const wifi::ConnectApInfo &ap_info) override
    {
        std::lock_guard lock(backend_->mutex_);
        backend_->last_connected_ap_ = ap_info;
        return true;
    }
    bool set_connected_ap_infos(const wifi::ConnectApInfoList &ap_infos) override
    {
        std::lock_guard lock(backend_->mutex_);
        backend_->connected_aps_ = ap_infos;
        return true;
    }
    const wifi::ConnectApInfo &get_target_connect_ap_info() const override
    {
        return backend_->target_ap_;
    }
    const wifi::ConnectApInfo &get_last_connected_ap_info() const override
    {
        return backend_->last_connected_ap_;
    }
    const wifi::ConnectApInfoList &get_connected_ap_infos() const override
    {
        return backend_->connected_aps_;
    }
    bool set_scan_params(const wifi::ScanParams &params) override
    {
        std::lock_guard lock(backend_->mutex_);
        backend_->scan_params_ = params;
        return true;
    }
    const wifi::ScanParams &get_scan_params() const override
    {
        return backend_->scan_params_;
    }
    bool start_scan() override
    {
        std::lock_guard lock(backend_->mutex_);
        backend_->scan_started_ = true;
        if (backend_->station_callbacks_.on_scan_state_changed) {
            backend_->station_callbacks_.on_scan_state_changed(true);
            backend_->station_callbacks_.on_scan_state_changed(false);
        }
        if (backend_->station_callbacks_.on_scan_ap_infos_updated) {
            std::array<wifi::ScanApInfo, 3> aps = {{
                {
                    .ssid = "Brookesia Fake AP",
                    .is_locked = true,
                    .rssi = -42,
                    .signal_level = wifi::ScanApSignalLevel::LEVEL_4,
                    .channel = 6,
                },
                {
                    .ssid = "Wasm Lab",
                    .is_locked = false,
                    .rssi = -56,
                    .signal_level = wifi::ScanApSignalLevel::LEVEL_3,
                    .channel = 11,
                },
                {
                    .ssid = "Offline Simulator",
                    .is_locked = true,
                    .rssi = -68,
                    .signal_level = wifi::ScanApSignalLevel::LEVEL_2,
                    .channel = 1,
                },
            }};
            const size_t count = std::min(aps.size(), backend_->scan_params_.ap_count);
            backend_->station_callbacks_.on_scan_ap_infos_updated(std::span<const wifi::ScanApInfo>(
                        aps.data(), count
                    ));
        }
        backend_->scan_started_ = false;
        return true;
    }
    void stop_scan() override
    {
        std::lock_guard lock(backend_->mutex_);
        backend_->scan_started_ = false;
        if (backend_->station_callbacks_.on_scan_state_changed) {
            backend_->station_callbacks_.on_scan_state_changed(false);
        }
    }

private:
    std::shared_ptr<WifiWasmBackend> backend_;
};

class WifiWasmSoftApIface: public wifi::SoftApIface {
public:
    explicit WifiWasmSoftApIface(std::shared_ptr<WifiWasmBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(Callbacks callbacks) override
    {
        return backend_->configure(std::move(callbacks));
    }
    void clear_callbacks() override
    {
        std::lock_guard lock(backend_->mutex_);
        backend_->softap_callbacks_ = {};
    }
    bool set_params(const wifi::SoftApParams &params) override
    {
        std::lock_guard lock(backend_->mutex_);
        backend_->softap_params_ = params;
        if (backend_->softap_callbacks_.on_params_updated) {
            backend_->softap_callbacks_.on_params_updated(backend_->softap_params_);
        }
        return true;
    }
    const wifi::SoftApParams &get_params() const override
    {
        return backend_->softap_params_;
    }
    bool start() override
    {
        std::lock_guard lock(backend_->mutex_);
        backend_->softap_started_ = true;
        if (backend_->softap_callbacks_.on_event) {
            backend_->softap_callbacks_.on_event(wifi::SoftApEvent::Started);
        }
        return true;
    }
    void stop() override
    {
        std::lock_guard lock(backend_->mutex_);
        backend_->softap_started_ = false;
        if (backend_->softap_callbacks_.on_event) {
            backend_->softap_callbacks_.on_event(wifi::SoftApEvent::Stopped);
        }
    }
    bool start_provision() override
    {
        return start();
    }
    bool stop_provision() override
    {
        stop();
        return true;
    }

private:
    std::shared_ptr<WifiWasmBackend> backend_;
};

class WifiConnectivityWasmImpl: public network::ConnectivityIface {
public:
    explicit WifiConnectivityWasmImpl(std::shared_ptr<WifiWasmBackend> backend)
        : backend_(std::move(backend))
    {
    }

    network::NetworkStatus get_status() const override
    {
        return backend_->get_status();
    }

private:
    std::shared_ptr<WifiWasmBackend> backend_;
};

bool WifiWasmDevice::probe()
{
    return true;
}

std::vector<InterfaceSpec> WifiWasmDevice::get_interface_specs() const
{
    return {
        {wifi::BasicIface::NAME, BASIC_IFACE_NAME},
        {wifi::StationIface::NAME, STATION_IFACE_NAME},
        {wifi::SoftApIface::NAME, SOFTAP_IFACE_NAME},
        {network::ConnectivityIface::NAME, CONNECTIVITY_IFACE_NAME},
    };
}

bool WifiWasmDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    backend_ = std::make_shared<WifiWasmBackend>();
    interfaces_.emplace(BASIC_IFACE_NAME, std::make_shared<WifiWasmBasicIface>(backend_));
    interfaces_.emplace(STATION_IFACE_NAME, std::make_shared<WifiWasmStationIface>(backend_));
    interfaces_.emplace(SOFTAP_IFACE_NAME, std::make_shared<WifiWasmSoftApIface>(backend_));
    interfaces_.emplace(CONNECTIVITY_IFACE_NAME, std::make_shared<WifiConnectivityWasmImpl>(backend_));
    return true;
}

void WifiWasmDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(BASIC_IFACE_NAME);
    interfaces_.erase(STATION_IFACE_NAME);
    interfaces_.erase(SOFTAP_IFACE_NAME);
    interfaces_.erase(CONNECTIVITY_IFACE_NAME);
    if (backend_ != nullptr) {
        backend_->clear_callbacks();
    }
    backend_.reset();
}

bool PowerWasmDevice::probe()
{
    return true;
}

bool PowerWasmDevice::on_init()
{
    return true;
}

void PowerWasmDevice::on_deinit()
{
}

class AudioOutputControlWasm {
public:
    void set_volume(uint8_t volume)
    {
        std::lock_guard lock(mutex_);
        volume_ = std::min<uint8_t>(volume, 100);
    }

    bool set_pa_on_off(bool on)
    {
        std::lock_guard lock(mutex_);
        pa_on_ = on;
        return true;
    }

    bool is_pa_on() const
    {
        std::lock_guard lock(mutex_);
        return pa_on_;
    }

private:
    mutable std::mutex mutex_;
    uint8_t volume_ = 100;
    bool pa_on_ = true;
};

class AudioCodecPlayerWasmImpl: public audio::CodecPlayerIface {
public:
    explicit AudioCodecPlayerWasmImpl(std::shared_ptr<AudioOutputControlWasm> output_control)
        : output_control_(std::move(output_control))
    {
    }

    ~AudioCodecPlayerWasmImpl() override
    {
        close();
    }

    bool open(const Config &config) override
    {
        std::lock_guard lock(mutex_);
        if ((config.bits == 0) || (config.channels == 0) || (config.sample_rate == 0)) {
            BROOKESIA_LOGE(
                "Invalid wasm audio player config: bits(%1%), channels(%2%), sample_rate(%3%)",
                config.bits, config.channels, config.sample_rate
            );
            return false;
        }
        config_ = config;
        is_opened_ = true;
        total_bytes_written_ = 0;
        return true;
    }

    void close() override
    {
        std::lock_guard lock(mutex_);
        is_opened_ = false;
    }

    bool set_volume(uint8_t volume) override
    {
        output_control_->set_volume(volume);
        return true;
    }

    bool write_data(const uint8_t *data, size_t size) override
    {
        std::lock_guard lock(mutex_);
        if ((data == nullptr) || !is_opened_) {
            return false;
        }
        total_bytes_written_ += size;
        return true;
    }

    bool is_pa_on_off_supported() override
    {
        return true;
    }

    bool set_pa_on_off(bool on) override
    {
        return output_control_->set_pa_on_off(on);
    }

    bool is_pa_on() const override
    {
        return output_control_->is_pa_on();
    }

private:
    std::mutex mutex_;
    std::shared_ptr<AudioOutputControlWasm> output_control_;
    Config config_{};
    bool is_opened_ = false;
    size_t total_bytes_written_ = 0;
};

class AudioPlaybackWasmImpl: public audio::PlaybackIface {
public:
    bool open(EventCallback callback) override
    {
        {
            std::lock_guard lock(mutex_);
            callback_ = std::move(callback);
            state_ = audio::PlayState::Idle;
            is_opened_ = true;
        }
        notify(audio::PlayState::Idle);
        return true;
    }

    void close() override
    {
        std::lock_guard lock(mutex_);
        callback_ = nullptr;
        current_url_.clear();
        state_ = audio::PlayState::Idle;
        is_opened_ = false;
    }

    bool play(const std::string &url) override
    {
        if (url.empty()) {
            return false;
        }
        {
            std::lock_guard lock(mutex_);
            if (!is_opened_) {
                return false;
            }
            current_url_ = url;
            state_ = audio::PlayState::Playing;
        }
        notify(audio::PlayState::Playing);
        return true;
    }

    bool pause() override
    {
        {
            std::lock_guard lock(mutex_);
            if (!is_opened_ || (state_ != audio::PlayState::Playing)) {
                return false;
            }
            state_ = audio::PlayState::Paused;
        }
        notify(audio::PlayState::Paused);
        return true;
    }

    bool resume() override
    {
        {
            std::lock_guard lock(mutex_);
            if (!is_opened_ || (state_ != audio::PlayState::Paused)) {
                return false;
            }
            state_ = audio::PlayState::Playing;
        }
        notify(audio::PlayState::Playing);
        return true;
    }

    bool stop() override
    {
        {
            std::lock_guard lock(mutex_);
            if (!is_opened_) {
                return false;
            }
            current_url_.clear();
            state_ = audio::PlayState::Idle;
        }
        notify(audio::PlayState::Idle);
        return true;
    }

    bool is_opened() const override
    {
        std::lock_guard lock(mutex_);
        return is_opened_;
    }

private:
    void notify(audio::PlayState state)
    {
        EventCallback callback;
        {
            std::lock_guard lock(mutex_);
            callback = callback_;
        }
        if (callback) {
            callback(state);
        }
    }

    mutable std::mutex mutex_;
    EventCallback callback_;
    std::string current_url_;
    audio::PlayState state_ = audio::PlayState::Idle;
    bool is_opened_ = false;
};

class AudioEncoderWasmImpl: public audio::EncoderIface {
public:
    std::vector<std::string> get_afe_wake_words() override
    {
        return {"Hi Brookesia"};
    }

    bool start(const audio::EncoderDynamicConfig &config, Callbacks callbacks) override
    {
        if ((config.general.channels == 0) || (config.general.sample_bits == 0) ||
                (config.general.sample_rate == 0) || (config.type == audio::CodecFormat::Max)) {
            return false;
        }
        Callbacks callbacks_copy = callbacks;
        {
            std::lock_guard lock(mutex_);
            config_ = config;
            callbacks_ = std::move(callbacks);
            is_started_ = true;
            is_paused_ = false;
        }
        if (config.enable_afe && callbacks_copy.afe_event) {
            callbacks_copy.afe_event(audio::AfeEvent::WakeStart);
        }
        return true;
    }

    int read_encoded_data(uint8_t *data, size_t size) override
    {
        if ((data == nullptr) || (size == 0)) {
            return -1;
        }

        Callbacks callbacks;
        size_t output_size = 0;
        {
            std::lock_guard lock(mutex_);
            if (!is_started_) {
                return -1;
            }
            if (is_paused_) {
                return 0;
            }
            output_size = std::min<size_t>(size, std::max<size_t>(1, config_.fetch_data_size));
            callbacks = callbacks_;
        }

        std::fill_n(data, output_size, static_cast<uint8_t>(0));
        if (callbacks.recorder_data) {
            callbacks.recorder_data(data, output_size);
        }
        return static_cast<int>(output_size);
    }

    void stop() override
    {
        std::lock_guard lock(mutex_);
        is_started_ = false;
        is_paused_ = false;
        callbacks_ = {};
    }

    void pause() override
    {
        std::lock_guard lock(mutex_);
        if (is_started_) {
            is_paused_ = true;
        }
    }

    void resume() override
    {
        std::lock_guard lock(mutex_);
        if (is_started_) {
            is_paused_ = false;
        }
    }

    bool is_started() const override
    {
        std::lock_guard lock(mutex_);
        return is_started_;
    }

    bool is_paused() const override
    {
        std::lock_guard lock(mutex_);
        return is_paused_;
    }

private:
    mutable std::mutex mutex_;
    audio::EncoderDynamicConfig config_{};
    Callbacks callbacks_{};
    bool is_started_ = false;
    bool is_paused_ = false;
};

class AudioDecoderWasmImpl: public audio::DecoderIface {
public:
    explicit AudioDecoderWasmImpl(std::shared_ptr<AudioCodecPlayerWasmImpl> player)
        : player_(std::move(player))
    {
    }

    bool start(const audio::DecoderDynamicConfig &config) override
    {
        if ((config.general.channels == 0) || (config.general.sample_bits == 0) ||
                (config.general.sample_rate == 0) || (config.type == audio::CodecFormat::Max)) {
            return false;
        }
        auto result = player_->open(audio::CodecPlayerIface::Config{
            .bits = config.general.sample_bits,
            .channels = config.general.channels,
            .sample_rate = config.general.sample_rate,
        });
        is_started_ = result;
        return result;
    }

    void stop() override
    {
        player_->close();
        is_started_ = false;
    }

    bool is_started() const override
    {
        return is_started_;
    }

    bool feed_data(const uint8_t *data, size_t size) override
    {
        return is_started_ && player_->write_data(data, size);
    }

private:
    std::shared_ptr<AudioCodecPlayerWasmImpl> player_;
    bool is_started_ = false;
};

std::string AudioWasmDevice::get_playback_iface_name()
{
    return audio::PlaybackIface::get_default_instance_name();
}

std::string AudioWasmDevice::get_encoder_iface_name(size_t id)
{
    return audio::EncoderIface::get_default_instance_name(id);
}

std::string AudioWasmDevice::get_decoder_iface_name(size_t id)
{
    return audio::DecoderIface::get_default_instance_name(id);
}

bool AudioWasmDevice::probe()
{
    return true;
}

std::vector<InterfaceSpec> AudioWasmDevice::get_interface_specs() const
{
    return {
        {audio::CodecPlayerIface::NAME, PLAYER_IFACE_NAME},
        {audio::PlaybackIface::NAME, get_playback_iface_name()},
        {audio::EncoderIface::NAME, get_encoder_iface_name(0)},
        {audio::DecoderIface::NAME, get_decoder_iface_name(0)},
    };
}

bool AudioWasmDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto output_control = std::make_shared<AudioOutputControlWasm>();
    player_iface_ = std::make_shared<AudioCodecPlayerWasmImpl>(output_control);
    playback_iface_ = std::make_shared<AudioPlaybackWasmImpl>();
    encoder_iface_ = std::make_shared<AudioEncoderWasmImpl>();
    decoder_iface_ = std::make_shared<AudioDecoderWasmImpl>(player_iface_);
    interfaces_.emplace(PLAYER_IFACE_NAME, player_iface_);
    interfaces_.emplace(get_playback_iface_name(), playback_iface_);
    interfaces_.emplace(get_encoder_iface_name(0), encoder_iface_);
    interfaces_.emplace(get_decoder_iface_name(0), decoder_iface_);
    return true;
}

void AudioWasmDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(get_decoder_iface_name(0));
    interfaces_.erase(get_encoder_iface_name(0));
    interfaces_.erase(get_playback_iface_name());
    interfaces_.erase(PLAYER_IFACE_NAME);
    decoder_iface_.reset();
    encoder_iface_.reset();
    playback_iface_.reset();
    player_iface_.reset();
}

void set_video_error(std::string *error_message, std::string message)
{
    if (error_message != nullptr) {
        *error_message = std::move(message);
    }
}

bool validate_video_encoder_config(const video::EncoderConfig &config, std::string *error_message)
{
    if (config.sinks.empty()) {
        set_video_error(error_message, "Encoder sink list is empty");
        return false;
    }
    const auto invalid_sink = std::find_if(config.sinks.begin(), config.sinks.end(), [](const auto &sink) {
        return (sink.format == video::EncoderSinkFormat::Max) || (sink.width == 0) || (sink.height == 0) ||
               (sink.fps == 0);
    });
    if (invalid_sink != config.sinks.end()) {
        set_video_error(error_message, "Encoder sink configuration is invalid");
        return false;
    }
    if (error_message != nullptr) {
        error_message->clear();
    }
    return true;
}

bool validate_video_decoder_config(const video::DecoderConfig &config, std::string *error_message)
{
    if ((config.width == 0) || (config.height == 0) || (config.source_format == video::DecoderSourceFormat::Max) ||
            (config.sink_format == video::DecoderSinkFormat::Max)) {
        set_video_error(error_message, "Decoder configuration is invalid");
        return false;
    }
    if (error_message != nullptr) {
        error_message->clear();
    }
    return true;
}

std::vector<uint8_t> make_fake_video_frame(size_t sink_index, const video::EncoderSinkInfo &sink_info)
{
    const size_t pixel_count = static_cast<size_t>(sink_info.width) * static_cast<size_t>(sink_info.height);
    std::vector<uint8_t> frame;
    if (sink_info.format == video::EncoderSinkFormat::RGB565) {
        frame.resize(pixel_count * 2);
        for (uint16_t y = 0; y < sink_info.height; ++y) {
            for (uint16_t x = 0; x < sink_info.width; ++x) {
                const uint8_t r = static_cast<uint8_t>((x + sink_index * 11) & 0xFF);
                const uint8_t g = static_cast<uint8_t>((y + sink_index * 17) & 0xFF);
                const uint8_t b = static_cast<uint8_t>((x + y + sink_index * 23) & 0xFF);
                const uint16_t pixel = static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
                const size_t offset = (static_cast<size_t>(y) * sink_info.width + x) * 2;
                frame[offset] = static_cast<uint8_t>(pixel & 0xFF);
                frame[offset + 1] = static_cast<uint8_t>(pixel >> 8);
            }
        }
        return frame;
    }
    if ((sink_info.format == video::EncoderSinkFormat::RGB888) ||
            (sink_info.format == video::EncoderSinkFormat::BGR888)) {
        frame.resize(pixel_count * 3);
        for (uint16_t y = 0; y < sink_info.height; ++y) {
            for (uint16_t x = 0; x < sink_info.width; ++x) {
                const size_t offset = (static_cast<size_t>(y) * sink_info.width + x) * 3;
                frame[offset] = static_cast<uint8_t>((x + sink_index * 11) & 0xFF);
                frame[offset + 1] = static_cast<uint8_t>((y + sink_index * 17) & 0xFF);
                frame[offset + 2] = static_cast<uint8_t>((x + y + sink_index * 23) & 0xFF);
            }
        }
        return frame;
    }

    static constexpr std::array<uint8_t, 16> FAKE_ENCODED_FRAME = {
        0x42, 0x52, 0x4f, 0x4f, 0x4b, 0x45, 0x53, 0x49,
        0x41, 0x2d, 0x57, 0x41, 0x53, 0x4d, 0x00, 0x01,
    };
    frame.assign(FAKE_ENCODED_FRAME.begin(), FAKE_ENCODED_FRAME.end());
    frame.back() = static_cast<uint8_t>(sink_index & 0xFF);
    return frame;
}

std::vector<uint8_t> make_fake_decoded_frame(const video::DecoderConfig &config)
{
    const size_t pixel_count = static_cast<size_t>(config.width) * static_cast<size_t>(config.height);
    std::vector<uint8_t> frame;
    if ((config.sink_format == video::DecoderSinkFormat::RGB565_LE) ||
            (config.sink_format == video::DecoderSinkFormat::RGB565_BE)) {
        frame.resize(pixel_count * 2);
    } else if ((config.sink_format == video::DecoderSinkFormat::RGB888) ||
               (config.sink_format == video::DecoderSinkFormat::BGR888)) {
        frame.resize(pixel_count * 3);
    } else {
        frame.resize(std::max<size_t>(16, pixel_count));
    }
    for (size_t i = 0; i < frame.size(); ++i) {
        frame[i] = static_cast<uint8_t>((i * 13) & 0xFF);
    }
    return frame;
}

class VideoCameraWasmImpl: public video::CameraIface {
public:
    std::vector<DeviceInfo> get_device_infos() const override
    {
        return {
            DeviceInfo{
                .id = 0,
                .name = "Wasm Fake Camera",
                .device_path = "wasm://camera0",
                .supported_formats = {
                    video::EncoderSinkFormat::RGB565,
                    video::EncoderSinkFormat::RGB888,
                    video::EncoderSinkFormat::MJPEG,
                },
            },
        };
    }
};

class VideoEncoderWasmImpl: public video::EncoderIface {
public:
    bool open(const video::EncoderConfig &config, FrameCallback callback, std::string *error_message) override
    {
        if (!validate_video_encoder_config(config, error_message)) {
            return false;
        }
        std::lock_guard lock(mutex_);
        config_ = config;
        callback_ = std::move(callback);
        is_opened_ = true;
        is_started_ = false;
        return true;
    }

    void close() override
    {
        std::lock_guard lock(mutex_);
        config_ = {};
        callback_ = nullptr;
        is_opened_ = false;
        is_started_ = false;
    }

    bool start(std::string *error_message) override
    {
        FrameCallback callback;
        video::EncoderConfig config;
        {
            std::lock_guard lock(mutex_);
            if (!is_opened_) {
                set_video_error(error_message, "Encoder is not opened");
                return false;
            }
            is_started_ = true;
            callback = callback_;
            config = config_;
        }
        if (error_message != nullptr) {
            error_message->clear();
        }
        if (config.enable_stream_mode && !config.sinks.empty()) {
            emit_frame(0, config.sinks[0], callback);
        }
        return true;
    }

    bool stop(std::string *error_message) override
    {
        std::lock_guard lock(mutex_);
        if (!is_opened_) {
            set_video_error(error_message, "Encoder is not opened");
            return false;
        }
        is_started_ = false;
        if (error_message != nullptr) {
            error_message->clear();
        }
        return true;
    }

    bool fetch_frame(size_t sink_index, FrameCallback callback, std::string *error_message) override
    {
        FrameCallback selected_callback = std::move(callback);
        video::EncoderSinkInfo sink_info{};
        {
            std::lock_guard lock(mutex_);
            if (!is_opened_ || !is_started_) {
                set_video_error(error_message, "Encoder is not started");
                return false;
            }
            if (sink_index >= config_.sinks.size()) {
                set_video_error(error_message, "Encoder sink index is out of range");
                return false;
            }
            sink_info = config_.sinks[sink_index];
            if (!selected_callback) {
                selected_callback = callback_;
            }
        }
        if (error_message != nullptr) {
            error_message->clear();
        }
        emit_frame(sink_index, sink_info, selected_callback);
        return true;
    }

    bool is_opened() const override
    {
        std::lock_guard lock(mutex_);
        return is_opened_;
    }

    bool is_started() const override
    {
        std::lock_guard lock(mutex_);
        return is_started_;
    }

private:
    static void emit_frame(size_t sink_index, const video::EncoderSinkInfo &sink_info, const FrameCallback &callback)
    {
        if (!callback) {
            return;
        }
        auto frame = make_fake_video_frame(sink_index, sink_info);
        callback(sink_index, sink_info, frame.data(), frame.size());
    }

    mutable std::mutex mutex_;
    video::EncoderConfig config_{};
    FrameCallback callback_;
    bool is_opened_ = false;
    bool is_started_ = false;
};

class VideoDecoderWasmImpl: public video::DecoderIface {
public:
    bool open(const video::DecoderConfig &config, FrameCallback callback, std::string *error_message) override
    {
        if (!validate_video_decoder_config(config, error_message)) {
            return false;
        }
        std::lock_guard lock(mutex_);
        config_ = config;
        callback_ = std::move(callback);
        is_opened_ = true;
        is_started_ = false;
        return true;
    }

    void close() override
    {
        std::lock_guard lock(mutex_);
        config_ = {};
        callback_ = nullptr;
        is_opened_ = false;
        is_started_ = false;
    }

    bool start(std::string *error_message) override
    {
        std::lock_guard lock(mutex_);
        if (!is_opened_) {
            set_video_error(error_message, "Decoder is not opened");
            return false;
        }
        is_started_ = true;
        if (error_message != nullptr) {
            error_message->clear();
        }
        return true;
    }

    bool stop(std::string *error_message) override
    {
        std::lock_guard lock(mutex_);
        if (!is_opened_) {
            set_video_error(error_message, "Decoder is not opened");
            return false;
        }
        is_started_ = false;
        if (error_message != nullptr) {
            error_message->clear();
        }
        return true;
    }

    bool feed_frame(const uint8_t *data, size_t size, std::string *error_message) override
    {
        FrameCallback callback;
        video::DecoderConfig config;
        {
            std::lock_guard lock(mutex_);
            if (!is_opened_ || !is_started_) {
                set_video_error(error_message, "Decoder is not started");
                return false;
            }
            if ((data == nullptr) || (size == 0)) {
                set_video_error(error_message, "Decoder source frame is empty");
                return false;
            }
            callback = callback_;
            config = config_;
        }
        if (error_message != nullptr) {
            error_message->clear();
        }
        if (callback) {
            auto frame = make_fake_decoded_frame(config);
            callback(config.width, config.height, frame.data(), frame.size());
        }
        return true;
    }

    bool is_opened() const override
    {
        std::lock_guard lock(mutex_);
        return is_opened_;
    }

    bool is_started() const override
    {
        std::lock_guard lock(mutex_);
        return is_started_;
    }

private:
    mutable std::mutex mutex_;
    video::DecoderConfig config_{};
    FrameCallback callback_;
    bool is_opened_ = false;
    bool is_started_ = false;
};

std::string VideoWasmDevice::get_camera_iface_name(size_t id)
{
    return video::CameraIface::get_default_instance_name(id);
}

std::string VideoWasmDevice::get_encoder_iface_name(size_t id)
{
    return video::EncoderIface::get_default_instance_name(id);
}

std::string VideoWasmDevice::get_decoder_iface_name(size_t id)
{
    return video::DecoderIface::get_default_instance_name(id);
}

bool VideoWasmDevice::probe()
{
    return true;
}

std::vector<InterfaceSpec> VideoWasmDevice::get_interface_specs() const
{
    return {
        {video::CameraIface::NAME, get_camera_iface_name(0)},
        {video::EncoderIface::NAME, get_encoder_iface_name(0)},
        {video::DecoderIface::NAME, get_decoder_iface_name(0)},
    };
}

bool VideoWasmDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    camera_iface_ = std::make_shared<VideoCameraWasmImpl>();
    encoder_iface_ = std::make_shared<VideoEncoderWasmImpl>();
    decoder_iface_ = std::make_shared<VideoDecoderWasmImpl>();
    interfaces_.emplace(get_camera_iface_name(0), camera_iface_);
    interfaces_.emplace(get_encoder_iface_name(0), encoder_iface_);
    interfaces_.emplace(get_decoder_iface_name(0), decoder_iface_);
    return true;
}

void VideoWasmDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(get_decoder_iface_name(0));
    interfaces_.erase(get_encoder_iface_name(0));
    interfaces_.erase(get_camera_iface_name(0));
    decoder_iface_.reset();
    encoder_iface_.reset();
    camera_iface_.reset();
}

#if BROOKESIA_HAL_WASM_ENABLE_DISPLAY_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, DisplayWasmDevice, DisplayWasmDevice::DEVICE_NAME, DisplayWasmDevice::get_instance(),
    BROOKESIA_HAL_WASM_DISPLAY_DEVICE_PLUGIN_SYMBOL
);
#endif

#if BROOKESIA_HAL_WASM_ENABLE_STORAGE_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, StorageWasmDevice, StorageWasmDevice::DEVICE_NAME, StorageWasmDevice::get_instance(),
    BROOKESIA_HAL_WASM_STORAGE_DEVICE_PLUGIN_SYMBOL
);
#endif

#if BROOKESIA_HAL_WASM_ENABLE_SYSTEM_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, SystemWasmDevice, SystemWasmDevice::DEVICE_NAME, SystemWasmDevice::get_instance(),
    BROOKESIA_HAL_WASM_SYSTEM_DEVICE_PLUGIN_SYMBOL
);
#endif

#if BROOKESIA_HAL_WASM_ENABLE_NETWORK_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, NetworkWasmDevice, NetworkWasmDevice::DEVICE_NAME, NetworkWasmDevice::get_instance(),
    BROOKESIA_HAL_WASM_NETWORK_DEVICE_PLUGIN_SYMBOL
);
#endif

#if BROOKESIA_HAL_WASM_ENABLE_WIFI_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, WifiWasmDevice, WifiWasmDevice::DEVICE_NAME, WifiWasmDevice::get_instance(),
    BROOKESIA_HAL_WASM_WIFI_DEVICE_PLUGIN_SYMBOL
);
#endif

#if BROOKESIA_HAL_WASM_ENABLE_POWER_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, PowerWasmDevice, PowerWasmDevice::DEVICE_NAME, PowerWasmDevice::get_instance(),
    BROOKESIA_HAL_WASM_POWER_DEVICE_PLUGIN_SYMBOL
);
#endif

#if BROOKESIA_HAL_WASM_ENABLE_AUDIO_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, AudioWasmDevice, AudioWasmDevice::DEVICE_NAME, AudioWasmDevice::get_instance(),
    BROOKESIA_HAL_WASM_AUDIO_DEVICE_PLUGIN_SYMBOL
);
#endif

#if BROOKESIA_HAL_WASM_ENABLE_VIDEO_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, VideoWasmDevice, VideoWasmDevice::DEVICE_NAME, VideoWasmDevice::get_instance(),
    BROOKESIA_HAL_WASM_VIDEO_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
