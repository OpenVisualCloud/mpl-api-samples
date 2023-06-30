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

inline void getArgs_resize(int argc, char **argv, char *pfilename, char *poutfilename, int &frames, int &src_width,
                           int &src_height, int &dst_width, int &dst_height, impl_video_format &in_format,
                           impl_interp_mtd &interp_mtd, bool &enable_profiling, bool &is_target_cpu, bool &pre_read,
                           bool &perfopt) {
    std::string interp_name, infile, outfile, format_name, device_name;
    ParseContext P;

    P.GetCommand(argc, argv);
    P.get("-in_size", "widthXheight     set input size to [width x height]", src_width, src_height, "1920X1080", true);
    P.get("-frame", "n                  check n frames", &frames, (int)1, false);
    P.get("-profile", "                 enable profiling, disabled by default", &enable_profiling, (bool)false, false);
    P.get("-out_size", "widthXheight    set output size]", dst_width, dst_height, "3840X2160", true);
    P.get("-in_format", "input_format   set input picture format", &format_name, (std::string) "", true);
    P.get("-interp_mtd", "              set interpolate method", &interp_name, (std::string) "bilinear", false);
    P.get("-o", "output_filename        set output picture filename", &outfile, (std::string) "", false);
    P.get("-i", "input_filename         set input picture file", &infile, (std::string) "", true);
    P.get("-d", "device                 set gpu(default)/cpu", &device_name, (std::string) "gpu", false);
    P.get("-pre_read", "                pre_read one frame in gpu buffer", &pre_read, (bool)false, false);
    P.get("-perfopt", "                 process frame in best performance", &perfopt, (bool)false, false);
    P.check("usage:\tresize [options]\noptions:");
    in_format     = GetIMPLformat(format_name);
    is_target_cpu = getdevice(device_name);

    if (interp_name == "bilinear") {
        interp_mtd = IMPL_INTERP_MTD_BILINEAR;
    } else if (interp_name == "bicubic") {
        interp_mtd = IMPL_INTERP_MTD_BICUBIC;
    } else {
        err("%s, interp_mtd is not supported (bilinear or bicubic)\n", __func__);
        exit(1);
    }
    if (perfopt) {
        info("perfopt=1 means enable_profiling=0, pre_read=1");
        enable_profiling = 0;
        pre_read         = 1;
    }
    memcpy(pfilename, infile.c_str(), infile.length() + 1);
    memcpy(poutfilename, outfile.c_str(), outfile.length() + 1);
    info("input bitstream %s src_size %dx%d dst_size %dx%d in_format %s interp_mtd %s\n", pfilename, src_width,
         src_height, dst_width, dst_height, format_name.c_str(), interp_name.c_str());
}

