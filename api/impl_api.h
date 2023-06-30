/* SPDX-License-Identifier: BSD-3-Clause
* Copyright(c) 2023 Intel Corporation
*/

#ifndef __IMPL_API_H__
#define __IMPL_API_H__

#define IMPL_API __attribute__ ((visibility ("default")))

#include <stdbool.h>
#include <stddef.h>
/**
 * IMPL version number
 */
#define IMPL_VERSION 23.06

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * IMPL API values for return
 */
typedef enum {
  IMPL_STATUS_SUCCESS = 0,
  IMPL_STATUS_INVALID_PARAMS,
  IMPL_STATUS_FAIL,
  IMPL_STATUS_MAX
} IMPL_STATUS;

/**
 * IMPL video format type
 */
typedef enum {
    IMPL_VIDEO_I420,
    IMPL_VIDEO_V210,
    IMPL_VIDEO_Y210,
    IMPL_VIDEO_NV12,
    IMPL_VIDEO_P010,
    IMPL_VIDEO_YUV420P10LE,
    IMPL_VIDEO_YUV422P10LE,
    IMPL_VIDEO_YUV422YCBCR10BE,
    IMPL_VIDEO_YUV422YCBCR10LE,
    IMPL_VIDEO_MAX
} impl_video_format;

/**
 * IMPL memory type
 */
typedef enum {
    /** IMPL memory device type */
    IMPL_MEM_TYPE_DEVICE = 0,
    /** IMPL memory host type */
    IMPL_MEM_TYPE_HOST,
    /** IMPL memory shared type */
    IMPL_MEM_TYPE_SHARED,
    IMPL_MEM_TYPE_MAX
} impl_mem_type;

/**
 * IMPL interpolate method for resize
 */
typedef enum {
    /** IMPL bilinear interpolate method */
    IMPL_INTERP_MTD_BILINEAR = 0,
    /** IMPL bicubic interpolate method */
    IMPL_INTERP_MTD_BICUBIC,
    IMPL_INTERP_MTD_MAX
} impl_interp_mtd;

/**
 * Retrieve IMPL version value
 */
IMPL_API float impl_version();

/**
 * initialize IMPL to generate a new queue.
 *
 * @param is_target_cpu
 *   Indicate if the target is CPU or GPU.
 * @param enable_profiling
 *   Indicate if enabling profiling for the queue.
 * @return
 *   - void queue pointer.
 */
IMPL_API void* impl_common_init(bool is_target_cpu, bool enable_profiling);

/**
 * allocate IMPL Unified Shared Memory(USM).
 *
 * @param pq
 *   The queue pointer.
 * @param ebytes
 *   The bytes per element.
 * @param num_element
 *   The element number.
 * @param type
 *   Indicate the memory type to allocate the buffer
 * @return
 *   - allocated buffer pointer.
 */
IMPL_API void* impl_common_mem_alloc(void *pq, int ebytes, int num_element, impl_mem_type type);

/**
 * allocate IMPL image USM memory
 *
 * @param pq
 *   The queue pointer.
 * @param format
 *   IMPL video format type.
 * @param width
 *   Image width.
 * @param height
 *   Image height.
 * @param type
 *   Indicate the memory type to allocate the buffer
 * @param allocated_size
 *   allocated buffer size in bytes
 * @return
 *   - allocated buffer pointer
 */
IMPL_API unsigned char* impl_image_mem_alloc(void *pq, impl_video_format format, int width, int height, impl_mem_type type, size_t *allocated_size);

/**
 * IMPL USM memory copy.
 *
 * @param pq
 *   The queue pointer.
 * @param evt
 *   The memcopy event pointer.
 * @param dst
 *   The dstenation memory.
 * @param src
 *   The source memory.
 * @param size
 *   The copy size in unit of unsigned char.
 * @param dep_evt
 *   The dep_evt is the event that memcpy needs to depend on.
 *   After dep_evt ends, memcpy runs.
 * @param is_sync
 *   Indicate if copying is synchronous mode or asynchronous mode.
 */
