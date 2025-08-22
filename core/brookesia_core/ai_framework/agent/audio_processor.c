/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <string.h>
#include <math.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "esp_gmf_element.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_io.h"
#include "esp_gmf_pipeline.h"
#include "esp_gmf_ringbuffer.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_rate_cvt.h"
#include "esp_g711_enc.h"
#include "esp_gmf_audio_helper.h"
#include "esp_audio_simple_player.h"
#include "esp_audio_simple_player_advance.h"
#include "esp_gmf_setup_pool.h"
#include "esp_gmf_setup_peripheral.h"
#include "esp_codec_dev.h"
#include "esp_gmf_fifo.h"

#include "esp_gmf_ringbuffer.h"

#ifndef CONFIG_KEY_PRESS_DIALOG_MODE
#include "esp_vad.h"
#include "esp_afe_config.h"
#include "esp_gmf_afe_manager.h"
#include "esp_gmf_afe.h"
#endif  /* CONFIG_KEY_PRESS_DIALOG_MODE */
#include "audio_processor.h"

#define VAD_ENABLE       (true)
#define VCMD_ENABLE      (false)
#define DEFAULT_FIFO_NUM (5)

#define DEFAULT_PLAYBACK_VOLUME (70)

#define DEFAULT_FEED_TASK_PRIO          (6)
#define DEFAULT_FEED_TASK_STACK_SIZE    (5 * 1024)
#define DEFAULT_FETCH_TASK_PRIO         (6)
#define DEFAULT_FETCH_TASK_STACK_SIZE   (5 * 1024)
#define DEFAULT_GMF_TASK_PRIO           (6)
#define DEFAULT_GMF_TASK_STACK_SIZE     (5 * 1024)

#define AFE_WAKEUP_END_MS       (30000)

static char *TAG = "AUDIO_PROCESSOR";

typedef struct {
    esp_asp_handle_t          player;
    enum audio_player_state_e state;
} audio_prompt_t;

typedef struct {
    esp_gmf_fifo_handle_t         fifo;
    recorder_event_callback_t     cb;
    void                         *ctx;
    enum audio_player_state_e     state;
#ifndef CONFIG_KEY_PRESS_DIALOG_MODE
    esp_gmf_pipeline_handle_t     pipe;
    esp_gmf_afe_manager_handle_t  afe_manager;
    afe_config_t                 *afe_cfg;
    esp_gmf_task_handle_t         task;
#endif  /* CONFIG_KEY_PRESS_DIALOG_MODE */
} audio_recordert_t;

typedef struct {
    esp_asp_handle_t          player;
    esp_gmf_fifo_handle_t     fifo;
    enum audio_player_state_e state;
} audio_playback_t;

typedef struct {
    esp_codec_dev_handle_t play_dev;
    esp_codec_dev_handle_t rec_dev;
    esp_gmf_pool_handle_t  pool;
} audio_manager_t;

#define AUDIO_BUFFER_SIZE   1024 * sizeof(int16_t)
#define GAUSSIAN_SIGMA      1.0  // Gaussian filter standard deviation
static esp_gmf_rb_handle_t out_rb = NULL;
float *gaussian_weights;

static audio_manager_t   audio_manager;
static audio_recordert_t audio_recorder;
static audio_playback_t  audio_playback;
static audio_prompt_t    audio_prompt;

esp_err_t audio_manager_init(esp_gmf_setup_periph_hardware_info *info, void **play_dev, void **rec_dev)
{
    esp_gmf_setup_periph(info);
    esp_gmf_setup_periph_codec(&audio_manager.play_dev, &audio_manager.rec_dev);
    esp_gmf_pool_init(&audio_manager.pool);
    pool_register_io(audio_manager.pool);
    pool_register_audio_codecs(audio_manager.pool);
    pool_register_audio_effects(audio_manager.pool);
    pool_register_codec_dev_io(audio_manager.pool, audio_manager.play_dev, audio_manager.rec_dev);
    esp_codec_dev_set_out_vol(audio_manager.play_dev, DEFAULT_PLAYBACK_VOLUME);
    // esp_codec_dev_set_in_gain(audio_manager.rec_dev, 21);

    if (play_dev != NULL) {
        *play_dev = audio_manager.play_dev;
    }
    if (rec_dev != NULL) {
        *rec_dev = audio_manager.rec_dev;
    }

    return ESP_OK;
}

