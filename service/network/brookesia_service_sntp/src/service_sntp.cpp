/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <chrono>
#include <expected>
#include <string_view>
#include "boost/format.hpp"
#include "brookesia/service_sntp/macro_configs.h"
#if !BROOKESIA_SERVICE_SNTP_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_interface.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_helper/system/storage.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_sntp/service_sntp.hpp"

namespace esp_brookesia::service {

constexpr uint32_t STORAGE_ERASE_DATA_TIMEOUT_MS = 100;

namespace {

std::expected<std::string, std::string> make_storage_namespace(std::string_view raw_namespace)
{
    auto result = helper::Storage::make_kv_namespace({raw_namespace});
    if (!result) {
        return std::unexpected(
                   (boost::format("Failed to make SNTP Storage namespace '%1%': %2%") %
                    raw_namespace % result.error()).str()
               );
    }
    if (result->hashed) {
        BROOKESIA_LOGW("%1%", result->warning);
    }
    return result->name;
}

std::expected<std::string, std::string> make_storage_key(std::string_view raw_key)
{
    auto result = helper::Storage::make_kv_key({raw_key});
    if (!result) {
        return std::unexpected(
                   (boost::format("Failed to make SNTP Storage key '%1%': %2%") %
                    raw_key % result.error()).str()
               );
    }
    if (result->hashed) {
        BROOKESIA_LOGW("%1%", result->warning);
    }
    return result->name;
}

} // namespace

bool SNTP::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_SNTP_VER_MAJOR, BROOKESIA_SERVICE_SNTP_VER_MINOR,
        BROOKESIA_SERVICE_SNTP_VER_PATCH
    );

    state_ = State::Stopped;
    return true;
}

void SNTP::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_state_machine(State::Stopped);
    sntp_iface_.reset();
    connectivity_iface_.reset();
    scheduler_.reset();
    is_data_loaded_ = false;
}

bool SNTP::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    scheduler_ = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler_, false, "Failed to get task scheduler");

#if BROOKESIA_SERVICE_SNTP_ENABLE_AUTO_LOAD_DATA
    try_load_data();
#endif
    BROOKESIA_CHECK_FALSE_RETURN(start_state_machine(), false, "Failed to start SNTP state machine");

    return true;
}

void SNTP::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_state_machine(State::Stopped);
    scheduler_.reset();
    is_data_loaded_ = false;
}

std::expected<void, std::string> SNTP::function_set_servers(const boost::json::array &servers)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::vector<std::string> servers_list;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(servers, servers_list)) {
        return std::unexpected("Failed to parse servers list");
    }
    if (servers_list.empty()) {
        servers_list.push_back(DEFAULT_NTP_SERVER.data());
    }

    if (!ensure_sntp_iface()) {
        return std::unexpected("SNTP interface is not available");
    }
    if (servers_list.size() > sntp_iface_->get_max_servers()) {
        return std::unexpected(
                   (boost::format("The number of servers (%1%) is greater than the maximum number of servers (%2%)")
                    % servers_list.size() % sntp_iface_->get_max_servers()).str());
    }
    if (!sntp_iface_->set_servers(servers_list)) {
        return std::unexpected("Failed to apply SNTP servers: " + sntp_iface_->get_last_error());
    }

    set_data<DataType::Servers>(std::move(servers_list));
    try_save_data(DataType::Servers);

    if (is_running() && (state_ != State::Stopped)) {
        if (!start_state_machine()) {
            return std::unexpected("Failed to restart SNTP");
        }
    }

    return {};
}

std::expected<void, std::string> SNTP::function_set_timezone(const std::string &timezone)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (timezone.empty()) {
        return std::unexpected("Timezone is empty");
    }

    if (!ensure_sntp_iface()) {
        return std::unexpected("SNTP interface is not available");
    }
    if (!sntp_iface_->set_timezone(timezone)) {
        return std::unexpected("Failed to apply SNTP timezone: " + sntp_iface_->get_last_error());
    }

    set_data<DataType::Timezone>(timezone);
    try_save_data(DataType::Timezone);
    if (is_running()) {
        (void)publish_timezone_changed();
    }

    return {};
}

std::expected<void, std::string> SNTP::function_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!start_state_machine()) {
        return std::unexpected(sntp_iface_ ? sntp_iface_->get_last_error() : "SNTP interface is not available");
    }

    return {};
}

std::expected<void, std::string> SNTP::function_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_state_machine(State::Stopped);

    return {};
}

std::expected<boost::json::array, std::string> SNTP::function_get_servers()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return BROOKESIA_DESCRIBE_TO_JSON(data_servers_).as_array();
}

