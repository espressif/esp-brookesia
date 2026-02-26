/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "esp_lcd_panel_ops.h"
#include "private/utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper.hpp"
#if CONFIG_EXAMPLE_EXPRESSIONS_ENABLE_EMOTE
#   include "brookesia/expression_emote.hpp"
#endif
#include "expression.hpp"

using namespace esp_brookesia;

using EmoteHelper = esp_brookesia::service::helper::ExpressionEmote;

#if CONFIG_EXAMPLE_EXPRESSIONS_ENABLE_EMOTE
namespace {
// `expression::Emote::get_instance()` cannot be called in a hardware interrupt, as this will cause a crash
expression::Emote &emote_instance = expression::Emote::get_instance();
}
#endif

bool Expression::init(std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> task_scheduler)
{
    BROOKESIA_CHECK_NULL_RETURN(task_scheduler, false, "Task scheduler is not available");

    if (is_initialized()) {
        BROOKESIA_LOGW("Expression is already initialized");
        return true;
    }

#if !CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER
    BROOKESIA_LOGW("Board manager is not enabled, initializing display peripheral");
    return true;
#else
    auto &board = Board::get_instance();
    BROOKESIA_CHECK_FALSE_RETURN(
        board.init_display(display_config_), false, "Failed to initialize display peripheral"
    );

    DisplayCallbacks display_callbacks{
        .bitmap_flush_done = [&]()
        {
#if CONFIG_EXAMPLE_EXPRESSIONS_ENABLE_EMOTE
            emote_instance.native_notify_flush_finished();
#endif // CONFIG_EXAMPLE_EXPRESSIONS_ENABLE_EMOTE
            return false;
        },
    };
    BROOKESIA_CHECK_FALSE_RETURN(
        board.register_display_callbacks(display_callbacks), false, "Failed to register display callbacks"
    );

    board.set_display_backlight(100);

    is_initialized_.store(true);
    task_scheduler_ = task_scheduler;

    BROOKESIA_LOGI("Expression initialized successfully");
#endif

    return true;
}

void Expression::init_emote()
{
    if (!EmoteHelper::is_available()) {
        BROOKESIA_LOGW("Emote is not available, skip initialization");
        return;
    }

    BROOKESIA_CHECK_FALSE_EXIT(is_initialized(), "Expression service is not initialized");

#if !CONFIG_EXAMPLE_EXPRESSIONS_ENABLE_EMOTE
    BROOKESIA_LOGW("Emote is not enabled, skip initialization");
    return;
#else
    BROOKESIA_LOGI("Initializing emote...");

    // Set emote config
    EmoteHelper::Config config{
        .h_res = display_config_.h_res,
        .v_res = display_config_.v_res,
        .buf_pixels = display_config_.h_res * 16,
        .fps = 30,
        .task_priority = 5,
        .task_stack = 10 * 1024,
        .task_affinity = CONFIG_EXAMPLE_BOARD_MANAGER_INIT_CORE_ID,
        .task_stack_in_ext = true,
        .flag_swap_color_bytes = display_config_.flag_swap_color_bytes,
        .flag_double_buffer = true,
        .flag_buff_dma = true,
    };
    auto result = EmoteHelper::call_function_sync(
                      EmoteHelper::FunctionId::SetConfig, BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
                  );
    if (!result.has_value()) {
        BROOKESIA_LOGE("Failed to set emote config: %1%", result.error());
        return;
    }
    BROOKESIA_LOGI("Emote config initialized successfully");

    // Subscribe to flush ready event
    auto flush_ready_event_slot = [&](const std::string & event_name, const boost::json::object & param_json) {
        EmoteHelper::FlushReadyEventParam param;
        auto success = BROOKESIA_DESCRIBE_FROM_JSON(param_json, param);
        BROOKESIA_CHECK_FALSE_EXIT(success, "Failed to parse flush ready event param: %1%");

        auto result = Board::get_instance().draw_display_bitmap(
                          param.x_start, param.y_start, param.x_end, param.y_end, param.data
                      );
        if (!result) {
            BROOKESIA_LOGE("Failed to draw bitmap, directly notify flush finished");
            emote_instance.native_notify_flush_finished();
        }
    };
    auto connection = EmoteHelper::subscribe_event(EmoteHelper::EventId::FlushReady, flush_ready_event_slot);
    if (connection.connected()) {
        service_connections_.push_back(std::move(connection));
    } else {
        BROOKESIA_LOGE("Failed to subscribe to flush ready event");
        return;
    }

    auto &service_manager = service::ServiceManager::get_instance();
    auto binding = service_manager.bind(EmoteHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind Emote service");
        return;
    } else {
        service_bindings_.push_back(std::move(binding));
    }

    // Load emote assets
    {
        EmoteHelper::AssetSource source{
            .source = ASSETS_PARTITION_NAME,
            .type = EmoteHelper::AssetSourceType::PartitionLabel,
            .flag_enable_mmap = false,
        };
        auto result = EmoteHelper::call_function_sync(
                          EmoteHelper::FunctionId::LoadAssetsSource, BROOKESIA_DESCRIBE_TO_JSON(source).as_object()
                      );
        if (!result.has_value()) {
            BROOKESIA_LOGE("Failed to load emote assets: %1%", result.error());
        } else {
            BROOKESIA_LOGI("Emote assets initialized successfully");
        }
    }

    // Set idle event message
    {
        auto result = EmoteHelper::call_function_sync(
                          EmoteHelper::FunctionId::SetEventMessage, BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::Idle)
                      );
        if (!result.has_value()) {
            BROOKESIA_LOGE("Failed to set emote event message: %1%", result.error());
        } else {
            BROOKESIA_LOGI("Emote event message set successfully");
        }
    }

    BROOKESIA_LOGI("Emote initialized successfully");
#endif // CONFIG_EXAMPLE_EXPRESSIONS_ENABLE_EMOTE
}
