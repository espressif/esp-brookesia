/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_cap.h"
#include "gmf_fake_dec.h"
#include "esp_gmf_caps_def.h"

static const char *TAG = "TEST_ESP_GMF_CAPS";

static esp_gmf_err_t audio_attr_iter_fun(uint32_t attr_index, esp_gmf_cap_attr_t *attr)
{
    switch (attr_index) {
        case 0: {
                ESP_GMF_CAP_ATTR_SET_MULTIPLE(attr, STR_2_EIGHTCC("BITS"), 8, 8, 32);
                break;
            }
        case 1: {
                ESP_GMF_CAP_ATTR_SET_MULTIPLE(attr, STR_2_EIGHTCC("RATE"), 8000, 8000, 192000);
                break;
            }
        case 2: {
                ESP_GMF_CAP_ATTR_SET_MULTIPLE(attr, STR_2_EIGHTCC("RATE"), 8000, 11025, 192000);
                break;
            }
        case 3: {
                ESP_GMF_CAP_ATTR_SET_STEPWISE(attr, STR_2_EIGHTCC("TEST"), 8000, 3000, 22000);
                break;
            }
        case 4: {
                const static uint8_t support_chan[] = {1, 2, 5, 11, 29, 88};
                ESP_GMF_CAP_ATTR_SET_DISCRETE(attr, STR_2_EIGHTCC("CHAN"),  (uint8_t *) &support_chan,
                                              sizeof(support_chan) / sizeof(uint8_t), sizeof(uint8_t));
                break;
            }
        case 5: {
                ESP_GMF_CAP_ATTR_SET_CONSTANT(attr, STR_2_EIGHTCC("VALUE"), 2000);
                break;
            }
        case 6: {
                ESP_GMF_CAP_ATTR_SET_STEPWISE(attr, STR_2_EIGHTCC("TET1"), 8000, 8000, 8000);
                break;
            }
        case 7: {
                ESP_GMF_CAP_ATTR_SET_MULTIPLE(attr, STR_2_EIGHTCC("TET2"), 8000, 8000, 8000);
                break;
            }
        default:
            attr->prop_type = ESP_GMF_PROP_TYPE_NONE;
            return ESP_GMF_ERR_NOT_FOUND;
    }
    return ESP_GMF_ERR_OK;
}

TEST_CASE("GMF CAPS Create and Destroy", "ESP_GMF_CAPS")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_CAPS", ESP_LOG_DEBUG);

    esp_gmf_cap_t *caps_list = NULL;

    esp_gmf_cap_t alc_caps = {0};
    alc_caps.cap_eightcc = STR_2_EIGHTCC("AUDALC");
    alc_caps.perf.oper_per_sec = 100;
    alc_caps.attr_fun = audio_attr_iter_fun;

    esp_gmf_cap_t alc_caps1 = {0};
    alc_caps1.cap_eightcc = STR_2_EIGHTCC("AUDALC1");
    alc_caps1.perf.oper_per_sec = 101;
    alc_caps1.attr_fun = audio_attr_iter_fun;

    esp_gmf_cap_t alc_caps2 = {0};
    alc_caps2.cap_eightcc = STR_2_EIGHTCC("AUDALC2");
    alc_caps2.perf.oper_per_sec = 102;
    alc_caps2.attr_fun = audio_attr_iter_fun;

    ESP_GMF_MEM_SHOW(TAG);
    esp_gmf_cap_append(&caps_list, &alc_caps);
    esp_gmf_cap_append(&caps_list, &alc_caps1);
    esp_gmf_cap_append(&caps_list, &alc_caps2);
    esp_gmf_cap_t *tmp = caps_list;
    while (tmp) {
        ESP_LOGI(TAG, "%s,%ld,%p", EIGHTCC_2_STR(tmp->cap_eightcc), tmp->perf.oper_per_sec, tmp->attr_fun);
        tmp = tmp->next;
    }
    esp_gmf_cap_destroy(caps_list);
    caps_list = NULL;
    for (int i = 0; i < 10; ++i) {
        esp_gmf_cap_append(&caps_list, &alc_caps);
        esp_gmf_cap_append(&caps_list, &alc_caps1);
        esp_gmf_cap_append(&caps_list, &alc_caps2);
        esp_gmf_cap_destroy(caps_list);
        caps_list = NULL;
    }
    ESP_GMF_MEM_SHOW(TAG);
}