std::expected<std::string, std::string> SNTP::function_get_timezone()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return data_timezone_;
}

std::expected<std::string, std::string> SNTP::function_get_state()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return BROOKESIA_DESCRIBE_TO_STR(state_);
}

std::expected<bool, std::string> SNTP::function_is_time_synced()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (sntp_iface_) {
        is_time_synced_ = is_time_synced_ || sntp_iface_->is_time_synced();
    }

    return is_time_synced_;
}

std::expected<void, std::string> SNTP::function_reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    reset_data();
    try_erase_data();

    if (is_running()) {
        if (!start_state_machine()) {
            return std::unexpected("Failed to restart SNTP");
        }
    }

    return {};
}

std::expected<void, std::string> SNTP::function_load_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    is_data_loaded_ = false;
    try_load_data();

    return {};
}

bool SNTP::ensure_sntp_iface()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (sntp_iface_) {
        return true;
    }
    if (!hal::has_interface(hal::network::SntpClientIface::NAME)) {
        BROOKESIA_LOGD("SNTP interface is not available");
        return false;
    }

    sntp_iface_ = hal::acquire_first_interface<hal::network::SntpClientIface>();
    return static_cast<bool>(sntp_iface_);
}

bool SNTP::ensure_connectivity_iface()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (connectivity_iface_) {
        return true;
    }
    if (!hal::has_interface(hal::network::ConnectivityIface::NAME)) {
        BROOKESIA_LOGD("Network connectivity interface is not available");
        return false;
    }

    connectivity_iface_ = hal::acquire_first_interface<hal::network::ConnectivityIface>();
    return static_cast<bool>(connectivity_iface_);
}

bool SNTP::start_state_machine()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_NULL_RETURN(scheduler_, false, "Task scheduler is not available");
    BROOKESIA_CHECK_FALSE_RETURN(
        ensure_connectivity_iface(), false, "Failed to acquire network connectivity interface"
    );

    stop_tasks();
    stop_backend();
    is_time_synced_ = false;

    return start_checking_network();
}

bool SNTP::start_checking_network()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_NULL_RETURN(scheduler_, false, "Task scheduler is not available");
    BROOKESIA_CHECK_FALSE_RETURN(
        ensure_connectivity_iface(), false, "Failed to acquire network connectivity interface"
    );

    set_state(State::CheckingNetwork);

    auto wait_for_network_task = [this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        if (!is_running()) {
            wait_for_network_task_id_ = 0;
            return false;
        }
        if (!connectivity_iface_ || !connectivity_iface_->is_network_ready()) {
            BROOKESIA_LOGD("Network is not ready for SNTP, keep checking");
            return true;
        }

        BROOKESIA_LOGI("Network is ready, starting SNTP synchronization");
        wait_for_network_task_id_ = 0;
        BROOKESIA_CHECK_FALSE_RETURN(start_syncing(), false, "Failed to start SNTP synchronization");
        return false;
    };
    auto result = scheduler_->post_periodic(
                      std::move(wait_for_network_task), BROOKESIA_SERVICE_SNTP_WAIT_FOR_NETWORK_INTERVAL_MS,
                      &wait_for_network_task_id_, get_call_task_group()
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to post wait-for-network task");

    return true;
}

