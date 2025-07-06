/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "soc/soc_caps.h"

#include "esp_gmf_io.h"
#include "esp_gmf_pipeline.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_app_setup_peripheral.h"

#include "esp_gmf_io_codec_dev.h"
#include "esp_gmf_app_cli.h"
#include "gmf_loader_setup_defaults.h"

#if SOC_SDMMC_HOST_SUPPORTED == 1
#define VOICE2FILE     (false)
#endif  /* SOC_SDMMC_HOST_SUPPORTED == 1 */
#ifdef CONFIG_GMF_AI_AUDIO_WAKEUP_ENABLE
#define WAKENET_ENABLE (true)
#else
#define WAKENET_ENABLE (false)
#endif /* CONFIG_GMF_AI_AUDIO_WAKEUP_ENABLE */
#ifdef CONFIG_GMF_AI_AUDIO_VOICE_COMMAND_ENABLE
#define VCMD_ENABLE (true)
#else
#define VCMD_ENABLE (false)
#endif /* CONFIG_GMF_AI_AUDIO_VOICE_COMMAND_ENABLE */
#define VAD_ENABLE     (true)
#define QUIT_CMD_FOUND (BIT0)

#define BOARD_LYRAT_MINI (0)
#define BOARD_KORVO_2    (1)
#define BOARD_XD_AIOT_C3 (2)
#define BOARD_ESP_SPOT   (3)

#if defined CONFIG_IDF_TARGET_ESP32S3
#define WITH_AFE    (true)
#define AUDIO_BOARD (BOARD_KORVO_2)
#elif defined CONFIG_IDF_TARGET_ESP32
#define WITH_AFE    (true)
#define AUDIO_BOARD (BOARD_LYRAT_MINI)
#elif defined CONFIG_IDF_TARGET_ESP32C3
#define WITH_AFE    (false)
#define AUDIO_BOARD (BOARD_XD_AIOT_C3)
#elif defined CONFIG_IDF_TARGET_ESP32C5
#define WITH_AFE    (false)
#define AUDIO_BOARD (BOARD_ESP_SPOT)
#endif  /* defined CONFIG_IDF_TARGET_ESP32S3 */

#if AUDIO_BOARD == BOARD_KORVO_2
#define ADC_I2S_CH          (2)
#define ADC_I2S_BITS        (32)
#define INPUT_CH_NUM        (4)
#define INPUT_CH_BITS       (16) /* For board `ESP32-S3-Korvo-2`, the es7210 is configured as 32-bit,
                                   2-channel mode to accommodate 16-bit, 4-channel data */
#define INPUT_CH_ALLOCATION ("RMNM")
#elif AUDIO_BOARD == BOARD_LYRAT_MINI
#define ADC_I2S_CH          (2)
#define ADC_I2S_BITS        (16)
#define INPUT_CH_NUM        (ADC_I2S_CH)
#define INPUT_CH_BITS       (ADC_I2S_BITS)
#define INPUT_CH_ALLOCATION ("RM")
#elif AUDIO_BOARD == BOARD_XD_AIOT_C3
#define ADC_I2S_CH          (2)
#define ADC_I2S_BITS        (16)
#define INPUT_CH_NUM        (ADC_I2S_CH)
#define INPUT_CH_BITS       (ADC_I2S_BITS)
#define INPUT_CH_ALLOCATION ("MR")
#elif AUDIO_BOARD == BOARD_ESP_SPOT
#define ADC_I2S_CH          (2)
#define ADC_I2S_BITS        (16)
#define INPUT_CH_NUM        (ADC_I2S_CH)
#define INPUT_CH_BITS       (ADC_I2S_BITS)
#define INPUT_CH_ALLOCATION ("MR")
#endif  /* AUDIO_BOARD == BOARD_KORVO_2 */

#if WITH_AFE == true
#include "esp_gmf_afe.h"
#else
#include "esp_gmf_wn.h"
#endif  /* WITH_AFE == true */

static const char *TAG = "AI_AUDIO_WWE";

#if WITH_AFE == true
static bool speeching = false;
static bool wakeup    = false;
#endif  /* WITH_AFE == true */
static EventGroupHandle_t g_event_group = NULL;

static esp_err_t _pipeline_event(esp_gmf_event_pkt_t *event, void *ctx)
{
    ESP_LOGI(TAG, "CB: RECV Pipeline EVT: el:%s-%p, type:%d, sub:%s, payload:%p, size:%d,%p",
             OBJ_GET_TAG(event->from), event->from, event->type, esp_gmf_event_get_state_str(event->sub),
             event->payload, event->payload_size, ctx);
    return 0;
}