IMPL_API void impl_common_mem_copy(void *pq, void *evt, void *dst, void *src, int size, void *dep_evt, bool is_sync);

/**
 * IMPL synchronous waiting queue.
 *
 * @param pq
 *   The queue pointer.
 */
IMPL_API void impl_common_queue_sync(void *pq);

/**
 * IMPL synchronous waiting event.
 *
 * @param evt
 *   The event pointer.
 */
IMPL_API void impl_common_event_sync(void *evt);

/**
 * IMPL event profiling.
 *
 * @param evt
 *   The event pointer.
 * @return
 *   - profiling event time nseconds.
 */
IMPL_API double impl_common_event_profiling(void *evt);

/**
 * free IMPL GPU USM memory.
 *
 * @param pq
 *   The queue pointer.
 * @param buf
 *   memory pointer to be free.
 */
IMPL_API void impl_common_mem_free(void *pq, void *buf);

/**
 * uninitialize IMPL queue.
 *
 * @param pq
 *   The queue pointer.
 */
IMPL_API void impl_common_uninit(void *pq);

/**
 * allocate IMPL a new event.
 *
 * @return
 *   - allocated event pointer.
 */
IMPL_API void* impl_common_new_event();

/**
 * free IMPL an event.
 *
 * @param evt
 *   The event pointer.
 */
IMPL_API void impl_common_free_event(void *evt);

/**
 * IMPL color space conversion(CSC) structure
 */
struct impl_csc_params {
    /** void queue pointer */
    void *pq;
    /** void CSC event pointer */
    void *evt;
    /** is asynchronous mode, 1: asynchronous, 0: synchronous(default) */
    bool is_async;
    /** IMPL CSC input video format */
    impl_video_format in_format;
    /** IMPL CSC output video format */
    impl_video_format out_format;
    /** IMPL CSC source width */
    int width;
    /** IMPL CSC source height */
    int height;
};

/**
 * IMPL color space conversion structure initialize
 *
 * @param pcsc
 *   IMPL impl_csc_params pointer
 *   All CSC parameters must be set before impl_csc_init
 * @param pcsc_context
 *   The CSC context will be created
 * @return
 *   - IMPL_STATUS_SUCCESS for success
 *   - IMPL_STATUS_INVALID_PARAMS for invalid parameters
 *   - IMPL_STATUS_FAIL for other failure
 */
IMPL_API IMPL_STATUS impl_csc_init(struct impl_csc_params *pcsc, void *&pcsc_context);

/**
 * IMPL color space conversion process runs.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param pcsc_context
 *   void CSC context pointer
 * @param buf_in
 *   Source video buffer pointer.
 * @param buf_dst
 *   Destination video buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that CSC needs to depend on.
 *   After dep_evt ends, CSC runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - IMPL_STATUS_INVALID_PARAMS for invalid parameters
 *   - IMPL_STATUS_FAIL for other failure
 */
IMPL_API IMPL_STATUS impl_csc_run(struct impl_csc_params *pcsc, void *pcsc_context, unsigned char *buf_in, unsigned char *buf_dst, void *dep_evt);

/**
 * IMPL color space conversion structure free.
 *
 * @param pcsc
 *   The impl_csc_params pointer.
 * @param pcsc_context
 *   The CSC context pointer, will be released.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - IMPL_STATUS_INVALID_PARAMS for invalid parameters
 *   - IMPL_STATUS_FAIL for other failure
 */
IMPL_API IMPL_STATUS impl_csc_uninit(struct impl_csc_params *pcsc, void *pcsc_context);

/**
 * IMPL resize parameters
 */
struct impl_resize_params {
    /** void queue pointer */
    void *pq;
    /** void event pointer */
    void *evt;
    /** if asynchronous mode */
    bool is_async;
    /** interpolation method, defalut is IMPL_INTERP_MTD_BILINEAR */
    impl_interp_mtd interp_mtd;
    /** IMPL video format */
    impl_video_format format;
    /** IMPL resize source width */
    int src_width;
    /** IMPL resize source height */
    int src_height;
    /** IMPL resize destination width */
    int dst_width;
    /** IMPL resize destination height */
    int dst_height;    
    /** X coordinate for first pixel in output surface */
    int offset_x;
    /** Y coordinate for first pixel in output surface */
    int offset_y;
    /** Stride in pixel of output surface */
    int pitch_pixel;
    /** Hight in pixel of output surface */
    int surface_height;
};