bool SNTP::start_syncing()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_NULL_RETURN(scheduler_, false, "Task scheduler is not available");
    BROOKESIA_CHECK_FALSE_RETURN(ensure_sntp_iface(), false, "Failed to acquire SNTP interface");

    stop_backend();
    hal::network::SntpClientIface::Config config = {
        .servers = data_servers_,
        .timezone = data_timezone_,
        .use_dhcp = true,
    };
    if (!sntp_iface_->init(config)) {
        BROOKESIA_LOGW("Failed to initialize SNTP backend: %1%", sntp_iface_->get_last_error());
        return start_checking_network();
    }
    if (!sntp_iface_->start()) {
        BROOKESIA_LOGW("Failed to start SNTP backend: %1%", sntp_iface_->get_last_error());
        stop_backend();
        return start_checking_network();
    }

    set_state(State::Syncing);

    auto sync_start_time = std::chrono::steady_clock::now();
    auto sync_time_task = [this, sync_start_time]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        if (!is_running()) {
            sync_time_task_id_ = 0;
            return false;
        }
        if (sntp_iface_ && sntp_iface_->wait_time_sync(BROOKESIA_SERVICE_SNTP_SYNC_TIME_WAIT_STEP_MS)) {
            BROOKESIA_LOGI("SNTP time synchronization completed");
            is_time_synced_ = true;
            sync_time_task_id_ = 0;
            set_state(State::Synced);
            return false;
        }

        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - sync_start_time
                                ).count();
        if (elapsed_ms <= BROOKESIA_SERVICE_SNTP_SYNC_TIME_TIMEOUT_MS) {
            BROOKESIA_LOGD(
                "SNTP time is not synchronized yet: %1%",
                sntp_iface_ ? sntp_iface_->get_last_error() : std::string("interface is not available")
            );
            return true;
        }

        BROOKESIA_LOGW(
            "SNTP sync timeout, retrying after %1%ms", BROOKESIA_SERVICE_SNTP_SYNC_TIME_RETRY_DELAY_MS
        );
        stop_backend();
        auto retry_task = [this]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            retry_task_id_ = 0;
            if (is_running()) {
                BROOKESIA_CHECK_FALSE_EXIT(start_checking_network(), "Failed to restart SNTP network check");
            }
        };
        auto result = scheduler_->post_delayed(
                          std::move(retry_task), BROOKESIA_SERVICE_SNTP_SYNC_TIME_RETRY_DELAY_MS,
                          &retry_task_id_, get_call_task_group()
                      );
        BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to post SNTP retry task");
        sync_time_task_id_ = 0;
        return false;
    };
    auto result = scheduler_->post_periodic(
                      std::move(sync_time_task), BROOKESIA_SERVICE_SNTP_SYNC_TIME_INTERVAL_MS,
                      &sync_time_task_id_, get_call_task_group()
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to post SNTP sync task");

    return true;
}

void SNTP::stop_state_machine(State next_state)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_tasks();
    stop_backend();
    set_state(next_state);
}

void SNTP::stop_tasks()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!scheduler_) {
        wait_for_network_task_id_ = 0;
        sync_time_task_id_ = 0;
        retry_task_id_ = 0;
        return;
    }
    if (wait_for_network_task_id_ != 0) {
        scheduler_->cancel(wait_for_network_task_id_);
        wait_for_network_task_id_ = 0;
    }
    if (sync_time_task_id_ != 0) {
        scheduler_->cancel(sync_time_task_id_);
        sync_time_task_id_ = 0;
    }
    if (retry_task_id_ != 0) {
        scheduler_->cancel(retry_task_id_);
        retry_task_id_ = 0;
    }
}

void SNTP::stop_backend()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    is_time_synced_ = false;
    if (!sntp_iface_) {
        return;
    }
    sntp_iface_->stop();
    sntp_iface_->deinit();
}

void SNTP::reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    data_servers_ = {DEFAULT_NTP_SERVER.data()};
    data_timezone_ = DEFAULT_TIMEZONE.data();
    is_time_synced_ = false;
}

void SNTP::set_state(State state, bool publish)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (state_ == state) {
        return;
    }

    state_ = state;
    BROOKESIA_LOGI("State changed: %1%", BROOKESIA_DESCRIBE_TO_STR(state_));
    if (publish && is_running()) {
        publish_state_changed();
    }
}

bool SNTP::publish_state_changed()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto result = publish_event(
                      BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::StateChanged),
                      std::vector<EventItem> {EventItem(BROOKESIA_DESCRIBE_TO_STR(state_))}
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish state changed event");

    return true;
}

bool SNTP::publish_timezone_changed()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto result = publish_event(
                      BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::TimezoneChanged),
                      std::vector<EventItem> {EventItem(data_timezone_)}
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish timezone changed event");

    return true;
}

