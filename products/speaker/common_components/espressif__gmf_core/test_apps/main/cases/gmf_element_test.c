/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_port.h"
#include "esp_gmf_data_bus.h"
#include "esp_gmf_new_databus.h"

#include "gmf_fake_dec.h"
#include "gmf_fake_io.h"

static const char *TAG = "TEST_GMF_ELEMENT";

#define ESP_GMF_PORT_PAYLOAD_LEN_DEFAULT (4096)

esp_err_t element_event(esp_gmf_event_pkt_t *event, void *ctx)
{
    ESP_LOGI(TAG, "Get event,from:%p, type:%d, sub:%d, payload:%p, size:%d",
             event->from, event->type, event->sub, event->payload, event->payload_size);
    return ESP_OK;
}

TEST_CASE("Register and unregister port for GMF ELEMENT", "ESP_GMF_ELEMENT")
{
    esp_log_level_set("*", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_GMF_ELEMENT", ESP_LOG_VERBOSE);

    ESP_GMF_MEM_SHOW(TAG);
    fake_io_cfg_t cfg = FAKE_IO_CFG_DEFAULT();
    cfg.dir = ESP_GMF_IO_DIR_READER;
    esp_gmf_io_handle_t reader = NULL;
    fake_io_init(&cfg, &reader);
    TEST_ASSERT_NOT_NULL(reader);
    esp_gmf_io_set_uri(reader, "test.mp3");
    ESP_GMF_MEM_SHOW(TAG);

    esp_gmf_io_handle_t writer = NULL;
    cfg.dir = ESP_GMF_IO_DIR_WRITER;
    fake_io_init(&cfg, &writer);
    TEST_ASSERT_NOT_NULL(writer);

    fake_dec_cfg_t fake_cfg = DEFAULT_FAKE_DEC_CONFIG();
    fake_cfg.cb = element_event;
    esp_gmf_element_handle_t fake_dec_el = NULL;
    fake_dec_init(&fake_cfg, &fake_dec_el);

    ESP_LOGW(TAG, "Register the in port, %d", __LINE__);
    esp_gmf_db_handle_t db = NULL;
    esp_gmf_db_new_ringbuf(10, 1024, &db);
    esp_gmf_port_handle_t in_port = NEW_ESP_GMF_PORT_IN_BYTE(esp_gmf_db_acquire_read, esp_gmf_db_release_read, NULL, db,
                                                             ESP_GMF_PORT_PAYLOAD_LEN_DEFAULT, portMAX_DELAY);
    TEST_ASSERT_NOT_NULL(in_port);
    TEST_ASSERT_EQUAL(esp_gmf_element_register_in_port(fake_dec_el, in_port), ESP_GMF_ERR_OK);
    esp_gmf_port_handle_t in_port1 = NEW_ESP_GMF_PORT_IN_BYTE(esp_gmf_db_acquire_read, esp_gmf_db_release_read, NULL, db,
                                                              ESP_GMF_PORT_PAYLOAD_LEN_DEFAULT, portMAX_DELAY);
    TEST_ASSERT_NOT_NULL(in_port1);
    TEST_ASSERT_EQUAL(esp_gmf_element_register_in_port(fake_dec_el, in_port1), ESP_GMF_ERR_NOT_SUPPORT);

    esp_gmf_port_handle_t out_port = NEW_ESP_GMF_PORT_OUT_BYTE(esp_gmf_db_acquire_write, esp_gmf_db_release_write, NULL, db,
                                                               ESP_GMF_PORT_PAYLOAD_LEN_DEFAULT, portMAX_DELAY);
    TEST_ASSERT_NOT_NULL(out_port);
    TEST_ASSERT_EQUAL(esp_gmf_element_register_out_port(fake_dec_el, out_port), ESP_GMF_ERR_OK);
    esp_gmf_port_handle_t out_port1 = NEW_ESP_GMF_PORT_OUT_BYTE(esp_gmf_db_acquire_write, esp_gmf_db_release_write, NULL, db,
                                                                ESP_GMF_PORT_PAYLOAD_LEN_DEFAULT, portMAX_DELAY);
    TEST_ASSERT_NOT_NULL(out_port1);
    TEST_ASSERT_EQUAL(esp_gmf_element_register_out_port(fake_dec_el, out_port1), ESP_GMF_ERR_NOT_SUPPORT);

    esp_gmf_port_t *el_in_port = ESP_GMF_ELEMENT_GET(fake_dec_el)->in;
    esp_gmf_port_t *el_out_port = ESP_GMF_ELEMENT_GET(fake_dec_el)->out;
    uint8_t k = 0;
    while (el_in_port) {
        if (k == 0) {
            ESP_LOGI(TAG, "Compare IN port, %d, %p,%p", __LINE__, in_port, el_in_port);
            TEST_ASSERT_EQUAL((uint32_t)in_port, (uint32_t)el_in_port);
        } else if (k == 1) {
            ESP_LOGI(TAG, "Compare IN port, %d, %p,%p", __LINE__, in_port, el_in_port);
            TEST_ASSERT_EQUAL((uint32_t)in_port1, (uint32_t)el_in_port);
        }
        k++;
        el_in_port = el_in_port->next;
    }
    k = 0;
    while (el_out_port) {
        if (k == 0) {
            ESP_LOGI(TAG, "Compare OUT port, %d, %p,%p", __LINE__, out_port, el_out_port);
            TEST_ASSERT_EQUAL((uint32_t)out_port, (uint32_t)el_out_port);
        } else if (k == 1) {
            ESP_LOGI(TAG, "Compare OUT port, %d, %p,%p", __LINE__, out_port1, el_out_port);
            TEST_ASSERT_EQUAL((uint32_t)out_port1, (uint32_t)el_out_port);
        }
        k++;
        el_out_port = el_out_port->next;
    }

    ESP_LOGW(TAG, "Unregister the in port, %d", __LINE__);
    esp_gmf_element_unregister_in_port(fake_dec_el, in_port);
    esp_gmf_element_unregister_in_port(fake_dec_el, in_port1);
    esp_gmf_element_unregister_out_port(fake_dec_el, out_port);
    esp_gmf_element_unregister_out_port(fake_dec_el, out_port1);
    esp_gmf_port_deinit(in_port1);
    esp_gmf_port_deinit(out_port1);
    esp_gmf_obj_delete(reader);
    esp_gmf_obj_delete(writer);
    esp_gmf_obj_delete(fake_dec_el);
    esp_gmf_db_deinit(db);

    ESP_GMF_MEM_SHOW(TAG);
}
