/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include "impl_resize_pixel.hpp"

/*******************************************************************/
// below is index init function
/*******************************************************************/
void impl_init_resize_index_table_yuv420p(struct impl_resize_params *prs, impl_resize_context *prst) {
    IMPL_ASSERT(prs->pq != NULL, "resize index table init yuv420p failed, queue is null");
    // info("init resize I420 or yuv420P10le index cache\n");
    (void)prs;
    (void)prst;
}

void impl_init_resize_index_table_V210(struct impl_resize_params *prs, impl_resize_context *prst) {
    IMPL_ASSERT(prs->pq != NULL, "resize index table init V210 failed, queue is null");
    // info("init resize V210 index cache\n");
    (void)prs;
    (void)prst;
}

void impl_init_resize_index_table_p010(struct impl_resize_params *prs, impl_resize_context *prst) {
    IMPL_ASSERT(prs->pq != NULL, "resize index table init p010 failed, queue is null");
    // info("init resize P010 index cache\n");
    (void)prs;
    (void)prst;
}

void impl_init_resize_index_table_yuv422ycbcr10be(struct impl_resize_params *prs, impl_resize_context *prst) {
    IMPL_ASSERT(prs->pq != NULL, "resize index table init yuv422ycbcr10be failed, queue is null");
    // info("init resize yuv422ycbcr10be index cache\n");
    (void)prs;
    (void)prst;
}

void impl_init_resize_index_table_y210(struct impl_resize_params *prs, impl_resize_context *prst) {
    IMPL_ASSERT(prs->pq != NULL, "resize index table inity210 failed, queue is null");
    // info("init resize y210 index cache\n");
    (void)prs;
    (void)prst;
}

typedef IMPL_STATUS (*RS_Function)(struct impl_resize_params *prs, impl_resize_context *prst, unsigned char *buf_in,
                                   unsigned char *buf_out, void *dep_evt);
constexpr RS_Function ResizeFunctionList[18] = {
    impl_resize_i420<IMPL_INTERP_MTD_BILINEAR>, // IMPL_VIDEO_I420
    impl_resize_i420<IMPL_INTERP_MTD_BICUBIC>,
    impl_resize_v210<IMPL_INTERP_MTD_BILINEAR>, // IMPL_VIDEO_V210
    impl_resize_v210<IMPL_INTERP_MTD_BICUBIC>,
    impl_resize_y210<IMPL_INTERP_MTD_BILINEAR>, // IMPL_VIDEO_Y210
    impl_resize_y210<IMPL_INTERP_MTD_BICUBIC>,
    NULL, // IMPL_VIDEO_NV12
    NULL,
    impl_resize_p010<IMPL_INTERP_MTD_BILINEAR>, // IMPL_VIDEO_P010
    impl_resize_p010<IMPL_INTERP_MTD_BICUBIC>,
    impl_resize_yuv420p10le<IMPL_INTERP_MTD_BILINEAR>, // IMPL_VIDEO_YUV420P10LE
    impl_resize_yuv420p10le<IMPL_INTERP_MTD_BICUBIC>,
    NULL, // IMPL_VIDEO_YUV422P10LE
    NULL,
    impl_resize_yuv422ycbcr10be<IMPL_INTERP_MTD_BILINEAR>, // IMPL_VIDEO_YUV422YCBCR10BE
    impl_resize_yuv422ycbcr10be<IMPL_INTERP_MTD_BICUBIC>,
    NULL, // IMPL_VIDEO_YUV422YCBCR10LE
    NULL};

IMPL_STATUS impl_resize_init(struct impl_resize_params *prs, void *&prs_context) {
    typedef void (*Table_init_function)(struct impl_resize_params * prs, impl_resize_context * prst);
    constexpr Table_init_function Table_init_function_list[9] = {
        impl_init_resize_index_table_yuv420p,         // IMPL_VIDEO_I420
        impl_init_resize_index_table_V210,            // IMPL_VIDEO_V210
        impl_init_resize_index_table_y210,            // IMPL_VIDEO_Y210
        NULL,                                         // IMPL_VIDEO_NV12
        impl_init_resize_index_table_p010,            // IMPL_VIDEO_P010
        impl_init_resize_index_table_yuv420p,         // IMPL_VIDEO_YUV420P10LE
        NULL,                                         // IMPL_VIDEO_YUV422P10LE
        impl_init_resize_index_table_yuv422ycbcr10be, // impl_init_resize_index_table_yuv422ycbcr10be
        NULL                                          // IMPL_VIDEO_YUV422YCBCR10LE
    };

    IMPL_ASSERT(prs != NULL, "resize init failed, prs is null");
    if (!(prs->src_width > 0 && prs->src_height > 0 && prs->dst_width > 0 && prs->dst_height > 0)) {
        err("%s, The parameter set is illegal \n", __func__);
        return IMPL_STATUS_INVALID_PARAMS;
    }

    prs->evt = impl_common_new_event();
    if (prs->pitch_pixel == 0)
        prs->pitch_pixel = prs->dst_width;
    if (prs->surface_height == 0)
        prs->surface_height = prs->dst_height;

    impl_resize_context *prst = new impl_resize_context;
    memset(prst, 0, sizeof(impl_resize_context));
    prs_context = (void *)prst;

    Table_init_function init_table = Table_init_function_list[prs->format];
    if (NULL == init_table) {
        err("%s, Format not supported by IMPL resize \n", __func__);
        return IMPL_STATUS_FAIL;
    }

    if (IMPL_VIDEO_V210 == prs->format && ((prs->src_width % 48 != 0) || (prs->dst_width % 48 != 0) ||
                                           (prs->src_height % 2 != 0) || (prs->dst_height % 2 != 0))) {
        err("%s, Illegal V210 size: The width must be a multiple of 48, and the height must "
            "be a multiple of 2\n",
            __func__);
        return IMPL_STATUS_FAIL;
    }

    prst->resize_func_index = (int)prs->format * 2 + (int)prs->interp_mtd;
    init_table(prs, prst);
    return IMPL_STATUS_SUCCESS;
}

IMPL_STATUS impl_resize_run(struct impl_resize_params *prs, void *prs_context, unsigned char *buf_in,
                            unsigned char *buf_out, void *dep_evt) {
    IMPL_STATUS ret           = IMPL_STATUS_SUCCESS;
    impl_resize_context *prst = (impl_resize_context *)prs_context;

    ret = ResizeFunctionList[prst->resize_func_index](prs, prst, buf_in, buf_out, dep_evt);
    IMPL_ASSERT(ret >= 0, "resize run failed");
    return ret;
}

IMPL_STATUS impl_resize_uninit(struct impl_resize_params *prs, void *prs_context) {
    IMPL_ASSERT(prs->pq, "resize uninit fail, queue is null");
    queue q = *(queue *)(prs->pq);
    impl_common_free_event(prs->evt);
    impl_resize_context *prst = (impl_resize_context *)prs_context;
    delete prst;

    return IMPL_STATUS_SUCCESS;
}
