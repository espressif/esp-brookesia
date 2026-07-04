/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

/**
 * @file panel.hpp
 * @brief Declares the display panel HAL interface.
 */

namespace esp_brookesia::hal::display {

/**
 * @brief Display panel interface for rendering pixel data.
 */
class PanelIface: public Interface {
public:
    static constexpr const char *NAME = "DisplayPanel";  ///< Interface registry name.
    static constexpr uint32_t DEFAULT_SYNC_DRAW_TIMEOUT_MS = 5000; ///< Default timeout for synchronous draws.

    /**
     * @brief Pixel format enum.
     */
    enum class PixelFormat : uint8_t {
        RGB565,
        RGB888,
        Max,
    };

    /**
     * @brief Static panel capability information.
     */
    struct Info {
        uint16_t h_res = 0;        ///< Horizontal resolution in pixels.
        uint16_t v_res = 0;        ///< Vertical resolution in pixels.
        PixelFormat pixel_format = PixelFormat::Max; ///< Pixel format.
        std::string group_id;      ///< Stable physical display group identifier.

        /**
         * @brief Get the number of bits per pixel
         *
         * @return Number of bits per pixel.
         */
        uint8_t get_pixel_bits() const
        {
            switch (pixel_format) {
            case PixelFormat::RGB565:
                return 16;
            case PixelFormat::RGB888:
                return 24;
            default:
                return 0;
            }
        }

        /**
         * @brief Check if the panel information is valid.
         *
         * @return `true` if the panel information is valid; otherwise `false`.
         */
        bool is_valid() const
        {
            return (h_res > 0) && (v_res > 0) && (pixel_format != PixelFormat::Max);
        }
    };

    /**
     * @brief Driver-specific bus type enum.
     */
    enum class BusType : uint8_t {
        Generic, /// < Generic bus type, such as SPI, I80, QSPI, etc.
        RGB,     /// < RGB bus type.
        MIPI,    /// < MIPI bus type.
        Max,
    };

    /**
     * @brief Panel backend event type.
     */
    enum class EventType : uint8_t {
        ColorTransDone, ///< The caller-provided color buffer can be reused.
        FrameDone,      ///< A frame buffer or frame refresh event has completed.
        Max,
    };

    using EventCallback = bool (*)(uint8_t event, bool in_isr, void *user_ctx);

    /**
     * @brief Driver event dispatcher for sharing backend LCD callbacks.
     */
    struct EventDispatcher {
        void *ctx = nullptr; ///< Dispatcher owner context.
        bool (*add_listener)(void *ctx, EventCallback callback, void *user_ctx, uint32_t *listener_id) = nullptr;
        bool (*remove_listener)(void *ctx, uint32_t listener_id) = nullptr;

        bool is_valid() const
        {
            return (ctx != nullptr) && (add_listener != nullptr) && (remove_listener != nullptr);
        }
    };

    /**
     * @brief Driver-specific data.
     */
    struct DriverSpecific {
        void *io_handle = nullptr;    ///< Handle of the IO.
        void *panel_handle = nullptr; ///< Handle of the panel.
        BusType bus_type = BusType::Max; ///< Bus type.
        uint8_t frame_buffer_count = 0; ///< Number of panel frame buffers.
        uint8_t draw_x_align_bytes = 1;  ///< X-axis coordinate alignment bytes.
        uint8_t draw_y_align_bytes = 1;  ///< Y-axis coordinate alignment bytes.
        EventDispatcher event_dispatcher; ///< Optional LCD event dispatcher.
    };

    /**
     * @brief Construct a display panel interface.
     *
     * @param[in] info Static panel capability information.
     */
    PanelIface(Info info)
        : Interface(NAME)
        , info_(std::move(info))
    {
    }

    /**
     * @brief Virtual destructor for polymorphic panel interfaces.
     */
    virtual ~PanelIface() = default;

    /**
     * @brief Draw a bitmap into a rectangular panel region.
     *
     * The region follows half-open bounds semantics: `[x1, x2)` and `[y1, y2)`.
     *
     * @param[in] x1 Left coordinate.
     * @param[in] y1 Top coordinate.
     * @param[in] x2 Right coordinate (exclusive).
     * @param[in] y2 Bottom coordinate (exclusive).
     * @param[in] data Pixel buffer pointer.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data) = 0;

    /**
     * @brief Draw a bitmap and optionally wait until the panel backend has finished using the input buffer.
     *
     * The default implementation falls back to @c draw_bitmap(). Backends that perform asynchronous transfers
     * should override this method and only return after the transfer completion signal has been observed, or after
     * @p timeout_ms expires. When @p timeout_ms is `0`, the call only submits the draw and does not wait for a
     * completion signal.
     *
     * @param[in] x1 Left coordinate.
     * @param[in] y1 Top coordinate.
     * @param[in] x2 Right coordinate (exclusive).
     * @param[in] y2 Bottom coordinate (exclusive).
     * @param[in] data Pixel buffer pointer.
     * @param[in] timeout_ms Maximum time to wait in milliseconds. `0` means no wait.
     * @return `true` on completion or no-wait submission; otherwise `false`.
     */
    virtual bool draw_bitmap_sync(
        uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data,
        uint32_t timeout_ms = DEFAULT_SYNC_DRAW_TIMEOUT_MS
    )
    {
        (void)timeout_ms;
        return draw_bitmap(x1, y1, x2, y2, data);
    }

    /**
     * @brief Get the driver-specific data.
     *
     * @param[out] specific The driver-specific data.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool get_driver_specific(DriverSpecific &specific)
    {
        (void)specific;
        return false;
    }

    /**
     * @brief Get static panel capability information.
     *
     * @return Panel information.
     */
    const Info &get_info() const
    {
        return info_;
    }

private:
    Info info_{};
};

BROOKESIA_DESCRIBE_ENUM(PanelIface::PixelFormat, RGB565, RGB888, Max);
BROOKESIA_DESCRIBE_ENUM(PanelIface::BusType, Generic, RGB, MIPI, Max);
BROOKESIA_DESCRIBE_ENUM(PanelIface::EventType, ColorTransDone, FrameDone, Max);
BROOKESIA_DESCRIBE_STRUCT(PanelIface::Info, (), (h_res, v_res, pixel_format, group_id));
BROOKESIA_DESCRIBE_STRUCT(
    PanelIface::DriverSpecific, (),
    (io_handle, panel_handle, bus_type, frame_buffer_count, draw_x_align_bytes, draw_y_align_bytes)
);

} // namespace esp_brookesia::hal::display
