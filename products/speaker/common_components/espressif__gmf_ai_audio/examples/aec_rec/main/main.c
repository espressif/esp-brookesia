/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "esp_gmf_err.h"
#include "esp_gmf_io_codec_dev.h"
#include "esp_gmf_obj.h"
#include "esp_gmf_payload.h"
#include "esp_gmf_port.h"
#include "esp_gmf_aec.h"
#include "esp_gmf_element.h"
#include "esp_gmf_pipeline.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_rate_cvt.h"
#include "esp_gmf_bit_cvt.h"
#include "esp_gmf_ch_cvt.h"
#include "esp_gmf_audio_dec.h"
#include "esp_gmf_app_setup_peripheral.h"
#include "esp_gmf_app_cli.h"
#include "gmf_loader_setup_defaults.h"

#define BOARD_LYRAT_MINI (0)
#define BOARD_KORVO_2    (1)

#if defined CONFIG_IDF_TARGET_ESP32S3
#define AUDIO_BOARD (BOARD_KORVO_2)
#elif defined CONFIG_IDF_TARGET_ESP32
#define AUDIO_BOARD (BOARD_LYRAT_MINI)
#endif  /* defined CONFIG_IDF_TARGET_ESP32S3 */

#if AUDIO_BOARD == BOARD_KORVO_2
#define ADC_I2S_PORT        (0)
#define ADC_I2S_CH          (2)
#define ADC_I2S_BITS        (32)
#define DAC_I2S_PORT        (0)
#define DAC_I2S_CH          (2)
#define DAC_I2S_BITS        (32)
#define INPUT_CH_NUM        (4)
#define INPUT_CH_BITS       (16) /* For board `ESP32-S3-Korvo-2`, the es7210 is configured as 32-bit,
                                   2-channel mode to accommodate 16-bit, 4-channel data */
#define INPUT_CH_ALLOCATION ("RMNM")
#elif AUDIO_BOARD == BOARD_LYRAT_MINI
#define ADC_I2S_PORT        (1)
#define ADC_I2S_CH          (2)
#define ADC_I2S_BITS        (16)
#define DAC_I2S_PORT        (0)
#define DAC_I2S_CH          (1)
#define DAC_I2S_BITS        (16)
#define INPUT_CH_NUM        (ADC_I2S_CH)
#define INPUT_CH_BITS       (ADC_I2S_BITS)
#define INPUT_CH_ALLOCATION ("RM")
#endif  /* AUDIO_BOARD == BOARD_KORVO_2 */

#define ENCODER_ENABLE (false)

static const char *TAG = "AEC_EL_2_FILE";

static uint8_t *pcm_buffer   = NULL;
const uint32_t  buf_size     = 600 * 1024;
static size_t   pcm_received = 0;

static esp_err_t _pipeline_event(esp_gmf_event_pkt_t *event, void *ctx)
{
    ESP_LOGI(TAG, "CB: RECV Pipeline EVT: el:%s-%p, type:%d, sub:%s, payload:%p, size:%d,%p",
             OBJ_GET_TAG(event->from), event->from, event->type, esp_gmf_event_get_state_str(event->sub),
             event->payload, event->payload_size, ctx);
    return ESP_OK;
}

static esp_gmf_err_io_t pcm_buf_acq_write(void *handle, esp_gmf_payload_t *load, int wanted_size, int block_ticks)
{
    return ESP_GMF_IO_OK;
}

