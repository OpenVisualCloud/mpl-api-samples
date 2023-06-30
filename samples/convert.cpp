/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <chrono>

#include "defines.hpp"
#include "getargs.hpp"
#include "impl_api.h"
#include "log.h"
// Convert pipeline
// 1080p V210 --> IMPL CSC to Y210 --> HW Scaling + 3DLUT --> 480p ARGB or P010

#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>
#include <va/va_vpp.h>

#include "vpl/mfxvideo.h"
#if (MFX_VERSION >= 2000)
#include "vpl/mfxdispatcher.h"
#endif

#include <CL/sycl.hpp>
#include <fcntl.h>
#include <level_zero/ze_api.h>

#include "examples_util.h"

#ifdef __SYCL_DEVICE_ONLY__
#define CONSTANT __attribute__((opencl_constant))
#else
#define CONSTANT
#endif

using namespace sycl;

inline void getArgsConvert(int argc, char **argv, char *pfilename, char *poutfilename, int &frames,
                           char *p3dlutfilename, bool &is_p010, bool &is2call, bool &enable_profiling, bool &pre_read) {
    std::string infile, outfile, p3dlut_name;
    ParseContext P;

    P.GetCommand(argc, argv);
    P.get("-frame", "n                  check n frames", &frames, (int)1, false);
    P.get("-profile", "                 enable profiling, disabled by default", &enable_profiling, (bool)false, false);
    P.get("-3dlut", "3dlut_file         3dlut input file", &p3dlut_name, (std::string) "", true);
    P.get("-is_p010", "                 set output format to P010", &is_p010, (bool)false, false);
    P.get("-o", "output_filename        set output picture filename", &outfile, (std::string) "", false);
    P.get("-i", "input_filename         set input picture file", &infile, (std::string) "", true);
    P.get("-is2call", "                 set vpp HW 2 calls, else 1 call", &is2call, (bool)false, false);
    P.get("-pre_read", "                pre_read one frame in gpu buffer", &pre_read, (bool)false, false);
    P.check("usage:\tconvert [options]\noptions:");

    memcpy(pfilename, infile.c_str(), infile.length() + 1);
    memcpy(poutfilename, outfile.c_str(), outfile.length() + 1);
    memcpy(p3dlutfilename, p3dlut_name.c_str(), p3dlut_name.length() + 1);
    info("input yuv %s output yuv format %s is2call %d profile %d\n", pfilename, is_p010 ? "P010" : "ARGB", is2call,
         enable_profiling);
}

void *cpy_evt             = NULL;
double duration_3dlutcopy = 0.0;
bool output_file          = false;
bool enable_profiling     = false;
bool pre_read             = false;
// pre-read in N frames and process only N frames
int N   = 20;
int ret = 0;

static VADisplay va_dpy             = NULL;
static VASurfaceID surface_id_3dlut = VA_INVALID_ID;
static bool is_p010                 = false;

mfxLoader loader   = NULL;
mfxSession session = NULL;

ze_context_handle_t ze_context = {};
ze_device_handle_t ze_device   = {};
ze_result_t ze_res             = ZE_RESULT_SUCCESS;
static int dma_buf_fd          = 0;
/* is2call indicates the VPP HW pipleine is scaling(1pass) -> 3dlut, two seperate calls for best performance, else 1call
 * indicates the pipleine is scaling(2pass) -> 3dlut for best quality, by default is 1call */
static bool is2call               = false;
static VAContextID context_id     = 0;
static VAConfigID config_id       = 0;
static VASurfaceID in_surface_id  = VA_INVALID_ID;
static VASurfaceID out_surface_id = VA_INVALID_ID;
static uint16_t seg_size_3dlut    = 65;
static uint16_t mul_size_3dlut    = 128;
static uint32_t out_fourcc        = VA_FOURCC_P010;
/* only for VA_SCALING_3DLUT scaled surface */
static VASurfaceID inter_surface_id = VA_INVALID_ID;

#define MAJOR_API_VERSION_REQUIRED 2
#define MINOR_API_VERSION_REQUIRED 2
#define VPLVERSION(major, minor) (major << 16 | minor)
#define TARGETKBPS 4000
#define FRAMERATE 30
#define BITSTREAM_BUFFER_SIZE 2000000

static FILE *out_fp = NULL;

static char filename_3dlut[512] = "";
static int is_3dlut_support     = 1;
double duration_cpu             = 0.0;
double duration_copy            = 0.0;
double gpu_time_ns              = 0.0;
double cpy_time_ns              = 0.0;
int width_rs                    = 0;
int height_rs                   = 0;

struct impl_csc_params csc_params;
void *pcsc_context = NULL;

