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
// Replay pipeline
// CSC yuv422ycbcr10be_p010 --> Resize --> HW 3DLUT nv12 --> HW HEVC Encode //420 8bits
//                              Resize --> HW 3DLUT nv12 --> HW HEVC Encode //420 8bits
// CSC yuv422ycbcr10be_p010 --> Resize --> HW 3DLUT p010 --> HW HEVC Encode //420 10bits
//                              Resize --> HW 3DLUT p010 --> HW HEVC Encode //420 10bits

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

inline void getArgsReplay(int argc, char **argv, char *pfilename, char *poutfilename, char *poutfilename1, int &frames,
                          char *p3dlutfilename, int &is_p010, int &streams, int &sync, bool &enable_profiling,
                          bool &is_target_cpu, bool &pre_read) {
    std::string infile, outfile, outfile1, p3dlut_name, device_name;
    ParseContext P;

    P.GetCommand(argc, argv);
    P.get("-frame", "n                  check n frames", &frames, (int)1, false);
    P.get("-profile", "                 enable profiling, disabled by default", &enable_profiling, (bool)false, false);
    P.get("-3dlut", "3dlut_file         3dlut input file", &p3dlut_name, (std::string) "", true);
    P.get("-is_p010", "no               0: nv12, 1: p010", &is_p010, (int)0, false);
    P.get("-streams", "num_streams      set output bitstreams number, is 1 or 2", &streams, (int)2, false);
    P.get("-o", "output_filename        set output picture filename", &outfile, (std::string) "", false);
    P.get("-o1", "output_filename1      set output picture filename", &outfile1, (std::string) "", false);
    P.get("-i", "input_filename         set input picture file", &infile, (std::string) "", true);
    P.get("-sync", "sync_mode           set sync mode, 0, async; 1, sync", &sync, (int)1, false);
    P.get("-d", "device                 set gpu(default)/cpu", &device_name, (std::string) "gpu", false);
    P.get("-pre_read", "                pre_read one frame in gpu buffer", &pre_read, (bool)false, false);
    P.check("usage:\treplay [options]\noptions:");

    is_target_cpu = getdevice(device_name);
    memcpy(pfilename, infile.c_str(), infile.length() + 1);
    memcpy(poutfilename, outfile.c_str(), outfile.length() + 1);
    memcpy(poutfilename1, outfile1.c_str(), outfile1.length() + 1);
    memcpy(p3dlutfilename, p3dlut_name.c_str(), p3dlut_name.length() + 1);
    if (streams != 1 && streams != 2) {
        info("input options -streams %d error, output bitstreams number could be only 1 or 2\n", streams);
        exit(1);
    }
    info("input yuv %s output streams number %d output yuv format %s sync %d profile %d\n", pfilename, streams,
         is_p010 ? "P010" : "NV12", sync, enable_profiling);
}

void *cpy_evt             = NULL;
double duration_3dlutcopy = 0.0;
bool output_file          = false;
bool enable_profiling     = false;
bool pre_read             = false;
// pre-read in N frames and process only N frames
int N   = 20;
int ret = 0;

static VADisplay va_dpy               = NULL;
static VASurfaceID g_3dlut_surface_id = VA_INVALID_ID;
static int is_p010                    = 0;
static int streams                    = 2; // oubput bitstreams number, could be 1 or 2(default)

mfxLoader loader           = NULL;
mfxSession session         = NULL;
int mfx_fd                 = 0;
mfxVideoParam vppParams    = {};
mfxVideoParam encParams    = {};
unsigned char *p3dlut_buff = NULL;
mfxExtVPP3DLut lut;
mfxExtVideoSignalInfo inSignalInfo;
mfxExtVideoSignalInfo outSignalInfo;
mfxU16 nSurfNumVPPIn               = 0;
mfxU16 nSurfNumVPPOut              = 0;
int numFrameVpp                    = 0;
int numFrameEnc                    = 0;
mfxBitstream bitstream             = {};
mfxFrameSurface1 *outSurface       = nullptr;
mfxFrameSurface1 *noutSurfaces[50] = {
    nullptr,
};
int numFrameVpp1                    = 0;
int numFrameEnc1                    = 0;
mfxBitstream bitstream1             = {};
mfxFrameSurface1 *outSurface1       = nullptr;
mfxFrameSurface1 *noutSurfaces1[50] = {
    nullptr,
};
mfxSession session1                    = NULL;
static VASurfaceID g_3dlut_surface_id1 = VA_INVALID_ID;
mfxVideoParam vppParams1               = {};
mfxVideoParam encParams1               = {};

#define MAJOR_API_VERSION_REQUIRED 2
#define MINOR_API_VERSION_REQUIRED 2
#define VPLVERSION(major, minor) (major << 16 | minor)
#define TARGETKBPS 4000
#define FRAMERATE 30
#define ENCODED_BITSTREAM_BUFF_SIZE 2000000

static FILE *out3dlut_fp  = NULL;
static FILE *outhevc_fp   = NULL;
static FILE *out3dlut_fp1 = NULL;
static FILE *outhevc_fp1  = NULL;

static char g_3dlut_file_name[512] = "";
static int is_3dlut_support        = 1;
double duration_cpu                = 0.0;
double duration_copy               = 0.0;
double gpu_time_ns                 = 0.0;
double cpy_time_ns                 = 0.0;
int width_rs                       = 0;
int height_rs                      = 0;
int width_rs1                      = 0;
int height_rs1                     = 0;

struct impl_resize_params resize_params;
void *prs_context;
struct impl_resize_params resize1_params;
void *prs1_context;
struct impl_csc_params csc_params;
void *pcsc_context = NULL;

