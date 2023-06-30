/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef __RESIZE_H__
#define __RESIZE_H__

#include <CL/sycl.hpp>

#include "impl_api.h"

using namespace sycl;

struct impl_resize_context {
    /** IMPL resize functon index*/
    int resize_func_index;
};

template <impl_interp_mtd INTERP_METHOD> constexpr int GetDataSize() {
    if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BILINEAR)
        return 2;
    else if constexpr (INTERP_METHOD == IMPL_INTERP_MTD_BICUBIC)
        return 4;
}

template <impl_interp_mtd INTERP_METHOD>
inline void get_idw(uint32_t dstid, float coef, uint32_t limit, uint32_t *id, float *weight) {
    (void)dstid;
    (void)coef;
    (void)limit;
    (void)id;
    (void)weight;
    exit(-1);
}

template <>
inline void get_idw<IMPL_INTERP_MTD_BILINEAR>(uint32_t dstid, float coef, uint32_t limit, uint32_t *id, float *weight) {
    float srcid = ((float)dstid + (float)0.5) * coef - (float)0.5;
    float t;
    weight[1] = sycl::fract(srcid, &t);
    weight[0] = (float)1.0 - weight[1];

    id[0] = sycl::clamp<int>(t, 0, limit - 1);
    id[1] = sycl::clamp<int>(t + 1, 0, limit - 1);
}

template <>
inline void get_idw<IMPL_INTERP_MTD_BICUBIC>(uint32_t dstid, float coef, uint32_t limit, uint32_t *id, float *weight) {
    float srcid = ((float)dstid + (float)0.5) * coef - (float)0.5;
    float t, fsrcid;
    t = sycl::fract(srcid, &fsrcid);

    float t2  = t * t;
    float t3  = t2 * t;
    weight[0] = t2 - (float)0.5 * t - (float)0.5 * t3;
    weight[1] = (float)1 - (float)2.5 * t2 + (float)1.5 * t3;
    weight[2] = (float)0.5 * t + (float)2 * t2 - (float)1.5 * t3;
    weight[3] = (float)0.5 * t3 - (float)0.5 * t2;

    id[0] = sycl::clamp<int>(fsrcid - 1, 0, limit - 1);
    id[1] = sycl::clamp<int>(fsrcid, 0, limit - 1);
    id[2] = sycl::clamp<int>(fsrcid + 1, 0, limit - 1);
    id[3] = sycl::clamp<int>(fsrcid + 2, 0, limit - 1);
}

// read(yid, xid) -> float
template <class Func>
inline float pixel_interp_bilinear(uint32_t xid[2], float xw[2], uint32_t yid[2], float yw[2], Func read) {
    float col[2];
    float value0[2], value1[2];

    value0[0] = read(yid[0], xid[0]);
    value0[1] = read(yid[0], xid[1]);
    col[0]    = value0[0] * xw[0] + value0[1] * xw[1];

    value1[0] = read(yid[1], xid[0]);
    value1[1] = read(yid[1], xid[1]);
    col[1]    = value1[0] * xw[0] + value1[1] * xw[1];

    float result = col[0] * yw[0] + col[1] * yw[1];
    return result;
}

// read(yid, xid) -> float
template <class Func>
inline float pixel_interp_bicubic(uint32_t xid[4], float xw[4], uint32_t yid[4], float yw[4], Func read) {
    float col[4];
#pragma unroll
    for (uchar i = 0; i < 4; i++) {
        float value[4];
        value[0] = read(yid[i], xid[0]);
        value[1] = read(yid[i], xid[1]);
        value[2] = read(yid[i], xid[2]);
        value[3] = read(yid[i], xid[3]);
        col[i]   = dot(*(vec<float, 4> *)value, *(vec<float, 4> *)xw);
    }
    float result = dot(*(vec<float, 4> *)col, *(vec<float, 4> *)yw);
    return result;
}

inline ushort read_yuv422ycbcr10be_lm(uint32_t x, uint32_t y, unsigned char *src_ptr, uint32_t src_pitch) {
    constexpr uchar idx_tab[2]   = {1, 3};
    constexpr uchar shift_tab[2] = {4, 8};
    constexpr uchar mask_tab[2][2]{{0x3f, 0xf0}, {0x3, 0xff}};
    uchar t[2];
    uchar off = x % 2;

    uint32_t offset = mad24(y, src_pitch, x / 2 * 5 + idx_tab[off]);
    uchar *tp       = src_ptr + offset;
    t[0]            = tp[0];
    t[1]            = tp[1];
    ushort value = ((t[0] & mask_tab[off][0]) << shift_tab[off]) | ((t[1] & mask_tab[off][1]) >> (8 - shift_tab[off]));
    return value;
}

inline ushort read_yuv422ycbcr10be_cr(uint32_t x, uint32_t y, unsigned char *src_ptr, uint32_t src_pitch) {
    uchar t[2];
    uint32_t offset = mad24(y, src_pitch, x * 5 + 2);
    uchar *tp       = src_ptr + offset;
    t[0]            = tp[0];
    t[1]            = tp[1];
    ushort value_cr = ((t[0] & 0xf) << 6) | ((t[1] >> 2) & 0x3f);
    return value_cr;
}

inline ushort read_yuv422ycbcr10be_cb(uint32_t x, uint32_t y, unsigned char *src_ptr, uint32_t src_pitch) {
    uchar t[2];
    uint32_t offset = mad24(y, src_pitch, x * 5);
    uchar *tp       = src_ptr + offset;
    t[0]            = tp[0];
    t[1]            = tp[1];
    ushort value_cb = (t[0] << 2) | ((t[1] & 0xc0) >> 6);
    return value_cb;
}

inline ushort read_v210_lm(unsigned int *src_ptr, uint32_t y_id, uint32_t x_id, uint32_t pitch_src) {
    constexpr uint32_t idx_tab[6]{0, 1, 1, 2, 3, 3};
    constexpr uchar sht_tab[6]{10, 0, 20, 10, 0, 20};
    uchar off           = x_id % 6;
    unsigned int src_dw = src_ptr[mad24(y_id, pitch_src, x_id / 6 * 4 + idx_tab[off])];
    ushort value        = (src_dw >> sht_tab[off]) & 0x3ff;
    return value;
}

inline ushort read_v210_cb(unsigned int *src_ptr, uint32_t y_id, uint32_t x_id, uint32_t pitch_src) {
    constexpr uint32_t idx_tab[3]{0, 1, 2};
    constexpr uchar sht_tab[3]{0, 10, 20};
    uchar off           = x_id % 3;
    unsigned int src_dw = src_ptr[mad24(y_id, pitch_src, x_id * 2 / 6 * 4 + idx_tab[off])];
    ushort value        = (src_dw >> sht_tab[off]) & 0x3ff;
    return value;
}

inline ushort read_v210_cr(unsigned int *src_ptr, uint32_t y_id, uint32_t x_id, uint32_t pitch_src) {
    constexpr uint32_t idx_tab[3]{0, 2, 3};
    constexpr uchar sht_tab[3]{20, 0, 10};
    uchar off           = x_id % 3;
    unsigned int src_dw = src_ptr[mad24(y_id, pitch_src, x_id * 2 / 6 * 4 + idx_tab[off])];
    ushort value        = (src_dw >> sht_tab[off]) & 0x3ff;
    return value;
}

#endif
