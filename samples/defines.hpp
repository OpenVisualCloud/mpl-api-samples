/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#pragma once

#include <string>
#include <unordered_map>

#include "impl_api.h"
#include "log.h"

#define CHECK_IMPL(x, y)                                                                                               \
    if (x != IMPL_STATUS_SUCCESS) {                                                                                    \
        err("failed in %s ret=%d\n", y, x);                                                                            \
        return -1;                                                                                                     \
    }

inline impl_video_format GetIMPLformat(std::string tempformat) {
    std::unordered_map<std::string, impl_video_format> format_map{{
                                                                      "nv12",
                                                                      IMPL_VIDEO_NV12,
                                                                  },
                                                                  {
                                                                      "v210",
                                                                      IMPL_VIDEO_V210,
                                                                  },
                                                                  {
                                                                      "y210",
                                                                      IMPL_VIDEO_Y210,
                                                                  },
                                                                  {
                                                                      "yuv422p10le",
                                                                      IMPL_VIDEO_YUV422P10LE,
                                                                  },
                                                                  {
                                                                      "yuv420p10le",
                                                                      IMPL_VIDEO_YUV420P10LE,
                                                                  },
                                                                  {
                                                                      "i420",
                                                                      IMPL_VIDEO_I420,
                                                                  },
                                                                  {
                                                                      "p010",
                                                                      IMPL_VIDEO_P010,
                                                                  },
                                                                  {
                                                                      "yuv422ycbcr10be",
                                                                      IMPL_VIDEO_YUV422YCBCR10BE,
                                                                  },
                                                                  {
                                                                      "yuv422ycbcr10le",
                                                                      IMPL_VIDEO_YUV422YCBCR10LE,
                                                                  }};
    std::unordered_map<std::string, impl_video_format>::iterator it;
    impl_video_format format = IMPL_VIDEO_MAX;
    it                       = format_map.find(tempformat);
    if (it != format_map.end()) {
        format = it->second;
    } else {
        err("input format %s is not supported\n", tempformat.c_str());
        exit(-1);
    }
    return format;
}

inline int fread_repeat(void *data, size_t num, int sizef, FILE *input, int framenum) {
    int readsize = fread(data, num, sizef, input);
    if (readsize != sizef) {
        fseek(input, 0, SEEK_SET);
        info("%s, read input file seek0 and read again!!! f=%d readsize=%d size=%d\n", __func__, framenum, readsize,
             sizef);
        // Fill the buffer with a complete frame
        readsize = fread(data, num, sizef, input);
        if (readsize != sizef) {
            err("%s, read input file error!!!\n", __func__);
            exit(-1);
        }
    }
    return 0;
}

inline bool getdevice(std::string device_name) {
    bool is_cpu;
    if (device_name == "gpu")
        is_cpu = false;
    else if (device_name == "cpu")
        is_cpu = true;
    else {
        err("%s, illegal device name (cpu or gpu)\n", __func__);
        exit(1);
    }
    return is_cpu;
}