esp_err_t audio_manager_deinit()
{
    pool_unregister_audio_codecs();
    esp_gmf_pool_deinit(audio_manager.pool);
    esp_gmf_teardown_periph_codec(audio_manager.play_dev, audio_manager.rec_dev);
    return ESP_OK;
}

esp_err_t audio_manager_suspend(bool suspend)
{
    return esp_gmf_afe_manager_suspend(audio_recorder.afe_manager, suspend);
}

static int prompt_out_data_callback(uint8_t *data, int data_size, void *ctx)
{
    esp_codec_dev_write(audio_manager.play_dev, data, data_size);
    return 0;
}

static int prompt_event_callback(esp_asp_event_pkt_t *event, void *ctx)
{
    if (event->type == ESP_ASP_EVENT_TYPE_MUSIC_INFO) {
        esp_asp_music_info_t info = {0};
        memcpy(&info, event->payload, event->payload_size);
        ESP_LOGI(TAG, "Get info, rate:%d, channels:%d, bits:%d", info.sample_rate, info.channels, info.bits);
    } else if (event->type == ESP_ASP_EVENT_TYPE_STATE) {
        esp_asp_state_t st = 0;
        memcpy(&st, event->payload, event->payload_size);
        ESP_LOGI(TAG, "Get State, %d,%s", st, esp_audio_simple_player_state_to_str(st));
        if (((st == ESP_ASP_STATE_STOPPED) || (st == ESP_ASP_STATE_FINISHED) || (st == ESP_ASP_STATE_ERROR))) {
            audio_prompt.state = AUDIO_PLAYER_STATE_IDLE;
            audio_prompt_play_mute(true);
            vTaskDelay(pdMS_TO_TICKS(100));
            audio_prompt_play_mute(false);
        }
    }
    return 0;
}

#ifndef CONFIG_KEY_PRESS_DIALOG_MODE
static esp_err_t recorder_pipeline_event(esp_gmf_event_pkt_t *event, void *ctx)
{
    ESP_LOGD(TAG, "CB: RECV Pipeline EVT: el:%s-%p, type:%d, sub:%s, payload:%p, size:%d,%p",
             OBJ_GET_TAG(event->from), event->from, event->type, esp_gmf_event_get_state_str(event->sub),
             event->payload, event->payload_size, ctx);
    return 0;
}

#include "esp_timer.h"

static int recorder_outport_acquire_write(void *handle, esp_gmf_data_bus_block_t *blk, int wanted_size, int block_ticks)
{
    esp_gmf_rb_acquire_write(out_rb, blk, wanted_size, block_ticks);
    return wanted_size;
}

static int recorder_outport_release_write(void *handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    int ret = 0;
    if (blk->valid_size) {
        ret = blk->valid_size;
    } else {
        printf("||||| release write, valid_size: %d\n", blk->valid_size);
    }
    esp_gmf_rb_release_write(out_rb, blk, portMAX_DELAY);
    return ret;
}

static int recorder_inport_acquire_read(void *handle, esp_gmf_payload_t *load, int wanted_size, int block_ticks)
{
    load->valid_size = wanted_size;
    esp_codec_dev_read(audio_manager.rec_dev, load->buf, wanted_size);
    return wanted_size;
}

static int recorder_inport_release_read(void *handle, esp_gmf_payload_t *load, int block_ticks)
{
    return load->valid_size;
}

