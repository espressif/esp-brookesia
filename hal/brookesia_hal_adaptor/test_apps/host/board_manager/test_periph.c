/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "src/esp_board_periph.c"

static int init_calls;
static int deinit_calls;
static int failure;
static bool consume_on_failure;
static void *retained;

static esp_err_t initialize(void *cfg, int size, void **handle)
{
    (void)cfg;
    (void)size;
    init_calls++;
    *handle = malloc(8);
    assert(*handle);
    return ESP_OK;
}

static esp_err_t deinitialize(void *handle)
{
    deinit_calls++;
    if (!failure || consume_on_failure) {
        free(handle);
    } else {
        retained = handle;
    }
    return failure;
}

const esp_board_periph_desc_t g_esp_board_peripherals[] = {
    {.name = "first", .type = "test", .next = &g_esp_board_peripherals[1]},
    {.name = "second", .type = "test", .next = NULL},
};
esp_board_periph_entry_t g_esp_board_periph_handles[] = {
    {.type = "test", .init = initialize, .deinit = deinitialize},
};

const esp_board_periph_desc_t *esp_board_find_periph_desc(const char *name)
{
    for (int i = 0; i < 2; i++) {
        if (strcmp(name, g_esp_board_peripherals[i].name) == 0) {
            return &g_esp_board_peripherals[i];
        }
    }
    return NULL;
}

esp_board_periph_entry_t *esp_board_find_periph_handle(const char *type, esp_board_periph_role_t role)
{
    (void)type;
    (void)role;
    return &g_esp_board_periph_handles[0];
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    void *handle = NULL;
    assert(esp_board_periph_ref_handle("first", &handle) == ESP_OK);
    assert(handle && init_calls == 1);
    if (strcmp(argv[1], "missing_callback") == 0) {
        g_esp_board_periph_handles[0].deinit = NULL;
        assert(esp_board_periph_deinit("first") != ESP_OK);
        assert(esp_board_periph_get_handle("first", &handle) == ESP_OK);
        g_esp_board_periph_handles[0].deinit = deinitialize;
        assert(esp_board_periph_deinit("first") == ESP_OK);
        assert(deinit_calls == 1);
        return 0;
    }
    if (strcmp(argv[1], "overflow") == 0) {
        for (int i = 1; i < UINT8_MAX; i++) {
            assert(esp_board_periph_init("first") == ESP_OK);
        }
        assert(esp_board_periph_init("first") == ESP_ERR_INVALID_STATE);
        for (int i = 0; i < UINT8_MAX; i++) {
            assert(esp_board_periph_deinit("first") == ESP_OK);
        }
        assert(deinit_calls == 1);
        return 0;
    }
    failure = 42;
    consume_on_failure = strcmp(argv[1], "consumed") == 0;
    assert(esp_board_periph_deinit("first") == 42);
    assert(deinit_calls == 1);
    // Both possible failure contracts forbid reviving or re-releasing the node.
    assert(esp_board_periph_deinit("first") == 42);
    assert(esp_board_periph_unref_handle("first") == 42);
    assert(esp_board_periph_init("first") == 42);
    assert(esp_board_periph_init_custom("first", initialize, deinitialize) == 42);
    handle = (void *)1;
    assert(esp_board_periph_get_handle("first", &handle) == 42 && !handle);
    handle = (void *)1;
    assert(esp_board_periph_ref_handle("first", &handle) == 42 && !handle);
    assert(init_calls == 1 && deinit_calls == 1);
    if (strcmp(argv[1], "batch") == 0) {
        failure = 0;
        assert(esp_board_periph_init_all() == 42);
        assert(init_calls == 2); // The unaffected node still initializes.
        assert(esp_board_periph_deinit_all() == 42);
        assert(deinit_calls == 2); // The unaffected node still releases.
        assert(esp_board_periph_init_all() == 42);
        assert(init_calls == 3);
    }
    free(retained);
    return 0;
}