int main(int argc, char **argv) {
    // basic variables for running
    char pfilename[512] = "./xxx.yuv";
    FILE *input;
    FILE *output;
    char outputfilename[512] = "";
    bool is_target_cpu       = 0;
    int src_width            = 1920;
    int src_height           = 1080;
    int dst_width            = 3840;
    int dst_height           = 2160;
    int frames               = 1;
    int ret                  = 0;
    void *pq;
    void *cpy_evt;
    impl_video_format resize_format;
    impl_interp_mtd interp_mtd;
    size_t in_size, out_size;
    struct impl_resize_params resize_params;
    void *prs_context;

    unsigned char *data    = nullptr;
    unsigned char *buf_in  = nullptr;
    unsigned char *buf_out = nullptr;
    unsigned char *buf_cpu = nullptr;
    bool output_file       = false;
    bool enable_profiling  = false;
    bool pre_read          = false;
    bool perfopt           = false;
    // Variables for recording time
    std::chrono::high_resolution_clock::time_point s, e;
    double duration_cpy  = 0.0;
    double duration_cpy0 = 0.0;
    double duration_cpu  = 0.0;
    double duration_cpu0 = 0.0;
    double gpu_time_ns   = 0.0;
    double gpu_time_ns0  = 0.0;

    getArgs_resize(argc, argv, pfilename, outputfilename, frames, src_width, src_height, dst_width, dst_height,
                   resize_format, interp_mtd, enable_profiling, is_target_cpu, pre_read, perfopt);

    if (strcmp(outputfilename, "") != 0)
        output_file = true;

    pq      = impl_common_init(is_target_cpu, enable_profiling);
    cpy_evt = impl_common_new_event();
    input   = fopen(pfilename, "rb");
    if (input == NULL) {
        err("%s, input file does not exit\n", __func__);
        return -1;
    }
    if (output_file == true) {
        info("Resize %d frames to file %s\n", frames, outputfilename);
        output = fopen(outputfilename, "wb");
    }

    data    = impl_image_mem_alloc(pq, resize_format, src_width, src_height, IMPL_MEM_TYPE_HOST, &in_size);
    buf_in  = impl_image_mem_alloc(pq, resize_format, src_width, src_height, IMPL_MEM_TYPE_DEVICE, NULL);
    buf_out = impl_image_mem_alloc(pq, resize_format, dst_width, dst_height, IMPL_MEM_TYPE_DEVICE, &out_size);
    buf_cpu = impl_image_mem_alloc(pq, resize_format, dst_width, dst_height, IMPL_MEM_TYPE_HOST, NULL);

    s = std::chrono::high_resolution_clock::now();
    memset(&resize_params, 0, sizeof(resize_params));
    resize_params.pq         = pq;
    resize_params.format     = resize_format;
    resize_params.src_width  = src_width;
    resize_params.src_height = src_height;
    resize_params.dst_width  = dst_width;
    resize_params.dst_height = dst_height;
    resize_params.interp_mtd = interp_mtd;
    ret                      = impl_resize_init(&resize_params, prs_context);
    CHECK_IMPL(ret, "impl_resize_init");

    e             = std::chrono::high_resolution_clock::now();
    duration_cpu0 = std::chrono::duration<double, std::milli>(e - s).count();
    info("impl_init_resize_index_table use time %lfms\n", duration_cpu0);

    {
        // do not count in time for the first frame for Just-in-Time compilation, start frame time is always very long,
        // ignore it for AOT
        ret = fread_repeat(data, 1, in_size, input, 0);

        impl_common_mem_copy(pq, NULL, buf_in, data, in_size, NULL, 1);
        s = std::chrono::high_resolution_clock::now();

        ret = impl_resize_run(&resize_params, prs_context, buf_in, buf_out, NULL);
        CHECK_IMPL(ret, "impl_resize_run");

        e             = std::chrono::high_resolution_clock::now();
        duration_cpu0 = std::chrono::duration<double, std::milli>(e - s).count();

        if (output_file == true) {
            impl_common_mem_copy(pq, NULL, buf_cpu, buf_out, out_size, NULL, 1);
            fwrite(buf_cpu, 1, out_size, output);
        }
        info("First frame kenel CPU time=%lfms\nnote: first frame time is not counted in!!!\n", duration_cpu0);
    }

    info("\nStart processing...\n");
    for (int f = 1; f < frames; f++) {
        if (!pre_read)
            ret = fread_repeat(data, 1, in_size, input, f);
        s = std::chrono::high_resolution_clock::now();
        if (!pre_read)
            impl_common_mem_copy(pq, cpy_evt, buf_in, data, in_size, NULL, 1);
        e = std::chrono::high_resolution_clock::now();
        // count copy input image time
        duration_cpy0 = std::chrono::duration<double, std::milli>(e - s).count();
        duration_cpy += duration_cpy0;
        s = std::chrono::high_resolution_clock::now();

        ret = impl_resize_run(&resize_params, prs_context, buf_in, buf_out, NULL);
        CHECK_IMPL(ret, "impl_resize_run");

        e = std::chrono::high_resolution_clock::now();
        // count kernel running cpu time
        duration_cpu0 = std::chrono::duration<double, std::milli>(e - s).count();
        duration_cpu += duration_cpu0;

        if (enable_profiling) {
            gpu_time_ns0 = impl_common_event_profiling(resize_params.evt);
            gpu_time_ns += gpu_time_ns0;
        }

        if (output_file == true) {
            impl_common_mem_copy(pq, NULL, buf_cpu, buf_out, out_size, NULL, 1);
            fwrite(buf_cpu, 1, out_size, output);
        }
        // info("Kernel GPU CPU time %dth frame=%lfms %lfms copy time=%lfms\n", f, gpu_time_ns0 / 1000000,
        // duration_cpu0, duration_cpy0);
    }

    info("\nPre frame: kernel GPU CPU time %lfms %lfms copy time %lfms\n", gpu_time_ns / 1000000 / (frames - 1),
         duration_cpu / (frames - 1), duration_cpy / (frames - 1));

    impl_common_mem_free(pq, data);
    impl_common_mem_free(pq, buf_in);
    impl_common_mem_free(pq, buf_out);
    impl_common_mem_free(pq, buf_cpu);
    ret = impl_resize_uninit(&resize_params, prs_context);
    CHECK_IMPL(ret, "impl_resize_uninit");
    impl_common_free_event(cpy_evt);
    fclose(input);
    if (output_file == true) {
        info("\nClosing output file...\n");
        fclose(output);
    }
    impl_common_uninit(pq);

    return 0;
}
