/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "boost/thread.hpp"
#include "brookesia/agent_manager/macro_configs.h"
#if !BROOKESIA_AGENT_MANAGER_MANAGER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/service_helper/nvs.hpp"
#include "brookesia/agent_manager/manager.hpp"

namespace esp_brookesia::agent {

using AgentManagerHelper = service::helper::AgentManager;
using NVSHelper = service::helper::NVS;

constexpr uint32_t NVS_SAVE_DATA_TIMEOUT_MS = 20;
constexpr uint32_t NVS_ERASE_DATA_TIMEOUT_MS = 20;

bool Manager::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_AGENT_MANAGER_VER_MAJOR, BROOKESIA_AGENT_MANAGER_VER_MINOR,
        BROOKESIA_AGENT_MANAGER_VER_PATCH
    );

    // Initialize all agents
    auto agents = Registry::get_all_instances();
    for (const auto &[_, agent] : agents) {
        if (!agent->init()) {
            BROOKESIA_LOGE("Failed to initialize agent '%1%'", agent->get_attributes().name);
        }
    }
    // Register callbacks
    auto general_action_triggered_callback = [this](GeneralAction action) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_LOGD("Params: action(%1%)", BROOKESIA_DESCRIBE_TO_STR(action));

        BROOKESIA_CHECK_FALSE_EXECUTE(publish_event(
        BROOKESIA_DESCRIBE_ENUM_TO_STR(AgentManagerHelper::EventId::GeneralActionTriggered), {
            BROOKESIA_DESCRIBE_TO_STR(action)
        }), {}, {
            BROOKESIA_LOGE("Failed to publish general action triggered event");
        });
    };
    auto general_event_happened_callback = [this](GeneralEvent event, bool is_unexpected_event) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_LOGD(
            "Params: event(%1%), is_unexpected_event(%2%)", BROOKESIA_DESCRIBE_TO_STR(event),
            BROOKESIA_DESCRIBE_TO_STR(is_unexpected_event)
        );

        BROOKESIA_CHECK_NULL_EXIT(active_agent_, "No active agent");

        // Check if the event is unexpected, if so, trigger the corresponding general action to sync the state machine
        if (is_unexpected_event) {
            auto action = Base::get_general_action_from_target_event(event);
            BROOKESIA_CHECK_FALSE_EXIT(
                action != GeneralAction::Max, "Invalid action: %1%", BROOKESIA_DESCRIBE_TO_STR(action)
            );
            BROOKESIA_LOGW(
                "Unexpected event: %1%, sync the state machine with action: %2%",
                BROOKESIA_DESCRIBE_TO_STR(event), BROOKESIA_DESCRIBE_TO_STR(action)
            );
            BROOKESIA_CHECK_FALSE_EXECUTE(
            state_machine_->trigger_general_action(action), {}, {
                BROOKESIA_LOGE("Failed to trigger general action: %1%", BROOKESIA_DESCRIBE_TO_STR(action));
            });
        } else {
            // Publish general event happened event
            BROOKESIA_CHECK_FALSE_EXECUTE(publish_event(
            BROOKESIA_DESCRIBE_ENUM_TO_STR(AgentManagerHelper::EventId::GeneralEventHappened), std::vector<service::EventItem> {
                BROOKESIA_DESCRIBE_TO_STR(event)
            }), {}, {
                BROOKESIA_LOGE("Failed to publish general event happened event");
            });
        }
    };
    auto suspend_status_changed_callback = [this](bool is_suspended) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_LOGD("Params: is_suspended(%1%)", is_suspended);

        BROOKESIA_CHECK_FALSE_EXECUTE(
            publish_event(BROOKESIA_DESCRIBE_ENUM_TO_STR(AgentManagerHelper::EventId::SuspendStatusChanged),
        std::vector<service::EventItem> {
            is_suspended
        }), {}, {
            BROOKESIA_LOGE("Failed to publish suspend status changed event");
        });
    };
    Base::register_callbacks({
        .general_action_triggered_callback = general_action_triggered_callback,
        .general_event_happened_callback = general_event_happened_callback,
        .suspend_status_changed_callback = suspend_status_changed_callback,
    });

    // Create state machine
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        state_machine_ = std::make_unique<StateMachine>(), false, "Failed to create state machine"
    );
    BROOKESIA_CHECK_FALSE_RETURN(state_machine_->init(), false, "Failed to initialize state machine");

    return true;
}