void store_surface_to_file(VASurfaceID surface_id, FILE *out_fp) {
    VAStatus va_status;
    va_status = vaSyncSurface(va_dpy, surface_id);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaSyncSurface");

    // GPU memory Map to CPU --> copy GPU to CPU --> UnMap back to GPU
    VAImage surf_image;
    unsigned char *y_src = NULL;
    unsigned char *u_src = NULL;
    unsigned char *y_dst = NULL;
    unsigned char *u_dst = NULL;
    void *surf_ptr       = NULL;
    size_t n_items;
    unsigned char *buf_image = NULL;

    va_status = vaDeriveImage(va_dpy, surface_id, &surf_image);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaDeriveImage");

    va_status = vaMapBuffer(va_dpy, surf_image.buf, &surf_ptr);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaMapBuffer");

    if (surf_image.format.fourcc == VA_FOURCC_P010) {
        // P010 store
        uint32_t y_size = surf_image.width * surf_image.height * 2;

        buf_image = (unsigned char *)malloc(y_size * 3);
        assert(buf_image);
        y_dst = buf_image;

        y_src = (unsigned char *)((unsigned char *)surf_ptr + surf_image.offsets[0]);

        /* Y plane copy */
        for (int row = 0; row < surf_image.height; row++) {
            memcpy(y_dst, y_src, surf_image.width * 2);
            y_src += surf_image.pitches[0];
            y_dst += surf_image.width * 2;
        }

        u_dst = buf_image + y_size;

        u_src = (unsigned char *)((unsigned char *)surf_ptr + surf_image.offsets[1]);

        for (int row = 0; row < surf_image.height / 2; row++) {
            memcpy(u_dst, u_src, surf_image.width * 2);
            u_dst += surf_image.width * 2;
            u_src += surf_image.pitches[1];
        }
        // info("---> store 3dlut output p010 wxh=%d %d, pitch0=%d offset0=%d\n", surf_image.width,
        // surf_image.height, surf_image.pitches[0], surf_image.offsets[0]);

        /* write frame to file */
        do {
            n_items = fwrite(buf_image, y_size * 3 / 2, 1, out_fp);
        } while (n_items != 1);

        if (buf_image) {
            free(buf_image);
            buf_image = NULL;
        }
    } else if (surf_image.format.fourcc == VA_FOURCC_BGRA || surf_image.format.fourcc == VA_FOURCC_ARGB) {
        // out_fourcc == VA_FOURCC_ARGB
        //  bgra store
        uint32_t size = surf_image.width * surf_image.height * 4;

        buf_image = (unsigned char *)malloc(size);
        assert(buf_image);

        /* stored as ARGB format */
        y_dst = buf_image;

        y_src = (unsigned char *)((unsigned char *)surf_ptr + surf_image.offsets[0]);

        // memcpy(y_dst, y_src, size);
        for (int i = 0; i < surf_image.height; i++) {
            memcpy(y_dst, y_src, surf_image.width * 4);
            y_dst += surf_image.width * 4;
            y_src += surf_image.pitches[0];
        }
        // info("---> store ARGB 0x%x output  wxh=%d %d, pitch0=%d offset0=%d\n", surf_image.format.fourcc,
        // surf_image.width, surf_image.height, surf_image.pitches[0], surf_image.offsets[0]);

        /* write frame to file */
        do {
            n_items = fwrite(buf_image, size, 1, out_fp);
        } while (n_items != 1);

        if (buf_image) {
            free(buf_image);
            buf_image = NULL;
        }
    } else {
        info("Not supported YUV surface fourcc 0x%x!!! \n", surf_image.format.fourcc);
    }

    vaUnmapBuffer(va_dpy, surf_image.buf);
    vaDestroyImage(va_dpy, surf_image.image_id);

    return;
}

static VAStatus upload_3dlut_data(FILE *fp, VASurfaceID &surface_id) {
    VAStatus va_status;
    VAImage surf_image;
    void *surf_ptr = NULL;
    uint32_t frame_size, lut3d_size;
    unsigned char *buf_image = NULL;
    va_status                = vaSyncSurface(va_dpy, surface_id);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaSyncSurface");

    va_status = vaDeriveImage(va_dpy, surface_id, &surf_image);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaDeriveImage");

    va_status = vaMapBuffer(va_dpy, surf_image.buf, &surf_ptr);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaMapBuffer");

    if (surf_image.format.fourcc == VA_FOURCC_RGBA && fp) {
        /* 3DLUT surface is allocated to 32 bit RGB */
        frame_size = surf_image.width * surf_image.height * 4;
        buf_image  = (unsigned char *)malloc(frame_size);
        assert(buf_image);

        fseek(fp, 0L, SEEK_END);
        lut3d_size = ftell(fp);
        rewind(fp);

        uint32_t real_size = (frame_size > lut3d_size) ? lut3d_size : frame_size;

        if (fread(buf_image, real_size, 1, fp) != 0) {
            memcpy(surf_ptr, buf_image, real_size);
            info("upload_3dlut_data: 3DLUT surface width %d, height %d, pitch %d, frame size %d, 3dlut file size: "
                 "%d\n",
                 surf_image.width, surf_image.height, surf_image.pitches[0], frame_size, lut3d_size);
        }
    }

    if (buf_image) {
        free(buf_image);
        buf_image = NULL;
    }

    vaUnmapBuffer(va_dpy, surf_image.buf);
    vaDestroyImage(va_dpy, surf_image.image_id);

    return VA_STATUS_SUCCESS;
}

