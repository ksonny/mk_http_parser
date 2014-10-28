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
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <limits.h>

#include "mk_http.h"


/* Request levels
 * ==============
 *
 * 1. FIRST_LINE         : Method, URI (+ QS) + Protocol version + CRLF
 * 2. HEADERS (optional) : KEY, SEP, VALUE + CRLF
 * 3. BODY (option)      : data based on Content-Length or Chunked transfer encoding
 */

enum {
    REQ_LEVEL_FIRST    = 1,
    REQ_LEVEL_HEADERS  = 2,
    REQ_LEVEL_BODY     = 3
};

/* Statuses per levels */
enum {
    /* REQ_LEVEL_FIRST */
    MK_ST_REQ_METHOD       = 1,
    MK_ST_REQ_URI          ,
    MK_ST_REQ_QUERY_STRING ,
    MK_ST_REQ_PROT_VERSION ,
    MK_ST_FIRST_CONTINUE   ,
    MK_ST_FIRST_FINALIZE   ,    /* LEVEL_FIRST finalize the request */

    /* REQ_HEADERS */
    MK_ST_HEADER_KEY       ,
    MK_ST_HEADER_VALUE     ,
    MK_ST_HEADER_END       ,
    MK_ST_BLOCK_END        ,

    MK_ST_LF               ,
    MK_ST_DONE
};

typedef struct {
    int level;   /* request level */
    int status;  /* level status */
    int next;    /* something next after status ? */
    int length;

    /* lookup fields */
    int start;
    int end;
} mk_http_request_t;


static inline int _set_method(mk_http_request_t *req)
{
    return -1;
}

void p_field(mk_http_request_t *req, char *buffer)
{
    int i;

    for (i = req->start; i < req->end; i++) {
        printf("%c", buffer[i]);
    }
}

static inline int eval_field(mk_http_request_t *req, char *buffer)
{
    if (req->level == REQ_LEVEL_FIRST) {
        printf("[ \033[35mfirst level\033[0m ] ");
    }
    else {
        printf("[   \033[36mheaders\033[0m   ] ");
    }

    printf(" ");
    switch (req->status) {
    case MK_ST_REQ_METHOD:
        printf("MK_ST_REQ_METHOD      : ");
        break;
    case MK_ST_REQ_URI:
        printf("MK_ST_REQ_URI         : ");
        break;
    case MK_ST_REQ_QUERY_STRING:
        printf("MK_ST_REQ_QUERY_STRING: ");
        break;
    case MK_ST_REQ_PROT_VERSION:
        printf("MK_ST_REQ_PROT_VERSION: ");
        break;
    case MK_ST_HEADER_KEY:
        printf("MK_ST_HEADER_KEY      : ");
        break;
    case MK_ST_HEADER_VALUE:
        printf("MK_ST_HEADER_VALUE    : ");
        break;
    default:
        printf("\033[31mUNKNOWN UNKNOWN\033[0m       : ");
        break;
    };


    p_field(req, buffer);
    printf("\n");

    return 0;
}


#define mark_end()    req->end   = i; eval_field(req, buffer)
#define parse_next()  req->start = i + 1; continue
#define field_len()   (req->end - req->start)

/*
 * Parse the protocol and point relevant fields, don't take logic decisions
 * based on this, just parse to locate things.
 */