void Manager::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Deinitialize state machine
    state_machine_.reset();

    // Deinitialize all agents
    auto agents = Registry::get_all_instances();
    for (const auto &[_, agent] : agents) {
        agent->deinit();
    }

    // Reset all data
    reset_data();
}

bool Manager::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Try to load data from NVS
    try_load_data();

    return true;
}

void Manager::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (state_machine_->is_running()) {
        state_machine_->stop();
    }
    active_agent_.reset();
}

std::expected<void, std::string> Manager::function_set_agent_info(
    const std::string &name, const boost::json::object &info
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%), info(%2%)", name, BROOKESIA_DESCRIBE_TO_STR(info));

    auto agent = Registry::get_instance(name);
    if (agent == nullptr) {
        return std::unexpected((boost::format("No agent found with name '%1%'") % name).str());
    }

    if (!agent->set_info(info)) {
        return std::unexpected("Failed to set agent info");
    }

    return {};
}

std::expected<void, std::string> Manager::function_activate_agent(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    if ((active_agent_ != nullptr) && (active_agent_->get_attributes().name == name)) {
        return {};
    }

    auto result = activate_agent_without_nvs(name);
    if (!result) {
        return std::unexpected(result.error());
    }

    try_save_data(DataType::ActiveAgent);

    return {};
}

std::expected<void, std::string> Manager::function_deactivate_agent()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // If the agent is not active, return
    if (active_agent_ == nullptr) {
        return {};
    }

    // Stop the state machine if it is running
    state_machine_->stop();
    // Deactivate the active agent
    active_agent_->deactivate();
    active_agent_.reset();

    // Remove agent functions and events from the manager
    auto manager_function_schemas = get_function_schemas();
    auto manager_function_handlers = get_function_handlers();
    auto manager_event_schemas = get_event_schemas();
    if (!register_functions(std::move(manager_function_schemas), std::move(manager_function_handlers))) {
        return std::unexpected("Failed to remove agent functions");
    }
    if (!register_events(std::move(manager_event_schemas))) {
        return std::unexpected("Failed to remove agent events");
    }

    try_save_data(DataType::ActiveAgent);

    return {};
}

std::expected<boost::json::array, std::string> Manager::function_get_agent_attributes(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::vector<AgentAttributes> agent_attributes;

    if (name.empty()) {
        auto instances = Registry::get_all_instances();
        for (const auto &[_, instance] : instances) {
            agent_attributes.push_back(instance->get_attributes());
        }
    } else {
        auto agent = Registry::get_instance(name);
        if (agent == nullptr) {
            return std::unexpected((boost::format("No agent found with name '%1%'") % name).str());
        } else {
            agent_attributes.push_back(agent->get_attributes());
        }
    }

    return BROOKESIA_DESCRIBE_TO_JSON(agent_attributes).as_array();
}

std::expected<std::string, std::string> Manager::function_get_active_agent()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (active_agent_ == nullptr) {
        return "";
    }

    return active_agent_->get_attributes().name;
}

std::expected<void, std::string> Manager::function_trigger_general_action(const std::string &action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: action(%1%)", action);

    if (active_agent_ == nullptr) {
        return std::unexpected("No active agent");
    }

    GeneralAction action_enum;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM(action, action_enum)) {
        return std::unexpected((boost::format("Invalid general action '%1%'") % action).str());
    }

    if (!state_machine_->trigger_general_action(action_enum)) {
        return std::unexpected((boost::format("Failed to trigger general action '%1%'") % action).str());
    }

    return {};
}

std::expected<void, std::string> Manager::function_trigger_suspend()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (active_agent_ == nullptr) {
        return std::unexpected("No active agent");
    }

    if (!active_agent_->do_suspend()) {
        return std::unexpected("Failed to suspend agent");
    }

    return {};
}

std::expected<void, std::string> Manager::function_trigger_resume()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (active_agent_ == nullptr) {
        return std::unexpected("No active agent");
    }

    active_agent_->do_resume();

    return {};
}

