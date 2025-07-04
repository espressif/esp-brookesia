/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_gmf_uri_parser.h"

void esp_gmf_uri_free(esp_gmf_uri_t *uri)
{
    if (uri->scheme) {
        free(uri->scheme);
    }
    if (uri->userinfo) {
        free(uri->userinfo);
    }
    if (uri->username) {
        free(uri->username);
    }
    if (uri->password) {
        free(uri->password);
    }
    if (uri->host) {
        free(uri->host);
    }
    if (uri->path) {
        free(uri->path);
    }
    if (uri->query) {
        free(uri->query);
    }
    if (uri->fragment) {
        free(uri->fragment);
    }
    free(uri);
}

int esp_gmf_uri_parse(const char *uri_str, esp_gmf_uri_t **uri_out)
{
    if (!uri_str || !uri_out) {
        printf("Invalid parameters, str:%p, out:%p\r\n", uri_str, uri_out);
        return -1;
    }
    const size_t len = strlen(uri_str);
    if (len == 0) {
        return -1;
    }

    esp_gmf_uri_t *uri = calloc(1, sizeof(esp_gmf_uri_t));
    if (!uri) {
        return -1;
    }

    const char *curr = uri_str;

    // Parsing scheme
    const char *scheme_end = strstr(curr, "://");
    if (!scheme_end) {
        esp_gmf_uri_free(uri);
        return -1;
    }
    size_t scheme_len = scheme_end - curr;
    uri->scheme = strndup(curr, scheme_len);
    curr = scheme_end + 3;  // Move past "://"

    // Parsing userinfo (if present)
    const char *host_start = strchr(curr, '@');
    if (host_start) {
        size_t userinfo_len = host_start - curr;
        uri->userinfo = strndup(curr, userinfo_len);
        curr = host_start + 1;

        // Split userinfo into username and password
        const char *password_sep = strchr(uri->userinfo, ':');
        if (password_sep) {
            size_t username_len = password_sep - uri->userinfo;
            uri->username = strndup(uri->userinfo, username_len);
            uri->password = strndup(password_sep + 1, userinfo_len - username_len - 1);
        } else {
            uri->username = strdup(uri->userinfo);
        }
    }
    // Parsing host
    const char *port_start = strchr(curr, ':');
    const char *path_start = strchr(curr, '/');
    const char *host_end = path_start ? path_start : curr + strlen(curr);

    if (port_start && port_start < host_end) {
        uri->host = strndup(curr, port_start - curr);
        uri->port = atoi(port_start + 1);
        curr = port_start + 1;
    } else {
        uri->host = strndup(curr, host_end - curr);
    }
    if (path_start) {
        curr = path_start;
        // Parsing path
        const char *query_start = strchr(curr, '?');
        const char *fragment_start = strchr(curr, '#');

        if (query_start) {
            uri->path = strndup(curr, query_start - curr);
            curr = query_start + 1;
        } else if (fragment_start) {
            uri->path = strndup(curr, fragment_start - curr);
            curr = fragment_start;
        } else {
            uri->path = strdup(curr);
            curr += strlen(curr);
        }
        // Parsing query
        if (query_start) {
            if (fragment_start) {
                uri->query = strndup(query_start + 1, fragment_start - query_start - 1);
                curr = fragment_start;
            } else {
                uri->query = strdup(query_start + 1);
                curr += strlen(curr);
            }
        }
        // Parsing fragment
        if (fragment_start) {
            uri->fragment = strdup(fragment_start + 1);
        }
    }
    *uri_out = uri;
    return 0;
}
