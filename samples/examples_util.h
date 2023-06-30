/* SPDX-License-Identifier: BSD-3-Clause
* Copyright(c) 2023 Intel Corporation
*/

#ifndef __EXAMPLES_UTIL_H__
#define __EXAMPLES_UTIL_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_MEDIASDK1
    #include "mfxvideo.h"
enum {
    MFX_FOURCC_I420 = MFX_FOURCC_IYUV /*!< Alias for the IYUV color format. */
};
#else
    #include "vpl/mfxjpeg.h"
    #include "vpl/mfxvideo.h"
#endif

#if (MFX_VERSION >= 2000)
    #include "vpl/mfxdispatcher.h"
#endif

#ifdef __linux__
    #include <fcntl.h>
    #include <unistd.h>
#endif

#ifdef LIBVA_SUPPORT
    #include "va/va.h"
    #include "va/va_drm.h"
#endif

#define WAIT_100_MILLISECONDS 100
#define ALIGN16(value)           (((value + 15) >> 4) << 4)
#define ALIGN32(X)               (((mfxU32)((X) + 31)) & (~(mfxU32)31))

#define CHECK_STATUS(x, y)   \
    if (!(x)) {              \
        printf("%s:%s (%d) failed, exit\n", __func__, y, __LINE__); \
        exit(1);             \
    }

void *InitMfxHandle(mfxSession session, int *mfxFd) {
    mfxIMPL mfx_impl;
    mfxStatus sts = MFXQueryIMPL(session, &mfx_impl);
    if (sts != MFX_ERR_NONE)
        return NULL;

    VADisplay va_dpy = NULL;
#ifdef LIBVA_SUPPORT
    int major_ver = 0, minor_ver = 0;
    if ((mfx_impl & MFX_IMPL_VIA_VAAPI) == MFX_IMPL_VIA_VAAPI) {
        if (!mfxFd)
            return NULL;
        // initialize VAAPI context and set session handle
        *mfxFd = open("/dev/dri/renderD128", O_RDWR);
        if (*mfxFd >= 0) {
            va_dpy = vaGetDisplayDRM(*mfxFd);
            if (va_dpy) {
                if (VA_STATUS_SUCCESS == vaInitialize(va_dpy, &major_ver, &minor_ver)) {
                    MFXVideoCORE_SetHandle(session, static_cast<mfxHandleType>(MFX_HANDLE_VA_DISPLAY), va_dpy);
                }
            }
        }
        return va_dpy;
    }
#endif

    return va_dpy;
}

void FreeMfxHandle(void *mfxHandle, int mfxFd) {
#ifdef LIBVA_SUPPORT
    if (mfxHandle) {
        vaTerminate((VADisplay)mfxHandle);
    }
    if (mfxFd) {
        close(mfxFd);
    }
#endif
}

void PrepareMfxFrameInfo(mfxFrameInfo *info, mfxU32 format, mfxU16 w, mfxU16 h) {
    if (format == MFX_FOURCC_P010) {
        info->Shift          = 1;
        info->BitDepthLuma   = 10;
        info->BitDepthChroma = 10;
    }
    info->FourCC        = format;
    info->ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
    info->CropX         = 0;
    info->CropY         = 0;
    info->CropW         = w;
    info->CropH         = h;
    info->PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
    info->FrameRateExtN = 30;
    info->FrameRateExtD = 1;
    // width must be a multiple of 16
    info->Width = ALIGN16(info->CropW);
    // height must be a multiple of 16 for frame picture or a multiple of 32 for field picture
    info->Height = (MFX_PICSTRUCT_PROGRESSIVE == info->PicStruct) ? ALIGN16(info->CropH) : ALIGN32(info->CropH);
}

#endif //__EXAMPLES_UTIL_H__