/**
 * IMPL resize initialize
 *
 * supported resize formats and methods valid check
 * resize resource allocate
 * initialize resize parameters
 *
 * @param prs
 *   The struct impl_resize_params pointer.
 *   All resize parameters must be set before impl_resize_init
 * @param prs_context
 *   The resize context will be created
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - IMPL_STATUS_INVALID_PARAMS for invalid parameters
 *   - IMPL_STATUS_FAIL for other failure
 */
IMPL_API IMPL_STATUS impl_resize_init(struct impl_resize_params *prs, void *&prs_context);
/**
 * IMPL resize image function
 *
 * @param prs
 *   The struct impl_resize_params pointer.
 * @param prs_context
 *   void resize context pointer
 * @param buf_in
 *   Source buffer pointer.
 * @param buf_out
 *   Desternation buffer pointer.
 * @param dep_evt
 *   dep_evt is the event that resize needs to depend on.
 *   After dep_evt ends, resize runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - IMPL_STATUS_INVALID_PARAMS for invalid parameters
 *   - IMPL_STATUS_FAIL for other failure
 */
IMPL_API IMPL_STATUS impl_resize_run(struct impl_resize_params *prs, void *prs_context, unsigned char *buf_in, unsigned char *buf_out, void *dep_evt);

/**
 * IMPL resize uninitialize.
 *
 * resize resource free
 *
 * @param prs
 *   The struct impl_resize_params pointer.
 * @param prs_context
 *   The void resize context pointer, will be released
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - IMPL_STATUS_INVALID_PARAMS for invalid parameters
 *   - IMPL_STATUS_FAIL for other failure
 */
IMPL_API IMPL_STATUS impl_resize_uninit(struct impl_resize_params *prs, void *prs_context);

/** IMPL mixer max supported fields number */
#define IMPL_MIXER_MAX_FIELDS 20

/**
 * IMPL mixer field parameters, for composition and alphablending
 */
struct impl_mixer_field_params {
    /** void event pointer */
    void *evt;
    /** IMPL mixer one field data buffer */
    void *buff;
    /** IMPL mixer field index, should be < IMPL_MIXER_MAX_FIELDS */
    int field_idx;
    /** IMPL mixer one field source width */
    int width;
    /** IMPL mixer one field source height */
    int height;
    /** IMPL mixer one field x direction overlapping offset */
    int offset_x;
    /** IMPL mixer one field y direction overlapping offset */
    int offset_y;
    /** IMPL mixer one field x direction crop start coordinate */
    int crop_x;
    /** IMPL mixer one field y direction crop start coordinate */
    int crop_y;
    /** IMPL mixer field crop width */
    int crop_w;
    /** IMPL mixer field crop height */
    int crop_h;
    /** Choose a static alpha value for alphablending */
    int static_alpha;
    /** Choose a non-static alpha value file */
    unsigned char *alpha_surf;
    /** Whether alphablending or composition */
    bool is_alphab;
};

/**
 * IMPL mixer parameters, for composition and alphablending
 */
struct impl_mixer_params {
    /** queue pointer */
    void *pq;
    /** max field layers supported, should be <= IMPL_MIXER_MAX_FIELDS */
    int layers;
    /** if asynchronous mode */
    bool is_async;
    /** IMPL video format */
    impl_video_format format;
    /** IMPL mixer field parameters, field[0] is input and output, field[1~IMPL_MIXER_MAX_FIELDS-1] are input */
    struct impl_mixer_field_params field[IMPL_MIXER_MAX_FIELDS];
};

/**
 * IMPL mixer initialize parameters
 *
 * supported mixer formats and parmeters valid check
 * mixer resource allocate
 * initialize mixer parameters
 *
 * @param pmixer
 *   The struct impl_mixer_params pointer.
 *   All mixer parameters must be set before impl_mixer_init
 * @param pmx_context
 *   The mixer context will be created
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - IMPL_STATUS_INVALID_PARAMS for invalid parameters
 *   - IMPL_STATUS_FAIL for other failure
 */
