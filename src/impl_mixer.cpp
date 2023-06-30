/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include "impl_mixer.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

#include "impl_trace.hpp"

IMPL_STATUS composition_i420(struct impl_mixer_params *pmixer, struct impl_mixer_field_params *pfield,
                             struct impl_mixer_field_params *pfield0, void *dep_evt) {
    IMPL_ASSERT(pmixer != NULL, "pmixer is null");
    IMPL_ASSERT(pfield != NULL, "pfield is null");
    IMPL_ASSERT(pfield0 != NULL, "pfield0 is null");
    unsigned char *src_ptr   = (unsigned char *)pfield->buff;
    unsigned char *dst_ptr   = (unsigned char *)pfield0->buff;
    int width_src            = pfield->width;
    int height_src           = pfield->height;
    int crop_x_src           = pfield->crop_x;
    int crop_y_src           = pfield->crop_y;
    int crop_w_src           = pfield->crop_w;
    int crop_h_src           = pfield->crop_h;
    int width_dst            = pfield0->width;
    int height_dst           = pfield0->height;
    unsigned int left_offset = pfield->offset_x;
    unsigned int top_offset  = pfield->offset_y;
    queue q                  = *(queue *)(pmixer->pq);
    IMPL_STATUS ret          = IMPL_STATUS_SUCCESS;
    unsigned char *srcu_ptr  = src_ptr + width_src * height_src;
    unsigned char *srcv_ptr  = src_ptr + width_src * height_src * 5 / 4;
    unsigned char *dstu_ptr  = dst_ptr + width_dst * height_dst;
    unsigned char *dstv_ptr  = dst_ptr + width_dst * height_dst * 5 / 4;
    src_ptr                  = src_ptr + crop_y_src * width_src + crop_x_src;
    srcu_ptr                 = srcu_ptr + (crop_y_src >> 1) * (width_src >> 1) + (crop_x_src >> 1);
    srcv_ptr                 = srcv_ptr + (crop_y_src >> 1) * (width_src >> 1) + (crop_x_src >> 1);
    auto event               = q.submit([&](sycl::handler &h) {
        try {
            if (dep_evt) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<1>(crop_h_src * 2), [=](sycl::id<1> idx) {
                const auto y = idx[0];
                bool is_uv   = ((int)y >= crop_h_src);
                if (is_uv) {
                    bool is_u            = ((int)y < (crop_h_src * 3 / 2));
                    unsigned int uvp_src = width_src >> 1;
                    unsigned int uvp_dst = width_dst >> 1;
                    if (is_u) {
                        unsigned int y0 = y - crop_h_src;
                        memcpy(dstu_ptr + (y0 + (top_offset >> 1)) * uvp_dst + (left_offset >> 1),
                                             srcu_ptr + y0 * uvp_src, (crop_w_src >> 1));
                    } else {
                        unsigned int y0 = y - (crop_h_src * 3 / 2);
                        memcpy(dstv_ptr + (y0 + (top_offset >> 1)) * uvp_dst + (left_offset >> 1),
                                             srcv_ptr + y0 * uvp_src, (crop_w_src >> 1));
                    }
                } else {
                    memcpy(dst_ptr + (y + top_offset) * width_dst + left_offset, src_ptr + y * width_src, crop_w_src);
                }
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    if (pmixer->is_async == 0) {
        event.wait();
    }
    *(sycl::event *)pfield->evt = event;

    return ret;
}

IMPL_STATUS composition_v210(struct impl_mixer_params *pmixer, struct impl_mixer_field_params *pfield,
                             struct impl_mixer_field_params *pfield0, void *dep_evt) {
    IMPL_ASSERT(pmixer != NULL, "pmixer is null");
    IMPL_ASSERT(pfield != NULL, "pfield is null");
    IMPL_ASSERT(pfield0 != NULL, "pfield0 is null");
    unsigned int *src_ptr    = (unsigned int *)pfield->buff;
    unsigned int *dst_ptr    = (unsigned int *)pfield0->buff;
    int width_src            = pfield->width;
    int crop_x_src           = pfield->crop_x;
    int crop_y_src           = pfield->crop_y;
    int crop_w_src           = pfield->crop_w;
    int crop_h_src           = pfield->crop_h;
    int width_dst            = pfield0->width;
    unsigned int left_offset = pfield->offset_x;
    unsigned int top_offset  = pfield->offset_y;
    queue q                  = *(queue *)(pmixer->pq);
    IMPL_STATUS ret          = IMPL_STATUS_SUCCESS;
    auto event               = q.submit([&](sycl::handler &h) {
        try {
            if (dep_evt) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            const unsigned int pitch_src = width_src * 2 / 3;
            const unsigned int pitch_dst = width_dst * 2 / 3;
            const unsigned int yoffset   = top_offset;
            const unsigned int xoffset   = left_offset * 2 / 3;
            src_ptr                      = src_ptr + crop_y_src * pitch_src + crop_x_src * 2 / 3;
            h.parallel_for(sycl::range<1>(crop_h_src), [=](sycl::id<1> idx) {
                const auto y = idx[0];
                memcpy((dst_ptr + (y + yoffset) * pitch_dst + xoffset), (src_ptr + y * pitch_src), crop_w_src * 8 / 3);
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    if (pmixer->is_async == 0) {
        event.wait();
    }
    *(sycl::event *)pfield->evt = event;

    return ret;
}

IMPL_STATUS alphablending_alphasurf_i420(struct impl_mixer_params *pmixer, struct impl_mixer_field_params *pfield,
                                         struct impl_mixer_field_params *pfield0, void *dep_evt) {
    IMPL_ASSERT(pmixer != NULL, "pmixer is null");
    IMPL_ASSERT(pfield != NULL, "pfield is null");
    IMPL_ASSERT(pfield0 != NULL, "pfield0 is null");
    unsigned char *src_ptr    = (unsigned char *)pfield->buff;
    unsigned char *dst_ptr    = (unsigned char *)pfield0->buff;
    int width_src             = pfield->width;
    int height_src            = pfield->height;
    int crop_x_src            = pfield->crop_x;
    int crop_y_src            = pfield->crop_y;
    int crop_w_src            = pfield->crop_w;
    int crop_h_src            = pfield->crop_h;
    int width_dst             = pfield0->width;
    int height_dst            = pfield0->height;
    unsigned int left_offset  = pfield->offset_x;
    unsigned int top_offset   = pfield->offset_y;
    unsigned char *alpha_surf = pfield->alpha_surf;
    queue q                   = *(queue *)(pmixer->pq);
    IMPL_STATUS ret           = IMPL_STATUS_SUCCESS;
    unsigned char *srcu_ptr   = src_ptr + width_src * height_src;
    unsigned char *srcv_ptr   = src_ptr + width_src * height_src * 5 / 4;
    unsigned char *dstu_ptr   = dst_ptr + width_dst * height_dst;
    unsigned char *dstv_ptr   = dst_ptr + width_dst * height_dst * 5 / 4;
    src_ptr                   = src_ptr + crop_y_src * width_src + crop_x_src;
    srcu_ptr                  = srcu_ptr + (crop_y_src >> 1) * (width_src >> 1) + (crop_x_src >> 1);
    srcv_ptr                  = srcv_ptr + (crop_y_src >> 1) * (width_src >> 1) + (crop_x_src >> 1);
    const unsigned int bps    = 8;
    const unsigned int alphas = 1 << bps;
    auto event                = q.submit([&](sycl::handler &h) {
        try {
            if (dep_evt) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>((crop_h_src * 3 / 2), crop_w_src), [=](sycl::id<2> idx) {
                const auto x = idx[1];
                const auto y = idx[0];
                bool is_uv   = ((int)y >= crop_h_src);
                if (is_uv) {
                    bool is_u = is_uv && ((int)y < (crop_h_src * 5 / 4));
                    if (is_u) {
                        unsigned int y0          = 2 * (y - crop_h_src) + x / (crop_w_src / 2);
                        unsigned int x0          = x % (crop_w_src / 2);
                        unsigned int values      = (unsigned int)src_read_char(srcu_ptr, y0, x0, (width_src >> 1));
                        unsigned int valued      = (unsigned int)src_read_char(dstu_ptr, y0 + (top_offset >> 1),
                                                                                              x0 + (left_offset >> 1), (width_dst >> 1));
                        const unsigned int alpha = src_read_char(alpha_surf, (y0 >> 1), (x0 >> 1), (width_src >> 1));
                        unsigned int value       = (valued * (alphas - alpha) + values * alpha) >> bps;
                        dst_write_char(dstu_ptr, y0 + (top_offset >> 1), x0 + (left_offset >> 1), (width_dst / 2),
                                                      value);
                    } else {
                        unsigned int y0          = 2 * (y - crop_h_src * 5 / 4) + x / (crop_w_src / 2);
                        unsigned int x0          = x % (crop_w_src / 2);
                        unsigned int values      = (unsigned int)src_read_char(srcv_ptr, y0, x0, (width_src >> 1));
                        unsigned int valued      = (unsigned int)src_read_char(dstv_ptr, y0 + (top_offset >> 1),
                                                                                              x0 + (left_offset >> 1), (width_dst >> 1));
                        const unsigned int alpha = src_read_char(alpha_surf, (y0 >> 1), (x0 >> 1), (width_src >> 1));
                        unsigned int value       = (valued * (alphas - alpha) + values * alpha) >> bps;
                        dst_write_char(dstv_ptr, y0 + (top_offset >> 1), x0 + (left_offset >> 1), (width_dst / 2),
                                                      value);
                    }
                } else {
                    unsigned int values = (unsigned int)src_read_char(src_ptr, y, x, width_src);
                    unsigned int valued =
                        (unsigned int)src_read_char(dst_ptr, y + top_offset, x + left_offset, width_dst);
                    const unsigned int alpha = src_read_char(alpha_surf, y, x, width_src);
                    unsigned int value       = (valued * (alphas - alpha) + values * alpha) >> bps;
                    dst_write_char(dst_ptr, y + top_offset, x + left_offset, width_dst, value);
                }
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    if (pmixer->is_async == 0) {
        event.wait();
    }
    *(sycl::event *)pfield->evt = event;

    return ret;
}

IMPL_STATUS alphablending_staticalpha_i420(struct impl_mixer_params *pmixer, struct impl_mixer_field_params *pfield,
                                           struct impl_mixer_field_params *pfield0, void *dep_evt) {
    IMPL_ASSERT(pmixer != NULL, "pmixer is null");
    IMPL_ASSERT(pfield != NULL, "pfield is null");
    IMPL_ASSERT(pfield0 != NULL, "pfield0 is null");
    unsigned char *src_ptr    = (unsigned char *)pfield->buff;
    unsigned char *dst_ptr    = (unsigned char *)pfield0->buff;
    int width_src             = pfield->width;
    int height_src            = pfield->height;
    int crop_x_src            = pfield->crop_x;
    int crop_y_src            = pfield->crop_y;
    int crop_w_src            = pfield->crop_w;
    int crop_h_src            = pfield->crop_h;
    int width_dst             = pfield0->width;
    int height_dst            = pfield0->height;
    unsigned int left_offset  = pfield->offset_x;
    unsigned int top_offset   = pfield->offset_y;
    const unsigned int alpha  = (unsigned int)pfield->static_alpha;
    queue q                   = *(queue *)(pmixer->pq);
    IMPL_STATUS ret           = IMPL_STATUS_SUCCESS;
    unsigned char *srcu_ptr   = src_ptr + width_src * height_src;
    unsigned char *srcv_ptr   = src_ptr + width_src * height_src * 5 / 4;
    unsigned char *dstu_ptr   = dst_ptr + width_dst * height_dst;
    unsigned char *dstv_ptr   = dst_ptr + width_dst * height_dst * 5 / 4;
    src_ptr                   = src_ptr + crop_y_src * width_src + crop_x_src;
    srcu_ptr                  = srcu_ptr + (crop_y_src >> 1) * (width_src >> 1) + (crop_x_src >> 1);
    srcv_ptr                  = srcv_ptr + (crop_y_src >> 1) * (width_src >> 1) + (crop_x_src >> 1);
    const unsigned int bps    = 8;
    const unsigned int alphas = 1 << bps;
    auto event                = q.submit([&](sycl::handler &h) {
        try {
            if (dep_evt) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>((crop_h_src * 3 / 2), crop_w_src), [=](sycl::id<2> idx) {
                const auto x = idx[1];
                const auto y = idx[0];
                bool is_uv   = ((int)y >= crop_h_src);
                if (is_uv) {
                    bool is_u = is_uv && ((int)y < (crop_h_src * 5 / 4));
                    if (is_u) {
                        unsigned int y0     = 2 * (y - crop_h_src) + x / (crop_w_src / 2);
                        unsigned int x0     = x % (crop_w_src / 2);
                        unsigned int values = (unsigned int)src_read_char(srcu_ptr, y0, x0, (width_src >> 1));
                        unsigned int valued = (unsigned int)src_read_char(dstu_ptr, y0 + (top_offset >> 1),
                                                                                         x0 + (left_offset >> 1), (width_dst >> 1));
                        unsigned int value  = (valued * (alphas - alpha) + values * alpha) >> bps;
                        dst_write_char(dstu_ptr, y0 + (top_offset >> 1), x0 + (left_offset >> 1), (width_dst / 2),
                                                      value);
                    } else {
                        unsigned int y0     = 2 * (y - crop_h_src * 5 / 4) + x / (crop_w_src / 2);
                        unsigned int x0     = x % (crop_w_src / 2);
                        unsigned int values = (unsigned int)src_read_char(srcv_ptr, y0, x0, (width_src >> 1));
                        unsigned int valued = (unsigned int)src_read_char(dstv_ptr, y0 + (top_offset >> 1),
                                                                                         x0 + (left_offset >> 1), (width_dst >> 1));
                        unsigned int value  = (valued * (alphas - alpha) + values * alpha) >> bps;
                        dst_write_char(dstv_ptr, y0 + (top_offset >> 1), x0 + (left_offset >> 1), (width_dst / 2),
                                                      value);
                    }
                } else {
                    unsigned int values = (unsigned int)src_read_char(src_ptr, y, x, width_src);
                    unsigned int valued =
                        (unsigned int)src_read_char(dst_ptr, y + top_offset, x + left_offset, width_dst);
                    unsigned int value = (valued * (alphas - alpha) + values * alpha) >> bps;
                    dst_write_char(dst_ptr, y + top_offset, x + left_offset, width_dst, value);
                }
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });
    if (pmixer->is_async == 0) {
        event.wait();
    }
    *(sycl::event *)pfield->evt = event;

    return ret;
}

IMPL_STATUS alphablending_alphasurf_v210(struct impl_mixer_params *pmixer, struct impl_mixer_field_params *pfield,
                                         struct impl_mixer_field_params *pfield0, void *dep_evt) {
    IMPL_ASSERT(pmixer != NULL, "pmixer is null");
    IMPL_ASSERT(pfield != NULL, "pfield is null");
    IMPL_ASSERT(pfield0 != NULL, "pfield0 is null");
    unsigned int *src_ptr     = (unsigned int *)pfield->buff;
    unsigned int *dst_ptr     = (unsigned int *)pfield0->buff;
    int width_src             = pfield->width;
    int crop_x_src            = pfield->crop_x;
    int crop_y_src            = pfield->crop_y;
    int crop_w_src            = pfield->crop_w;
    int crop_h_src            = pfield->crop_h;
    int width_dst             = pfield0->width;
    unsigned int left_offset  = pfield->offset_x;
    unsigned int top_offset   = pfield->offset_y;
    unsigned char *alpha_surf = pfield->alpha_surf;
    const unsigned int bps    = 8;
    const unsigned int alphas = 1 << bps;
    queue q                   = *(queue *)(pmixer->pq);
    IMPL_STATUS ret           = IMPL_STATUS_SUCCESS;
    auto event                = q.submit([&](sycl::handler &h) {
        try {
            if (dep_evt) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            const unsigned int pitch_src = width_src * 2 / 3;
            const unsigned int pitch_dst = width_dst * 2 / 3;
            const unsigned int yoffset   = top_offset;
            const unsigned int xoffset   = left_offset * 2 / 3;
            src_ptr                      = src_ptr + crop_y_src * pitch_src + crop_x_src * 2 / 3;
            h.parallel_for(sycl::range<2>(crop_h_src, (crop_w_src / 6)), [=](sycl::id<2> idx) {
                const auto y                  = idx[0];
                const auto x                  = idx[1];
                const unsigned int value_src0 = src_read_int(src_ptr, y, 4 * x, pitch_src);
                const unsigned int value_dst0 = src_read_int(dst_ptr, y + yoffset, 4 * x + xoffset, pitch_dst);
                unsigned int cb0_src          = value_src0 & 0x3ff;
                unsigned int lm0_src          = (value_src0 >> 10) & 0x3ff;
                unsigned int cr0_src          = (value_src0 >> 20) & 0x3ff;
                unsigned int cb0_dst          = value_dst0 & 0x3ff;
                unsigned int lm0_dst          = (value_dst0 >> 10) & 0x3ff;
                unsigned int cr0_dst          = (value_dst0 >> 20) & 0x3ff;
                const unsigned int alpha0     = src_read_char(alpha_surf, y, 6 * x, width_src);
                unsigned int cb0              = (cb0_dst * (alphas - alpha0) + cb0_src * alpha0) >> bps;
                unsigned int lm0              = (lm0_dst * (alphas - alpha0) + lm0_src * alpha0) >> bps;
                unsigned int cr0              = (cr0_dst * (alphas - alpha0) + cr0_src * alpha0) >> bps;
                unsigned int value0           = (cb0 & 0x3ff) | ((lm0 & 0x3ff) << 10) | ((cr0 & 0x3ff) << 20);
                dst_write_int(dst_ptr, y + yoffset, 4 * x + xoffset, pitch_dst, value0);

                const unsigned int value_src1 = src_read_int(src_ptr, y, 4 * x + 1, pitch_src);
                const unsigned int value_dst1 = src_read_int(dst_ptr, y + yoffset, 4 * x + 1 + xoffset, pitch_dst);
                unsigned int lm1_src          = value_src1 & 0x3ff;
                unsigned int cb1_src          = (value_src1 >> 10) & 0x3ff;
                unsigned int lm2_src          = (value_src1 >> 20) & 0x3ff;
                unsigned int lm1_dst          = value_dst1 & 0x3ff;
                unsigned int cb1_dst          = (value_dst1 >> 10) & 0x3ff;
                unsigned int lm2_dst          = (value_dst1 >> 20) & 0x3ff;
                const unsigned int alpha1     = src_read_char(alpha_surf, y, 6 * x + 1, width_src);
                const unsigned int alpha2     = src_read_char(alpha_surf, y, 6 * x + 2, width_src);
                unsigned int lm1              = (lm1_dst * (alphas - alpha1) + lm1_src * alpha1) >> bps;
                unsigned int cb1              = (cb1_dst * (alphas - alpha2) + cb1_src * alpha2) >> bps;
                unsigned int lm2              = (lm2_dst * (alphas - alpha2) + lm2_src * alpha2) >> bps;
                unsigned int value1           = (lm1 & 0x3ff) | ((cb1 & 0x3ff) << 10) | ((lm2 & 0x3ff) << 20);
                dst_write_int(dst_ptr, y + yoffset, 4 * x + 1 + xoffset, pitch_dst, value1);

                const unsigned int value_src2 = src_read_int(src_ptr, y, 4 * x + 2, pitch_src);
                const unsigned int value_dst2 = src_read_int(dst_ptr, y + yoffset, 4 * x + 2 + xoffset, pitch_dst);
                unsigned int cr1_src          = value_src2 & 0x3ff;
                unsigned int lm3_src          = (value_src2 >> 10) & 0x3ff;
                unsigned int cb2_src          = (value_src2 >> 20) & 0x3ff;
                unsigned int cr1_dst          = value_dst2 & 0x3ff;
                unsigned int lm3_dst          = (value_dst2 >> 10) & 0x3ff;
                unsigned int cb2_dst          = (value_dst2 >> 20) & 0x3ff;
                const unsigned int alpha3     = src_read_char(alpha_surf, y, 6 * x + 3, width_src);
                const unsigned int alpha4     = src_read_char(alpha_surf, y, 6 * x + 4, width_src);
                unsigned int cr1              = (cr1_dst * (alphas - alpha2) + cr1_src * alpha2) >> bps;
                unsigned int lm3              = (lm3_dst * (alphas - alpha3) + lm3_src * alpha3) >> bps;
                unsigned int cb2              = (cb2_dst * (alphas - alpha4) + cb2_src * alpha4) >> bps;
                unsigned int value2           = (cr1 & 0x3ff) | ((lm3 & 0x3ff) << 10) | ((cb2 & 0x3ff) << 20);
                dst_write_int(dst_ptr, y + yoffset, 4 * x + 2 + xoffset, pitch_dst, value2);

                const unsigned int value_src3 = src_read_int(src_ptr, y, 4 * x + 3, pitch_src);
                const unsigned int value_dst3 = src_read_int(dst_ptr, y + yoffset, 4 * x + 3 + xoffset, pitch_dst);
                unsigned int lm4_src          = value_src3 & 0x3ff;
                unsigned int cr2_src          = (value_src3 >> 10) & 0x3ff;
                unsigned int lm5_src          = (value_src3 >> 20) & 0x3ff;
                unsigned int lm4_dst          = value_dst3 & 0x3ff;
                unsigned int cr2_dst          = (value_dst3 >> 10) & 0x3ff;
                unsigned int lm5_dst          = (value_dst3 >> 20) & 0x3ff;
                const unsigned int alpha5     = src_read_char(alpha_surf, y, 6 * x + 5, width_src);
                unsigned int lm4              = (lm4_dst * (alphas - alpha4) + lm4_src * alpha4) >> bps;
                unsigned int cr2              = (cr2_dst * (alphas - alpha4) + cr2_src * alpha4) >> bps;
                unsigned int lm5              = (lm5_dst * (alphas - alpha5) + lm5_src * alpha5) >> bps;
                unsigned int value3           = (lm4 & 0x3ff) | ((cr2 & 0x3ff) << 10) | ((lm5 & 0x3ff) << 20);
                dst_write_int(dst_ptr, y + yoffset, 4 * x + 3 + xoffset, pitch_dst, value3);
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    if (pmixer->is_async == 0) {
        event.wait();
    }
    *(sycl::event *)pfield->evt = event;

    return ret;
}

IMPL_STATUS alphablending_staticalpha_v210(struct impl_mixer_params *pmixer, struct impl_mixer_field_params *pfield,
                                           struct impl_mixer_field_params *pfield0, void *dep_evt) {
    IMPL_ASSERT(pmixer != NULL, "pmixer is null");
    IMPL_ASSERT(pfield != NULL, "pfield is null");
    IMPL_ASSERT(pfield0 != NULL, "pfield0 is null");
    unsigned int *src_ptr     = (unsigned int *)pfield->buff;
    unsigned int *dst_ptr     = (unsigned int *)pfield0->buff;
    int width_src             = pfield->width;
    int crop_x_src            = pfield->crop_x;
    int crop_y_src            = pfield->crop_y;
    int crop_w_src            = pfield->crop_w;
    int crop_h_src            = pfield->crop_h;
    int width_dst             = pfield0->width;
    unsigned int left_offset  = pfield->offset_x;
    unsigned int top_offset   = pfield->offset_y;
    const unsigned int alpha  = (unsigned int)pfield->static_alpha;
    queue q                   = *(queue *)(pmixer->pq);
    IMPL_STATUS ret           = IMPL_STATUS_SUCCESS;
    const unsigned int bps    = 8;
    const unsigned int alphas = 1 << bps;
    auto event                = q.submit([&](sycl::handler &h) {
        try {
            if (dep_evt) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            const unsigned int pitch_src = width_src * 2 / 3;
            const unsigned int pitch_dst = width_dst * 2 / 3;
            const unsigned int yoffset   = top_offset;
            const unsigned int xoffset   = left_offset * 2 / 3;
            src_ptr                      = src_ptr + crop_y_src * pitch_src + crop_x_src * 2 / 3;
            h.parallel_for(sycl::range<2>(crop_h_src, (crop_w_src / 6)), [=](sycl::id<2> idx) {
                const auto y                  = idx[0];
                const auto x                  = idx[1];
                const unsigned int value_src0 = src_read_int(src_ptr, y, 4 * x, pitch_src);
                const unsigned int value_dst0 = src_read_int(dst_ptr, y + yoffset, 4 * x + xoffset, pitch_dst);
                unsigned int cb0_src          = value_src0 & 0x3ff;
                unsigned int lm0_src          = (value_src0 >> 10) & 0x3ff;
                unsigned int cr0_src          = (value_src0 >> 20) & 0x3ff;
                unsigned int cb0_dst          = value_dst0 & 0x3ff;
                unsigned int lm0_dst          = (value_dst0 >> 10) & 0x3ff;
                unsigned int cr0_dst          = (value_dst0 >> 20) & 0x3ff;
                unsigned int cb0              = (cb0_dst * (alphas - alpha) + cb0_src * alpha) >> bps;
                unsigned int lm0              = (lm0_dst * (alphas - alpha) + lm0_src * alpha) >> bps;
                unsigned int cr0              = (cr0_dst * (alphas - alpha) + cr0_src * alpha) >> bps;
                unsigned int value0           = (cb0 & 0x3ff) | ((lm0 & 0x3ff) << 10) | ((cr0 & 0x3ff) << 20);
                dst_write_int(dst_ptr, y + yoffset, 4 * x + xoffset, pitch_dst, value0);

                const unsigned int value_src1 = src_read_int(src_ptr, y, 4 * x + 1, pitch_src);
                const unsigned int value_dst1 = src_read_int(dst_ptr, y + yoffset, 4 * x + 1 + xoffset, pitch_dst);
                unsigned int lm1_src          = value_src1 & 0x3ff;
                unsigned int cb1_src          = (value_src1 >> 10) & 0x3ff;
                unsigned int lm2_src          = (value_src1 >> 20) & 0x3ff;
                unsigned int lm1_dst          = value_dst1 & 0x3ff;
                unsigned int cb1_dst          = (value_dst1 >> 10) & 0x3ff;
                unsigned int lm2_dst          = (value_dst1 >> 20) & 0x3ff;
                unsigned int lm1              = (lm1_dst * (alphas - alpha) + lm1_src * alpha) >> bps;
                unsigned int cb1              = (cb1_dst * (alphas - alpha) + cb1_src * alpha) >> bps;
                unsigned int lm2              = (lm2_dst * (alphas - alpha) + lm2_src * alpha) >> bps;
                unsigned int value1           = (lm1 & 0x3ff) | ((cb1 & 0x3ff) << 10) | ((lm2 & 0x3ff) << 20);
                dst_write_int(dst_ptr, y + yoffset, 4 * x + 1 + xoffset, pitch_dst, value1);

                const unsigned int value_src2 = src_read_int(src_ptr, y, 4 * x + 2, pitch_src);
                const unsigned int value_dst2 = src_read_int(dst_ptr, y + yoffset, 4 * x + 2 + xoffset, pitch_dst);
                unsigned int cr1_src          = value_src2 & 0x3ff;
                unsigned int lm3_src          = (value_src2 >> 10) & 0x3ff;
                unsigned int cb2_src          = (value_src2 >> 20) & 0x3ff;
                unsigned int cr1_dst          = value_dst2 & 0x3ff;
                unsigned int lm3_dst          = (value_dst2 >> 10) & 0x3ff;
                unsigned int cb2_dst          = (value_dst2 >> 20) & 0x3ff;
                unsigned int cr1              = (cr1_dst * (alphas - alpha) + cr1_src * alpha) >> bps;
                unsigned int lm3              = (lm3_dst * (alphas - alpha) + lm3_src * alpha) >> bps;
                unsigned int cb2              = (cb2_dst * (alphas - alpha) + cb2_src * alpha) >> bps;
                unsigned int value2           = (cr1 & 0x3ff) | ((lm3 & 0x3ff) << 10) | ((cb2 & 0x3ff) << 20);
                dst_write_int(dst_ptr, y + yoffset, 4 * x + 2 + xoffset, pitch_dst, value2);

                const unsigned int value_src3 = src_read_int(src_ptr, y, 4 * x + 3, pitch_src);
                const unsigned int value_dst3 = src_read_int(dst_ptr, y + yoffset, 4 * x + 3 + xoffset, pitch_dst);
                unsigned int lm4_src          = value_src3 & 0x3ff;
                unsigned int cr2_src          = (value_src3 >> 10) & 0x3ff;
                unsigned int lm5_src          = (value_src3 >> 20) & 0x3ff;
                unsigned int lm4_dst          = value_dst3 & 0x3ff;
                unsigned int cr2_dst          = (value_dst3 >> 10) & 0x3ff;
                unsigned int lm5_dst          = (value_dst3 >> 20) & 0x3ff;
                unsigned int lm4              = (lm4_dst * (alphas - alpha) + lm4_src * alpha) >> bps;
                unsigned int cr2              = (cr2_dst * (alphas - alpha) + cr2_src * alpha) >> bps;
                unsigned int lm5              = (lm5_dst * (alphas - alpha) + lm5_src * alpha) >> bps;
                unsigned int value3           = (lm4 & 0x3ff) | ((cr2 & 0x3ff) << 10) | ((lm5 & 0x3ff) << 20);
                dst_write_int(dst_ptr, y + yoffset, 4 * x + 3 + xoffset, pitch_dst, value3);
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    if (pmixer->is_async == 0) {
        event.wait();
    }
    *(sycl::event *)pfield->evt = event;

    return ret;
}

IMPL_STATUS alphablending_alphasurf_yuv420p10le(struct impl_mixer_params *pmixer,
                                                struct impl_mixer_field_params *pfield,
                                                struct impl_mixer_field_params *pfield0, void *dep_evt) {
    IMPL_ASSERT(pmixer != NULL, "pmixer is null");
    IMPL_ASSERT(pfield != NULL, "pfield is null");
    IMPL_ASSERT(pfield0 != NULL, "pfield0 is null");
    unsigned short *src_ptr   = (unsigned short *)pfield->buff;
    unsigned short *dst_ptr   = (unsigned short *)pfield0->buff;
    int width_src             = pfield->width;
    int height_src            = pfield->height;
    int crop_x_src            = pfield->crop_x;
    int crop_y_src            = pfield->crop_y;
    int crop_w_src            = pfield->crop_w;
    int crop_h_src            = pfield->crop_h;
    int width_dst             = pfield0->width;
    int height_dst            = pfield0->height;
    unsigned int left_offset  = pfield->offset_x;
    unsigned int top_offset   = pfield->offset_y;
    unsigned char *alpha_surf = pfield->alpha_surf;
    queue q                   = *(queue *)(pmixer->pq);
    IMPL_STATUS ret           = IMPL_STATUS_SUCCESS;
    unsigned short *srcu_ptr  = src_ptr + width_src * height_src;
    unsigned short *srcv_ptr  = src_ptr + width_src * height_src * 5 / 4;
    unsigned short *dstu_ptr  = dst_ptr + width_dst * height_dst;
    unsigned short *dstv_ptr  = dst_ptr + width_dst * height_dst * 5 / 4;
    src_ptr                   = src_ptr + crop_y_src * width_src + crop_x_src;
    srcu_ptr                  = srcu_ptr + (crop_y_src >> 1) * (width_src >> 1) + (crop_x_src >> 1);
    srcv_ptr                  = srcv_ptr + (crop_y_src >> 1) * (width_src >> 1) + (crop_x_src >> 1);
    const unsigned int bps    = 8;
    const unsigned int alphas = 1 << bps;
    auto event                = q.submit([&](sycl::handler &h) {
        try {
            if (dep_evt) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>((crop_h_src * 3 / 2), crop_w_src), [=](sycl::id<2> idx) {
                const auto x = idx[1];
                const auto y = idx[0];
                bool is_uv   = ((int)y >= crop_h_src);
                if (is_uv) {
                    bool is_u = is_uv && ((int)y < (crop_h_src * 5 / 4));
                    if (is_u) {
                        unsigned int y0          = 2 * (y - crop_h_src) + x / (crop_w_src / 2);
                        unsigned int x0          = x % (crop_w_src / 2);
                        unsigned int values      = (unsigned int)src_read_short(srcu_ptr, y0, x0, (width_src >> 1));
                        unsigned int valued      = (unsigned int)src_read_short(dstu_ptr, y0 + (top_offset >> 1),
                                                                                               x0 + (left_offset >> 1), (width_dst >> 1));
                        const unsigned int alpha = src_read_char(alpha_surf, (y0 >> 1), (x0 >> 1), (width_src >> 1));
                        unsigned int value       = (valued * (alphas - alpha) + values * alpha) >> bps;
                        dst_write_short(dstu_ptr, y0 + (top_offset >> 1), x0 + (left_offset >> 1), (width_dst >> 1),
                                                       value);
                    } else {
                        unsigned int y0          = 2 * (y - crop_h_src * 5 / 4) + x / (crop_w_src / 2);
                        unsigned int x0          = x % (crop_w_src / 2);
                        unsigned int values      = (unsigned int)src_read_short(srcv_ptr, y0, x0, (width_src >> 1));
                        unsigned int valued      = (unsigned int)src_read_short(dstv_ptr, y0 + (top_offset >> 1),
                                                                                               x0 + (left_offset >> 1), (width_dst >> 1));
                        const unsigned int alpha = src_read_char(alpha_surf, (y0 >> 1), (x0 >> 1), (width_src >> 1));
                        unsigned int value       = (valued * (alphas - alpha) + values * alpha) >> bps;
                        dst_write_short(dstv_ptr, y0 + (top_offset >> 1), x0 + (left_offset >> 1), (width_dst >> 1),
                                                       value);
                    }
                } else {
                    unsigned int values = (unsigned int)src_read_short(src_ptr, y, x, width_src);
                    unsigned int valued =
                        (unsigned int)src_read_short(dst_ptr, y + top_offset, x + left_offset, width_dst);
                    const unsigned int alpha = src_read_char(alpha_surf, y, x, width_src);
                    unsigned int value       = (valued * (alphas - alpha) + values * alpha) >> bps;
                    dst_write_short(dst_ptr, y + top_offset, x + left_offset, width_dst, value);
                }
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    if (pmixer->is_async == 0) {
        event.wait();
    }
    *(sycl::event *)pfield->evt = event;

    return ret;
}

IMPL_STATUS alphablending_staticalpha_yuv420p10le(struct impl_mixer_params *pmixer,
                                                  struct impl_mixer_field_params *pfield,
                                                  struct impl_mixer_field_params *pfield0, void *dep_evt) {
    IMPL_ASSERT(pmixer != NULL, "pmixer is null");
    IMPL_ASSERT(pfield != NULL, "pfield is null");
    IMPL_ASSERT(pfield0 != NULL, "pfield0 is null");
    unsigned short *src_ptr   = (unsigned short *)pfield->buff;
    unsigned short *dst_ptr   = (unsigned short *)pfield0->buff;
    int width_src             = pfield->width;
    int height_src            = pfield->height;
    int crop_x_src            = pfield->crop_x;
    int crop_y_src            = pfield->crop_y;
    int crop_w_src            = pfield->crop_w;
    int crop_h_src            = pfield->crop_h;
    int width_dst             = pfield0->width;
    int height_dst            = pfield0->height;
    unsigned int left_offset  = pfield->offset_x;
    unsigned int top_offset   = pfield->offset_y;
    const unsigned int alpha  = (unsigned int)pfield->static_alpha;
    queue q                   = *(queue *)(pmixer->pq);
    IMPL_STATUS ret           = IMPL_STATUS_SUCCESS;
    unsigned short *srcu_ptr  = src_ptr + width_src * height_src;
    unsigned short *srcv_ptr  = src_ptr + width_src * height_src * 5 / 4;
    unsigned short *dstu_ptr  = dst_ptr + width_dst * height_dst;
    unsigned short *dstv_ptr  = dst_ptr + width_dst * height_dst * 5 / 4;
    src_ptr                   = src_ptr + crop_y_src * width_src + crop_x_src;
    srcu_ptr                  = srcu_ptr + (crop_y_src >> 1) * (width_src >> 1) + (crop_x_src >> 1);
    srcv_ptr                  = srcv_ptr + (crop_y_src >> 1) * (width_src >> 1) + (crop_x_src >> 1);
    const unsigned int bps    = 8;
    const unsigned int alphas = 1 << bps;
    auto event                = q.submit([&](sycl::handler &h) {
        try {
            if (dep_evt) {
                auto d_evt = *(sycl::event *)dep_evt;
                h.depends_on(d_evt);
            }
            h.parallel_for(sycl::range<2>((crop_h_src * 3 / 2), crop_w_src), [=](sycl::id<2> idx) {
                const auto x = idx[1];
                const auto y = idx[0];
                bool is_uv   = ((int)y >= crop_h_src);
                if (is_uv) {
                    bool is_u = is_uv && ((int)y < (crop_h_src * 5 / 4));
                    if (is_u) {
                        unsigned int y0     = 2 * (y - crop_h_src) + x / (crop_w_src / 2);
                        unsigned int x0     = x % (crop_w_src / 2);
                        unsigned int values = (unsigned int)src_read_short(srcu_ptr, y0, x0, (width_src >> 1));
                        unsigned int valued = (unsigned int)src_read_short(dstu_ptr, y0 + (top_offset >> 1),
                                                                                          x0 + (left_offset >> 1), (width_dst >> 1));
                        unsigned int value  = (valued * (alphas - alpha) + values * alpha) >> bps;
                        dst_write_short(dstu_ptr, y0 + (top_offset >> 1), x0 + (left_offset >> 1), (width_dst >> 1),
                                                       value);
                    } else {
                        unsigned int y0     = 2 * (y - crop_h_src * 5 / 4) + x / (crop_w_src / 2);
                        unsigned int x0     = x % (crop_w_src / 2);
                        unsigned int values = (unsigned int)src_read_short(srcv_ptr, y0, x0, (width_src >> 1));
                        unsigned int valued = (unsigned int)src_read_short(dstv_ptr, y0 + (top_offset >> 1),
                                                                                          x0 + (left_offset >> 1), (width_dst >> 1));
                        unsigned int value  = (valued * (alphas - alpha) + values * alpha) >> bps;
                        dst_write_short(dstv_ptr, y0 + (top_offset >> 1), x0 + (left_offset >> 1), (width_dst >> 1),
                                                       value);
                    }
                } else {
                    unsigned int values = (unsigned int)src_read_short(src_ptr, y, x, width_src);
                    unsigned int valued =
                        (unsigned int)src_read_short(dst_ptr, y + top_offset, x + left_offset, width_dst);
                    unsigned int value = (valued * (alphas - alpha) + values * alpha) >> bps;
                    dst_write_short(dst_ptr, y + top_offset, x + left_offset, width_dst, value);
                }
            });
        } catch (std::exception e) {
            err("%s, SYCL exception caught: %s\n", __func__, e.what());
            ret = IMPL_STATUS_FAIL;
        }
    });

    if (pmixer->is_async == 0) {
        event.wait();
    }
    *(sycl::event *)pfield->evt = event;

    return ret;
}

enum Mixer_function_index {
    index_composition_i420 = 0,
    index_composition_v210,
    index_alphablending_staticalpha_i420,
    index_alphablending_staticalpha_v210,
    index_alphablending_staticalpha_yuv420p10le,
    index_alphablending_alphasurf_i420,
    index_alphablending_alphasurf_v210,
    index_alphablending_alphasurf_yuv420p10le,
    MAX_MIXER_FUNCTION_NUM
};

typedef IMPL_STATUS (*Mixer_function)(struct impl_mixer_params *pmixer, struct impl_mixer_field_params *pfield,
                                      struct impl_mixer_field_params *pfield0, void *dep_evt);
Mixer_function mixerfunction[MAX_MIXER_FUNCTION_NUM] = {
    composition_i420,
    composition_v210,
    alphablending_staticalpha_i420,
    alphablending_staticalpha_v210,
    alphablending_staticalpha_yuv420p10le,
    alphablending_alphasurf_i420,
    alphablending_alphasurf_v210,
    alphablending_alphasurf_yuv420p10le,
};

IMPL_API IMPL_STATUS impl_mixer_init(struct impl_mixer_params *pmixer, void *&pmx_context) {
    IMPL_ASSERT(pmixer != NULL, "mixer init failed, pmixer is null");
    IMPL_ASSERT(pmixer->pq != NULL, "mixer init failed, pq is null");
    if (pmixer->layers < 2) {
        err("%s, Illegal layers: mixer layers/field number %d must be >= 2\n", __func__, pmixer->layers);
        return IMPL_STATUS_INVALID_PARAMS;
    }
    if (pmixer->layers > IMPL_MIXER_MAX_FIELDS) {
        err("%s, Illegal layers: mixer layers/field number %d must be <= %d\n", __func__, pmixer->layers,
            IMPL_MIXER_MAX_FIELDS);
        return IMPL_STATUS_INVALID_PARAMS;
    }

    impl_mixer_context *pcontext = new impl_mixer_context;
    memset(pcontext, 0, sizeof(impl_mixer_context));
    pmx_context = (void *)pcontext;

    for (int layer = 0; layer < pmixer->layers; layer++) {
        struct impl_mixer_field_params *pfield = &(pmixer->field[layer]);
        // memset(pfield, 0, sizeof(*pfield));
        pfield->evt = impl_common_new_event();
        if (pfield->crop_w == 0)
            pfield->crop_w = pfield->width;
        if (pfield->crop_h == 0)
            pfield->crop_h = pfield->height;
        if (pfield->is_alphab) {
            if (IMPL_VIDEO_I420 == pmixer->format) {
                if (pfield->alpha_surf) {
                    pcontext->mixer_func_index[layer] = index_alphablending_alphasurf_i420;
                } else {
                    pcontext->mixer_func_index[layer] = index_alphablending_staticalpha_i420;
                }
            } else if (IMPL_VIDEO_V210 == pmixer->format) {
                if (pfield->alpha_surf) {
                    pcontext->mixer_func_index[layer] = index_alphablending_alphasurf_v210;
                } else {
                    pcontext->mixer_func_index[layer] = index_alphablending_staticalpha_v210;
                }
            } else if (IMPL_VIDEO_YUV420P10LE == pmixer->format) {
                if (pfield->alpha_surf) {
                    pcontext->mixer_func_index[layer] = index_alphablending_alphasurf_yuv420p10le;
                } else {
                    pcontext->mixer_func_index[layer] = index_alphablending_staticalpha_yuv420p10le;
                }
            }
        } else {
            if (IMPL_VIDEO_I420 == pmixer->format) {
                pcontext->mixer_func_index[layer] = index_composition_i420;
            } else if (IMPL_VIDEO_V210 == pmixer->format) {
                pcontext->mixer_func_index[layer] = index_composition_v210;
            }
        }

        if ((pfield->width % 2) != 0 || (pfield->height % 2) != 0) {
            err("%s, Illegal size: The width and height must be a multiple of 2\n", __func__);
            return IMPL_STATUS_INVALID_PARAMS;
        }
        if ((pfield->offset_x % 2) != 0 || (pfield->offset_y % 2) != 0) {
            err("%s, Illegal offset: The offset_x and offset_y must be a multiple of 2\n", __func__);
            return IMPL_STATUS_INVALID_PARAMS;
        }
        if (IMPL_VIDEO_V210 == pmixer->format && (pfield->width % 48) != 0) {
            err("%s, Illegal V210 size: The width=%d must be a multiple of 48\n", __func__, pfield->width);
            return IMPL_STATUS_INVALID_PARAMS;
        }
        if (IMPL_VIDEO_V210 == pmixer->format && (pfield->offset_x % 48) != 0) {
            err("%s, Illegal V210 offset_x: The offset_x=%d must be a multiple of 48\n", __func__, pfield->offset_x);
            return IMPL_STATUS_INVALID_PARAMS;
        }
    }
    return IMPL_STATUS_SUCCESS;
}

IMPL_STATUS impl_mixer_run(struct impl_mixer_params *pmixer, void *pmx_context, void *fieldbuffs[], void *dep_evts[]) {
    IMPL_ASSERT(pmixer != NULL, "mixer run failed, pmixer is null");
    IMPL_ASSERT(pmx_context != NULL, "mixer run failed, pmx_context is null");
    IMPL_STATUS ret = IMPL_STATUS_SUCCESS;

    struct impl_mixer_context *pcontext     = (struct impl_mixer_context *)pmx_context;
    struct impl_mixer_field_params *pfield0 = &(pmixer->field[0]);
    if (fieldbuffs && fieldbuffs[0]) {
        pfield0->buff = fieldbuffs[0];
    }
    for (int layer = 1; layer < pmixer->layers; layer++) {
        struct impl_mixer_field_params *pfield = &(pmixer->field[layer]);
        void *dep_evt                          = nullptr;
        bool out_of_bounds                     = (pfield->crop_w + pfield->offset_x) > pfield0->width ||
                             (pfield->crop_h + pfield->offset_y) > pfield0->height;
        if (out_of_bounds) {
            info("%s, out_of_bounds, skipped composition or mixer for field%d !!!\n", __func__, layer);
            continue;
        }
        if (fieldbuffs && fieldbuffs[layer]) {
            pfield->buff = fieldbuffs[layer];
        }
        if (dep_evts && dep_evts[layer]) {
            dep_evt = dep_evts[layer];
        }

        ret = mixerfunction[pcontext->mixer_func_index[layer]](pmixer, pfield, pfield0, dep_evt);
        if (IMPL_STATUS_SUCCESS != ret) {
            err("%s, impl_mixer_run failed on field%d !!!", __func__, layer);
            break;
        }
    }
    return ret;
}

IMPL_STATUS impl_mixer_uninit(struct impl_mixer_params *pmixer, void *pmx_context) {
    IMPL_ASSERT(pmixer != NULL, "mixer uninit failed, pmixer is null");
    IMPL_ASSERT(pmx_context != NULL, "mixer uninit failed, pmx_context is null");

    struct impl_mixer_context *pcontext = (struct impl_mixer_context *)pmx_context;
    for (int layer = 0; layer < pmixer->layers; layer++) {
        impl_common_free_event(pmixer->field[layer].evt);
    }
    delete pcontext;

    return IMPL_STATUS_SUCCESS;
}