void set_3dlut_surface(void *va_dpy, mfxVideoParam &vppParams, VASurfaceID *p3dlut_surface_id) {
    VAStatus va_status;
    FILE *g_3dlut_file = NULL;
    unsigned int frame_size, g_3dlut_size;
    unsigned int seg_size = 65;
    unsigned int mul_size = 128;
    // g_3dlut_size = seg_size * seg_size * mul_size * (16 / 8) * 4;
    /* create 3dlut surface and fill it with the data in 3dlut file */
    // va_status = create_surface(p3dlut_surface_id, 0, seg_size * mul_size, seg_size * 2,
    //                            VA_FOURCC_RGBA, VA_RT_FORMAT_RGB32);
    VASurfaceAttrib surface_attrib;
    surface_attrib.type          = VASurfaceAttribPixelFormat;
    surface_attrib.flags         = VA_SURFACE_ATTRIB_SETTABLE;
    surface_attrib.value.type    = VAGenericValueTypeInteger;
    surface_attrib.value.value.i = VA_FOURCC_RGBA;

    va_status = vaCreateSurfaces(va_dpy, VA_RT_FORMAT_RGB32, seg_size * mul_size, seg_size * 2, p3dlut_surface_id, 1,
                                 &surface_attrib, 1);

    g_3dlut_file = fopen(g_3dlut_file_name, "rb");
    if (!g_3dlut_file) {
        info("warning read input 3dlut file %s failed\n", g_3dlut_file_name);
        is_3dlut_support = 0;
        return;
    }
    fseek(g_3dlut_file, 0L, SEEK_END);
    g_3dlut_size = ftell(g_3dlut_file);
    // info("set_3dlut_surface input file size=%d\n", g_3dlut_size);
    rewind(g_3dlut_file);

    VAImage surface_image;
    void *surface_p = NULL;
    va_status       = vaSyncSurface(va_dpy, *p3dlut_surface_id);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaSyncSurface failed\n");

    va_status = vaDeriveImage(va_dpy, *p3dlut_surface_id, &surface_image);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaDeriveImage failed\n");

    va_status = vaMapBuffer(va_dpy, surface_image.buf, &surface_p);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vaMapBuffer failed\n");

    frame_size   = surface_image.width * surface_image.height * 4;
    g_3dlut_size = (frame_size > g_3dlut_size) ? g_3dlut_size : frame_size;
    // info("3dlut file name: %s, 3dlut size: %d\n", g_3dlut_file_name, g_3dlut_size);

    p3dlut_buff = (unsigned char *)malloc(g_3dlut_size);
    memset(p3dlut_buff, 0, g_3dlut_size);
    if (fread(p3dlut_buff, g_3dlut_size, 1, g_3dlut_file) != 0) {
        memcpy(surface_p, p3dlut_buff, g_3dlut_size);
        // info("upload data to 3dlut: width %d, height %d, 3dlut file size: %d\n",
        // surface_image.width, surface_image.height, g_3dlut_size);
    }
    vaUnmapBuffer(va_dpy, surface_image.buf);
    vaDestroyImage(va_dpy, surface_image.image_id);
    fclose(g_3dlut_file);

    lut.Header.BufferId       = MFX_EXTBUFF_VPP_3DLUT;
    lut.Header.BufferSz       = sizeof(mfxExtVPP3DLut);
    lut.ChannelMapping        = MFX_3DLUT_CHANNEL_MAPPING_RGB_RGB;
    lut.BufferType            = MFX_RESOURCE_VA_SURFACE;
    lut.VideoBuffer.DataType  = MFX_DATA_TYPE_U16;
    lut.VideoBuffer.MemLayout = MFX_3DLUT_MEMORY_LAYOUT_INTEL_65LUT;
    lut.VideoBuffer.MemId     = p3dlut_surface_id;
    vppParams.ExtParam        = new mfxExtBuffer *[3];
    vppParams.ExtParam[0]     = (mfxExtBuffer *)&lut;

    inSignalInfo.Header.BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO_IN;
    inSignalInfo.Header.BufferSz = sizeof(mfxExtVideoSignalInfo);
    // if (pInParams->SignalInfoIn.Enabled) {
    //     inSignalInfo.VideoFullRange  = pInParams->SignalInfoIn.VideoFullRange;
    //     inSignalInfo.ColourPrimaries = pInParams->SignalInfoIn.ColourPrimaries;
    // } else
    {
        inSignalInfo.VideoFullRange  = 0; // Limited range P010
        inSignalInfo.ColourPrimaries = 9; // BT.2020
    }
    vppParams.ExtParam[1]         = (mfxExtBuffer *)&inSignalInfo;
    outSignalInfo.Header.BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO_OUT;
    outSignalInfo.Header.BufferSz = sizeof(mfxExtVideoSignalInfo);
    // if (pInParams->SignalInfoOut.Enabled) {
    //     outSignalInfo.VideoFullRange  = pInParams->SignalInfoOut.VideoFullRange;
    //     outSignalInfo.ColourPrimaries = pInParams->SignalInfoOut.ColourPrimaries;
    // } else
    {
        outSignalInfo.VideoFullRange  = 0; // Limited range NV12
        outSignalInfo.ColourPrimaries = 1; // BT.709
    }
    vppParams.ExtParam[2] = (mfxExtBuffer *)&outSignalInfo;
    vppParams.NumExtParam = 3;
}

