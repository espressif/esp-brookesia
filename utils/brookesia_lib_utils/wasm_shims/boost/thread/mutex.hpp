/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <mutex>

namespace boost {

using mutex = std::mutex;
using recursive_mutex = std::recursive_mutex;
using std::lock_guard;
using std::unique_lock;

} // namespace boost
