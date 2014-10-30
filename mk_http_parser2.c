/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Monkey HTTP Server
 *  ==================
 *  Copyright 2001-2014 Monkey Software LLC <eduardo@monkey.io>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "mk_http_parser2.h"

#define mark_end()    req->end   = i; eval_field(req, buffer)
#define parse_next()  req->start = i + 1; continue
#define field_len()   (req->end - req->start)

#define MK_TRACE(M, ...) printf(M "\n", ##__VA_ARGS__)

#define MK_ENDBLOCK "\r\n\r\n"

/* Methods */
enum mk_http_method {
    HTTP_METHOD_UNKNOWN = -1,
#define HTTP_METHOD_UNKNOWN -1
    HTTP_METHOD_GET = 0,
#define HTTP_METHOD_GET             0
    HTTP_METHOD_POST = 1,
#define HTTP_METHOD_POST            1
    HTTP_METHOD_HEAD = 2,
#define HTTP_METHOD_HEAD            2
    HTTP_METHOD_PUT = 3,
#define HTTP_METHOD_PUT             3
    HTTP_METHOD_DELETE = 4,
#define HTTP_METHOD_DELETE          4
};

#define HTTP_METHOD_GET_STR         "GET"
#define HTTP_METHOD_POST_STR        "POST"
#define HTTP_METHOD_HEAD_STR        "HEAD"
#define HTTP_METHOD_PUT_STR         "PUT"
#define HTTP_METHOD_DELETE_STR      "DELETE"

enum mk_http_protocol {
    HTTP_PROTOCOL_UNKNOWN = -1,
#define HTTP_PROTOCOL_UNKNOWN (-1)
    HTTP_PROTOCOL_09 = 9,
#define HTTP_PROTOCOL_09 (9)
    HTTP_PROTOCOL_12 = 10,
#define HTTP_PROTOCOL_10 (10)
    HTTP_PROTOCOL_11 = 11,
#define HTTP_PROTOCOL_11 (11)
};

#define HTTP_PROTOCOL_09_STR "HTTP/0.9"
#define HTTP_PROTOCOL_10_STR "HTTP/1.0"
#define HTTP_PROTOCOL_11_STR "HTTP/1.1"

/* Client Errors */
#define MK_CLIENT_BAD_REQUEST			400
#define MK_CLIENT_UNAUTH			401
#define MK_CLIENT_PAYMENT_REQ   		402     /* Wtf?! :-) */
#define MK_CLIENT_FORBIDDEN			403
#define MK_CLIENT_NOT_FOUND			404
#define MK_CLIENT_METHOD_NOT_ALLOWED		405
#define MK_CLIENT_NOT_ACCEPTABLE		406
#define MK_CLIENT_PROXY_AUTH			407
#define MK_CLIENT_REQUEST_TIMEOUT		408
#define MK_CLIENT_CONFLICT			409
#define MK_CLIENT_GONE				410
#define MK_CLIENT_LENGTH_REQUIRED		411
#define MK_CLIENT_PRECOND_FAILED		412
#define MK_CLIENT_REQUEST_ENTITY_TOO_LARGE	413
#define MK_CLIENT_REQUEST_URI_TOO_LONG		414
#define MK_CLIENT_UNSUPPORTED_MEDIA		415
#define MK_CLIENT_REQUESTED_RANGE_NOT_SATISF    416

/* Server Errors */
#define MK_SERVER_INTERNAL_ERROR		500
#define MK_SERVER_NOT_IMPLEMENTED		501
#define MK_SERVER_BAD_GATEWAY			502
#define MK_SERVER_SERVICE_UNAV			503
#define MK_SERVER_GATEWAY_TIMEOUT		504
#define MK_SERVER_HTTP_VERSION_UNSUP		505

static const char *mk_quick_headers[MK_QUICK_HEADER_COUNT] = {
    "Host",
    "Accept-Encoding",
    "Last-Modified",
    "If-Modified-Since",
    "Range",
    "Content-Length"
};