static VAStatus create_surface(VASurfaceID *p_surface_id, int is_linear, uint32_t width, uint32_t height,
                               uint32_t fourCC, uint32_t format) {
    VAStatus va_status;
    if (is_linear) {
        VASurfaceAttrib surface_attrib[3];
        VASurfaceAttribExternalBuffers extBuffer;
        surface_attrib[0].flags         = VA_SURFACE_ATTRIB_SETTABLE;
        surface_attrib[0].type          = VASurfaceAttribPixelFormat;
        surface_attrib[0].value.type    = VAGenericValueTypeInteger;
        surface_attrib[0].value.value.i = fourCC;

        surface_attrib[1].type          = VASurfaceAttribMemoryType;
        surface_attrib[1].flags         = VA_SURFACE_ATTRIB_SETTABLE;
        surface_attrib[1].value.type    = VAGenericValueTypeInteger;
        surface_attrib[1].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_VA;

        surface_attrib[2].flags         = VA_SURFACE_ATTRIB_SETTABLE;
        surface_attrib[2].type          = VASurfaceAttribExternalBufferDescriptor;
        surface_attrib[2].value.type    = VAGenericValueTypePointer;
        surface_attrib[2].value.value.p = (void *)&extBuffer;
        memset(&extBuffer, 0, sizeof(extBuffer));
        // extBuffer.flags = (~VA_SURFACE_EXTBUF_DESC_ENABLE_TILING);
        extBuffer.flags        = 0;
        extBuffer.pixel_format = fourCC;

        uint32_t base_addr_align = 0x1000;
        uint32_t size            = 0;
        uint32_t pitch_align     = 16; // surf.alignsize;
        switch (fourCC) {
        // case VA_FOURCC_YUY2:
        //     extBuffer.pitches[0] = (((width << 1)  + pitch_align - 1)/pitch_align) * pitch_align;
        //     extBuffer.data_size = extBuffer.pitches[0] *(height + 2);
        //     extBuffer.offsets[0] = 0;
        //     extBuffer.num_planes = 1;
        //     break;
        case VA_FOURCC_P010:
            // extBuffer.pixel_format = VA_FOURCC_P010;
            extBuffer.pitches[0] = (((width << 1) + pitch_align - 1) / pitch_align) * pitch_align;
            size                 = (extBuffer.pitches[0] * height) * 3 / 2;
            size = (size + base_addr_align - 1) / base_addr_align * base_addr_align; // frame size align as 4K page.
            extBuffer.data_size  = size;
            extBuffer.offsets[0] = 0;
            extBuffer.offsets[1] = extBuffer.pitches[0] * height;
            extBuffer.pitches[1] = extBuffer.pitches[0];
            extBuffer.num_planes = 2;
            // info("---> create input VA P010 surface pitch=%d width=%d height=%d size=%d\n",
            //  extBuffer.pitches[0], width, height, size);
            break;
        case VA_FOURCC_Y210:
            // extBuffer.pixel_format = VA_FOURCC_Y210;
            extBuffer.pitches[0] = (((width << 1) + pitch_align - 1) / pitch_align) * pitch_align;
            size                 = (extBuffer.pitches[0] * height) * 2;
            size = (size + base_addr_align - 1) / base_addr_align * base_addr_align; // frame size align as 4K page.
            extBuffer.data_size  = size;
            extBuffer.offsets[0] = 0;
            extBuffer.num_planes = 1;
            // info("---> create input VA Y210 surface pitch=%d width=%d height=%d size=%d\n",
            //  extBuffer.pitches[0], width, height, size);
            break;
        default:
            info("---> %s %d format doesn't support!", __func__, fourCC);
            return VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT;
        }

        extBuffer.width  = width;
        extBuffer.height = height;
        va_status        = vaCreateSurfaces(va_dpy, format, width, height, p_surface_id, 1, surface_attrib, 3);
    } else {
        VASurfaceAttrib surface_attrib;
        surface_attrib.type          = VASurfaceAttribPixelFormat;
        surface_attrib.flags         = VA_SURFACE_ATTRIB_SETTABLE;
        surface_attrib.value.type    = VAGenericValueTypeInteger;
        surface_attrib.value.value.i = fourCC;

        va_status = vaCreateSurfaces(va_dpy, format, width, height, p_surface_id, 1, &surface_attrib, 1);
    }
    return va_status;
}

#define MAJOR_API_VERSION_REQUIRED 2
#define MINOR_API_VERSION_REQUIRED 2
#define VPLVERSION(major, minor) (major << 16 | minor)

