/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef ESP_PLATFORM

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <pthread.h>

#include "sdkconfig.h"

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace {

constexpr uint32_t MUTEX_ALLOC_CAPS = MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT;

struct WrappedPthreadMutex {
    SemaphoreHandle_t sem;
    int type;
    WrappedPthreadMutex *next;
};

portMUX_TYPE s_registry_lock = portMUX_INITIALIZER_UNLOCKED;
WrappedPthreadMutex *s_mutex_registry = nullptr;

bool is_supported_mutex_type(int type)
{
    return type == PTHREAD_MUTEX_NORMAL || type == PTHREAD_MUTEX_RECURSIVE || type == PTHREAD_MUTEX_ERRORCHECK;
}

void registry_add(WrappedPthreadMutex *mutex)
{
    portENTER_CRITICAL(&s_registry_lock);
    mutex->next = s_mutex_registry;
    s_mutex_registry = mutex;
    portEXIT_CRITICAL(&s_registry_lock);
}

bool registry_contains(const WrappedPthreadMutex *mutex)
{
    bool found = false;

    portENTER_CRITICAL(&s_registry_lock);
    for (const WrappedPthreadMutex *current = s_mutex_registry; current != nullptr; current = current->next) {
        if (current == mutex) {
            found = true;
            break;
        }
    }
    portEXIT_CRITICAL(&s_registry_lock);

    return found;
}

void registry_remove(WrappedPthreadMutex *mutex)
{
    portENTER_CRITICAL(&s_registry_lock);
    WrappedPthreadMutex **current = &s_mutex_registry;
    while (*current != nullptr) {
        if (*current == mutex) {
            *current = mutex->next;
            break;
        }
        current = &(*current)->next;
    }
    portEXIT_CRITICAL(&s_registry_lock);
}

int mutex_lock_internal(WrappedPthreadMutex *mutex, TickType_t timeout)
{
    if (mutex == nullptr) {
        return EINVAL;
    }

    if (mutex->type == PTHREAD_MUTEX_ERRORCHECK &&
            xSemaphoreGetMutexHolder(mutex->sem) == xTaskGetCurrentTaskHandle()) {
        return EDEADLK;
    }

    if (mutex->type == PTHREAD_MUTEX_RECURSIVE) {
        if (xSemaphoreTakeRecursive(mutex->sem, timeout) != pdTRUE) {
            return EBUSY;
        }
    } else if (xSemaphoreTake(mutex->sem, timeout) != pdTRUE) {
        return EBUSY;
    }

    return 0;
}

} // namespace

extern "C" int __real_pthread_mutex_destroy(pthread_mutex_t *mutex);

extern "C" int __wrap_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    int type = PTHREAD_MUTEX_NORMAL;

    if (mutex == nullptr) {
        return EINVAL;
    }

    if (attr != nullptr) {
        if (!attr->is_initialized || !is_supported_mutex_type(attr->type)) {
            return EINVAL;
        }
        type = attr->type;
    }

    auto *wrapped_mutex = static_cast<WrappedPthreadMutex *>(
                              heap_caps_malloc(sizeof(WrappedPthreadMutex), MUTEX_ALLOC_CAPS)
                          );
    if (wrapped_mutex == nullptr) {
        return ENOMEM;
    }

    wrapped_mutex->type = type;
    wrapped_mutex->next = nullptr;
    if (type == PTHREAD_MUTEX_RECURSIVE) {
        wrapped_mutex->sem = xSemaphoreCreateRecursiveMutexWithCaps(MUTEX_ALLOC_CAPS);
    } else {
        wrapped_mutex->sem = xSemaphoreCreateMutexWithCaps(MUTEX_ALLOC_CAPS);
    }

    if (wrapped_mutex->sem == nullptr) {
        free(wrapped_mutex);
        return EAGAIN;
    }

    registry_add(wrapped_mutex);
    *mutex = static_cast<pthread_mutex_t>(reinterpret_cast<uintptr_t>(wrapped_mutex));

    return 0;
}

extern "C" int __wrap_pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    if (mutex == nullptr) {
        return EINVAL;
    }

    if (static_cast<intptr_t>(*mutex) == PTHREAD_MUTEX_INITIALIZER) {
        return 0;
    }

    auto *wrapped_mutex = reinterpret_cast<WrappedPthreadMutex *>(static_cast<uintptr_t>(*mutex));
    if (wrapped_mutex == nullptr) {
        return EINVAL;
    }

    if (!registry_contains(wrapped_mutex)) {
        return __real_pthread_mutex_destroy(mutex);
    }

    const int lock_result = mutex_lock_internal(wrapped_mutex, 0);
    if (lock_result == EBUSY) {
        return EBUSY;
    }

    int give_result = pdFALSE;
    if (wrapped_mutex->type == PTHREAD_MUTEX_RECURSIVE) {
        give_result = xSemaphoreGiveRecursive(wrapped_mutex->sem);
    } else {
        give_result = xSemaphoreGive(wrapped_mutex->sem);
    }
    assert(give_result == pdTRUE && "Failed to release mutex");

    registry_remove(wrapped_mutex);
    vSemaphoreDeleteWithCaps(wrapped_mutex->sem);
    free(wrapped_mutex);
    *mutex = 0;

    return 0;
}

#endif // ESP_PLATFORM
