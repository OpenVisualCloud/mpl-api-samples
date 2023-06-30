/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include <CL/sycl.hpp>

#include "impl_api.h"

#ifdef __SYCL_DEVICE_ONLY__
#define CONSTANT __attribute__((opencl_constant))
#else
#define CONSTANT
#endif

using namespace sycl;

#define src_read_char(src_ptr, srcy, srcx, width) (unsigned char)(src_ptr[(srcy)*width + srcx])
#define src_read_short(src_ptr, srcy, srcx, width) (unsigned short)(src_ptr[(srcy)*width + srcx])
#define src_read_int(src_ptr, srcy, srcx, width) (unsigned int)(src_ptr[(srcy)*width + srcx])
#define src_read_float(src_ptr, srcy, srcx, width) (float)(src_ptr[(srcy)*width + srcx])
#define src_read_rshift6_float(src_ptr, srcy, srcx, width) (float)(src_ptr[(srcy)*width + srcx] >> 6)
#define src_read_rshift6_int(src_ptr, srcy, srcx, width) (int)(src_ptr[(srcy)*width + srcx] >> 6)

#define dst_write_int(dst_ptr, dst_y, dst_x, width, value) dst_ptr[(dst_y)*width + dst_x] = (unsigned int)(value)
#define dst_write_short(dst_ptr, dst_y, dst_x, width, value) dst_ptr[(dst_y)*width + dst_x] = (unsigned short)(value)
#define dst_write_char(dst_ptr, dst_y, dst_x, width, value) dst_ptr[(dst_y)*width + dst_x] = (unsigned char)(value)
#define dst_write_floattochar(dst_ptr, dst_y, dst_x, width, value)                                                     \
    dst_ptr[(dst_y)*width + dst_x] = (unsigned char)(value + (float)0.5)
#define dst_write_floattoshort(dst_ptr, dst_y, dst_x, width, value)                                                    \
    dst_ptr[(dst_y)*width + dst_x] = (unsigned short)(value + (float)0.5)
#define dst_write_lshift6_floattoshort(dst_ptr, dst_y, dst_x, width, value)                                            \
    dst_ptr[(dst_y)*width + dst_x] = (((unsigned short)(value + (float)0.5)) << 6)
#define dst_write_lshift6_short(dst_ptr, dst_y, dst_x, width, value)                                                   \
    dst_ptr[(dst_y)*width + dst_x] = (((unsigned short)value) << 6)
