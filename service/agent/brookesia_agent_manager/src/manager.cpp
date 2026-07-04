/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <expected>
#include <string_view>

#include "boost/format.hpp"
#include "boost/thread.hpp"
#include "brookesia/agent_manager/macro_configs.h"
#if !BROOKESIA_AGENT_MANAGER_MANAGER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/service_helper/system/storage.hpp"
#include "brookesia/agent_manager/manager.hpp"

namespace esp_brookesia::agent {

using ManagerHelper = service::helper::Manager;
using StorageHelper = service::helper::Storage;
using AudioHelper = service::helper::Audio;

constexpr uint32_t STORAGE_ERASE_DATA_TIMEOUT_MS = 100;

namespace {

std::expected<std::string, std::string> make_storage_namespace(std::string_view raw_namespace)
{
    auto result = StorageHelper::make_kv_namespace({raw_namespace});
    if (!result) {
        return std::unexpected(
                   (boost::format("Failed to make AgentManager Storage namespace '%1%': %2%") %
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
    auto result = StorageHelper::make_kv_key({raw_key});
    if (!result) {
        return std::unexpected(
                   (boost::format("Failed to make AgentManager Storage key '%1%': %2%") %
                    raw_key % result.error()).str()
               );
    }
    if (result->hashed) {
        BROOKESIA_LOGW("%1%", result->warning);
    }
    return result->name;
}

} // namespace

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

    auto agent_names = get_agent_names();
    if (!agent_names.empty()) {
        set_data<DataType::TargetAgent>(agent_names[0]);
    } else {
        set_data<DataType::TargetAgent>("");
    }

    return true;
}

void Manager::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Deinitialize state machine
    state_machine_.reset();

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
        if (service_attributes.task_scheduler_config.has_value()) {
            continue;
        }
        stop_agent_if_running(agent, "setting task scheduler");
        if (!agent->set_task_scheduler(get_task_scheduler())) {
            BROOKESIA_LOGE("Failed to set task scheduler for agent '%1%'", service_attributes.name);
        }
    }

#if BROOKESIA_AGENT_MANAGER_ENABLE_AUTO_LOAD_DATA
    // Try to load data from Storage
    try_load_data();
#endif

    return true;
}

void Manager::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_active_agent();

    // Reset task scheduler for all agents
    auto agents = Registry::get_all_instances();
    for (const auto &[_, agent] : agents) {
        stop_agent_if_running(agent, "resetting task scheduler");
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
    try_save_data(DataType::ChatMode);

    return {};
}

std::expected<void, std::string> Manager::function_set_target_agent(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    if (name.empty()) {
        return std::unexpected("Target agent name is required");
    }

    auto target_agent = get_data<DataType::TargetAgent>();
    if (target_agent == name) {
        BROOKESIA_LOGD("Target agent not changed, skip");
        return {};
    }

    auto agent = Registry::get_instance(name);
    if (agent == nullptr) {
        return std::unexpected((boost::format("No agent found with name '%1%'") % name).str());
    }

    set_data<DataType::TargetAgent>(name);
    try_save_data(DataType::TargetAgent);

    return {};
}

std::expected<std::string, std::string> Manager::function_get_target_agent()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return get_data<DataType::TargetAgent>();
}

std::expected<boost::json::array, std::string> Manager::function_get_agent_names()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return BROOKESIA_DESCRIBE_TO_JSON(get_agent_names()).as_array();
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

std::expected<std::string, std::string> Manager::function_get_chat_mode()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return BROOKESIA_DESCRIBE_TO_STR(get_chat_mode());
}

std::expected<std::string, std::string> Manager::function_get_active_agent()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (active_agent_ == nullptr) {
        return std::unexpected("No active agent");
    }

    return active_agent_->get_attributes().name;
}

std::expected<void, std::string> Manager::function_trigger_general_action(const std::string &action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: action(%1%)", action);

    GeneralAction action_enum;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM(action, action_enum)) {
        return std::unexpected((boost::format("Invalid general action '%1%'") % action).str());
    }

    if (action_enum == GeneralAction::Activate) {
        if ((active_agent_ == nullptr) || (active_agent_->get_attributes().name != get_data<DataType::TargetAgent>())) {
            if (get_data<DataType::TargetAgent>().empty()) {
                return std::unexpected("No target agent specified");
            } else {
                auto result = activate_agent(get_data<DataType::TargetAgent>());
                if (!result) {
                    return std::unexpected(result.error());
                }
            }
        }
    }

    if (active_agent_ == nullptr) {
        return std::unexpected("No active agent");
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

std::expected<void, std::string> Manager::function_load_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    is_data_loaded_ = false;
    try_load_data();

    return {};
}

std::expected<void, std::string> Manager::function_reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto binding = service::ServiceManager::get_instance().bind(StorageHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind Storage service");
    }

    reset_data();

    return {};
}

std::expected<void, std::string> Manager::activate_agent(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    std::string target_agent = name;
    if (name.empty()) {
        if (get_data<DataType::TargetAgent>().empty()) {
            return std::unexpected("No target active agent specified");
        }
        target_agent = get_data<DataType::TargetAgent>();
    }

    if ((active_agent_ != nullptr) && (active_agent_->get_attributes().name == target_agent)) {
        BROOKESIA_LOGD("Agent is already active, skip");
        return {};
    }

    // Get the new agent
    auto new_agent = Registry::get_instance(target_agent);
    if (new_agent == nullptr) {
        return std::unexpected((boost::format("No agent found with name '%1%'") % target_agent).str());
    }

    stop_active_agent();

    // Start the new agent
    if (!new_agent->start()) {
        return std::unexpected("Failed to start agent");
    }

    // Set the new agent as the active agent before registering its functions and events
    active_agent_ = new_agent;
    lib_utils::FunctionGuard stop_new_agent_guard([this, new_agent]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        if ((state_machine_ != nullptr) && state_machine_->is_running()) {
            state_machine_->stop();
        }
        if (new_agent->is_running()) {
            new_agent->stop();
        }
        if (active_agent_ == new_agent) {
            active_agent_.reset();
        }
    });

    // Start the state machine with the new agent
    if (!state_machine_->start()) {
        return std::unexpected("Failed to start state machine");
    }

    // Trigger the general action to activate the agent
    if (!state_machine_->trigger_general_action(GeneralAction::Activate)) {
        return std::unexpected("Failed to trigger general action to activate agent");
    }

    stop_new_agent_guard.release();

    return {};
}

