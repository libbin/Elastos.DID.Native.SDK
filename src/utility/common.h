/*
 * Copyright (c) 2019 - 2021 Elastos Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <jansson.h>

#include "diderror.h"

#if defined(_WIN32) || defined(_WIN64)
#include <crystal.h>
#include <string.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define DOC_BUFFER_LEN                     512
#define MAX_ID_LEN                         64

#define CHECK(func)                    do { if (func < 0) return -1; } while(0)
#define CHECK_TO_ERROREXIT(func)       do { if (func < 0) goto errorExit;} while(0)
#define CHECK_TO_MSG(func, code, msg)              do {   \
    if (func < 0) {                                       \
        DIDError_Set(code, msg);                          \
        return -1;                                        \
    }                                                     \
} while(0)

#define CHECK_TO_MSG_ERROREXIT(func, code, msg)    do {   \
    if (func < 0) {                                       \
        DIDError_Set(code, msg);                          \
        goto errorExit;                                   \
    }                                                     \
} while(0)

#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))

#if defined(_WIN32) || defined(_WIN64)
    static const char *PATH_SEP = "\\";
    #define strtok_r         strtok_s
#else
    static const char *PATH_SEP = "/";
#endif

const char *get_time_string(char *timestring, size_t len, time_t *p_time);

int parse_time(time_t *time, const char *string);

int test_path(const char *path);

int list_dir(const char *path, const char *pattern,
        int (*callback)(const char *name, void *context), void *context);

void delete_file(const char *path);

int get_dir(char* path, bool create, int count, ...);

int get_file(char *path, bool create, int count, ...);

int store_file(const char *path, const char *string);

const char *load_file(const char *path);

bool is_empty(const char *path);

int mkdirs(const char *path, mode_t mode);

int md5_hex(char *id, size_t size, uint8_t *data, size_t datasize);

int md5_hexfrombase58(char *id, size_t size, const char *base58);

char *last_strstr(const char *haystack, const char *needle);

const char *json_astext(json_t *item);

#ifdef __cplusplus
}
#endif

#endif //__COMMON_H__
