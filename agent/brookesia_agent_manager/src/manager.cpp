/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
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

using ManagerHelper = helper::Manager;
using NVSHelper = service::helper::NVS;
using AudioHelper = service::helper::Audio;

constexpr uint32_t NVS_SAVE_DATA_TIMEOUT_MS = 20;
constexpr uint32_t NVS_ERASE_DATA_TIMEOUT_MS = 20;

bool Manager::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_AGENT_MANAGER_VER_MAJOR, BROOKESIA_AGENT_MANAGER_VER_MINOR,
        BROOKESIA_AGENT_MANAGER_VER_PATCH
    );

    // Register callbacks
    auto unexpected_general_event_happened_callback = [this](GeneralEvent event) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        auto event_str = BROOKESIA_DESCRIBE_TO_STR(event);
        BROOKESIA_LOGD("Params: event(%1%)", event_str);

        BROOKESIA_CHECK_NULL_EXIT(active_agent_, "No active agent");

        // Force transition to the target state to sync the state machine
        auto state = StateMachine::get_general_event_target_state(event);
        auto state_str = BROOKESIA_DESCRIBE_TO_STR(state);
        BROOKESIA_CHECK_FALSE_EXIT(state != GeneralState::Max, "Invalid state: %1%", state_str);

        BROOKESIA_LOGW(
            "Detected unexpected event: %1%, force transition to the target state: %2%", event_str, state_str
        );

        BROOKESIA_CHECK_FALSE_EXECUTE(
        state_machine_->force_transition_to(state), {}, {
            BROOKESIA_LOGE("Failed to force transition to target state: %1%", state_str);
        });
    };
    auto afe_event_happened_callback = [this](AudioHelper::AFE_Event event) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD("Params: event(%1%)", BROOKESIA_DESCRIBE_TO_STR(event));

        switch (event) {
        case AudioHelper::AFE_Event::WakeStart:
            state_machine_->trigger_general_action(GeneralAction::WakeUp);
            break;
        case AudioHelper::AFE_Event::WakeEnd:
            state_machine_->trigger_general_action(GeneralAction::Sleep);
            break;
        default:
            break;
        }
    };
    Base::register_callbacks({
        .unexpected_general_event_happened = unexpected_general_event_happened_callback,
        .afe_event_happened = afe_event_happened_callback,
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

    // Reset all data
    reset_data();

    // Release all agents
    Registry::release_all_instances();
}

bool Manager::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Set task scheduler for all agents which do not have task scheduler configuration
    auto agents = Registry::get_all_instances();
    for (const auto &[_, agent] : agents) {
        auto &service_attributes = agent->service::ServiceBase::get_attributes();
        if (!service_attributes.task_scheduler_config.has_value() && !agent->set_task_scheduler(get_task_scheduler())) {
            BROOKESIA_LOGE("Failed to set task scheduler for agent '%1%'", service_attributes.name);
        }
    }

    // Try to load data from NVS
    try_load_data();

    return true;
}

void Manager::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Stop the state machine
    if (state_machine_->is_running()) {
        state_machine_->stop();
    }

    // Reset the active agent
    active_agent_.reset();

    // Reset task scheduler for all agents
    auto agents = Registry::get_all_instances();
    for (const auto &[_, agent] : agents) {
        if (!agent->set_task_scheduler(nullptr)) {
            BROOKESIA_LOGE("Failed to reset task scheduler for agent '%1%'", agent->get_attributes().name);
        }
    }
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

std::expected<void, std::string> Manager::function_set_chat_mode(const std::string &mode)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: mode(%1%)", mode);

    ChatMode chat_mode;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM(mode, chat_mode)) {
        return std::unexpected((boost::format("Invalid chat mode '%1%'") % mode).str());
    }

    set_data<DataType::ChatMode>(chat_mode);

    return {};
}

std::expected<void, std::string> Manager::function_activate_agent(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    auto result = activate_agent(name);
    if (!result) {
        return std::unexpected(result.error());
    }

    set_data<DataType::ActiveAgent>(name);

    return {};
}

std::expected<boost::json::array, std::string> Manager::function_get_attributes(const std::string &name)
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

std::expected<std::string, std::string> Manager::function_get_chat_mode()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return BROOKESIA_DESCRIBE_TO_STR(get_chat_mode());
}

std::expected<std::string, std::string> Manager::function_get_active_agent()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (active_agent_ == nullptr) {
        return "";
    }

    return active_agent_->get_attributes().get_name();
}

std::expected<void, std::string> Manager::function_trigger_general_action(const std::string &action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: action(%1%)", action);

    if (active_agent_ == nullptr) {
        auto result = activate_agent(get_data<DataType::ActiveAgent>());
        if (!result) {
            return std::unexpected(result.error());
        }
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

std::expected<void, std::string> Manager::function_suspend()
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

std::expected<void, std::string> Manager::function_resume()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (active_agent_ == nullptr) {
        return std::unexpected("No active agent");
    }

    if (!active_agent_->do_resume()) {
        return std::unexpected("Failed to resume agent");
    }

    return {};
}

std::expected<void, std::string> Manager::function_interrupt_speaking()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (active_agent_ == nullptr) {
        return std::unexpected("No active agent");
    }

    if (!active_agent_->do_interrupt_speaking()) {
        return std::unexpected("Failed to interrupt speaking");
    }

    return {};
}

