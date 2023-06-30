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

inline void getArgs_csc(int argc, char **argv, char *pfilename, char *poutfilename, int &frames, int &width,
                        int &height, impl_video_format &in_format, impl_video_format &out_format,
                        bool &enable_profiling, bool &is_target_cpu, bool &pre_read, bool &perfopt) {
    std::string infile, outfile, informat_name, outformat_name, device_name;
    ParseContext P;

    P.GetCommand(argc, argv);
    P.get("-size", "widthXheight           set input size to [width x height]", width, height, "1920X1080", true);
    P.get("-frame", "n                     check n frames", &frames, (int)1, false);
    P.get("-profile", "                    enable profiling, disabled by default", &enable_profiling, (bool)false,
          false);
    P.get("-in_format", "input_format      set input picture format", &informat_name, (std::string) "", true);
    P.get("-out_format", "output_filenset  output picture filename", &outformat_name, (std::string) "", true);
    P.get("-o", "output_filename           set output picture filename", &outfile, (std::string) "", false);
    P.get("-i", "input_filename            set input picture file", &infile, (std::string) "", true);
    P.get("-d", "device                    set gpu(default)/cpu", &device_name, (std::string) "gpu", false);
    P.get("-pre_read", "                   pre_read one frame in gpu buffer", &pre_read, (bool)false, false);
    P.get("-perfopt", "                    process frame in best performance", &perfopt, (bool)false, false);
    P.check("usage:\tcsc [options]\noptions:");

    in_format     = GetIMPLformat(informat_name);
    out_format    = GetIMPLformat(outformat_name);
    is_target_cpu = getdevice(device_name);
    memcpy(pfilename, infile.c_str(), infile.length() + 1);
    memcpy(poutfilename, outfile.c_str(), outfile.length() + 1);
    if (perfopt) {
        info("perfopt=1 means enable_profiling=0, pre_read=1");
        enable_profiling = 0;
        pre_read         = 1;
    }
    info("input bitstream %s src_size %dx%d in_format %s out_format %s\n ", pfilename, width, height,
         informat_name.c_str(), outformat_name.c_str());
}

