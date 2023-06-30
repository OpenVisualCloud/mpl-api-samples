/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef __IMPL_ROTATION_HPP__
#define __IMPL_ROTATION_HPP__
#include "impl_api.h"
#include "impl_common.hpp"

struct impl_rotation_context {
    /** IMPL Rotation function index */
    int rotation_func_index;
};

IMPL_STATUS rotation_i420(struct impl_rotation_params *prt, unsigned char *buf_in, unsigned char *buf_out,
                          void *dep_evt);
IMPL_STATUS rotation_v210(struct impl_rotation_params *prt, unsigned char *buf_in, unsigned char *buf_out,
                          void *dep_evt);

#endif // __IMPL_ROTATION_HPP__