TEST_CASE("GMF CAPS Iterate test", "ESP_GMF_CAPS")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_CAPS", ESP_LOG_DEBUG);

    esp_gmf_cap_t alc_caps = {0};
    alc_caps.cap_eightcc = STR_2_EIGHTCC("AUDALC");
    alc_caps.perf.oper_per_sec = 100;
    alc_caps.attr_fun = audio_attr_iter_fun;

    ESP_LOGI(TAG, "--- MULTIPLE PROPERTY TEST ---");
    esp_gmf_cap_attr_t out_attr = {0};
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_cap_find_attr(&alc_caps, STR_2_EIGHTCC("RATE"), &out_attr));
    bool is_support = false;
    esp_gmf_cap_attr_check_value(&out_attr, 16000, &is_support);
    TEST_ASSERT_TRUE(is_support);
    esp_gmf_cap_attr_check_value(&out_attr, 23000, &is_support);
    TEST_ASSERT_FALSE(is_support);

    uint32_t support_sample_rate1[] = {8000, 16000, 24000,
                                       32000, 48000, 64000, 96000, 192000
                                      };
    // uint32_t support_sample_rate2[] = {11025, 22050, 44100, 88200};

    uint32_t support_sample_rate3[] = {8000, 11025, 16000, 22050, 24000,
                                       32000, 44100, 48000, 64000, 88200, 96000, 192000
                                      };

    uint32_t unsupport_sample_rate[] = {1000, 4000, 16008, 22051, 44000, 39000, 199000};
    for (int i = 0; i < sizeof(support_sample_rate1) / sizeof(uint32_t); ++i) {
        esp_gmf_cap_attr_check_value(&out_attr, support_sample_rate1[i], &is_support);
        TEST_ASSERT_TRUE(is_support);
    }

    for (int i = 0; i < sizeof(support_sample_rate3) / sizeof(uint32_t); ++i) {
        esp_gmf_cap_t *tmp_caps = &alc_caps;
        int ret = esp_gmf_cap_find_attr(tmp_caps, STR_2_EIGHTCC("RATE"), &out_attr);

        while (ret == ESP_GMF_ERR_OK) {
            esp_gmf_cap_attr_check_value(&out_attr, support_sample_rate3[i], &is_support);
            if (is_support) {
                ESP_LOGI(TAG, "%ld rate is supported", support_sample_rate3[i]);
                break;
            } else {
                tmp_caps = tmp_caps->next;
                if (tmp_caps == NULL) {
                   break;
                }
                ret = esp_gmf_cap_find_attr(tmp_caps, STR_2_EIGHTCC("RATE"), &out_attr);
            }
        }
    }

    for (int i = 0; i < sizeof(unsupport_sample_rate) / sizeof(uint32_t); ++i) {
        esp_gmf_cap_attr_check_value(&out_attr, unsupport_sample_rate[i], &is_support);
        ESP_LOGI(TAG, "MULTI,check value, %d", is_support);
        TEST_ASSERT_FALSE(is_support);
    }

    bool is_last = false;
    uint8_t cnt = 0;

    while (is_last == false) {
        uint32_t value = 0;
        esp_gmf_cap_attr_iterator_value(&out_attr, &value, &is_last);
        ESP_LOGI(TAG, "MULTI,iterate value, %s, %d", EIGHTCC_2_STR(out_attr.fourcc), is_support);
        for (int i = 0; i < sizeof(support_sample_rate1) / sizeof(uint32_t); ++i) {
            if (value == support_sample_rate1[i]) {
                ESP_LOGI(TAG, "MULTI:%ld is supported", value);
                cnt++;
                break;
            }
        }
    }
    TEST_ASSERT_EQUAL(cnt, sizeof(support_sample_rate1) / sizeof(uint32_t));

    cnt = 0;
    while (is_last == false) {
        uint32_t value = 0;
        esp_gmf_cap_attr_iterator_value(&out_attr, &value, &is_last);
        for (int i = 0; i < sizeof(unsupport_sample_rate) / sizeof(uint32_t); ++i) {
            if (value == unsupport_sample_rate[i]) {
                ESP_LOGI(TAG, "MULTI: %ld is not supported", value);
                cnt++;
            }
        }
    }
    TEST_ASSERT_EQUAL(cnt, 0);
    ESP_LOGI(TAG, "--- MULTIPLE PROPERTY TEST END ---\r\n");


    ESP_LOGI(TAG, "--- STEPWISE PROPERTY TEST ---");

    uint32_t support_stepwise_test[] = {8000, 11000, 14000, 17000, 20000};
    uint32_t unsupport_stepwise_test[] = {7000, 11900, 13000, 19000, 21000, 25000};
    esp_gmf_cap_find_attr(&alc_caps, STR_2_EIGHTCC("TEST"), &out_attr);
    is_last = false;
    cnt = 0;
    while (is_last == false) {
        uint32_t value = 0;
        esp_gmf_cap_attr_iterator_value(&out_attr, &value, &is_last);
        for (int i = 0; i < sizeof(support_stepwise_test) / sizeof(uint32_t); ++i) {
            if (value == support_stepwise_test[i]) {
                ESP_LOGI(TAG, "STEP: %ld is supported", value);
                cnt++;
            }
        }
    }
    TEST_ASSERT_EQUAL(cnt, sizeof(support_stepwise_test) / sizeof(uint32_t));

    cnt = 0;
    while (is_last == false) {
        uint32_t value = 0;
        esp_gmf_cap_attr_iterator_value(&out_attr, &value, &is_last);
        for (int i = 0; i < sizeof(unsupport_stepwise_test) / sizeof(uint32_t); ++i) {
            if (value == unsupport_stepwise_test[i]) {
                ESP_LOGI(TAG, "STEP: %ld is not supported", value);
                cnt++;
            }
        }
    }
    TEST_ASSERT_EQUAL(cnt, 0);

    is_support = false;
    for (int i = 0; i < sizeof(support_stepwise_test) / sizeof(uint32_t); ++i) {
        esp_gmf_cap_attr_check_value(&out_attr, support_stepwise_test[i], &is_support);

        TEST_ASSERT_TRUE(is_support);
    }

    is_support = false;
    for (int i = 0; i < sizeof(unsupport_stepwise_test) / sizeof(uint32_t); ++i) {
        esp_gmf_cap_attr_check_value(&out_attr, unsupport_stepwise_test[i], &is_support);
        ESP_LOGI(TAG, "STEP: %ld, support:%d", unsupport_stepwise_test[i], is_support);
        TEST_ASSERT_FALSE(is_support);
    }
    ESP_LOGI(TAG, "--- STEPWISE PROPERTY TEST END ---\r\n");


    ESP_LOGI(TAG, "--- DISCRETE PROPERTY TEST ---");
    uint32_t support_discrete_test[] = {1, 2, 5, 11, 29, 88};
    uint32_t unsupport_discrete_test[] = {0, 4, 8, 30, 100};
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_cap_find_attr(&alc_caps, STR_2_EIGHTCC("CHAN"), &out_attr));
    is_last = false;
    cnt = 0;
    while (is_last == false) {
        uint32_t value = 0;
        esp_gmf_cap_attr_iterator_value(&out_attr, &value, &is_last);
        for (int i = 0; i < sizeof(support_discrete_test) / sizeof(uint32_t); ++i) {
            if (value == support_discrete_test[i]) {
                cnt++;
            }
            ESP_LOGI(TAG, "DISCRETE: val: %ld, src:%ld", value, support_discrete_test[i]);
        }
    }
    TEST_ASSERT_EQUAL(cnt, sizeof(support_discrete_test) / sizeof(uint32_t));

    cnt = 0;
    while (is_last == false) {
        uint32_t value = 0;
        esp_gmf_cap_attr_iterator_value(&out_attr, &value, &is_last);
        for (int i = 0; i < sizeof(unsupport_discrete_test) / sizeof(uint32_t); ++i) {
            if (value == unsupport_discrete_test[i]) {
                ESP_LOGI(TAG, "DISCRETE: %ld is not supported", value);
                cnt++;
            }
        }
    }
    TEST_ASSERT_EQUAL(cnt, 0);

    is_support = false;
    for (int i = 0; i < sizeof(support_discrete_test) / sizeof(uint32_t); ++i) {
        esp_gmf_cap_attr_check_value(&out_attr, support_discrete_test[i], &is_support);
        TEST_ASSERT_TRUE(is_support);
    }

    is_support = false;
    for (int i = 0; i < sizeof(unsupport_discrete_test) / sizeof(uint32_t); ++i) {
        esp_gmf_cap_attr_check_value(&out_attr, unsupport_discrete_test[i], &is_support);
        TEST_ASSERT_FALSE(is_support);
    }
    ESP_LOGI(TAG, "--- DISCRETE PROPERTY TEST END ---\r\n");


    ESP_LOGI(TAG, "--- CONSTANT PROPERTY TEST ---");
    uint32_t support_constant_test[] = {2000};
    uint32_t unsupport_constant_test[] = {0, 1999, 3000};
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_cap_find_attr(&alc_caps, STR_2_EIGHTCC("VALUE"), &out_attr));
    is_last = false;
    cnt = 0;
    while (is_last == false) {
        uint32_t value = 0;
        esp_gmf_cap_attr_iterator_value(&out_attr, &value, &is_last);
        for (int i = 0; i < sizeof(support_constant_test) / sizeof(uint32_t); ++i) {
            if (value == support_constant_test[i]) {
                cnt++;
            }
            ESP_LOGI(TAG, "CONSTANT: val: %ld, src:%ld", value, support_constant_test[i]);
        }
    }
    TEST_ASSERT_EQUAL(cnt, sizeof(support_constant_test) / sizeof(uint32_t));

    cnt = 0;
    while (is_last == false) {
        uint32_t value = 0;
        esp_gmf_cap_attr_iterator_value(&out_attr, &value, &is_last);
        for (int i = 0; i < sizeof(unsupport_constant_test) / sizeof(uint32_t); ++i) {
            if (value == unsupport_constant_test[i]) {
                ESP_LOGI(TAG, "CONSTANT: %ld is not supported", value);
                cnt++;
            }
        }
    }
    TEST_ASSERT_EQUAL(cnt, 0);

    is_support = false;
    for (int i = 0; i < sizeof(support_constant_test) / sizeof(uint32_t); ++i) {
        esp_gmf_cap_attr_check_value(&out_attr, support_constant_test[i], &is_support);
        TEST_ASSERT_TRUE(is_support);
    }

    is_support = false;
    for (int i = 0; i < sizeof(unsupport_constant_test) / sizeof(uint32_t); ++i) {
        esp_gmf_cap_attr_check_value(&out_attr, unsupport_constant_test[i], &is_support);
        TEST_ASSERT_FALSE(is_support);
    }

    out_attr.value.c.data = 3000;
    esp_gmf_cap_attr_check_value(&out_attr, support_constant_test[0], &is_support);
    TEST_ASSERT_FALSE(is_support);
    esp_gmf_cap_attr_check_value(&out_attr, 3000, &is_support);
    TEST_ASSERT_TRUE(is_support);

    ESP_LOGI(TAG, "--- CONSTANT PROPERTY TEST END ---\r\n");
}

