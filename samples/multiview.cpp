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

// Multi-view pipeline overview:
// Multi-view pipeline subxy * subxy yuv422ycbcr10be inputs and 1 yuv422ycbcr10be output
// subxy * subxy input files in subxy * subxy channels --> Resize yuv422ycbcr10be --> 1
// output file

inline void getArgsMultiview(int argc, char **argv, char *pfilename, char *poutfilename, int &frames, int &width,
                             int &height, int &subxy, int &sync, bool &enable_profiling, bool &is_target_cpu,
                             bool &pre_read) {
    std::string infile, outfile, device_name;
    ParseContext P;

    P.GetCommand(argc, argv);
    P.get("-size", "widthXheight        set input size to [width x height]", width, height, "1920X1080", false);
    P.get("-frame", "n                  check n frames", &frames, (int)1, false);
    P.get("-profile", "                 enable profiling, disabled by default", &enable_profiling, (bool)false, false);
    P.get("-subxy", "sub                downscale width or height, supports 2, 3, 4", &subxy, (int)4, false);
    P.get("-o", "output_filename        set output picture filename", &outfile, (std::string) "", false);
    P.get("-i", "input_filename         set input picture file", &infile, (std::string) "", true);
    P.get("-sync", "sync_mode           set sync mode, 0, async; 1, sync", &sync, (int)0, false);
    P.get("-d", "device                 set gpu(default)/cpu", &device_name, (std::string) "gpu", false);
    P.get("-pre_read", "                pre_read one frame in gpu buffer", &pre_read, (bool)false, false);
    P.check("usage:\tmultiview [options]\noptions:");

    is_target_cpu = getdevice(device_name);
    memcpy(pfilename, infile.c_str(), infile.length() + 1);
    memcpy(poutfilename, outfile.c_str(), outfile.length() + 1);
    info("input bitstream %s size %d %d downscale by %dx sync %d, profile %d\n", pfilename, width, height,
         subxy * subxy, sync, enable_profiling);
}

// record copy + gpu time for each frame(including subxy * subxy channels) and total
// frames
double duration_cpu0 = 0.0;
double duration_cpu  = 0.0;
// record copy time for each frame and total frames
double duration_cpy0 = 0.0;
double duration_cpy  = 0.0;
// record resize time for each frame and total frames
double duration_rs0 = 0.0;
double duration_rs  = 0.0;
// record gpu time for each frame and total frames
double gpu_time_ns0 = 0.0;
double gpu_time_ns  = 0.0;
// record copy back time for each frame and total frames
double duration_cpy_back0 = 0.0;
double duration_cpy_back  = 0.0;

// channels value is 16 by default
int channels = 16;
// no output file by default
bool output_file = false;
// not profiling by default
bool enable_profiling = false;
// set async mode by default
int sync_mode = false;
// pre-read in N frames and process only N frames
int N = 20;
// set return value of IMPL function IMPL_STATUS_FAIL by default
int ret = IMPL_STATUS_FAIL;

// process one frame in async mode
int process_one_frame_async(void *pq, unsigned char *data, unsigned char **buf_in, unsigned char *buf_final,
                            unsigned char *buf_cpu, struct impl_resize_params *presize_params, void **prs_contextlist,
                            size_t size) {
    void *pcpy_event[channels];
    for (int channel = 0; channel < channels; channel++) {
        pcpy_event[channel] = impl_common_new_event();
        impl_common_mem_copy(pq, pcpy_event[channel], buf_in[channel], data, size * sizeof(unsigned char), NULL, 0);
        ret = impl_resize_run(&presize_params[channel], prs_contextlist[channel], (unsigned char *)buf_in[channel],
                              (unsigned char *)buf_final, pcpy_event[channel]);
        CHECK_IMPL(ret, "impl_resize_run");
    }

    for (int channel = 0; channel < channels; channel++)
        impl_common_event_sync(presize_params[channel].evt);
    for (int channel = 0; channel < channels; channel++)
        impl_common_free_event(pcpy_event[channel]);
    // copy back from gpu to cpu
    impl_common_mem_copy(pq, NULL, buf_cpu, buf_final, size * sizeof(unsigned char), NULL, 1);

    return IMPL_STATUS_SUCCESS;
}

