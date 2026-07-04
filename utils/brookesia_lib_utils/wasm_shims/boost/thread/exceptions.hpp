/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <exception>

namespace boost {

class thread_interrupted: public std::exception {
public:
    const char *what() const noexcept override
    {
        return "thread interrupted";
    }
};

} // namespace boost
