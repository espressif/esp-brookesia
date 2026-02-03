/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
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
#include "brookesia/agent_helper/manager.hpp"
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
        ChatMode,
        Max,
    };

    static Manager &get_instance()
    {
        static Manager instance;
        return instance;
    }

private:
    using Helper = helper::Manager;

    const std::string DEFAULT_ACTIVE_AGENT = "";
    const Helper::ChatMode DEFAULT_CHAT_MODE = Helper::ChatMode::RealTime;

    Manager():
        service::ServiceBase(service::ServiceBase::Attributes{
        .name = Helper::get_name().data(),
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
    std::expected<void, std::string> function_set_chat_mode(const std::string &mode);
    std::expected<void, std::string> function_activate_agent(const std::string &name);
    std::expected<boost::json::array, std::string> function_get_attributes(const std::string &name);
    std::expected<std::string, std::string> function_get_chat_mode();
    std::expected<std::string, std::string> function_get_active_agent();
    std::expected<void, std::string> function_trigger_general_action(const std::string &action);
    std::expected<void, std::string> function_suspend();
    std::expected<void, std::string> function_resume();
    std::expected<void, std::string> function_interrupt_speaking();
    std::expected<void, std::string> function_manual_start_listening();
    std::expected<void, std::string> function_manual_stop_listening();
    std::expected<std::string, std::string> function_get_general_state();
    std::expected<bool, std::string> function_get_suspend_status();
    std::expected<bool, std::string> function_get_speaking_status();
    std::expected<bool, std::string> function_get_listening_status();
    std::expected<void, std::string> function_reset_data();

    std::vector<service::FunctionSchema> get_function_schemas() override
    {
        auto helper_schemas = Helper::get_function_schemas();
        return std::vector<service::FunctionSchema>(helper_schemas.begin(), helper_schemas.end());
    }

    std::vector<service::EventSchema> get_event_schemas() override
    {
        auto helper_schemas = Helper::get_event_schemas();
        return std::vector<service::EventSchema>(helper_schemas.begin(), helper_schemas.end());
    }

    service::ServiceBase::FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::SetAgentInfo, std::string, boost::json::object,
                function_set_agent_info(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetChatMode, std::string,
                function_set_chat_mode(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::ActivateAgent, std::string,
                function_activate_agent(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::GetAgentAttributes, std::string,
                function_get_attributes(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetChatMode,
                function_get_chat_mode()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetActiveAgent,
                function_get_active_agent()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::TriggerGeneralAction, std::string,
                function_trigger_general_action(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::Suspend,
                function_suspend()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::Resume,
                function_resume()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::InterruptSpeaking,
                function_interrupt_speaking()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::ManualStartListening,
                function_manual_start_listening()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::ManualStopListening,
                function_manual_stop_listening()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetGeneralState,
                function_get_general_state()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetSuspendStatus,
                function_get_suspend_status()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetSpeakingStatus,
                function_get_speaking_status()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetListeningStatus,
                function_get_listening_status()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::ResetData,
                function_reset_data()
            )
        };
    }

    std::expected<void, std::string> activate_agent(const std::string &name);

    void reset_data();
    void try_load_data();
    void try_save_data(DataType type);
    void try_erase_data();
    template<DataType type>
    constexpr auto get_data()
    {
        if constexpr (type == DataType::ActiveAgent) {
            return target_active_agent_;
        } else if constexpr (type == DataType::ChatMode) {
            return chat_mode_;
        } else {
            static_assert(false, "Invalid data type");
        }
    }
    template<DataType type>
    void set_data(const auto &data)
    {
        // If the data is the same, skip
        if (get_data<type>() == data) {
            return;
        }
        // Otherwise, set the data and save to NVS
        if constexpr (type == DataType::ActiveAgent) {
            target_active_agent_ = data;
        } else if constexpr (type == DataType::ChatMode) {
            chat_mode_ = data;
        } else {
            static_assert(false, "Invalid data type");
        }
        try_save_data(type);
    }

    lib_utils::TaskScheduler::Group get_state_task_group() const
    {
        return get_call_task_group();
    }

    std::shared_ptr<Base> get_active_agent() const
    {
        return active_agent_;
    }

    Helper::ChatMode get_chat_mode() const
    {
        return chat_mode_;
    }

    std::unique_ptr<StateMachine> state_machine_;
    std::shared_ptr<Base> active_agent_;

    bool is_data_loaded_ = false;
    std::string target_active_agent_ = DEFAULT_ACTIVE_AGENT;
    Helper::ChatMode chat_mode_ = DEFAULT_CHAT_MODE;
};

BROOKESIA_DESCRIBE_ENUM(Manager::DataType, ActiveAgent, ChatMode, Max);

} // namespace esp_brookesia::agent