static void esp_gmf_afe_event_cb(esp_gmf_obj_handle_t obj, esp_gmf_afe_evt_t *event, void *user_data)
{
    audio_recorder.cb((void *)event, audio_recorder.ctx);
    switch (event->type) {
    case ESP_GMF_AFE_EVT_WAKEUP_START: {
        // wakeup = true;
#if CONFIG_LANGUAGE_WAKEUP_MODE
        esp_gmf_afe_vcmd_detection_cancel(obj);
        esp_gmf_afe_vcmd_detection_begin(obj);
#endif  /* CONFIG_LANGUAGE_WAKEUP_MODE */
        esp_gmf_afe_wakeup_info_t *info = event->event_data;
        ESP_LOGI(TAG, "WAKEUP_START [%d : %d]", info->wake_word_index, info->wakenet_model_index);
        break;
    }
    case ESP_GMF_AFE_EVT_WAKEUP_END: {
#if CONFIG_LANGUAGE_WAKEUP_MODE
        esp_gmf_afe_vcmd_detection_cancel(obj);
#endif  /* CONFIG_LANGUAGE_WAKEUP_MODE */
        ESP_LOGI(TAG, "WAKEUP_END");
        break;
    }
    case ESP_GMF_AFE_EVT_VAD_START: {
#ifndef CONFIG_LANGUAGE_WAKEUP_MODE
        esp_gmf_afe_vcmd_detection_cancel(obj);
        esp_gmf_afe_vcmd_detection_begin(obj);
#endif  /* CONFIG_LANGUAGE_WAKEUP_MODE */
        ESP_LOGI(TAG, "VAD_START");
        break;
    }
    case ESP_GMF_AFE_EVT_VAD_END: {
#ifndef CONFIG_LANGUAGE_WAKEUP_MODE
        esp_gmf_afe_vcmd_detection_cancel(obj);
#endif  /* CONFIG_LANGUAGE_WAKEUP_MODE */
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
        break;
    }
    }
}
#endif  /* CONFIG_KEY_PRESS_DIALOG_MODE */

esp_err_t audio_gmf_trigger_wakeup()
{
    esp_gmf_trigger_wakeup(audio_processor_get_afe_handle());
    return ESP_OK;
}

