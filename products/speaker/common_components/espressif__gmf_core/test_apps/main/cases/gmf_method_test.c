/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_method.h"
#include "gmf_ut_common.h"
#include "gmf_fake_dec.h"
#include "gmf_fake_io.h"

static const char *TAG = "TEST_ESP_GMF_METHOD";

esp_gmf_err_t esp_gmf_method_func1(void *handle, esp_gmf_args_desc_t *arg_desc, uint8_t *buf, int buf_len)
{
    printf("%s\r\n", __func__);
    return ESP_OK;
}
esp_gmf_err_t esp_gmf_method_func2(void *handle, esp_gmf_args_desc_t *arg_desc, uint8_t *buf, int buf_len)
{
    printf("%s\r\n", __func__);
    return ESP_OK;
}
esp_gmf_err_t esp_gmf_method_func3(void *handle, esp_gmf_args_desc_t *arg_desc, uint8_t *buf, int buf_len)
{
    printf("%s\r\n", __func__);
    return ESP_OK;
}

TEST_CASE("Method create and destroy test", "ESP_GMF_METHOD")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_gmf_method_t *method = NULL;
    ESP_GMF_MEM_SHOW(TAG);
    esp_gmf_method_create("test1", esp_gmf_method_func1, NULL, &method);
    esp_gmf_method_show(method);
    esp_gmf_method_destroy(method);
    method = NULL;

    esp_gmf_method_append(&method, "test1", esp_gmf_method_func1, NULL);
    esp_gmf_method_append(&method, "test2", esp_gmf_method_func2, NULL);
    esp_gmf_method_append(&method, "test3", esp_gmf_method_func3, NULL);

    esp_gmf_args_desc_t *args_desc = NULL;
    esp_gmf_method_query_args(method, &args_desc);

    esp_gmf_method_show(method);
    esp_gmf_method_destroy(method);
    ESP_GMF_MEM_SHOW(TAG);
}

