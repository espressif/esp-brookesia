/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <map>
#include "nvs_flash.h"
#include "nvs.h"
#include "private/esp_brookesia_service_storage_nvs_utils.hpp"
#include "esp_brookesia_service_storage_nvs.hpp"

#define STORAGE_NVS_PARTITION_NAME          NVS_DEFAULT_PART_NAME
#define STORAGE_NVS_NAMESPACE               "storage"

#define EVENT_THREAD_NAME                   "storage_nvs"
#define EVENT_THREAD_STACK_SIZE             (4 * 1024)
#define EVENT_THREAD_STACK_CAPS_EXT         (false)
#define EVENT_WAIT_FINISH_TIMEOUT_MS_MAX    (60 * 60 * 1000)

namespace esp_brookesia::services {

static const std::map<nvs_type_t, const char *> type_str_pair = {
    { NVS_TYPE_I8, "i8" },
    { NVS_TYPE_U8, "u8" },
    { NVS_TYPE_U16, "u16" },
    { NVS_TYPE_I16, "i16" },
    { NVS_TYPE_U32, "u32" },
    { NVS_TYPE_I32, "i32" },
    { NVS_TYPE_U64, "u64" },
    { NVS_TYPE_I64, "i64" },
    { NVS_TYPE_STR, "str" },
    { NVS_TYPE_BLOB, "blob" },
    { NVS_TYPE_ANY, "any" },
};

void StorageNVS::Event::dump() const
{
    ESP_UTILS_LOGI(
        "{Event}:\n"
        "\t-Operation(%d)\n"
        "\t-Key(%s)\n",
        static_cast<int>(operation),
        key.empty() ? "None" : key.c_str()
    );
}

bool StorageNVS::begin()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    {
        esp_utils::thread_config_guard thread_config(esp_utils::ThreadConfig{
            .name = EVENT_THREAD_NAME,
            .stack_size = EVENT_THREAD_STACK_SIZE,
            .stack_in_ext = EVENT_THREAD_STACK_CAPS_EXT,
        });
        _event_thread = boost::thread([this]() {
            ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

            // Initialize NVS flash
            esp_err_t ret = nvs_flash_init();
            if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) || (ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
                ESP_UTILS_CHECK_FALSE_RETURN(nvs_flash_erase() == ESP_OK, false, "Erase NVS flash failed");
                ESP_UTILS_CHECK_FALSE_RETURN(nvs_flash_init() == ESP_OK, false, "Init NVS flash failed");
            } else {
                ESP_UTILS_CHECK_FALSE_RETURN(ret == ESP_OK, false, "Initialize NVS flash failed");
            }

            while (true) {
                std::unique_lock<std::mutex> lock(_event_mutex);
                _event_cv.wait(lock, [this] {
                    return !_event_queue.empty();
                });

                while (!_event_queue.empty()) {
                    auto event_wrapper = _event_queue.front();
                    _event_queue.pop();

                    lock.unlock();
                    auto ret = processEvent(event_wrapper.event);
                    lock.lock();

                    if (event_wrapper.promise != nullptr) {
                        event_wrapper.promise->set_value(ret);
                    }
                }
            }
        });
    }

    // Initialize NVS parameters
    sendEvent({
        .operation = Operation::UpdateParam
    }, -1);

    return true;
}

bool StorageNVS::setLocalParam(const Key &key, const Value &value, std::optional<int> wait_finish_timeout_ms)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: key(%s), value(%s), wait_finish_timeout_ms(%s)", key.c_str(),
                   std::holds_alternative<int>(value) ?
                   std::to_string(std::get<int>(value)).c_str() : std::get<std::string>(value).c_str(),
                   wait_finish_timeout_ms.has_value() ? std::to_string(wait_finish_timeout_ms.value()).c_str() : "None"
                  );

    {
        std::lock_guard<std::mutex> lock(_params_mutex);
        _local_params[key] = value;
    }

    if (wait_finish_timeout_ms.has_value()) {
        ESP_UTILS_CHECK_FALSE_RETURN(sendEvent({
            .operation = Operation::UpdateNVS,
            .key = key,
        }, wait_finish_timeout_ms.value()), false, "Send storage NVS event failed");
    }

    return true;
}

