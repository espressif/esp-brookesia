/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <condition_variable>
#include <cstdint>
#include <expected>
#include <mutex>
#include <queue>
#include <span>
#include <string>
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/wifi/basic.hpp"
#include "brookesia/hal_interface/interfaces/wifi/softap.hpp"
#include "brookesia/hal_interface/interfaces/wifi/station.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/service_helper/network/wifi.hpp"
#include "brookesia/service_helper/system/storage.hpp"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_manager/macro_configs.h"
#include "brookesia/service_wifi/hal/type.hpp"
#include "brookesia/service_wifi/state_machine.hpp"
#include "brookesia/service_wifi/macro_configs.h"

namespace esp_brookesia::service::wifi {

/**
 * @brief Wi-Fi service that coordinates HAL operations, persistence, and the Wi-Fi state machine.
 */
class Wifi : public ServiceBase {
    friend class StateMachine;

public:
    /**
     * @brief Persisted data items managed by the Wi-Fi service.
     */
    enum class DataType : uint8_t {
        LastAp,
        ConnectedAps,
        ScanParams,
        SoftApParams,
        Max,
    };

    /**
     * @brief Get the process-wide singleton instance.
     *
     * @return Reference to the singleton Wi-Fi service.
     */
    static Wifi &get_instance()
    {
        static Wifi instance;
        return instance;
    }

private:
    using Helper = helper::Wifi;
    using StorageHelper = helper::Storage;

    Wifi()
        : ServiceBase({
        .name = Helper::get_name().data(),
        .dependencies = {
            StorageHelper::get_name().data(),
        },
#if BROOKESIA_SERVICE_WIFI_ENABLE_WORKER
        .task_scheduler_config = lib_utils::TaskScheduler::StartConfig{
            .worker_configs = {
                lib_utils::ThreadConfig{
                    .name = BROOKESIA_SERVICE_WIFI_WORKER_NAME "0",
                    .core_id = BROOKESIA_SERVICE_WIFI_WORKER_0_CORE_ID,
                    .priority = BROOKESIA_SERVICE_WIFI_WORKER_PRIORITY,
                    .stack_size = BROOKESIA_SERVICE_WIFI_WORKER_STACK_SIZE,
                    .stack_in_ext = BROOKESIA_SERVICE_WIFI_WORKER_STACK_IN_EXT,
                },
                lib_utils::ThreadConfig{
                    .name = BROOKESIA_SERVICE_WIFI_WORKER_NAME "1",
                    .core_id = BROOKESIA_SERVICE_WIFI_WORKER_1_CORE_ID,
                    .priority = BROOKESIA_SERVICE_WIFI_WORKER_PRIORITY,
                    .stack_size = BROOKESIA_SERVICE_WIFI_WORKER_STACK_SIZE,
                    .stack_in_ext = BROOKESIA_SERVICE_WIFI_WORKER_STACK_IN_EXT,
                },
            },
            .worker_poll_interval_ms = BROOKESIA_SERVICE_WIFI_WORKER_POLL_INTERVAL_MS,
        }
#endif // BROOKESIA_SERVICE_WIFI_ENABLE_WORKER
    })
    {}
    ~Wifi() = default;

    bool on_init() override;
    void on_deinit() override;
    bool on_start() override;
    void on_stop() override;

    std::expected<void, std::string> function_trigger_general_action(const std::string &action);
    std::expected<std::string, std::string> function_get_general_state();
    std::expected<void, std::string> function_set_connect_ap(const std::string &ssid, const std::string &password);
    std::expected<boost::json::object, std::string> function_get_connect_ap();
    std::expected<boost::json::array, std::string> function_get_connected_aps();
    std::expected<void, std::string> function_remove_connected_ap(const std::string &ssid);
    std::expected<void, std::string> function_set_scan_params(const boost::json::object &params);
    std::expected<void, std::string> function_trigger_scan_start();
    std::expected<void, std::string> function_trigger_scan_stop();
    std::expected<void, std::string> function_set_softap_params(const boost::json::object &params);
    std::expected<boost::json::object, std::string> function_get_softap_params();
    std::expected<void, std::string> function_trigger_softap_start();
    std::expected<void, std::string> function_trigger_softap_stop();
    std::expected<void, std::string> function_trigger_softap_provision_start();
    std::expected<void, std::string> function_trigger_softap_provision_stop();
    std::expected<void, std::string> function_load_data();
    std::expected<void, std::string> function_reset_data();

    std::vector<FunctionSchema> get_function_schemas() override
    {
        auto function_schemas = Helper::get_function_schemas();
        return std::vector<FunctionSchema>(function_schemas.begin(), function_schemas.end());
    }