TEST_CASE("Test basic arithmetic type arguments description", "ESP_GMF_METHOD")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_GMF_MEM_SHOW(TAG);

    fake_dec_cfg_t cfg = DEFAULT_FAKE_DEC_CONFIG();
    esp_gmf_obj_handle_t dec = NULL;
    fake_dec_init(&cfg, &dec);

    esp_gmf_port_handle_t out_port = NEW_ESP_GMF_PORT_OUT_BYTE(NULL, NULL, NULL, NULL, 1000, ESP_GMF_MAX_DELAY);
    esp_gmf_element_register_out_port((esp_gmf_element_handle_t)dec, out_port);
    esp_gmf_port_handle_t in_port = NEW_ESP_GMF_PORT_OUT_BYTE(NULL, NULL, NULL, NULL, 1000, ESP_GMF_MAX_DELAY);
    esp_gmf_element_register_in_port((esp_gmf_element_handle_t)dec, in_port);
    esp_gmf_element_process_open((esp_gmf_element_handle_t)dec, NULL);

    /// Check  uint32_t
    const esp_gmf_method_t *method_head = NULL;
    const esp_gmf_method_t *method1 = NULL;
    esp_gmf_element_get_method((esp_gmf_element_handle_t)dec, &method_head);
    esp_gmf_method_found(method_head, "set_info", &method1);
    size_t cnt = 0;
    esp_gmf_args_desc_get_total_size(method1->args_desc, &cnt);
    ESP_LOGI(TAG, "Total size %d", cnt);
    uint8_t *buf = NULL;
    buf = esp_gmf_oal_malloc(cnt);

    uint32_t rate = 48000;
    uint32_t channel = 3;
    uint32_t bits = 24;
    esp_gmf_args_set_value(method1->args_desc, "rate", buf, (uint8_t *)&rate, sizeof(rate));
    esp_gmf_args_set_value(method1->args_desc, "ch", buf, (uint8_t *)&channel, sizeof(channel));
    esp_gmf_args_set_value(method1->args_desc, "bits", buf, (uint8_t *)&bits, sizeof(bits));
    esp_gmf_element_exe_method((esp_gmf_element_handle_t)dec, "set_info", buf, cnt);

    memset(buf, 0, cnt);
    esp_gmf_element_exe_method((esp_gmf_element_handle_t)dec, "get_info", buf, cnt);
    uint32_t get_rate = 0;
    uint32_t get_channel = 0;
    uint32_t get_bits = 0;
    esp_gmf_method_found(method_head, "get_info", &method1);
    esp_gmf_args_extract_value(method1->args_desc, "bits", buf, cnt, &get_bits);
    esp_gmf_args_extract_value(method1->args_desc, "ch", buf, cnt, &get_channel);
    esp_gmf_args_extract_value(method1->args_desc, "rate", buf, cnt, &get_rate);

    TEST_ASSERT_EQUAL(rate, get_rate);
    TEST_ASSERT_EQUAL(bits, get_bits);
    TEST_ASSERT_EQUAL(channel, get_channel);

    // Check string
    cnt = 0;
    esp_gmf_method_found(method_head, "set_name", &method1);
    esp_gmf_args_desc_get_total_size(method1->args_desc, &cnt);
    esp_gmf_oal_free(buf);
    buf = esp_gmf_oal_malloc(cnt);
    const char *name = "1234567890abcdefghijklmnopqrstvu";
    esp_gmf_args_set_value(method1->args_desc, "dec_name", buf, (uint8_t *)name, strlen(name));
    esp_gmf_element_exe_method((esp_gmf_element_handle_t)dec, "set_name", buf, cnt);

    memset(buf, 0, cnt);
    esp_gmf_element_exe_method((esp_gmf_element_handle_t)dec, "get_name", buf, cnt);
    printf("%s\n", buf);
    TEST_ASSERT_NOT_EQUAL(0, strcasecmp(name, (char *)buf));

    // Check uint64_t
    cnt = 0;
    esp_gmf_method_found(method_head, "set_size", &method1);
    esp_gmf_args_desc_get_total_size(method1->args_desc, &cnt);
    esp_gmf_oal_free(buf);
    buf = esp_gmf_oal_malloc(cnt);
    uint64_t x = 0xFFFFFFF99998888;
    esp_gmf_args_set_value(method1->args_desc, "size", buf, (uint8_t *)&x, sizeof(x));
    esp_gmf_element_exe_method((esp_gmf_element_handle_t)dec, "set_size", buf, cnt);

    memset(buf, 0, cnt);
    uint64_t x_out = 0;
    esp_gmf_element_exe_method((esp_gmf_element_handle_t)dec, "get_size", buf, cnt);
    TEST_ASSERT_NOT_EQUAL(x, x_out);

    // Check uint64_t and uint8_t
    cnt = 0;
    esp_gmf_method_found(method_head, "set_filter", &method1);
    esp_gmf_args_desc_get_total_size(method1->args_desc, &cnt);
    esp_gmf_oal_free(buf);
    buf = esp_gmf_oal_malloc(cnt);
    uint8_t filter_idx = 1;
    esp_gmf_args_set_value(method1->args_desc, "index", buf, (uint8_t *)&filter_idx, sizeof(filter_idx));
    uint64_t filter = 0x1122334455667788;
    esp_gmf_args_set_value(method1->args_desc, "filter", buf, (uint8_t *)&filter, sizeof(filter));
    esp_gmf_element_exe_method((esp_gmf_element_handle_t)dec, "set_filter", buf, cnt);

    filter_idx = 0;
    esp_gmf_args_set_value(method1->args_desc, "index", buf, (uint8_t *)&filter_idx, sizeof(filter_idx));
    filter = 0xAABBCCDDEEFF0011;
    esp_gmf_args_set_value(method1->args_desc, "filter", buf, (uint8_t *)&filter, sizeof(filter));
    esp_gmf_element_exe_method((esp_gmf_element_handle_t)dec, "set_filter", buf, cnt);

    memset(buf, 0, cnt);
    esp_gmf_element_exe_method((esp_gmf_element_handle_t)dec, "get_filter", buf, cnt);
    x_out = 0;
    TEST_ASSERT_NOT_EQUAL(filter, x_out);

    esp_gmf_element_process_close((esp_gmf_element_handle_t)dec, NULL);
    esp_gmf_obj_delete(dec);
    esp_gmf_oal_free(buf);
    ESP_GMF_MEM_SHOW(TAG);
}