TEST_CASE("GMF CAPS Boundary Value test", "ESP_GMF_CAPS")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_CAPS", ESP_LOG_DEBUG);

    esp_gmf_cap_t alc_caps = {0};
    alc_caps.cap_eightcc = STR_2_EIGHTCC("AUDALC");
    alc_caps.perf.oper_per_sec = 100;
    alc_caps.attr_fun = audio_attr_iter_fun;

    ESP_LOGI(TAG, "--- STEPWISE PROPERTY TEST ---");
    esp_gmf_cap_attr_t out_attr = {0};
    bool is_support = false;
    bool is_last = false;
    uint8_t cnt = 0;

    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_cap_find_attr(&alc_caps, STR_2_EIGHTCC("TET1"), &out_attr));
    esp_gmf_cap_attr_check_value(&out_attr, 8000, &is_support);
    TEST_ASSERT_TRUE(is_support);
    esp_gmf_cap_attr_check_value(&out_attr, 16000, &is_support);
    TEST_ASSERT_FALSE(is_support);

    uint32_t sample_rate1[] = {8000, 16000, 24000,
                                       32000, 48000, 64000, 96000, 192000};
    for (int i = 0; i < sizeof(sample_rate1) / sizeof(uint32_t); ++i) {
        esp_gmf_cap_attr_check_value(&out_attr, sample_rate1[i], &is_support);
        ESP_LOGI(TAG, "STEP, iterate value, %ld, support:%s", sample_rate1[i], is_support ? "TRUE" : "FALSE");
        if (i == 0) {
            TEST_ASSERT_TRUE(is_support);
        } else {
            TEST_ASSERT_FALSE(is_support);
        }
    }
    while (is_last == false) {
        uint32_t value = 0;
        esp_gmf_cap_attr_iterator_value(&out_attr, &value, &is_last);
        ESP_LOGI(TAG, "STEP,iterate value, %s, %d", EIGHTCC_2_STR(out_attr.fourcc), is_support);
        for (int i = 0; i < sizeof(sample_rate1) / sizeof(uint32_t); ++i) {
            if (value == sample_rate1[i]) {
                ESP_LOGI(TAG, "STEP:%ld is supported", value);
                cnt++;
                break;
            }
        }
    }
    TEST_ASSERT_EQUAL(cnt, 1);
    ESP_LOGI(TAG, "--- STEPWISE PROPERTY TEST END ---\r\n");


    ESP_LOGI(TAG, "--- MULTIPLE PROPERTY TEST ---");
    TEST_ASSERT_EQUAL(ESP_GMF_ERR_OK, esp_gmf_cap_find_attr(&alc_caps, STR_2_EIGHTCC("TET2"), &out_attr));
    esp_gmf_cap_attr_check_value(&out_attr, 8000, &is_support);
    TEST_ASSERT_TRUE(is_support);
    esp_gmf_cap_attr_check_value(&out_attr, 16000, &is_support);
    TEST_ASSERT_FALSE(is_support);
    for (int i = 0; i < sizeof(sample_rate1) / sizeof(uint32_t); ++i) {
        esp_gmf_cap_attr_check_value(&out_attr, sample_rate1[i], &is_support);
        ESP_LOGI(TAG, "MULTI, iterate value, %ld, support:%s", sample_rate1[i], is_support ? "TRUE" : "FALSE");
        if (i == 0) {
            TEST_ASSERT_TRUE(is_support);
        } else {
            TEST_ASSERT_FALSE(is_support);
        }
    }
    is_last = false;
    cnt = 0;
    while (is_last == false) {
        uint32_t value = 0;
        esp_gmf_cap_attr_iterator_value(&out_attr, &value, &is_last);
        ESP_LOGI(TAG, "MULTI, iterate %s, value:%ld", EIGHTCC_2_STR(out_attr.fourcc), value);
        for (int i = 0; i < sizeof(sample_rate1) / sizeof(uint32_t); ++i) {
            if (value == sample_rate1[i]) {
                ESP_LOGI(TAG, "MULTI:%ld is supported", value);
                cnt++;
                break;
            }
        }
    }
    TEST_ASSERT_EQUAL(cnt, 1);
    ESP_LOGI(TAG, "--- MULTIPLE PROPERTY TEST END ---\r\n");
}