esp_err_t audio_recorder_open(recorder_event_callback_t cb, void *ctx)
{
    esp_gmf_rb_create(1, 1024 * 3, &out_rb);
#if CONFIG_KEY_PRESS_DIALOG_MODE
    (void)cb;
    (void)ctx;
    return ESP_OK;
#else

    esp_gmf_setup_periph_hardware_info hardware_info = {};
    esp_gmf_get_periph_info(&hardware_info);

    srmodel_list_t *models = esp_srmodel_init("model");
    const char *ch_format = hardware_info.codec.type == ESP_GMF_CODEC_TYPE_ES7210_IN_ES8311_OUT ? "RMNM" : "MR";
    audio_recorder.afe_cfg = afe_config_init(ch_format, models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    audio_recorder.afe_cfg->vad_init = VAD_ENABLE;
    audio_recorder.afe_cfg->vad_mode = VAD_MODE_3;
    audio_recorder.afe_cfg->vad_min_speech_ms = 64;
    audio_recorder.afe_cfg->vad_min_noise_ms = 1000;
    audio_recorder.afe_cfg->agc_init = true;
    audio_recorder.afe_cfg->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    audio_recorder.afe_cfg->wakenet_init = true;
    audio_recorder.afe_cfg->aec_init = true;
    esp_gmf_afe_manager_cfg_t afe_manager_cfg = DEFAULT_GMF_AFE_MANAGER_CFG(audio_recorder.afe_cfg, NULL, NULL, NULL, NULL);
    afe_manager_cfg.feed_task_setting.prio = DEFAULT_FEED_TASK_PRIO;
    afe_manager_cfg.feed_task_setting.stack_size = DEFAULT_FEED_TASK_STACK_SIZE;
    afe_manager_cfg.fetch_task_setting.prio = DEFAULT_FETCH_TASK_PRIO;
    afe_manager_cfg.fetch_task_setting.stack_size = DEFAULT_FETCH_TASK_STACK_SIZE;
    esp_gmf_afe_manager_create(&afe_manager_cfg, &audio_recorder.afe_manager);
    esp_gmf_element_handle_t gmf_afe = NULL;
    esp_gmf_afe_cfg_t gmf_afe_cfg = DEFAULT_GMF_AFE_CFG(audio_recorder.afe_manager, esp_gmf_afe_event_cb, NULL, models);
    gmf_afe_cfg.vcmd_detect_en = VCMD_ENABLE;
    gmf_afe_cfg.wakeup_end = AFE_WAKEUP_END_MS;
    esp_gmf_afe_init(&gmf_afe_cfg, &gmf_afe);
    esp_gmf_pool_register_element(audio_manager.pool, gmf_afe, NULL);
    const char *name[] = {"ai_afe", "rate_cvt", "encoder"};
    esp_gmf_pool_new_pipeline(audio_manager.pool, NULL, name, sizeof(name) / sizeof(char *), NULL, &audio_recorder.pipe);
    if (audio_recorder.pipe == NULL) {
        ESP_LOGE(TAG, "There is no pipeline");
        goto __quit;
    }
    // printf("---》 outport acquire: %p\n", recorder_outport_acquire_write);
    esp_gmf_port_handle_t outport = NULL;
    outport = NEW_ESP_GMF_PORT_OUT_BYTE(recorder_outport_acquire_write,
                                        recorder_outport_release_write,
                                        NULL,
                                        &outport,
                                        0,
                                        100);
    esp_gmf_pipeline_reg_el_port(audio_recorder.pipe, "encoder", ESP_GMF_IO_DIR_WRITER, outport);

    esp_gmf_port_handle_t import = NEW_ESP_GMF_PORT_IN_BYTE(recorder_inport_acquire_read,
                                   recorder_inport_release_read,
                                   NULL,
                                   NULL,
                                   2048,
                                   100);
    esp_gmf_pipeline_reg_el_port(audio_recorder.pipe, "ai_afe", ESP_GMF_IO_DIR_READER, import);

    esp_gmf_obj_handle_t rate_cvt = NULL;
    esp_gmf_pipeline_get_el_by_name(audio_recorder.pipe, "rate_cvt", &rate_cvt);
    esp_gmf_rate_cvt_set_dest_rate(rate_cvt, 8000);

    esp_gmf_element_handle_t enc_handle = NULL;
    esp_gmf_pipeline_get_el_by_name(audio_recorder.pipe, "encoder", &enc_handle);

    esp_gmf_info_sound_t info = { 0 };
    info.sample_rates = 8000;
    info.channels = 1;
    info.bits = 16;
    esp_gmf_audio_helper_reconfig_enc_by_type(ESP_AUDIO_TYPE_G711A, &info, (esp_audio_enc_config_t *)OBJ_GET_CFG(enc_handle));

    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    cfg.thread.core = 0;
    cfg.thread.prio = DEFAULT_GMF_TASK_PRIO;
    cfg.thread.stack = DEFAULT_GMF_TASK_STACK_SIZE;
    cfg.thread.stack_in_ext = true;
    esp_gmf_task_init(&cfg, &audio_recorder.task);
    esp_gmf_pipeline_bind_task(audio_recorder.pipe, audio_recorder.task);
    esp_gmf_pipeline_loading_jobs(audio_recorder.pipe);
    esp_gmf_pipeline_set_event(audio_recorder.pipe, recorder_pipeline_event, NULL);
    esp_gmf_pipeline_run(audio_recorder.pipe);

    audio_recorder.cb = cb;
    audio_recorder.ctx = ctx;
    return ESP_OK;
__quit:
    esp_gmf_pipeline_stop(audio_recorder.pipe);
    esp_gmf_task_deinit(audio_recorder.task);
    afe_config_free(audio_recorder.afe_cfg);
    esp_gmf_afe_manager_destroy(audio_recorder.afe_manager);
    return ESP_FAIL;
#endif  /* CONFIG_KEY_PRESS_DIALOG_MODE */
}

esp_err_t audio_recorder_close(void)
{
    if (audio_recorder.state == AUDIO_PLAYER_STATE_CLOSED) {
        ESP_LOGW(TAG, "Audio recorded is ready closed");
        return ESP_OK;
    }
#ifndef CONFIG_KEY_PRESS_DIALOG_MODE
    esp_gmf_pipeline_destroy(audio_recorder.pipe);
    esp_gmf_task_deinit(audio_recorder.task);
    afe_config_free(audio_recorder.afe_cfg);
    esp_gmf_afe_manager_destroy(audio_recorder.afe_manager);
#endif  /* CONFIG_KEY_PRESS_DIALOG_MODE */
    audio_recorder.state = AUDIO_PLAYER_STATE_CLOSED;
    return ESP_OK;
}

esp_err_t audio_recorder_read_data(uint8_t *data, int data_size)
{
    // TODO: will support opus data
#if CONFIG_KEY_PRESS_DIALOG_MODE
    esp_codec_dev_read(audio_manager.rec_dev, data, data_size);
    return data_size;
#else
    esp_gmf_data_bus_block_t blk;
    blk.buf = malloc(data_size);
    blk.buf_length = data_size;
    blk.valid_size = 0;
    blk.is_last = false;
    esp_gmf_rb_acquire_read(out_rb, &blk, data_size, portMAX_DELAY);
    memcpy(data, blk.buf, blk.valid_size);

    esp_gmf_rb_release_read(out_rb, &blk, portMAX_DELAY);
    free(blk.buf);
    return blk.valid_size;
#endif  /* CONFIG_KEY_PRESS_DIALOG_MODE */
}

esp_err_t audio_playback_feed_data(uint8_t *data, int data_size)
{
    esp_gmf_data_bus_block_t blk = {0};
    int ret = esp_gmf_fifo_acquire_write(audio_playback.fifo, &blk, data_size, portMAX_DELAY);
    if (ret < 0) {
        ESP_LOGE(TAG, "Fifo acquire write failed (0x%x)", ret);
        return ESP_FAIL;
    }
    memcpy(blk.buf, data, data_size);
    blk.valid_size = data_size;
    esp_gmf_fifo_release_write(audio_playback.fifo, &blk, portMAX_DELAY);
    return ESP_OK;
}

static int playback_read_callback(uint8_t *data, int data_size, void *ctx)
{
    esp_gmf_data_bus_block_t blk = {0};
    int ret = esp_gmf_fifo_acquire_read(audio_playback.fifo, &blk, data_size,
                                        portMAX_DELAY);
    if (ret < 0) {
        ESP_LOGE(TAG, "Fifo acquire read failed (0x%x)", ret);
    }
    memcpy(data, blk.buf, blk.valid_size);
    esp_gmf_fifo_release_read(audio_playback.fifo, &blk, 0);
    return blk.valid_size;
}

static int playback_write_callback(uint8_t *data, int data_size, void *ctx)
{
    if (audio_prompt.state == AUDIO_PLAYER_STATE_PLAYING) {
        ESP_LOGW(TAG, "Audio prompt is playing, skip\n");
        return data_size;
    }
    esp_codec_dev_handle_t dev = (esp_codec_dev_handle_t)ctx;
    int ret =  esp_codec_dev_write(dev, data, data_size);
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Write to codec dev failed (0x%x)\n", ret);
        return -1;
    }
    return data_size;
}