std::expected<void, std::string> Manager::function_manual_start_listening()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (active_agent_ == nullptr) {
        return std::unexpected("No active agent");
    }

    if (get_chat_mode() != Helper::ChatMode::Manual) {
        return std::unexpected("Only available when the chat mode is `Manual`");
    }

    if (active_agent_->is_general_event_ready(GeneralEvent::Slept) ||
            active_agent_->is_general_action_running(GeneralAction::Sleep)) {
        if (!active_agent_->do_general_action(GeneralAction::WakeUp)) {
            return std::unexpected("Failed to wake up");
        }
    }

    if (!active_agent_->do_manual_start_listening()) {
        return std::unexpected("Failed to manually start listening");
    }

    return {};
}

std::expected<void, std::string> Manager::function_manual_stop_listening()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (active_agent_ == nullptr) {
        return std::unexpected("No active agent");
    }

    if (get_chat_mode() != Helper::ChatMode::Manual) {
        return std::unexpected("Only available when the chat mode is `Manual`");
    }

    if (!active_agent_->do_manual_stop_listening()) {
        return std::unexpected("Failed to manually stop listening");
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

std::expected<bool, std::string> Manager::function_get_speaking_status()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (active_agent_ == nullptr) {
        return std::unexpected("No active agent");
    }

    return active_agent_->is_speaking();
}

std::expected<bool, std::string> Manager::function_get_listening_status()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (active_agent_ == nullptr) {
        return std::unexpected("No active agent");
    }

    return active_agent_->is_listening();
}

std::expected<void, std::string> Manager::function_reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto binding = service::ServiceManager::get_instance().bind(NVSHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind NVS service");
    }

    reset_data();

    try_erase_data();

    return {};
}

std::expected<void, std::string> Manager::activate_agent(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    if ((active_agent_ != nullptr) && (active_agent_->get_attributes().name == name)) {
        BROOKESIA_LOGD("Agent is already active, skip");
        return {};
    }

    std::string target_agent = name;
    if (name.empty()) {
        if (get_data<DataType::ActiveAgent>().empty()) {
            return std::unexpected("No target active agent specified");
        }
        target_agent = get_data<DataType::ActiveAgent>();
    }

    // Get the new agent
    auto new_agent = Registry::get_instance(target_agent);
    if (new_agent == nullptr) {
        return std::unexpected((boost::format("No agent found with name '%1%'") % target_agent).str());
    }

    // Stop the state machine and the active agent
    if (active_agent_ != nullptr) {
        state_machine_->stop();
        active_agent_->stop();
    }

    // Start the new agent
    if (!new_agent->start()) {
        return std::unexpected("Failed to start agent");
    }

    // Set the new agent as the active agent before registering its functions and events
    active_agent_ = new_agent;

    // Start the state machine with the new agent
    if (!state_machine_->start()) {
        return std::unexpected("Failed to start state machine");
    }

    // Trigger the general action to activate the agent
    if (!state_machine_->trigger_general_action(GeneralAction::Activate)) {
        return std::unexpected("Failed to trigger general action to activate agent");
    }

    return {};
}


void Manager::reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    set_data<DataType::ActiveAgent>(DEFAULT_ACTIVE_AGENT);
    set_data<DataType::ChatMode>(DEFAULT_CHAT_MODE);

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
    auto load_data_from_nvs_function = [this, &nvs_namespace]<DataType type>() {
        auto key = BROOKESIA_DESCRIBE_TO_STR(type);
        auto nvs_result = NVSHelper::get_key_value<decltype(get_data<type>())>(nvs_namespace, key);
        if (!nvs_result) {
            BROOKESIA_LOGW("Failed to load '%1%' from NVS: %2%", key, nvs_result.error());
            return;
        }
        set_data<type>(nvs_result.value());
        BROOKESIA_LOGI("Loaded '%1%' from NVS", key);
    };

    load_data_from_nvs_function.template operator()<DataType::ActiveAgent>();
    load_data_from_nvs_function.template operator()<DataType::ChatMode>();

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

    auto nvs_namespace = get_attributes().name;
    auto save_data_to_nvs_function = [this, &nvs_namespace]<DataType type>() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        auto key = BROOKESIA_DESCRIBE_TO_STR(type);
        auto result = NVSHelper::save_key_value(nvs_namespace, key, get_data<type>(), NVS_SAVE_DATA_TIMEOUT_MS);
        if (!result) {
            BROOKESIA_LOGE("Failed to save '%1%' to NVS: %2%", key, result.error());
        } else {
            BROOKESIA_LOGI("Saved '%1%' to NVS", key);
        }
    };

    if (type == DataType::ActiveAgent) {
        save_data_to_nvs_function.template operator()<DataType::ActiveAgent>();
    } else if (type == DataType::ChatMode) {
        save_data_to_nvs_function.template operator()<DataType::ChatMode>();
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

#if BROOKESIA_AGENT_MANAGER_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    service::ServiceBase, Manager, Manager::get_instance().get_attributes().name, Manager::get_instance(),
    BROOKESIA_AGENT_MANAGER_PLUGIN_SYMBOL
)
#endif

} // namespace esp_brookesia::agent
