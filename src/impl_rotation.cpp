/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include "impl_rotation.hpp"

#include <stdio.h>
#include <stdlib.h>

#include "impl_api.h"
#include "impl_common.hpp"
#include "impl_trace.hpp"

IMPL_STATUS rotation_i420(struct impl_rotation_params *prt, unsigned char *buf_in, unsigned char *buf_out,
                          void *dep_evt) {
    queue q        = *(queue *)(prt->pq);
    int src_width  = prt->src_width;
    int src_height = prt->src_height;
    int dst_width  = prt->dst_width;
    int dst_height = prt->dst_height;
    int angle      = prt->angle;

    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;

    auto event               = q.submit([&](sycl::handler &h) {
        unsigned char *srcu_ptr = buf_in + src_width * src_height;
        unsigned char *srcv_ptr = buf_in + src_width * src_height * 5 / 4;
        unsigned char *dstu_ptr = buf_out + dst_width * dst_height;
        unsigned char *dstv_ptr = buf_out + dst_width * dst_height * 5 / 4;

        try {
            if (dep_evt) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            if (angle == 0) {
                h.parallel_for(sycl::range<1>(dst_height * 2), [=](sycl::id<1> idx) {
                    const int dst_y = idx[0];
                    bool is_uv      = (dst_y >= dst_height);
                    if (is_uv) {
                        bool is_u            = is_uv && (dst_y < (dst_height * 3 / 2));
                        unsigned int uvp_src = src_width >> 1;
                        unsigned int uvp_dst = dst_width >> 1;
                        if (is_u) {
                            unsigned int y0 = dst_y - dst_height;
                            memcpy(dstu_ptr + y0 * uvp_dst, srcu_ptr + y0 * uvp_src, uvp_src);
                        } else {
                            unsigned int y0 = dst_y - (dst_height * 3 / 2);
                            memcpy(dstv_ptr + y0 * uvp_dst, srcv_ptr + y0 * uvp_src, uvp_src);
                        }
                    } else {
                        memcpy(buf_out + dst_y * dst_width, buf_in + dst_y * src_width, src_width);
                    }
                });
            } else if (angle == 90) {
                h.parallel_for(sycl::range<2>(dst_height * 2, dst_width / 2), [=](sycl::id<2> idx) {
                    const int dst_y = idx[0];
                    const int dst_x = idx[1];
                    bool is_uv      = (dst_y >= dst_height);
                    if (is_uv) {
                        bool is_u               = is_uv && (dst_y < (dst_height * 3 / 2));
                        unsigned int dst_y_uv   = dst_y - (is_u ? dst_height : (dst_height * 3 / 2));
                        unsigned char *src0_ptr = is_u ? srcu_ptr : srcv_ptr;
                        unsigned char *dst0_ptr = is_u ? dstu_ptr : dstv_ptr;

                        int src_x = dst_y_uv;
                        int src_y = src_height / 2 - dst_x - 1;
                        dst_write_char(dst0_ptr, dst_y_uv, dst_x, dst_width / 2,
                                                     *(src0_ptr + src_y * (src_width / 2) + src_x));
                    } else {
                        int dst_x_luma_1 = dst_x * 2;
                        int dst_x_luma_2 = dst_x * 2 + 1;

                        int src_x_luma_1 = dst_y;
                        int src_x_luma_2 = dst_y;
                        int src_y_luma_1 = src_height - dst_x_luma_1 - 1;
                        int src_y_luma_2 = src_height - dst_x_luma_2 - 1;

                        dst_write_char(buf_out, dst_y, dst_x_luma_1, dst_width,
                                                     *(buf_in + src_y_luma_1 * src_width + src_x_luma_1));
                        dst_write_char(buf_out, dst_y, dst_x_luma_2, dst_width,
                                                     *(buf_in + src_y_luma_2 * src_width + src_x_luma_2));
                    }
                });
            } else if (angle == 180) {
                h.parallel_for(sycl::range<2>(dst_height * 2, dst_width / 2), [=](sycl::id<2> idx) {
                    const int dst_y = idx[0];
                    const int dst_x = idx[1];
                    bool is_uv      = (dst_y >= dst_height);
                    if (is_uv) {
                        bool is_u               = is_uv && (dst_y < (dst_height * 3 / 2));
                        unsigned int dst_y_uv   = dst_y - (is_u ? dst_height : (dst_height * 3 / 2));
                        unsigned char *src0_ptr = is_u ? srcu_ptr : srcv_ptr;
                        unsigned char *dst0_ptr = is_u ? dstu_ptr : dstv_ptr;

                        int src_x = src_width / 2 - dst_x - 1;
                        int src_y = src_height / 2 - dst_y_uv - 1;
                        dst_write_char(dst0_ptr, dst_y_uv, dst_x, dst_width / 2,
                                                     *(src0_ptr + src_y * (src_width / 2) + src_x));
                    } else {
                        int dst_x_luma_1 = dst_x * 2;
                        int dst_x_luma_2 = dst_x * 2 + 1;

                        int src_x_luma_1 = src_width - dst_x_luma_1 - 1;
                        int src_x_luma_2 = src_width - dst_x_luma_2 - 1;
                        int src_y_luma_1 = src_height - dst_y - 1;
                        int src_y_luma_2 = src_height - dst_y - 1;
                        dst_write_char(buf_out, dst_y, dst_x_luma_1, dst_width,
                                                     *(buf_in + src_y_luma_1 * src_width + src_x_luma_1));
                        dst_write_char(buf_out, dst_y, dst_x_luma_2, dst_width,
                                                     *(buf_in + src_y_luma_2 * src_width + src_x_luma_2));
                    }
                });
            } else if (angle == 270) {
                h.parallel_for(sycl::range<2>(dst_height * 2, dst_width / 2), [=](sycl::id<2> idx) {
                    const int dst_y = idx[0];
                    const int dst_x = idx[1];
                    bool is_uv      = (dst_y >= dst_height);
                    if (is_uv) {
                        bool is_u               = is_uv && (dst_y < (dst_height * 3 / 2));
                        unsigned int dst_y_uv   = dst_y - (is_u ? dst_height : (dst_height * 3 / 2));
                        unsigned char *src0_ptr = is_u ? srcu_ptr : srcv_ptr;
                        unsigned char *dst0_ptr = is_u ? dstu_ptr : dstv_ptr;

                        int src_x = src_width / 2 - dst_y_uv - 1;
                        int src_y = dst_x;
                        dst_write_char(dst0_ptr, dst_y_uv, dst_x, dst_width / 2,
                                                     *(src0_ptr + src_y * (src_width / 2) + src_x));
                    } else {
                        int dst_x_luma_1 = dst_x * 2;
                        int dst_x_luma_2 = dst_x * 2 + 1;

                        int src_x_luma_1 = src_width - dst_y - 1;
                        int src_x_luma_2 = src_width - dst_y - 1;
                        int src_y_luma_1 = dst_x_luma_1;
                        int src_y_luma_2 = dst_x_luma_2;
                        dst_write_char(buf_out, dst_y, dst_x_luma_1, dst_width,
                                                     *(buf_in + src_y_luma_1 * src_width + src_x_luma_1));
                        dst_write_char(buf_out, dst_y, dst_x_luma_2, dst_width,
                                                     *(buf_in + src_y_luma_2 * src_width + src_x_luma_2));
                    }
                });
            }
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });
    *(sycl::event *)prt->evt = event;
    if (prt->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS rotation_v210(struct impl_rotation_params *prt, unsigned char *buf_in_c, unsigned char *buf_out_c,
                          void *dep_evt) {
    queue q        = *(queue *)(prt->pq);
    int src_width  = prt->src_width;
    int src_height = prt->src_height;
    int dst_width  = prt->dst_width;
    int dst_height = prt->dst_height;
    int angle      = prt->angle;

    unsigned int *buf_in  = (unsigned int *)buf_in_c;
    unsigned int *buf_out = (unsigned int *)buf_out_c;

    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;

    auto event               = q.submit([&](sycl::handler &h) {
        try {
            if (dep_evt) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            if (angle == 0) {
                h.parallel_for(sycl::range<1>(dst_height), [=](sycl::id<1> idx) {
                    const auto dst_y = idx[0];
                    memcpy(buf_out + dst_y * dst_width * 2 / 3, buf_in + dst_y * src_width * 2 / 3,
                                         src_width * 2 / 3 * sizeof(unsigned int));
                });
            } else if (angle == 90) {
                int dst_pitch = (dst_width + 47) / 48 * 48;
                int src_pitch = src_width * 2 / 3;

                h.parallel_for(sycl::range<2>(dst_height / 6, dst_width / 6), [=](sycl::id<2> idx) {
                    const auto y_pixel_dst = idx[0] * 6;
                    const auto y_pixel_src = idx[0] * 4;
                    const auto x_pixel_dst = idx[1] * 4;
                    const auto x_pixel_src = idx[1] * 6;

                    // pixel 0
                    const unsigned int pixel0_r0 =
                        src_read_int(buf_in, src_height - x_pixel_src - 1, y_pixel_src, src_pitch);
                    unsigned int lm0 = (((unsigned int)pixel0_r0 >> 10) & 0x3ff);

                    unsigned int cb0_p0 = (((unsigned int)pixel0_r0) & 0x3ff);
                    unsigned int cr0_p0 = (((unsigned int)pixel0_r0 >> 20) & 0x3ff);

                    const unsigned int pixel0_r1 =
                        src_read_int(buf_in, src_height - x_pixel_src - 2, y_pixel_src, src_pitch);
                    unsigned int lm1 = (((unsigned int)pixel0_r1 >> 10) & 0x3ff);

                    const unsigned int pixel0_r2 =
                        src_read_int(buf_in, src_height - x_pixel_src - 3, y_pixel_src, src_pitch);
                    unsigned int lm2 = (((unsigned int)pixel0_r2 >> 10) & 0x3ff);

                    unsigned int cb1_p0 = (((unsigned int)pixel0_r2) & 0x3ff);
                    unsigned int cr1_p0 = (((unsigned int)pixel0_r2 >> 20) & 0x3ff);

                    const unsigned int pixel0_r3 =
                        src_read_int(buf_in, src_height - x_pixel_src - 4, y_pixel_src, src_pitch);
                    unsigned int lm3 = (((unsigned int)pixel0_r3 >> 10) & 0x3ff);

                    const unsigned int pixel0_r4 =
                        src_read_int(buf_in, src_height - x_pixel_src - 5, y_pixel_src, src_pitch);
                    unsigned int lm4 = (((unsigned int)pixel0_r4 >> 10) & 0x3ff);

                    unsigned int cb2_p0 = (((unsigned int)pixel0_r4) & 0x3ff);
                    unsigned int cr2_p0 = (((unsigned int)pixel0_r4 >> 20) & 0x3ff);

                    const unsigned int pixel0_r5 =
                        src_read_int(buf_in, src_height - x_pixel_src - 6, y_pixel_src, src_pitch);
                    unsigned int lm5 = (((unsigned int)pixel0_r5 >> 10) & 0x3ff);

                    unsigned int dst_pixel0 = cb0_p0 | lm0 << 10 | cr0_p0 << 20;
                    dst_write_int(buf_out, y_pixel_dst, x_pixel_dst, dst_pitch * 2 / 3, dst_pixel0);
                    unsigned int dst_pixel1 = lm1 | cb1_p0 << 10 | lm2 << 20;
                    dst_write_int(buf_out, y_pixel_dst, x_pixel_dst + 1, dst_pitch * 2 / 3, dst_pixel1);
                    unsigned int dst_pixel2 = cr1_p0 | lm3 << 10 | cb2_p0 << 20;
                    dst_write_int(buf_out, y_pixel_dst, x_pixel_dst + 2, dst_pitch * 2 / 3, dst_pixel2);
                    unsigned int dst_pixel3 = lm4 | cr2_p0 << 10 | lm5 << 20;
                    dst_write_int(buf_out, y_pixel_dst, x_pixel_dst + 3, dst_pitch * 2 / 3, dst_pixel3);

                    // pixel 1
                    const unsigned int pixel1_r0 =
                        src_read_int(buf_in, src_height - x_pixel_src - 1, y_pixel_src + 1, src_pitch);
                    const unsigned int pixel2_r0 =
                        src_read_int(buf_in, src_height - x_pixel_src - 1, y_pixel_src + 2, src_pitch);
                    lm0                 = (((unsigned int)pixel1_r0) & 0x3ff);
                    unsigned int cb0_p2 = (((unsigned int)pixel1_r0 >> 10) & 0x3ff);
                    unsigned int cr0_p2 = (((unsigned int)pixel2_r0) & 0x3ff);
                    unsigned int cb0_p1 = (cb0_p0 + cb0_p2) / 2;
                    unsigned int cr0_p1 = (cr0_p0 + cr0_p2) / 2;

                    const unsigned int pixel1_r1 =
                        src_read_int(buf_in, src_height - x_pixel_src - 2, y_pixel_src + 1, src_pitch);
                    lm1 = (((unsigned int)pixel1_r1) & 0x3ff);

                    const unsigned int pixel1_r2 =
                        src_read_int(buf_in, src_height - x_pixel_src - 3, y_pixel_src + 1, src_pitch);
                    const unsigned int pixel2_r2 =
                        src_read_int(buf_in, src_height - x_pixel_src - 3, y_pixel_src + 2, src_pitch);
                    lm2                 = (((unsigned int)pixel1_r2) & 0x3ff);
                    unsigned int cb1_p2 = (((unsigned int)pixel1_r2 >> 10) & 0x3ff);
                    unsigned int cr1_p2 = (((unsigned int)pixel2_r2) & 0x3ff);
                    unsigned int cb1_p1 = (cb1_p0 + cb1_p2) / 2;
                    unsigned int cr1_p1 = (cr1_p0 + cr1_p2) / 2;

                    const unsigned int pixel1_r3 =
                        src_read_int(buf_in, src_height - x_pixel_src - 4, y_pixel_src + 1, src_pitch);
                    lm3 = (((unsigned int)pixel1_r3) & 0x3ff);

                    const unsigned int pixel1_r4 =
                        src_read_int(buf_in, src_height - x_pixel_src - 5, y_pixel_src + 1, src_pitch);
                    const unsigned int pixel2_r4 =
                        src_read_int(buf_in, src_height - x_pixel_src - 5, y_pixel_src + 2, src_pitch);
                    unsigned int cb2_p2 = (((unsigned int)pixel1_r4 >> 10) & 0x3ff);
                    unsigned int cr2_p2 = (((unsigned int)pixel2_r4) & 0x3ff);
                    unsigned int cb2_p1 = (cb2_p0 + cb2_p2) / 2;
                    unsigned int cr2_p1 = (cr2_p0 + cr2_p2) / 2;
                    lm4                 = (((unsigned int)pixel1_r4) & 0x3ff);

                    const unsigned int pixel1_r5 =
                        src_read_int(buf_in, src_height - x_pixel_src - 6, y_pixel_src + 1, src_pitch);
                    lm5 = (((unsigned int)pixel1_r5) & 0x3ff);

                    dst_pixel0 = cb0_p1 | lm0 << 10 | cr0_p1 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 1, x_pixel_dst, dst_pitch * 2 / 3, dst_pixel0);
                    dst_pixel1 = lm1 | cb1_p1 << 10 | lm2 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 1, x_pixel_dst + 1, dst_pitch * 2 / 3, dst_pixel1);
                    dst_pixel2 = cr1_p1 | lm3 << 10 | cb2_p1 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 1, x_pixel_dst + 2, dst_pitch * 2 / 3, dst_pixel2);
                    dst_pixel3 = lm4 | cr2_p1 << 10 | lm5 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 1, x_pixel_dst + 3, dst_pitch * 2 / 3, dst_pixel3);

                    // pixel 2
                    lm0 = (((unsigned int)pixel1_r0 >> 20) & 0x3ff);
                    lm1 = (((unsigned int)pixel1_r1 >> 20) & 0x3ff);
                    lm2 = (((unsigned int)pixel1_r2 >> 20) & 0x3ff);
                    lm3 = (((unsigned int)pixel1_r3 >> 20) & 0x3ff);
                    lm4 = (((unsigned int)pixel1_r4 >> 20) & 0x3ff);
                    lm5 = (((unsigned int)pixel1_r5 >> 20) & 0x3ff);

                    dst_pixel0 = cb0_p2 | lm0 << 10 | cr0_p2 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 2, x_pixel_dst, dst_pitch * 2 / 3, dst_pixel0);
                    dst_pixel1 = lm1 | cb1_p2 << 10 | lm2 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 2, x_pixel_dst + 1, dst_pitch * 2 / 3, dst_pixel1);
                    dst_pixel2 = cr1_p2 | lm3 << 10 | cb2_p2 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 2, x_pixel_dst + 2, dst_pitch * 2 / 3, dst_pixel2);
                    dst_pixel3 = lm4 | cr2_p2 << 10 | lm5 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 2, x_pixel_dst + 3, dst_pitch * 2 / 3, dst_pixel3);

                    // pixel 3
                    const unsigned int pixel3_r0 =
                        src_read_int(buf_in, src_height - x_pixel_src - 1, y_pixel_src + 3, src_pitch);
                    lm0                 = (((unsigned int)pixel2_r0 >> 10) & 0x3ff);
                    unsigned int cb0_p4 = (((unsigned int)pixel2_r0 >> 20) & 0x3ff);
                    unsigned int cr0_p4 = (((unsigned int)pixel3_r0 >> 10) & 0x3ff);
                    unsigned int cb0_p3 = (cb0_p2 + cb0_p4) / 2;
                    unsigned int cr0_p3 = (cr0_p2 + cr0_p4) / 2;

                    const unsigned int pixel2_r1 =
                        src_read_int(buf_in, src_height - x_pixel_src - 2, y_pixel_src + 2, src_pitch);
                    lm1 = (((unsigned int)pixel2_r1 >> 10) & 0x3ff);

                    const unsigned int pixel3_r2 =
                        src_read_int(buf_in, src_height - x_pixel_src - 3, y_pixel_src + 3, src_pitch);
                    lm2                 = (((unsigned int)pixel2_r2 >> 10) & 0x3ff);
                    unsigned int cb1_p4 = (((unsigned int)pixel2_r2 >> 20) & 0x3ff);
                    unsigned int cr1_p4 = (((unsigned int)pixel3_r2 >> 10) & 0x3ff);
                    unsigned int cb1_p3 = (cb1_p2 + cb1_p4) / 2;
                    unsigned int cr1_p3 = (cr1_p2 + cr1_p4) / 2;

                    const unsigned int pixel2_r3 =
                        src_read_int(buf_in, src_height - x_pixel_src - 4, y_pixel_src + 2, src_pitch);
                    lm3 = (((unsigned int)pixel2_r3 >> 10) & 0x3ff);

                    const unsigned int pixel3_r4 =
                        src_read_int(buf_in, src_height - x_pixel_src - 5, y_pixel_src + 3, src_pitch);
                    lm4                 = (((unsigned int)pixel2_r4 >> 10) & 0x3ff);
                    unsigned int cb2_p4 = (((unsigned int)pixel2_r4 >> 20) & 0x3ff);
                    unsigned int cr2_p4 = (((unsigned int)pixel3_r4 >> 10) & 0x3ff);
                    // unsigned int cb2_p3 = (cb2_p2 + cb2_p4) / 2;
                    // unsigned int cr2_p3 = (cr2_p2 + cr2_p4) / 2;

                    const unsigned int pixel2_r5 =
                        src_read_int(buf_in, src_height - x_pixel_src - 6, y_pixel_src + 2, src_pitch);
                    lm5 = (((unsigned int)pixel2_r5 >> 10) & 0x3ff);

                    dst_pixel0 = cb0_p3 | lm0 << 10 | cr0_p3 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 3, x_pixel_dst, dst_pitch * 2 / 3, dst_pixel0);
                    dst_pixel1 = lm1 | cb1_p3 << 10 | lm2 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 3, x_pixel_dst + 1, dst_pitch * 2 / 3, dst_pixel1);
                    dst_pixel2 = cr1_p3 | lm3 << 10 | cb2_p4 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 3, x_pixel_dst + 2, dst_pitch * 2 / 3, dst_pixel2);
                    dst_pixel3 = lm4 | cr2_p4 << 10 | lm5 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 3, x_pixel_dst + 3, dst_pitch * 2 / 3, dst_pixel3);

                    // pixel 4
                    lm0 = (((unsigned int)pixel3_r0) & 0x3ff);
                    const unsigned int pixel3_r1 =
                        src_read_int(buf_in, src_height - x_pixel_src - 2, y_pixel_src + 3, src_pitch);
                    lm1 = (((unsigned int)pixel3_r1) & 0x3ff);
                    lm2 = (((unsigned int)pixel3_r2) & 0x3ff);
                    const unsigned int pixel3_r3 =
                        src_read_int(buf_in, src_height - x_pixel_src - 4, y_pixel_src + 3, src_pitch);
                    lm3 = (((unsigned int)pixel3_r3) & 0x3ff);
                    lm4 = (((unsigned int)pixel3_r4) & 0x3ff);
                    const unsigned int pixel3_r5 =
                        src_read_int(buf_in, src_height - x_pixel_src - 6, y_pixel_src + 3, src_pitch);
                    lm5 = (((unsigned int)pixel3_r5) & 0x3ff);

                    dst_pixel0 = cb0_p4 | lm0 << 10 | cr0_p4 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 4, x_pixel_dst, dst_pitch * 2 / 3, dst_pixel0);
                    dst_pixel1 = lm1 | cb1_p4 << 10 | lm2 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 4, x_pixel_dst + 1, dst_pitch * 2 / 3, dst_pixel1);
                    dst_pixel2 = cr1_p4 | lm3 << 10 | cb2_p4 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 4, x_pixel_dst + 2, dst_pitch * 2 / 3, dst_pixel2);
                    dst_pixel3 = lm4 | cr2_p4 << 10 | lm5 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 4, x_pixel_dst + 3, dst_pitch * 2 / 3, dst_pixel3);

                    // pixel 5
                    lm0 = (((unsigned int)pixel3_r0 >> 20) & 0x3ff);
                    lm1 = (((unsigned int)pixel3_r1 >> 20) & 0x3ff);
                    lm2 = (((unsigned int)pixel3_r2 >> 20) & 0x3ff);
                    lm3 = (((unsigned int)pixel3_r3 >> 20) & 0x3ff);
                    lm4 = (((unsigned int)pixel3_r4 >> 20) & 0x3ff);
                    lm5 = (((unsigned int)pixel3_r5 >> 20) & 0x3ff);

                    unsigned int cb0_p5 = (((unsigned int)pixel2_r0 >> 20) & 0x3ff);
                    unsigned int cr0_p5 = (((unsigned int)pixel3_r0 >> 10) & 0x3ff);

                    unsigned int cb1_p5 = (((unsigned int)pixel2_r2 >> 20) & 0x3ff);
                    unsigned int cr1_p5 = (((unsigned int)pixel3_r2 >> 10) & 0x3ff);

                    unsigned int cb2_p5 = (((unsigned int)pixel2_r4 >> 20) & 0x3ff);
                    unsigned int cr2_p5 = (((unsigned int)pixel3_r4 >> 10) & 0x3ff);

                    if ((int)idx[0] != dst_height / 6 - 1) {
                        unsigned int pixel_r0_next =
                            src_read_int(buf_in, src_height - x_pixel_src - 1, y_pixel_src + 4, src_pitch);
                        unsigned int cb0_p0_next = (((unsigned int)pixel_r0_next) & 0x3ff);
                        unsigned int cr0_p0_next = (((unsigned int)pixel_r0_next >> 20) & 0x3ff);
                        cb0_p5                   = (cb0_p4 + cb0_p0_next) / 2;
                        cr0_p5                   = (cr0_p4 + cr0_p0_next) / 2;

                        unsigned int pixel_r2_next =
                            src_read_int(buf_in, src_height - x_pixel_src - 3, y_pixel_src + 4, src_pitch);
                        unsigned int cb1_p0_next = (((unsigned int)pixel_r2_next) & 0x3ff);
                        unsigned int cr1_p0_next = (((unsigned int)pixel_r2_next >> 20) & 0x3ff);
                        cb1_p5                   = (cb1_p4 + cb1_p0_next) / 2;
                        cr1_p5                   = (cr1_p4 + cr1_p0_next) / 2;

                        unsigned int pixel_r4_next =
                            src_read_int(buf_in, src_height - x_pixel_src - 5, y_pixel_src + 4, src_pitch);
                        unsigned int cb2_p0_next = (((unsigned int)pixel_r4_next) & 0x3ff);
                        unsigned int cr2_p0_next = (((unsigned int)pixel_r4_next >> 20) & 0x3ff);
                        cb2_p5                   = (cb2_p4 + cb2_p0_next) / 2;
                        cr2_p5                   = (cr2_p4 + cr2_p0_next) / 2;
                    }

                    dst_pixel0 = cb0_p5 | lm0 << 10 | cr0_p5 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 5, x_pixel_dst, dst_pitch * 2 / 3, dst_pixel0);
                    dst_pixel1 = lm1 | cb1_p5 << 10 | lm2 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 5, x_pixel_dst + 1, dst_pitch * 2 / 3, dst_pixel1);
                    dst_pixel2 = cr1_p5 | lm3 << 10 | cb2_p5 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 5, x_pixel_dst + 2, dst_pitch * 2 / 3, dst_pixel2);
                    dst_pixel3 = lm4 | cr2_p5 << 10 | lm5 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 5, x_pixel_dst + 3, dst_pitch * 2 / 3, dst_pixel3);

                    if ((int)idx[1] == (dst_width / 6 - 1)) {
                        for (int y = 0; y < dst_height; y++) {
                            for (int x = dst_width / 6; x < dst_pitch / 6; x++) {
                                int dst_pitch          = (dst_width + 47) / 48 * 48;
                                const auto x_pixel_dst = x * 4;

                                unsigned int pixel3 =
                                    src_read_int(buf_out, y, dst_width * 2 / 3 - 1, dst_pitch * 2 / 3);
                                unsigned int pixel2 =
                                    src_read_int(buf_out, y, dst_width * 2 / 3 - 2, dst_pitch * 2 / 3);
                                unsigned int y5  = (((unsigned int)pixel3 >> 20) & 0x3ff);
                                unsigned int cr2 = (((unsigned int)pixel3 >> 10) & 0x3ff);
                                unsigned int cb2 = (((unsigned int)pixel2 >> 20) & 0x3ff);

                                unsigned int dst_pixel0 = cb2 | y5 << 10 | cr2 << 20;
                                dst_write_int(buf_out, y, x_pixel_dst, dst_pitch * 2 / 3, dst_pixel0);
                                unsigned int dst_pixel1 = y5 | cb2 << 10 | y5 << 20;
                                dst_write_int(buf_out, y, x_pixel_dst + 1, dst_pitch * 2 / 3, dst_pixel1);
                                unsigned int dst_pixel2 = cr2 | y5 << 10 | cb2 << 20;
                                dst_write_int(buf_out, y, x_pixel_dst + 2, dst_pitch * 2 / 3, dst_pixel2);
                                unsigned int dst_pixel3 = y5 | cr2 << 10 | y5 << 20;
                                dst_write_int(buf_out, y, x_pixel_dst + 3, dst_pitch * 2 / 3, dst_pixel3);
                            }
                        }
                    }
                });
            } else if (angle == 180) {
                h.parallel_for(sycl::range<1>(dst_height * dst_width / 6), [=](sycl::id<1> idx) {
                    const unsigned int pixel3 = *(buf_in + dst_height * dst_width * 2 / 3 - idx[0] * 4 - 1);
                    const unsigned int pixel2 = *(buf_in + dst_height * dst_width * 2 / 3 - idx[0] * 4 - 2);
                    const unsigned int pixel1 = *(buf_in + dst_height * dst_width * 2 / 3 - idx[0] * 4 - 3);
                    const unsigned int pixel0 = *(buf_in + dst_height * dst_width * 2 / 3 - idx[0] * 4 - 4);

                    unsigned int cb0 = (((unsigned int)pixel0) & 0x3ff);
                    unsigned int lm0 = (((unsigned int)pixel0 >> 10) & 0x3ff);
                    unsigned int cr0 = (((unsigned int)pixel0 >> 20) & 0x3ff);
                    unsigned int lm1 = (((unsigned int)pixel1) & 0x3ff);
                    unsigned int cb1 = (((unsigned int)pixel1 >> 10) & 0x3ff);
                    unsigned int lm2 = (((unsigned int)pixel1 >> 20) & 0x3ff);
                    unsigned int cr1 = (((unsigned int)pixel2) & 0x3ff);
                    unsigned int lm3 = (((unsigned int)pixel2 >> 10) & 0x3ff);
                    unsigned int cb2 = (((unsigned int)pixel2 >> 20) & 0x3ff);
                    unsigned int lm4 = (((unsigned int)pixel3) & 0x3ff);
                    unsigned int cr2 = (((unsigned int)pixel3 >> 10) & 0x3ff);
                    unsigned int lm5 = (((unsigned int)pixel3 >> 20) & 0x3ff);

                    unsigned int dst_pixel0     = cb2 | lm5 << 10 | cr2 << 20;
                    *(buf_out + idx[0] * 4)     = dst_pixel0;
                    unsigned int dst_pixel1     = lm4 | cb1 << 10 | lm3 << 20;
                    *(buf_out + idx[0] * 4 + 1) = dst_pixel1;
                    unsigned int dst_pixel2     = cr1 | lm2 << 10 | cb0 << 20;
                    *(buf_out + idx[0] * 4 + 2) = dst_pixel2;
                    unsigned int dst_pixel3     = lm1 | cr0 << 10 | lm0 << 20;
                    *(buf_out + idx[0] * 4 + 3) = dst_pixel3;
                });
            } else if (angle == 270) {
                int dst_pitch = (dst_width + 47) / 48 * 48;
                int src_pitch = src_width * 2 / 3;

                h.parallel_for(sycl::range<2>(dst_height / 6, dst_width / 6), [=](sycl::id<2> idx) {
                    const auto y_pixel_dst = idx[0] * 6;
                    const auto y_pixel_src = idx[0] * 4;
                    const auto x_pixel_dst = idx[1] * 4;
                    const auto x_pixel_src = idx[1] * 6;

                    // pixel 0
                    const unsigned int pixel3_r0 =
                        src_read_int(buf_in, x_pixel_src, src_pitch - y_pixel_src - 1, src_pitch);
                    const unsigned int pixel2_r0 =
                        src_read_int(buf_in, x_pixel_src, src_pitch - y_pixel_src - 2, src_pitch);

                    unsigned int lm0 = (((unsigned int)pixel3_r0 >> 20) & 0x3ff);

                    unsigned int cb0_p0 = (((unsigned int)pixel2_r0 >> 20) & 0x3ff);
                    unsigned int cr0_p0 = (((unsigned int)pixel3_r0 >> 10) & 0x3ff);

                    const unsigned int pixel3_r1 =
                        src_read_int(buf_in, x_pixel_src + 1, src_pitch - y_pixel_src - 1, src_pitch);
                    unsigned int lm1 = (((unsigned int)pixel3_r1 >> 20) & 0x3ff);

                    const unsigned int pixel3_r2 =
                        src_read_int(buf_in, x_pixel_src + 2, src_pitch - y_pixel_src - 1, src_pitch);
                    const unsigned int pixel2_r2 =
                        src_read_int(buf_in, x_pixel_src + 2, src_pitch - y_pixel_src - 2, src_pitch);
                    unsigned int lm2 = (((unsigned int)pixel3_r2 >> 20) & 0x3ff);

                    unsigned int cb1_p0 = (((unsigned int)pixel2_r2 >> 20) & 0x3ff);
                    unsigned int cr1_p0 = (((unsigned int)pixel3_r2 >> 10) & 0x3ff);

                    unsigned int lm3 = (((unsigned int)pixel3_r2 >> 20) & 0x3ff);

                    const unsigned int pixel3_r3 =
                        src_read_int(buf_in, x_pixel_src + 3, src_pitch - y_pixel_src - 1, src_pitch);

                    const unsigned int pixel3_r4 =
                        src_read_int(buf_in, x_pixel_src + 4, src_pitch - y_pixel_src - 1, src_pitch);
                    const unsigned int pixel2_r4 =
                        src_read_int(buf_in, x_pixel_src + 4, src_pitch - y_pixel_src - 2, src_pitch);
                    unsigned int lm4 = (((unsigned int)pixel3_r4 >> 20) & 0x3ff);

                    unsigned int cb2_p0 = (((unsigned int)pixel2_r4 >> 20) & 0x3ff);
                    unsigned int cr2_p0 = (((unsigned int)pixel3_r4 >> 10) & 0x3ff);

                    const unsigned int pixel3_r5 =
                        src_read_int(buf_in, x_pixel_src + 5, src_pitch - y_pixel_src - 1, src_pitch);
                    unsigned int lm5 = (((unsigned int)pixel3_r5 >> 20) & 0x3ff);

                    unsigned int dst_pixel0 = cb0_p0 | lm0 << 10 | cr0_p0 << 20;
                    dst_write_int(buf_out, y_pixel_dst, x_pixel_dst, dst_pitch * 2 / 3, dst_pixel0);
                    unsigned int dst_pixel1 = lm1 | cb1_p0 << 10 | lm2 << 20;
                    dst_write_int(buf_out, y_pixel_dst, x_pixel_dst + 1, dst_pitch * 2 / 3, dst_pixel1);
                    unsigned int dst_pixel2 = cr1_p0 | lm3 << 10 | cb2_p0 << 20;
                    dst_write_int(buf_out, y_pixel_dst, x_pixel_dst + 2, dst_pitch * 2 / 3, dst_pixel2);
                    unsigned int dst_pixel3 = lm4 | cr2_p0 << 10 | lm5 << 20;
                    dst_write_int(buf_out, y_pixel_dst, x_pixel_dst + 3, dst_pitch * 2 / 3, dst_pixel3);

                    // pixel 1
                    lm0 = (((unsigned int)pixel3_r0) & 0x3ff);

                    const unsigned int pixel1_r0 =
                        src_read_int(buf_in, x_pixel_src, src_pitch - y_pixel_src - 3, src_pitch);
                    unsigned int cb0_p2 = (((unsigned int)pixel1_r0 >> 10) & 0x3ff);
                    unsigned int cr0_p2 = (((unsigned int)pixel2_r0) & 0x3ff);
                    unsigned int cb0_p1 = (cb0_p0 + cb0_p2) / 2;
                    unsigned int cr0_p1 = (cr0_p0 + cr0_p2) / 2;

                    lm1 = (((unsigned int)pixel3_r1) & 0x3ff);

                    lm2 = (((unsigned int)pixel3_r2) & 0x3ff);

                    const unsigned int pixel1_r2 =
                        src_read_int(buf_in, x_pixel_src + 2, src_pitch - y_pixel_src - 3, src_pitch);
                    unsigned int cb1_p2 = (((unsigned int)pixel1_r2 >> 10) & 0x3ff);
                    unsigned int cr1_p2 = (((unsigned int)pixel2_r2) & 0x3ff);
                    unsigned int cb1_p1 = (cb1_p0 + cb1_p2) / 2;
                    unsigned int cr1_p1 = (cr1_p0 + cr1_p2) / 2;

                    lm3 = (((unsigned int)pixel3_r3) & 0x3ff);

                    lm4 = (((unsigned int)pixel3_r4) & 0x3ff);
                    const unsigned int pixel1_r4 =
                        src_read_int(buf_in, x_pixel_src + 4, src_pitch - y_pixel_src - 3, src_pitch);
                    unsigned int cb2_p2 = (((unsigned int)pixel1_r4 >> 10) & 0x3ff);
                    unsigned int cr2_p2 = (((unsigned int)pixel2_r4) & 0x3ff);
                    unsigned int cb2_p1 = (cb2_p0 + cb2_p2) / 2;
                    unsigned int cr2_p1 = (cr2_p0 + cr2_p2) / 2;

                    lm5 = (((unsigned int)pixel3_r5) & 0x3ff);

                    dst_pixel0 = cb0_p1 | lm0 << 10 | cr0_p1 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 1, x_pixel_dst, dst_pitch * 2 / 3, dst_pixel0);
                    dst_pixel1 = lm1 | cb1_p1 << 10 | lm2 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 1, x_pixel_dst + 1, dst_pitch * 2 / 3, dst_pixel1);
                    dst_pixel2 = cr1_p1 | lm3 << 10 | cb2_p1 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 1, x_pixel_dst + 2, dst_pitch * 2 / 3, dst_pixel2);
                    dst_pixel3 = lm4 | cr2_p1 << 10 | lm5 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 1, x_pixel_dst + 3, dst_pitch * 2 / 3, dst_pixel3);

                    // pixel 2
                    lm0 = (((unsigned int)pixel2_r0 >> 10) & 0x3ff);
                    const unsigned int pixel2_r1 =
                        src_read_int(buf_in, x_pixel_src + 1, src_pitch - y_pixel_src - 2, src_pitch);
                    lm1 = (((unsigned int)pixel2_r1 >> 10) & 0x3ff);
                    lm2 = (((unsigned int)pixel2_r2 >> 10) & 0x3ff);
                    const unsigned int pixel2_r3 =
                        src_read_int(buf_in, x_pixel_src + 3, src_pitch - y_pixel_src - 2, src_pitch);
                    lm3 = (((unsigned int)pixel2_r3 >> 10) & 0x3ff);
                    lm4 = (((unsigned int)pixel2_r4 >> 10) & 0x3ff);
                    const unsigned int pixel2_r5 =
                        src_read_int(buf_in, x_pixel_src + 5, src_pitch - y_pixel_src - 2, src_pitch);
                    lm5 = (((unsigned int)pixel2_r5 >> 10) & 0x3ff);

                    dst_pixel0 = cb0_p2 | lm0 << 10 | cr0_p2 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 2, x_pixel_dst, dst_pitch * 2 / 3, dst_pixel0);
                    dst_pixel1 = lm1 | cb1_p2 << 10 | lm2 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 2, x_pixel_dst + 1, dst_pitch * 2 / 3, dst_pixel1);
                    dst_pixel2 = cr1_p2 | lm3 << 10 | cb2_p2 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 2, x_pixel_dst + 2, dst_pitch * 2 / 3, dst_pixel2);
                    dst_pixel3 = lm4 | cr2_p2 << 10 | lm5 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 2, x_pixel_dst + 3, dst_pitch * 2 / 3, dst_pixel3);

                    // pixel 3
                    const unsigned int pixel0_r0 =
                        src_read_int(buf_in, x_pixel_src, src_pitch - y_pixel_src - 4, src_pitch);
                    lm0                 = (((unsigned int)pixel1_r0 >> 20) & 0x3ff);
                    unsigned int cb0_p4 = (((unsigned int)pixel0_r0) & 0x3ff);
                    unsigned int cr0_p4 = (((unsigned int)pixel0_r0 >> 20) & 0x3ff);
                    unsigned int cb0_p3 = (cb0_p2 + cb0_p4) / 2;
                    unsigned int cr0_p3 = (cr0_p2 + cr0_p4) / 2;
                    const unsigned int pixel1_r1 =
                        src_read_int(buf_in, x_pixel_src + 1, src_pitch - y_pixel_src - 3, src_pitch);
                    lm1 = (((unsigned int)pixel1_r1 >> 20) & 0x3ff);
                    const unsigned int pixel0_r2 =
                        src_read_int(buf_in, x_pixel_src + 2, src_pitch - y_pixel_src - 4, src_pitch);
                    lm2                 = (((unsigned int)pixel1_r2 >> 20) & 0x3ff);
                    unsigned int cb1_p4 = (((unsigned int)pixel0_r2) & 0x3ff);
                    unsigned int cr1_p4 = (((unsigned int)pixel0_r2 >> 20) & 0x3ff);
                    unsigned int cb1_p3 = (cb1_p2 + cb1_p4) / 2;
                    unsigned int cr1_p3 = (cr1_p2 + cr1_p4) / 2;
                    const unsigned int pixel1_r3 =
                        src_read_int(buf_in, x_pixel_src + 3, src_pitch - y_pixel_src - 3, src_pitch);
                    lm3 = (((unsigned int)pixel1_r3 >> 20) & 0x3ff);
                    const unsigned int pixel0_r4 =
                        src_read_int(buf_in, x_pixel_src + 4, src_pitch - y_pixel_src - 4, src_pitch);
                    lm4                 = (((unsigned int)pixel1_r4 >> 20) & 0x3ff);
                    unsigned int cb2_p4 = (((unsigned int)pixel0_r4) & 0x3ff);
                    unsigned int cr2_p4 = (((unsigned int)pixel0_r4 >> 20) & 0x3ff);
                    unsigned int cb2_p3 = (cb2_p2 + cb2_p4) / 2;
                    unsigned int cr2_p3 = (cr2_p2 + cr2_p4) / 2;

                    const unsigned int pixel1_r5 =
                        src_read_int(buf_in, x_pixel_src + 5, src_pitch - y_pixel_src - 3, src_pitch);
                    lm5 = (((unsigned int)pixel1_r5 >> 20) & 0x3ff);

                    dst_pixel0 = cb0_p3 | lm0 << 10 | cr0_p3 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 3, x_pixel_dst, dst_pitch * 2 / 3, dst_pixel0);
                    dst_pixel1 = lm1 | cb1_p3 << 10 | lm2 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 3, x_pixel_dst + 1, dst_pitch * 2 / 3, dst_pixel1);
                    dst_pixel2 = cr1_p3 | lm3 << 10 | cb2_p3 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 3, x_pixel_dst + 2, dst_pitch * 2 / 3, dst_pixel2);
                    dst_pixel3 = lm4 | cr2_p3 << 10 | lm5 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 3, x_pixel_dst + 3, dst_pitch * 2 / 3, dst_pixel3);

                    // pixel 4
                    lm0 = (((unsigned int)pixel1_r0) & 0x3ff);
                    lm1 = (((unsigned int)pixel1_r1) & 0x3ff);
                    lm2 = (((unsigned int)pixel1_r2) & 0x3ff);
                    lm3 = (((unsigned int)pixel1_r3) & 0x3ff);
                    lm4 = (((unsigned int)pixel1_r4) & 0x3ff);
                    lm5 = (((unsigned int)pixel1_r5) & 0x3ff);

                    dst_pixel0 = cb0_p4 | lm0 << 10 | cr0_p4 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 4, x_pixel_dst, dst_pitch * 2 / 3, dst_pixel0);
                    dst_pixel1 = lm1 | cb1_p4 << 10 | lm2 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 4, x_pixel_dst + 1, dst_pitch * 2 / 3, dst_pixel1);
                    dst_pixel2 = cr1_p4 | lm3 << 10 | cb2_p4 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 4, x_pixel_dst + 2, dst_pitch * 2 / 3, dst_pixel2);
                    dst_pixel3 = lm4 | cr2_p4 << 10 | lm5 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 4, x_pixel_dst + 3, dst_pitch * 2 / 3, dst_pixel3);

                    // pixel 5
                    lm0 = (((unsigned int)pixel0_r0 >> 10) & 0x3ff);
                    const unsigned int pixel0_r1 =
                        src_read_int(buf_in, x_pixel_src + 1, src_pitch - y_pixel_src - 4, src_pitch);
                    lm1 = (((unsigned int)pixel0_r1 >> 10) & 0x3ff);
                    lm2 = (((unsigned int)pixel0_r2 >> 10) & 0x3ff);
                    const unsigned int pixel0_r3 =
                        src_read_int(buf_in, x_pixel_src + 3, src_pitch - y_pixel_src - 4, src_pitch);
                    lm3 = (((unsigned int)pixel0_r3 >> 10) & 0x3ff);
                    lm4 = (((unsigned int)pixel0_r4 >> 10) & 0x3ff);
                    const unsigned int pixel0_r5 =
                        src_read_int(buf_in, x_pixel_src + 5, src_pitch - y_pixel_src - 4, src_pitch);
                    lm5 = (((unsigned int)pixel0_r5 >> 10) & 0x3ff);

                    unsigned int cb0_p5 = cb0_p4;
                    unsigned int cr0_p5 = cr0_p4;

                    unsigned int cb1_p5 = cb1_p4;
                    unsigned int cr1_p5 = cr1_p4;

                    unsigned int cb2_p5 = cb2_p4;
                    unsigned int cr2_p5 = cr2_p4;

                    if ((int)idx[0] != dst_height / 6 - 1) {
                        unsigned int pixel3_r0_next =
                            src_read_int(buf_in, x_pixel_src, src_pitch - y_pixel_src - 5, src_pitch);
                        unsigned int pixel2_r0_next =
                            src_read_int(buf_in, x_pixel_src, src_pitch - y_pixel_src - 6, src_pitch);
                        unsigned int cb0_p0_next = (((unsigned int)pixel2_r0_next >> 20) & 0x3ff);
                        unsigned int cr0_p0_next = (((unsigned int)pixel3_r0_next >> 10) & 0x3ff);
                        cb0_p5                   = (cb0_p4 + cb0_p0_next) / 2;
                        cr0_p5                   = (cr0_p4 + cr0_p0_next) / 2;

                        unsigned int pixel3_r2_next =
                            src_read_int(buf_in, x_pixel_src + 2, src_pitch - y_pixel_src - 5, src_pitch);
                        unsigned int pixel2_r2_next =
                            src_read_int(buf_in, x_pixel_src + 2, src_pitch - y_pixel_src - 6, src_pitch);
                        unsigned int cb1_p0_next = (((unsigned int)pixel2_r2_next >> 20) & 0x3ff);
                        unsigned int cr1_p0_next = (((unsigned int)pixel3_r2_next >> 10) & 0x3ff);
                        cb1_p5                   = (cb1_p4 + cb1_p0_next) / 2;
                        cr1_p5                   = (cr1_p4 + cr1_p0_next) / 2;

                        unsigned int pixel3_r4_next =
                            src_read_int(buf_in, x_pixel_src + 4, src_pitch - y_pixel_src - 5, src_pitch);
                        unsigned int pixel2_r4_next =
                            src_read_int(buf_in, x_pixel_src + 4, src_pitch - y_pixel_src - 6, src_pitch);
                        unsigned int cb2_p0_next = (((unsigned int)pixel2_r4_next >> 20) & 0x3ff);
                        unsigned int cr2_p0_next = (((unsigned int)pixel3_r4_next >> 10) & 0x3ff);
                        cb2_p5                   = (cb2_p4 + cb2_p0_next) / 2;
                        cr2_p5                   = (cr2_p4 + cr2_p0_next) / 2;
                    }

                    dst_pixel0 = cb0_p5 | lm0 << 10 | cr0_p5 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 5, x_pixel_dst, dst_pitch * 2 / 3, dst_pixel0);
                    dst_pixel1 = lm1 | cb1_p5 << 10 | lm2 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 5, x_pixel_dst + 1, dst_pitch * 2 / 3, dst_pixel1);
                    dst_pixel2 = cr1_p5 | lm3 << 10 | cb2_p5 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 5, x_pixel_dst + 2, dst_pitch * 2 / 3, dst_pixel2);
                    dst_pixel3 = lm4 | cr2_p5 << 10 | lm5 << 20;
                    dst_write_int(buf_out, y_pixel_dst + 5, x_pixel_dst + 3, dst_pitch * 2 / 3, dst_pixel3);

                    for (int y = 0; y < dst_height; y++) {
                        int dst_pitch = (dst_width + 47) / 48 * 48;
                        for (int x = dst_width / 6; x < dst_pitch / 6; x++) {
                            const auto x_pixel_dst = x * 4;

                            unsigned int pixel3 = src_read_int(buf_out, y, dst_width * 2 / 3 - 1, dst_pitch * 2 / 3);
                            unsigned int pixel2 = src_read_int(buf_out, y, dst_width * 2 / 3 - 2, dst_pitch * 2 / 3);
                            unsigned int y5     = (((unsigned int)pixel3 >> 20) & 0x3ff);
                            unsigned int cr2    = (((unsigned int)pixel3 >> 10) & 0x3ff);
                            unsigned int cb2    = (((unsigned int)pixel2 >> 20) & 0x3ff);

                            unsigned int dst_pixel0 = cb2 | y5 << 10 | cr2 << 20;
                            dst_write_int(buf_out, y, x_pixel_dst, dst_pitch * 2 / 3, dst_pixel0);
                            unsigned int dst_pixel1 = y5 | cb2 << 10 | y5 << 20;
                            dst_write_int(buf_out, y, x_pixel_dst + 1, dst_pitch * 2 / 3, dst_pixel1);
                            unsigned int dst_pixel2 = cr2 | y5 << 10 | cb2 << 20;
                            dst_write_int(buf_out, y, x_pixel_dst + 2, dst_pitch * 2 / 3, dst_pixel2);
                            unsigned int dst_pixel3 = y5 | cr2 << 10 | y5 << 20;
                            dst_write_int(buf_out, y, x_pixel_dst + 3, dst_pitch * 2 / 3, dst_pixel3);
                        }
                    }
                });
            }
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });
    *(sycl::event *)prt->evt = event;
    if (prt->is_async == 0) {
        event.wait();
    }
    return ret;
}