static int playback_event_callback(esp_asp_event_pkt_t *event, void *ctx)
{
    if (event->type == ESP_ASP_EVENT_TYPE_MUSIC_INFO) {
        esp_asp_music_info_t info;
        memcpy(&info, event->payload, event->payload_size);
        ESP_LOGI(TAG, "Get info, rate:%d, channels:%d, bits:%d", info.sample_rate,
                 info.channels, info.bits);
    } else if (event->type == ESP_ASP_EVENT_TYPE_STATE) {
        esp_asp_state_t st = ESP_ASP_STATE_NONE;
        memcpy(&st, event->payload, event->payload_size);
        ESP_LOGI(TAG, "Get State, %d,%s", st,
                 esp_audio_simple_player_state_to_str(st));
        if (ctx && ((st == ESP_ASP_STATE_STOPPED) || (st == ESP_ASP_STATE_FINISHED) || (st == ESP_ASP_STATE_ERROR))) {
            audio_prompt.state = AUDIO_PLAYER_STATE_IDLE;
        }
    }
    return 0;
}

esp_err_t audio_playback_open(void)
{
    esp_err_t err = ESP_GMF_ERR_OK;

    do {
        err = esp_gmf_fifo_create(DEFAULT_FIFO_NUM, 1, &audio_playback.fifo);
        if (err != ESP_GMF_ERR_OK) {
            ESP_LOGE(TAG, "oai_plr_dec_fifo init failed (0x%x)", err);
            break;
        }
        esp_asp_cfg_t player_cfg = {0};
        player_cfg.in.cb = playback_read_callback;
        player_cfg.in.user_ctx = audio_playback.fifo;
        player_cfg.out.cb = playback_write_callback;
        player_cfg.out.user_ctx = audio_manager.play_dev;
        player_cfg.task_prio = 5;
        player_cfg.task_stack = 12 * 1024;
        player_cfg.task_core = 1;

        err = esp_audio_simple_player_new(&player_cfg, &audio_playback.player);
        if (err != ESP_GMF_ERR_OK) {
            ESP_LOGE(TAG, "simple_player init failed (0x%x)", err);
            break;
        }
        err = esp_audio_simple_player_set_event(audio_playback.player,
                                                playback_event_callback, NULL);
        if (err != ESP_GMF_ERR_OK) {
            ESP_LOGE(TAG, "set_event failed (0x%x)", err);
            break;
        }
        audio_playback.state = AUDIO_PLAYER_STATE_IDLE;
        return ESP_OK;

    } while (0);

    if (audio_playback.player) {
        esp_audio_simple_player_destroy(audio_playback.player);
    }
    if (audio_playback.fifo) {
        esp_gmf_fifo_destroy(audio_playback.fifo);
    }
    return err;
}