std::expected<void, std::string> Manager::function_interrupt_speaking()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (active_agent_ == nullptr) {
        return std::unexpected("No active agent");
    }

    auto &agent_attributes   = active_agent_->get_attributes();
    if (!agent_attributes.support_interrupt_speaking) {
        return std::unexpected(
                   (boost::format("Agent '%1%' does not support interrupt speaking") % agent_attributes.name).str()
               );
    }

    if (!active_agent_->do_interrupt_speaking()) {
        return std::unexpected("Failed to interrupt speaking");
    }

    return {};
}

std::expected<std::string, std::string> Manager::function_get_general_state()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (active_agent_ == nullptr) {
        return std::unexpected("No active agent");
    }

    auto general_state = state_machine_->get_current_state();
    if (general_state == GeneralState::Max) {
        return std::unexpected("Invalid general state");
    }

    return BROOKESIA_DESCRIBE_TO_STR(general_state);
}

std::expected<bool, std::string> Manager::function_get_suspend_status()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (active_agent_ == nullptr) {
        return std::unexpected("No active agent");
    }

    return active_agent_->is_suspended();
}

std::expected<void, std::string> Manager::function_reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto binding = service::ServiceManager::get_instance().bind(NVSHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind NVS service");
    }

    reset_data();

    auto agents = Registry::get_all_instances();
    for (const auto &[_, agent] : agents) {
        if (!agent->reset_data()) {
            BROOKESIA_LOGE("Failed to reset data for agent '%1%'", agent->get_attributes().name);
        }
    }

    try_erase_data();

    return {};
}

std::vector<service::FunctionSchema> Manager::get_function_schemas()
{
    auto helper_schemas = AgentManagerHelper::get_function_schemas();
    std::vector<service::FunctionSchema> function_schemas(helper_schemas.begin(), helper_schemas.end());

    if (active_agent_ != nullptr) {
        auto additional = active_agent_->get_function_schemas();
        function_schemas.insert(
            function_schemas.end(), std::make_move_iterator(additional.begin()),
            std::make_move_iterator(additional.end())
        );
    }

    return function_schemas;
}

std::vector<service::EventSchema> Manager::get_event_schemas()
{
    auto helper_schemas = AgentManagerHelper::get_event_schemas();
    std::vector<service::EventSchema> event_schemas(helper_schemas.begin(), helper_schemas.end());

    if (active_agent_ != nullptr) {
        auto additional = active_agent_->get_event_schemas();
        event_schemas.insert(
            event_schemas.end(), std::make_move_iterator(additional.begin()),
            std::make_move_iterator(additional.end())
        );
    }

    return event_schemas;
}

service::ServiceBase::FunctionHandlerMap Manager::get_function_handlers()
{
    service::ServiceBase::FunctionHandlerMap function_handlers{
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
            AgentManagerHelper, AgentManagerHelper::FunctionId::SetAgentInfo, std::string, boost::json::object,
            function_set_agent_info(PARAM1, PARAM2)
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
            AgentManagerHelper, AgentManagerHelper::FunctionId::ActivateAgent, std::string,
            function_activate_agent(PARAM)
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
            AgentManagerHelper, AgentManagerHelper::FunctionId::DeactivateAgent,
            function_deactivate_agent()
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
            AgentManagerHelper, AgentManagerHelper::FunctionId::GetAgentAttributes, std::string,
            function_get_agent_attributes(PARAM)
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
            AgentManagerHelper, AgentManagerHelper::FunctionId::GetActiveAgent,
            function_get_active_agent()
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
            AgentManagerHelper, AgentManagerHelper::FunctionId::TriggerGeneralAction, std::string,
            function_trigger_general_action(PARAM)
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
            AgentManagerHelper, AgentManagerHelper::FunctionId::TriggerSuspend,
            function_trigger_suspend()
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
            AgentManagerHelper, AgentManagerHelper::FunctionId::TriggerResume,
            function_trigger_resume()
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
            AgentManagerHelper, AgentManagerHelper::FunctionId::TriggerInterruptSpeaking,
            function_interrupt_speaking()
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
            AgentManagerHelper, AgentManagerHelper::FunctionId::GetGeneralState,
            function_get_general_state()
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
            AgentManagerHelper, AgentManagerHelper::FunctionId::GetSuspendStatus,
            function_get_suspend_status()
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
            AgentManagerHelper, AgentManagerHelper::FunctionId::ResetData,
            function_reset_data()
        )
    };

    if (active_agent_ != nullptr) {
        auto additional = active_agent_->get_function_handlers();
        for (auto &&[key, value] : additional) {
            function_handlers.emplace(std::move(key), std::move(value));
        }
    }

    return function_handlers;
}