// process one frame in sync mode
int process_one_frame_sync(void *pq, unsigned char *data, unsigned char **buf_in, unsigned char *buf_final,
                           unsigned char *buf_cpu, struct impl_resize_params *presize_params, void **prs_contextlist,
                           size_t size) {
    std::chrono::high_resolution_clock::time_point s0, s1, e0;
    for (int channel = 0; channel < channels; channel++) {
        s0 = std::chrono::high_resolution_clock::now();
        impl_common_mem_copy(pq, NULL, buf_in[channel], data, size * sizeof(unsigned char), NULL, 1);

        s1 = std::chrono::high_resolution_clock::now();
        // record copy cpu time
        duration_cpy0 += std::chrono::duration<double, std::milli>(s1 - s0).count();

        ret = impl_resize_run(&presize_params[channel], prs_contextlist[channel], (unsigned char *)buf_in[channel],
                              (unsigned char *)buf_final, NULL);
        CHECK_IMPL(ret, "impl_resize_run");
        // record resize gpu time
        if (enable_profiling) {
            gpu_time_ns0 += impl_common_event_profiling(presize_params[channel].evt);
        }
        e0 = std::chrono::high_resolution_clock::now();
        // record resize cpu time
        duration_rs0 += std::chrono::duration<double, std::milli>(e0 - s1).count();
    }

    // copy back from gpu to cpu
    std::chrono::high_resolution_clock::time_point s, e;
    s = std::chrono::high_resolution_clock::now();
    impl_common_mem_copy(pq, NULL, buf_cpu, buf_final, size * sizeof(unsigned char), NULL, 1);
    e                  = std::chrono::high_resolution_clock::now();
    duration_cpy_back0 = std::chrono::duration<double, std::milli>(e - s).count();
    return IMPL_STATUS_SUCCESS;
}

