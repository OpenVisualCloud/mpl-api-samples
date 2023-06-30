/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <chrono>
#include <string>

#include "defines.hpp"
#include "getargs.hpp"
#include "impl_api.h"
#include "log.h"

inline void getArgsComp(int argc, char **argv, impl_video_format &comp_format, char *pfilename, char *poutfilename,
                        char *comppfilename, int &frames, int &width, int &height, int &compwidth, int &compheight,
                        int &offset_x, int &offset_y, bool &enable_profiling, bool &is_target_cpu, bool &pre_read,
                        bool &perfopt) {
    std::string infile, outfile, device_name, format_name, compfile;
    ParseContext P;

    P.GetCommand(argc, argv);
    P.get("-size", "widthXheight         set size to [width x height]", width, height, "1920X1080", false);
    P.get("-in_format", "                set the type of input picture", &format_name, (std::string) "", true);
    P.get("-compsize", "widthXheight     set composition size to [width x height]", compwidth, compheight, "960X540",
          true);
    P.get("-compfile", "filename         set composition foreground file.yuv", &compfile, (std::string) "", false);
    P.get("-offset", "offset_xXoffset_y  set the offset of comp picture", offset_x, offset_y, "0X0", false);
    P.get("-frame", "n                   check n frames", &frames, (int)1, false);
    P.get("-profile", "                  enable profiling, disabled by default", &enable_profiling, (bool)false, false);
    P.get("-o", "output_filename         set output picture filename", &outfile, (std::string) "", false);
    P.get("-i", "input_filename          set input picture file", &infile, (std::string) "", true);
    P.get("-d", "device                  set gpu(default)/cpu", &device_name, (std::string) "gpu", false);
    P.get("-pre_read", "                 pre_read one frame in gpu buffer", &pre_read, (bool)false, false);
    P.get("-perfopt", "                  process frame in best performance", &perfopt, (bool)false, false);
    P.check("usage:\tcomposition [options]\noptions:");

    is_target_cpu = getdevice(device_name);
    comp_format   = GetIMPLformat(format_name);
    memcpy(pfilename, infile.c_str(), infile.length() + 1);
    memcpy(poutfilename, outfile.c_str(), outfile.length() + 1);
    memcpy(comppfilename, compfile.c_str(), compfile.length() + 1);
    if (perfopt) {
        info("perfopt=1 means enable_profiling=0, pre_read=1");
        enable_profiling = 0;
        pre_read         = 1;
    }
    info("input image %s size %dx%d format %s\n", pfilename, width, height, format_name.c_str());
    info("input composition image %s size %dx%d\n", comppfilename, compwidth, compheight);
}