enum RT_function_index { rotation_i420_index, rotation_v210_index, rotation_max_index };
typedef IMPL_STATUS (*Rt_Function)(struct impl_rotation_params *prt, unsigned char *buf_in, unsigned char *buf_out,
                                   void *dep_evt);
Rt_Function rotationfunction[2] = {rotation_i420, rotation_v210};

IMPL_STATUS impl_rotation_init(struct impl_rotation_params *prt, void *&prt_context) {
    IMPL_ASSERT(prt != NULL, "rotation init failed, prt is null");
    IMPL_ASSERT(prt->pq != NULL, "rotation init failed, pq is null");
    if (!(0 == prt->angle || 90 == prt->angle || 180 == prt->angle || 270 == prt->angle)) {
        err("%s, Illegal angle: must be 0 90 180 270\n", __func__);
        return IMPL_STATUS_INVALID_PARAMS;
    }

    prt->evt                        = impl_common_new_event();
    impl_rotation_context *pcontext = new impl_rotation_context;
    memset(pcontext, 0, sizeof(impl_rotation_context));
    prt_context = (void *)pcontext;
    if (IMPL_VIDEO_I420 == prt->format) {
        pcontext->rotation_func_index = rotation_i420_index;
    } else if (IMPL_VIDEO_V210 == prt->format) {
        pcontext->rotation_func_index = rotation_v210_index;
    } else {
        err("%s, unsupported format, rotation only supports i420 and v210\n", __func__);
        return IMPL_STATUS_FAIL;
    }
    if ((prt->src_width % 2) != 0 || (prt->src_height % 2) != 0 || (prt->dst_width % 2) != 0 ||
        (prt->dst_height % 2) != 0) {
        err("%s, Illegal size: The src/dstwidth and height must be a multiple of 2\n", __func__);
        return IMPL_STATUS_INVALID_PARAMS;
    }
    if (IMPL_VIDEO_V210 == prt->format && (prt->src_width % 48) != 0 && (prt->dst_width % 48) != 0) {
        err("%s, Illegal V210 size: The src/width width=%d/%d must be a multiple of 48\n", __func__, prt->src_width,
            prt->dst_width);
        return IMPL_STATUS_INVALID_PARAMS;
    }

    return IMPL_STATUS_SUCCESS;
}

IMPL_STATUS impl_rotation_run(struct impl_rotation_params *prt, void *prt_context, unsigned char *buf_in,
                              unsigned char *buf_out, void *dep_evt) {
    IMPL_ASSERT(prt != NULL, "rotation run failed, prt is null");
    IMPL_ASSERT(prt_context != NULL, "rotation run failed, prt_context is null");

    IMPL_STATUS ret                        = IMPL_STATUS_SUCCESS;
    struct impl_rotation_context *pcontext = (struct impl_rotation_context *)prt_context;
    ret = rotationfunction[pcontext->rotation_func_index](prt, buf_in, buf_out, dep_evt);

    return ret;
}

void impl_rotation_uninit(struct impl_rotation_params *prt, void *prt_context) {
    IMPL_ASSERT(prt != NULL, "rotation uninit failed, prt is null");
    IMPL_ASSERT(prt_context != NULL, "rotation uninit failed, prt_context is null");

    struct impl_rotation_context *pcontext = (struct impl_rotation_context *)prt_context;
    impl_common_free_event(prt->evt);

    delete pcontext;
}