static VAStatus mfx_create_context(int width, int height, int width1, int height1) {
    VAStatus va_status = VA_STATUS_SUCCESS;
    mfxStatus sts      = MFX_ERR_NONE;
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
    CHECK_STATUS(MFX_ERR_NONE == sts, "MFXSetConfigFilterProperty failed for Impl");
    // Implementation used must provide MFX API version 2.2 or newer
    mfxVariant mfxCfgVal;
    mfxCfg[1] = MFXCreateConfig(loader);
    CHECK_STATUS(NULL != mfxCfg[1], "MFXCreateConfig failed")
    mfxCfgVal.Type     = MFX_VARIANT_TYPE_U32;
    mfxCfgVal.Data.U32 = VPLVERSION(MAJOR_API_VERSION_REQUIRED, MINOR_API_VERSION_REQUIRED);
    sts = MFXSetConfigFilterProperty(mfxCfg[1], (mfxU8 *)"mfxImplDescription.ApiVersion.Version", mfxCfgVal);
    CHECK_STATUS(MFX_ERR_NONE == sts, "MFXSetConfigFilterProperty failed for API version");

    int desiredSessionIdx = 0;

    sts = MFXCreateSession(loader, desiredSessionIdx, &session);
    CHECK_STATUS(MFX_ERR_NONE == sts, "Cannot create session -- no implementations meet desired session index");

    // Convenience function to initialize available MFX
    va_dpy = (VADisplay)InitMfxHandle(session, &mfx_fd);

    // Initialize MFX VPP parameters
    PrepareMfxFrameInfo(&vppParams.vpp.In, MFX_FOURCC_P010, width, height);
    PrepareMfxFrameInfo(&vppParams.vpp.Out, (is_p010 ? MFX_FOURCC_P010 : MFX_FOURCC_NV12), width, height);

    // vppParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    vppParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;

    /* Create and set 3dlut surfaces for VPP */
    if (is_3dlut_support) {
        set_3dlut_surface(va_dpy, vppParams, &g_3dlut_surface_id);
    }

    // Initialize VPP
    sts = MFXVideoVPP_Init(session, &vppParams);
    CHECK_STATUS(MFX_ERR_NONE == sts, "Could not initialize MFX VPP");

    // Query number of required surfaces for VPP
    mfxFrameAllocRequest VPPRequest[2] = {};
    sts                                = MFXVideoVPP_QueryIOSurf(session, &vppParams, VPPRequest);
    CHECK_STATUS(MFX_ERR_NONE == sts, "Error in MFXVideoVPP_QueryIOSurf");
    nSurfNumVPPIn  = VPPRequest[0].NumFrameSuggested; // vpp in
    nSurfNumVPPOut = VPPRequest[1].NumFrameSuggested; // vpp out
    // info("---> VPP num surfaces In/Out %d %d\n", nSurfNumVPPIn, nSurfNumVPPOut);

    if (streams == 2) {
        sts = MFXCreateSession(loader, desiredSessionIdx, &session1);
        CHECK_STATUS(MFX_ERR_NONE == sts, "Cannot create session1 -- no implementations meet desired session index");

        memcpy(&vppParams1, &vppParams, sizeof(vppParams));
        PrepareMfxFrameInfo(&vppParams1.vpp.In, MFX_FOURCC_P010, width1, height1);
        PrepareMfxFrameInfo(&vppParams1.vpp.Out, (is_p010 ? MFX_FOURCC_P010 : MFX_FOURCC_NV12), width1, height1);
        sts = MFXVideoCORE_SetHandle(session1, static_cast<mfxHandleType>(MFX_HANDLE_VA_DISPLAY), va_dpy);
        CHECK_STATUS(MFX_ERR_NONE == sts, "Error in MFXVideoCORE_SetHandle session1");
        if (is_3dlut_support) {
            set_3dlut_surface(va_dpy, vppParams1, &g_3dlut_surface_id1);
        }
        sts = MFXVideoVPP_Init(session1, &vppParams1);
        CHECK_STATUS(MFX_ERR_NONE == sts, "Could not initialize MFX VPP1");
    }

    // Initialize MFX encoder parameters
    encParams.mfx.CodecId                 = MFX_CODEC_HEVC;
    encParams.mfx.TargetUsage             = MFX_TARGETUSAGE_BALANCED; // MFX_TARGETUSAGE_BEST_SPEED;
    encParams.mfx.TargetKbps              = TARGETKBPS;
    encParams.mfx.RateControlMethod       = MFX_RATECONTROL_VBR;
    encParams.mfx.FrameInfo.FrameRateExtN = FRAMERATE;
    encParams.mfx.FrameInfo.FrameRateExtD = 1;
    if (is_p010) {
        encParams.mfx.FrameInfo.FourCC         = MFX_FOURCC_P010;
        encParams.mfx.FrameInfo.Shift          = 1;
        encParams.mfx.FrameInfo.BitDepthLuma   = 10;
        encParams.mfx.FrameInfo.BitDepthChroma = 10;
    } else {
        encParams.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    }
    encParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    encParams.mfx.FrameInfo.CropX        = 0;
    encParams.mfx.FrameInfo.CropY        = 0;
    encParams.mfx.FrameInfo.CropW        = width;
    encParams.mfx.FrameInfo.CropH        = height;
    encParams.mfx.FrameInfo.Width        = ALIGN16(width);
    encParams.mfx.FrameInfo.Height       = ALIGN16(height);

    encParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

    // fill in missing params
    sts = MFXVideoENCODE_Query(session, &encParams, &encParams);
    CHECK_STATUS(MFX_ERR_NONE == sts, "Encode query failed");

    // Initialize MFX video encoder
    sts = MFXVideoENCODE_Init(session, &encParams);
    CHECK_STATUS(MFX_ERR_NONE == sts, "Encode init failed");

    // Query number required surfaces for decoder
    mfxFrameAllocRequest encRequest = {};
    sts                             = MFXVideoENCODE_QueryIOSurf(session, &encParams, &encRequest);
    CHECK_STATUS(MFX_ERR_NONE == sts, "QueryIOSurf failed");
    // info("---> ENCODE num surfaces In %d\n", encRequest.NumFrameSuggested);
    nSurfNumVPPOut = (encRequest.NumFrameSuggested > nSurfNumVPPOut) ? encRequest.NumFrameSuggested : nSurfNumVPPOut;
    // nSurfNumVPPOut = 30; // hack to enlarge surfaces pool

    // Prepare MFX hevc output bitstream
    bitstream.MaxLength = ENCODED_BITSTREAM_BUFF_SIZE;
    bitstream.Data      = (mfxU8 *)malloc(bitstream.MaxLength * sizeof(mfxU8));

    if (streams == 2) {
        memcpy(&encParams1, &encParams, sizeof(encParams));
        encParams1.mfx.FrameInfo.CropW  = width1;
        encParams1.mfx.FrameInfo.CropH  = height1;
        encParams1.mfx.FrameInfo.Width  = ALIGN16(width1);
        encParams1.mfx.FrameInfo.Height = ALIGN16(height1);
        sts                             = MFXVideoENCODE_Init(session1, &encParams1);
        CHECK_STATUS(MFX_ERR_NONE == sts, "Encode init failed");
        // sts = MFXVideoENCODE_QueryIOSurf(session1, &encParams1, &encRequest);
        // CHECK_STATUS(MFX_ERR_NONE == sts, "QueryIOSurf failed");
        // info("---> ENCODE1 num surfaces In %d\n", encRequest.NumFrameSuggested);

        bitstream1.MaxLength = ENCODED_BITSTREAM_BUFF_SIZE;
        bitstream1.Data      = (mfxU8 *)malloc(bitstream1.MaxLength * sizeof(mfxU8));
    }

    return va_status;
}

