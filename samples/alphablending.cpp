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

inline void getArgsAlphaB(int argc, char **argv, impl_video_format &in_format, char *pfilename, char *poutfilename,
                          char *comppfilename, char *alphapfilename, int &frames, int &width, int &height,
                          int &compwidth, int &compheight, int &offset_x, int &offset_y, unsigned int &static_alpha,
                          bool &enable_profiling, bool &is_target_cpu, bool &pre_read, bool &perfopt) {
    std::string infile, outfile, device_name, format_name, compfile, alphafile_name;
    ParseContext P;

    P.GetCommand(argc, argv);
    P.get("-size", "widthXheight                    set size to [width x height]", width, height, "1920X1080", false);
    P.get("-in_format", "                           set the type of input picture", &format_name, (std::string) "",
          true);
    P.get("-compsize", "widthXheight                set composition size to [width x height]", compwidth, compheight,
          "960X540", true);
    P.get("-compfile", "filename                    set composition foreground file.yuv", &compfile, (std::string) "",
          false);
    P.get("-alphafile", "alpha_surface_filename     set alhpa surface file.yuv", &alphafile_name, (std::string) "",
          false);
    P.get("-frame", "n                              check n frames", &frames, (int)1, false);
    P.get("-profile", "                             enable profiling, disabled by default", &enable_profiling,
          (bool)false, false);
    P.get("-offset", "offset_xXoffset_y             set the offset of comp picture", offset_x, offset_y, "0X0", false);
    P.get("-static_alpha", "value                   set the value(0-255)", &static_alpha, (unsigned int)128, false);
    P.get("-o", "output_filename                    set output picture filename", &outfile, (std::string) "", false);
    P.get("-i", "input_filename                     set input picture file", &infile, (std::string) "", true);
    P.get("-d", "device                             set gpu(default)/cpu", &device_name, (std::string) "gpu", false);
    P.get("-pre_read", "                            pre_read one frame in gpu buffer", &pre_read, (bool)false, false);
    P.get("-perfopt", "                             process frame in best performance", &perfopt, (bool)false, false);
    P.check("usage:\talphablending  [options]\noptions:");

    is_target_cpu = getdevice(device_name);
    in_format     = GetIMPLformat(format_name);
    memcpy(pfilename, infile.c_str(), infile.length() + 1);
    memcpy(poutfilename, outfile.c_str(), outfile.length() + 1);
    memcpy(comppfilename, compfile.c_str(), compfile.length() + 1);
    memcpy(alphapfilename, alphafile_name.c_str(), alphafile_name.length() + 1);
    if (perfopt) {
        info("perfopt=1 means enable_profiling=0, pre_read=1");
        enable_profiling = 0;
        pre_read         = 1;
    }
    info("input image %s size %dx%d format %s\n", pfilename, width, height, format_name.c_str());
    info("input alphablending image %s size %dx%d\n", alphapfilename, compwidth, compheight);
}