void SNTP::try_load_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_data_loaded_) {
        BROOKESIA_LOGD("Data is already loaded, skip");
        return;
    }
    if (!StorageHelper::is_available()) {
        BROOKESIA_LOGD("Storage service is not available, skip");
        is_data_loaded_ = true;
        return;
    }

    auto binding = ServiceManager::get_instance().bind(StorageHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_EXIT(binding.is_valid(), "Failed to bind Storage service");

    auto namespace_result = make_storage_namespace(get_attributes().name);
    if (!namespace_result) {
        BROOKESIA_LOGW("%1%", namespace_result.error());
        is_data_loaded_ = true;
        return;
    }
    const auto kv_namespace = std::move(namespace_result.value());
    {
        const auto raw_key = BROOKESIA_DESCRIBE_TO_STR(DataType::Timezone);
        auto key_result = make_storage_key(raw_key);
        if (!key_result) {
            BROOKESIA_LOGW("%1%", key_result.error());
            is_data_loaded_ = true;
            return;
        }
        const auto key = std::move(key_result.value());
        auto result = StorageHelper::get_key_value<std::string>(kv_namespace, key);
        if (!result) {
            BROOKESIA_LOGD("No persisted '%1%' in Storage: %2%", raw_key, result.error());
        } else {
            BROOKESIA_LOGI("Loaded '%1%' from Storage: %2%", raw_key, result.value());
            data_timezone_ = result.value();
            if (data_timezone_.empty()) {
                data_timezone_ = DEFAULT_TIMEZONE.data();
            }
        }
    }

    {
        const auto raw_key = BROOKESIA_DESCRIBE_TO_STR(DataType::Servers);
        auto key_result = make_storage_key(raw_key);
        if (!key_result) {
            BROOKESIA_LOGW("%1%", key_result.error());
            is_data_loaded_ = true;
            return;
        }
        const auto key = std::move(key_result.value());
        auto result = StorageHelper::get_key_value<std::vector<std::string>>(kv_namespace, key);
        if (!result) {
            BROOKESIA_LOGD("No persisted '%1%' in Storage: %2%", raw_key, result.error());
        } else {
            BROOKESIA_LOGI("Loaded '%1%' from Storage: %2%", raw_key, BROOKESIA_DESCRIBE_TO_STR(result.value()));
            data_servers_ = result.value();
            if (data_servers_.empty()) {
                data_servers_.push_back(DEFAULT_NTP_SERVER.data());
            }
        }
    }

    is_data_loaded_ = true;
    BROOKESIA_LOGI("Loaded all data from Storage");
}

void SNTP::try_save_data(DataType type)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!StorageHelper::is_available()) {
        BROOKESIA_LOGD("Storage service is not available, skip");
        return;
    }

    auto namespace_result = make_storage_namespace(get_attributes().name);
    BROOKESIA_CHECK_FALSE_EXIT(namespace_result, "%1%", namespace_result.error());
    const auto kv_namespace = std::move(namespace_result.value());
    switch (type) {
    case DataType::Timezone: {
        const auto raw_key = BROOKESIA_DESCRIBE_TO_STR(DataType::Timezone);
        auto key_result = make_storage_key(raw_key);
        BROOKESIA_CHECK_FALSE_EXIT(key_result, "%1%", key_result.error());
        const auto key = std::move(key_result.value());
        auto result = StorageHelper::save_key_value_async(
                          kv_namespace, key, data_timezone_,
        [this, raw_key](auto &&result) mutable {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            BROOKESIA_CHECK_FALSE_EXIT(
                result.success, "Failed to save %1% to Storage: %2%", raw_key, result.error_message
            );
            BROOKESIA_LOGD("Saved '%1%' to Storage", raw_key);
        }
                      );
        BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to save data to Storage");
        break;
    }
    case DataType::Servers: {
        const auto raw_key = BROOKESIA_DESCRIBE_TO_STR(DataType::Servers);
        auto key_result = make_storage_key(raw_key);
        BROOKESIA_CHECK_FALSE_EXIT(key_result, "%1%", key_result.error());
        const auto key = std::move(key_result.value());
        auto result = StorageHelper::save_key_value_async(
                          kv_namespace, key, data_servers_,
        [this, raw_key](auto &&result) mutable {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            BROOKESIA_CHECK_FALSE_EXIT(
                result.success, "Failed to save %1% to Storage: %2%", raw_key, result.error_message
            );
            BROOKESIA_LOGD("Saved '%1%' to Storage", raw_key);
        }
                      );
        BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to save data to Storage");
        break;
    }
    case DataType::Max:
    default:
        BROOKESIA_LOGE("Invalid data type for saving to Storage");
        break;
    }
}

void SNTP::try_erase_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!StorageHelper::is_available()) {
        BROOKESIA_LOGD("Storage service is not available, skip");
        return;
    }

    auto namespace_result = make_storage_namespace(get_attributes().name);
    if (!namespace_result) {
        BROOKESIA_LOGE("%1%", namespace_result.error());
        return;
    }
    auto result = StorageHelper::erase_keys(
                      namespace_result.value(), {}, STORAGE_ERASE_DATA_TIMEOUT_MS
                  );
    if (!result) {
        BROOKESIA_LOGE("Failed to erase Storage data: %1%", result.error());
        return;
    }

    BROOKESIA_LOGI("Erased Storage data");
}

#if BROOKESIA_SERVICE_SNTP_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    ServiceBase, SNTP, SNTP::get_instance().get_attributes().name, SNTP::get_instance(),
    BROOKESIA_SERVICE_SNTP_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::service