TEST_CASE("Test structure description", "ESP_GMF_METHOD")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_GMF_MEM_SHOW(TAG);

    fake_dec_cfg_t cfg = DEFAULT_FAKE_DEC_CONFIG();
    esp_gmf_obj_handle_t dec = NULL;
    fake_dec_init(&cfg, &dec);

    esp_gmf_port_handle_t out_port = NEW_ESP_GMF_PORT_OUT_BYTE(NULL, NULL, NULL, NULL, 1000, ESP_GMF_MAX_DELAY);
    esp_gmf_element_register_out_port((esp_gmf_element_handle_t)dec, out_port);
    esp_gmf_port_handle_t in_port = NEW_ESP_GMF_PORT_OUT_BYTE(NULL, NULL, NULL, NULL, 1000, ESP_GMF_MAX_DELAY);
    esp_gmf_element_register_in_port((esp_gmf_element_handle_t)dec, in_port);
    esp_gmf_element_process_open((esp_gmf_element_handle_t)dec, NULL);

    const esp_gmf_method_t *method_head = NULL;
    const esp_gmf_method_t *method1 = NULL;
    esp_gmf_element_get_method((esp_gmf_element_handle_t)dec, &method_head);
    size_t cnt = 0;
    uint8_t *buf = NULL;

    /// Check Structure
    cnt = 0;
    esp_gmf_method_found(method_head, "set_para", &method1);

    esp_gmf_args_desc_get_total_size(method1->args_desc, &cnt);
    buf = esp_gmf_oal_malloc(cnt);
    uint8_t idx = 1;
    uint32_t type = 9;
    uint32_t fc = 100;
    float q = 4.0;
    float gain = 3.5;

    esp_gmf_args_set_value(method1->args_desc, "index", buf, (uint8_t *)&idx, sizeof(idx));
    esp_gmf_args_set_value(method1->args_desc, "filter_type", buf, (uint8_t *)&type, sizeof(type));
    esp_gmf_args_set_value(method1->args_desc, "fc", buf, (uint8_t *)&fc, sizeof(fc));
    esp_gmf_args_set_value(method1->args_desc, "q", buf, (uint8_t *)&q, sizeof(q));
    esp_gmf_args_set_value(method1->args_desc, "gain", buf, (uint8_t *)&gain, sizeof(gain));
    esp_gmf_element_exe_method((esp_gmf_element_handle_t)dec, "set_para", buf, cnt);
    memset(buf, 0, cnt);
    idx = 3;
    esp_gmf_method_found(method_head, "get_para", &method1);
    esp_gmf_args_set_value(method1->args_desc, "index", buf, (uint8_t *)&idx, sizeof(idx));
    esp_gmf_element_exe_method((esp_gmf_element_handle_t)dec, "get_para", buf, cnt);
    uint32_t type2 = 0;
    uint32_t fc2 = 0;
    float q2 = 0.0;
    float gain2 = 0.0;
    esp_gmf_args_extract_value(method1->args_desc, "filter_type", buf, cnt, &type2);
    esp_gmf_args_extract_value(method1->args_desc, "fc", buf, cnt, &fc2);
    esp_gmf_args_extract_value(method1->args_desc, "q", buf, cnt, (uint32_t *)&q2);
    esp_gmf_args_extract_value(method1->args_desc, "gain", buf, cnt, (uint32_t *)&gain2);
    TEST_ASSERT_EQUAL(type, type2);
    TEST_ASSERT_EQUAL(fc, fc2);
    TEST_ASSERT_EQUAL_FLOAT(q, q2);
    TEST_ASSERT_EQUAL_FLOAT(gain, gain2);

    /// Check nested Structure
    ESP_LOGI(TAG, ">>>>>> %s <<<<<<\r\n", "Check nested Structure");
    cnt = 0;
    esp_gmf_method_found(method_head, "set_args", &method1);
    esp_gmf_args_desc_get_total_size(method1->args_desc, &cnt);
    ESP_LOGI(TAG, "The total size :%d", cnt);
    esp_gmf_oal_free(buf);
    buf = esp_gmf_oal_malloc(cnt);

    const char *name = "1234567890abcdef";
    uint32_t a = 0x99;
    uint32_t b = 0x33;
    uint32_t c = 0xbb;
    uint32_t d = 0x98;
    uint32_t e = 0x32;
    uint32_t f = 0xba;
    uint32_t val = 0xdd;

    esp_gmf_args_set_value(method1->args_desc, "a", buf, (uint8_t *)&a, sizeof(a));
    esp_gmf_args_set_value(method1->args_desc, "b", buf, (uint8_t *)&b, sizeof(b));
    esp_gmf_args_set_value(method1->args_desc, "c", buf, (uint8_t *)&c, sizeof(c));

    esp_gmf_args_set_value(method1->args_desc, "d", buf, (uint8_t *)&d, sizeof(d));
    esp_gmf_args_set_value(method1->args_desc, "e", buf, (uint8_t *)&e, sizeof(e));
    esp_gmf_args_set_value(method1->args_desc, "f", buf, (uint8_t *)&f, sizeof(f));

    esp_gmf_args_set_value(method1->args_desc, "value", buf, (uint8_t *)&val, sizeof(val));
    esp_gmf_args_set_value(method1->args_desc, "label", buf, (uint8_t *)name, strlen(name));

    esp_gmf_element_exe_method((esp_gmf_element_handle_t)dec, "set_args", buf, cnt);

    memset(buf, 0, cnt);
    esp_gmf_method_found(method_head, "get_args", &method1);
    esp_gmf_element_exe_method((esp_gmf_element_handle_t)dec, "get_args", buf, cnt);

    uint32_t a2 = 0;
    uint32_t b2 = 0;
    uint32_t c2 = 0;
    uint32_t d2 = 0;
    uint32_t e2 = 0;
    uint32_t f2 = 0;
    uint32_t val2 = 0;
    char out_name[16] = {0};

    esp_gmf_args_extract_value(method1->args_desc, "a", buf, cnt, &a2);
    esp_gmf_args_extract_value(method1->args_desc, "b", buf, cnt, &b2);
    esp_gmf_args_extract_value(method1->args_desc, "c", buf, cnt, &c2);
    esp_gmf_args_extract_value(method1->args_desc, "d", buf, cnt, &d2);
    esp_gmf_args_extract_value(method1->args_desc, "e", buf, cnt, &e2);
    esp_gmf_args_extract_value(method1->args_desc, "f", buf, cnt, &f2);

    esp_gmf_args_extract_value(method1->args_desc, "value", buf, cnt, &val2);
    esp_gmf_args_extract_value(method1->args_desc, "label", buf, cnt, (uint32_t *)&out_name);

    TEST_ASSERT_EQUAL(a, a2);
    TEST_ASSERT_EQUAL(b, b2);
    TEST_ASSERT_EQUAL(c, c2);
    TEST_ASSERT_EQUAL(d, d2);
    TEST_ASSERT_EQUAL(e, e2);
    TEST_ASSERT_EQUAL(f, f2);
    TEST_ASSERT_EQUAL(val, val2);
    TEST_ASSERT_NOT_EQUAL(0, strcasecmp(name, (char *)out_name));

    esp_gmf_element_process_close((esp_gmf_element_handle_t)dec, NULL);
    esp_gmf_obj_delete(dec);
    esp_gmf_oal_free(buf);
    ESP_GMF_MEM_SHOW(TAG);
}