int main(int argc, char **argv) {
    // input picture
    char pfilename[512] = "./xxx.yuv";
    // output picture
    char outputfilename[512] = "";
    bool is_target_cpu       = 0;
    int ret                  = 0;
    FILE *input;
    FILE *output;
    // set width=1920, height=1080, frames=1 by default
    int width  = 1920;
    int height = 1080;
    int frames = 1;
    size_t in_size, out_size;
    unsigned char *data    = nullptr;
    unsigned char *buf_in  = nullptr;
    unsigned char *buf_dst = nullptr;
    unsigned char *buf_cpu = nullptr;
    impl_video_format csc_in_format, csc_out_format;

    // no output file by default
    bool output_file = false;
    // no profiling by default
    bool enable_profiling = false;
    // do not pre read frames
    bool pre_read = false;
    bool perfopt  = false;
    // record cpu time for each frame and total frames
    double duration_cpu0 = 0.0;
    double duration_cpu  = 0.0;
    // record copy time from cpu to gpu for each frame and total frames
    double duration_cpy  = 0.0;
    double duration_cpy0 = 0.0;
    // record CSC gpu time for each frame and total frames
    double gpu_time_ns0 = 0.0;
    double gpu_time_ns  = 0.0;

    // get args from cmdline
    getArgs_csc(argc, argv, pfilename, outputfilename, frames, width, height, csc_in_format, csc_out_format,
                enable_profiling, is_target_cpu, pre_read, perfopt);
    // init queue depend on device type
    void *pq = impl_common_init(is_target_cpu, enable_profiling);
    // if outfilename is empty, no output file
    if (strcmp(outputfilename, "") != 0)
        output_file = true;
    // if outfilename is not empty. open output file
    if (output_file == true) {
        info("CSC %d frames to file %s\n", frames, outputfilename);
        output = fopen(outputfilename, "wb");
    }
    // open input file
    input = fopen(pfilename, "rb");
    if (input == NULL) {
        err("input file does not exit\n");
        return -1;
    }
    // malloc data, buf_in, buf_dst, buf_cpu
    data    = impl_image_mem_alloc(pq, csc_in_format, width, height, IMPL_MEM_TYPE_HOST, &in_size);
    buf_in  = impl_image_mem_alloc(pq, csc_in_format, width, height, IMPL_MEM_TYPE_DEVICE, NULL);
    buf_dst = impl_image_mem_alloc(pq, csc_out_format, width, height, IMPL_MEM_TYPE_DEVICE, &out_size);
    buf_cpu = impl_image_mem_alloc(pq, csc_out_format, width, height, IMPL_MEM_TYPE_HOST, NULL);
    // define and init impl_csc_params object
    struct impl_csc_params csc_params;
    void *pcsc_context = NULL;
    memset(&csc_params, 0, sizeof(csc_params));
    csc_params.pq         = pq;
    csc_params.is_async   = false;
    csc_params.in_format  = csc_in_format;
    csc_params.out_format = csc_out_format;
    csc_params.width      = width;
    csc_params.height     = height;

    ret = impl_csc_init(&csc_params, pcsc_context);
    CHECK_IMPL(ret, "impl_csc_init");

    {
        // do not count in time for the first frame for Just-in-Time compilation, start frame time is always very long,
        // ignore it for AOT
        ret = fread_repeat(data, 1, in_size, input, 0);
        CHECK_IMPL(ret, "read one frame from file");
        impl_common_mem_copy(pq, NULL, buf_in, data, in_size, NULL, 1);
        std::chrono::high_resolution_clock::time_point s, e;
        s = std::chrono::high_resolution_clock::now();

        ret = impl_csc_run(&csc_params, pcsc_context, buf_in, buf_dst, NULL);
        CHECK_IMPL(ret, "impl_csc_run");

        e             = std::chrono::high_resolution_clock::now();
        duration_cpu0 = std::chrono::duration<double, std::milli>(e - s).count();
        if (output_file == true) {
            // copy back from gpu to cpu
            impl_common_mem_copy(pq, NULL, buf_cpu, buf_dst, out_size, NULL, 1);
            fwrite(buf_cpu, 1, out_size, output);
        }
        info("First frame kenel CPU time=%lfms\nnote: first frame time is not counted in!!!\n", duration_cpu0);
    }

    info("\nStart processing...\n");
    for (int f = 1; f < frames; f++) {
        if (!pre_read) {
            ret = fread_repeat(data, 1, in_size, input, f);
            CHECK_IMPL(ret, "read one frame from file");
        }
        std::chrono::high_resolution_clock::time_point s, e;
        s = std::chrono::high_resolution_clock::now();
        if (!pre_read)
            impl_common_mem_copy(pq, NULL, buf_in, data, in_size, NULL, 1);
        e = std::chrono::high_resolution_clock::now();
        // count copy input image time
        duration_cpy0 = std::chrono::duration<double, std::milli>(e - s).count();
        duration_cpy += duration_cpy0;

        s = std::chrono::high_resolution_clock::now();

        ret = impl_csc_run(&csc_params, pcsc_context, buf_in, buf_dst, NULL);
        CHECK_IMPL(ret, "impl_csc_run");

        e             = std::chrono::high_resolution_clock::now();
        duration_cpu0 = std::chrono::duration<double, std::milli>(e - s).count();
        duration_cpu += duration_cpu0;

        if (enable_profiling) {
            // get kernel gpu running time
            gpu_time_ns0 = impl_common_event_profiling(csc_params.evt);
            gpu_time_ns += gpu_time_ns0;
        }
        // info("Kernel GPU CPU time %dth frame=%lfms %lfms copy time=%lfms\n", f, gpu_time_ns0 / 1000000,
        // duration_cpu0, duration_cpy0);
        if (output_file == true) {
            // copy back from gpu to cpu
            impl_common_mem_copy(pq, NULL, buf_cpu, buf_dst, out_size, NULL, 1);
            fwrite(buf_cpu, 1, out_size, output);
        }
    }

    info("\nPre frame: kernel GPU CPU time %lfms %lfms copy time %lfms\n", gpu_time_ns / 1000000 / (frames - 1),
         duration_cpu / (frames - 1), duration_cpy / (frames - 1));

    impl_common_mem_free(pq, (void *)data);
    impl_common_mem_free(pq, (void *)buf_in);
    impl_common_mem_free(pq, (void *)buf_dst);
    impl_common_mem_free(pq, (void *)buf_cpu);
    ret = impl_csc_uninit(&csc_params, pcsc_context);
    CHECK_IMPL(ret, "impl_csc_uninit");
    fclose(input);
    if (output_file == true) {
        info("\nClosing output file...\n");
        fclose(output);
    }
    impl_common_uninit(pq);

    return 0;
}
