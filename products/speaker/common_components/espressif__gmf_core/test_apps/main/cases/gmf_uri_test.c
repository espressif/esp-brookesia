/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include "unity.h"
#include "esp_gmf_uri_parser.h"

TEST_CASE("URI, Parser test", "ESP_GMF_URI")
{
    esp_gmf_uri_t *uri = NULL;

    TEST_ASSERT_EQUAL(0, esp_gmf_uri_parse("http://username:password@www.example.com:8080/path/to/resource?query=param#fragment", &uri));
    TEST_ASSERT_EQUAL_STRING("http", uri->scheme);
    TEST_ASSERT_EQUAL_STRING("username", uri->username);
    TEST_ASSERT_EQUAL_STRING("password", uri->password);
    TEST_ASSERT_EQUAL_STRING("www.example.com", uri->host);
    TEST_ASSERT_EQUAL_INT(8080, uri->port);
    TEST_ASSERT_EQUAL_STRING("/path/to/resource", uri->path);
    TEST_ASSERT_EQUAL_STRING("query=param", uri->query);
    TEST_ASSERT_EQUAL_STRING("fragment", uri->fragment);
    esp_gmf_uri_free(uri);

    TEST_ASSERT_EQUAL(0, esp_gmf_uri_parse("http://www.example.com", &uri));
    TEST_ASSERT_EQUAL_STRING("http", uri->scheme);
    TEST_ASSERT_EQUAL_STRING("www.example.com", uri->host);
    TEST_ASSERT_NULL(uri->path);
    TEST_ASSERT_NULL(uri->query);
    TEST_ASSERT_NULL(uri->fragment);
    esp_gmf_uri_free(uri);

    TEST_ASSERT_EQUAL(0, esp_gmf_uri_parse("http://www.example.com/path/to/resource", &uri));
    TEST_ASSERT_EQUAL_STRING("http", uri->scheme);
    TEST_ASSERT_EQUAL_STRING("www.example.com", uri->host);
    TEST_ASSERT_EQUAL_STRING("/path/to/resource", uri->path);
    TEST_ASSERT_NULL(uri->query);
    TEST_ASSERT_NULL(uri->fragment);
    esp_gmf_uri_free(uri);

    TEST_ASSERT_EQUAL(0, esp_gmf_uri_parse("http://www.example.com/path?query=param", &uri));
    TEST_ASSERT_EQUAL_STRING("http", uri->scheme);
    TEST_ASSERT_EQUAL_STRING("www.example.com", uri->host);
    TEST_ASSERT_EQUAL_STRING("/path", uri->path);
    TEST_ASSERT_EQUAL_STRING("query=param", uri->query);
    TEST_ASSERT_NULL(uri->fragment);
    esp_gmf_uri_free(uri);

    TEST_ASSERT_EQUAL(0, esp_gmf_uri_parse("http://www.example.com/path#fragment", &uri));
    TEST_ASSERT_EQUAL_STRING("http", uri->scheme);
    TEST_ASSERT_EQUAL_STRING("www.example.com", uri->host);
    TEST_ASSERT_EQUAL_STRING("/path", uri->path);
    TEST_ASSERT_NULL(uri->query);
    TEST_ASSERT_EQUAL_STRING("fragment", uri->fragment);
    esp_gmf_uri_free(uri);

    TEST_ASSERT_EQUAL(0, esp_gmf_uri_parse("http://www.example.com/path?query=param#fragment", &uri));
    TEST_ASSERT_EQUAL_STRING("http", uri->scheme);
    TEST_ASSERT_EQUAL_STRING("www.example.com", uri->host);
    TEST_ASSERT_EQUAL_STRING("/path", uri->path);
    TEST_ASSERT_EQUAL_STRING("query=param", uri->query);
    TEST_ASSERT_EQUAL_STRING("fragment", uri->fragment);
    esp_gmf_uri_free(uri);

    TEST_ASSERT_EQUAL(0, esp_gmf_uri_parse("http://www.example.com:8080", &uri));
    TEST_ASSERT_EQUAL_STRING("http", uri->scheme);
    TEST_ASSERT_EQUAL_STRING("www.example.com", uri->host);
    TEST_ASSERT_EQUAL_INT(8080, uri->port);
    TEST_ASSERT_NULL(uri->path);
    TEST_ASSERT_NULL(uri->query);
    TEST_ASSERT_NULL(uri->fragment);
    esp_gmf_uri_free(uri);

    TEST_ASSERT_EQUAL(0, esp_gmf_uri_parse("http://username:password@www.example.com", &uri));
    TEST_ASSERT_EQUAL_STRING("http", uri->scheme);
    TEST_ASSERT_EQUAL_STRING("username", uri->username);
    TEST_ASSERT_EQUAL_STRING("password", uri->password);
    TEST_ASSERT_EQUAL_STRING("www.example.com", uri->host);
    esp_gmf_uri_free(uri);

    TEST_ASSERT_EQUAL(0, esp_gmf_uri_parse("http://username:password@www.example.com:8080/path?query=param#fragment", &uri));
    TEST_ASSERT_EQUAL_STRING("http", uri->scheme);
    TEST_ASSERT_EQUAL_STRING("username", uri->username);
    TEST_ASSERT_EQUAL_STRING("password", uri->password);
    TEST_ASSERT_EQUAL_STRING("www.example.com", uri->host);
    TEST_ASSERT_EQUAL_INT(8080, uri->port);
    TEST_ASSERT_EQUAL_STRING("/path", uri->path);
    TEST_ASSERT_EQUAL_STRING("query=param", uri->query);
    TEST_ASSERT_EQUAL_STRING("fragment", uri->fragment);
    esp_gmf_uri_free(uri);

    TEST_ASSERT_EQUAL(0, esp_gmf_uri_parse("https://www.secure.com", &uri));
    TEST_ASSERT_EQUAL_STRING("https", uri->scheme);
    TEST_ASSERT_EQUAL_STRING("www.secure.com", uri->host);
    esp_gmf_uri_free(uri);

    TEST_ASSERT_EQUAL(0, esp_gmf_uri_parse("ftp://ftp.example.com/resource", &uri));
    TEST_ASSERT_EQUAL_STRING("ftp", uri->scheme);
    TEST_ASSERT_EQUAL_STRING("ftp.example.com", uri->host);
    TEST_ASSERT_EQUAL_STRING("/resource", uri->path);
    esp_gmf_uri_free(uri);

    TEST_ASSERT_EQUAL(0, esp_gmf_uri_parse("http:///test.mp3", &uri));
    TEST_ASSERT_EQUAL_STRING("http", uri->scheme);
    TEST_ASSERT_EQUAL_STRING("", uri->host);
    TEST_ASSERT_EQUAL_STRING("/test.mp3", uri->path);
    esp_gmf_uri_free(uri);

    TEST_ASSERT_EQUAL(0, esp_gmf_uri_parse("http://test.mp3", &uri));
    TEST_ASSERT_EQUAL_STRING("http", uri->scheme);
    TEST_ASSERT_EQUAL_STRING("test.mp3", uri->host);
    esp_gmf_uri_free(uri);

    TEST_ASSERT_EQUAL(-1, esp_gmf_uri_parse("mailto:user@example.com", &uri));
    TEST_ASSERT_EQUAL(-1, esp_gmf_uri_parse("data:text/plain;base64,SGVsbG8sIFdvcmxkIQ==", &uri));
    TEST_ASSERT_EQUAL(-1, esp_gmf_uri_parse("www.example.com/path", &uri));
    TEST_ASSERT_EQUAL(-1, esp_gmf_uri_parse("", &uri));
    TEST_ASSERT_EQUAL(-1, esp_gmf_uri_parse("data:application//octet-stream;base64,SGVsbG8sIFdvcmxkIQ==", &uri));

    // TEST_ASSERT_EQUAL(-1, esp_gmf_uri_parse("http://www.example.com/path/%E0%A4%A", &uri));
    // TEST_ASSERT_EQUAL(-1, esp_gmf_uri_parse("http://username:pass word@www.exa mple.com:80a0/path/to/resource?key1=&=value2#fragment#anotherfragment", &uri));
    // TEST_ASSERT_EQUAL(-1, esp_gmf_uri_parse("ht@tp://www.example.com", &uri));
    // TEST_ASSERT_EQUAL(-1, esp_gmf_uri_parse("http://www.exa mple.com", &uri));
    // TEST_ASSERT_EQUAL(-1, esp_gmf_uri_parse("http://www.example.com:80a0", &uri));
    // TEST_ASSERT_EQUAL(-1, esp_gmf_uri_parse("http://www.example.com/pa th", &uri));
    // TEST_ASSERT_EQUAL(-1, esp_gmf_uri_parse("http://www.example.com//path/to//resource", &uri));
    // TEST_ASSERT_EQUAL(-1, esp_gmf_uri_parse("http://www.example.com/path#fragment#anotherfragment", &uri));
    // TEST_ASSERT_EQUAL(-1, esp_gmf_uri_parse("http://www.example.com/path?key1=&=value2", &uri));
    // TEST_ASSERT_EQUAL(-1, esp_gmf_uri_parse("http://[2001:db8::1g]:8080/path", &uri));
    // TEST_ASSERT_EQUAL(-1, esp_gmf_uri_parse("http://[2001:db8::1:8080/path", &uri));
}