IMPL_API IMPL_STATUS impl_mixer_init(struct impl_mixer_params *pmixer, void *&pmx_context);

/**
 * IMPL composition or alphablending starts running
 *
 * @param pmixer
 *   The struct impl_mixer_params pointer
 * @param pmx_context
 *   The mixer context pointer
 * @param fieldbuffs
 *   The field buffers pointer array to be updated, nullptr means not updating pmixer->field[].buff
 * @param dep_evts
 *   The dep_evts is the event pointer array that field mixer needs to depend on,
 *   nullptr field event means no event to depend on
 *   After dep_evts[] ends, all fields mixing runs
 * @return
 *   - IMPL_STATUS_SUCCESS if successful
 *   - IMPL_STATUS_INVALID_PARAMS for invalid parameters
 *   - IMPL_STATUS_FAIL for other failure
 */
IMPL_API IMPL_STATUS impl_mixer_run(struct impl_mixer_params *pmixer, void *pmx_context, void *fieldbuffs[], void *dep_evts[]);

/**
 * IMPL composition or alphablending free
 *
 * mixer resource free
 *
 * @param pmixer
 *   The struct impl_mixer_params pointer
 * @param pmx_context
 *   The mixer context pointer, will be released
 * @return
 *   - IMPL_STATUS_SUCCESS if successful
 *   - IMPL_STATUS_INVALID_PARAMS for invalid parameters
 *   - IMPL_STATUS_FAIL for other failure
 */
IMPL_API IMPL_STATUS impl_mixer_uninit(struct impl_mixer_params *pmixer, void *pmx_context);

/**
 * IMPL rotation parameters
 */
struct impl_rotation_params {
    /** void queue pointer */
    void *pq;
    /** void event pointer */
    void *evt;
    /** if asynchronous mode */
    bool is_async;
    /** IMPL rotation video format */
    impl_video_format format;
    /** IMPL rotation source width */
    int src_width;
    /** IMPL rotation source height */
    int src_height;
    /** IMPL rotation source width */
    int dst_width;
    /** IMPL rotation source height */
    int dst_height;
    /** IMPL rotation angle */
    int angle;
    /** IMPL rotation function index */
    int rotation_func_index;
};

/**
 * IMPL rotation initialize.
 *
 * @param prt
 *   The impl_rotation_params pointer.
 *   All rotation parameters must be set before impl_rotation_init
 * @param prt_context
 *   The rotation context will be created.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - IMPL_STATUS_INVALID_PARAMS for invalid parameters
 *   - IMPL_STATUS_FAIL for other failure
 */
IMPL_API IMPL_STATUS impl_rotation_init(struct impl_rotation_params *prt, void *&prt_context);

/**
 * IMPL rotation starts running.
 *
 * @param prt
 *   The impl_rotation_params pointer.
 * @param prt_context
 *   void rotation context pointer.
 * @param buf_in
 *   The source buffer.
 * @param buf_out
 *   The destination buffer.
 * @param dep_evt
 *   dep_evt is the event that rotation needs to depend on.
 *   After dep_evt ends, rotation runs.
 * @return
 *   - IMPL_STATUS_SUCCESS if successful.
 *   - IMPL_STATUS_INVALID_PARAMS for invalid parameters
 *   - IMPL_STATUS_FAIL for other failure
 */
IMPL_API IMPL_STATUS impl_rotation_run(struct impl_rotation_params *prt, void *prt_context, unsigned char *buf_in, unsigned char *buf_out, void *dep_evt);

/**
 * IMPL rotation free.
 *
 * @param prt
 *   The impl_rotation_params pointer.
 * @param prt_context
 *   The rotation context, will be released
 */
IMPL_API void impl_rotation_uninit(struct impl_rotation_params *prt, void *prt_context);

#ifdef __cplusplus
}
#endif

#endif  // __IMPL_API_H__
