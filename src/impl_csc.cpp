/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include "impl_csc.hpp"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "impl_api.h"
#include "impl_trace.hpp"

IMPL_STATUS impl_csc_nv12_to_yuv422ycbcr10be(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pq is null");
    IMPL_ASSERT(buf_in != NULL, "buf_in is null");
    IMPL_ASSERT(buf_dst != NULL, "buf_out is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height / 2, width / 2), [=](sycl::id<2> idx) {
                const auto y_pixel    = idx[0] * 2;
                const auto x_pixel    = idx[1] * 2;
                const auto y_src_cbcr = idx[0] + height;

                // Load Cb and Cr pixels and use them for whole 2x2 square
                const auto src_cb = src_read_char(buf_in, y_src_cbcr, x_pixel, width);
                const auto src_cr = src_read_char(buf_in, y_src_cbcr, x_pixel + 1, width);

                const auto luma0 = src_read_char(buf_in, y_pixel, x_pixel, width);
                const auto luma1 = src_read_char(buf_in, y_pixel, x_pixel + 1, width);
                const auto luma2 = src_read_char(buf_in, y_pixel + 1, x_pixel, width);
                const auto luma3 = src_read_char(buf_in, y_pixel + 1, x_pixel + 1, width);

                unsigned long t =
                    ((((unsigned long)src_cb) << 2) & 0x3ff) | (((((unsigned long)luma0) << 2) & 0x3ff) << 10) |
                    (((((unsigned long)src_cr) << 2) & 0x3ff) << 20) | (((((unsigned long)luma1) << 2) & 0x3ff) << 30);
                unsigned long p    = 0;
                unsigned char *in  = (unsigned char *)&t;
                unsigned char *out = (unsigned char *)&p;
                *(out + 0)         = ((*(in + 0) & 0xff) >> 2) | ((*(in + 1) & 0x3) << 6);
                *(out + 1)         = ((*(in + 0) & 0x3) << 6) | ((*(in + 1) & 0xc0) >> 6) | ((*(in + 2) & 0xf) << 2);
                *(out + 2)         = ((*(in + 1) & 0x3c) << 2) | ((*(in + 3) & 0x3f) >> 2);
                *(out + 3)         = ((*(in + 2) & 0xf0) >> 2) | ((*(in + 3) & 0x3) << 6) | ((*(in + 4) & 0xc0) >> 6);
                *(out + 4)         = ((*(in + 3) & 0xc0) >> 6) | ((*(in + 4) & 0x3f) << 2);

                auto y_dst  = idx[0] * 2;
                auto x0_dst = idx[1] * 5;
                auto x1_dst = x0_dst + 1;
                auto x2_dst = x0_dst + 2;
                auto x3_dst = x0_dst + 3;
                auto x4_dst = x0_dst + 4;

                dst_write_char(buf_dst, y_dst, x0_dst, width * 5 / 2, (p & 0xff));
                dst_write_char(buf_dst, y_dst, x1_dst, width * 5 / 2, ((p >> 8) & 0xff));
                dst_write_char(buf_dst, y_dst, x2_dst, width * 5 / 2, ((p >> 16) & 0xff));
                dst_write_char(buf_dst, y_dst, x3_dst, width * 5 / 2, ((p >> 24) & 0xff));
                dst_write_char(buf_dst, y_dst, x4_dst, width * 5 / 2, ((p >> 32) & 0xff));

                unsigned long t_1 =
                    ((((unsigned long)src_cb) << 2) & 0x3ff) | (((((unsigned long)luma2) << 2) & 0x3ff) << 10) |
                    (((((unsigned long)src_cr) << 2) & 0x3ff) << 20) | (((((unsigned long)luma3) << 2) & 0x3ff) << 30);

                unsigned long p_1    = 0;
                unsigned char *in_1  = (unsigned char *)&t_1;
                unsigned char *out_1 = (unsigned char *)&p_1;
                *(out_1 + 0)         = ((*(in_1 + 0) & 0xff) >> 2) | ((*(in_1 + 1) & 0x3) << 6);
                *(out_1 + 1) = ((*(in_1 + 0) & 0x3) << 6) | ((*(in_1 + 1) & 0xc0) >> 6) | ((*(in_1 + 2) & 0xf) << 2);
                *(out_1 + 2) = ((*(in_1 + 1) & 0x3c) << 2) | ((*(in_1 + 3) & 0x3f) >> 2);
                *(out_1 + 3) = ((*(in_1 + 2) & 0xf0) >> 2) | ((*(in_1 + 3) & 0x3) << 6) | ((*(in_1 + 4) & 0xc0) >> 6);
                *(out_1 + 4) = ((*(in_1 + 3) & 0xc0) >> 6) | ((*(in_1 + 4) & 0x3f) << 2);

                y_dst  = idx[0] * 2 + 1;
                x0_dst = idx[1] * 5;
                x1_dst = x0_dst + 1;
                x2_dst = x0_dst + 2;
                x3_dst = x0_dst + 3;
                x4_dst = x0_dst + 4;

                dst_write_char(buf_dst, y_dst, x0_dst, width * 5 / 2, (p_1 & 0xff));
                dst_write_char(buf_dst, y_dst, x1_dst, width * 5 / 2, ((p_1 >> 8) & 0xff));
                dst_write_char(buf_dst, y_dst, x2_dst, width * 5 / 2, ((p_1 >> 16) & 0xff));
                dst_write_char(buf_dst, y_dst, x3_dst, width * 5 / 2, ((p_1 >> 24) & 0xff));
                dst_write_char(buf_dst, y_dst, x4_dst, width * 5 / 2, ((p_1 >> 32) & 0xff));
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)(pcsc->evt) = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_v210_to_yuv422p10le(struct impl_csc_params *pcsc, unsigned char *buf_in_c,
                                         unsigned char *buf_dst_c, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_in_c != NULL, "buf_in_c is null");
    IMPL_ASSERT(buf_dst_c != NULL, "buf_dst_c is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            unsigned int *buf_in    = (unsigned int *)buf_in_c;
            unsigned short *buf_dst = (unsigned short *)buf_dst_c;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height, width / 6), [=](sycl::id<2> idx) {
                const auto src_lmy = idx[0];
                const auto src_lmx = idx[1] * 4;

                const auto dst_lmy = idx[0];
                const auto dst_lmx = idx[1] * 6;

                const auto pixel0 = src_read_int(buf_in, src_lmy, src_lmx, width * 2 / 3);
                const auto pixel1 = src_read_int(buf_in, src_lmy, (src_lmx + 1), width * 2 / 3);
                const auto pixel2 = src_read_int(buf_in, src_lmy, (src_lmx + 2), width * 2 / 3);
                const auto pixel3 = src_read_int(buf_in, src_lmy, (src_lmx + 3), width * 2 / 3);

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

                dst_write_short(buf_dst, dst_lmy, dst_lmx, width, ((unsigned short)lm0) & 0x3ff);
                dst_write_short(buf_dst, dst_lmy, dst_lmx + 1, width, ((unsigned short)lm1) & 0x3ff);
                dst_write_short(buf_dst, dst_lmy, dst_lmx + 2, width, ((unsigned short)lm2) & 0x3ff);
                dst_write_short(buf_dst, dst_lmy, dst_lmx + 3, width, ((unsigned short)lm3) & 0x3ff);
                dst_write_short(buf_dst, dst_lmy, dst_lmx + 4, width, ((unsigned short)lm4) & 0x3ff);
                dst_write_short(buf_dst, dst_lmy, dst_lmx + 5, width, ((unsigned short)lm5) & 0x3ff);

                std::size_t dst_offset_y = 0;
                std::size_t dst_offset_x = 0;

                std::size_t u_linear_id = (-dst_offset_y * 3 / 2 + dst_lmy) * (width / 2) + dst_lmx / 2 +
                                          dst_offset_y * width + dst_offset_x / 2;
                std::size_t v_linear_id = u_linear_id + width * height / 2;
                std::size_t u_x         = u_linear_id % width - dst_offset_x;
                std::size_t u_y         = u_linear_id / width + height - dst_offset_y;
                std::size_t v_x         = v_linear_id % width - dst_offset_x;
                std::size_t v_y         = v_linear_id / width + height - dst_offset_y;

                dst_write_short(buf_dst, u_y, u_x, width, ((unsigned short)cb0) & 0x3ff);
                dst_write_short(buf_dst, v_y, v_x, width, ((unsigned short)cr0) & 0x3ff);

                u_linear_id = (-dst_offset_y * 3 / 2 + dst_lmy) * (width / 2) + dst_lmx / 2 + 1 + dst_offset_y * width +
                              dst_offset_x / 2;
                v_linear_id = u_linear_id + width * height / 2;
                u_x         = u_linear_id % width - dst_offset_x;
                u_y         = u_linear_id / width + height - dst_offset_y;
                v_x         = v_linear_id % width - dst_offset_x;
                v_y         = v_linear_id / width + height - dst_offset_y;

                dst_write_short(buf_dst, u_y, u_x, width, ((unsigned short)cb1) & 0x3ff);
                dst_write_short(buf_dst, v_y, v_x, width, ((unsigned short)cr1) & 0x3ff);

                u_linear_id = (-dst_offset_y * 3 / 2 + dst_lmy) * (width / 2) + dst_lmx / 2 + 2 + dst_offset_y * width +
                              dst_offset_x / 2;
                v_linear_id = u_linear_id + width * height / 2;
                u_x         = u_linear_id % width - dst_offset_x;
                u_y         = u_linear_id / width + height - dst_offset_y;
                v_x         = v_linear_id % width - dst_offset_x;
                v_y         = v_linear_id / width + height - dst_offset_y;

                dst_write_short(buf_dst, u_y, u_x, width, ((unsigned short)cb2) & 0x3ff);
                dst_write_short(buf_dst, v_y, v_x, width, ((unsigned short)cr2) & 0x3ff);
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)(pcsc->evt) = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_v210_to_yuv422ycbcr10be(struct impl_csc_params *pcsc, unsigned char *buf_in_c,
                                             unsigned char *buf_dst, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_in_c != NULL, "buf_in_c is null");
    IMPL_ASSERT(buf_dst != NULL, "buf_dst is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            unsigned int *buf_in = (unsigned int *)buf_in_c;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height, width / 6), [=](sycl::id<2> idx) {
                const auto src_lmy = idx[0];
                const auto src_lmx = idx[1] * 4;

                const auto pixel0 = src_read_int(buf_in, src_lmy, src_lmx, width * 2 / 3);
                const auto pixel1 = src_read_int(buf_in, src_lmy, (src_lmx + 1), width * 2 / 3);
                const auto pixel2 = src_read_int(buf_in, src_lmy, (src_lmx + 2), width * 2 / 3);
                const auto pixel3 = src_read_int(buf_in, src_lmy, (src_lmx + 3), width * 2 / 3);

                unsigned long t0 = ((unsigned long)pixel0 & 0x3fffffff) | (((unsigned long)pixel1 & 0x3ff) << 30);
                unsigned long t1 =
                    (((unsigned long)pixel1 & 0x3fffffff) >> 10) | (((unsigned long)pixel2 & 0xfffff) << 20);
                unsigned long t2 =
                    (((unsigned long)pixel2 & 0x3fffffff) >> 20) | (((unsigned long)pixel3 & 0x3fffffff) << 10);

                const auto dst_y0 = idx[0];
                auto dst_x0       = idx[1] * 15;
                auto dst_x1       = dst_x0 + 1;
                auto dst_x2       = dst_x0 + 2;
                auto dst_x3       = dst_x0 + 3;
                auto dst_x4       = dst_x0 + 4;

                unsigned long p    = 0;
                unsigned char *in  = (unsigned char *)&t0;
                unsigned char *out = (unsigned char *)&p;
                *(out + 0)         = ((*(in + 0) & 0xff) >> 2) | ((*(in + 1) & 0x3) << 6);
                *(out + 1)         = ((*(in + 0) & 0x3) << 6) | ((*(in + 1) & 0xc0) >> 6) | ((*(in + 2) & 0xf) << 2);
                *(out + 2)         = ((*(in + 1) & 0x3c) << 2) | ((*(in + 3) & 0x3f) >> 2);
                *(out + 3)         = ((*(in + 2) & 0xf0) >> 2) | ((*(in + 3) & 0x3) << 6) | ((*(in + 4) & 0xc0) >> 6);
                *(out + 4)         = ((*(in + 3) & 0xc0) >> 6) | ((*(in + 4) & 0x3f) << 2);

                dst_write_char(buf_dst, dst_y0, dst_x0, width * 5 / 2, (unsigned char)(p & 0xff));
                dst_write_char(buf_dst, dst_y0, dst_x1, width * 5 / 2, (unsigned char)((p >> 8) & 0xff));
                dst_write_char(buf_dst, dst_y0, dst_x2, width * 5 / 2, (unsigned char)((p >> 16) & 0xff));
                dst_write_char(buf_dst, dst_y0, dst_x3, width * 5 / 2, (unsigned char)((p >> 24) & 0xff));
                dst_write_char(buf_dst, dst_y0, dst_x4, width * 5 / 2, (unsigned char)((p >> 32) & 0xff));

                dst_x0     = idx[1] * 15 + 5;
                dst_x1     = dst_x0 + 1;
                dst_x2     = dst_x0 + 2;
                dst_x3     = dst_x0 + 3;
                dst_x4     = dst_x0 + 4;
                in         = (unsigned char *)&t1;
                *(out + 0) = ((*(in + 0) & 0xff) >> 2) | ((*(in + 1) & 0x3) << 6);
                *(out + 1) = ((*(in + 0) & 0x3) << 6) | ((*(in + 1) & 0xc0) >> 6) | ((*(in + 2) & 0xf) << 2);
                *(out + 2) = ((*(in + 1) & 0x3c) << 2) | ((*(in + 3) & 0x3f) >> 2);
                *(out + 3) = ((*(in + 2) & 0xf0) >> 2) | ((*(in + 3) & 0x3) << 6) | ((*(in + 4) & 0xc0) >> 6);
                *(out + 4) = ((*(in + 3) & 0xc0) >> 6) | ((*(in + 4) & 0x3f) << 2);
                dst_write_char(buf_dst, dst_y0, dst_x0, width * 5 / 2, (unsigned char)(p & 0xff));
                dst_write_char(buf_dst, dst_y0, dst_x1, width * 5 / 2, (unsigned char)((p >> 8) & 0xff));
                dst_write_char(buf_dst, dst_y0, dst_x2, width * 5 / 2, (unsigned char)((p >> 16) & 0xff));
                dst_write_char(buf_dst, dst_y0, dst_x3, width * 5 / 2, (unsigned char)((p >> 24) & 0xff));
                dst_write_char(buf_dst, dst_y0, dst_x4, width * 5 / 2, (unsigned char)((p >> 32) & 0xff));

                dst_x0     = idx[1] * 15 + 10;
                dst_x1     = dst_x0 + 1;
                dst_x2     = dst_x0 + 2;
                dst_x3     = dst_x0 + 3;
                dst_x4     = dst_x0 + 4;
                in         = (unsigned char *)&t2;
                *(out + 0) = ((*(in + 0) & 0xff) >> 2) | ((*(in + 1) & 0x3) << 6);
                *(out + 1) = ((*(in + 0) & 0x3) << 6) | ((*(in + 1) & 0xc0) >> 6) | ((*(in + 2) & 0xf) << 2);
                *(out + 2) = ((*(in + 1) & 0x3c) << 2) | ((*(in + 3) & 0x3f) >> 2);
                *(out + 3) = ((*(in + 2) & 0xf0) >> 2) | ((*(in + 3) & 0x3) << 6) | ((*(in + 4) & 0xc0) >> 6);
                *(out + 4) = ((*(in + 3) & 0xc0) >> 6) | ((*(in + 4) & 0x3f) << 2);
                dst_write_char(buf_dst, dst_y0, dst_x0, width * 5 / 2, (unsigned char)(p & 0xff));
                dst_write_char(buf_dst, dst_y0, dst_x1, width * 5 / 2, (unsigned char)((p >> 8) & 0xff));
                dst_write_char(buf_dst, dst_y0, dst_x2, width * 5 / 2, (unsigned char)((p >> 16) & 0xff));
                dst_write_char(buf_dst, dst_y0, dst_x3, width * 5 / 2, (unsigned char)((p >> 24) & 0xff));
                dst_write_char(buf_dst, dst_y0, dst_x4, width * 5 / 2, (unsigned char)((p >> 32) & 0xff));
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)(pcsc->evt) = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_v210_to_yuv422ycbcr10le(struct impl_csc_params *pcsc, unsigned char *buf_in_c,
                                             unsigned char *buf_dst, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_in_c != NULL, "buf_in_c is null");
    IMPL_ASSERT(buf_dst != NULL, "buf_dst_c is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            unsigned int *buf_in = (unsigned int *)buf_in_c;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height, (width / 6)), [=](sycl::id<2> idx) {
                const auto y0     = idx[0];
                const auto x0     = idx[1] * 4;
                const auto pixel0 = src_read_int(buf_in, y0, x0, width * 2 / 3);
                const auto pixel1 = src_read_int(buf_in, y0, (x0 + 1), width * 2 / 3);
                const auto pixel2 = src_read_int(buf_in, y0, (x0 + 2), width * 2 / 3);
                const auto pixel3 = src_read_int(buf_in, y0, (x0 + 3), width * 2 / 3);

                unsigned int cb0 = (((unsigned long)pixel0) & 0x3ff);
                unsigned int lm0 = (((unsigned long)pixel0 >> 10) & 0x3ff);
                unsigned int cr0 = (((unsigned long)pixel0 >> 20) & 0x3ff);
                unsigned int lm1 = (((unsigned long)pixel1) & 0x3ff);
                unsigned int cb1 = (((unsigned long)pixel1 >> 10) & 0x3ff);
                unsigned int lm2 = (((unsigned long)pixel1 >> 20) & 0x3ff);
                unsigned int cr1 = (((unsigned long)pixel2) & 0x3ff);
                unsigned int lm3 = (((unsigned long)pixel2 >> 10) & 0x3ff);
                unsigned int cb2 = (((unsigned long)pixel2 >> 20) & 0x3ff);
                unsigned int lm4 = (((unsigned long)pixel3) & 0x3ff);
                unsigned int cr2 = (((unsigned long)pixel3 >> 10) & 0x3ff);
                unsigned int lm5 = (((unsigned long)pixel3 >> 20) & 0x3ff);

                auto x_dst      = idx[1] * 15;
                unsigned long t = ((unsigned long)cb0 & 0x3ff) | (((unsigned long)lm0 & 0x3ff) << 10) |
                                  (((unsigned long)cr0 & 0x3ff) << 20) | (((unsigned long)lm1 & 0x3ff) << 30);
                unsigned char *out = (unsigned char *)&t;
                dst_write_char(buf_dst, y0, x_dst, width * 5 / 2, out[0]);
                dst_write_char(buf_dst, y0, x_dst + 1, width * 5 / 2, out[1]);
                dst_write_char(buf_dst, y0, x_dst + 2, width * 5 / 2, out[2]);
                dst_write_char(buf_dst, y0, x_dst + 3, width * 5 / 2, out[3]);
                dst_write_char(buf_dst, y0, x_dst + 4, width * 5 / 2, out[4]);

                unsigned long t_1 = ((unsigned long)cb1 & 0x3ff) | (((unsigned long)lm2 & 0x3ff) << 10) |
                                    (((unsigned long)cr1 & 0x3ff) << 20) | (((unsigned long)lm3 & 0x3ff) << 30);
                unsigned char *out_1 = (unsigned char *)&t_1;
                dst_write_char(buf_dst, y0, x_dst + 5, width * 5 / 2, out_1[0]);
                dst_write_char(buf_dst, y0, x_dst + 6, width * 5 / 2, out_1[1]);
                dst_write_char(buf_dst, y0, x_dst + 7, width * 5 / 2, out_1[2]);
                dst_write_char(buf_dst, y0, x_dst + 8, width * 5 / 2, out_1[3]);
                dst_write_char(buf_dst, y0, x_dst + 9, width * 5 / 2, out_1[4]);

                unsigned long t_2 = ((unsigned long)cb2 & 0x3ff) | (((unsigned long)lm4 & 0x3ff) << 10) |
                                    (((unsigned long)cr2 & 0x3ff) << 20) | (((unsigned long)lm5 & 0x3ff) << 30);
                unsigned char *out_2 = (unsigned char *)&t_2;

                dst_write_char(buf_dst, y0, x_dst + 10, width * 5 / 2, out_2[0]);
                dst_write_char(buf_dst, y0, x_dst + 11, width * 5 / 2, out_2[1]);
                dst_write_char(buf_dst, y0, x_dst + 12, width * 5 / 2, out_2[2]);
                dst_write_char(buf_dst, y0, x_dst + 13, width * 5 / 2, out_2[3]);
                dst_write_char(buf_dst, y0, x_dst + 14, width * 5 / 2, out_2[4]);
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)(pcsc->evt) = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_v210_to_y210(struct impl_csc_params *pcsc, unsigned char *buf_in_c, unsigned char *buf_dst_c,
                                  void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_in_c != NULL, "buf_in_c is null");
    IMPL_ASSERT(buf_dst_c != NULL, "buf_dst_c is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            unsigned int *buf_in    = (unsigned int *)buf_in_c;
            unsigned short *buf_dst = (unsigned short *)buf_dst_c;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height, (width / 6)), [=](sycl::id<2> idx) {
                const auto src_lmy = idx[0];
                const auto src_lmx = idx[1] * 4;

                const auto pixel0 = src_read_int(buf_in, src_lmy, src_lmx, width * 2 / 3);
                const auto pixel1 = src_read_int(buf_in, src_lmy, (src_lmx + 1), width * 2 / 3);
                const auto pixel2 = src_read_int(buf_in, src_lmy, (src_lmx + 2), width * 2 / 3);
                const auto pixel3 = src_read_int(buf_in, src_lmy, (src_lmx + 3), width * 2 / 3);

                unsigned int cb0 = (((unsigned long)pixel0) & 0x3ff);
                unsigned int lm0 = (((unsigned long)pixel0 >> 10) & 0x3ff);
                unsigned int cr0 = (((unsigned long)pixel0 >> 20) & 0x3ff);
                unsigned int lm1 = (((unsigned long)pixel1) & 0x3ff);
                unsigned int cb1 = (((unsigned long)pixel1 >> 10) & 0x3ff);
                unsigned int lm2 = (((unsigned long)pixel1 >> 20) & 0x3ff);
                unsigned int cr1 = (((unsigned long)pixel2) & 0x3ff);
                unsigned int lm3 = (((unsigned long)pixel2 >> 10) & 0x3ff);
                unsigned int cb2 = (((unsigned long)pixel2 >> 20) & 0x3ff);
                unsigned int lm4 = (((unsigned long)pixel3) & 0x3ff);
                unsigned int cr2 = (((unsigned long)pixel3 >> 10) & 0x3ff);
                unsigned int lm5 = (((unsigned long)pixel3 >> 20) & 0x3ff);

                const auto dst_y0 = idx[0];
                const auto dst_x0 = idx[1] * 12;
                dst_write_short(buf_dst, dst_y0, dst_x0, width * 2, (unsigned short)((lm0 << 6) & 0xffff));
                dst_write_short(buf_dst, dst_y0, (dst_x0 + 1), width * 2, (unsigned short)((cb0 << 6) & 0xffff));
                dst_write_short(buf_dst, dst_y0, (dst_x0 + 2), width * 2, (unsigned short)((lm1 << 6) & 0xffff));
                dst_write_short(buf_dst, dst_y0, (dst_x0 + 3), width * 2, (unsigned short)((cr0 << 6) & 0xffff));
                dst_write_short(buf_dst, dst_y0, (dst_x0 + 4), width * 2, (unsigned short)((lm2 << 6) & 0xffff));
                dst_write_short(buf_dst, dst_y0, (dst_x0 + 5), width * 2, (unsigned short)((cb1 << 6) & 0xffff));
                dst_write_short(buf_dst, dst_y0, (dst_x0 + 6), width * 2, (unsigned short)((lm3 << 6) & 0xffff));
                dst_write_short(buf_dst, dst_y0, (dst_x0 + 7), width * 2, (unsigned short)((cr1 << 6) & 0xffff));
                dst_write_short(buf_dst, dst_y0, (dst_x0 + 8), width * 2, (unsigned short)((lm4 << 6) & 0xffff));
                dst_write_short(buf_dst, dst_y0, (dst_x0 + 9), width * 2, (unsigned short)((cb2 << 6) & 0xffff));
                dst_write_short(buf_dst, dst_y0, (dst_x0 + 10), width * 2, (unsigned short)((lm5 << 6) & 0xffff));
                dst_write_short(buf_dst, dst_y0, (dst_x0 + 11), width * 2, (unsigned short)((cr2 << 6) & 0xffff));
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)(pcsc->evt) = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_y210_to_yuv422ycbcr10be(struct impl_csc_params *pcsc, unsigned char *buf_dst_c,
                                             unsigned char *buf_out, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_out != NULL, "buf_out is null");
    IMPL_ASSERT(buf_dst_c != NULL, "buf_dst_c is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            unsigned short *buf_dst = (unsigned short *)buf_dst_c;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height, (width / 2)), [=](sycl::id<2> idx) {
                const auto x0 = idx[1] * 4;
                const auto x1 = x0 + 1;
                const auto x2 = x0 + 2;
                const auto x3 = x0 + 3;
                const auto y0 = idx[0];

                unsigned short src_y0       = src_read_short(buf_dst, y0, x0, width * 2);
                unsigned short src_cb       = src_read_short(buf_dst, y0, x1, width * 2);
                unsigned short src_y1       = src_read_short(buf_dst, y0, x2, width * 2);
                unsigned short src_cr       = src_read_short(buf_dst, y0, x3, width * 2);
                unsigned short src_y0_pixel = (((unsigned short)src_y0 & 0xffff) >> 6);
                unsigned short src_cb_pixel = (((unsigned short)src_cb & 0xffff) >> 6);
                unsigned short src_y1_pixel = (((unsigned short)src_y1 & 0xffff) >> 6);
                unsigned short src_cr_pixel = (((unsigned short)src_cr & 0xffff) >> 6);

                unsigned long p = 0;
                unsigned long t =
                    ((unsigned long)src_cb_pixel & 0x3ff) | (((unsigned long)src_y0_pixel & 0x3ff) << 10) |
                    (((unsigned long)src_cr_pixel & 0x3ff) << 20) | (((unsigned long)src_y1_pixel & 0x3ff) << 30);

                unsigned char *in  = (unsigned char *)&t;
                unsigned char *out = (unsigned char *)&p;
                *(out + 0)         = ((*(in + 0) & 0xff) >> 2) | ((*(in + 1) & 0x3) << 6);
                *(out + 1)         = ((*(in + 0) & 0x3) << 6) | ((*(in + 1) & 0xc0) >> 6) | ((*(in + 2) & 0xf) << 2);
                *(out + 2)         = ((*(in + 1) & 0x3c) << 2) | ((*(in + 3) & 0x3f) >> 2);
                *(out + 3)         = ((*(in + 2) & 0xf0) >> 2) | ((*(in + 3) & 0x3) << 6) | ((*(in + 4) & 0xc0) >> 6);
                *(out + 4)         = ((*(in + 3) & 0xc0) >> 6) | ((*(in + 4) & 0x3f) << 2);

                auto y_dst  = idx[0];
                auto x0_dst = idx[1] * 5;
                auto x1_dst = x0_dst + 1;
                auto x2_dst = x0_dst + 2;
                auto x3_dst = x0_dst + 3;
                auto x4_dst = x0_dst + 4;
                dst_write_char(buf_out, y_dst, x0_dst, width * 5 / 2, (unsigned char)(p & 0xff));
                dst_write_char(buf_out, y_dst, x1_dst, width * 5 / 2, (unsigned char)((p >> 8) & 0xff));
                dst_write_char(buf_out, y_dst, x2_dst, width * 5 / 2, (unsigned char)((p >> 16) & 0xff));
                dst_write_char(buf_out, y_dst, x3_dst, width * 5 / 2, (unsigned char)((p >> 24) & 0xff));
                dst_write_char(buf_out, y_dst, x4_dst, width * 5 / 2, (unsigned char)((p >> 32) & 0xff));
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)pcsc->evt = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_y210_to_yuv422p10le(struct impl_csc_params *pcsc, unsigned char *buf_dst_c,
                                         unsigned char *buf_out_c, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_out_c != NULL, "buf_out_c is null");
    IMPL_ASSERT(buf_dst_c != NULL, "buf_dst_c is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            unsigned short *buf_dst = (unsigned short *)buf_dst_c;
            unsigned short *buf_out = (unsigned short *)buf_out_c;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height, (width / 2)), [=](sycl::id<2> idx) {
                const auto y_dst = idx[0];
                const auto x_dst = idx[1] * 2;

                const auto x0 = idx[1] * 4;
                const auto x1 = x0 + 1;
                const auto x2 = x0 + 2;
                const auto x3 = x0 + 3;
                const auto y0 = idx[0];

                unsigned short src_y0       = src_read_short(buf_dst, y0, x0, width * 2);
                unsigned short src_cb       = src_read_short(buf_dst, y0, x1, width * 2);
                unsigned short src_y1       = src_read_short(buf_dst, y0, x2, width * 2);
                unsigned short src_cr       = src_read_short(buf_dst, y0, x3, width * 2);
                unsigned short src_y0_pixel = (((unsigned short)src_y0 & 0xffff) >> 6);
                unsigned short src_cb_pixel = (((unsigned short)src_cb & 0xffff) >> 6);
                unsigned short src_y1_pixel = (((unsigned short)src_y1 & 0xffff) >> 6);
                unsigned short src_cr_pixel = (((unsigned short)src_cr & 0xffff) >> 6);

                dst_write_short(buf_out, y_dst, x_dst, width, (unsigned short)(src_y0_pixel));
                dst_write_short(buf_out, y_dst, x_dst + 1, width, (unsigned short)(src_y1_pixel));

                const std::size_t dst_offset_y = 0;
                const std::size_t dst_offset_x = 0;

                const std::size_t u_linear_id =
                    (-dst_offset_y * 3 / 2 + idx[0]) * (width / 2) + idx[1] + dst_offset_y * width + dst_offset_x / 2;
                const std::size_t v_linear_id = u_linear_id + width * height / 2;
                const std::size_t u_x         = u_linear_id % width - dst_offset_x;
                const std::size_t u_y         = u_linear_id / width + height - dst_offset_y;
                const std::size_t v_x         = v_linear_id % width - dst_offset_x;
                const std::size_t v_y         = v_linear_id / width + height - dst_offset_y;

                dst_write_short(buf_out, u_y, u_x, width, (unsigned short)(src_cb_pixel));
                dst_write_short(buf_out, v_y, v_x, width, (unsigned short)(src_cr_pixel));
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)pcsc->evt = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_y210_to_yuv422ycbcr10le(struct impl_csc_params *pcsc, unsigned char *buf_dst_c,
                                             unsigned char *buf_out, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_out != NULL, "buf_out is null");
    IMPL_ASSERT(buf_dst_c != NULL, "buf_dst_c is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            unsigned short *buf_dst = (unsigned short *)buf_dst_c;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height, width / 2), [=](sycl::id<2> idx) {
                const auto x0 = idx[1] * 4;
                const auto x1 = x0 + 1;
                const auto x2 = x0 + 2;
                const auto x3 = x0 + 3;
                const auto y0 = idx[0];

                unsigned short src_y0 = src_read_short(buf_dst, y0, x0, width * 2);
                unsigned short src_cb = src_read_short(buf_dst, y0, x1, width * 2);
                unsigned short src_y1 = src_read_short(buf_dst, y0, x2, width * 2);
                unsigned short src_cr = src_read_short(buf_dst, y0, x3, width * 2);

                unsigned int src_y0_pixel = (((unsigned int)src_y0 & 0xffff) >> 6);
                unsigned int src_cb_pixel = (((unsigned int)src_cb & 0xffff) >> 6);
                unsigned int src_y1_pixel = (((unsigned int)src_y1 & 0xffff) >> 6);
                unsigned int src_cr_pixel = (((unsigned int)src_cr & 0xffff) >> 6);

                unsigned long p =
                    ((unsigned long)src_cb_pixel & 0x3ff) | (((unsigned long)src_y0_pixel & 0x3ff) << 10) |
                    (((unsigned long)src_cr_pixel & 0x3ff) << 20) | (((unsigned long)src_y1_pixel & 0x3ff) << 30);

                auto y_dst  = idx[0];
                auto x0_dst = idx[1] * 5;
                auto x1_dst = x0_dst + 1;
                auto x2_dst = x0_dst + 2;
                auto x3_dst = x0_dst + 3;
                auto x4_dst = x0_dst + 4;
                dst_write_char(buf_out, y_dst, x0_dst, width * 5 / 2, (unsigned char)(p & 0xff));
                dst_write_char(buf_out, y_dst, x1_dst, width * 5 / 2, (unsigned char)((p >> 8) & 0xff));
                dst_write_char(buf_out, y_dst, x2_dst, width * 5 / 2, (unsigned char)((p >> 16) & 0xff));
                dst_write_char(buf_out, y_dst, x3_dst, width * 5 / 2, (unsigned char)((p >> 24) & 0xff));
                dst_write_char(buf_out, y_dst, x4_dst, width * 5 / 2, (unsigned char)((p >> 32) & 0xff));
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)pcsc->evt = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_y210_to_v210(struct impl_csc_params *pcsc, unsigned char *buf_dst_c, unsigned char *buf_out_c,
                                  void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_out_c != NULL, "buf_out_c is null");
    IMPL_ASSERT(buf_dst_c != NULL, "buf_dst_c is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            unsigned short *buf_dst = (unsigned short *)buf_dst_c;
            unsigned int *buf_out   = (unsigned int *)buf_out_c;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height, width / 6), [=](sycl::id<2> idx) {
                const auto y_dst = idx[0];
                const auto x_dst = idx[1] * 4;

                const auto x0_src = idx[1] * 12;
                const auto y_src  = idx[0];

                unsigned short src_y0  = src_read_short(buf_dst, y_src, x0_src, width * 2);
                unsigned short src_cb0 = src_read_short(buf_dst, y_src, (x0_src + 1), width * 2);
                unsigned short src_y1  = src_read_short(buf_dst, y_src, (x0_src + 2), width * 2);
                unsigned short src_cr0 = src_read_short(buf_dst, y_src, (x0_src + 3), width * 2);

                unsigned int lm0 = (((unsigned int)src_y0 & 0xffff) >> 6);
                unsigned int cb0 = (((unsigned int)src_cb0 & 0xffff) >> 6);
                unsigned int lm1 = (((unsigned int)src_y1 & 0xffff) >> 6);
                unsigned int cr0 = (((unsigned int)src_cr0 & 0xffff) >> 6);

                unsigned int pixel0 = cb0 | lm0 << 10 | cr0 << 20;
                dst_write_int(buf_out, y_dst, x_dst, width * 2 / 3, pixel0);

                unsigned short src_y2  = src_read_short(buf_dst, y_src, (x0_src + 4), width * 2);
                unsigned short src_cb1 = src_read_short(buf_dst, y_src, (x0_src + 5), width * 2);
                unsigned short src_y3  = src_read_short(buf_dst, y_src, (x0_src + 6), width * 2);
                unsigned short src_cr1 = src_read_short(buf_dst, y_src, (x0_src + 7), width * 2);

                unsigned int lm2 = (((unsigned int)src_y2 & 0xffff) >> 6);
                unsigned int cb1 = (((unsigned int)src_cb1 & 0xffff) >> 6);
                unsigned int lm3 = (((unsigned int)src_y3 & 0xffff) >> 6);
                unsigned int cr1 = (((unsigned int)src_cr1 & 0xffff) >> 6);

                unsigned int pixel1 = lm1 | cb1 << 10 | lm2 << 20;
                dst_write_int(buf_out, y_dst, (x_dst + 1), width * 2 / 3, pixel1);

                unsigned short src_y4  = src_read_short(buf_dst, y_src, (x0_src + 8), width * 2);
                unsigned short src_cb2 = src_read_short(buf_dst, y_src, (x0_src + 9), width * 2);
                unsigned short src_y5  = src_read_short(buf_dst, y_src, (x0_src + 10), width * 2);
                unsigned short src_cr2 = src_read_short(buf_dst, y_src, (x0_src + 11), width * 2);

                unsigned int lm4 = (((unsigned int)src_y4 & 0xffff) >> 6);
                unsigned int cb2 = (((unsigned int)src_cb2 & 0xffff) >> 6);
                unsigned int lm5 = (((unsigned int)src_y5 & 0xffff) >> 6);
                unsigned int cr2 = (((unsigned int)src_cr2 & 0xffff) >> 6);

                unsigned int pixel2 = cr1 | lm3 << 10 | cb2 << 20;
                dst_write_int(buf_out, y_dst, (x_dst + 2), width * 2 / 3, pixel2);

                unsigned int pixel3 = lm4 | cr2 << 10 | lm5 << 20;
                dst_write_int(buf_out, y_dst, (x_dst + 3), width * 2 / 3, pixel3);
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)pcsc->evt = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_yuv422p10le_to_y210(struct impl_csc_params *pcsc, unsigned char *buf_in_c,
                                         unsigned char *buf_dst_c, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_in_c != NULL, "buf_in_c is null");
    IMPL_ASSERT(buf_dst_c != NULL, "buf_dst_c is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        unsigned short *buf_in  = (unsigned short *)buf_in_c;
        unsigned short *buf_dst = (unsigned short *)buf_dst_c;

        try {
            const std::size_t src_offset_y = 0;
            const std::size_t src_offset_x = 0;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height, width / 2), [=](sycl::id<2> idx) {
                const auto src_lmy = idx[0];
                const auto src_lmx = idx[1] * 2;

                const std::size_t cb_linear_id =
                    (-src_offset_y * 3 / 2 + idx[0]) * (width / 2) + idx[1] + src_offset_y * width + src_offset_x / 2;
                const std::size_t cr_linear_id = cb_linear_id + width * height / 2;
                const std::size_t cb_x         = cb_linear_id % width - src_offset_x;
                const std::size_t cb_y         = cb_linear_id / width + height - src_offset_y;
                const std::size_t cr_x         = cr_linear_id % width - src_offset_x;
                const std::size_t cr_y         = cr_linear_id / width + height - src_offset_y;

                const auto src_y0 = src_read_short(buf_in, src_lmy, src_lmx, width);
                const auto src_y1 = src_read_short(buf_in, src_lmy, src_lmx + 1, width);
                const auto cb0    = src_read_short(buf_in, cb_y, cb_x, width);
                const auto cr0    = src_read_short(buf_in, cr_y, cr_x, width);

                unsigned short t[4] = {
                    0,
                };
                unsigned short *in = t;
                in[3]              = (unsigned short)cr0 & 0x3ff;
                in[2]              = (unsigned short)cb0 & 0x3ff;
                in[1]              = (unsigned short)src_y1 & 0x3ff;
                in[0]              = (unsigned short)src_y0 & 0x3ff;

                const auto lmy_dst = idx[0];
                const auto lmx_dst = idx[1] * 4;
                dst_write_short(buf_dst, lmy_dst, lmx_dst, width * 2, ((unsigned short)src_y0 << 6) & 0xffc0);
                dst_write_short(buf_dst, lmy_dst, lmx_dst + 1, width * 2, ((unsigned short)cb0 << 6) & 0xffc0);
                dst_write_short(buf_dst, lmy_dst, lmx_dst + 2, width * 2, ((unsigned short)src_y1 << 6) & 0xffc0);
                dst_write_short(buf_dst, lmy_dst, lmx_dst + 3, width * 2, ((unsigned short)cr0 << 6) & 0xffc0);
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)pcsc->evt = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_yuv422p10le_to_v210(struct impl_csc_params *pcsc, unsigned char *buf_in_c,
                                         unsigned char *buf_dst_c, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_in_c != NULL, "buf_in_c is null");
    IMPL_ASSERT(buf_dst_c != NULL, "buf_dst_c is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            const std::size_t src_offset_y = 0;
            const std::size_t src_offset_x = 0;
            unsigned short *buf_in         = (unsigned short *)buf_in_c;
            unsigned int *buf_dst          = (unsigned int *)buf_dst_c;

            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height, width / 6), [=](sycl::id<2> idx) {
                const auto src_lmy = idx[0];
                const auto src_lmx = idx[1] * 6;

                const auto dst_lmy = idx[0];
                const auto dst_lmx = idx[1] * 4;

                std::size_t cb_linear_id = (-src_offset_y * 3 / 2 + src_lmy) * (width / 2) + src_lmx / 2 +
                                           src_offset_y * width + src_offset_x / 2;
                std::size_t cr_linear_id = cb_linear_id + width * height / 2;
                std::size_t cb_x         = cb_linear_id % width - src_offset_x;
                std::size_t cb_y         = cb_linear_id / width + height - src_offset_y;
                std::size_t cr_x         = cr_linear_id % width - src_offset_x;
                std::size_t cr_y         = cr_linear_id / width + height - src_offset_y;

                auto src_y0 = src_read_short(buf_in, src_lmy, src_lmx, width);
                auto src_y1 = src_read_short(buf_in, src_lmy, (src_lmx + 1), width);
                auto cb     = src_read_short(buf_in, cb_y, cb_x, width);
                auto cr     = src_read_short(buf_in, cr_y, cr_x, width);

                unsigned int cb0 = (((unsigned int)cb) & 0x3ff);
                unsigned int lm0 = (((unsigned int)src_y0) & 0x3ff);
                unsigned int cr0 = (((unsigned int)cr) & 0x3ff);
                unsigned int lm1 = (((unsigned int)src_y1) & 0x3ff);

                unsigned int pixel0 = cb0 | lm0 << 10 | cr0 << 20;
                dst_write_int(buf_dst, dst_lmy, dst_lmx, width * 2 / 3, pixel0 & 0x3fffffff);
                cb_linear_id = (-src_offset_y * 3 / 2 + src_lmy) * (width / 2) + src_lmx / 2 + 1 +
                               src_offset_y * width + src_offset_x / 2;
                cr_linear_id = cb_linear_id + width * height / 2;
                cb_x         = cb_linear_id % width - src_offset_x;
                cb_y         = cb_linear_id / width + height - src_offset_y;
                cr_x         = cr_linear_id % width - src_offset_x;
                cr_y         = cr_linear_id / width + height - src_offset_y;

                src_y0 = src_read_short(buf_in, src_lmy, (src_lmx + 2), width);
                src_y1 = src_read_short(buf_in, src_lmy, (src_lmx + 3), width);
                cb     = src_read_short(buf_in, cb_y, cb_x, width);
                cr     = src_read_short(buf_in, cr_y, cr_x, width);

                unsigned int cb1 = (((unsigned int)cb) & 0x3ff);
                unsigned int lm2 = (((unsigned int)src_y0) & 0x3ff);
                unsigned int cr1 = (((unsigned int)cr) & 0x3ff);
                unsigned int lm3 = (((unsigned int)src_y1) & 0x3ff);

                unsigned int pixel1 = lm1 | cb1 << 10 | lm2 << 20;
                dst_write_int(buf_dst, dst_lmy, (dst_lmx + 1), width * 2 / 3, pixel1 & 0x3fffffff);

                cb_linear_id = (-src_offset_y * 3 / 2 + src_lmy) * (width / 2) + src_lmx / 2 + 2 +
                               src_offset_y * width + src_offset_x / 2;
                cr_linear_id = cb_linear_id + width * height / 2;
                cb_x         = cb_linear_id % width - src_offset_x;
                cb_y         = cb_linear_id / width + height - src_offset_y;
                cr_x         = cr_linear_id % width - src_offset_x;
                cr_y         = cr_linear_id / width + height - src_offset_y;

                src_y0 = src_read_short(buf_in, src_lmy, (src_lmx + 4), width);
                src_y1 = src_read_short(buf_in, src_lmy, (src_lmx + 5), width);
                cb     = src_read_short(buf_in, cb_y, cb_x, width);
                cr     = src_read_short(buf_in, cr_y, cr_x, width);

                unsigned int cb2 = (((unsigned int)cb) & 0x3ff);
                unsigned int lm4 = (((unsigned int)src_y0) & 0x3ff);
                unsigned int cr2 = (((unsigned int)cr) & 0x3ff);
                unsigned int lm5 = (((unsigned int)src_y1) & 0x3ff);

                unsigned int pixel2 = cr1 | lm3 << 10 | cb2 << 20;
                dst_write_int(buf_dst, dst_lmy, (dst_lmx + 2), width * 2 / 3, pixel2 & 0x3fffffff);

                unsigned int pixel3 = lm4 | cr2 << 10 | lm5 << 20;
                dst_write_int(buf_dst, dst_lmy, (dst_lmx + 3), width * 2 / 3, pixel3 & 0x3fffffff);
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)pcsc->evt = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_yuv422p10le_to_yuv422ycbcr10be(struct impl_csc_params *pcsc, unsigned char *buf_in_c,
                                                    unsigned char *buf_dst, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_in_c != NULL, "buf_in_c is null");
    IMPL_ASSERT(buf_dst != NULL, "buf_dst_c is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            const std::size_t src_offset_y = 0;
            const std::size_t src_offset_x = 0;
            unsigned short *buf_in         = (unsigned short *)buf_in_c;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height, width / 2), [=](sycl::id<2> idx) {
                const auto src_lmy = idx[0];
                const auto src_lmx = idx[1] * 2;

                std::size_t cb_linear_id = (-src_offset_y * 3 / 2 + src_lmy) * (width / 2) + src_lmx / 2 +
                                           src_offset_y * width + src_offset_x / 2;
                std::size_t cr_linear_id = cb_linear_id + width * height / 2;
                std::size_t cb_x         = cb_linear_id % width - src_offset_x;
                std::size_t cb_y         = cb_linear_id / width + height - src_offset_y;
                std::size_t cr_x         = cr_linear_id % width - src_offset_x;
                std::size_t cr_y         = cr_linear_id / width + height - src_offset_y;

                const auto src_y0 = src_read_short(buf_in, src_lmy, src_lmx, width);
                const auto src_y1 = src_read_short(buf_in, src_lmy, (src_lmx + 1), width);
                const auto src_cb = src_read_short(buf_in, cb_y, cb_x, width);
                const auto src_cr = src_read_short(buf_in, cr_y, cr_x, width);

                unsigned long p = 0;
                unsigned long t = ((unsigned long)src_cb & 0x3ff) | (((unsigned long)src_y0 & 0x3ff) << 10) |
                                  (((unsigned long)src_cr & 0x3ff) << 20) | (((unsigned long)src_y1 & 0x3ff) << 30);
                unsigned char *in  = (unsigned char *)&t;
                unsigned char *out = (unsigned char *)&p;
                *(out + 0)         = ((*(in + 0) & 0xff) >> 2) | ((*(in + 1) & 0x3) << 6);
                *(out + 1)         = ((*(in + 0) & 0x3) << 6) | ((*(in + 1) & 0xc0) >> 6) | ((*(in + 2) & 0xf) << 2);
                *(out + 2)         = ((*(in + 1) & 0x3c) << 2) | ((*(in + 3) & 0x3f) >> 2);
                *(out + 3)         = ((*(in + 2) & 0xf0) >> 2) | ((*(in + 3) & 0x3) << 6) | ((*(in + 4) & 0xc0) >> 6);
                *(out + 4)         = ((*(in + 3) & 0xc0) >> 6) | ((*(in + 4) & 0x3f) << 2);

                auto y_dst  = idx[0];
                auto x0_dst = idx[1] * 5;
                auto x1_dst = x0_dst + 1;
                auto x2_dst = x0_dst + 2;
                auto x3_dst = x0_dst + 3;
                auto x4_dst = x0_dst + 4;

                dst_write_char(buf_dst, y_dst, x0_dst, width * 5 / 2, (unsigned char)(p & 0xff));
                dst_write_char(buf_dst, y_dst, x1_dst, width * 5 / 2, (unsigned char)((p >> 8) & 0xff));
                dst_write_char(buf_dst, y_dst, x2_dst, width * 5 / 2, (unsigned char)((p >> 16) & 0xff));
                dst_write_char(buf_dst, y_dst, x3_dst, width * 5 / 2, (unsigned char)((p >> 24) & 0xff));
                dst_write_char(buf_dst, y_dst, x4_dst, width * 5 / 2, (unsigned char)((p >> 32) & 0xff));
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)pcsc->evt = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_yuv422ycbcr10be_to_y210(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst_c, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_in != NULL, "buf_in is null");
    IMPL_ASSERT(buf_dst_c != NULL, "buf_dst_c is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            unsigned short *buf_dst = (unsigned short *)buf_dst_c;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height, width / 2), [=](sycl::id<2> idx) {
                const auto lmy_dst = idx[0];
                const auto lmx_dst = idx[1] * 4;

                const auto y_src = idx[0];
                auto x0_src      = idx[1] * 5;
                auto x1_src      = x0_src + 1;
                auto x2_src      = x0_src + 2;
                auto x3_src      = x0_src + 3;
                auto x4_src      = x0_src + 4;
                auto src0_pixel  = src_read_char(buf_in, y_src, x0_src, width * 5 / 2);
                auto src1_pixel  = src_read_char(buf_in, y_src, x1_src, width * 5 / 2);
                auto src2_pixel  = src_read_char(buf_in, y_src, x2_src, width * 5 / 2);
                auto src3_pixel  = src_read_char(buf_in, y_src, x3_src, width * 5 / 2);
                auto src4_pixel  = src_read_char(buf_in, y_src, x4_src, width * 5 / 2);

                unsigned char t[5] = {
                    0,
                };
                unsigned char *in = t;
                in[4]             = (unsigned char)src4_pixel & 0xff;
                in[3]             = (unsigned char)src3_pixel & 0xff;
                in[2]             = (unsigned char)src2_pixel & 0xff;
                in[1]             = (unsigned char)src1_pixel & 0xff;
                in[0]             = (unsigned char)src0_pixel & 0xff;

                unsigned long y0 = ((*(in + 1) & 0x3f) << 4) | ((*(in + 2) & 0xf0) >> 4);
                unsigned long y1 = *(in + 4) | ((*(in + 3) & 0x3) << 8);
                unsigned long cb = (*in << 2) | ((*(in + 1) & 0xc0) >> 6);
                unsigned long cr = ((*(in + 2) & 0xf) << 6) | ((*(in + 3) >> 2) & 0x3f);

                dst_write_short(buf_dst, lmy_dst, lmx_dst, width * 2, ((unsigned int)y0 << 6) & 0xffff);
                dst_write_short(buf_dst, lmy_dst, lmx_dst + 1, width * 2, ((unsigned int)cb << 6) & 0xffff);
                dst_write_short(buf_dst, lmy_dst, lmx_dst + 2, width * 2, ((unsigned int)y1 << 6) & 0xffff);
                dst_write_short(buf_dst, lmy_dst, lmx_dst + 3, width * 2, ((unsigned int)cr << 6) & 0xffff);
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)pcsc->evt = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_yuv422ycbcr10be_to_nv12(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_in != NULL, "buf_in is null");
    IMPL_ASSERT(buf_dst != NULL, "buf_dst is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height / 2, width / 2), [=](sycl::id<2> idx) {
                const auto lmy_dst    = idx[0] * 2;
                const auto lmx_dst    = idx[1] * 2;
                const auto y_src_cbcr = idx[0] + height;

                auto y_src      = idx[0] * 2;
                auto x0_src     = idx[1] * 5;
                auto x1_src     = x0_src + 1;
                auto x2_src     = x0_src + 2;
                auto x3_src     = x0_src + 3;
                auto x4_src     = x0_src + 4;
                auto src0_pixel = src_read_char(buf_in, y_src, x0_src, width * 5 / 2);
                auto src1_pixel = src_read_char(buf_in, y_src, x1_src, width * 5 / 2);
                auto src2_pixel = src_read_char(buf_in, y_src, x2_src, width * 5 / 2);
                auto src3_pixel = src_read_char(buf_in, y_src, x3_src, width * 5 / 2);
                auto src4_pixel = src_read_char(buf_in, y_src, x4_src, width * 5 / 2);

                unsigned char t[5] = {
                    0,
                };
                unsigned char *in = t;
                in[4]             = (unsigned char)src4_pixel & 0xff;
                in[3]             = (unsigned char)src3_pixel & 0xff;
                in[2]             = (unsigned char)src2_pixel & 0xff;
                in[1]             = (unsigned char)src1_pixel & 0xff;
                in[0]             = (unsigned char)src0_pixel & 0xff;

                unsigned long y0 = ((*(in + 1) & 0x3f) << 4) | ((*(in + 2) & 0xf0) >> 4);
                unsigned long y1 = *(in + 4) | ((*(in + 3) & 0x3) << 8);
                unsigned long cb = (*in << 2) | ((*(in + 1) & 0xc0) >> 6);
                unsigned long cr = ((*(in + 2) & 0xf) << 6) | ((*(in + 3) >> 2) & 0x3f);

                y_src      = idx[0] * 2 + 1;
                x0_src     = idx[1] * 5;
                x1_src     = x0_src + 1;
                x2_src     = x0_src + 2;
                x3_src     = x0_src + 3;
                x4_src     = x0_src + 4;
                src0_pixel = src_read_char(buf_in, y_src, x0_src, width * 5 / 2);
                src1_pixel = src_read_char(buf_in, y_src, x1_src, width * 5 / 2);
                src2_pixel = src_read_char(buf_in, y_src, x2_src, width * 5 / 2);
                src3_pixel = src_read_char(buf_in, y_src, x3_src, width * 5 / 2);
                src4_pixel = src_read_char(buf_in, y_src, x4_src, width * 5 / 2);

                unsigned char t_1[5] = {
                    0,
                };
                unsigned char *in_1 = t_1;
                in_1[4]             = (unsigned char)src4_pixel & 0xff;
                in_1[3]             = (unsigned char)src3_pixel & 0xff;
                in_1[2]             = (unsigned char)src2_pixel & 0xff;
                in_1[1]             = (unsigned char)src1_pixel & 0xff;
                in_1[0]             = (unsigned char)src0_pixel & 0xff;

                unsigned long y2 = ((*(in_1 + 1) & 0x3f) << 4) | ((*(in_1 + 2) & 0xf0) >> 4);
                unsigned long y3 = *(in_1 + 4) | ((*(in_1 + 3) & 0x3) << 8);

                dst_write_char(buf_dst, lmy_dst, lmx_dst, width, (unsigned char)(y0 >> 2));
                dst_write_char(buf_dst, lmy_dst, lmx_dst + 1, width, (unsigned char)(y1 >> 2));
                dst_write_char(buf_dst, lmy_dst + 1, lmx_dst, width, (unsigned char)(y2 >> 2));
                dst_write_char(buf_dst, lmy_dst + 1, lmx_dst + 1, width, (unsigned char)(y3 >> 2));
                dst_write_char(buf_dst, y_src_cbcr, lmx_dst, width, (unsigned char)(cb >> 2));
                dst_write_char(buf_dst, y_src_cbcr, lmx_dst + 1, width, (unsigned char)(cr >> 2));
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)pcsc->evt = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_yuv422ycbcr10be_to_v210(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst_c, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_in != NULL, "buf_in is null");
    IMPL_ASSERT(buf_dst_c != NULL, "buf_dst_c is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            unsigned int *buf_dst = (unsigned int *)buf_dst_c;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height, width / 6), [=](sycl::id<2> idx) {
                const auto lmy_dst = idx[0];
                const auto lmx_dst = idx[1] * 4;

                auto y_src     = idx[0];
                auto x0_src    = idx[1] * 15;
                auto x1_src    = x0_src + 1;
                auto x2_src    = x0_src + 2;
                auto x3_src    = x0_src + 3;
                auto x4_src    = x0_src + 4;
                auto src0_byte = src_read_char(buf_in, y_src, x0_src, width * 5 / 2);
                auto src1_byte = src_read_char(buf_in, y_src, x1_src, width * 5 / 2);
                auto src2_byte = src_read_char(buf_in, y_src, x2_src, width * 5 / 2);
                auto src3_byte = src_read_char(buf_in, y_src, x3_src, width * 5 / 2);
                auto src4_byte = src_read_char(buf_in, y_src, x4_src, width * 5 / 2);

                unsigned char t[5] = {
                    0,
                };
                unsigned char *in = t;
                in[4]             = (unsigned char)src4_byte & 0xff;
                in[3]             = (unsigned char)src3_byte & 0xff;
                in[2]             = (unsigned char)src2_byte & 0xff;
                in[1]             = (unsigned char)src1_byte & 0xff;
                in[0]             = (unsigned char)src0_byte & 0xff;

                unsigned long y0 = ((*(in + 1) & 0x3f) << 4) | ((*(in + 2) & 0xf0) >> 4);
                unsigned long y1 = *(in + 4) | ((*(in + 3) & 0x3) << 8);
                unsigned long cb = (*in << 2) | ((*(in + 1) & 0xc0) >> 6);
                unsigned long cr = ((*(in + 2) & 0xf) << 6) | ((*(in + 3) >> 2) & 0x3f);

                unsigned int pixel0 = cb | y0 << 10 | cr << 20;
                dst_write_int(buf_dst, lmy_dst, lmx_dst, width * 2 / 3, pixel0 & 0x3fffffff);

                x0_src    = idx[1] * 15 + 5;
                x1_src    = x0_src + 1;
                x2_src    = x0_src + 2;
                x3_src    = x0_src + 3;
                x4_src    = x0_src + 4;
                src0_byte = src_read_char(buf_in, y_src, x0_src, width * 5 / 2);
                src1_byte = src_read_char(buf_in, y_src, x1_src, width * 5 / 2);
                src2_byte = src_read_char(buf_in, y_src, x2_src, width * 5 / 2);
                src3_byte = src_read_char(buf_in, y_src, x3_src, width * 5 / 2);
                src4_byte = src_read_char(buf_in, y_src, x4_src, width * 5 / 2);

                in[4] = (unsigned char)src4_byte & 0xff;
                in[3] = (unsigned char)src3_byte & 0xff;
                in[2] = (unsigned char)src2_byte & 0xff;
                in[1] = (unsigned char)src1_byte & 0xff;
                in[0] = (unsigned char)src0_byte & 0xff;

                unsigned long y2  = ((*(in + 1) & 0x3f) << 4) | ((*(in + 2) & 0xf0) >> 4);
                unsigned long y3  = *(in + 4) | ((*(in + 3) & 0x3) << 8);
                unsigned long cb1 = (*in << 2) | ((*(in + 1) & 0xc0) >> 6);
                unsigned long cr1 = ((*(in + 2) & 0xf) << 6) | ((*(in + 3) >> 2) & 0x3f);

                unsigned int pixel1 = y1 | cb1 << 10 | y2 << 20;
                dst_write_int(buf_dst, lmy_dst, (lmx_dst + 1), width * 2 / 3, pixel1 & 0x3fffffff);

                x0_src    = idx[1] * 15 + 10;
                x1_src    = x0_src + 1;
                x2_src    = x0_src + 2;
                x3_src    = x0_src + 3;
                x4_src    = x0_src + 4;
                src0_byte = src_read_char(buf_in, y_src, x0_src, width * 5 / 2);
                src1_byte = src_read_char(buf_in, y_src, x1_src, width * 5 / 2);
                src2_byte = src_read_char(buf_in, y_src, x2_src, width * 5 / 2);
                src3_byte = src_read_char(buf_in, y_src, x3_src, width * 5 / 2);
                src4_byte = src_read_char(buf_in, y_src, x4_src, width * 5 / 2);

                in[4] = (unsigned char)src4_byte & 0xff;
                in[3] = (unsigned char)src3_byte & 0xff;
                in[2] = (unsigned char)src2_byte & 0xff;
                in[1] = (unsigned char)src1_byte & 0xff;
                in[0] = (unsigned char)src0_byte & 0xff;

                unsigned long y4  = ((*(in + 1) & 0x3f) << 4) | ((*(in + 2) & 0xf0) >> 4);
                unsigned long y5  = *(in + 4) | ((*(in + 3) & 0x3) << 8);
                unsigned long cb2 = (*in << 2) | ((*(in + 1) & 0xc0) >> 6);
                unsigned long cr2 = ((*(in + 2) & 0xf) << 6) | ((*(in + 3) >> 2) & 0x3f);

                unsigned int pixel2 = cr1 | y3 << 10 | cb2 << 20;
                dst_write_int(buf_dst, lmy_dst, (lmx_dst + 2), width * 2 / 3, pixel2 & 0x3fffffff);

                unsigned int pixel3 = y4 | cr2 << 10 | y5 << 20;
                dst_write_int(buf_dst, lmy_dst, (lmx_dst + 3), width * 2 / 3, pixel3 & 0x3fffffff);
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)pcsc->evt = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_yuv422ycbcr10be_to_yuv422p10le(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                                    unsigned char *buf_dst_c, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_in != NULL, "buf_in is null");
    IMPL_ASSERT(buf_dst_c != NULL, "buf_dst_c is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            const std::size_t dst_offset_y = 0;
            const std::size_t dst_offset_x = 0;
            unsigned short *buf_dst        = (unsigned short *)buf_dst_c;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height, width / 2), [=](sycl::id<2> idx) {
                const auto lmy_dst = idx[0];
                const auto lmx_dst = idx[1] * 2;

                auto y_src     = idx[0];
                auto x0_src    = idx[1] * 5;
                auto x1_src    = x0_src + 1;
                auto x2_src    = x0_src + 2;
                auto x3_src    = x0_src + 3;
                auto x4_src    = x0_src + 4;
                auto src0_byte = src_read_char(buf_in, y_src, x0_src, width * 5 / 2);
                auto src1_byte = src_read_char(buf_in, y_src, x1_src, width * 5 / 2);
                auto src2_byte = src_read_char(buf_in, y_src, x2_src, width * 5 / 2);
                auto src3_byte = src_read_char(buf_in, y_src, x3_src, width * 5 / 2);
                auto src4_byte = src_read_char(buf_in, y_src, x4_src, width * 5 / 2);

                unsigned char t[5] = {
                    0,
                };
                unsigned char *in = t;
                in[4]             = (unsigned char)src4_byte & 0xff;
                in[3]             = (unsigned char)src3_byte & 0xff;
                in[2]             = (unsigned char)src2_byte & 0xff;
                in[1]             = (unsigned char)src1_byte & 0xff;
                in[0]             = (unsigned char)src0_byte & 0xff;

                unsigned long y0 = ((*(in + 1) & 0x3f) << 4) | ((*(in + 2) & 0xf0) >> 4);
                unsigned long y1 = *(in + 4) | ((*(in + 3) & 0x3) << 8);
                unsigned long cb = (*in << 2) | ((*(in + 1) & 0xc0) >> 6);
                unsigned long cr = ((*(in + 2) & 0xf) << 6) | ((*(in + 3) >> 2) & 0x3f);
                dst_write_short(buf_dst, lmy_dst, lmx_dst, width, (unsigned short)(y0 & 0xfff));
                dst_write_short(buf_dst, lmy_dst, (lmx_dst + 1), width, (unsigned short)(y1 & 0xfff));

                const std::size_t u_linear_id =
                    (-dst_offset_y * 3 / 2 + idx[0]) * (width / 2) + idx[1] + dst_offset_y * width + dst_offset_x / 2;
                const std::size_t v_linear_id = u_linear_id + width * height / 2;
                const std::size_t u_x         = u_linear_id % width - dst_offset_x;
                const std::size_t u_y         = u_linear_id / width + height - dst_offset_y;
                const std::size_t v_x         = v_linear_id % width - dst_offset_x;
                const std::size_t v_y         = v_linear_id / width + height - dst_offset_y;
                dst_write_short(buf_dst, u_y, u_x, width, (unsigned short)(cb & 0xfff));
                dst_write_short(buf_dst, v_y, v_x, width, (unsigned short)(cr & 0xfff));
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)pcsc->evt = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_yuv422ycbcr10be_to_yuv420p10le(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                                    unsigned char *buf_dst_c, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_in != NULL, "buf_in is null");
    IMPL_ASSERT(buf_dst_c != NULL, "buf_dst_c is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            // const std::size_t dst_offset_y = 0;
            // const std::size_t dst_offset_x = 0;
            unsigned short *buf_dst  = (unsigned short *)buf_dst_c;
            unsigned short *buf_dstu = buf_dst + width * height;
            unsigned short *buf_dstv = buf_dst + width * height * 5 / 4;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height / 2, width / 2), [=](sycl::id<2> idx) {
                const auto lmy_dst = idx[0] * 2;
                const auto lmx_dst = idx[1] * 2;

                auto y_src     = lmy_dst;
                auto x0_src    = idx[1] * 5;
                auto x1_src    = x0_src + 1;
                auto x2_src    = x0_src + 2;
                auto x3_src    = x0_src + 3;
                auto x4_src    = x0_src + 4;
                auto src0_byte = src_read_char(buf_in, y_src, x0_src, width * 5 / 2);
                auto src1_byte = src_read_char(buf_in, y_src, x1_src, width * 5 / 2);
                auto src2_byte = src_read_char(buf_in, y_src, x2_src, width * 5 / 2);
                auto src3_byte = src_read_char(buf_in, y_src, x3_src, width * 5 / 2);
                auto src4_byte = src_read_char(buf_in, y_src, x4_src, width * 5 / 2);

                unsigned char t[5] = {
                    0,
                };
                unsigned char *in = t;
                in[4]             = (unsigned char)src4_byte & 0xff;
                in[3]             = (unsigned char)src3_byte & 0xff;
                in[2]             = (unsigned char)src2_byte & 0xff;
                in[1]             = (unsigned char)src1_byte & 0xff;
                in[0]             = (unsigned char)src0_byte & 0xff;

                unsigned long y0  = ((*(in + 1) & 0x3f) << 4) | ((*(in + 2) & 0xf0) >> 4);
                unsigned long y1  = *(in + 4) | ((*(in + 3) & 0x3) << 8);
                unsigned long cb0 = (*in << 2) | ((*(in + 1) & 0xc0) >> 6);
                unsigned long cr0 = ((*(in + 2) & 0xf) << 6) | ((*(in + 3) >> 2) & 0x3f);
                dst_write_short(buf_dst, lmy_dst, lmx_dst, width, (unsigned short)(y0 & 0xfff));
                dst_write_short(buf_dst, lmy_dst, (lmx_dst + 1), width, (unsigned short)(y1 & 0xfff));

                y_src += 1;
                src0_byte         = src_read_char(buf_in, y_src, x0_src, width * 5 / 2);
                src1_byte         = src_read_char(buf_in, y_src, x1_src, width * 5 / 2);
                src2_byte         = src_read_char(buf_in, y_src, x2_src, width * 5 / 2);
                src3_byte         = src_read_char(buf_in, y_src, x3_src, width * 5 / 2);
                src4_byte         = src_read_char(buf_in, y_src, x4_src, width * 5 / 2);
                in[4]             = (unsigned char)src4_byte & 0xff;
                in[3]             = (unsigned char)src3_byte & 0xff;
                in[2]             = (unsigned char)src2_byte & 0xff;
                in[1]             = (unsigned char)src1_byte & 0xff;
                in[0]             = (unsigned char)src0_byte & 0xff;
                y0                = ((*(in + 1) & 0x3f) << 4) | ((*(in + 2) & 0xf0) >> 4);
                y1                = *(in + 4) | ((*(in + 3) & 0x3) << 8);
                unsigned long cb1 = (*in << 2) | ((*(in + 1) & 0xc0) >> 6);
                unsigned long cr1 = ((*(in + 2) & 0xf) << 6) | ((*(in + 3) >> 2) & 0x3f);
                dst_write_short(buf_dst, (lmy_dst + 1), lmx_dst, width, (unsigned short)(y0 & 0xfff));
                dst_write_short(buf_dst, (lmy_dst + 1), (lmx_dst + 1), width, (unsigned short)(y1 & 0xfff));

                const std::size_t uv_y = idx[0];
                const std::size_t uv_x = idx[1];
                dst_write_short(buf_dstu, uv_y, uv_x, (width / 2), (unsigned short)(((cb0 + cb1) / 2) & 0xfff));
                dst_write_short(buf_dstv, uv_y, uv_x, (width / 2), (unsigned short)(((cr0 + cr1) / 2) & 0xfff));
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)pcsc->evt = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_yuv422ycbcr10be_to_i420(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst_c, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_in != NULL, "buf_in is null");
    IMPL_ASSERT(buf_dst_c != NULL, "buf_dst_c is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            // const std::size_t dst_offset_y = 0;
            // const std::size_t dst_offset_x = 0;
            unsigned char *buf_dst  = buf_dst_c;
            unsigned char *buf_dstu = buf_dst + width * height;
            unsigned char *buf_dstv = buf_dst + width * height * 5 / 4;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height / 2, width / 2), [=](sycl::id<2> idx) {
                const auto lmy_dst = idx[0] * 2;
                const auto lmx_dst = idx[1] * 2;

                auto y_src     = lmy_dst;
                auto x0_src    = idx[1] * 5;
                auto x1_src    = x0_src + 1;
                auto x2_src    = x0_src + 2;
                auto x3_src    = x0_src + 3;
                auto x4_src    = x0_src + 4;
                auto src0_byte = src_read_char(buf_in, y_src, x0_src, width * 5 / 2);
                auto src1_byte = src_read_char(buf_in, y_src, x1_src, width * 5 / 2);
                auto src2_byte = src_read_char(buf_in, y_src, x2_src, width * 5 / 2);
                auto src3_byte = src_read_char(buf_in, y_src, x3_src, width * 5 / 2);
                auto src4_byte = src_read_char(buf_in, y_src, x4_src, width * 5 / 2);

                unsigned char t[5] = {
                    0,
                };
                unsigned char *in = t;
                in[4]             = (unsigned char)src4_byte & 0xff;
                in[3]             = (unsigned char)src3_byte & 0xff;
                in[2]             = (unsigned char)src2_byte & 0xff;
                in[1]             = (unsigned char)src1_byte & 0xff;
                in[0]             = (unsigned char)src0_byte & 0xff;

                unsigned long y0  = ((*(in + 1) & 0x3f) << 4) | ((*(in + 2) & 0xf0) >> 4);
                unsigned long y1  = *(in + 4) | ((*(in + 3) & 0x3) << 8);
                unsigned long cb0 = (*in << 2) | ((*(in + 1) & 0xc0) >> 6);
                unsigned long cr0 = ((*(in + 2) & 0xf) << 6) | ((*(in + 3) >> 2) & 0x3f);
                dst_write_char(buf_dst, lmy_dst, lmx_dst, width, (unsigned char)((y0 & 0xfff) >> 2));
                dst_write_char(buf_dst, lmy_dst, (lmx_dst + 1), width, (unsigned char)((y1 & 0xfff) >> 2));

                y_src += 1;
                src0_byte         = src_read_char(buf_in, y_src, x0_src, width * 5 / 2);
                src1_byte         = src_read_char(buf_in, y_src, x1_src, width * 5 / 2);
                src2_byte         = src_read_char(buf_in, y_src, x2_src, width * 5 / 2);
                src3_byte         = src_read_char(buf_in, y_src, x3_src, width * 5 / 2);
                src4_byte         = src_read_char(buf_in, y_src, x4_src, width * 5 / 2);
                in[4]             = (unsigned char)src4_byte & 0xff;
                in[3]             = (unsigned char)src3_byte & 0xff;
                in[2]             = (unsigned char)src2_byte & 0xff;
                in[1]             = (unsigned char)src1_byte & 0xff;
                in[0]             = (unsigned char)src0_byte & 0xff;
                y0                = ((*(in + 1) & 0x3f) << 4) | ((*(in + 2) & 0xf0) >> 4);
                y1                = *(in + 4) | ((*(in + 3) & 0x3) << 8);
                unsigned long cb1 = (*in << 2) | ((*(in + 1) & 0xc0) >> 6);
                unsigned long cr1 = ((*(in + 2) & 0xf) << 6) | ((*(in + 3) >> 2) & 0x3f);
                dst_write_char(buf_dst, (lmy_dst + 1), lmx_dst, width, (unsigned char)((y0 & 0xfff) >> 2));
                dst_write_char(buf_dst, (lmy_dst + 1), (lmx_dst + 1), width, (unsigned char)((y1 & 0xfff) >> 2));

                const std::size_t uv_y = idx[0];
                const std::size_t uv_x = idx[1];
                dst_write_char(buf_dstu, uv_y, uv_x, (width / 2), (unsigned char)((((cb0 + cb1) / 2) & 0xfff) >> 2));
                dst_write_char(buf_dstv, uv_y, uv_x, (width / 2), (unsigned char)((((cr0 + cr1) / 2) & 0xfff) >> 2));
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)pcsc->evt = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_yuv422ycbcr10be_to_p010(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst_c, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_in != NULL, "buf_in is null");
    IMPL_ASSERT(buf_dst_c != NULL, "buf_dst_c is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            // const std::size_t dst_offset_y = 0;
            // const std::size_t dst_offset_x = 0;
            unsigned short *buf_dst  = (unsigned short *)buf_dst_c;
            unsigned short *buf_dstu = buf_dst + width * height;
            unsigned short *buf_dstv = buf_dstu + 1;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height / 2, width / 2), [=](sycl::id<2> idx) {
                const auto lmy_dst = idx[0] * 2;
                const auto lmx_dst = idx[1] * 2;

                auto y_src     = lmy_dst;
                auto x0_src    = idx[1] * 5;
                auto x1_src    = x0_src + 1;
                auto x2_src    = x0_src + 2;
                auto x3_src    = x0_src + 3;
                auto x4_src    = x0_src + 4;
                auto src0_byte = src_read_char(buf_in, y_src, x0_src, width * 5 / 2);
                auto src1_byte = src_read_char(buf_in, y_src, x1_src, width * 5 / 2);
                auto src2_byte = src_read_char(buf_in, y_src, x2_src, width * 5 / 2);
                auto src3_byte = src_read_char(buf_in, y_src, x3_src, width * 5 / 2);
                auto src4_byte = src_read_char(buf_in, y_src, x4_src, width * 5 / 2);

                unsigned char t[5] = {
                    0,
                };
                unsigned char *in = t;
                in[4]             = (unsigned char)src4_byte & 0xff;
                in[3]             = (unsigned char)src3_byte & 0xff;
                in[2]             = (unsigned char)src2_byte & 0xff;
                in[1]             = (unsigned char)src1_byte & 0xff;
                in[0]             = (unsigned char)src0_byte & 0xff;

                unsigned long y0  = ((*(in + 1) & 0x3f) << 4) | ((*(in + 2) & 0xf0) >> 4);
                unsigned long y1  = *(in + 4) | ((*(in + 3) & 0x3) << 8);
                unsigned long cb0 = (*in << 2) | ((*(in + 1) & 0xc0) >> 6);
                unsigned long cr0 = ((*(in + 2) & 0xf) << 6) | ((*(in + 3) >> 2) & 0x3f);
                dst_write_short(buf_dst, lmy_dst, lmx_dst, width, (unsigned short)((y0 & 0xfff) << 6));
                dst_write_short(buf_dst, lmy_dst, (lmx_dst + 1), width, (unsigned short)((y1 & 0xfff) << 6));

                y_src += 1;
                src0_byte         = src_read_char(buf_in, y_src, x0_src, width * 5 / 2);
                src1_byte         = src_read_char(buf_in, y_src, x1_src, width * 5 / 2);
                src2_byte         = src_read_char(buf_in, y_src, x2_src, width * 5 / 2);
                src3_byte         = src_read_char(buf_in, y_src, x3_src, width * 5 / 2);
                src4_byte         = src_read_char(buf_in, y_src, x4_src, width * 5 / 2);
                in[4]             = (unsigned char)src4_byte & 0xff;
                in[3]             = (unsigned char)src3_byte & 0xff;
                in[2]             = (unsigned char)src2_byte & 0xff;
                in[1]             = (unsigned char)src1_byte & 0xff;
                in[0]             = (unsigned char)src0_byte & 0xff;
                y0                = ((*(in + 1) & 0x3f) << 4) | ((*(in + 2) & 0xf0) >> 4);
                y1                = *(in + 4) | ((*(in + 3) & 0x3) << 8);
                unsigned long cb1 = (*in << 2) | ((*(in + 1) & 0xc0) >> 6);
                unsigned long cr1 = ((*(in + 2) & 0xf) << 6) | ((*(in + 3) >> 2) & 0x3f);
                dst_write_short(buf_dst, (lmy_dst + 1), lmx_dst, width, (unsigned short)((y0 & 0xfff) << 6));
                dst_write_short(buf_dst, (lmy_dst + 1), (lmx_dst + 1), width, (unsigned short)((y1 & 0xfff) << 6));

                const std::size_t uv_y = idx[0];
                const std::size_t uv_x = idx[1];
                dst_write_short(buf_dstu, uv_y, uv_x * 2, width, (unsigned short)((((cb0 + cb1) / 2) & 0xfff) << 6));
                dst_write_short(buf_dstv, uv_y, uv_x * 2, width, (unsigned short)((((cr0 + cr1) / 2) & 0xfff) << 6));
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)pcsc->evt = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_yuv422ycbcr10le_to_y210(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst_c, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_in != NULL, "buf_in is null");
    IMPL_ASSERT(buf_dst_c != NULL, "buf_dst_c is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            unsigned short *buf_dst = (unsigned short *)buf_dst_c;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height, width / 2), [=](sycl::id<2> idx) {
                const auto lmy_dst = idx[0];
                const auto lmx_dst = idx[1] * 4;

                auto y_src      = idx[0];
                auto x0_src     = idx[1] * 5;
                auto x1_src     = x0_src + 1;
                auto x2_src     = x0_src + 2;
                auto x3_src     = x0_src + 3;
                auto x4_src     = x0_src + 4;
                auto src0_pixel = src_read_char(buf_in, y_src, x0_src, width * 5 / 2);
                auto src1_pixel = src_read_char(buf_in, y_src, x1_src, width * 5 / 2);
                auto src2_pixel = src_read_char(buf_in, y_src, x2_src, width * 5 / 2);
                auto src3_pixel = src_read_char(buf_in, y_src, x3_src, width * 5 / 2);
                auto src4_pixel = src_read_char(buf_in, y_src, x4_src, width * 5 / 2);

                unsigned long p = (unsigned long)src4_pixel & 0xff;
                p               = (p << 8) | ((unsigned long)src3_pixel & 0xff);
                p               = (p << 8) | ((unsigned long)src2_pixel & 0xff);
                p               = (p << 8) | ((unsigned long)src1_pixel & 0xff);
                p               = (p << 8) | ((unsigned long)src0_pixel & 0xff);

                unsigned short cb = (p & 0x3ff);
                unsigned short y0 = (p >> 10) & 0x3ff;
                unsigned short cr = (p >> 20) & 0x3ff;
                unsigned short y1 = (p >> 30) & 0x3ff;

                dst_write_short(buf_dst, lmy_dst, lmx_dst, width * 2, ((unsigned int)y0 << 6) & 0xffff);
                dst_write_short(buf_dst, lmy_dst, lmx_dst + 1, width * 2, ((unsigned int)cb << 6) & 0xffff);
                dst_write_short(buf_dst, lmy_dst, lmx_dst + 2, width * 2, ((unsigned int)y1 << 6) & 0xffff);
                dst_write_short(buf_dst, lmy_dst, lmx_dst + 3, width * 2, ((unsigned int)cr << 6) & 0xffff);
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)pcsc->evt = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

IMPL_STATUS impl_csc_yuv422ycbcr10le_to_v210(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst_c, void *dep_evt) {
    IMPL_ASSERT(pcsc->pq != NULL, "pcsc->pq is null");
    IMPL_ASSERT(buf_in != NULL, "buf_in_c is null");
    IMPL_ASSERT(buf_dst_c != NULL, "buf_dst_c is null");
    queue q         = *(queue *)(pcsc->pq);
    int width       = pcsc->width;
    int height      = pcsc->height;
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        try {
            unsigned int *buf_dst = (unsigned int *)buf_dst_c;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(height, width / 6), [=](sycl::id<2> idx) {
                const auto y_dst = idx[0];
                const auto x_dst = idx[1] * 4;

                auto y_src          = idx[0];
                auto x_src          = idx[1] * 15;
                auto src0_pixel     = src_read_char(buf_in, y_src, x_src, width * 5 / 2);
                auto src1_pixel     = src_read_char(buf_in, y_src, x_src + 1, width * 5 / 2);
                auto src2_pixel     = src_read_char(buf_in, y_src, x_src + 2, width * 5 / 2);
                auto src3_pixel     = src_read_char(buf_in, y_src, x_src + 3, width * 5 / 2);
                auto src4_pixel     = src_read_char(buf_in, y_src, x_src + 4, width * 5 / 2);
                unsigned long p     = (unsigned long)src4_pixel & 0xff;
                p                   = (p << 8) | ((unsigned long)src3_pixel & 0xff);
                p                   = (p << 8) | ((unsigned long)src2_pixel & 0xff);
                p                   = (p << 8) | ((unsigned long)src1_pixel & 0xff);
                p                   = (p << 8) | ((unsigned long)src0_pixel & 0xff);
                unsigned int cb0    = (p & 0x3ff);
                unsigned int lm0    = (p >> 10) & 0x3ff;
                unsigned int cr0    = (p >> 20) & 0x3ff;
                unsigned int lm1    = (p >> 30) & 0x3ff;
                unsigned int pixel0 = cb0 | lm0 << 10 | cr0 << 20;
                dst_write_int(buf_dst, y_dst, x_dst, width * 2 / 3, pixel0);

                auto src5_pixel     = src_read_char(buf_in, y_src, x_src + 5, width * 5 / 2);
                auto src6_pixel     = src_read_char(buf_in, y_src, x_src + 6, width * 5 / 2);
                auto src7_pixel     = src_read_char(buf_in, y_src, x_src + 7, width * 5 / 2);
                auto src8_pixel     = src_read_char(buf_in, y_src, x_src + 8, width * 5 / 2);
                auto src9_pixel     = src_read_char(buf_in, y_src, x_src + 9, width * 5 / 2);
                p                   = (unsigned long)src9_pixel & 0xff;
                p                   = (p << 8) | ((unsigned long)src8_pixel & 0xff);
                p                   = (p << 8) | ((unsigned long)src7_pixel & 0xff);
                p                   = (p << 8) | ((unsigned long)src6_pixel & 0xff);
                p                   = (p << 8) | ((unsigned long)src5_pixel & 0xff);
                unsigned int cb1    = (p & 0x3ff);
                unsigned int lm2    = (p >> 10) & 0x3ff;
                unsigned int cr1    = (p >> 20) & 0x3ff;
                unsigned int lm3    = (p >> 30) & 0x3ff;
                unsigned int pixel1 = lm1 | cb1 << 10 | lm2 << 20;
                dst_write_int(buf_dst, y_dst, (x_dst + 1), width * 2 / 3, pixel1);

                auto src10_pixel    = src_read_char(buf_in, y_src, x_src + 10, width * 5 / 2);
                auto src11_pixel    = src_read_char(buf_in, y_src, x_src + 11, width * 5 / 2);
                auto src12_pixel    = src_read_char(buf_in, y_src, x_src + 12, width * 5 / 2);
                auto src13_pixel    = src_read_char(buf_in, y_src, x_src + 13, width * 5 / 2);
                auto src14_pixel    = src_read_char(buf_in, y_src, x_src + 14, width * 5 / 2);
                p                   = (unsigned long)src14_pixel & 0xff;
                p                   = (p << 8) | ((unsigned long)src13_pixel & 0xff);
                p                   = (p << 8) | ((unsigned long)src12_pixel & 0xff);
                p                   = (p << 8) | ((unsigned long)src11_pixel & 0xff);
                p                   = (p << 8) | ((unsigned long)src10_pixel & 0xff);
                unsigned int cb2    = (p & 0x3ff);
                unsigned int lm4    = (p >> 10) & 0x3ff;
                unsigned int cr2    = (p >> 20) & 0x3ff;
                unsigned int lm5    = (p >> 30) & 0x3ff;
                unsigned int pixel2 = cr1 | lm3 << 10 | cb2 << 20;
                dst_write_int(buf_dst, y_dst, (x_dst + 2), width * 2 / 3, pixel2);

                unsigned int pixel3 = lm4 | cr2 << 10 | lm5 << 20;
                dst_write_int(buf_dst, y_dst, (x_dst + 3), width * 2 / 3, pixel3);
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)pcsc->evt = event;
    if (pcsc->is_async == 0) {
        event.wait();
    }
    return ret;
}

enum CSC_function_index {
    index_y210_to_v210 = 0,
    index_v210_to_y210,
    index_yuv422p10le_to_v210,
    index_v210_to_yuv422p10le,
    index_yuv422p10le_to_y210,
    index_y210_to_yuv422p10le,
    index_nv12_to_yuv422ycbcr10be,
    index_yuv422ycbcr10be_to_nv12,
    index_yuv422ycbcr10be_to_v210,
    index_v210_to_yuv422ycbcr10be,
    index_yuv422ycbcr10be_to_y210,
    index_y210_to_yuv422ycbcr10be,
    index_yuv422ycbcr10be_to_yuv422p10le,
    index_yuv422ycbcr10be_to_yuv420p10le,
    index_yuv422ycbcr10be_to_i420,
    index_yuv422ycbcr10be_to_p010,
    index_yuv422p10le_to_yuv422ycbcr10be,
    index_yuv422ycbcr10le_to_v210,
    index_v210_to_yuv422ycbcr10le,
    index_yuv422ycbcr10le_to_y210,
    index_y210_to_yuv422ycbcr10le,
    MAX_CSC_FUNCTION_NUM
};

typedef IMPL_STATUS (*CSC_function)(struct impl_csc_params *csc_params, unsigned char *buf_in, unsigned char *buf_out,
                                    void *dep_evt);
CSC_function cscfunction[MAX_CSC_FUNCTION_NUM] = {impl_csc_y210_to_v210,
                                                  impl_csc_v210_to_y210,
                                                  impl_csc_yuv422p10le_to_v210,
                                                  impl_csc_v210_to_yuv422p10le,
                                                  impl_csc_yuv422p10le_to_y210,
                                                  impl_csc_y210_to_yuv422p10le,
                                                  impl_csc_nv12_to_yuv422ycbcr10be,
                                                  impl_csc_yuv422ycbcr10be_to_nv12,
                                                  impl_csc_yuv422ycbcr10be_to_v210,
                                                  impl_csc_v210_to_yuv422ycbcr10be,
                                                  impl_csc_yuv422ycbcr10be_to_y210,
                                                  impl_csc_y210_to_yuv422ycbcr10be,
                                                  impl_csc_yuv422ycbcr10be_to_yuv422p10le,
                                                  impl_csc_yuv422ycbcr10be_to_yuv420p10le,
                                                  impl_csc_yuv422ycbcr10be_to_i420,
                                                  impl_csc_yuv422ycbcr10be_to_p010,
                                                  impl_csc_yuv422p10le_to_yuv422ycbcr10be,
                                                  impl_csc_yuv422ycbcr10le_to_v210,
                                                  impl_csc_v210_to_yuv422ycbcr10le,
                                                  impl_csc_yuv422ycbcr10le_to_y210,
                                                  impl_csc_y210_to_yuv422ycbcr10le};

IMPL_API IMPL_STATUS impl_csc_init(struct impl_csc_params *pcsc, void *&pcsc_context) {
    IMPL_ASSERT(pcsc != NULL, "csc init failed, pcsc is null");
    IMPL_STATUS ret            = IMPL_STATUS_SUCCESS;
    pcsc->evt                  = impl_common_new_event();
    impl_csc_context *pcontext = new impl_csc_context;
    memset(pcontext, 0, sizeof(impl_csc_context));
    pcsc_context = (void *)pcontext;

    if (pcsc->in_format == IMPL_VIDEO_YUV422YCBCR10BE) {
        switch (pcsc->out_format) {
        case IMPL_VIDEO_YUV422P10LE:
            pcontext->csc_func_index = index_yuv422ycbcr10be_to_yuv422p10le;
            break;
        case IMPL_VIDEO_I420:
            pcontext->csc_func_index = index_yuv422ycbcr10be_to_i420;
            break;
        case IMPL_VIDEO_NV12:
            pcontext->csc_func_index = index_yuv422ycbcr10be_to_nv12;
            break;
        case IMPL_VIDEO_V210:
            if ((pcsc->width % 48) != 0 || (pcsc->height % 2) != 0) {
                err("%s, Illegal V210 size: The width must be a multiple of 48, and the height "
                    "must be a multiple of 2\n",
                    __func__);
                ret = IMPL_STATUS_INVALID_PARAMS;
            }
            pcontext->csc_func_index = index_yuv422ycbcr10be_to_v210;
            break;
        case IMPL_VIDEO_Y210:
            pcontext->csc_func_index = index_yuv422ycbcr10be_to_y210;
            break;
        case IMPL_VIDEO_P010:
            pcontext->csc_func_index = index_yuv422ycbcr10be_to_p010;
            break;
        default:
            err("%s unsuported output format %d for input %d\n", __func__, pcsc->out_format, pcsc->in_format);
            ret = IMPL_STATUS_INVALID_PARAMS;
            break;
        }
    } else if (pcsc->in_format == IMPL_VIDEO_YUV422YCBCR10LE) {
        switch (pcsc->out_format) {
        case IMPL_VIDEO_V210:
            if ((pcsc->width % 48) != 0 || (pcsc->height % 2) != 0) {
                err("%s, Illegal V210 size: The width must be a multiple of 48, and the height "
                    "must be a multiple of 2\n",
                    __func__);
                ret = IMPL_STATUS_INVALID_PARAMS;
            }
            pcontext->csc_func_index = index_yuv422ycbcr10le_to_v210;
            break;
        case IMPL_VIDEO_Y210:
            pcontext->csc_func_index = index_yuv422ycbcr10le_to_y210;
            break;
        default:
            err("%s unsuported output format %d for input %d\n", __func__, pcsc->out_format, pcsc->in_format);
            ret = IMPL_STATUS_INVALID_PARAMS;
            break;
        }
    } else if (pcsc->in_format == IMPL_VIDEO_YUV422P10LE) {
        switch (pcsc->out_format) {
        case IMPL_VIDEO_YUV422YCBCR10BE:
            pcontext->csc_func_index = index_yuv422p10le_to_yuv422ycbcr10be;
            break;
        case IMPL_VIDEO_V210:
            if ((pcsc->width % 48) != 0 || (pcsc->height % 2) != 0) {
                err("%s, Illegal V210 size: The width must be a multiple of 48, and the height "
                    "must be a multiple of 2\n",
                    __func__);
                ret = IMPL_STATUS_INVALID_PARAMS;
            }
            pcontext->csc_func_index = index_yuv422p10le_to_v210;
            break;
        case IMPL_VIDEO_Y210:
            pcontext->csc_func_index = index_yuv422p10le_to_y210;
            break;
        default:
            err("%s unsuported output format %d for input %d\n", __func__, pcsc->out_format, pcsc->in_format);
            ret = IMPL_STATUS_INVALID_PARAMS;
            break;
        }
    } else if (pcsc->in_format == IMPL_VIDEO_NV12) {
        switch (pcsc->out_format) {
        case IMPL_VIDEO_YUV422YCBCR10BE:
            pcontext->csc_func_index = index_nv12_to_yuv422ycbcr10be;
            break;
        default:
            err("%s unsuported output format %d for input %d\n", __func__, pcsc->out_format, pcsc->in_format);
            ret = IMPL_STATUS_INVALID_PARAMS;
            break;
        }
    } else if (pcsc->in_format == IMPL_VIDEO_V210) {
        if ((pcsc->width % 48) != 0 || (pcsc->height % 2) != 0) {
            err("%s, Illegal V210 size: The width must be a multiple of 48, and the height "
                "must be a multiple of 2\n",
                __func__);
            ret = IMPL_STATUS_INVALID_PARAMS;
        }
        switch (pcsc->out_format) {
        case IMPL_VIDEO_YUV422YCBCR10BE:
            pcontext->csc_func_index = index_v210_to_yuv422ycbcr10be;
            break;
        case IMPL_VIDEO_YUV422YCBCR10LE:
            pcontext->csc_func_index = index_v210_to_yuv422ycbcr10le;
            break;
        case IMPL_VIDEO_YUV422P10LE:
            pcontext->csc_func_index = index_v210_to_yuv422p10le;
            break;
        case IMPL_VIDEO_Y210:
            pcontext->csc_func_index = index_v210_to_y210;
            break;
        default:
            err("%s unsuported output format %d for input %d\n", __func__, pcsc->out_format, pcsc->in_format);
            ret = IMPL_STATUS_INVALID_PARAMS;
            break;
        }
    } else if (pcsc->in_format == IMPL_VIDEO_Y210) {
        switch (pcsc->out_format) {
        case IMPL_VIDEO_YUV422YCBCR10BE:
            pcontext->csc_func_index = index_y210_to_yuv422ycbcr10be;
            break;
        case IMPL_VIDEO_YUV422YCBCR10LE:
            pcontext->csc_func_index = index_y210_to_yuv422ycbcr10le;
            break;
        case IMPL_VIDEO_YUV422P10LE:
            pcontext->csc_func_index = index_y210_to_yuv422p10le;
            break;
        case IMPL_VIDEO_V210:
            if ((pcsc->width % 48) != 0 || (pcsc->height % 2) != 0) {
                err("%s, Illegal V210 size: The width must be a multiple of 48, and the height "
                    "must be a multiple of 2\n",
                    __func__);
                ret = IMPL_STATUS_INVALID_PARAMS;
            }
            pcontext->csc_func_index = index_y210_to_v210;
            break;
        default:
            err("%s unsuported output format %d for input %d\n", __func__, pcsc->out_format, pcsc->in_format);
            ret = IMPL_STATUS_INVALID_PARAMS;
            break;
        }
    } else {
        err("%s unsuported input format %d\n", __func__, pcsc->in_format);
        ret = IMPL_STATUS_INVALID_PARAMS;
    }
    return ret;
}

IMPL_STATUS impl_csc_uninit(struct impl_csc_params *pcsc, void *pcsc_context) {
    IMPL_ASSERT(pcsc != NULL, "csc uninit fail, pcsc is null");
    IMPL_ASSERT(pcsc_context != NULL, "csc uninit failed, pcsc_context is null");
    delete (event *)pcsc->evt;
    struct impl_csc_context *pcontext = (struct impl_csc_context *)pcsc_context;
    delete pcontext;
    return IMPL_STATUS_SUCCESS;
}

IMPL_STATUS impl_csc_run(struct impl_csc_params *pcsc, void *pcsc_context, unsigned char *buf_in,
                         unsigned char *buf_dst, void *dep_evt) {
    IMPL_ASSERT(pcsc != NULL, "csc run fail, pcsc is null");
    IMPL_ASSERT(pcsc_context != NULL, "csc uninit failed, pcsc_context is null");
    IMPL_STATUS ret                   = IMPL_STATUS_SUCCESS;
    struct impl_csc_context *pcontext = (struct impl_csc_context *)pcsc_context;
    ret                               = cscfunction[pcontext->csc_func_index](pcsc, buf_in, buf_dst, dep_evt);
    IMPL_ASSERT(ret >= 0, "csc_function failed");
    return ret;
}
