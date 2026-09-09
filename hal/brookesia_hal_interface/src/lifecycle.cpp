/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <boost/thread/recursive_mutex.hpp>
#include "brookesia/hal_interface/lifecycle.h"

namespace {

boost::recursive_mutex &get_lifecycle_mutex()
{
    static boost::recursive_mutex mutex;
    return mutex;
}

} // namespace

extern "C" void brookesia_hal_lifecycle_lock(void)
{
    get_lifecycle_mutex().lock();
}

extern "C" void brookesia_hal_lifecycle_unlock(void)
{
    get_lifecycle_mutex().unlock();
}
