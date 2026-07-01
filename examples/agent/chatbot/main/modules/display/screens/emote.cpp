/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "private/utils.hpp"
#include "brookesia/service_helper.hpp"
#include "modules/display/display.hpp"
#include "emote.hpp"

using namespace esp_brookesia;
using AgentHelper = service::helper::Manager;
using WifiHelper = service::helper::Wifi;

ScreenEmote::ScreenEmote():
    StateBase(BROOKESIA_DESCRIBE_TO_STR(DisplayScreen::Emote))
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
}

ScreenEmote::~ScreenEmote()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
}

bool ScreenEmote::on_enter(const std::string &from_state, const std::string &action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI("Entering '%1%' from '%2%' with action '%3%'", get_name(), from_state, action);

    if (AgentHelper::is_running() && WifiHelper::is_running()) {
        auto wifi_state_handler = [this](service::FunctionResult && result) {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

            BROOKESIA_CHECK_FALSE_EXIT(result.success, "Failed to get WiFi state: %1%", result.error_message);

            auto &data = result.get_data<std::string>();
            WifiHelper::GeneralState wifi_state = WifiHelper::GeneralState::Max;
            auto parse_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(data, wifi_state);
            BROOKESIA_CHECK_FALSE_EXIT(parse_result, "Failed to parse WiFi state");

            auto start_agent_result = AgentHelper::call_function_async(
                                          AgentHelper::FunctionId::TriggerGeneralAction,
                                          BROOKESIA_DESCRIBE_TO_STR(AgentHelper::GeneralAction::Start)
                                      );
            BROOKESIA_CHECK_FALSE_EXIT(start_agent_result, "Failed to start agent");

            auto resume_result = AgentHelper::call_function_async(AgentHelper::FunctionId::Resume);
            BROOKESIA_CHECK_FALSE_EXIT(resume_result, "Failed to resume agent");
        };
        auto wifi_state_result = WifiHelper::call_function_async(WifiHelper::FunctionId::GetGeneralState, wifi_state_handler);
        BROOKESIA_CHECK_FALSE_RETURN(wifi_state_result, true, "Failed to get WiFi state");
    }

    // Activate the emote display source. The Emote component refreshes itself once the Display
    // service confirms the emote source owns the output, so no RefreshAll is needed here.
    BROOKESIA_CHECK_FALSE_RETURN(
        Display::get_instance().set_active_source_role(Display::DrawSource::Emote), false,
        "Failed to activate emote display source"
    );

    return true;
}

bool ScreenEmote::on_exit(const std::string &to_state, const std::string &action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI("Exiting '%1%' to '%2%' with action '%3%'", get_name(), to_state, action);

    if (AgentHelper::is_running()) {
        auto suspend_result = AgentHelper::call_function_async(AgentHelper::FunctionId::Suspend);
        BROOKESIA_CHECK_FALSE_RETURN(suspend_result, false, "Failed to suspend agent");
    }

    return true;
}
