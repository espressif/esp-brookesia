/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include "video_processor.c"

static int opens, starts, stops, closes, sources, source_frees, queue_deletes, event_deletes, waits;
static bool pipeline_live, source_live, stop_called, worker_live;
static int caps_failure, open_failure, sink_failure, start_failure, thread_failure, stop_failure, close_failure;
static esp_capture_video_src_if_t *allocated_source;
static char trace[128];
static size_t trace_count;

static void record(char step)
{
    assert(trace_count + 1 < sizeof(trace));
    trace[trace_count++] = step;
}

static esp_capture_err_t set_caps(esp_capture_video_src_if_t *source, const esp_capture_video_info_t *caps)
{
    assert(source == allocated_source && source_live);
    return caps_failure;
}

esp_capture_video_src_if_t *esp_capture_new_video_v4l2_src(esp_capture_video_v4l2_src_cfg_t *cfg)
{
    assert(!source_live);
    allocated_source = calloc(1, sizeof(*allocated_source));
    assert(allocated_source);
    allocated_source->set_fixed_caps = set_caps;
    source_live = true;
    sources++;
    record('N');
    return allocated_source;
}

void *esp_gmf_oal_calloc(size_t count, size_t size)
{
    return calloc(count, size);
}
void *esp_gmf_oal_malloc(size_t size)
{
    return malloc(size);
}

void esp_gmf_oal_free(void *pointer)
{
    if (pointer == allocated_source) {
        assert(source_live && !pipeline_live && !worker_live);
        source_live = false;
        source_frees++;
        allocated_source = NULL;
        record('F');
    }
    free(pointer);
}

esp_capture_err_t esp_capture_open(const esp_capture_cfg_t *cfg, void **out)
{
    assert(source_live && !pipeline_live && cfg->video_src == allocated_source);
    opens++;
    record('O');
    if (open_failure) {
        *out = NULL;
        return open_failure;
    }
    pipeline_live = true;
    stop_called = false;
    *out = &pipeline_live;
    return ESP_CAPTURE_ERR_OK;
}

esp_capture_err_t esp_capture_sink_setup(void *capture, int index, const esp_capture_sink_cfg_t *cfg, void **out)
{
    assert(pipeline_live);
    *out = sink_failure ? NULL : &source_live;
    return sink_failure;
}

esp_capture_err_t esp_capture_start(void *capture)
{
    assert(pipeline_live && source_live);
    starts++;
    return start_failure;
}

esp_capture_err_t esp_capture_stop(void *capture)
{
    assert(pipeline_live && source_live);
    stops++;
    stop_called = true;
    record('S');
    return stop_failure;
}

esp_capture_err_t esp_capture_close(void *capture)
{
    assert(pipeline_live && source_live && stop_called && !worker_live);
    closes++;
    pipeline_live = false;
    record('C');
    // esp_capture 0.8.4 consumes every valid handle on close. The injected
    // error exercises av_processor's reporting without inventing retry ownership.
    return close_failure;
}

void *xQueueCreate(unsigned count, unsigned size)
{
    return malloc(1);
}
void *xEventGroupCreate(void)
{
    return malloc(1);
}
void vQueueDelete(void *queue)
{
    queue_deletes++;
    free(queue);
}
void vEventGroupDelete(void *group)
{
    event_deletes++;
    free(group);
}

int xEventGroupWaitBits(void *group, int bits, int clear, int all, unsigned ticks)
{
    assert(stop_called && worker_live && pipeline_live);
    waits++;
    worker_live = false;
    record('W');
    return bits;
}

int esp_gmf_oal_thread_create(void **out, const char *name, void (*fn)(void *), void *arg,
                              size_t stack, int priority, bool external, int core)
{
    if (thread_failure) {
        return thread_failure;
    }
    *out = &worker_live;
    worker_live = true;
    return 0;
}

