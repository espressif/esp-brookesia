/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "host_stubs.h"

static int fake_open(const char *path, int flags, ...);
static int fake_close(int fd);
static int fake_ioctl(int fd, unsigned long request, ...);
static void *fake_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
#define open(...) fake_open(__VA_ARGS__)
#define close(...) fake_close(__VA_ARGS__)
#define ioctl(...) fake_ioctl(__VA_ARGS__)
#define mmap(...) fake_mmap(__VA_ARGS__)
#include "capture_video_v4l2_src.c"
#undef open
#undef close
#undef ioctl
#undef mmap

static uint32_t formats[2];
static unsigned format_count;
static uint32_t expected_format;
static int set_format_calls;
static int closes;
static int opens;
static bool streaming;
static uint16_t pixels[4];
static const uint8_t yuyv[] = {10, 40, 20, 60, 30, 50, 40, 70};
static const uint8_t uyvy[] = {40, 10, 60, 20, 50, 30, 70, 40};

static int fake_open(const char *path, int flags, ...)
{
    assert(strcmp(path, "/dev/video2") == 0);
    opens++;
    return 5;
}

static int fake_close(int fd)
{
    assert(fd == 5 && !streaming);
    closes++;
    return 0;
}

static int fake_ioctl(int fd, unsigned long request, ...)
{
    assert(fd == 5);
    va_list args;
    va_start(args, request);
    void *argument = va_arg(args, void *);
    va_end(args);
    switch (request) {
    case VIDIOC_QUERYCAP:
        ((struct v4l2_capability *)argument)->capabilities = V4L2_CAP_VIDEO_CAPTURE;
        return 0;
    case VIDIOC_ENUM_FMT: {
        struct v4l2_fmtdesc *format = argument;
        if (format->index >= format_count) {
            return -1;
        }
        format->pixelformat = formats[format->index];
        return 0;
    }
    case VIDIOC_ENUM_FRAMESIZES: {
        struct v4l2_frmsizeenum *size = argument;
        assert(size->pixel_format == expected_format);
        size->type = V4L2_FRMSIZE_TYPE_DISCRETE;
        size->discrete.width = size->discrete.height = 2;
        return size->index == 0 ? 0 : -1;
    }
    case VIDIOC_S_FMT:
        assert(((struct v4l2_format *)argument)->fmt.pix.pixelformat == expected_format);
        set_format_calls++;
        return 0;
    case VIDIOC_QUERYBUF:
        ((struct v4l2_buffer *)argument)->length = sizeof(pixels);
        return 0;
    case VIDIOC_DQBUF:
        assert(streaming);
        ((struct v4l2_buffer *)argument)->index = 0;
        ((struct v4l2_buffer *)argument)->bytesused = sizeof(pixels);
        memcpy(pixels, expected_format == V4L2_PIX_FMT_UYVY ? uyvy : yuyv, sizeof(pixels));
        return 0;
    case VIDIOC_STREAMON:
        streaming = true;
        return 0;
    case VIDIOC_STREAMOFF:
        streaming = false;
        return 0;
    case VIDIOC_S_DQBUF_TIMEOUT:
    case VIDIOC_REQBUFS:
    case VIDIOC_QBUF:
        return 0;
    default:
        assert(false);
        return -1;
    }
}

static void *fake_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    assert(fd == 5 && length == sizeof(pixels));
    return pixels;
}

void *xSemaphoreCreateCounting(int maximum, int initial)
{
    return (void *)1;
}
int xSemaphoreTake(void *handle, unsigned timeout)
{
    return 1;
}
int xSemaphoreGive(void *handle)
{
    return 1;
}
void vSemaphoreDelete(void *handle) {}

int main(int argc, char **argv)
{
    assert(argc == 2);
    bool to_420 = strstr(argv[1], "420") != NULL;
    expected_format = V4L2_PIX_FMT_YUYV;
    if (strncmp(argv[1], "uyvy", 4) == 0) {
        expected_format = formats[0] = V4L2_PIX_FMT_UYVY;
        format_count = 1;
    } else if (strncmp(argv[1], "yuyv", 4) == 0) {
        formats[0] = V4L2_PIX_FMT_YUYV;
        format_count = 1;
    } else if (strncmp(argv[1], "both", 4) == 0) {
        bool uyvy_first = strcmp(argv[1], "both_uyvy_first") == 0;
        formats[uyvy_first ? 0 : 1] = V4L2_PIX_FMT_UYVY;
        formats[uyvy_first ? 1 : 0] = V4L2_PIX_FMT_YUYV;
        format_count = 2;
    } else {
        formats[0] = V4L2_PIX_FMT_GREY;
        format_count = 1;
    }
    esp_capture_video_v4l2_src_cfg_t config = {.dev_name = "/dev/video2", .buf_count = 1};
    esp_capture_video_src_if_t *source = esp_capture_new_video_v4l2_src(&config);
    assert(source);
    esp_capture_video_info_t wanted = {
        .format_id = to_420 ? ESP_CAPTURE_FMT_ID_YUV420 : ESP_CAPTURE_FMT_ID_YUV422,
        .width = 2, .height = 2, .fps = 30,
    };
    esp_capture_video_info_t result = {0};
    if (strcmp(argv[1], "none") == 0) {
        assert(source->open(source) == ESP_CAPTURE_ERR_NO_RESOURCES);
        assert(source->negotiate_caps(source, &wanted, &result) == ESP_CAPTURE_ERR_NOT_SUPPORTED);
        assert(closes == 1 && set_format_calls == 0);
        assert(source->close(source) == ESP_CAPTURE_ERR_OK && closes == 1);
        // The same source must be reusable after failed format enumeration.
        formats[0] = expected_format = V4L2_PIX_FMT_UYVY;
    }
    assert(source->open(source) == ESP_CAPTURE_ERR_OK);
    const esp_capture_format_id_t *supported;
    uint8_t count;
    assert(source->get_support_codecs(source, &supported, &count) == ESP_CAPTURE_ERR_OK);
    assert(count == 1 && supported[0] == ESP_CAPTURE_FMT_ID_YUV422);
    assert(source->negotiate_caps(source, &wanted, &result) == ESP_CAPTURE_ERR_OK);
    assert(result.format_id == wanted.format_id);
    assert(source->start(source) == ESP_CAPTURE_ERR_OK && set_format_calls == 1);
    esp_capture_stream_frame_t frame = {0};
    assert(source->acquire_frame(source, &frame) == ESP_CAPTURE_ERR_OK);
    const uint8_t yuv420[] = {10, 20, 30, 40, 40, 60};
    assert(frame.size == (to_420 ? sizeof(yuv420) : sizeof(yuyv)));
    assert(memcmp(frame.data, to_420 ? yuv420 : yuyv, frame.size) == 0);
    assert(source->release_frame(source, &frame) == ESP_CAPTURE_ERR_OK);
    assert(source->stop(source) == ESP_CAPTURE_ERR_OK);
    assert(source->close(source) == ESP_CAPTURE_ERR_OK);
    assert(closes == opens && opens == (strcmp(argv[1], "none") == 0 ? 2 : 1));
    free(source);
    return 0;
}
