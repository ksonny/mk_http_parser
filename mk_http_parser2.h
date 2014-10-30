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

#ifndef MK_HTTP_PARSER_H
#define MK_HTTP_PARSER_H

/* General status */
#define MK_HTTP_PENDING -10  /* cannot complete until more data arrives */
#define MK_HTTP_ERROR    -1  /* found an error when parsing the string */
#define MK_HTTP_OK        0

typedef struct
{
    char *data;
    unsigned long len;
} mk_pointer;

struct vhost {
    char *hostname;
};

#define MK_QUICK_HEADER_COUNT (8)
#define MK_REQUEST_FILTER_COUNT (4)

struct mk_quick_header
{
    unsigned int value_index;
    unsigned int value_len;
};

struct mk_request_info {
    mk_pointer method;
    mk_pointer protocol;
    mk_pointer uri;
    mk_pointer path;
    mk_pointer query;
    mk_pointer headers;
    mk_pointer body;
    struct vhost *vhost;

    struct mk_quick_header quick_headers[MK_QUICK_HEADER_COUNT];
};

enum mk_response_state {
    MK_RESPONSE_UNUSED = 0,
    MK_RESPONSE_NEW = 1,
    MK_RESPONSE_HEADER = 2,
    MK_RESPONSE_BODY = 3,
    MK_RESPONSE_FILTER = 4,
    MK_RESPONSE_DONE = 5,
};

struct mk_response_info {
    int http_status;
};

struct mk_request {
    enum mk_response_state state;
    struct mk_request_info request;
    struct mk_response_info response;
    struct mk_request *next;
};

int mk_http_parser(struct mk_request *sr,
        char *buffer,
        size_t length);


/* ANSI Colors */
#define ANSI_RESET "\033[0m"
#define ANSI_BOLD  "\033[1m"

#define ANSI_CYAN          "\033[36m"
#define ANSI_BOLD_CYAN     ANSI_BOLD ANSI_CYAN
#define ANSI_MAGENTA       "\033[35m"
#define ANSI_BOLD_MAGENTA  ANSI_BOLD ANSI_MAGENTA
#define ANSI_RED           "\033[31m"
#define ANSI_BOLD_RED      ANSI_BOLD ANSI_RED
#define ANSI_YELLOW        "\033[33m"
#define ANSI_BOLD_YELLOW   ANSI_BOLD ANSI_YELLOW
#define ANSI_BLUE          "\033[34m"
#define ANSI_BOLD_BLUE     ANSI_BOLD ANSI_BLUE
#define ANSI_GREEN         "\033[32m"
#define ANSI_BOLD_GREEN    ANSI_BOLD ANSI_GREEN
#define ANSI_WHITE         "\033[37m"
#define ANSI_BOLD_WHITE    ANSI_BOLD ANSI_WHITE

#define TEST_OK      0
#define TEST_FAIL    1

#endif // MK_HTTP_PARSER_H