static esp_gmf_err_io_t pcm_buf_release_write(void *handle, esp_gmf_payload_t *load, int block_ticks)
{
    if (load == NULL) {
        return ESP_GMF_IO_FAIL;
    }
    if (load->valid_size && (pcm_received + load->valid_size <= buf_size)) {
        memcpy(pcm_buffer + pcm_received, load->buf, load->valid_size);
        pcm_received += load->valid_size;
    }
    return ESP_GMF_IO_OK;
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    pcm_buffer = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (!pcm_buffer) {
        ESP_LOGE(TAG, "Failed to allocate SPIRAM buffer");
        return;
    }
    pcm_received = 0;

    esp_gmf_app_codec_info_t codec_info = ESP_GMF_APP_CODEC_INFO_DEFAULT();
    codec_info.play_info.sample_rate = 48000;
    codec_info.play_info.channel = DAC_I2S_CH;
    codec_info.play_info.bits_per_sample = DAC_I2S_BITS;
    codec_info.record_info.sample_rate = codec_info.play_info.sample_rate;
    codec_info.record_info.channel = ADC_I2S_CH;
    codec_info.record_info.bits_per_sample = ADC_I2S_BITS;
    esp_gmf_app_setup_codec_dev(&codec_info);

    void *sdcard_handle = NULL;
    esp_gmf_app_setup_sdcard(&sdcard_handle);

    esp_gmf_pool_handle_t pool = NULL;
    esp_gmf_pool_init(&pool);
    gmf_loader_setup_all_defaults(pool);

    ESP_GMF_POOL_SHOW_ITEMS(pool);
    esp_gmf_pipeline_handle_t read_pipe = NULL;

#if ENCODER_ENABLE
    const char *name[] = {"aud_rate_cvt", "ai_aec", "aud_enc"};
#else
    const char *name[] = {"aud_rate_cvt", "ai_aec"};
#endif  /* ENCODER_ENABLE */
    esp_gmf_pool_new_pipeline(pool, "io_codec_dev", name, sizeof(name) / sizeof(char *), NULL, &read_pipe);
    if (read_pipe == NULL) {
        ESP_LOGE(TAG, "There is no pipeline");
        return;
    }
    esp_gmf_io_codec_dev_set_dev(ESP_GMF_PIPELINE_GET_IN_INSTANCE(read_pipe), esp_gmf_app_get_record_handle());

    esp_gmf_port_handle_t out_port = NEW_ESP_GMF_PORT_OUT_BYTE(pcm_buf_acq_write, pcm_buf_release_write, NULL, NULL, 1024, portMAX_DELAY);
    esp_gmf_element_register_out_port(read_pipe->last_el, out_port);
    esp_gmf_obj_handle_t rate_cvt = NULL;
    esp_gmf_pipeline_get_el_by_name(read_pipe, "aud_rate_cvt", &rate_cvt);
    esp_gmf_rate_cvt_set_dest_rate(rate_cvt, 16000);

    esp_gmf_info_sound_t info = {
        .format_id = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3,
        .sample_rates = 48000,
        .channels = INPUT_CH_NUM,
        .bits = INPUT_CH_BITS,
    };
    esp_gmf_pipeline_report_info(read_pipe, ESP_GMF_INFO_SOUND, &info, sizeof(info));
    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.thread.core = 1;
    cfg.thread.stack = 5120;
    esp_gmf_task_handle_t read_task = NULL;
    esp_gmf_task_init(&cfg, &read_task);

    esp_gmf_pipeline_bind_task(read_pipe, read_task);
    esp_gmf_pipeline_loading_jobs(read_pipe);
    esp_gmf_pipeline_set_event(read_pipe, _pipeline_event, NULL);
    esp_gmf_pipeline_run(read_pipe);

    esp_gmf_app_cli_init("Audio >", NULL);

    // New pipeline to play 'test.mp3'
    esp_gmf_pipeline_handle_t play_pipe = NULL;
    const char *play_name[] = {"aud_dec", "aud_rate_cvt", "aud_ch_cvt", "aud_bit_cvt"};
    esp_gmf_pool_new_pipeline(pool, "io_file", play_name, sizeof(play_name) / sizeof(char *), "io_codec_dev", &play_pipe);
    if (play_pipe == NULL) {
        ESP_LOGE(TAG, "There is no play pipeline");
        return;
    }
    esp_gmf_io_codec_dev_set_dev(ESP_GMF_PIPELINE_GET_OUT_INSTANCE(play_pipe), esp_gmf_app_get_playback_handle());

    esp_gmf_obj_handle_t bit_cvt = NULL;
    esp_gmf_pipeline_get_el_by_name(play_pipe, "aud_bit_cvt", &aud_bit_cvt);
    esp_gmf_bit_cvt_set_dest_bits(bit_cvt, DAC_I2S_BITS);
    esp_gmf_obj_handle_t ch_cvt = NULL;
    esp_gmf_pipeline_get_el_by_name(play_pipe, "aud_ch_cvt", &ch_cvt);
    esp_gmf_ch_cvt_set_dest_channel(ch_cvt, DAC_I2S_CH);
    esp_gmf_obj_handle_t dec_el = NULL;
    esp_gmf_pipeline_get_el_by_name(play_pipe, "aud_dec", &dec_el);
    esp_gmf_audio_dec_reconfig_by_sound_info(dec_el, &info);

    esp_gmf_pipeline_set_in_uri(play_pipe, "/sdcard/test.mp3");

    esp_gmf_task_handle_t play_task = NULL;
    cfg.thread.core = 0;
    esp_gmf_task_init(&cfg, &play_task);

    esp_gmf_pipeline_bind_task(play_pipe, play_task);
    esp_gmf_pipeline_loading_jobs(play_pipe);
    esp_gmf_pipeline_set_event(play_pipe, _pipeline_event, NULL);
    esp_gmf_pipeline_run(play_pipe);

    vTaskDelay(20000 / portTICK_PERIOD_MS);
    esp_gmf_pipeline_stop(read_pipe);
    esp_gmf_pipeline_stop(play_pipe);

#if ENCODER_ENABLE
    FILE *file = fopen("/sdcard/aec.aac", "wb");
#else
    FILE *file = fopen("/sdcard/aec_16k_16bit_1ch.pcm", "wb");
#endif  /* ENCODER_ENABLE */
    if (file) {
        fwrite(pcm_buffer, 1, pcm_received, file);
        fclose(file);
    } else {
        ESP_LOGE(TAG, "Failed to open file for writing");
    }
    heap_caps_free(pcm_buffer);

    esp_gmf_task_deinit(read_task);
    esp_gmf_task_deinit(play_task);
    esp_gmf_pipeline_destroy(read_pipe);
    esp_gmf_pipeline_destroy(play_pipe);
    gmf_loader_teardown_all_defaults(pool);
    esp_gmf_pool_deinit(pool);
    esp_gmf_app_teardown_codec_dev();
    esp_gmf_app_teardown_sdcard(sdcard_handle);
}