#if WITH_AFE == true
void esp_gmf_afe_event_cb(esp_gmf_obj_handle_t obj, esp_gmf_afe_evt_t *event, void *user_data)
{
    switch (event->type) {
        case ESP_GMF_AFE_EVT_WAKEUP_START: {
            wakeup = true;
#if WAKENET_ENABLE == true && VCMD_ENABLE == true
            esp_gmf_afe_vcmd_detection_cancel(obj);
            esp_gmf_afe_vcmd_detection_begin(obj);
#endif  /* WAKENET_ENABLE == true && VCMD_ENABLE == true */
            esp_gmf_afe_wakeup_info_t *info = event->event_data;
            ESP_LOGI(TAG, "WAKEUP_START [%d : %d]", info->wake_word_index, info->wakenet_model_index);
            break;
        }
        case ESP_GMF_AFE_EVT_WAKEUP_END: {
            wakeup = false;
#if WAKENET_ENABLE == true && VCMD_ENABLE == true
            esp_gmf_afe_vcmd_detection_cancel(obj);
#endif  /* WAKENET_ENABLE == true && VCMD_ENABLE == true */
            ESP_LOGI(TAG, "WAKEUP_END");
            break;
        }
        case ESP_GMF_AFE_EVT_VAD_START: {
#if WAKENET_ENABLE != true && VCMD_ENABLE == true
            esp_gmf_afe_vcmd_detection_cancel(obj);
            esp_gmf_afe_vcmd_detection_begin(obj);
#endif  /* WAKENET_ENABLE != true && VCMD_ENABLE == true */
            speeching = true;
            ESP_LOGI(TAG, "VAD_START");
            break;
        }
        case ESP_GMF_AFE_EVT_VAD_END: {
#if WAKENET_ENABLE != true && VCMD_ENABLE == true
            esp_gmf_afe_vcmd_detection_cancel(obj);
#endif  /* WAKENET_ENABLE != true && VCMD_ENABLE == true */
            speeching = false;
            ESP_LOGI(TAG, "VAD_END");
            break;
        }
        case ESP_GMF_AFE_EVT_VCMD_DECT_TIMEOUT: {
            ESP_LOGI(TAG, "VCMD_DECT_TIMEOUT");
            break;
        }
        default: {
            esp_gmf_afe_vcmd_info_t *info = event->event_data;
            ESP_LOGW(TAG, "Command %d, phrase_id %d, prob %f, str: %s",
                     event->type, info->phrase_id, info->prob, info->str);
            /* Here use the first command to quit this demo
             * For Chinese model, the first default command is `ba xiao shi hou guan ji`
             * For English model, the first default command is `tell me a joke`
             * If user had modified the commands, please refer to the commands setting.
             */
            if (event->type == 1) {
                xEventGroupSetBits(g_event_group, QUIT_CMD_FOUND);
            }
            /* Here use the third command to enable the `keep awake` mode of gmf_afe
             * For Chinese model, the second default command is `ba xiao shi hou kai ji`
             * For English model, the second default command is `sing a song`
             * If user had modified the commands, please refer to the commands setting.
             */
            else if (event->type == 2) {
                esp_gmf_afe_keep_awake(obj, true);
            }
            /* Here use the fourth command to disable the `keep awake` mode of gmf_afe
             * For Chinese model, the third default command is `bi kai wo chui`
             * For English model, the third default command is `play new channel`
             * If user had modified the commands, please refer to the commands setting.
             */
            else if (event->type == 3) {
                esp_gmf_afe_keep_awake(obj, false);
            }
            break;
        }
    }
}
#else
static void esp_gmf_wn_event_cb(esp_gmf_obj_handle_t obj, int32_t trigger_ch, void *user_ctx)
{
    static int32_t cnt = 1;
    ESP_LOGI(TAG, "WWE detected on channel %" PRIi32 ", cnt: %" PRIi32, trigger_ch, cnt++);
    if (cnt >= 10) {
        xEventGroupSetBits(g_event_group, QUIT_CMD_FOUND);
    }
}
#endif  /* WITH_AFE == true */

static void voice_2_file(uint8_t *buffer, int len)
{
#if VOICE2FILE == true
#define MAX_FNAME_LEN (50)

    static FILE *fp = NULL;
    static int fcnt = 0;

    if (speeching) {
        if (!fp) {
            char fname[MAX_FNAME_LEN] = {0};
            snprintf(fname, MAX_FNAME_LEN - 1, "/sdcard/16k_16bit_1ch_%d.pcm", fcnt++);
            fp = fopen(fname, "wb");
            if (!fp) {
                ESP_LOGE(TAG, "File open failed");
                return;
            }
        }
        if (len) {
            fwrite(buffer, len, 1, fp);
        }
    } else {
        if (fp) {
            ESP_LOGI(TAG, "File closed");
            fclose(fp);
            fp = NULL;
        }
    }
#endif  /* VOICE2FILE == true */
}