static int mk_http_quick_header_index(const char * restrict key)
{
    unsigned int i;
    char c = toupper(key[0]);

    for (i = 0; i < MK_QUICK_HEADER_COUNT; i++) {
        if (mk_quick_headers[i] == NULL) {
            continue;
        }
        else if (c != mk_quick_headers[i][0]) {
            continue;
        }
        else if (!strcasecmp(key, mk_quick_headers[i])) {
            return i;
        }
    }

    return -1;
}

static enum mk_http_method mk_http_method_check(mk_pointer method)
{
    if (method.len == sizeof(HTTP_METHOD_GET_STR) - 1 &&
            !strncmp(method.data, HTTP_METHOD_GET_STR, method.len)) {
        return HTTP_METHOD_GET;
    }
    if (method.len == sizeof(HTTP_METHOD_PUT_STR) - 1 &&
            !strncmp(method.data, HTTP_METHOD_PUT_STR, method.len)) {
        return HTTP_METHOD_PUT;
    }
    if (method.len == sizeof(HTTP_METHOD_POST_STR) - 1 &&
            !strncmp(method.data, HTTP_METHOD_POST_STR, method.len)) {
        return HTTP_METHOD_POST;
    }
    if (method.len == sizeof(HTTP_METHOD_HEAD_STR) - 1 &&
            !strncmp(method.data, HTTP_METHOD_HEAD_STR, method.len)) {
        return HTTP_METHOD_HEAD;
    }
    if (method.len == sizeof(HTTP_METHOD_DELETE_STR) - 1 &&
            !strncmp(method.data, HTTP_METHOD_DELETE_STR, method.len)) {
        return HTTP_METHOD_DELETE;
    }

    return HTTP_METHOD_UNKNOWN;
}

static enum mk_http_protocol mk_http_protocol_check(mk_pointer protocol)
{
    if (!strncmp(protocol.data, HTTP_PROTOCOL_11_STR, protocol.len)) {
        return HTTP_PROTOCOL_11;
    }
    if (!strncmp(protocol.data, HTTP_PROTOCOL_10_STR, protocol.len)) {
        return HTTP_PROTOCOL_10;
    }
    if (!strncmp(protocol.data, HTTP_PROTOCOL_09_STR, protocol.len)) {
        return HTTP_PROTOCOL_09;
    }

    return HTTP_PROTOCOL_UNKNOWN;
}

static char *url_decode(mk_pointer uri)
{
    // TODO: Implement
    return uri.data;
}

static void parse_uri(mk_pointer uri, struct mk_request_info *info)
{
    unsigned int i;
    unsigned int endPath = 0, endQuery = 0;
    mk_pointer path;
    char *tmp;

    for (i = 0; i < uri.len && uri.data[i] != '?'; i++);
    endPath = i;
    if (i == '?') {
        endQuery = uri.len;
    }

    path.data = uri.data;
    path.len = endPath;
    tmp = url_decode(path);
    if (tmp) {
        info->path.data = tmp;
        info->path.len = strlen(tmp);
    }
    else {
        info->path = path;
    }

    if (endQuery > 0) {
        info->query.data = uri.data + endPath + 1;
        info->query.len = endQuery - endPath - 1;
    }
    else {
        info->query.data = NULL;
        info->query.len = 0;
    }
}

int mk_http_request_header(const struct mk_request_info *info,
        const char *key,
        const char **value,
        size_t *value_len)
{
    char *d, *p;
    off_t rem;
    size_t len;
    int quick_index;

    quick_index = mk_http_quick_header_index(key);
    if (quick_index >= 0) {
        if (info->quick_headers[quick_index].value_len != 0) {
            if (value != NULL) {
                *value = info->headers.data +
                    info->quick_headers[quick_index].value_index;
            }
            if (value_len != NULL) {
                *value_len = info->quick_headers[quick_index].value_len;
            }
            return 0;
        }
        else {
            return -1;
        }
    }

    d = info->headers.data;
    len = strlen(key);

    do {
        rem = info->headers.len - (d - info->headers.data);
        p = memchr(d, ':', rem);
        if (!p) {
            break;
        }
        else if ((size_t)(p - d) == len) {
            if (!strncasecmp(p - len, key, len)) {
                for (; *p && (*p == ' '  || *p == ':'); p++);
                if (value != NULL) {
                    *value = p;
                }
                if (value_len != NULL) {
                    rem = info->headers.len - (p - info->headers.data);
                    d = memchr(p, '\n', rem);
                    if (!d) {
                            d = info->headers.data + info->headers.len;
                    }
                    if (d[-1] == '\r') {
                        *value_len = d - p - 1;
                    }
                    else {
                        *value_len = d - p;
                    }
                }
                return 0;
            }
        }
        rem = info->headers.len - (p - info->headers.data);
        d = memchr(p, '\n', rem);
        if (d) {
            d++;
        }
    } while (p && d);

    return -1;
}