static void mfx_destroy_context() {
    if (streams == 2) {
        if (session1) {
            MFXVideoVPP_Close(session1);
            MFXVideoENCODE_Close(session1);
            MFXClose(session1);
        }
        if (bitstream1.Data) {
            free(bitstream1.Data);
        }

        vaDestroySurfaces(va_dpy, &g_3dlut_surface_id1, 1);
    }
    if (session) {
        MFXVideoVPP_Close(session);
        MFXVideoENCODE_Close(session);
        MFXClose(session);
    }
    if (bitstream.Data) {
        free(bitstream.Data);
    }

    vaDestroySurfaces(va_dpy, &g_3dlut_surface_id, 1);

    FreeMfxHandle(va_dpy, mfx_fd);
    mfx_fd = 0;

    if (loader)
        MFXUnload(loader);

    delete p3dlut_buff;
    delete[] vppParams.ExtParam;
}

void ReadRawFrameP010(queue *pq, unsigned char *buf_rs, mfxFrameSurface1 *surface) {
    mfxFrameData *data    = &surface->Data;
    mfxFrameInfo *info    = &surface->Info;
    int width             = info->CropW;
    int height            = info->CropH;
    int frame_size        = width * height * 3;
    unsigned char *y_src  = (unsigned char *)malloc(frame_size);
    unsigned char *uv_src = y_src + width * height * 2;
    unsigned char *y_dst  = (unsigned char *)((unsigned char *)data->Y);
    unsigned char *uv_dst = (unsigned char *)((unsigned char *)data->UV);

    pq->memcpy(y_src, buf_rs, frame_size).wait();

    for (int y = 0; y < height; y++) {
        memcpy(y_dst, y_src + y * width * 2, width * 2);
        y_dst += data->Pitch;
    }
    for (int y = 0; y < (height / 2); y++) {
        memcpy(uv_dst, uv_src + y * width * 2, width * 2);
        uv_dst += data->Pitch;
    }

    free(y_src);
}

void WriteRawFrameNV12P010(mfxFrameSurface1 *surface, FILE *out_fp, int is_p010) {
    mfxFrameData *data    = &surface->Data;
    mfxFrameInfo *info    = &surface->Info;
    int width             = info->CropW;
    int height            = info->CropH;
    int frame_size        = width * height * 3 / (is_p010 ? 1 : 2);
    unsigned char *y_dst  = (unsigned char *)malloc(frame_size);
    unsigned char *uv_dst = y_dst + width * height * (is_p010 ? 2 : 1);
    unsigned char *y_src  = (unsigned char *)((unsigned char *)data->Y);
    unsigned char *uv_src = (unsigned char *)((unsigned char *)data->UV);

    int pitch = width * (is_p010 ? 2 : 1);
    for (int y = 0; y < height; y++) {
        memcpy(y_dst + y * pitch, y_src, pitch);
        y_src += data->Pitch;
    }
    for (int y = 0; y < (height / 2); y++) {
        memcpy(uv_dst + y * pitch, uv_src, pitch);
        uv_src += data->Pitch;
    }
    fwrite(y_dst, 1, frame_size, out_fp);

    free(y_dst);
}