std::vector<std::string> Manager::get_agent_names()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto instances = Registry::get_all_instances();
    std::vector<std::string> agent_names;
    for (const auto &[_, instance] : instances) {
        agent_names.push_back(instance->get_attributes().name);
    }

    return agent_names;
}

void Manager::stop_active_agent()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if ((state_machine_ != nullptr) && state_machine_->is_running()) {
        state_machine_->stop();
    }

    if (active_agent_ == nullptr) {
        return;
    }

    if (active_agent_->is_running()) {
        active_agent_->stop();
    }
    active_agent_.reset();
}

void Manager::stop_agent_if_running(const std::shared_ptr<Base> &agent, const char *reason)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if ((agent == nullptr) || !agent->is_running()) {
        return;
    }

    BROOKESIA_LOGW("Stopping running agent '%1%' before %2%", agent->get_attributes().name, reason);
    agent->stop();
    if (active_agent_ == agent) {
        active_agent_.reset();
    }
}

void Manager::reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto agent_names = get_agent_names();
    if (!agent_names.empty()) {
        set_data<DataType::TargetAgent>(agent_names[0]);
    } else {
        set_data<DataType::TargetAgent>("");
    }
    set_data<DataType::ChatMode>(DEFAULT_CHAT_MODE);

    try_erase_data();

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

    if (!StorageHelper::is_available()) {
        BROOKESIA_LOGD("Storage service is not available, skip");
        return;
    }

    auto namespace_result = make_storage_namespace(get_attributes().name);
    if (!namespace_result) {
        BROOKESIA_LOGW("%1%", namespace_result.error());
        return;
    }
    auto kv_namespace = std::move(namespace_result.value());
    auto load_data_from_kv_function = [this, &kv_namespace]<DataType type>() {
        auto raw_key = BROOKESIA_DESCRIBE_TO_STR(type);
        auto key_result = make_storage_key(raw_key);
        if (!key_result) {
            BROOKESIA_LOGW("%1%", key_result.error());
            return;
        }
        auto key = std::move(key_result.value());
        auto kv_result = StorageHelper::get_key_value<decltype(get_data<type>())>(kv_namespace, key);
        if (!kv_result) {
            BROOKESIA_LOGD("No persisted '%1%' in Storage: %2%", raw_key, kv_result.error());
            return;
        }
        set_data<type>(kv_result.value());
        BROOKESIA_LOGI("Loaded '%1%' from Storage", raw_key);
    };

    load_data_from_kv_function.template operator()<DataType::TargetAgent>();
    load_data_from_kv_function.template operator()<DataType::ChatMode>();

    is_data_loaded_ = true;

    BROOKESIA_LOGI("Loaded all data from Storage");
}

void Manager::try_save_data(DataType type)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!StorageHelper::is_available()) {
        BROOKESIA_LOGD("Storage service is not available, skip");
        return;
    }

    auto namespace_result = make_storage_namespace(get_attributes().name);
    BROOKESIA_CHECK_FALSE_EXIT(namespace_result, "%1%", namespace_result.error());
    auto kv_namespace = std::move(namespace_result.value());
    auto save_data_to_kv_function = [this, &kv_namespace]<DataType type>() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        auto raw_key = BROOKESIA_DESCRIBE_TO_STR(type);
        auto key_result = make_storage_key(raw_key);
        BROOKESIA_CHECK_FALSE_EXIT(key_result, "%1%", key_result.error());
        auto key = std::move(key_result.value());
        auto result = StorageHelper::save_key_value_async(kv_namespace, key, get_data<type>(),
        [this, raw_key](auto &&result) mutable {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            BROOKESIA_CHECK_FALSE_EXIT(
                result.success, "Failed to save %1% to Storage: %2%", raw_key, result.error_message
            );
            BROOKESIA_LOGI("Saved '%1%' to Storage", raw_key);
        });
        BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to save data to Storage");
    };

    if (type == DataType::TargetAgent) {
        save_data_to_kv_function.template operator()<DataType::TargetAgent>();
    } else if (type == DataType::ChatMode) {
        save_data_to_kv_function.template operator()<DataType::ChatMode>();
    } else {
        BROOKESIA_LOGE("Invalid data type for saving to Storage");
    }
}

void Manager::try_erase_data()
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
    auto result = StorageHelper::erase_keys(namespace_result.value(), {}, STORAGE_ERASE_DATA_TIMEOUT_MS);
    if (!result) {
        BROOKESIA_LOGE("Failed to erase Storage data: %1%", result.error());
    } else {
        BROOKESIA_LOGI("Erased Storage data");
    }
}

#if BROOKESIA_AGENT_MANAGER_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    service::ServiceBase, Manager, Manager::get_instance().get_attributes().name, Manager::get_instance(),
    BROOKESIA_AGENT_MANAGER_PLUGIN_SYMBOL
)
#endif

} // namespace esp_brookesia::agent
