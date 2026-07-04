/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <shared_mutex>
#include "boost/thread/condition_variable.hpp"
#include "boost/thread/mutex.hpp"

namespace boost {

using shared_mutex = std::shared_mutex;
using std::shared_lock;

} // namespace boost