std::expected<void, std::string> Manager::activate_agent_without_nvs(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    // If the agent is already active, return
    if ((active_agent_ != nullptr) && (active_agent_->get_attributes().name == name)) {
        return {};
    }

    // Get the new agent
    auto new_agent = Registry::get_instance(name);
    if (new_agent == nullptr) {
        return std::unexpected((boost::format("No agent found with name '%1%'") % name).str());
    }

    // Stop the state machine
    state_machine_->stop();
    // Deactivate the current active agent
    if (active_agent_ != nullptr) {
        active_agent_->deactivate();
    }

    // Activate the new agent
    if (!new_agent->activate()) {
        return std::unexpected("Failed to activate agent");
    }

    // Set the new agent as the active agent before registering its functions and events
    active_agent_ = new_agent;

    lib_utils::FunctionGuard deactivate_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        state_machine_->stop();
        active_agent_->deactivate();
        active_agent_.reset();
    });

    // Start the state machine with the new agent
    if (!state_machine_->start()) {
        return std::unexpected("Failed to start state machine");
    }

    deactivate_guard.release();

    BROOKESIA_LOGI("Activated agent '%1%'", name);

    return {};
}

void Manager::reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto agents = Registry::get_all_instances();
    for (const auto &[_, agent] : agents) {
        if (!agent->reset_data()) {
            BROOKESIA_LOGE("Failed to reset data for agent '%1%'", agent->get_attributes().name);
        }
    }
    is_data_loaded_ = false;

    BROOKESIA_LOGI("Reset all data");
}

void Manager::try_load_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_data_loaded_) {
        BROOKESIA_LOGD("Data is already loaded, skip");
        return;
    }

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto nvs_namespace = get_attributes().name;

    // Load active agent from NVS
    {
        auto key = BROOKESIA_DESCRIBE_TO_STR(DataType::ActiveAgent);
        auto result = NVSHelper::get_key_value<std::string>(nvs_namespace, key);
        if (!result) {
            BROOKESIA_LOGW("Failed to load '%1%' from NVS: %2%", key, result.error());
        } else {
            auto active_agent_name = result.value();
            BROOKESIA_LOGD("Loaded '%1%' from NVS", active_agent_name);
            auto result = activate_agent_without_nvs(active_agent_name);
            if (!result) {
                BROOKESIA_LOGE("Failed to activate agent '%1%': %2%", active_agent_name, result.error());
            }
        }
    }

    is_data_loaded_ = true;

    BROOKESIA_LOGI("Loaded all data from NVS");
}

void Manager::try_save_data(DataType type)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto key = BROOKESIA_DESCRIBE_TO_STR(type);
    BROOKESIA_LOGD("Params: type(%1%)", key);

    auto nvs_namespace = get_attributes().name;

    auto save_function = [this, &nvs_namespace, &key](const auto & data_value) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        auto result = NVSHelper::save_key_value(nvs_namespace, key, data_value, NVS_SAVE_DATA_TIMEOUT_MS);
        if (!result) {
            BROOKESIA_LOGE("Failed to save '%1%' to NVS: %2%", key, result.error());
        } else {
            BROOKESIA_LOGI("Saved '%1%' to NVS", key);
        }
    };
    if (type == DataType::ActiveAgent) {
        save_function(get_data<DataType::ActiveAgent>());
    } else {
        BROOKESIA_LOGE("Invalid data type for saving to NVS");
    }
}

void Manager::try_erase_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto result = NVSHelper::erase_keys(get_attributes().name, {}, NVS_ERASE_DATA_TIMEOUT_MS);
    if (!result) {
        BROOKESIA_LOGE("Failed to erase NVS data: %1%", result.error());
    } else {
        BROOKESIA_LOGI("Erased NVS data");
    }
}

BROOKESIA_PLUGIN_REGISTER_SINGLETON(
    service::ServiceBase, Manager, Manager::get_instance().get_attributes().name, Manager::get_instance()
)

} // namespace esp_brookesia::agent