int mk_http_parser(mk_http_request_t *req, char *buffer, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        if (req->level == REQ_LEVEL_FIRST) {
            switch (req->status) {
            case MK_ST_REQ_METHOD:
                if (buffer[i] == ' ') {
                    mark_end();
                    req->status = MK_ST_REQ_URI;
                    if (req->end < 2) {
                        return -1;
                    }
                    parse_next();
                }
                break;
            case MK_ST_REQ_URI:
                if (buffer[i] == ' ') {
                    mark_end();
                    req->status = MK_ST_REQ_PROT_VERSION;
                    if (field_len() < 1) {
                        return -1;
                    }
                    parse_next();
                }
                else if (buffer[i] == '?') {
                    req->status = MK_ST_REQ_QUERY_STRING;
                    parse_next();
                }
                break;
            case MK_ST_REQ_QUERY_STRING:
                if (buffer[i] == ' ') {
                    mark_end();
                    req->status = MK_ST_REQ_PROT_VERSION;
                    parse_next();
                }
                break;
            case MK_ST_REQ_PROT_VERSION:
                if (buffer[i] == '\r') {
                    mark_end();
                    req->status = MK_ST_LF;
                    continue;
                }
                break;
            case MK_ST_LF:
                if (buffer[i] == '\n') {
                    req->status = MK_ST_FIRST_CONTINUE;
                    continue;
                }
                else {
                    return -1;
                }
                break;
            case MK_ST_FIRST_CONTINUE:
                if (buffer[i] == '\r') {
                    req->status = MK_ST_FIRST_FINALIZE;
                    continue;
                }
                else {
                    req->level = REQ_LEVEL_HEADERS;
                    req->status = MK_ST_HEADER_KEY;
                    i--;
                    parse_next();
                }
                break;
            case MK_ST_FIRST_FINALIZE:
                if (buffer[i] == '\n') {
                    req->level  = REQ_LEVEL_HEADERS;
                    req->status = MK_ST_HEADER_KEY;
                    parse_next();
                }
                else {
                    return -1;
                }
            };
        }
        else if (req->level == REQ_LEVEL_HEADERS) {
            if (req->status == MK_ST_HEADER_KEY) {
                if (buffer[i] == ':') {
                    mark_end();
                    if (field_len() < 1) {
                        return -1;
                    }
                    req->status = MK_ST_HEADER_VALUE;
                    parse_next();
                }
            }
            else if (req->status == MK_ST_HEADER_VALUE) {
                if (buffer[i] == ' ') {
                    continue;
                }
                else {
                    mark_end();
                    req->status = MK_ST_HEADER_END;
                    parse_next();
                }
            }
            else if (req->status == MK_ST_HEADER_END) {
                if (buffer[i] == '\r') {
                    mark_end();
                    continue;
                }
                else if (buffer[i] == '\n') {
                    req->status = MK_ST_HEADER_KEY;
                }
            }
        }
    }

    /* No headers */
    if (req->level == REQ_LEVEL_HEADERS && req->status == MK_ST_HEADER_KEY) {
        return MK_ST_DONE;
    }

    return -1;
}

mk_http_request_t *mk_http_request_new()
{
    mk_http_request_t *req;

    req = malloc(sizeof(mk_http_request_t));
    req->level  = REQ_LEVEL_FIRST;
    req->status = MK_ST_REQ_METHOD;
    req->length = 0;
    req->start  = 0;
    req->end    = 0;

    return req;
}

#ifdef HTTP_STANDALONE


void test(int id, char *buf, int res)
{
    int len;
    int ret;
    int status = TEST_FAIL;
    mk_http_request_t *req = mk_http_request_new();

    len = strlen(buf);
    ret = mk_http_parser(req, buf, len);

    if (res == TEST_OK) {
        if (ret == MK_ST_DONE) {
            status = TEST_OK;
        }
    }
    else if (res == TEST_FAIL) {
        if (ret == -1) {
            status = TEST_OK;
        }
    }

    if (status == TEST_OK) {
        printf("%s[%s%s%s______OK_____%s%s]%s  test %20i",
               ANSI_BOLD, ANSI_RESET, ANSI_BOLD, ANSI_GREEN,
               ANSI_RESET, ANSI_BOLD, ANSI_RESET,
               id);
    }
    else {
        printf("%s[%s%sFAIL%s%s]%s  test='%2i'",
               ANSI_BOLD, ANSI_RESET, ANSI_RED, ANSI_RESET, ANSI_BOLD, ANSI_RESET,
               id);
    }

    int i;
    printf(ANSI_BOLD ANSI_YELLOW "\n                 ");
    for (i = 0; i < 40; i++) {
        printf("-");
    }
    printf(ANSI_RESET "\n\n\n");
}

int main()
{
    char *r1 = "GET / HTTP/1.0\r\n\r\n";
    char *r2 = "GET/HTTP/1.0\r\n\r\n";
    char *r3 = "GET /HTTP/1.0\r\n\r\n";
    char *r4 = "GET / HTTP/1.0\r\r";
    char *r5 = "GET/ HTTP/1.0\r\n\r";
    char *r6 = "     \r\n\r\n";
    char *r7 = "GET / HTTP/1.0\r\n:\r\n\r\n";
    char *r8 = "GET / HTTP/1.0\r\nA: B\r\n\r\n";

    test(1, r1, TEST_OK);
    test(2, r2, TEST_FAIL);
    test(3, r3, TEST_FAIL);
    test(4, r4, TEST_FAIL);
    test(5, r5, TEST_FAIL);
    test(6, r6, TEST_FAIL);
    test(7, r7, TEST_FAIL);
    test(8, r8, TEST_OK);

    printf("\n");
    return 0;
}

#endif