inline void bind_queue_va(void *pq) {
    mfxStatus sts = MFX_ERR_NONE;
    mfxConfig mfxCfg[2];

    // Initialize oneVPL MFX session
    loader = MFXLoad();
    CHECK_STATUS(NULL != loader, "MFXLoad failed");

    // MFX config setting
    mfxCfg[0] = MFXCreateConfig(loader);
    CHECK_STATUS(NULL != mfxCfg[0], "MFXCreateConfig failed")

    mfxVariant mfxImplValue;
    mfxImplValue.Type     = MFX_VARIANT_TYPE_U32;
    mfxImplValue.Data.U32 = MFX_IMPL_TYPE_HARDWARE;
    sts                   = MFXSetConfigFilterProperty(mfxCfg[0], (mfxU8 *)"mfxImplDescription.Impl", mfxImplValue);
    CHECK_STATUS(MFX_ERR_NONE == sts, "MFXSetConfigFilterProperty failed for Implementation");
    // Implementation used must provide MFX API version 2.2 or newer
    mfxVariant mfxCfgVal;
    mfxCfg[1] = MFXCreateConfig(loader);
    CHECK_STATUS(NULL != mfxCfg[1], "MFXCreateConfig failed")
    mfxCfgVal.Type     = MFX_VARIANT_TYPE_U32;
    mfxCfgVal.Data.U32 = VPLVERSION(MAJOR_API_VERSION_REQUIRED, MINOR_API_VERSION_REQUIRED);
    sts = MFXSetConfigFilterProperty(mfxCfg[1], (mfxU8 *)"mfxImplDescription.ApiVersion.Version", mfxCfgVal);
    CHECK_STATUS(MFX_ERR_NONE == sts, "MFXSetConfigFilterProperty failed for MFX API version");

    queue q               = *(queue *)pq;
    mfxU32 desiredDevice  = 0;
    mfxU32 sessionIdx     = 0;
    int desiredSessionIdx = -1;
    mfxImplDescription *mfxImplDesc;

    // Get Level-zero ze context and device
    ze_context = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(q.get_context());
    ze_device  = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(q.get_device());

    if (q.get_device().get_info<sycl::info::device::device_type>() == sycl::info::device_type::gpu) {
        ze_device_properties_t deviceProperties;
        zeDeviceGetProperties(ze_device, &deviceProperties);
        desiredDevice = (mfxU32)deviceProperties.deviceId;
    }

    // Match oneVPL MFX implementation with sycl::q device
    while (MFX_ERR_NOT_FOUND !=
           MFXEnumImplementations(loader, sessionIdx, MFX_IMPLCAPS_IMPLDESCSTRUCTURE, (mfxHDL *)&mfxImplDesc)) {
        mfxU32 devID = (mfxU32)std::stoi(mfxImplDesc->Dev.DeviceID, nullptr, 16);
        if (devID == desiredDevice) {
            desiredSessionIdx = (int)sessionIdx;
            MFXDispReleaseImplDescription(loader, mfxImplDesc);
            break;
        }
        MFXDispReleaseImplDescription(loader, mfxImplDesc);
        sessionIdx++;
    }

    CHECK_STATUS(desiredSessionIdx >= 0, "Cannot create session -- invalid desired session index");
    sts = MFXCreateSession(loader, desiredSessionIdx, &session);
    CHECK_STATUS(MFX_ERR_NONE == sts, "Cannot create session -- no implementations meet desired session index");

    // open VA display and set handle
    mfxIMPL mfxImpl;
    sts = MFXQueryIMPL(session, &mfxImpl);
    CHECK_STATUS(MFX_ERR_NONE == sts, "MFXQueryIMPL");

    if ((mfxImpl & MFX_IMPL_VIA_VAAPI) == MFX_IMPL_VIA_VAAPI) {
        // Set session handle
        if (va_dpy) {
            MFXVideoCORE_SetHandle(session, static_cast<mfxHandleType>(MFX_HANDLE_VA_DISPLAY), va_dpy);
        }
    }
}

