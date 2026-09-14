/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void *tracked_calloc(size_t count, size_t size);
static void tracked_free(void *pointer);

#define calloc tracked_calloc
#define free tracked_free
#define TAG DVP_TAG
#include "devices/dev_camera/dev_camera_sub_dvp.c"
#undef TAG
#define TAG CAMERA_TAG
#include "devices/dev_camera/dev_camera.c"
#undef TAG
#undef calloc
#undef free

static int references;
static int releases;
static int video_initializations;
static int video_closes;
static int freed_wrappers;
static int video_failure;
static int release_failure;
static int init_failure;
static bool consume_on_failure;
static bool bus_live;
static bool video_live;
static void *owned_wrapper;
static dev_camera_config_t config = {
    .name = "camera",
    .type = "camera",
    .sub_type = "dvp",
    .sub_cfg.dvp = {.i2c_name = "i2c", .i2c_freq = 400000},
};

static void *tracked_calloc(size_t count, size_t size)
{
    assert(owned_wrapper == NULL);
    owned_wrapper = calloc(count, size);
    return owned_wrapper;
}

static void tracked_free(void *pointer)
{
    assert(pointer != NULL && pointer == owned_wrapper);
    assert(!video_live);
    freed_wrappers++;
    free(pointer);
    owned_wrapper = NULL;
}

int esp_board_periph_ref_handle(const char *name, void **handle)
{
    assert(strcmp(name, "i2c") == 0 && !bus_live);
    references++;
    bus_live = true;
    *handle = &bus_live;
    return ESP_OK;
}

int esp_board_periph_unref_handle(const char *name)
{
    assert(strcmp(name, "i2c") == 0 && bus_live && !video_live);
    releases++;
    if (!release_failure || consume_on_failure) {
        bus_live = false;
    }
    return release_failure;
}

int esp_video_init(const esp_video_init_config_t *video_config)
{
    assert(bus_live && !video_live && video_config->dvp->sccb_config.i2c_handle == &bus_live);
    video_initializations++;
    if (!init_failure) {
        video_live = true;
    }
    return init_failure;
}

int esp_video_deinit(void)
{
    assert(video_live && bus_live);
    video_closes++;
    if (!video_failure) {
        video_live = false;
    }
    return video_failure;
}

int esp_board_device_get_config_by_handle(void *handle, void **out_config)
{
    assert(handle == owned_wrapper);
    *out_config = &config;
    return ESP_OK;
}

const esp_board_entry_desc_t *esp_board_entry_find_subtype_desc(const char *device, const char *subtype)
{
    assert(strcmp(device, "camera") == 0);
    (void)subtype;
    static const esp_board_entry_desc_t entry = {dev_camera_sub_dvp_init, dev_camera_sub_dvp_deinit};
    return &entry;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    void *handle = NULL;
    if (strcmp(argv[1], "init_failure") == 0) {
        init_failure = 30;
        assert(dev_camera_init(&config, sizeof(config), &handle) != ESP_OK);
        assert(handle == NULL && owned_wrapper == NULL && freed_wrappers == 1);
        assert(references == 1 && releases == 1 && video_closes == 0);
        return 0;
    }
    assert(dev_camera_init(&config, sizeof(config), &handle) == ESP_OK);
    assert(strcmp(((dev_camera_handle_t *)handle)->dev_path, "/dev/video2") == 0);
    if (strcmp(argv[1], "video_retry") == 0) {
        video_failure = 31;
        assert(dev_camera_deinit(handle) == 31);
        assert(video_live && bus_live && releases == 0 && freed_wrappers == 0);
        video_failure = 0;
        assert(dev_camera_deinit(handle) == ESP_OK);
        assert(video_closes == 2 && releases == 1 && freed_wrappers == 1);
    } else if (strcmp(argv[1], "other_subtype") == 0) {
        // The generic wrapper's existing non-DVP policy remains outside this patch.
        config.sub_type = "csi";
        video_failure = 31;
        assert(dev_camera_deinit(handle) == ESP_OK);
        assert(video_live && bus_live && releases == 0 && freed_wrappers == 0);
        video_failure = 0;
        assert(dev_camera_sub_dvp_deinit(handle) == ESP_OK);
    } else {
        release_failure = 32;
        consume_on_failure = strcmp(argv[1], "consumed") == 0;
        assert(dev_camera_deinit(handle) == 32);
        assert(!video_live && bus_live != consume_on_failure && freed_wrappers == 0);
        assert(dev_camera_deinit(handle) == 32);
        assert(dev_camera_sub_dvp_deinit(handle) == 32);
        assert(video_initializations == 1 && video_closes == 1 && releases == 1 && references == 1);
        assert(owned_wrapper == handle);
        // The production owner intentionally survives until a manual restart.
        free(owned_wrapper);
    }
    return 0;
}