int main(int argc, char **argv) {
    char pfilename[512]      = "./xxx.yuv";
    char in1pfilename[512]   = "./xxxin1.yuv";
    char alphapfilename[512] = "";
    FILE *input;
    FILE *input1;
    FILE *inputalpha;
    FILE *output;
    char outputfilename[512] = "";
    bool is_target_cpu       = 0;
    int ret                  = 0;
    int width                = 1920;
    int height               = 1080;
    size_t size, size1;
    int width1                 = 960;
    int height1                = 540;
    int frames                 = 1;
    int layers                 = 2;
    int offset_x               = 0;
    int offset_y               = 0;
    unsigned int static_alpha  = 128;
    unsigned char *buf_inalpha = NULL;
    void *data;
    void *data1;
    void *dataalpha;
    unsigned char *buf_in;
    unsigned char *buf_in1;
    unsigned char *buf_cpu;
    bool output_file      = false;
    bool enable_profiling = false;
    bool pre_read         = false;
    bool perfopt          = false;
    impl_video_format alphab_format;

    getArgsAlphaB(argc, argv, alphab_format, pfilename, outputfilename, in1pfilename, alphapfilename, frames, width,
                  height, width1, height1, offset_x, offset_y, static_alpha, enable_profiling, is_target_cpu, pre_read,
                  perfopt);

    void *pq = impl_common_init(is_target_cpu, enable_profiling);

    if (strcmp(outputfilename, "") != 0)
        output_file = true;

    input = fopen(pfilename, "rb");
    if (input == NULL) {
        err("input file does not exit\n");
        return -1;
    }
    input1 = fopen(in1pfilename, "rb");
    if (input1 == NULL) {
        err("alphablending file does not exit\n");
        return -1;
    }
    if (output_file == true) {
        info("Alphablending %d frames, outputfile is %s\n", frames, outputfilename);
        output = fopen(outputfilename, "wb");
    }

    buf_in  = impl_image_mem_alloc(pq, alphab_format, width, height, IMPL_MEM_TYPE_DEVICE, &size);
    buf_in1 = impl_image_mem_alloc(pq, alphab_format, width1, height1, IMPL_MEM_TYPE_DEVICE, &size1);
    data    = impl_image_mem_alloc(pq, alphab_format, width, height, IMPL_MEM_TYPE_HOST, &size);
    data1   = impl_image_mem_alloc(pq, alphab_format, width1, height1, IMPL_MEM_TYPE_HOST, &size1);
    buf_cpu = impl_image_mem_alloc(pq, alphab_format, width, height, IMPL_MEM_TYPE_HOST, NULL);

    std::chrono::high_resolution_clock::time_point s, e;
    double duration_cpy  = 0.0;
    double duration_cpy0 = 0.0;
    double duration_cpu  = 0.0;
    double duration_cpu0 = 0.0;
    double gpu_time_ns   = 0.0;
    double gpu_time_ns0  = 0.0;

    void *fieldbuffs[IMPL_MIXER_MAX_FIELDS];
    void *dep_events[IMPL_MIXER_MAX_FIELDS];
    struct impl_mixer_params alphab_params;
    struct impl_mixer_field_params *pfield;
    void *pmx_context = nullptr;
    memset(&alphab_params, 0, sizeof(alphab_params));
    alphab_params.pq       = pq;
    alphab_params.is_async = false;
    alphab_params.layers   = layers;
    alphab_params.format   = alphab_format;
    pfield                 = &(alphab_params.field[0]);
    pfield->field_idx      = 0;
    pfield->width          = width;
    pfield->height         = height;
    pfield->buff           = (void *)buf_in;
    pfield                 = &(alphab_params.field[1]);
    pfield->field_idx      = 1;
    pfield->width          = width1;
    pfield->height         = height1;
    pfield->offset_x       = offset_x;
    pfield->offset_y       = offset_y;
    pfield->buff           = (void *)buf_in1;
    pfield->is_alphab      = true;
    if (alphapfilename[0]) {
        int size2 = width1 * height1;
        info("input alpha surface from file %s\n", alphapfilename);
        inputalpha = fopen(alphapfilename, "rb");
        if (inputalpha == NULL) {
            err("%s, alpha value file does not exit\n", __func__);
            return -1;
        }
        dataalpha    = (unsigned char *)malloc(size2 * sizeof(unsigned char));
        int readsize = fread(dataalpha, 1, size2 * sizeof(unsigned char), inputalpha);
        (void)readsize;
        buf_inalpha = (unsigned char *)impl_common_mem_alloc(pq, 1, size2, IMPL_MEM_TYPE_DEVICE);
        impl_common_mem_copy(pq, NULL, buf_inalpha, dataalpha, size2 * sizeof(unsigned char), NULL, 1);
        pfield->alpha_surf = buf_inalpha;
    } else {
        pfield->static_alpha = static_alpha;
    }
    // to set field[2] for layers=3
    // alphab_params.layers = 3;
    // pfield = &(alphab_params.field[2]);
    // pfield->field_idx    = 2;
    // pfield->width        = width1;
    // pfield->height       = height1;
    // pfield->offset_x     = offset_x + 480;
    // pfield->offset_y     = offset_y + 480;
    // pfield->buff         = (void *)buf_in1;
    // pfield->is_alphab    = true;
    // pfield->static_alpha = static_alpha;
    // pfield->alpha_surf   = buf_inalpha;
    ret = impl_mixer_init(&alphab_params, pmx_context);
    CHECK_IMPL(ret, "impl_mixer_init alpha blending");
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

        ret = impl_mixer_run(&alphab_params, pmx_context, fieldbuffs, dep_events);
        CHECK_IMPL(ret, "impl_mixer_run alpha blending");

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

        ret = impl_mixer_run(&alphab_params, pmx_context, fieldbuffs, dep_events);
        CHECK_IMPL(ret, "impl_mixer_run alpha blending");

        e = std::chrono::high_resolution_clock::now();
        // count kernel running cpu time
        duration_cpu0 = std::chrono::duration<double, std::milli>(e - s).count();
        duration_cpu += duration_cpu0;

        if (enable_profiling) {
            // get kernel gpu running time
            gpu_time_ns0 = 0;
            for (int layer = 1; layer < alphab_params.layers; layer++)
                gpu_time_ns0 += impl_common_event_profiling(alphab_params.field[layer].evt);
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
    if (alphapfilename[0]) {
        free(dataalpha);
        impl_common_mem_free(pq, buf_inalpha);
        fclose(inputalpha);
    }

    fclose(input);
    fclose(input1);
    if (output_file == true) {
        info("\nClosing output file...\n");
        fclose(output);
    }
    ret = impl_mixer_uninit(&alphab_params, pmx_context);
    impl_common_uninit(pq);
    (void)ret;

    return 0;
}