static VAStatus vpp_create_context(void *pq, int width, int height, int width_rs, int height_rs) {
    VAStatus va_status = VA_STATUS_SUCCESS;
    int32_t j;
    int drm_fd = -1;

    /* VA driver initialization */
    static const char *drm_device_paths[] = {"/dev/dri/renderD128", "/dev/dri/card0", NULL};
    for (int i = 0; drm_device_paths[i]; i++) {
        drm_fd = open(drm_device_paths[i], O_RDWR);
        if (drm_fd < 0)
            continue;
        va_dpy = vaGetDisplayDRM(drm_fd);
        if (va_dpy)
            break;
        close(drm_fd);
        drm_fd = -1;
    }

    int32_t major_version, minor_version;
    va_status = vaInitialize(va_dpy, &major_version, &minor_version);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "Error in vaInitialize");

    bind_queue_va(pq);

    /* Check whether Video Processing HW is supported by vaapi */
    VAEntrypoint entrypoints[5];
    int32_t entrypoints_num;
    entrypoints_num = vaMaxNumEntrypoints(va_dpy);
    va_status       = vaQueryConfigEntrypoints(va_dpy, VAProfileNone, entrypoints, &entrypoints_num);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "Error in vaQueryConfigEntrypoints");

    for (j = 0; j < entrypoints_num; j++) {
        if (entrypoints[j] == VAEntrypointVideoProc)
            break;
    }

    if (j == entrypoints_num) {
        info("Video Processing Hardware is not supported by vaapi\n");
        assert(0);
    }

    /* Render target surface YUV420 format check */
    VAConfigAttrib attribute;
    attribute.type = VAConfigAttribRTFormat;
    va_status      = vaGetConfigAttributes(va_dpy, VAProfileNone, VAEntrypointVideoProc, &attribute, 1);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaGetConfigAttributes");
    if (!(attribute.value & VA_RT_FORMAT_YUV420)) {
        info("Format %d is not supported by VPP HW!\n", VA_RT_FORMAT_YUV420);
        assert(0);
    }

    out_fourcc = is_p010 ? VA_FOURCC_P010 : VA_FOURCC_ARGB;
    // out_fourcc = is_p010 ? VA_FOURCC_P010 : VA_FOURCC_BGRA;
    /* Create surfaces for VPP HW input output */
    va_status = create_surface(&in_surface_id, 1, width, height, VA_FOURCC_Y210, VA_RT_FORMAT_YUV420);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaCreateSurfaces for input surface");

    if (is2call) {
        /* Create intermediate surface for scaling + 3DLUT */
        va_status = create_surface(&inter_surface_id, 0, width_rs, height_rs, VA_FOURCC_Y210, VA_RT_FORMAT_YUV420);
        CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaCreateSurfaces for inter surface");
    }

    va_status = create_surface(&out_surface_id, 0, width_rs, height_rs, out_fourcc,
                               is_p010 ? VA_RT_FORMAT_YUV420 : VA_RT_FORMAT_RGB32);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaCreateSurfaces for output surface");

    uint32_t lut3d_size = seg_size_3dlut * seg_size_3dlut * mul_size_3dlut * (16 / 8) * 4;
    info("3dlut file name: %s, 3dlut size: %d\n", filename_3dlut, lut3d_size);
    /* create 3dlut surface and fill it with the data in 3dlut file */
    va_status = create_surface(&surface_id_3dlut, 0, seg_size_3dlut * mul_size_3dlut, seg_size_3dlut * 2,
                               VA_FOURCC_RGBA, VA_RT_FORMAT_RGB32);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaCreateSurfaces for 3dlut surface");
    /* fill 3dlut with the 3dlut file data */
    FILE *lut3d_file = NULL;
    lut3d_file       = fopen(filename_3dlut, "rb");
    if (!lut3d_file) {
        info("%s, warning read input 3dlut file %s failed, skip 3dlut processing\n", __func__, filename_3dlut);
        is_3dlut_support = 0;
        return va_status;
    }
    upload_3dlut_data(lut3d_file, surface_id_3dlut);
    if (lut3d_file) {
        fclose(lut3d_file);
        lut3d_file = NULL;
    }

    va_status = vaCreateConfig(va_dpy, VAProfileNone, VAEntrypointVideoProc, &attribute, 1, &config_id);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaCreateConfig failed");

    va_status = vaCreateContext(va_dpy, config_id, width, height, VA_PROGRESSIVE, &out_surface_id, 1, &context_id);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaCreateContext");
    return va_status;
}

static void vpp_destroy_context() {
    if (session)
        MFXClose(session);
    if (loader)
        MFXUnload(loader);

    info("vaDestroySurfaces input, inter and output surface!\n");
    vaDestroySurfaces(va_dpy, &in_surface_id, 1);
    if (is2call)
        vaDestroySurfaces(va_dpy, &inter_surface_id, 1);
    vaDestroySurfaces(va_dpy, &out_surface_id, 1);

    info("vaDestroySurfaces 3dlut surface for Scaling!\n");
    vaDestroySurfaces(va_dpy, &surface_id_3dlut, 1);

    vaDestroyContext(va_dpy, context_id);
    vaDestroyConfig(va_dpy, config_id);

    vaTerminate(va_dpy);
}

