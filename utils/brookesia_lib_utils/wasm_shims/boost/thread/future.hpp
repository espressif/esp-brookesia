/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <future>

namespace boost {

using std::future;
using std::future_error;
using future_status = std::future_status;
using std::promise;
using std::shared_future;

} // namespace boost