int http_request_info(struct mk_request *sr, struct mk_request_info *info)
{
    const char *hostname;
    size_t hostname_len;

    if (sr->request.vhost == NULL) {
        MK_TRACE("Get vhost entry.");

        if (!mk_http_request_header(&sr->request, "Host",
                    &hostname, &hostname_len)) {
            /*
             * TODO: Lookup vhost
            sr->request.vhost = mk_config_host_find(&config->host_list,
                    "HTTP", 4,
                    hostname, hostname_len);
                    */
        }
    }

    if (sr->request.path.data == NULL && sr->request.uri.data != NULL) {
        parse_uri(sr->request.uri, &sr->request);
    }

    *info = sr->request;
    return 0;
}

static void mk_http_premature_abort(struct mk_request *req, int status_code)
{
    req->response.http_status = status_code;
}

static void mk_response_set_status(struct mk_request *req, int status_code)
{
    req->response.http_status = status_code;
}

static int mk_http_headers_process(struct mk_request_info *info)
{
    unsigned int i;
    const char *key[MK_QUICK_HEADER_COUNT];
    char *d = info->headers.data, *p, *q;
    off_t rem;
    size_t len[MK_QUICK_HEADER_COUNT];
    size_t length;

    MK_TRACE("[http] Process headers.");

    for (i = 0; i < MK_QUICK_HEADER_COUNT; i++) {
        key[i] = mk_quick_headers[i] ? mk_quick_headers[i] : "";
        len[i] = strlen(key[i]);
    }

    do {
        rem = info->headers.len - (d - info->headers.data);
        q = memchr(d, '\n', rem);
        p = q ? memchr(d, ':', q - d) : NULL;
        if (!p) {
            info->headers.len = d - info->headers.data;
            MK_TRACE("[http] Header length is: %zd", info->headers.len);
            MK_TRACE("'''\n%.*s\n'''", (int)info->headers.len, info->headers.data);
            continue;
        }
        length = (p - d);
        for (i = 0; i < MK_QUICK_HEADER_COUNT; i++) {
            if (length != len[i]) {
                continue;
            }
            if (!strncasecmp(p - len[i], key[i], len[i])) {
                MK_TRACE("[http] Quick header '%s' set", key[i]);
                for (; *p && (*p == ' '  || *p == ':'); p++);
                info->quick_headers[i].value_index = p - info->headers.data;
                rem = info->headers.len - (p - info->headers.data);
                d = memchr(p, '\n', rem);
                if (!d) {
                    d = info->headers.data + info->headers.len;
                }
                if (d[-1] == '\r') {
                    info->quick_headers[i].value_len = d - p - 1;
                }
                else {
                    info->quick_headers[i].value_len = d - p;
                }
                break;
            }
        }

        rem = info->headers.len - (p - info->headers.data);
        d = memchr(p, '\n', rem);
        if (d) {
            d++;
        }
    } while (p && d);

    return 0;
}