static VAStatus create_lut3d_buffer(VABufferID &param_buf_id, VAContextID context_id) {
    VAStatus va_status;
    VAProcFilterParameterBuffer3DLUT lut_param;
    VABufferID lut3d_param_buf_id;
    uint32_t num_caps = 10;
    bool is_supported = false;

    VAProcFilterCap3DLUT caps[num_caps];
    memset(&caps, 0, sizeof(VAProcFilterCap3DLUT) * num_caps);
    va_status = vaQueryVideoProcFilterCaps(va_dpy, context_id, VAProcFilter3DLUT, (void *)caps, &num_caps);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "Error in vaQueryVideoProcFilterCaps");

    /* check if the input 3dlut parameters are supported */
    for (uint32_t index = 0; index < num_caps; index++) {
        // check lut_size and lut_stride
        if ((caps[index].lut_size = seg_size_3dlut) && (caps[index].lut_stride[0] == seg_size_3dlut) &&
            (caps[index].lut_stride[1] == seg_size_3dlut) && (caps[index].lut_stride[2] == mul_size_3dlut)) {
            is_supported = true;
        }
    }

    // info("---> vaQueryVideoProcFilterCaps is_supported=%d\n", is_supported);
    if (is_supported) {
        lut_param.type            = VAProcFilter3DLUT;
        lut_param.lut_surface     = surface_id_3dlut;
        lut_param.lut_size        = seg_size_3dlut;
        lut_param.lut_stride[0]   = seg_size_3dlut;
        lut_param.lut_stride[1]   = seg_size_3dlut;
        lut_param.lut_stride[2]   = mul_size_3dlut;
        lut_param.bit_depth       = 16;
        lut_param.num_channel     = 4;
        lut_param.channel_mapping = 1; // 3dlut_channel_mapping;

        /* create 3dlut fitler buffer */
        va_status = vaCreateBuffer(va_dpy, context_id, VAProcFilterParameterBufferType, sizeof(lut_param), 1,
                                   &lut_param, &lut3d_param_buf_id);

        param_buf_id = lut3d_param_buf_id;
    }

    return va_status;
}

static VAStatus vpp_process_one_frame_3dlut(VASurfaceID in_surf_id, VASurfaceID out_surf_id, VAContextID context_id,
                                            int width, int height, int widthout, int heightout) {
    VAStatus va_status = VA_STATUS_SUCCESS;
    VAProcPipelineParameterBuffer pipe_param;
    VARectangle surf_region, out_region;
    VABufferID pipe_param_buf_id = VA_INVALID_ID;
    VABufferID filter_buf_id     = VA_INVALID_ID;

    /* create 3DLUT buffer */
    create_lut3d_buffer(filter_buf_id, context_id);

    /* fill pipeline parameter buffer */
    surf_region.x      = 0;
    surf_region.y      = 0;
    surf_region.width  = width;
    surf_region.height = height;
    out_region.x       = 0;
    out_region.y       = 0;
    out_region.width   = widthout;
    out_region.height  = heightout;

    memset(&pipe_param, 0, sizeof(pipe_param));
    pipe_param.surface        = in_surf_id;
    pipe_param.surface_region = &surf_region;
    pipe_param.output_region  = &out_region;
    pipe_param.filter_flags   = 0;
    pipe_param.filters        = &filter_buf_id;
    pipe_param.num_filters    = 1;
    /* input is bt2020 */
    pipe_param.surface_color_standard = VAProcColorStandardBT2020;
    pipe_param.output_color_standard  = VAProcColorStandardBT709;

    va_status = vaCreateBuffer(va_dpy, context_id, VAProcPipelineParameterBufferType, sizeof(pipe_param), 1,
                               &pipe_param, &pipe_param_buf_id);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaCreateBuffer");

    va_status = vaBeginPicture(va_dpy, context_id, out_surf_id);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaBeginPicture");

    va_status = vaRenderPicture(va_dpy, context_id, &pipe_param_buf_id, 1);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaRenderPicture");

    va_status = vaEndPicture(va_dpy, context_id);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaEndPicture");

    if (pipe_param_buf_id != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, pipe_param_buf_id);
    }

    if (filter_buf_id != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, filter_buf_id);
    }

    return va_status;
}

static VAStatus vpp_process_one_frame_scaling(VASurfaceID in_surf_id, VASurfaceID out_surf_id, VAContextID context_id,
                                              int width, int height, int widthout, int heightout) {
    VAStatus va_status;
    VAProcPipelineParameterBuffer pipe_param;
    VARectangle surf_region, out_region;
    VABufferID pipe_param_buf_id = VA_INVALID_ID;

    // info("scaling from %dx%d to %dx%d\n", width, height, widthout, heightout);
    /* Fill pipeline buffer */
    surf_region.x      = 0;
    surf_region.y      = 0;
    surf_region.width  = width;
    surf_region.height = height;
    out_region.x       = 0;
    out_region.y       = 0;
    out_region.width   = widthout;
    out_region.height  = heightout;

    memset(&pipe_param, 0, sizeof(pipe_param));
    pipe_param.surface        = in_surf_id;
    pipe_param.surface_region = &surf_region;
    pipe_param.output_region  = &out_region;
    /* Default is going to SFC */
    pipe_param.num_filters = 0;

    va_status = vaCreateBuffer(va_dpy, context_id, VAProcPipelineParameterBufferType, sizeof(pipe_param), 1,
                               &pipe_param, &pipe_param_buf_id);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaCreateBuffer");

    va_status = vaBeginPicture(va_dpy, context_id, out_surf_id);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaBeginPicture");

    va_status = vaRenderPicture(va_dpy, context_id, &pipe_param_buf_id, 1);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaRenderPicture");

    va_status = vaEndPicture(va_dpy, context_id);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaEndPicture");

    if (pipe_param_buf_id != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, pipe_param_buf_id);
    }

    return va_status;
}

