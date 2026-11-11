/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <dlfcn.h>
#include <pthread.h>

extern "C" int pthread_create(pthread_t *thread, const pthread_attr_t *attributes,
                              void *(*entry)(void *), void *argument)
{
    static std::atomic_bool injected = false;
    if (std::getenv("GSP_TEST_FAIL_CLEANUP_THREAD") && !injected.exchange(true)) {
        return EAGAIN;
    }
    using Create = int (*)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
    static const auto real_create = reinterpret_cast<Create>(dlsym(RTLD_NEXT, "pthread_create"));
    return real_create(thread, attributes, entry, argument);
}
