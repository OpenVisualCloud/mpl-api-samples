/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef __IMPL_CSC_HPP__
#define __IMPL_CSC_HPP__

#include "impl_common.hpp"

struct impl_csc_context {
    /** IMPL CSC function index */
    int csc_func_index;
};

/**
 * IMPL color space conversion from nv12 to yuv422ycbcr10be.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_nv12_to_yuv422ycbcr10be(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst, void *dep_evt);

/**
 * IMPL color space conversion from v210 to yuv422p10le.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_v210_to_yuv422p10le(struct impl_csc_params *pcsc, unsigned char *buf_in, unsigned char *buf_dst,
                                         void *dep_evt);

/**
 * IMPL color space conversion from v210 to yuv422ycbcr10be.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_v210_to_yuv422ycbcr10be(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst, void *dep_evt);

/**
 * IMPL color space conversion from v210 to yuv422ycbcr10le.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_v210_to_yuv422ycbcr10le(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst, void *dep_evt);

/**
 * IMPL color space conversion from v210 to y210.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_v210_to_y210(struct impl_csc_params *pcsc, unsigned char *buf_in, unsigned char *buf_dst,
                                  void *dep_evt);

/**
 * IMPL color space conversion from y210 to yuv422ycbcr10be.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_y210_to_yuv422ycbcr10be(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst, void *dep_evt);

/**
 * IMPL color space conversion from y210 to yuv422ycbcr10le.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_y210_to_yuv422ycbcr10le(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst, void *dep_evt);

/**
 * IMPL color space conversion from y210 to yuv422p10le.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_y210_to_yuv422p10le(struct impl_csc_params *pcsc, unsigned char *buf_in, unsigned char *buf_dst,
                                         void *dep_evt);

/**
 * IMPL color space conversion from y210 to v210.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_y210_to_v210(struct impl_csc_params *pcsc, unsigned char *buf_in, unsigned char *buf_dst,
                                  void *dep_evt);

/**
 * IMPL color space conversion from yuv422p10le to y210.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_yuv422p10le_to_y210(struct impl_csc_params *pcsc, unsigned char *buf_in, unsigned char *buf_dst,
                                         void *dep_evt);

/**
 * IMPL color space conversion from yuv422p10le to v210.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_yuv422p10le_to_v210(struct impl_csc_params *pcsc, unsigned char *buf_in, unsigned char *buf_dst,
                                         void *dep_evt);

/**
 * IMPL color space conversion from yuv422p10le to yuv422ycbcr10be.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_yuv422p10le_to_yuv422ycbcr10be(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                                    unsigned char *buf_dst, void *dep_evt);

/**
 * IMPL color space conversion from yuv422ycbcr10be to y210.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_yuv422ycbcr10be_to_y210(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst, void *dep_evt);

/**
 * IMPL color space conversion from yuv422ycbcr10be to nv12.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_yuv422ycbcr10be_to_nv12(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst, void *dep_evt);

/**
 * IMPL color space conversion from yuv422ycbcr10be to v210.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_yuv422ycbcr10be_to_v210(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst, void *dep_evt);

/**
 * IMPL color space conversion from yuv422ycbcr10be to yuv422p10le.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_yuv422ycbcr10be_to_yuv422p10le(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                                    unsigned char *buf_dst, void *dep_evt);

/**
 * IMPL color space conversion from yuv422ycbcr10be to yuv420p10le.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_yuv422ycbcr10be_to_yuv420p10le(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                                    unsigned char *buf_dst, void *dep_evt);

/**
 * IMPL color space conversion from yuv422ycbcr10be to i420.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_yuv422ycbcr10be_to_i420(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst, void *dep_evt);

/**
 * IMPL color space conversion from yuv422ycbcr10be to p010.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_yuv422ycbcr10be_to_p010(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst, void *dep_evt);

/**
 * IMPL color space conversion from yuv422ycbcr10le to y210.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_yuv422ycbcr10le_to_y210(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst, void *dep_evt);

/**
 * IMPL color space conversion from yuv422ycbcr10le to v210.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_dst
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC event needs to depend on.
 *   After dep_evt ends, CSC event runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - else: Error code if failed.
 */
IMPL_STATUS impl_csc_yuv422ycbcr10le_to_v210(struct impl_csc_params *pcsc, unsigned char *buf_in,
                                             unsigned char *buf_dst, void *dep_evt);

#endif // __IMPL_CSC_HPP__