static esp_gmf_err_io_t outport_acquire_write(void *handle, esp_gmf_payload_t *load, int wanted_size, int block_ticks)
{
    ESP_LOGD(TAG, "Acquire write");
    return ESP_GMF_IO_OK;
}

static esp_gmf_err_io_t outport_release_write(void *handle, esp_gmf_payload_t *load, int block_ticks)
{
    ESP_LOGD(TAG, "Release write");
    voice_2_file(load->buf, load->valid_size);
    return ESP_GMF_IO_OK;
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_gmf_app_codec_info_t codec_info = ESP_GMF_APP_CODEC_INFO_DEFAULT();
    codec_info.record_info.sample_rate = 16000;
    codec_info.record_info.channel = ADC_I2S_CH;
    codec_info.record_info.bits_per_sample = ADC_I2S_BITS;
    codec_info.play_info.sample_rate = codec_info.record_info.sample_rate;
    esp_gmf_app_setup_codec_dev(&codec_info);
    void *sdcard_handle = NULL;
    esp_gmf_app_setup_sdcard(&sdcard_handle);
    g_event_group = xEventGroupCreate();

    esp_gmf_pool_handle_t pool = NULL;
    esp_gmf_pool_init(&pool);
    gmf_loader_setup_all_defaults(pool);

    esp_gmf_pipeline_handle_t pipe = NULL;
#if WITH_AFE == true
    const char *name[] = {"ai_afe"};
#else
    const char *name[] = {"ai_wn"};
#endif  /* WITH_AFE == true */
    esp_gmf_pool_new_pipeline(pool, "io_codec_dev", name, sizeof(name) / sizeof(char *), NULL, &pipe);
    if (pipe == NULL) {
        ESP_LOGE(TAG, "There is no pipeline");
        goto __quit;
    }
    esp_gmf_io_codec_dev_set_dev(ESP_GMF_PIPELINE_GET_IN_INSTANCE(pipe), esp_gmf_app_get_record_handle());
#if WITH_AFE == true
    esp_gmf_element_handle_t afe = NULL;
    esp_gmf_pipeline_get_el_by_name(pipe, "ai_afe", &afe);
    esp_gmf_afe_set_event_cb(afe, esp_gmf_afe_event_cb, NULL);
#else
    esp_gmf_element_handle_t wn = NULL;
    esp_gmf_pipeline_get_el_by_name(pipe, "ai_wn", &wn);
    esp_gmf_wn_set_detect_cb(wn, esp_gmf_wn_event_cb, NULL);
#endif  /* WITH_AFE == true */
    esp_gmf_port_handle_t outport = NEW_ESP_GMF_PORT_OUT_BYTE(outport_acquire_write,
                                                              outport_release_write,
                                                              NULL,
                                                              NULL,
                                                              2048,
                                                              100);
    esp_gmf_pipeline_reg_el_port(pipe, name[0], ESP_GMF_IO_DIR_WRITER, outport);

    esp_gmf_info_sound_t info = {
        .sample_rates = 16000,
        .channels = INPUT_CH_NUM,
        .bits = INPUT_CH_BITS,
    };
    esp_gmf_pipeline_report_info(pipe, ESP_GMF_INFO_SOUND, &info, sizeof(info));

    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    cfg.thread.core = 0;
    cfg.thread.prio = 5;
    cfg.thread.stack = 5120;
    esp_gmf_task_handle_t task = NULL;
    esp_gmf_task_init(&cfg, &task);
    esp_gmf_pipeline_bind_task(pipe, task);
    esp_gmf_pipeline_loading_jobs(pipe);
    esp_gmf_pipeline_set_event(pipe, _pipeline_event, NULL);
    esp_gmf_pipeline_run(pipe);

    esp_gmf_app_cli_init("Audio >", NULL);

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(g_event_group, QUIT_CMD_FOUND, pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & QUIT_CMD_FOUND) {
            ESP_LOGI(TAG, "Quit command found, stopping pipeline");
            break;
        }
    }

__quit:
    esp_gmf_pipeline_stop(pipe);
    esp_gmf_task_deinit(task);
    esp_gmf_pipeline_destroy(pipe);
    gmf_loader_teardown_all_defaults(pool);
    esp_gmf_pool_deinit(pool);
    esp_gmf_app_teardown_codec_dev();
    esp_gmf_app_teardown_sdcard(sdcard_handle);
    vEventGroupDelete(g_event_group);
    ESP_LOGW(TAG, "Wake word engine demo finished");
}
