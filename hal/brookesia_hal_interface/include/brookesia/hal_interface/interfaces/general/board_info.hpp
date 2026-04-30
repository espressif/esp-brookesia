/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <utility>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

/**
 * @file board_info.hpp
 * @brief Declares the board information HAL interface.
 */

namespace esp_brookesia::hal {

/**
 * @brief Board information interface for querying static board metadata.
 */
class BoardInfoIface: public Interface {
public:
    static constexpr const char *NAME = "BoardInfo";  ///< Interface registry name.

    /**
     * @brief Static board information.
     */
    struct Info {
        std::string name;          ///< Board name.
        std::string chip;          ///< Main chip model.
        std::string version;       ///< Board hardware or BOM version.
        std::string description;   ///< Human-readable board description.
        std::string manufacturer;  ///< Board manufacturer.

        /**
         * @brief Check whether the board information contains useful identity fields.
         *
         * @return `true` if either board name or chip is non-empty; otherwise `false`.
         */
        bool is_valid() const
        {
            return !name.empty() || !chip.empty();
        }
    };

    /**
     * @brief Construct a board information interface.
     *
     * @param[in] info Static board information.
     */
    explicit BoardInfoIface(Info info)
        : Interface(NAME)
        , info_(std::move(info))
    {
    }

    /**
     * @brief Virtual destructor for polymorphic board information interfaces.
     */
    virtual ~BoardInfoIface() = default;

    /**
     * @brief Get static board information.
     *
     * @return Board information.
     */
    const Info &get_info() const
    {
        return info_;
    }

private:
    Info info_{};
};

BROOKESIA_DESCRIBE_STRUCT(BoardInfoIface::Info, (), (name, chip, version, description, manufacturer));

} // namespace esp_brookesia::hal