int main(int argc, char **argv) {
    char pfilename[512]     = "./xxx.yuv";
    char comppfilename[512] = "./xxxcomp.yuv";
    FILE *input;
    FILE *input1;
    FILE *output;
    char outputfilename[512] = "";
    bool is_target_cpu       = 0;
    int ret                  = 0;
    int width                = 1920;
    int height               = 1080;
    size_t size, size1;
    int width1   = 960;
    int height1  = 540;
    int frames   = 1;
    int layers   = 2;
    int offset_x = 0;
    int offset_y = 0;
    void *data;
    void *data1;
    unsigned char *buf_in;
    unsigned char *buf_in1;
    unsigned char *buf_cpu;
    bool output_file      = false;
    bool enable_profiling = false;
    bool pre_read         = false;
    bool perfopt          = false;
    impl_video_format comp_format;

    getArgsComp(argc, argv, comp_format, pfilename, outputfilename, comppfilename, frames, width, height, width1,
                height1, offset_x, offset_y, enable_profiling, is_target_cpu, pre_read, perfopt);

    void *pq = impl_common_init(is_target_cpu, enable_profiling);

    if (strcmp(outputfilename, "") != 0)
        output_file = true;
    input = fopen(pfilename, "rb");
    if (input == NULL) {
        err("input file does not exit\n");
        return -1;
    }
    input1 = fopen(comppfilename, "rb");
    if (input1 == NULL) {
        err("composition file does not exit\n");
        return -1;
    }
    if (output_file == true) {
        info("Compositing %d frames, outputfile is %s\n", frames, outputfilename);
        output = fopen(outputfilename, "wb");
    }

    buf_in  = impl_image_mem_alloc(pq, comp_format, width, height, IMPL_MEM_TYPE_DEVICE, &size);
    buf_in1 = impl_image_mem_alloc(pq, comp_format, width1, height1, IMPL_MEM_TYPE_DEVICE, &size1);
    data    = impl_image_mem_alloc(pq, comp_format, width, height, IMPL_MEM_TYPE_HOST, &size);
    data1   = impl_image_mem_alloc(pq, comp_format, width1, height1, IMPL_MEM_TYPE_HOST, &size1);
    buf_cpu = impl_image_mem_alloc(pq, comp_format, width, height, IMPL_MEM_TYPE_HOST, NULL);

    std::chrono::high_resolution_clock::time_point s, e;
    double duration_cpy  = 0.0;
    double duration_cpy0 = 0.0;
    double duration_cpu  = 0.0;
    double duration_cpu0 = 0.0;
    double gpu_time_ns   = 0.0;
    double gpu_time_ns0  = 0.0;

    void *fieldbuffs[IMPL_MIXER_MAX_FIELDS];
    void *dep_events[IMPL_MIXER_MAX_FIELDS];
    struct impl_mixer_params comp_params;
    struct impl_mixer_field_params *pfield;
    void *pmx_context = nullptr;
    memset(&comp_params, 0, sizeof(comp_params));
    comp_params.pq       = pq;
    comp_params.is_async = false;
    comp_params.layers   = layers;
    comp_params.format   = comp_format;
    pfield               = &(comp_params.field[0]);
    pfield->field_idx    = 0;
    pfield->width        = width;
    pfield->height       = height;
    pfield->buff         = (void *)buf_in;
    pfield               = &(comp_params.field[1]);
    pfield->field_idx    = 1;
    pfield->width        = width1;
    pfield->height       = height1;
    pfield->offset_x     = offset_x;
    pfield->offset_y     = offset_y;
    pfield->buff         = (void *)buf_in1;

    // to set field[2] for layers=3
    // comp_params.layers   = 3;
    // pfield = &(comp_params.field[2]);
    // pfield->field_idx    = 2;
    // pfield->width        = width1;
    // pfield->height       = height1;
    // pfield->offset_x     = offset_x + 480;
    // pfield->offset_y     = offset_y + 480;
    // pfield->buff         = (void *)buf_in1;
    ret = impl_mixer_init(&comp_params, pmx_context);
    CHECK_IMPL(ret, "impl_mixer_init composition");
    memset(fieldbuffs, 0, IMPL_MIXER_MAX_FIELDS * sizeof(void *));
    memset(dep_events, 0, IMPL_MIXER_MAX_FIELDS * sizeof(void *));

    {
        // do not count in time for the first frame for Just-in-Time compilation, start frame time is always very long,
        // ignore it for AOT
        ret = fread_repeat(data, 1, size, input, 0);
        ret = fread_repeat(data1, 1, size1, input1, 0);

        impl_common_mem_copy(pq, NULL, buf_in, data, size, NULL, 1);
        impl_common_mem_copy(pq, NULL, buf_in1, data1, size1, NULL, 1);
        s = std::chrono::high_resolution_clock::now();

        ret = impl_mixer_run(&comp_params, pmx_context, fieldbuffs, dep_events);
        CHECK_IMPL(ret, "impl_mixer_run composition");

        e             = std::chrono::high_resolution_clock::now();
        duration_cpu0 = std::chrono::duration<double, std::milli>(e - s).count();

        if (output_file == true) {
            impl_common_mem_copy(pq, NULL, buf_cpu, buf_in, size, NULL, 1);
            fwrite(buf_cpu, 1, size, output);
        }

        info("First frame kenel CPU time=%lfms\nnote: first frame time is not counted in!!!\n", duration_cpu0);
    }

    info("\nStart processing...\n");
    for (int f = 1; f < frames; f++) {
        if (!pre_read) {
            ret = fread_repeat(data, 1, size, input, f);
            ret = fread_repeat(data1, 1, size1, input1, f);
        }
        s = std::chrono::high_resolution_clock::now();
        if (!pre_read) {
            impl_common_mem_copy(pq, NULL, buf_in, data, size, NULL, 1);
            impl_common_mem_copy(pq, NULL, buf_in1, data1, size1, NULL, 1);
        }
        e = std::chrono::high_resolution_clock::now();
        // count copy input image time
        duration_cpy0 = std::chrono::duration<double, std::milli>(e - s).count();
        duration_cpy += duration_cpy0;
        s = std::chrono::high_resolution_clock::now();

        ret = impl_mixer_run(&comp_params, pmx_context, fieldbuffs, dep_events);
        CHECK_IMPL(ret, "impl_mixer_run composition");

        e = std::chrono::high_resolution_clock::now();
        // count kernel running cpu time
        duration_cpu0 = std::chrono::duration<double, std::milli>(e - s).count();
        duration_cpu += duration_cpu0;

        if (enable_profiling) {
            // get kernel gpu running time
            gpu_time_ns0 = 0;
            for (int layer = 1; layer < comp_params.layers; layer++)
                gpu_time_ns0 += impl_common_event_profiling(comp_params.field[layer].evt);
            gpu_time_ns += gpu_time_ns0;
        }

        // info("Kernel GPU CPU time %dth frame=%lfms %lfms copy time=%lfms\n", f, gpu_time_ns0 / 1000000,
        // duration_cpu0, duration_cpy0);
        if (output_file == true) {
            impl_common_mem_copy(pq, NULL, buf_cpu, buf_in, size, NULL, 1);
            fwrite(buf_cpu, 1, size, output);
        }
    }

    info("\nPre frame: kernel GPU CPU time %lfms %lfms copy time %lfms\n", gpu_time_ns / 1000000 / (frames - 1),
         duration_cpu / (frames - 1), duration_cpy / (frames - 1));

    impl_common_mem_free(pq, data);
    impl_common_mem_free(pq, data1);
    impl_common_mem_free(pq, buf_cpu);
    impl_common_mem_free(pq, buf_in);
    impl_common_mem_free(pq, buf_in1);

    fclose(input);
    fclose(input1);
    if (output_file == true) {
        info("\nClosing output file...\n");
        fclose(output);
    }
    ret = impl_mixer_uninit(&comp_params, pmx_context);
    impl_common_uninit(pq);
    (void)ret;

    return 0;
}