static int mk_http_header_parse(mk_pointer request, struct mk_request_info *info)
{
    unsigned int method_len = 0, uri_len = 0, protocol_len = 0, headers_len = 0;
    char *method, *uri, *protocol, *headers, *tmp;

    method = request.data;
    tmp = memchr(method, ' ', request.len);
    if (tmp == NULL) {
        MK_TRACE("Error, first header can't be parsed.");
        return -1;
    }
    method_len = tmp - method;

    uri = tmp + 1;
    if (*uri != '/') {
        MK_TRACE("Error, first header can't be parsed.");
        return -1;
    }
    for (tmp = uri; tmp && *tmp != ' ' && *tmp != '\n'; tmp++);
    if (*tmp != ' ') {
        MK_TRACE("Error, first header can't be parsed.");
        return -1;
    }
    uri_len = tmp - uri;

    protocol = tmp + 1;
    tmp = memchr(protocol, '\n', request.len - (protocol - request.data));
    if (tmp == NULL) {
        MK_TRACE("Error, first header can't be parsed.");
        return -1;
    }
    protocol_len = tmp - protocol - 1;

    headers = tmp + 1;
    headers_len = (request.data + request.len) - headers; // Just guessing.

    memset(info, 0, sizeof(*info));
    info->protocol.data = protocol;
    info->protocol.len = protocol_len;
    info->method.data = method;
    info->method.len = method_len;
    info->uri.data = uri;
    info->uri.len = uri_len;
    info->headers.data = headers;
    info->headers.len = headers_len;

    if (headers_len == sizeof(MK_ENDBLOCK) - 3) {
        // First data is endblock.
        info->headers.len = 0;
        return 0;
    }
    else if (mk_http_headers_process(info)) {
        return -1;
    }
    else {
        return 0;
    }
}

static int mk_http_sanity_check(struct mk_request *sr)
{
    struct mk_request_info info;
    unsigned long content_length;
    const char *header, *tmp;
    size_t len;
    unsigned int i;
    int method;

    // Temporarily mark as ready for use
    sr->state = MK_RESPONSE_HEADER;

    http_request_info(sr, &info);

    /* Check backward directory request */
    if (memmem(info.path.data, info.path.len, "..", sizeof("..") - 1)) {
        mk_response_set_status(sr, MK_CLIENT_FORBIDDEN);
        goto error;
    }

    if (!mk_http_request_header(&info, "Host", &header, &len)) {
        tmp = memchr(header, ':', len);
        if (tmp) {
            tmp += 1;
            len = (header + len) - tmp;

            MK_TRACE("[http] Sanity checking port: '%.*s'", (int)len, tmp);

            if (len > 5) {
                MK_TRACE("[http] Port number too long.");
                mk_response_set_status(sr, MK_CLIENT_BAD_REQUEST);
                goto error;
            }
            else if (len == 5) {
                if (tmp[0] < '6') {
                }
                else if (tmp[0] > '6') {
                    goto error_port;
                }
                else if (tmp[1] < '5') {
                }
                else if (tmp[1] > '5') {
                    goto error_port;
                }
                else if (tmp[2] < '5') {
                }
                else if (tmp[2] > '5') {
                    goto error_port;
                }
                else if (tmp[3] < '3') {
                }
                else if (tmp[3] > '3') {
                    goto error_port;
                }
                else if (tmp[4] < '5') {
                }
                else if (tmp[4] > '5') {
                    goto error_port;
                }
                goto ok_port;
error_port:
                MK_TRACE("[http] Port number too large.");
                mk_response_set_status(sr, MK_CLIENT_BAD_REQUEST);
                goto error;
            }
ok_port:
            for (i = 0; i < len; i++) {
                if (!isdigit(tmp[i])) {
                    mk_response_set_status(sr, MK_CLIENT_BAD_REQUEST);
                    goto error;
                }
            }
        }
    }
    else if (mk_http_protocol_check(info.protocol) == HTTP_PROTOCOL_11) {
        MK_TRACE("No host header in HTTP/1.1 request.");
        mk_response_set_status(sr, MK_CLIENT_BAD_REQUEST);
        goto error;
    }

    method = mk_http_method_check(info.method);
    if (method == HTTP_METHOD_POST || method == HTTP_METHOD_PUT) {
        if (mk_http_request_header(&info, "Content-Length", &header, &len)) {
            MK_TRACE("Content length required.");
            mk_response_set_status(sr, MK_CLIENT_LENGTH_REQUIRED);
            goto error;
        }
        else if (sscanf(header, "%lu", &content_length) != 1) {
            MK_TRACE("Failed to get content length.");
            mk_response_set_status(sr, MK_CLIENT_BAD_REQUEST);
            goto error;
        }
        else if (content_length > 4096 /* (size_t)config->max_request_size */) {
            MK_TRACE("[http] Too large POST request, abort.");
            mk_response_set_status(sr, MK_CLIENT_REQUEST_ENTITY_TOO_LARGE);
            goto error;
        }
        else if (content_length != info.body.len) {
            MK_TRACE("Content length does not match body size, %zd != %zd",
                    content_length, info.body.len);
            mk_response_set_status(sr, MK_CLIENT_BAD_REQUEST);
            goto error;
        }
    }

    MK_TRACE("Sanity check complete.");

    sr->state = MK_RESPONSE_NEW;
    return 0;
error:
    /*
     * TODO: Add error page to response.
    mk_response_error_page(sr);
    mk_response_end(sr);
     */
    return -1;
}

