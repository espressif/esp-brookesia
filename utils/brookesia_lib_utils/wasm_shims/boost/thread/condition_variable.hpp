/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <condition_variable>

namespace boost {

using condition_variable = std::condition_variable;
using condition_variable_any = std::condition_variable_any;
using cv_status = std::cv_status;

} // namespace boost