void *surface_to_intero_ptr(VASurfaceID surface_id) {
    VADRMPRIMESurfaceDescriptor prime_desc = {};

    // Export DMA buffer descriptor from libva
    VAStatus va_sts = vaExportSurfaceHandle(va_dpy, surface_id, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                            VA_EXPORT_SURFACE_WRITE_ONLY, &prime_desc);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_sts, "error in vaExportHandle");

    // Check memory layout. At this moment it support only linear
    // memory layout.
    // CHECK_STATUS(prime_desc.objects[0].drm_format_modifier == 0,
    //       "Error. Only linear memory layout is supported by SYCL kernel.");

    // Retrieve DMA buf descriptor to pass it to Level-zero.
    dma_buf_fd = prime_desc.objects[0].fd;

    void *usm_ptr = nullptr;
    // Import DMA buf descriptor to Level-zero and convert it to USM.
    ze_device_mem_alloc_desc_t mem_alloc_desc = {};
    mem_alloc_desc.stype                      = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    ze_external_memory_import_fd_t import_fd  = {ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD, nullptr,
                                                 ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF, dma_buf_fd};
    mem_alloc_desc.pNext                      = &import_fd;
    ze_res = zeMemAllocDevice(ze_context, &mem_alloc_desc, prime_desc.objects[0].size, 0, ze_device, &usm_ptr);

    CHECK_STATUS(ze_res == ZE_RESULT_SUCCESS, "Error: Failed to get USM buffer pointer");

    // info("---> %s \n", __func_);

    return usm_ptr;
}

int process_one_frame_sync(void *pq, unsigned char *dataf, unsigned char *buf_in, int width, int height, int size,
                           int f, int is_last) {
    double duration_cpu0 = 0.0;
    double gpu_time_ns0  = 0.0;
    double cpy_time_ns0  = 0.0;
    duration_3dlutcopy   = 0;

    std::chrono::high_resolution_clock::time_point s0, s, e;
    s0 = std::chrono::high_resolution_clock::now();
    impl_common_mem_copy(pq, cpy_evt, buf_in, dataf, size * sizeof(unsigned char), NULL, 1);
    e                     = std::chrono::high_resolution_clock::now();
    double duration_copy0 = std::chrono::duration<double, std::milli>(e - s0).count();
    duration_copy += duration_copy0;
    if (enable_profiling) {
        cpy_time_ns0 = impl_common_event_profiling(cpy_evt);
        cpy_time_ns += cpy_time_ns0;
    }
    s                      = std::chrono::high_resolution_clock::now();
    void *buf_ptr          = surface_to_intero_ptr(in_surface_id);
    unsigned char *buf_dst = (unsigned char *)buf_ptr;
    ret = impl_csc_run(&csc_params, pcsc_context, (unsigned char *)buf_in, (unsigned char *)buf_dst, NULL);
    CHECK_IMPL(ret, "impl_csc_run");
    if (enable_profiling) {
        gpu_time_ns0 = impl_common_event_profiling(csc_params.evt);
    }

    gpu_time_ns += gpu_time_ns0;
    e             = std::chrono::high_resolution_clock::now();
    duration_cpu0 = std::chrono::duration<double, std::milli>(e - s).count();

    s = std::chrono::high_resolution_clock::now();
    if (is2call) {
        vpp_process_one_frame_scaling(in_surface_id, inter_surface_id, context_id, width, height, width_rs, height_rs);
        vpp_process_one_frame_3dlut(inter_surface_id, out_surface_id, context_id, width_rs, height_rs, width_rs,
                                    height_rs);
    } else {
        vpp_process_one_frame_3dlut(in_surface_id, out_surface_id, context_id, width, height, width_rs, height_rs);
    }
    // unmap memory from L0
    ze_res = zeMemFree(ze_context, buf_ptr);
    // close DMA buf file descriptor
    close(dma_buf_fd);
    if (output_file)
        store_surface_to_file(out_surface_id, out_fp);

    e                     = std::chrono::high_resolution_clock::now();
    double duration_3dlut = std::chrono::duration<double, std::milli>(e - s).count();
    // double duration_cpu1 = std::chrono::duration<double, std::milli>(e - s0).count();
    double duration_cpu1 = duration_copy0 + duration_cpu0 + duration_3dlut;
    duration_cpu += duration_cpu1;
    duration_copy += duration_3dlutcopy;
    // info("The %dth frame time %lfms, copy %lfms csc %lfms copyin %lfms copyin+scaling+3dlut %lfms\n", f,
    // duration_cpu1, duration_copy0, duration_cpu0, duration_3dlutcopy, duration_3dlut);
    (void)f;
    (void)width;
    (void)height;
    (void)is_last;
    return 0;
}