static esp_err_t element_event(esp_gmf_event_pkt_t *event, void *ctx)
{
    ESP_LOGI(TAG, "Get event,from:%p, type:%d, sub:%d, payload:%p, size:%d",
             event->from, event->type, event->sub, event->payload, event->payload_size);
    return ESP_OK;
}

TEST_CASE("GMF Element CAPS test", "ESP_GMF_CAPS")
{
    ESP_GMF_MEM_SHOW(TAG);
    fake_dec_cfg_t fake_cfg = DEFAULT_FAKE_DEC_CONFIG();
    fake_cfg.cb = element_event;
    esp_gmf_element_handle_t fake_dec_el = NULL;
    fake_dec_init(&fake_cfg, &fake_dec_el);

    const esp_gmf_cap_t *caps = NULL;
    esp_gmf_element_get_caps(fake_dec_el, &caps);
    printf("caps:%p\r\n", caps);
    ESP_LOGI(TAG, "--- STEPWISE PROPERTY TEST ---");
    esp_gmf_cap_attr_t out_attr;
    uint32_t support_stepwise_test[] = {8000, 11000, 14000, 17000, 20000};
    uint32_t unsupport_stepwise_test[] = {7000, 11900, 13000, 19000, 21000, 25000};
    esp_gmf_cap_find_attr((esp_gmf_cap_t*)caps, STR_2_EIGHTCC("TEST"), &out_attr);
    bool is_last = false;
    int cnt = 0;
    bool is_support = false;
    while (is_last == false) {
        uint32_t value = 0;
        esp_gmf_cap_attr_iterator_value(&out_attr, &value, &is_last);
        for (int i = 0; i < sizeof(support_stepwise_test) / sizeof(uint32_t); ++i) {
            if (value == support_stepwise_test[i]) {
                ESP_LOGI(TAG, "STEP: %ld is supported", value);
                cnt++;
            }
        }
    }
    TEST_ASSERT_EQUAL(cnt, sizeof(support_stepwise_test) / sizeof(uint32_t));

    cnt = 0;
    while (is_last == false) {
        uint32_t value = 0;
        esp_gmf_cap_attr_iterator_value(&out_attr, &value, &is_last);
        for (int i = 0; i < sizeof(unsupport_stepwise_test) / sizeof(uint32_t); ++i) {
            if (value == unsupport_stepwise_test[i]) {
                ESP_LOGI(TAG, "STEP: %ld is not supported", value);
                cnt++;
            }
        }
    }
    TEST_ASSERT_EQUAL(cnt, 0);

    is_support = false;
    for (int i = 0; i < sizeof(support_stepwise_test) / sizeof(uint32_t); ++i) {
        esp_gmf_cap_attr_check_value(&out_attr, support_stepwise_test[i], &is_support);

        TEST_ASSERT_TRUE(is_support);
    }

    is_support = false;
    for (int i = 0; i < sizeof(unsupport_stepwise_test) / sizeof(uint32_t); ++i) {
        esp_gmf_cap_attr_check_value(&out_attr, unsupport_stepwise_test[i], &is_support);
        ESP_LOGI(TAG, "STEP: %ld, support:%d", unsupport_stepwise_test[i], is_support);
        TEST_ASSERT_FALSE(is_support);
    }
    ESP_LOGI(TAG, "--- STEPWISE PROPERTY TEST END ---\r\n");
    esp_gmf_obj_delete(fake_dec_el);
}
