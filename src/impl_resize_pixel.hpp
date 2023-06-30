/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */
#pragma once

#include "impl_resize.hpp"
#include "impl_trace.hpp"

template <impl_interp_mtd INTERP_METHOD>
IMPL_STATUS impl_resize_i420(struct impl_resize_params *prs, impl_resize_context *prst, unsigned char *src_ptr,
                             unsigned char *dst_ptr, void *dep_evt) {
    (void)prst;
    IMPL_ASSERT(prs->pq != NULL, "queue is null");
    IMPL_ASSERT(src_ptr != NULL, "src_ptr is null");
    IMPL_ASSERT(dst_ptr != NULL, "dst_ptr is null");
    uint32_t src_width  = prs->src_width;
    uint32_t src_height = prs->src_height;
    uint32_t dst_width  = prs->dst_width;
    uint32_t dst_height = prs->dst_height;
    float coef_width    = (float)prs->src_width / (float)prs->dst_width;
    float coef_height   = (float)prs->src_height / (float)prs->dst_height;

    queue q         = *(queue *)(prs->pq);
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        const uint32_t pitch    = prs->pitch_pixel;
        const uint32_t height   = prs->surface_height;
        const uint32_t offset_x = prs->offset_x;
        const uint32_t offset_y = prs->offset_y;

        uint32_t src_u_offset = src_width * src_height;
        uint32_t dst_u_offset = pitch * height;
        uint32_t src_v_offset = src_u_offset * 5 / 4;
        uint32_t dst_v_offset = dst_u_offset * 5 / 4;

        try {
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range(dst_height / 2, dst_width / 2), [=](sycl::item<2> item) {
                constexpr int datasize = GetDataSize<INTERP_METHOD>();
                uint32_t sx            = 2 * item[1];
                uint32_t sy            = 2 * item[0];

                auto readlm = [&src_ptr, &src_width](uint32_t y, uint32_t x) {
                    return src_ptr[mad24(y, src_width, x)];
                };
                uint32_t offset = mad24(sy + offset_y, pitch, +sx + offset_x);

                float yw[datasize];
                uint32_t yid[datasize];

                // Y0 Y1
                get_idw<INTERP_METHOD>(sy, coef_height, src_height, yid, yw);
#pragma unroll
                for (uint32_t i = 0; i < 2; i++) {
                    float xw[datasize];
                    uint32_t xid[datasize];
                    float value;
                    get_idw<INTERP_METHOD>(sx + i, coef_width, src_width, xid, xw);
                    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                        value = pixel_interp_bilinear(xid, xw, yid, yw, readlm);
                    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                        value = pixel_interp_bicubic(xid, xw, yid, yw, readlm);
                    dst_ptr[offset + i] = (unsigned char)(value + (float)0.5);
                }

                // next line Y0 Y1
                get_idw<INTERP_METHOD>(sy + 1, coef_height, src_height, yid, yw);
#pragma unroll
                for (uint32_t i = 0; i < 2; i++) {
                    float xw[datasize];
                    uint32_t xid[datasize];
                    float value;
                    get_idw<INTERP_METHOD>(sx + i, coef_width, src_width, xid, xw);

                    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                        value = pixel_interp_bilinear(xid, xw, yid, yw, readlm);
                    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                        value = pixel_interp_bicubic(xid, xw, yid, yw, readlm);
                    dst_ptr[offset + i + pitch] = (unsigned char)(value + (float)0.5);
                }

                // UV
                {
                    auto readcr = [&src_ptr, &src_width, &src_u_offset](uint32_t y, uint32_t x) {
                        return src_ptr[mad24(y, src_width / 2, x + src_u_offset)];
                    };
                    auto readcb = [&src_ptr, &src_width, &src_v_offset](uint32_t y, uint32_t x) {
                        return src_ptr[mad24(y, src_width / 2, x + src_v_offset)];
                    };

                    uint32_t uv_offset = mad24(sy / 2 + offset_y / 2, pitch / 2, sx / 2 + offset_x / 2);

                    float xw[datasize];
                    uint32_t xid[datasize];
                    float valuecr, valuecb;
                    get_idw<INTERP_METHOD>(sy / 2, coef_height, src_height / 2, yid, yw);
                    get_idw<INTERP_METHOD>(sx / 2, coef_width, src_width / 2, xid, xw);

                    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                        valuecr = pixel_interp_bilinear(xid, xw, yid, yw, readcr);
                    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                        valuecr = pixel_interp_bicubic(xid, xw, yid, yw, readcr);
                    dst_ptr[dst_u_offset + uv_offset] = (unsigned char)(valuecr + (float)0.5);

                    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                        valuecb = pixel_interp_bilinear(xid, xw, yid, yw, readcb);
                    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                        valuecb = pixel_interp_bicubic(xid, xw, yid, yw, readcb);
                    dst_ptr[dst_v_offset + uv_offset] = (unsigned char)(valuecb + (float)0.5);
                }
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)prs->evt = event;
    if (prs->is_async == 0)
        event.wait();
    return ret;
}

template <impl_interp_mtd INTERP_METHOD>
IMPL_STATUS impl_resize_yuv420p10le(struct impl_resize_params *prs, impl_resize_context *prst, unsigned char *src_ptr_c,
                                    unsigned char *dst_ptr_c, void *dep_evt) {
    (void)prst;
    IMPL_ASSERT(prs->pq != NULL, "queue is null");
    IMPL_ASSERT(src_ptr_c != NULL, "src_ptr is null");
    IMPL_ASSERT(dst_ptr_c != NULL, "dst_ptr is null");
    uint32_t src_width  = prs->src_width;
    uint32_t src_height = prs->src_height;
    uint32_t dst_width  = prs->dst_width;
    uint32_t dst_height = prs->dst_height;
    float coef_width    = (float)prs->src_width / (float)prs->dst_width;
    float coef_height   = (float)prs->src_height / (float)prs->dst_height;

    queue q         = *(queue *)(prs->pq);
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        const uint32_t pitch    = prs->pitch_pixel;
        const uint32_t height   = prs->surface_height;
        const uint32_t offset_x = prs->offset_x;
        const uint32_t offset_y = prs->offset_y;

        uint32_t src_u_offset = src_width * src_height;
        uint32_t dst_u_offset = pitch * height;
        uint32_t src_v_offset = src_u_offset * 5 / 4;
        uint32_t dst_v_offset = dst_u_offset * 5 / 4;

        unsigned short *src_ptr = (unsigned short *)src_ptr_c;
        unsigned short *dst_ptr = (unsigned short *)dst_ptr_c;

        try {
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range(dst_height / 2, dst_width / 2), [=](sycl::item<2> item) {
                constexpr int datasize = GetDataSize<INTERP_METHOD>();
                uint32_t sx            = 2 * item[1];
                uint32_t sy            = 2 * item[0];

                auto readlm = [&src_ptr, &src_width](uint32_t y, uint32_t x) {
                    return src_ptr[mad24(y, src_width, x)];
                };
                uint32_t offset = mad24(sy + offset_y, pitch, +sx + offset_x);

                float yw[datasize];
                uint32_t yid[datasize];

                // Y0 Y1
                get_idw<INTERP_METHOD>(sy, coef_height, src_height, yid, yw);
#pragma unroll
                for (uint32_t i = 0; i < 2; i++) {
                    float xw[datasize];
                    uint32_t xid[datasize];
                    float value;
                    get_idw<INTERP_METHOD>(sx + i, coef_width, src_width, xid, xw);

                    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                        value = pixel_interp_bilinear(xid, xw, yid, yw, readlm);
                    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                        value = pixel_interp_bicubic(xid, xw, yid, yw, readlm);
                    dst_ptr[offset + i] = (unsigned short)(value + (float)0.5);
                }

                // next line Y0 Y1
                get_idw<INTERP_METHOD>(sy + 1, coef_height, src_height, yid, yw);
#pragma unroll
                for (uint32_t i = 0; i < 2; i++) {
                    float xw[datasize];
                    uint32_t xid[datasize];
                    float value;
                    get_idw<INTERP_METHOD>(sx + i, coef_width, src_width, xid, xw);

                    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                        value = pixel_interp_bilinear(xid, xw, yid, yw, readlm);
                    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                        value = pixel_interp_bicubic(xid, xw, yid, yw, readlm);
                    dst_ptr[offset + i + pitch] = (unsigned short)(value + (float)0.5);
                }

                // UV
                {
                    auto readcr = [&src_ptr, &src_width, &src_u_offset](uint32_t y, uint32_t x) {
                        return src_ptr[mad24(y, src_width / 2, x + src_u_offset)];
                    };
                    auto readcb = [&src_ptr, &src_width, &src_v_offset](uint32_t y, uint32_t x) {
                        return src_ptr[mad24(y, src_width / 2, x + src_v_offset)];
                    };

                    uint32_t uv_offset = mad24(sy / 2 + offset_y / 2, pitch / 2, sx / 2 + offset_x / 2);

                    float xw[datasize];
                    uint32_t xid[datasize];
                    float valuecr, valuecb;
                    get_idw<INTERP_METHOD>(sy / 2, coef_height, src_height / 2, yid, yw);
                    get_idw<INTERP_METHOD>(sx / 2, coef_width, src_width / 2, xid, xw);

                    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                        valuecr = pixel_interp_bilinear(xid, xw, yid, yw, readcr);
                    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                        valuecr = pixel_interp_bicubic(xid, xw, yid, yw, readcr);
                    dst_ptr[dst_u_offset + uv_offset] = (unsigned short)(valuecr + (float)0.5);

                    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                        valuecb = pixel_interp_bilinear(xid, xw, yid, yw, readcb);
                    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                        valuecb = pixel_interp_bicubic(xid, xw, yid, yw, readcb);
                    dst_ptr[dst_v_offset + uv_offset] = (unsigned short)(valuecb + (float)0.5);
                }
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });
    *(sycl::event *)prs->evt = event;
    if (prs->is_async == 0)
        event.wait();
    return ret;
}

template <impl_interp_mtd INTERP_METHOD>
IMPL_STATUS impl_resize_p010(struct impl_resize_params *prs, impl_resize_context *prst, unsigned char *src_ptr_c,
                             unsigned char *dst_ptr_c, void *dep_evt) {
    (void)prst;
    IMPL_ASSERT(prs->pq != NULL, "queue is null");
    IMPL_ASSERT(src_ptr_c != NULL, "src_ptr is null");
    IMPL_ASSERT(dst_ptr_c != NULL, "dst_ptr is null");
    uint32_t src_width  = prs->src_width;
    uint32_t src_height = prs->src_height;
    uint32_t dst_width  = prs->dst_width;
    uint32_t dst_height = prs->dst_height;
    float coef_width    = (float)prs->src_width / (float)prs->dst_width;
    float coef_height   = (float)prs->src_height / (float)prs->dst_height;

    queue q         = *(queue *)(prs->pq);
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        const uint32_t pitch    = prs->pitch_pixel;
        const uint32_t height   = prs->surface_height;
        const uint32_t offset_x = prs->offset_x;
        const uint32_t offset_y = prs->offset_y;

        uint32_t src_offset = src_width * src_height;
        uint32_t dst_offset = pitch * height;

        unsigned short *src_ptr = (unsigned short *)src_ptr_c;
        unsigned short *dst_ptr = (unsigned short *)dst_ptr_c;

        try {
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range(dst_height / 2, dst_width / 2), [=](sycl::item<2> item) {
                constexpr int datasize = GetDataSize<INTERP_METHOD>();
                uint32_t sx            = 2 * item[1];
                uint32_t sy            = 2 * item[0];

                auto readlm = [&src_ptr, &src_width](uint32_t y, uint32_t x) {
                    return (src_ptr[mad24(y, src_width, x)] >> 6);
                };
                uint32_t offset = mad24(sy + offset_y, pitch, +sx + offset_x);

                float yw[datasize];
                uint32_t yid[datasize];

                // Y0 Y1
                get_idw<INTERP_METHOD>(sy, coef_height, src_height, yid, yw);
#pragma unroll
                for (uint32_t i = 0; i < 2; i++) {
                    float xw[datasize];
                    uint32_t xid[datasize];
                    float value;
                    get_idw<INTERP_METHOD>(sx + i, coef_width, src_width, xid, xw);

                    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                        value = pixel_interp_bilinear(xid, xw, yid, yw, readlm);
                    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                        value = pixel_interp_bicubic(xid, xw, yid, yw, readlm);
                    dst_ptr[offset + i] = ((unsigned short)(value + (float)0.5) << 6);
                }

                // next line Y0 Y1
                get_idw<INTERP_METHOD>(sy + 1, coef_height, src_height, yid, yw);
#pragma unroll
                for (uint32_t i = 0; i < 2; i++) {
                    float xw[datasize];
                    uint32_t xid[datasize];
                    float value;
                    get_idw<INTERP_METHOD>(sx + i, coef_width, src_width, xid, xw);

                    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                        value = pixel_interp_bilinear(xid, xw, yid, yw, readlm);
                    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                        value = pixel_interp_bicubic(xid, xw, yid, yw, readlm);
                    dst_ptr[offset + i + pitch] = ((unsigned short)(value + (float)0.5) << 6);
                }

                // UV
                {
                    auto readcr = [&src_ptr, &src_width, &src_offset](uint32_t y, uint32_t x) {
                        return (src_ptr[mad24(y, src_width, x * 2 + src_offset)] >> 6);
                    };
                    auto readcb = [&src_ptr, &src_width, &src_offset](uint32_t y, uint32_t x) {
                        return (src_ptr[mad24(y, src_width, x * 2 + src_offset + 1)] >> 6);
                    };

                    uint32_t uv_offset = dst_offset + mad24(sy / 2 + offset_y / 2, pitch, sx + offset_x);

                    float xw[datasize];
                    uint32_t xid[datasize];
                    float valuecr, valuecb;
                    get_idw<INTERP_METHOD>(sy / 2, coef_height, src_height / 2, yid, yw);
                    get_idw<INTERP_METHOD>(sx / 2, coef_width, src_width / 2, xid, xw);

                    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                        valuecr = pixel_interp_bilinear(xid, xw, yid, yw, readcr);
                    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                        valuecr = pixel_interp_bicubic(xid, xw, yid, yw, readcr);
                    dst_ptr[uv_offset] = ((unsigned short)(valuecr + (float)0.5) << 6);

                    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                        valuecb = pixel_interp_bilinear(xid, xw, yid, yw, readcb);
                    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                        valuecb = pixel_interp_bicubic(xid, xw, yid, yw, readcb);
                    dst_ptr[uv_offset + 1] = ((unsigned short)(valuecb + (float)0.5) << 6);
                }
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)prs->evt = event;
    if (prs->is_async == 0)
        event.wait();
    return ret;
}

template <impl_interp_mtd INTERP_METHOD>
IMPL_STATUS impl_resize_yuv422ycbcr10be(struct impl_resize_params *prs, impl_resize_context *prst,
                                        unsigned char *src_ptr, unsigned char *dst_ptr, void *dep_evt) {
    (void)prst;
    IMPL_ASSERT(prs->pq != NULL, "queue is null");
    IMPL_ASSERT(src_ptr != NULL, "src_ptr is null");
    IMPL_ASSERT(dst_ptr != NULL, "dst_ptr is null");
    uint32_t src_width  = prs->src_width;
    uint32_t src_height = prs->src_height;
    uint32_t dst_width  = prs->dst_width;
    uint32_t dst_height = prs->dst_height;
    float coef_width    = (float)prs->src_width / (float)prs->dst_width;
    float coef_height   = (float)prs->src_height / (float)prs->dst_height;

    queue q         = *(queue *)(prs->pq);
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        const uint32_t pitch    = prs->pitch_pixel * 5 / 2;
        const uint32_t offset_x = prs->offset_x;
        const uint32_t offset_y = prs->offset_y;

        uint32_t pitch_src = src_width * 5 / 2;
        try {
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range(dst_height, dst_width / 2), [=](sycl::item<2> item) {
                constexpr int datasize = GetDataSize<INTERP_METHOD>();
                uint32_t sx            = 2 * item[1];
                uint32_t sy            = item[0];

                float dst_lm[2];
                float dst_cb;
                float dst_cr;

                auto readlm = [&src_ptr, &pitch_src](uint32_t y, uint32_t x) {
                    return read_yuv422ycbcr10be_lm(x, y, src_ptr, pitch_src);
                };

                float yw[datasize];
                uint32_t yid[datasize];

                // Y0 Y1
                get_idw<INTERP_METHOD>(sy, coef_height, src_height, yid, yw);
#pragma unroll
                for (uint32_t i = 0; i < 2; i++) {
                    float xw[datasize];
                    uint32_t xid[datasize];
                    get_idw<INTERP_METHOD>(sx + i, coef_width, src_width, xid, xw);

                    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                        dst_lm[i] = pixel_interp_bilinear(xid, xw, yid, yw, readlm);
                    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                        dst_lm[i] = pixel_interp_bicubic(xid, xw, yid, yw, readlm);
                }

                // UV
                {
                    auto readcr = [&src_ptr, &pitch_src](uint32_t y, uint32_t x) {
                        return read_yuv422ycbcr10be_cr(x, y, src_ptr, pitch_src);
                    };
                    auto readcb = [&src_ptr, &pitch_src](uint32_t y, uint32_t x) {
                        return read_yuv422ycbcr10be_cb(x, y, src_ptr, pitch_src);
                    };

                    float xw[datasize];
                    uint32_t xid[datasize];
                    get_idw<INTERP_METHOD>(sx / 2, coef_width, src_width / 2, xid, xw);

                    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                        dst_cb = pixel_interp_bilinear(xid, xw, yid, yw, readcb);
                    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                        dst_cb = pixel_interp_bicubic(xid, xw, yid, yw, readcb);

                    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                        dst_cr = pixel_interp_bilinear(xid, xw, yid, yw, readcr);
                    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                        dst_cr = pixel_interp_bicubic(xid, xw, yid, yw, readcr);
                }

                unsigned long cb = (unsigned long)(dst_cb + (float)0.5);
                unsigned long y0 = (unsigned long)(dst_lm[0] + (float)0.5);
                unsigned long cr = (unsigned long)(dst_cr + (float)0.5);
                unsigned long y1 = (unsigned long)(dst_lm[1] + (float)0.5);

                unsigned long out0 = ((cb >> 2) & 0xff);
                unsigned long out1 = ((cb & 0x3) << 6) | ((y0 >> 4) & 0x3f);
                unsigned long out2 = ((y0 & 0xf) << 4) | ((cr >> 6) & 0xf);
                unsigned long out3 = ((cr & 0x3f) << 2) | ((y1 >> 8) & 0x3);
                unsigned long out4 = (y1 & 0xff);

                uint32_t offset     = mad24(sy + offset_y, pitch, (sx + offset_x) / 2 * 5);
                dst_ptr[offset]     = (unsigned char)out0;
                dst_ptr[offset + 1] = (unsigned char)out1;
                dst_ptr[offset + 2] = (unsigned char)out2;
                dst_ptr[offset + 3] = (unsigned char)out3;
                dst_ptr[offset + 4] = (unsigned char)out4;
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)prs->evt = event;
    if (prs->is_async == 0)
        event.wait();
    return ret;
}

template <impl_interp_mtd INTERP_METHOD>
IMPL_STATUS impl_resize_v210(struct impl_resize_params *prs, impl_resize_context *prst, unsigned char *src_ptr_c,
                             unsigned char *dst_ptr_c, void *dep_evt) {
    (void)prst;
    IMPL_ASSERT(prs->pq != NULL, "queue is null");
    IMPL_ASSERT(src_ptr_c != NULL, "src_ptr is null");
    IMPL_ASSERT(dst_ptr_c != NULL, "dst_ptr is null");
    uint32_t src_width  = prs->src_width;
    uint32_t src_height = prs->src_height;
    uint32_t dst_width  = prs->dst_width;
    uint32_t dst_height = prs->dst_height;
    float coef_width    = (float)prs->src_width / (float)prs->dst_width;
    float coef_height   = (float)prs->src_height / (float)prs->dst_height;

    queue q         = *(queue *)(prs->pq);
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        uint32_t pitch     = prs->pitch_pixel * 4 / 6;
        uint32_t src_pitch = src_width * 4 / 6;
        uint32_t offset_x  = prs->offset_x;
        uint32_t offset_y  = prs->offset_y;

        try {
            unsigned int *src_ptr = (unsigned int *)src_ptr_c;
            unsigned int *dst_ptr = (unsigned int *)dst_ptr_c;
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>(dst_height, dst_width / 6), [=](sycl::item<2> item) {
                constexpr int datasize = GetDataSize<INTERP_METHOD>();
                uint32_t sx            = 6 * item[1];
                uint32_t sy            = item[0];
                float dst_lm[6];
                float dst_cb[3];
                float dst_cr[3];

                float yw[datasize];
                uint32_t yid[datasize];
                get_idw<INTERP_METHOD>(sy, coef_height, src_height, yid, yw);

                { // Y0 ~Y5
                    auto readlm = [&src_ptr, &src_pitch](uint32_t y, uint32_t x) {
                        return read_v210_lm(src_ptr, y, x, src_pitch);
                    };
                    for (int i = 0; i < 6; i++) {
                        float xw[datasize];
                        uint32_t xid[datasize];
                        get_idw<INTERP_METHOD>(sx + i, coef_width, src_width, xid, xw);

                        if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                            dst_lm[i] = pixel_interp_bilinear(xid, xw, yid, yw, readlm);
                        else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                            dst_lm[i] = pixel_interp_bicubic(xid, xw, yid, yw, readlm);
                    }
                }

                { // UV
                    auto readcr = [&src_ptr, &src_pitch](uint32_t y, uint32_t x) {
                        return read_v210_cr(src_ptr, y, x, src_pitch);
                    };
                    auto readcb = [&src_ptr, &src_pitch](uint32_t y, uint32_t x) {
                        return read_v210_cb(src_ptr, y, x, src_pitch);
                    };
                    for (int i = 0; i < 3; i++) {
                        float xw[datasize];
                        uint32_t xid[datasize];
                        get_idw<INTERP_METHOD>(sx / 2 + i, coef_width, src_width / 2, xid, xw);

                        if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                            dst_cr[i] = pixel_interp_bilinear(xid, xw, yid, yw, readcr);
                        else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                            dst_cr[i] = pixel_interp_bicubic(xid, xw, yid, yw, readcr);

                        if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                            dst_cb[i] = pixel_interp_bilinear(xid, xw, yid, yw, readcb);
                        else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                            dst_cb[i] = pixel_interp_bicubic(xid, xw, yid, yw, readcb);
                    }
                }

                uint32_t offset = mad24(sy + offset_y, pitch, (sx + offset_x) * 4 / 6);

                const auto dst_dw0 = ((unsigned int)dst_cb[0] & 0x3ff) | (((unsigned int)dst_lm[0] & 0x3ff) << 10) |
                                     (((unsigned int)dst_cr[0] & 0x3ff) << 20);

                const auto dst_dw1 = ((unsigned int)dst_lm[1] & 0x3ff) | (((unsigned int)dst_cb[1] & 0x3ff) << 10) |
                                     (((unsigned int)dst_lm[2] & 0x3ff) << 20);

                const auto dst_dw2 = ((unsigned int)dst_cr[1] & 0x3ff) | (((unsigned int)dst_lm[3] & 0x3ff) << 10) |
                                     (((unsigned int)dst_cb[2] & 0x3ff) << 20);

                const auto dst_dw3 = ((unsigned int)dst_lm[4] & 0x3ff) | (((unsigned int)dst_cr[2] & 0x3ff) << 10) |
                                     (((unsigned int)dst_lm[5] & 0x3ff) << 20);

                dst_ptr[offset]     = (unsigned int)(dst_dw0);
                dst_ptr[offset + 1] = (unsigned int)(dst_dw1);
                dst_ptr[offset + 2] = (unsigned int)(dst_dw2);
                dst_ptr[offset + 3] = (unsigned int)(dst_dw3);
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)prs->evt = event;
    if (prs->is_async == 0)
        event.wait();
    return ret;
}

template <impl_interp_mtd INTERP_METHOD>
IMPL_STATUS impl_resize_y210(struct impl_resize_params *prs, impl_resize_context *prst, unsigned char *src_ptr_c,
                             unsigned char *dst_ptr_c, void *dep_evt) {
    (void)prst;
    IMPL_ASSERT(prs->pq != NULL, "queue is null");
    IMPL_ASSERT(src_ptr_c != NULL, "src_ptr is null");
    IMPL_ASSERT(dst_ptr_c != NULL, "dst_ptr is null");
    uint32_t src_width  = prs->src_width;
    uint32_t src_height = prs->src_height;
    uint32_t dst_width  = prs->dst_width;
    uint32_t dst_height = prs->dst_height;
    float coef_width    = (float)prs->src_width / (float)prs->dst_width;
    float coef_height   = (float)prs->src_height / (float)prs->dst_height;

    queue q         = *(queue *)(prs->pq);
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;
    auto event      = q.submit([&](sycl::handler &h) {
        const uint32_t pitch    = prs->pitch_pixel * 2;
        const uint32_t offset_x = prs->offset_x;
        const uint32_t offset_y = prs->offset_y;
        uint32_t pitch_src      = src_width * 2;

        unsigned short *src_ptr = (unsigned short *)src_ptr_c;
        unsigned short *dst_ptr = (unsigned short *)dst_ptr_c;
        try {
            if (dep_evt != NULL) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range(dst_height, dst_width / 2), [=](sycl::item<2> item) {
                constexpr int datasize = GetDataSize<INTERP_METHOD>();

                uint32_t sx = 2 * item[1];
                uint32_t sy = item[0];

                float dst_lm[2];
                float dst_cb;
                float dst_cr;

                auto readlm = [&src_ptr, &pitch_src](uint32_t y, uint32_t x) {
                    auto off = mad24(x / 2, (uint32_t)4, (x % 2) * 2);
                    return ((src_ptr[mad24(y, pitch_src, off)]) >> 6);
                };

                float yw[datasize];
                uint32_t yid[datasize];

                // Y0 Y1
                get_idw<INTERP_METHOD>(sy, coef_height, src_height, yid, yw);
#pragma unroll
                for (uint32_t i = 0; i < 2; i++) {
                    float xw[datasize];
                    uint32_t xid[datasize];
                    get_idw<INTERP_METHOD>(sx + i, coef_width, src_width, xid, xw);

                    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                        dst_lm[i] = pixel_interp_bilinear(xid, xw, yid, yw, readlm);
                    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                        dst_lm[i] = pixel_interp_bicubic(xid, xw, yid, yw, readlm);
                }

                // UV
                {
                    auto readcr = [&src_ptr, &pitch_src](uint32_t y, uint32_t x) {
                        auto off = mad24(x, (uint32_t)4, (uint32_t)1);
                        return (src_ptr[mad24(y, pitch_src, off)] >> 6);
                    };
                    auto readcb = [&src_ptr, &pitch_src](uint32_t y, uint32_t x) {
                        auto off = mad24(x, (uint32_t)4, (uint32_t)3);
                        return (src_ptr[mad24(y, pitch_src, off)] >> 6);
                    };

                    float xw[datasize];
                    uint32_t xid[datasize];
                    get_idw<INTERP_METHOD>(sx / 2, coef_width, src_width / 2, xid, xw);

                    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                        dst_cr = pixel_interp_bilinear(xid, xw, yid, yw, readcr);
                    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                        dst_cr = pixel_interp_bicubic(xid, xw, yid, yw, readcr);

                    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
                        dst_cb = pixel_interp_bilinear(xid, xw, yid, yw, readcb);
                    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
                        dst_cb = pixel_interp_bicubic(xid, xw, yid, yw, readcb);
                }

                uint32_t offset     = mad24(sy + offset_y, pitch, (sx + offset_x) * 2);
                dst_ptr[offset]     = ((unsigned short)dst_lm[0] << 6) & 0xffc0;
                dst_ptr[offset + 1] = ((unsigned short)dst_cr << 6) & 0xffc0;
                dst_ptr[offset + 2] = ((unsigned short)dst_lm[1] << 6) & 0xffc0;
                dst_ptr[offset + 3] = ((unsigned short)dst_cb << 6) & 0xffc0;
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    *(sycl::event *)prs->evt = event;
    if (prs->is_async == 0)
        event.wait();
    return ret;
}