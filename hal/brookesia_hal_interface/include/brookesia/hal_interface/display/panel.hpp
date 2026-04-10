/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <string_view>
#include <utility>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

/**
 * @file panel.hpp
 * @brief Declares the display panel HAL interface.
 */

namespace esp_brookesia::hal {

/**
 * @brief Display panel interface for rendering pixel data.
 */
class DisplayPanelIface: public Interface {
public:
    static constexpr const char *NAME = "DisplayPanel";  ///< Interface registry name.

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
     * @brief Driver-specific data.
     */
    struct DriverSpecific {
        void *io_handle = nullptr;    ///< Handle of the IO.
        void *panel_handle = nullptr; ///< Handle of the panel.
        BusType bus_type = BusType::Max; ///< Bus type.
    };

    /**
     * @brief Construct a display panel interface.
     *
     * @param[in] info Static panel capability information.
     */
    DisplayPanelIface(Info info)
        : Interface(NAME)
        , info_(std::move(info))
    {
    }

    /**
     * @brief Virtual destructor for polymorphic panel interfaces.
     */
    virtual ~DisplayPanelIface() = default;

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
     * @brief Get the driver-specific data.
     *
     * @param[out] specific The driver-specific data.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool get_driver_specific(DriverSpecific &specific)
    {
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

BROOKESIA_DESCRIBE_ENUM(DisplayPanelIface::PixelFormat, RGB565, RGB888, Max);
BROOKESIA_DESCRIBE_ENUM(DisplayPanelIface::BusType, Generic, RGB, MIPI, Max);
BROOKESIA_DESCRIBE_STRUCT(DisplayPanelIface::Info, (), (h_res, v_res, pixel_format));

} // namespace esp_brookesia::hal