static void mk_http_request_init(struct mk_request *sr)
{
    sr->next = NULL;
    sr->state = MK_RESPONSE_UNUSED;
}

int mk_http_parser(struct mk_request *sr,
        char *buffer,
        size_t length)
{
    int method;
    char *cur;
    const char *header;
    size_t len;
    size_t content_length = 0;
    struct mk_request *current_request = sr;
    struct mk_request_info info;
    mk_pointer request;
    unsigned int i;

    mk_http_request_init(current_request);

    cur = buffer;
    do {
        request.data = cur;
        request.len = (buffer + length) - cur;

        for (i = 0; i < request.len && cur[i] != '\n'; i++);
        if (i == request.len) {
            MK_TRACE("[http] Partial request, wait.");
            return 0;
        }

        if (mk_http_header_parse(request, &info)) {
            MK_TRACE("[http] Failed to parse headers.");
            mk_http_premature_abort(current_request, MK_CLIENT_BAD_REQUEST);
            goto error;
        }

        cur = info.headers.data + info.headers.len + 2;
        if (strncmp(cur - 4, MK_ENDBLOCK, sizeof(MK_ENDBLOCK) - 1)) {
            MK_TRACE("[http] No endblock in request, wait.");
            return 0;
        }

        method = mk_http_method_check(info.method);
        if (method == HTTP_METHOD_POST) {
            if (!mk_http_request_header(&info, "Content-Length", &header, &len)) {
                sscanf(header, "%lu", &content_length);
            }
            if (content_length > 4096 /* (size_t)config->max_request_size */) {
                MK_TRACE("[http] Too large POST request, abort.");
                mk_http_premature_abort(current_request, MK_CLIENT_REQUEST_ENTITY_TOO_LARGE);
                goto error;
            }

            info.body.data = cur;
            info.body.len = (buffer + length) - cur;
            cur = buffer + length;

            if (content_length > info.body.len) {
                MK_TRACE("[http] Partial post request.");
                return 0;
            }
            else if (content_length < info.body.len) {
                MK_TRACE("[http] Extra data in POST body.");
                mk_http_premature_abort(current_request, MK_CLIENT_BAD_REQUEST);
                goto error;
            }
        }

        if (current_request != sr) {
            // Pipelined requests
            current_request->next = malloc(sizeof(*current_request));
            if (current_request->next == NULL) {
                printf("Malloc() failed: %s", strerror(errno));
                mk_http_premature_abort(current_request, MK_SERVER_INTERNAL_ERROR);
                goto error;
            }
            current_request = current_request->next;
            mk_http_request_init(current_request);
        }
        current_request->state = MK_RESPONSE_NEW;
        current_request->request = info;

        if (mk_http_sanity_check(current_request)) {
            MK_TRACE("[http] Sanity check failed.");
            cur = buffer + length;
        }
    } while (cur < buffer + length);

    return 0;
error:
    return -1;
}
