/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#pragma once

#include <assert.h>
#include <stdio.h>

/* log define */
#ifdef DEBUG
#define dbg(...)                                                                                                       \
    do {                                                                                                               \
        printf(__VA_ARGS__);                                                                                           \
    } while (0)
#else
#define dbg(...)                                                                                                       \
    do {                                                                                                               \
    } while (0)
#endif
#define info(...)                                                                                                      \
    do {                                                                                                               \
        printf(__VA_ARGS__);                                                                                           \
    } while (0)
#define err(...)                                                                                                       \
    do {                                                                                                               \
        printf(__VA_ARGS__);                                                                                           \
    } while (0)
#ifdef DEBUG
#define IMPL_ASSERT(x, y)                                                                                              \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            printf("%s, %s\n", __func__, y);                                                                           \
            assert(x);                                                                                                 \
        }                                                                                                              \
    } while (0)
#else
#define IMPL_ASSERT(x, y)                                                                                              \
    do {                                                                                                               \
    } while (0)
#endif