    std::vector<EventSchema> get_event_schemas() override
    {
        auto event_schemas = Helper::get_event_schemas();
        return std::vector<EventSchema>(event_schemas.begin(), event_schemas.end());
    }
    ServiceBase::FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::TriggerGeneralAction, std::string,
                function_trigger_general_action(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetGeneralState,
                function_get_general_state()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::SetConnectAp, std::string, std::string,
                function_set_connect_ap(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetConnectAp,
                function_get_connect_ap()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetConnectedAps,
                function_get_connected_aps()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::RemoveConnectedAp, std::string,
                function_remove_connected_ap(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetScanParams, boost::json::object,
                function_set_scan_params(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetSoftApParams, boost::json::object,
                function_set_softap_params(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetSoftApParams,
                function_get_softap_params()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::TriggerScanStart,
                function_trigger_scan_start()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::TriggerScanStop,
                function_trigger_scan_stop()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::TriggerSoftApStart,
                function_trigger_softap_start()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::TriggerSoftApStop,
                function_trigger_softap_stop()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::TriggerSoftApProvisionStart,
                function_trigger_softap_provision_start()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::TriggerSoftApProvisionStop,
                function_trigger_softap_provision_stop()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::LoadData,
                function_load_data()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::ResetData,
                function_reset_data()
            ),
        };
    }

    lib_utils::TaskScheduler::Group get_state_task_group() const
    {
        return get_call_task_group();
    }

    bool publish_general_event(GeneralEvent event, bool is_unexpected);
    bool publish_general_action(GeneralAction action);
    bool publish_scan_state_changed(bool is_running);
    bool publish_scan_ap_infos(std::span<const ScanApInfo> ap_infos);
    bool publish_softap_event(SoftApEvent event);

    bool on_hal_general_event(GeneralEvent event, bool is_unexpected);
    bool on_hal_error_state();
    bool handle_hal_general_event_policy(GeneralEvent event, bool is_unexpected, bool was_connecting);
    bool handle_scan_ap_infos_policy(std::span<const ScanApInfo> ap_infos);
    bool configure_hal_interfaces();
    void clear_hal_interface_callbacks();
    bool dispatch_general_action(GeneralAction action);
    bool is_general_action_running(GeneralAction action);
    bool is_general_event_ready(GeneralEvent event);

    void try_load_data();
    void try_save_data(DataType type);
    void try_erase_data();

    template<DataType type>
    constexpr auto &get_data()
    {
        if constexpr (type == DataType::LastAp) {
            return station_iface_->get_last_connected_ap_info();
        } else if constexpr (type == DataType::ConnectedAps) {
            return station_iface_->get_connected_ap_infos();
        } else if constexpr (type == DataType::ScanParams) {
            return station_iface_->get_scan_params();
        } else if constexpr (type == DataType::SoftApParams) {
            return softap_iface_->get_params();
        } else {
            static_assert(false, "Invalid data type");
        }
    }
    template<DataType type>
    bool set_data(const auto &data, bool is_force = false, bool is_skip_save = false)
    {
        if constexpr (type == DataType::SoftApParams) {
            if (!softap_iface_) {
                return false;
            }
        }
        // If the data is the same, skip
        if (!is_force && (get_data<type>() == data)) {
            return true;
        }
        // Otherwise, set the data and save to Storage
        bool result = false;
        if constexpr (type == DataType::LastAp) {
            result = station_iface_->set_last_connected_ap_info(data);
        } else if constexpr (type == DataType::ConnectedAps) {
            result = station_iface_->set_connected_ap_infos(data);
        } else if constexpr (type == DataType::ScanParams) {
            result = station_iface_->set_scan_params(data);
        } else if constexpr (type == DataType::SoftApParams) {
            result = softap_iface_->set_params(data);
        } else {
            static_assert(false, "Invalid data type");
        }
        if (result && !is_skip_save) {
            try_save_data(type);
        }

        return result;
    }
    void reset_data();

    bool is_general_action_queue_task_running();
    GeneralAction pop_general_action_pending_queue_front();
    GeneralAction pop_general_action_ready_queue_front();
    bool start_general_action_queue_task();
    void stop_general_action_queue_task();
    void make_general_action_ready_queue(GeneralAction action);
    bool trigger_general_action(const GeneralAction &action);

    bool is_auto_connect_allowed();
    bool schedule_auto_connect();
    void stop_auto_connect_task();
    bool trigger_auto_connect();
    bool trigger_connect_to_ap(const ConnectApInfo &ap_info);
    bool get_connectable_ap_info_by_ssid(const std::string &ssid, ConnectApInfo &ap_info);
    bool get_last_connectable_ap_info(ConnectApInfo &ap_info);
    bool mark_ap_connectable(const ConnectApInfo &ap_info, bool connectable);
    bool add_connected_ap_info(const ConnectApInfo &ap_info);
    bool remove_connected_ap_info_by_ssid(std::string_view ssid);
    bool update_connected_ap_info();
    bool handle_unexpected_disconnected(bool was_connecting);

    bool is_data_loaded_ = false;
    bool auto_connect_disabled_ = false;
    bool softap_provision_running_ = false;

    hal::InterfaceHandle<hal::wifi::BasicIface> basic_iface_;
    hal::InterfaceHandle<hal::wifi::StationIface> station_iface_;
    hal::InterfaceHandle<hal::wifi::SoftApIface> softap_iface_;
    std::unique_ptr<StateMachine> state_machine_;

    ConnectApInfo connecting_ap_info_{};
    uint8_t connect_retries_ = 0;
    std::queue<GeneralAction> general_action_pending_queue_;
    std::queue<GeneralAction> general_action_ready_queue_;
    lib_utils::TaskScheduler::TaskId general_action_queue_processing_task_ = 0;
    lib_utils::TaskScheduler::TaskId auto_connect_task_ = 0;

    /* Direct synchronization for on_stop -> HAL Deinited handshake.
     * We can't go through publish_event() here because ServiceBase::stop() holds a unique
     * lock on state_mutex_ while on_stop() runs, and publish_event()'s emit_signal_task
     * would block waiting for a shared_lock on the same mutex (reader-writer deadlock).
     * Signaling directly from on_hal_general_event() avoids the scheduler/lock entirely.
     */
    std::mutex deinit_wait_mutex_;
    std::condition_variable deinit_wait_cv_;
};

BROOKESIA_DESCRIBE_ENUM(Wifi::DataType, LastAp, ConnectedAps, ScanParams, SoftApParams, Max);

} // namespace esp_brookesia::service::wifi