void esp_gmf_oal_thread_delete(void *handle) {}
void vTaskDelete(void *handle) {}
void vTaskDelay(unsigned ticks) {}
int64_t esp_timer_get_time(void)
{
    return 1;
}
int xEventGroupSetBits(void *group, int bits)
{
    return bits;
}
int xQueueReceive(void *queue, void *data, unsigned ticks)
{
    return 0;
}
int xQueueSend(void *queue, const void *data, unsigned ticks)
{
    return 1;
}
void esp_video_enc_register_default(void) {}
esp_capture_err_t esp_capture_sink_enable(void *sink, int mode)
{
    return 0;
}
esp_capture_err_t esp_capture_sink_acquire_frame(void *sink, esp_capture_stream_frame_t *frame, bool no_wait)
{
    return ESP_CAPTURE_ERR_NOT_FOUND;
}
esp_capture_err_t esp_capture_sink_release_frame(void *sink, esp_capture_stream_frame_t *frame)
{
    return 0;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *scenario = argv[1];
    bool failed_start = strstr(scenario, "failure") != NULL;
    esp_capture_video_v4l2_src_cfg_t camera_config = {.dev_name = "/dev/video2", .buf_count = 1};
    video_capture_config_t config = {
        .camera_config = &camera_config,
        .source_fixed_format_id = ESP_CAPTURE_FMT_ID_YUV422,
        .sink_num = 1,
        .sink_cfg[0].video_info = {.format_id = ESP_CAPTURE_FMT_ID_YUV422, .width = 2, .height = 2},
        .stream_mode = failed_start || strcmp(scenario, "stream_stop") == 0,
    };
    video_capture_t *capture = video_capture_open(&config);
    assert(capture);
    void *queue = capture->frame_queue;
    void *group = capture->event_group;
    if (strcmp(scenario, "caps_failure") == 0) {
        caps_failure = ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    if (strcmp(scenario, "open_failure") == 0) {
        open_failure = ESP_CAPTURE_ERR_NO_MEM;
    }
    if (strcmp(scenario, "sink_failure") == 0) {
        sink_failure = ESP_CAPTURE_ERR_INTERNAL;
    }
    if (strcmp(scenario, "start_failure") == 0) {
        start_failure = ESP_CAPTURE_ERR_INTERNAL;
    }
    if (strcmp(scenario, "thread_failure") == 0) {
        thread_failure = ESP_FAIL;
    }
    int ret = video_capture_start(capture);
    if (failed_start) {
        assert(ret != ESP_OK);
        assert(!pipeline_live && !source_live && source_frees == 1);
        assert(capture->frame_queue == queue && capture->event_group == group);
        assert(!queue_deletes && !event_deletes && !capture->capture && !capture->is_running);
        caps_failure = open_failure = sink_failure = start_failure = thread_failure = 0;
        assert(video_capture_start(capture) == ESP_OK);
        assert(source_live && pipeline_live && sources == 2);
    } else {
        assert(ret == ESP_OK && pipeline_live && source_live);
    }
    if (strcmp(scenario, "frame_close") == 0) {
        assert(!capture->is_running && capture->capture);
        video_capture_close(capture);
        assert(strcmp(trace, "NOSCF") == 0);
    } else {
        if (strcmp(scenario, "stop_error") == 0) {
            stop_failure = ESP_CAPTURE_ERR_INTERNAL;
        }
        if (strcmp(scenario, "close_error") == 0) {
            close_failure = ESP_CAPTURE_ERR_INTERNAL;
        }
        int expected = stop_failure ? stop_failure : close_failure;
        assert(video_capture_stop(capture) == expected);
        assert(!source_live && !pipeline_live && !capture->capture);
        assert(capture->frame_queue == queue && capture->event_group == group);
        assert(!queue_deletes && !event_deletes);
        int stopped = stops, closed = closes, freed = source_frees;
        assert(video_capture_stop(capture) == ESP_OK);
        assert(stops == stopped && closes == closed && source_frees == freed);
        video_capture_close(capture);
        assert(stops == stopped && closes == closed && source_frees == freed);
        if (config.stream_mode) {
            assert(waits == 1 && queue_deletes == 1 && event_deletes == 1);
            assert(strstr(trace, "SWCF") != NULL);
        } else {
            assert(waits == 0 && strcmp(trace, "NOSCF") == 0);
        }
    }
    assert(!pipeline_live && !source_live && source_frees == sources);
    return 0;
}
