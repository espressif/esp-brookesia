/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "private/utils.hpp"
#if CONFIG_EXAMPLE_EXPRESSIONS_ENABLE_EMOTE
#   include "brookesia/expression_emote.hpp"
#endif
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper.hpp"
#if CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER
#   include "esp_lcd_panel_ops.h"
#   include "board.hpp"
#endif
#include "expression.hpp"

using namespace esp_brookesia;

using EmoteHelper = service::helper::ExpressionEmote;

/* Service manager instance */
static service::ServiceManager &service_manager = service::ServiceManager::get_instance();
/* Keep service bindings to avoid frequent start and stop of services */
static std::vector<service::ServiceBinding> service_bindings;
/* Keep event connections */
static std::vector<service::EventRegistry::SignalConnection> service_connections;

void expression_emote_init()
{
    if (!EmoteHelper::is_available()) {
        BROOKESIA_LOGW("Emote service is not enabled");
        return;
    }

#if CONFIG_EXAMPLE_EXPRESSIONS_ENABLE_EMOTE
    BROOKESIA_LOGI("Initializing emote service...");

#if !CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER
    BROOKESIA_LOGE("Only supported when board manager is enabled, skip");
#else
    expression::Emote &emote = expression::Emote::get_instance();

    DisplayPeripheralConfig display_config;
    BROOKESIA_CHECK_FALSE_EXIT(board_display_peripheral_init(display_config), "Failed to initialize display peripheral");

    DisplayCallbacks display_callbacks{
        .bitmap_flush_done = [&]()
        {
            // BROOKESIA_LOG_TRACE_GUARD();
            emote.native_notify_flush_finished();
            return false;
        },
    };
    BROOKESIA_CHECK_FALSE_EXIT(
        board_display_register_callbacks(display_callbacks), "Failed to register display callbacks"
    );

    board_display_backlight_set(100);

    // Set emote config before starting the service
    EmoteHelper::Config config{
        .h_res = display_config.h_res,
        .v_res = display_config.v_res,
        .buf_pixels = display_config.h_res * 16,
        .fps = 30,
        .task_priority = 5,
        .task_stack = 10 * 1024,
        .task_affinity = 1,
        .flag_swap_color_bytes = display_config.flag_swap_color_bytes,
        .flag_double_buffer = true,
        .flag_buff_dma = true,
    };
    auto result = EmoteHelper::call_function_sync(EmoteHelper::FunctionId::SetConfig, service::FunctionParameterMap{
        {
            BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::FunctionSetConfigParam::Config),
            BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
        }
    });
    if (!result.has_value()) {
        BROOKESIA_LOGE("Failed to set emote config: %s", result.error().c_str());
    } else {
        BROOKESIA_LOGI("Emote config set successfully");
    }

    // Subscribe to flush ready event
    auto flush_ready_event_handler = [&](const std::string & event, const service::EventItemMap & items) {
        // BROOKESIA_LOG_TRACE_GUARD();
        auto &param_json = std::get<boost::json::object>(
                               items.at(BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventFlushReadyParam::Param))
                           );
        EmoteHelper::FlushReadyEventParam param;
        auto success = BROOKESIA_DESCRIBE_FROM_JSON(param_json, param);
        BROOKESIA_CHECK_FALSE_EXIT(success, "Failed to parse flush ready event param");

        auto result = board_display_draw_bitmap(param.x_start, param.y_start, param.x_end, param.y_end, param.data);
        if (!result) {
            BROOKESIA_LOGE("Failed to draw bitmap, directly notify flush finished");
            emote.native_notify_flush_finished();
        }
    };
    auto connection = EmoteHelper::subscribe_event(EmoteHelper::EventId::FlushReady, flush_ready_event_handler);
    if (connection.connected()) {
        service_connections.push_back(std::move(connection));
    } else {
        BROOKESIA_LOGE("Failed to subscribe to flush ready event");
    }

    auto binding = service_manager.bind(EmoteHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind Emote service");
    } else {
        service_bindings.push_back(std::move(binding));
    }

    // Load emote assets
    {
        EmoteHelper::AssetSource source{
            .source = "anim_icon",
            .type = EmoteHelper::AssetSourceType::PartitionLabel,
            .flag_enable_mmap = false,
        };
        auto result = EmoteHelper::call_function_sync(EmoteHelper::FunctionId::LoadAssetsSource,
        service::FunctionParameterMap{
            {
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::FunctionLoadAssetsParam::Source),
                BROOKESIA_DESCRIBE_TO_JSON(source).as_object()
            }
        });
        if (!result.has_value()) {
            BROOKESIA_LOGE("Failed to load emote assets: %s", result.error().c_str());
        } else {
            BROOKESIA_LOGI("Emote assets loaded successfully");
        }
    }

    {
        auto result = EmoteHelper::call_function_sync(EmoteHelper::FunctionId::SetEmoji,
        service::FunctionParameterMap{
            {
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::FunctionSetEmojiParam::Emoji),
                "winking",
            }
        });
        if (!result.has_value()) {
            BROOKESIA_LOGE("Failed to set emoji: %1%", result.error());
        }
    }
#endif // CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER
#endif // CONFIG_EXAMPLE_EXPRESSIONS_ENABLE_EMOTE
}
