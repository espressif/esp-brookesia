/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <memory>
#include <string>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/lib_utils/state_machine.hpp"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_helper/audio.hpp"
#include "brookesia/service_helper/sntp.hpp"
#include "brookesia/service_helper/agent/manager.hpp"
#include "brookesia/service_manager/macro_configs.h"
#include "brookesia/agent_manager/state_machine.hpp"
#include "brookesia/agent_manager/base.hpp"

namespace esp_brookesia::agent {

using Registry = lib_utils::PluginRegistry<Base>;

class Manager: public service::ServiceBase {
public:
    friend class Base;
    friend class StateMachine;

    enum class DataType {
        ActiveAgent,
        Max,
    };

    lib_utils::TaskScheduler::Group get_state_task_group() const
    {
        return std::string(service::helper::AgentManager::get_name().data()) + "_state";
    }

    static Manager &get_instance()
    {
        static Manager instance;
        return instance;
    }

private:
    Manager():
        service::ServiceBase(service::ServiceBase::Attributes{
        .name = service::helper::AgentManager::get_name().data(),
        .dependencies = {
            service::helper::Audio::get_name().data(),
            service::helper::SNTP::get_name().data()
        },
#if BROOKESIA_AGENT_MANAGER_ENABLE_WORKER
        .task_scheduler_config = lib_utils::TaskScheduler::StartConfig{
            .worker_configs = {
                lib_utils::ThreadConfig{
                    .name = BROOKESIA_AGENT_MANAGER_WORKER_NAME,
                    .core_id = BROOKESIA_AGENT_MANAGER_WORKER_CORE_ID,
                    .priority = BROOKESIA_AGENT_MANAGER_WORKER_PRIORITY,
                    .stack_size = BROOKESIA_AGENT_MANAGER_WORKER_STACK_SIZE,
                    .stack_in_ext = BROOKESIA_AGENT_MANAGER_WORKER_STACK_IN_EXT,
                }
            },
            .worker_poll_interval_ms = BROOKESIA_AGENT_MANAGER_WORKER_POLL_INTERVAL_MS,
        }
#endif // BROOKESIA_AGENT_MANAGER_ENABLE_WORKER
    })
    {
    }
    ~Manager() = default;

    bool on_init() override;
    void on_deinit() override;
    bool on_start() override;
    void on_stop() override;

    std::expected<void, std::string> function_set_agent_info(const std::string &name, const boost::json::object &info);
    std::expected<void, std::string> function_activate_agent(const std::string &name);
    std::expected<void, std::string> function_deactivate_agent();
    std::expected<boost::json::array, std::string> function_get_agent_attributes(const std::string &name);
    std::expected<std::string, std::string> function_get_active_agent();
    std::expected<void, std::string> function_trigger_general_action(const std::string &action);
    std::expected<void, std::string> function_trigger_suspend();
    std::expected<void, std::string> function_trigger_resume();
    std::expected<void, std::string> function_interrupt_speaking();
    std::expected<std::string, std::string> function_get_general_state();
    std::expected<bool, std::string> function_get_suspend_status();
    std::expected<void, std::string> function_reset_data();

    std::vector<service::FunctionSchema> get_function_schemas() override;
    std::vector<service::EventSchema> get_event_schemas() override;
    service::ServiceBase::FunctionHandlerMap get_function_handlers() override;

    std::expected<void, std::string> activate_agent_without_nvs(const std::string &name);

    template<DataType type>
    constexpr auto get_data()
    {
        if constexpr (type == DataType::ActiveAgent) {
            return active_agent_ ? active_agent_->get_attributes().name : "";
        } else {
            static_assert(false, "Invalid data type");
        }
    }
    void reset_data();
    void try_load_data();
    void try_save_data(DataType type);
    void try_erase_data();

    std::shared_ptr<Base> get_active_agent() const
    {
        return active_agent_;
    }

    std::unique_ptr<StateMachine> state_machine_;
    std::shared_ptr<Base> active_agent_;

    bool is_data_loaded_ = false;
};

BROOKESIA_DESCRIBE_ENUM(Manager::DataType, ActiveAgent, Max);

} // namespace esp_brookesia::agent