bool StorageNVS::getLocalParam(const Key &key, Value &value)
{
    std::lock_guard<std::mutex> lock(_params_mutex);

    auto it = _local_params.find(key);
    if (it == _local_params.end()) {
        ESP_UTILS_LOGW("NVS key(%s) not found", key.c_str());
        return false;
    }

    value = it->second;

    return true;
}

bool StorageNVS::eraseNVS(std::optional<int> wait_finish_timeout_ms)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD(
        "Param: wait_finish_timeout_ms(%s)", wait_finish_timeout_ms.has_value() ?
        std::to_string(wait_finish_timeout_ms.value()).c_str() : "None"
    );

    ESP_UTILS_CHECK_FALSE_RETURN(sendEvent({
        .operation = Operation::EraseNVS,
    }, wait_finish_timeout_ms.has_value() ? wait_finish_timeout_ms.value() : 0), false, "Send storage NVS event failed");

    return true;
}

bool StorageNVS::sendEvent(const Event &event, int wait_finish_timeout_ms)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: event(%p), wait_finish_timeout_ms(%d)", &event, wait_finish_timeout_ms);
#if ESP_UTILS_CONF_LOG_LEVEL == ESP_UTILS_LOG_LEVEL_DEBUG
    event.dump();
#endif

    EventWrapper event_wrapper = {
        .event = event,
    };
    if (wait_finish_timeout_ms != 0) {
        event_wrapper.promise = std::make_shared<EventPromise>();
    }
    if (wait_finish_timeout_ms < 0) {
        wait_finish_timeout_ms = EVENT_WAIT_FINISH_TIMEOUT_MS_MAX;
    }

    {
        std::lock_guard<std::mutex> lock(_event_mutex);
        _event_queue.push(event_wrapper);
        _event_cv.notify_one();
    }

    if (event_wrapper.promise == nullptr) {
        ESP_UTILS_LOGD("Don't wait event finish");
        return true;
    }

    ESP_UTILS_LOGD(
        "Wait event finish: %d, timeout_ms(%d)", static_cast<int>(event_wrapper.event.operation),
        wait_finish_timeout_ms
    );
    auto future = event_wrapper.promise->get_future();
    ESP_UTILS_CHECK_FALSE_RETURN(
        future.wait_for(std::chrono::milliseconds(wait_finish_timeout_ms)) == std::future_status::ready,
        false, "Wait for event finished timeout"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(future.get(), false, "Event operation failed");

    return true;
}

bool StorageNVS::processEvent(const Event &event)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: event(%p)", &event);
#if ESP_UTILS_CONF_LOG_LEVEL == ESP_UTILS_LOG_LEVEL_DEBUG
    event.dump();
#endif

    switch (event.operation) {
    case Operation::UpdateNVS: {
        ESP_UTILS_CHECK_FALSE_RETURN(doEventOperationUpdateNVS(event.key), false, "Update NVS failed");
        break;
    }
    case Operation::UpdateParam: {
        ESP_UTILS_CHECK_FALSE_RETURN(doEventOperationUpdateParam(), false, "Update param failed");
        break;
    }
    case Operation::EraseNVS: {
        ESP_UTILS_CHECK_FALSE_RETURN(doEventOperationEraseNVS(), false, "Erase NVS failed");
        break;
    }
    default:
        ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Invalid operation(%d)", static_cast<int>(event.operation));
    }

    return true;
}