static VAStatus mfx_video_frame_process_3dlut(mfxSession *psession, queue *pq, unsigned char *buf_rs, int is_last,
                                              int numFrame, mfxFrameSurface1 *outSurfaces[50]) {
    VAStatus va_status   = VA_STATUS_SUCCESS;
    mfxStatus sts        = MFX_ERR_NONE;
    bool isDrainingVpp   = false;
    bool isStillGoingVpp = true;
    mfxSyncPoint syncp;

    while (isStillGoingVpp == true) {
        mfxFrameSurface1 *inSurface = nullptr;
        // Load a new frame if not draining
        if (isDrainingVpp == false) {
            sts = MFXMemory_GetSurfaceForVPPIn(*psession, &inSurface);
            CHECK_STATUS(MFX_ERR_NONE == sts, "Error in GetSurfaceForVPPIn");

            std::chrono::high_resolution_clock::time_point s, e;
            s = std::chrono::high_resolution_clock::now();
            // Map surface to the system memory
            sts = inSurface->FrameInterface->Map(inSurface, MFX_MAP_WRITE);
            CHECK_STATUS(MFX_ERR_NONE == sts, "Error in Map for write");
            // Write input pixels to the surface
            ReadRawFrameP010(pq, buf_rs, inSurface);
            if (sts == MFX_ERR_MORE_DATA)
                isDrainingVpp = true;
            else
                CHECK_STATUS(MFX_ERR_NONE == sts, "Unknown error reading input");
            // Unmap surface to the system memory
            sts = inSurface->FrameInterface->Unmap(inSurface);
            CHECK_STATUS(MFX_ERR_NONE == sts, "Error in Unmap");
            e = std::chrono::high_resolution_clock::now();
            duration_3dlutcopy += std::chrono::duration<double, std::milli>(e - s).count();

            // sts = MFXMemory_GetSurfaceForVPPOut(*psession, &outSurface);
            sts = MFXMemory_GetSurfaceForEncode(*psession, &outSurface);
            CHECK_STATUS(MFX_ERR_NONE == sts, "Error in MFXMemory_GetSurfaceForVPPOut");
            outSurfaces[numFrame % nSurfNumVPPOut] = outSurface;
        }

        sts = MFXVideoVPP_RunFrameVPPAsync(*psession, (isDrainingVpp == true) ? NULL : inSurface, outSurface, NULL,
                                           &syncp);

        switch (sts) {
        case MFX_ERR_NONE:
            // sts = MFXVideoCORE_SyncOperation(*psession, syncp, WAIT_100_MILLISECONDS * 1000);
            // CHECK_STATUS(MFX_ERR_NONE == sts, "Error in Encoder SyncOperation");
            isDrainingVpp   = is_last;
            isStillGoingVpp = false;
            // info("VPP processed a frame number: %d\n", numFrame);
            break;
        case MFX_ERR_MORE_DATA:
            // VPP needs more input frames before produce an output
            if (isDrainingVpp)
                isStillGoingVpp = false;
            info("Error Vpp needs more data\n");
            break;
        case MFX_ERR_MORE_SURFACE:
            // VPP needs more output surfaces for additional output frames available.
            break;
        case MFX_WRN_DEVICE_BUSY:
            // For non-CPU implementations.
            // Busy and wait a few milliseconds then try again
            break;
        case MFX_ERR_DEVICE_LOST:
            // For non-CPU implementations.
            // Try again if device is lost
            break;
        default:
            info("VPP gets unknown status %d\n", sts);
            isStillGoingVpp = false;
            break;
        }
        if (isDrainingVpp == false && inSurface) {
            sts = inSurface->FrameInterface->Release(inSurface);
            CHECK_STATUS(MFX_ERR_NONE == sts, "Error in FrameInterface->Release");
        }
        if (!is_last)
            isStillGoingVpp = false;
    }

    return va_status;
}

// Write encoded stream to file
void WriteEncodedStream(mfxBitstream *pbs, FILE *f) {
    fwrite(pbs->Data + pbs->DataOffset, 1, pbs->DataLength, f);
    // pbs->DataLength = 0;
    return;
}

static VAStatus mfx_video_frame_process_encoder(mfxSession *psession, mfxBitstream *pbitstream, int is_last,
                                                mfxFrameSurface1 *outSurfaces[50], int numFrame, int *pnumFrameEnc,
                                                FILE *outyuv_fp, FILE *outbs_fp) {
    VAStatus va_status   = VA_STATUS_SUCCESS;
    mfxStatus sts        = MFX_ERR_NONE;
    bool isDrainingEnc   = false;
    bool isStillGoingEnc = true;
    mfxSyncPoint syncp;

    while (isStillGoingEnc == true) {
        outSurface = outSurfaces[numFrame % nSurfNumVPPOut];
        // outSurface = outSurfaces[(*pnumFrameEnc) % nSurfNumVPPOut];
        sts = MFXVideoENCODE_EncodeFrameAsync(*psession, NULL, (isDrainingEnc == true) ? NULL : outSurface, pbitstream,
                                              &syncp);
        switch (sts) {
        case MFX_ERR_NONE:
            sts = MFXVideoCORE_SyncOperation(*psession, syncp, WAIT_100_MILLISECONDS * 1000);
            CHECK_STATUS(MFX_ERR_NONE == sts, "Error in Encoder SyncOperation");
            outSurface = outSurfaces[(*pnumFrameEnc) % nSurfNumVPPOut];
            if (output_file) {
#if 1
                sts = outSurface->FrameInterface->Map(outSurface, MFX_MAP_READ);
                CHECK_STATUS(MFX_ERR_NONE == sts, "Encoder Error in Map for output write");

                WriteRawFrameNV12P010(outSurface, outyuv_fp, is_p010);

                sts = outSurface->FrameInterface->Unmap(outSurface);
                CHECK_STATUS(MFX_ERR_NONE == sts, "Encoder Error in Unmap for output");
#endif
                WriteEncodedStream(pbitstream, outbs_fp);
            }
            pbitstream->DataLength = 0;
            if (isDrainingEnc == false && outSurface) {
                sts = outSurface->FrameInterface->Release(outSurface);
                CHECK_STATUS(MFX_ERR_NONE == sts, "Encoder mfxFrameSurfaceInterface->Release failed");
            }
            isDrainingEnc = is_last;
            // info("Encoded a frame number: %d\n", *pnumFrameEnc);
            (*pnumFrameEnc)++;
            break;
        case MFX_ERR_NOT_ENOUGH_BUFFER:
            // The output bitstream buffer is pre-set big enough. Handle when frame size exceeds available buffer here
            info("Error encoder no enough buffer\n");
            break;
        case MFX_ERR_MORE_DATA:
            // Encoder needs more input data before produce an output
            if (isDrainingEnc)
                isStillGoingEnc = false;
            info("Encoder needs more data\n");
            break;
        case MFX_WRN_DEVICE_BUSY:
            // For non-CPU implementations.
            // Busy and wait a few milliseconds then try again
            info("Error encoder device busy\n");
            break;
        case MFX_ERR_DEVICE_LOST:
            // For non-CPU implementations.
            // Try again if device is lost
            info("Error encoder device lost\n");
            break;
        default:
            info("Encoder gets unknown status %d\n", sts);
            isStillGoingEnc = false;
            break;
        }
        if (!is_last && isDrainingEnc == false)
            isStillGoingEnc = false;
    }

    return va_status;
}