int main(int argc, char **argv) {
    char pfilename[512] = "";
    FILE *input;
    char outputfilename[512] = "";
    // set async mode by default
    bool is_target_cpu    = 0; // target is always GPU for IMPL CSC + VPP HW coworking
    unsigned char *data   = NULL;
    unsigned char *buf_in = NULL;
    int frames            = 1;
    // source V210 size
    int width   = 1920;
    int height  = 1080;
    size_t size = width * height / 6 * 16;
    // destination Y210 size
    width_rs      = 720;
    height_rs     = 480;
    int frame_idx = 0;

    getArgsConvert(argc, argv, pfilename, outputfilename, frames, filename_3dlut, is_p010, is2call, enable_profiling,
                   pre_read);

    void *pq = impl_common_init(is_target_cpu, enable_profiling);
    cpy_evt  = impl_common_new_event();

    if (strcmp(outputfilename, "") != 0) {
        output_file = true;
    }
    // if(strcmp(filename_3dlut, "") == 0) {
    //     is_3dlut_support = 0;
    // }

    VAStatus va_status;
    va_status = vpp_create_context(pq, width, height, width_rs, height_rs);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vpp context create failed ");

    if (output_file) {
        info("Processed %d frames to file %s\n", frames, outputfilename);
        out_fp = fopen(outputfilename, "wb");
    }

    input = fopen(pfilename, "rb");
    if (input == NULL) {
        err("%s, input file does not exit", __func__);
        return -1;
    }

    impl_video_format in_format  = IMPL_VIDEO_V210;
    impl_video_format out_format = IMPL_VIDEO_Y210;
    if (pre_read)
        data = impl_image_mem_alloc(pq, in_format, width, height * N, IMPL_MEM_TYPE_HOST, NULL);
    else
        data = impl_image_mem_alloc(pq, in_format, width, height, IMPL_MEM_TYPE_HOST, NULL);
    buf_in = impl_image_mem_alloc(pq, in_format, width, height, IMPL_MEM_TYPE_DEVICE, &size);

    if (pre_read) {
        ret = fread_repeat(data, 1, size * N, input, 0);
    } else {
        ret = fread_repeat(data, 1, size, input, 0);
    }
    unsigned char *data_start = data;

    memset(&csc_params, 0, sizeof(csc_params));
    csc_params.pq         = pq;
    csc_params.is_async   = false;
    csc_params.in_format  = in_format;
    csc_params.out_format = out_format;
    csc_params.width      = width;
    csc_params.height     = height;

    ret = impl_csc_init(&csc_params, pcsc_context);
    CHECK_IMPL(ret, "impl_csc_init");

    {
        // do not count in time for the first frame for Just-in-Time compilation, start frame time is always very long,
        // ignore it for AOT
        ret = process_one_frame_sync(pq, data_start, buf_in, width, height, size, 0, frames == 1);
        CHECK_IMPL(ret, "process_one_frame");

        info("First frame time is not counted in!!!\n");
        // clear cpu, gpu and copy time for start frame
        duration_cpu  = 0.0;
        duration_copy = 0.0;
        gpu_time_ns   = 0.0;
        cpy_time_ns   = 0.0;

        if (pre_read)
            frame_idx++;
    }

    info("\nStart processing...\n");
    for (int f = 1; f < frames; f++) {
        if (pre_read) {
            if (frame_idx == N) {
                info("fseek data again %dth -> %dth frame\n", f, frame_idx);
                frame_idx = 0;
            }
            data_start = data + size * sizeof(unsigned char) * frame_idx;
        } else
            ret = fread_repeat(data_start, 1, size, input, f);

        ret = process_one_frame_sync(pq, data_start, buf_in, width, height, size, f, f == (frames - 1));
        CHECK_IMPL(ret, "process_one_frame");

        if (pre_read)
            frame_idx++;
    }

    info("\nPre frame: GPU CPU time=%lfms %lfms copy %lfms\n", gpu_time_ns / 1000000 / (frames - 1),
         duration_cpu / (frames - 1), duration_copy / (frames - 1));
    info("total %d frames fps=%lf\n", (frames - 1), 1 / (duration_cpu / (frames - 1) / 1000));

    ret = impl_csc_uninit(&csc_params, pcsc_context);
    CHECK_IMPL(ret, "impl_csc_uninit");

    impl_common_mem_free(pq, (void *)data);
    impl_common_mem_free(pq, (void *)buf_in);
    impl_common_free_event(cpy_evt);

    fclose(input);
    if (output_file) {
        info("\nClosing output file...\n");
        fclose(out_fp);
    }

    vpp_destroy_context();
    impl_common_uninit(pq);

    return 0;
}