esp_err_t audio_playback_close(void)
{
    if (audio_playback.state == AUDIO_PLAYER_STATE_CLOSED) {
        ESP_LOGW(TAG, "Aduio playback is realdy closed");
        return ESP_OK;
    }
    if (audio_playback.state == AUDIO_PLAYER_STATE_PLAYING) {
        audio_playback_stop();
    }
    esp_err_t err = esp_audio_simple_player_destroy(audio_playback.player);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Audio playback closing failed");
        return ESP_FAIL;
    }
    audio_playback.state = AUDIO_PLAYER_STATE_CLOSED;
    return ESP_OK;
}

esp_err_t audio_playback_run(void)
{
    if (audio_playback.state == AUDIO_PLAYER_STATE_PLAYING) {
        ESP_LOGW(TAG, "Audio playback is realdy running");
        return ESP_OK;
    }
    esp_asp_music_info_t music_info;
    music_info.sample_rate = 16000;
    music_info.channels = 1;
    music_info.bits = 16;
    music_info.bitrate = 0;

    esp_err_t err = esp_audio_simple_player_run(audio_playback.player, "raw://sdcard/coze.opus",
                    &music_info);
    if (err != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "run failed (0x%x)", err);
        return ESP_FAIL;
    }
    audio_playback.state = AUDIO_PLAYER_STATE_PLAYING;
    return ESP_OK;
}