int process_one_frame_async(void *pq, unsigned char *dataf, unsigned char *buf_in, unsigned char *buf_rs,
                            unsigned char *buf_rs1, unsigned char *buf_dst, int size, int f, int is_last) {
    duration_3dlutcopy = 0;
    std::chrono::high_resolution_clock::time_point s, e;
    s = std::chrono::high_resolution_clock::now();

    impl_common_mem_copy(pq, cpy_evt, buf_in, dataf, size * sizeof(unsigned char), NULL, 0);

    ret = impl_csc_run(&csc_params, pcsc_context, (unsigned char *)buf_in, (unsigned char *)buf_dst, cpy_evt);
    CHECK_IMPL(ret, "impl_csc_run");

    impl_resize_run(&resize_params, prs_context, buf_dst, buf_rs, csc_params.evt);
    CHECK_IMPL(ret, "impl_resize_run");
    if (streams == 2) {
        impl_resize_run(&resize1_params, prs1_context, buf_dst, buf_rs1, csc_params.evt);
        CHECK_IMPL(ret, "impl_resize_run1");
        impl_common_event_sync(resize1_params.evt);
    }
    impl_common_event_sync(resize_params.evt);

    e                    = std::chrono::high_resolution_clock::now();
    double duration_cpu0 = std::chrono::duration<double, std::milli>(e - s).count();

    s = std::chrono::high_resolution_clock::now();
    mfx_video_frame_process_3dlut(&session, (queue *)pq, buf_rs, is_last, numFrameVpp, noutSurfaces);
    mfx_video_frame_process_encoder(&session, &bitstream, is_last, noutSurfaces, numFrameVpp, &numFrameEnc, out3dlut_fp,
                                    outhevc_fp);
    numFrameVpp++;
    e                          = std::chrono::high_resolution_clock::now();
    double duration_3dlut_enc0 = std::chrono::duration<double, std::milli>(e - s).count();
    if (streams == 2) {
        s = std::chrono::high_resolution_clock::now();
        mfx_video_frame_process_3dlut(&session1, (queue *)pq, buf_rs1, is_last, numFrameVpp1, noutSurfaces1);
        mfx_video_frame_process_encoder(&session1, &bitstream1, is_last, noutSurfaces1, numFrameVpp1, &numFrameEnc1,
                                        out3dlut_fp1, outhevc_fp1);
        numFrameVpp1++;
        e                          = std::chrono::high_resolution_clock::now();
        double duration_3dlut_enc1 = std::chrono::duration<double, std::milli>(e - s).count();
        duration_3dlut_enc0 += duration_3dlut_enc1;
    }
    double duration_cpu1 = (duration_cpu0 + duration_3dlut_enc0);
    duration_cpu += duration_cpu1;
    // info("Kernel CPU time %dth frame %lfms, copy+csc+resize %lfms, 3dlutcopy+3dlut+encoder %lfms 3dlutcopy %lf\n", f,
    //      duration_cpu1, duration_cpu0, duration_3dlut_enc0, duration_3dlutcopy);
    (void)f;
    (void)is_last;
    (void)buf_rs1;
    return 0;
}

int process_one_frame_sync(void *pq, unsigned char *dataf, unsigned char *buf_in, unsigned char *buf_rs,
                           unsigned char *buf_rs1, unsigned char *buf_dst, int size, int f, int is_last) {
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
    s   = std::chrono::high_resolution_clock::now();
    ret = impl_csc_run(&csc_params, pcsc_context, (unsigned char *)buf_in, (unsigned char *)buf_dst, NULL);
    CHECK_IMPL(ret, "impl_csc_run");
    if (enable_profiling) {
        gpu_time_ns0 = impl_common_event_profiling(csc_params.evt);
    }
    impl_resize_run(&resize_params, prs_context, buf_dst, buf_rs, NULL);
    CHECK_IMPL(ret, "impl_resize_run");
    if (enable_profiling) {
        gpu_time_ns0 += impl_common_event_profiling(resize_params.evt);
    }
    if (streams == 2) {
        impl_resize_run(&resize1_params, prs1_context, buf_dst, buf_rs1, NULL);
        CHECK_IMPL(ret, "impl_resize_run1");
        if (enable_profiling) {
            gpu_time_ns0 += impl_common_event_profiling(resize1_params.evt);
        }
    }
    gpu_time_ns += gpu_time_ns0;
    e             = std::chrono::high_resolution_clock::now();
    duration_cpu0 = std::chrono::duration<double, std::milli>(e - s).count();

    s = std::chrono::high_resolution_clock::now();
    mfx_video_frame_process_3dlut(&session, (queue *)pq, buf_rs, is_last, numFrameVpp, noutSurfaces);
    mfx_video_frame_process_encoder(&session, &bitstream, is_last, noutSurfaces, numFrameVpp, &numFrameEnc, out3dlut_fp,
                                    outhevc_fp);
    numFrameVpp++;
    e                          = std::chrono::high_resolution_clock::now();
    double duration_3dlut_enc0 = std::chrono::duration<double, std::milli>(e - s).count();
    if (streams == 2) {
        s = std::chrono::high_resolution_clock::now();
        mfx_video_frame_process_3dlut(&session1, (queue *)pq, buf_rs1, is_last, numFrameVpp1, noutSurfaces1);
        mfx_video_frame_process_encoder(&session1, &bitstream1, is_last, noutSurfaces1, numFrameVpp1, &numFrameEnc1,
                                        out3dlut_fp1, outhevc_fp1);
        numFrameVpp1++;
        e = std::chrono::high_resolution_clock::now();
        duration_3dlut_enc0 += std::chrono::duration<double, std::milli>(e - s).count();
    }
    // double duration_cpu1 = std::chrono::duration<double, std::milli>(e - s0).count();
    double duration_cpu1 = duration_copy0 + duration_cpu0 + duration_3dlut_enc0;
    duration_cpu += duration_cpu1;
    // info("Kernel GPU CPU time %dth frame %lfms %lfms, copy %lf/%lfms csc+resize %lfms copyin+3dlut+enc %lfms copyin"
    //      "%lfms\n", f, gpu_time_ns0 / 1000000, duration_cpu1, cpy_time_ns0 / 1000000, duration_copy0, duration_cpu0,
    //      duration_3dlut_enc0, duration_3dlutcopy);
    (void)f;
    (void)is_last;
    (void)buf_rs1;
    return 0;
}