bool StorageNVS::doEventOperationUpdateNVS(const Key &key)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: key(%s)", key.c_str());

    std::lock_guard<std::mutex> lock(_params_mutex);

    auto it = _local_params.find(key);
    ESP_UTILS_CHECK_FALSE_RETURN(
        it != _local_params.end(), false, "Invalid NVS key(%s)", key.c_str()
    );
    ESP_UTILS_LOGD("Update key(%s) NVS parameter", key.c_str());

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(STORAGE_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    ESP_UTILS_CHECK_FALSE_RETURN(ret == ESP_OK, false, "Open NVS namespace failed");

    auto &value = it->second;
    const char *key_str = key.c_str();

    if (std::holds_alternative<int>(value)) {
        auto value_int = std::get<int>(value);
        ESP_UTILS_LOGD("Set key(%s) value(%d)", key_str, value_int);

        ret = nvs_set_i32(nvs_handle, key_str, static_cast<int32_t>(value_int));
        ESP_UTILS_CHECK_FALSE_GOTO(ret == ESP_OK, end, "Set NVS parameter failed");
    } else if (std::holds_alternative<std::string>(value)) {
        auto value_str = std::get<std::string>(value);
        ESP_UTILS_LOGD("Set key(%s) value(%s)", key_str, value_str.c_str());

        ret = nvs_set_str(nvs_handle, key_str, value_str.c_str());
        ESP_UTILS_CHECK_FALSE_GOTO(ret == ESP_OK, end, "Set NVS parameter failed");
    } else {
        ESP_UTILS_CHECK_FALSE_GOTO(false, end, "Invalid NVS key(%s) value type", key_str);
    }

    ret = nvs_commit(nvs_handle);
    ESP_UTILS_CHECK_FALSE_RETURN(ret == ESP_OK, false, "Commit NVS failed");

end:
    nvs_close(nvs_handle);

    if (ret != ESP_OK) {
        ESP_UTILS_LOGE("Failed reason(%s)", esp_err_to_name(ret));
    }

    return (ret == ESP_OK);
}

bool StorageNVS::doEventOperationUpdateParam()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard<std::mutex> lock(_params_mutex);

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(STORAGE_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    ESP_UTILS_CHECK_FALSE_RETURN(ret == ESP_OK, false, "Open NVS namespace failed(%s)", esp_err_to_name(ret));

    ESP_UTILS_LOGI("Finding keys in NVS...");

    nvs_iterator_t it = NULL;
    esp_err_t res = nvs_entry_find(STORAGE_NVS_PARTITION_NAME, STORAGE_NVS_NAMESPACE, NVS_TYPE_ANY, &it);
    while (res == ESP_OK) {
        nvs_entry_info_t info;
        res = nvs_entry_info(it, &info);
        if (res != ESP_OK) {
            ESP_UTILS_LOGE("Get key info failed");
            break;
        }

        auto type_str_it = type_str_pair.find(info.type);
        if (type_str_it == type_str_pair.end()) {
            ESP_UTILS_LOGE("\t- Invalid NVS key(%s) type(%d)", info.key, static_cast<int>(info.type));
            res = nvs_entry_next(&it);
            continue;
        }

        switch (info.type) {
        case NVS_TYPE_I32: {
            int32_t value_int;
            ret = nvs_get_i32(nvs_handle, info.key, &value_int);
            if (ret != ESP_OK) {
                ESP_UTILS_LOGE("\t- Get key(%s) value failed", info.key);
            } else {
                ESP_UTILS_LOGI(
                    "\t- Found key(%s): type(%s), value(%d)", info.key, type_str_it->second, static_cast<int>(value_int)
                );
                _local_params[info.key] = Value(static_cast<int>(value_int));
            }
            break;
        }
        case NVS_TYPE_STR: {
            size_t len = NVS_VALUE_STR_MAX_LEN;
            std::unique_ptr<char[]> value_str(new char[len]);
            ret = nvs_get_str(nvs_handle, info.key, value_str.get(), &len);
            if (ret != ESP_OK) {
                ESP_UTILS_LOGE("\t- Get key(%s) value failed", info.key);
            } else {
                ESP_UTILS_LOGI(
                    "\t- Found key(%s): type(%s), value(%s)", info.key, type_str_it->second, value_str.get()
                );
                _local_params[info.key] = Value(std::string(value_str.get()));
            }
            break;
        }
        default:
            ESP_UTILS_LOGI("\t- Skip key(%s): type(%s)", info.key, type_str_it->second);
            break;
        }
        res = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);
    nvs_close(nvs_handle);

    ESP_UTILS_LOGI("Found %d keys in NVS", static_cast<int>(_local_params.size()));

    return true;
}

bool StorageNVS::doEventOperationEraseNVS()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(nvs_flash_erase() == ESP_OK, false, "Erase NVS failed");

    return true;
}

} // namespace esp_brookesia::services