esp_err_t audio_playback_stop(void)
{
    if (audio_playback.state == AUDIO_PLAYER_STATE_IDLE) {
        ESP_LOGW(TAG, "Audio playback is already stopped");
        return ESP_OK;
    }
    esp_err_t ret = esp_audio_simple_player_stop(audio_playback.player);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Audio playback stop failed");
        return ESP_FAIL;
    }
    audio_playback.state = AUDIO_PLAYER_STATE_IDLE;
    return ESP_OK;
}

esp_err_t audio_prompt_open(void)
{
    esp_asp_cfg_t cfg = {
        .in.cb = NULL,
        .in.user_ctx = NULL,
        .out.cb = prompt_out_data_callback,
        .out.user_ctx = NULL,
        .task_prio = 5,
    };
    esp_gmf_err_t err = esp_audio_simple_player_new(&cfg, &audio_prompt.player);
    err = esp_audio_simple_player_set_event(audio_prompt.player, prompt_event_callback, NULL);
    audio_prompt.state = AUDIO_PLAYER_STATE_IDLE;
    return err;
}

esp_err_t audio_prompt_close(void)
{
    if (audio_prompt.state == AUDIO_PLAYER_STATE_PLAYING) {
        esp_audio_simple_player_stop(audio_prompt.player);
    }
    esp_err_t err = esp_audio_simple_player_destroy(audio_prompt.player);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Audio prompt closing failed");
        return ESP_FAIL;
    }
    audio_prompt.state = AUDIO_PLAYER_STATE_CLOSED;
    return ESP_OK;
}

esp_err_t audio_prompt_play(const char *url)
{
    if (audio_prompt.state == AUDIO_PLAYER_STATE_PLAYING) {
        ESP_LOGE(TAG, "audio_prompt is already playing");
        return ESP_OK;
    }
    esp_audio_simple_player_run(audio_prompt.player, url, NULL);
    audio_prompt.state = AUDIO_PLAYER_STATE_PLAYING;
    return ESP_OK;
}

esp_err_t audio_prompt_stop(void)
{
    if (audio_prompt.state == AUDIO_PLAYER_STATE_IDLE) {
        ESP_LOGW(TAG, "audio_prompt_stop, but state is idle");
        return ESP_FAIL;
    }
    esp_audio_simple_player_stop(audio_prompt.player);
    audio_prompt.state = AUDIO_PLAYER_STATE_IDLE;
    return ESP_OK;
}

esp_err_t audio_prompt_play_with_block(const char *url, int timeout_ms)
{
    ESP_LOGI(TAG, "audio_prompt_play_with_block, url: %s, timeout_ms: %d", url, timeout_ms);

    if (timeout_ms < 0) {
        timeout_ms = 60 * 60 * 1000;
    }

    int64_t start_time = esp_timer_get_time();
    while (audio_prompt.state == AUDIO_PLAYER_STATE_PLAYING) {
        if (esp_timer_get_time() - start_time > (int64_t)timeout_ms * 1000) {
            ESP_LOGE(
                TAG, "Play audio(%s) timeout(%dms), state(%d), start_time(%d), end_time(%d)",
                url, timeout_ms, audio_prompt.state, (int)(start_time / 1000), (int)(esp_timer_get_time() / 1000)
            );
            return ESP_FAIL;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return audio_prompt_play(url);
}

esp_gmf_element_handle_t audio_processor_get_afe_handle(void)
{
    esp_gmf_element_handle_t safe = NULL;
    esp_gmf_pipeline_get_el_by_name(audio_recorder.pipe, "ai_afe", &safe);
    return safe;
}

esp_err_t audio_prompt_play_mute(bool enable_mute)
{
    ESP_LOGI(TAG, "audio_prompt_play_mute, enable_mute: %d", enable_mute);

    esp_codec_dev_set_out_mute(audio_manager.play_dev, enable_mute);
    return ESP_OK;
}