int main(int argc, char **argv) {
    char pfilename[512] = "";
    FILE *input;
    FILE *output;
    char outhevcfilename[512]   = "";
    char outhevc1filename[512]  = "";
    char out3dlutfilename[512]  = "./out3dlut.yuv";
    char outRSfilename[512]     = "./tmp.yuv";
    char out3dlut1filename[512] = "./out3dlut1.yuv";
    char outRS1filename[512]    = "./tmp1.yuv";
    FILE *output1;
    unsigned char *buf_cpu_rs1 = NULL;
    // set async mode by default
    int sync_mode              = 1;
    bool is_target_cpu         = 0;
    unsigned char *data        = NULL;
    unsigned char *buf_in      = NULL;
    unsigned char *buf_dst     = NULL;
    unsigned char *buf_rs0     = NULL;
    unsigned char *buf_rs1     = NULL;
    unsigned char *buf_cpu_rs0 = NULL;
    int width                  = 3840;
    int height                 = 2160;
    int frames                 = 1;
    // source yuv422ycbcr10be size
    size_t size = width * height * 5 / 2;
    // destination P010 or NV12 size
    size_t dst_size = width * height * 3 / 2;
    width_rs        = 960;
    height_rs       = 544; // 540
    size_t rs_size  = width_rs * height_rs * 3 / 2;
    width_rs1       = 480;
    height_rs1      = 270;
    size_t rs_size1 = width_rs1 * height_rs1 * 3 / 2;
    int frame_idx   = 0;

    getArgsReplay(argc, argv, pfilename, outhevcfilename, outhevc1filename, frames, g_3dlut_file_name, is_p010, streams,
                  sync_mode, enable_profiling, is_target_cpu, pre_read);

    void *pq = impl_common_init(is_target_cpu, enable_profiling);
    cpy_evt  = impl_common_new_event();

    if ((streams == 1 && strcmp(outhevcfilename, "") != 0) ||
        (streams == 2 && strcmp(outhevcfilename, "") != 0 && strcmp(outhevc1filename, "") != 0)) {
        output_file = true;
    }
    // if(strcmp(g_3dlut_file_name, "") == 0) {
    //     is_3dlut_support = 0;
    // }

    VAStatus va_status;
    va_status = mfx_create_context(width_rs, height_rs, width_rs1, height_rs1);
    CHECK_STATUS(VA_STATUS_SUCCESS == va_status, "vpp context create failed ");

    if (output_file) {
        info("Encoding %d frames to file %s\n", frames, outhevcfilename);
        outhevc_fp  = fopen(outhevcfilename, "wb");
        out3dlut_fp = fopen(out3dlutfilename, "wb");
        if (streams == 2) {
            info("Encoding %d frames to file1 %s\n", frames, outhevc1filename);
            outhevc_fp1  = fopen(outhevc1filename, "wb");
            out3dlut_fp1 = fopen(out3dlut1filename, "wb");
        }
    }

    input = fopen(pfilename, "rb");
    if (input == NULL) {
        err("%s, input file does not exit", __func__);
        return -1;
    }
    if (output_file) {
        info("Dumping IMPL scaling output %d frames to file %s\n", frames, outRSfilename);
        output = fopen(outRSfilename, "wb");
        if (streams == 2) {
            info("Dumping IMPL scaling output %d frames to file1 %s\n", frames, outRS1filename);
            output1 = fopen(outRS1filename, "wb");
        }
    }

    if (pre_read)
        data = impl_image_mem_alloc(pq, IMPL_VIDEO_YUV422YCBCR10BE, width, height * N, IMPL_MEM_TYPE_HOST, NULL);
    else
        data = impl_image_mem_alloc(pq, IMPL_VIDEO_YUV422YCBCR10BE, width, height, IMPL_MEM_TYPE_HOST, NULL);
    // yuv422ycbcr10be
    buf_in = impl_image_mem_alloc(pq, IMPL_VIDEO_YUV422YCBCR10BE, width, height, IMPL_MEM_TYPE_DEVICE, &size);
    // P010
    buf_dst     = impl_image_mem_alloc(pq, IMPL_VIDEO_P010, width, height, IMPL_MEM_TYPE_DEVICE, &dst_size);
    buf_rs0     = impl_image_mem_alloc(pq, IMPL_VIDEO_P010, width_rs, height_rs, IMPL_MEM_TYPE_DEVICE, &rs_size);
    buf_cpu_rs0 = impl_image_mem_alloc(pq, IMPL_VIDEO_P010, width_rs, height_rs, IMPL_MEM_TYPE_HOST, NULL);
    if (streams == 2) {
        buf_rs1     = impl_image_mem_alloc(pq, IMPL_VIDEO_P010, width_rs1, height_rs1, IMPL_MEM_TYPE_DEVICE, &rs_size1);
        buf_cpu_rs1 = impl_image_mem_alloc(pq, IMPL_VIDEO_P010, width_rs1, height_rs1, IMPL_MEM_TYPE_HOST, NULL);
    }

    if (pre_read) {
        ret = fread_repeat(data, 1, size * N, input, 0);
    } else {
        ret = fread_repeat(data, 1, size, input, 0);
    }
    unsigned char *data_start = data;

    memset(&resize_params, 0, sizeof(resize_params));
    resize_params.pq         = pq;
    resize_params.format     = IMPL_VIDEO_P010;
    resize_params.src_width  = width;
    resize_params.src_height = height;
    resize_params.dst_width  = width_rs;
    resize_params.dst_height = height_rs;
    resize_params.is_async   = (sync_mode == 0);
    ret                      = impl_resize_init(&resize_params, prs_context);
    CHECK_IMPL(ret, "impl_resize_init");
    if (streams == 2) {
        memset(&resize1_params, 0, sizeof(resize1_params));
        resize1_params.pq         = pq;
        resize1_params.format     = IMPL_VIDEO_P010;
        resize1_params.src_width  = width;
        resize1_params.src_height = height;
        resize1_params.dst_width  = width_rs1;
        resize1_params.dst_height = height_rs1;
        resize1_params.is_async   = (sync_mode == 0);
        ret                       = impl_resize_init(&resize1_params, prs1_context);
        CHECK_IMPL(ret, "impl_resize_init1");
    }
    memset(&csc_params, 0, sizeof(csc_params));
    csc_params.pq         = pq;
    csc_params.is_async   = (sync_mode == 0);
    csc_params.in_format  = IMPL_VIDEO_YUV422YCBCR10BE;
    csc_params.out_format = IMPL_VIDEO_P010;
    csc_params.width      = width;
    csc_params.height     = height;

    ret = impl_csc_init(&csc_params, pcsc_context);
    CHECK_IMPL(ret, "impl_csc_init");

    {
        // do not count in time for the first frame for Just-in-Time compilation, start frame time is always very long,
        // ignore it for AOT
        if (sync_mode == false)
            ret = process_one_frame_async(pq, data_start, buf_in, buf_rs0, buf_rs1, buf_dst, size, 0, frames == 1);
        else
            ret = process_one_frame_sync(pq, data_start, buf_in, buf_rs0, buf_rs1, buf_dst, size, 0, frames == 1);
        CHECK_IMPL(ret, "process_one_frame");

        if (output_file) {
            impl_common_mem_copy(pq, NULL, buf_cpu_rs0, buf_rs0, rs_size, NULL, 1);
            fwrite(buf_cpu_rs0, 1, rs_size, output);
            if (streams == 2) {
                impl_common_mem_copy(pq, NULL, buf_cpu_rs1, buf_rs1, rs_size1, NULL, 1);
                fwrite(buf_cpu_rs1, 1, rs_size1, output1);
            }
        }
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

        if (sync_mode == false)
            ret =
                process_one_frame_async(pq, data_start, buf_in, buf_rs0, buf_rs1, buf_dst, size, f, f == (frames - 1));
        else
            ret = process_one_frame_sync(pq, data_start, buf_in, buf_rs0, buf_rs1, buf_dst, size, f, f == (frames - 1));
        CHECK_IMPL(ret, "process_one_frame");

        if (output_file) {
            impl_common_mem_copy(pq, NULL, buf_cpu_rs0, buf_rs0, rs_size, NULL, 1);
            fwrite(buf_cpu_rs0, 1, rs_size, output);
            if (streams == 2) {
                impl_common_mem_copy(pq, NULL, buf_cpu_rs1, buf_rs1, rs_size1, NULL, 1);
                fwrite(buf_cpu_rs1, 1, rs_size1, output1);
            }
        }
        if (pre_read)
            frame_idx++;
    }

    if (sync_mode == false) {
        info("\nPre frame: CPU time per frame=%lfms\n", duration_cpu / (frames - 1));
    } else {
        info("\nPre frame: GPU CPU time=%lfms %lfms copy %lfms\n", gpu_time_ns / 1000000 / (frames - 1),
             duration_cpu / (frames - 1), duration_copy / (frames - 1));
    }
    info("total %d frames fps=%lf\n", (frames - 1), 1 / (duration_cpu / (frames - 1) / 1000));

    ret = impl_csc_uninit(&csc_params, pcsc_context);
    CHECK_IMPL(ret, "impl_csc_uninit");
    ret = impl_resize_uninit(&resize_params, prs_context);
    CHECK_IMPL(ret, "impl_resize_uninit");
    if (streams == 2) {
        ret = impl_resize_uninit(&resize1_params, prs1_context);
        CHECK_IMPL(ret, "impl_resize_uninit1");
    }

    impl_common_mem_free(pq, (void *)data);
    impl_common_mem_free(pq, (void *)buf_cpu_rs0);
    impl_common_mem_free(pq, (void *)buf_in);
    impl_common_mem_free(pq, (void *)buf_rs0);
    impl_common_mem_free(pq, (void *)buf_dst);
    if (streams == 2) {
        impl_common_mem_free(pq, (void *)buf_rs1);
        impl_common_mem_free(pq, (void *)buf_cpu_rs1);
    }
    impl_common_free_event(cpy_evt);

    fclose(input);
    if (output_file) {
        info("\nClosing output file...\n");
        fclose(out3dlut_fp);
        fclose(outhevc_fp);
        fclose(output);
        if (streams == 2) {
            info("\nClosing output file1...\n");
            fclose(out3dlut_fp1);
            fclose(outhevc_fp1);
            fclose(output1);
        }
    }

    mfx_destroy_context();
    impl_common_uninit(pq);

    return 0;
}
