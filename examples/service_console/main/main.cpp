/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_spiffs.h"
#include "private/utils.hpp"
#include "modules/ai_agents.hpp"
#include "modules/board.hpp"
#include "modules/console.hpp"
#include "modules/expression.hpp"
#include "modules/general_services.hpp"
#include "modules/profiler.hpp"

constexpr const char *SPIFFS_MOUNT_ROOT = "/spiffs";
constexpr const char *SPIFFS_PARTITION_LABEL = "spiffs_data";

static void init_spiffs(void);

extern "C" void app_main(void)
{
    /* Initialize SPIFFS */
    init_spiffs();

    /* Initialize board */
    Board::get_instance().init();

    /* Initialize general services */
    GeneralServices::get_instance().init();
    GeneralServices::get_instance().init_audio();
    GeneralServices::get_instance().start_sntp();

    /* Initialize expression */
    Expression::get_instance().init();
    Expression::get_instance().init_emote();

    /* Initialize AI agents */
    AI_Agents::get_instance().init();
    AI_Agents::get_instance().init_coze();
    AI_Agents::get_instance().init_openai();
    AI_Agents::get_instance().init_xiaozhi();

    /* Start profiler */
    Profiler::get_instance().init();
    Profiler::get_instance().start_thread_profiler();
    Profiler::get_instance().start_memory_profiler();

    /* Start console */
    Console::get_instance().start();
}

static void init_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_MOUNT_ROOT,
        .partition_label = SPIFFS_PARTITION_LABEL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, {
        BROOKESIA_LOGE("Failed to mount or format SPIFFS");
    });
    BROOKESIA_LOGI("SPIFFS mounted successfully");
}