int main(int argc, char **argv) {
    FILE *input;
    // input picture
    char pfilename[512] = "./xxx.yuv";
    // output picture
    char outfilename[512] = "";
    unsigned char *data;
    unsigned char **pbuf_in;
    unsigned char *buf_final;
    unsigned char *buf_cpu;
    // set width=1920, height=1080, frames=1, subxy=4 by default
    int width          = 1920;
    int height         = 1080;
    int frames         = 1; // 300;
    int subxy          = 4;
    bool is_target_cpu = false;
    bool pre_read      = false;
    int frame_idx      = 0;

    // get args from cmdline
    getArgsMultiview(argc, argv, pfilename, outfilename, frames, width, height, subxy, sync_mode, enable_profiling,
                     is_target_cpu, pre_read);
    if (subxy != 1 && subxy != 2 && subxy != 4) {
        err("subxy only supports 1, 2 ,4\n");
        return -1;
    }

    // init queue depend on device type
    void *pq = impl_common_init(is_target_cpu, enable_profiling);

    // total subxy * subxy channels
    channels = subxy * subxy;
    // source yuv422ycbcr10be size
    size_t size;
    // destination yuv422ycbcr10be size
    int width_rs  = 0;
    int height_rs = 0;
    width_rs      = width / subxy;
    height_rs     = height / subxy;

    FILE *output;
    // if outfilename is empty, no output file
    if (strcmp(outfilename, "") != 0)
        output_file = true;
    // if outfilename is not empty. open output file
    if (output_file == true) {
        info("Processing %d frames to file %s\n", frames, outfilename);
        output = fopen(outfilename, "wb");
    }
    // open input file
    input = fopen(pfilename, "rb");
    if (input == NULL) {
        err("%s, input file does not exit\n", __func__);
        return -1;
    }

    // malloc data, pbuf_in, pbuf_dst, pbuf_out, buf_final, buf_cpu
    if (pre_read)
        data = impl_image_mem_alloc(pq, IMPL_VIDEO_YUV422YCBCR10BE, width, height * N, IMPL_MEM_TYPE_HOST, NULL);
    else
        data = impl_image_mem_alloc(pq, IMPL_VIDEO_YUV422YCBCR10BE, width, height, IMPL_MEM_TYPE_HOST, &size);
    pbuf_in = (unsigned char **)malloc(sizeof(unsigned char *) * channels);
    for (int channel = 0; channel < channels; channel++) {
        // input yuv422ycbcr10be size
        pbuf_in[channel] = (unsigned char *)impl_image_mem_alloc(pq, IMPL_VIDEO_YUV422YCBCR10BE, width, height,
                                                                 IMPL_MEM_TYPE_DEVICE, &size);
    }
    buf_final = (unsigned char *)impl_image_mem_alloc(pq, IMPL_VIDEO_YUV422YCBCR10BE, width, height,
                                                      IMPL_MEM_TYPE_DEVICE, &size);
    buf_cpu =
        (unsigned char *)impl_image_mem_alloc(pq, IMPL_VIDEO_YUV422YCBCR10BE, width, height, IMPL_MEM_TYPE_HOST, &size);

    // define and malloc impl_resize_params, impl_csc_params objects
    struct impl_resize_params *presize_params;
    void **prs_contextlist;
    presize_params  = (impl_resize_params *)malloc(sizeof(impl_resize_params) * channels);
    prs_contextlist = (void **)malloc(sizeof(void *) * channels);

    int x_value = 0, y_value = 0;
    for (int channel = 0; channel < channels; channel++) {
        memset(&presize_params[channel], 0, sizeof(impl_resize_params));
        presize_params[channel].pq         = pq;
        presize_params[channel].format     = IMPL_VIDEO_YUV422YCBCR10BE;
        presize_params[channel].src_width  = width;
        presize_params[channel].src_height = height;
        presize_params[channel].dst_width  = width_rs;
        presize_params[channel].dst_height = height_rs;
        // pitch_pixel refers to the width of destination buffer
        // offset_x and offset_y refer to the upper left coordinates of one scaling image in
        // destination buffer
        presize_params[channel].pitch_pixel = width;
        presize_params[channel].offset_x    = x_value;
        presize_params[channel].offset_y    = y_value;
        presize_params[channel].is_async    = !sync_mode;

        ret = impl_resize_init(&presize_params[channel], prs_contextlist[channel]);
        CHECK_IMPL(ret, "impl_resize_init");
        if ((x_value + width_rs) == width) {
            x_value = 0;
            y_value = y_value + height_rs;
        } else {
            x_value = x_value + width_rs;
        }
    }

    if (pre_read) {
        // read N frames from input to data
        ret = fread(data, 1, size * sizeof(unsigned char) * N, input);
    } else {
        ret = fread(data, 1, size * sizeof(unsigned char), input);
    }
    unsigned char *data_start = data;

    {
        // do not count in time for the first frame for Just-in-Time compilation, start frame time is always very long,
        // ignore it for AOT
        std::chrono::high_resolution_clock::time_point s, e;
        s = std::chrono::high_resolution_clock::now();

        if (sync_mode == false)
            ret = process_one_frame_async(pq, data_start, pbuf_in, buf_final, buf_cpu, presize_params, prs_contextlist,
                                          size);
        else
            ret = process_one_frame_sync(pq, data_start, pbuf_in, buf_final, buf_cpu, presize_params, prs_contextlist,
                                         size);
        CHECK_IMPL(ret, "process_one_frame");

        e             = std::chrono::high_resolution_clock::now();
        duration_cpu0 = std::chrono::duration<double, std::milli>(e - s).count();

        // write output file
        if (output_file == true)
            fwrite(buf_cpu, 1, size, output);
        info("First frame pipeline CPU time=%lfms\nnote: first frame time is not counted in!!!\n", duration_cpu0);
        // clear cpu and gpu time for start frame
        duration_cpu      = 0.0;
        gpu_time_ns       = 0.0;
        duration_cpy      = 0.0;
        duration_rs       = 0.0;
        duration_cpy_back = 0.0;

        if (pre_read)
            frame_idx++;
    }

    info("\nStart processing...\n");
    for (int f = 1; f < frames; f++) {
        if (pre_read) {
            // if already read N frames, read from the beginning
            if (frame_idx == N) {
                info("read again!!!, the %dth frame\n", frame_idx);
                frame_idx = 0;
            }
            // compute the location of temp frame in data
            data_start = data + size * sizeof(unsigned char) * frame_idx;
        } else {
            // read one frame repeatly
            ret = fread_repeat(data_start, 1, size, input, f);
        }
        duration_cpu0      = 0;
        gpu_time_ns0       = 0;
        duration_cpy0      = 0;
        duration_rs0       = 0;
        duration_cpy_back0 = 0;
        std::chrono::high_resolution_clock::time_point s, e;
        s       = std::chrono::high_resolution_clock::now();
        int ret = 0;
        if (sync_mode == false)
            ret = process_one_frame_async(pq, data_start, pbuf_in, buf_final, buf_cpu, presize_params, prs_contextlist,
                                          size);
        else
            ret = process_one_frame_sync(pq, data_start, pbuf_in, buf_final, buf_cpu, presize_params, prs_contextlist,
                                         size);
        CHECK_IMPL(ret, "process_one_frame");
        e             = std::chrono::high_resolution_clock::now();
        duration_cpu0 = std::chrono::duration<double, std::milli>(e - s).count();
        duration_cpu += duration_cpu0;
        if (sync_mode == false) {
            // info("CPU time for multiview %dth frame %lfms\n",f, duration_cpu0);
        } else {
            duration_cpy += duration_cpy0;
            duration_rs += duration_rs0;
            duration_cpy_back += duration_cpy_back0;
            // info("GPU CPU time for multiview %dth frame %lf/%lfms copy/rs/copy_back=%lf/%lf/%lfms\n", f, gpu_time_ns0
            // / 1000000, duration_cpu0, duration_cpy0, duration_rs0, duration_cpy_back0);
        }
        if (pre_read)
            frame_idx++;

        if (output_file == true)
            fwrite(buf_cpu, 1, size, output);
    }

    if (sync_mode == false)
        info("\nCPU time per frame=%lfms\n", duration_cpu / (frames - 1));
    else {
        info("\nGPU CPU time per frame=%lfms %lfms copy/rs/copy_back=%lf/%lf/%lfms\n",
             gpu_time_ns / 1000000 / (frames - 1), duration_cpu / (frames - 1), duration_cpy / (frames - 1),
             duration_rs / (frames - 1), duration_cpy_back / (frames - 1));
    }
    info("total %d frames fps=%lf\n", (frames - 1), 1 / (duration_cpu / (frames - 1) / 1000));

    for (int channel = 0; channel < channels; channel++) {
        impl_common_mem_free(pq, (void *)pbuf_in[channel]);
    }
    impl_common_mem_free(pq, (void *)data);
    impl_common_mem_free(pq, (void *)buf_final);
    impl_common_mem_free(pq, (void *)buf_cpu);
    free(pbuf_in);
    for (int channel = 0; channel < channels; channel++) {
        ret = impl_resize_uninit(&presize_params[channel], prs_contextlist[channel]);
        CHECK_IMPL(ret, "impl_resize_uninit");
    }
    free(prs_contextlist);
    if (output_file == true) {
        info("\nClosing output file...\n");
        fclose(output);
    }
    impl_common_uninit(pq);

    return 0;
}
