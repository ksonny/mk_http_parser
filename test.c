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

#include "mk_http_parser.h"

int t_succeed;
int t_failed;

#define TEST(str, status)  test(#str, str, status)

void test(char *id, char *buf, int res)
{
    int len;
    int ret;
    int status = TEST_FAIL;
    mk_http_request_t *req = mk_http_request_new();

    len = strlen(buf);
    ret = mk_http_parser(req, buf, len);

    if (res == MK_HTTP_OK) {
        if (ret == MK_HTTP_OK) {
            status = TEST_OK;
        }
    }
    else if (res == MK_HTTP_PENDING) {
        if (ret == MK_HTTP_PENDING) {
            status = TEST_OK;
        }

    }

    else if (res == MK_HTTP_ERROR) {
        if (ret == -1) {
            status = TEST_OK;
        }
    }

    if (status == TEST_OK) {
        printf("%s[%s%s%s______OK_____%s%s]%s  ",
               ANSI_BOLD, ANSI_RESET, ANSI_BOLD, ANSI_GREEN,
               ANSI_RESET, ANSI_BOLD, ANSI_RESET);
    }
    else {
        printf("%s[%s%s%s____FAIL_____%s%s]%s  ",
               ANSI_BOLD, ANSI_RESET, ANSI_BOLD, ANSI_RED,
               ANSI_RESET, ANSI_BOLD, ANSI_RESET);
    }
    printf("%s[%sTEST %s] [exp %s%s", ANSI_BOLD, ANSI_RESET, id, ANSI_BOLD, ANSI_YELLOW);
    switch (res) {
    case MK_HTTP_OK:
        printf("MK_HTTP_OK");
        break;
    case MK_HTTP_ERROR:
        printf("MK_HTTP_ERROR");
        break;
    case MK_HTTP_PENDING:
        printf("MK_HTTP_PENDING");
        break;
    };

    printf("%s got %s", ANSI_RESET, ANSI_BOLD);

    if (res == ret){
        printf("%s", ANSI_YELLOW);
    }
    else {
        printf("%s", ANSI_RED);
    }
    switch (ret) {
    case MK_HTTP_OK:
        printf("MK_HTTP_OK");
        break;
    case MK_HTTP_ERROR:
        printf("MK_HTTP_ERROR");
        break;
    case MK_HTTP_PENDING:
        printf("MK_HTTP_PENDING");
        break;
    };

    printf("%s]", ANSI_RESET);

    int i;
    printf(ANSI_BOLD ANSI_YELLOW "\n                 *");
    for (i = 0; i < 50; i++) {
        printf("-");
    }
    printf("*" ANSI_RESET "\n\n\n");

    if (status == TEST_OK) {
        t_succeed++;
    }
    else if (status == TEST_FAIL) {
        t_failed++;
    }
}

int main()
{
    /* first line */
    char *r10 = "GET / HTTP/1.0\r\n\r\n";
    char *r11 = "GET/HTTP/1.0\r\n\r\n";
    char *r12 = "GET /HTTP/1.0\r\n\r\n";
    char *r13 = "GET / HTTP/1.0\r\r";
    char *r14 = "GET/ HTTP/1.0\r\n\r";
    char *r15 = "     \r\n\r\n";
    char *r16 = "GET / HTTP/1.0\r";
    char *r17 = "GET / HTTP/1.0\r\n";
    char *r18 = "GET / HTTP/1.0\r\n\r\r";

    TEST(r10, MK_HTTP_OK);
    TEST(r11, MK_HTTP_ERROR);
    TEST(r12, MK_HTTP_PENDING);
    TEST(r13, MK_HTTP_ERROR);
    TEST(r14, MK_HTTP_PENDING);
    TEST(r15, MK_HTTP_ERROR);
    TEST(r16, MK_HTTP_PENDING);
    TEST(r17, MK_HTTP_PENDING);
    TEST(r18, MK_HTTP_ERROR);

    /* headers */
    char *r50 = "GET / HTTP/1.0\r\n:\r\n\r\n";
    char *r51 = "GET / HTTP/1.0\r\nA: B\r\n\r\n";
    char *r52 = "GET / HTTP/1.0\r\nA1: AAAA\r\nA2:   BBBB\r\n\r\n";
    char *r53 = "GET / HTTP/1.0\r\nB1: BBAA\r\nB2:   BBBB   \r\n\r\n";
    char *r54 = "GET / HTTP/1.0\r\nB1:\r\n\r\n";
    char *r55 = "GET / HTTP/1.0\r\nB1:\r\r";
    char *r56 = "GET / HTTP/1.0\r\nB1: BBAA\r\nB2:   BBBB   \r\n\r\r";

    TEST(r50, MK_HTTP_ERROR);
    TEST(r51, MK_HTTP_OK);
    TEST(r52, MK_HTTP_OK);
    TEST(r53, MK_HTTP_OK);
    TEST(r54, MK_HTTP_ERROR);
    TEST(r55, MK_HTTP_ERROR);
    TEST(r56, MK_HTTP_ERROR);

    printf("%s===> Tests Passed:%s %s%s%i/%i%s\n\n",
           ANSI_BOLD, ANSI_RESET,
           ANSI_BOLD,
           (t_failed > 0) ? ANSI_RED: ANSI_GREEN,
           t_succeed, (t_succeed+t_failed), ANSI_RESET);

    return 0;
}
