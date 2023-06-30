/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include "impl_common.hpp"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "impl_api.h"
#include "impl_csc.hpp"
#include "impl_trace.hpp"

float impl_version() {
    float version = IMPL_VERSION;
    info("%s, IMPL Src version is: %f\n", __func__, version);
    return version;
}

void *impl_common_init(bool is_target_cpu, bool enable_profiling) {
    queue *q0 = NULL;
    if (is_target_cpu) {
#ifdef ONLY_FOR_GPU
        info("%s warning:, The current library is compiled on the GPU using AOT, return "
             "NULL\n",
             __func__);
#else
        if (enable_profiling) {
            q0 = new queue(cpu_selector_v, cl::sycl::property_list{cl::sycl::property::queue::enable_profiling()});
        } else {
            q0 = new queue(cpu_selector_v);
        }
        info("%s, Selected CPU device: %s\n", __func__, q0->get_device().get_info<info::device::name>().c_str());
        // const sycl::device device{ cpu_selector{} };
        info("%s, Running on host", __func__);
#endif
    } else {
        if (enable_profiling) {
            q0 = new queue(gpu_selector_v, cl::sycl::property_list{cl::sycl::property::queue::enable_profiling()});
        } else {
            q0 = new queue(gpu_selector_v);
        }
        info("%s, Selected GPU device: %s\n", __func__, q0->get_device().get_info<info::device::name>().c_str());
        const sycl::device device{gpu_selector_v};
        info("%s, Running on device: %s, %s\n", __func__,
             device.get_platform().get_info<sycl::info::platform::name>().c_str(),
             device.get_info<sycl::info::device::name>().c_str());
    }
    return (void *)q0;
}

void *impl_common_mem_alloc(void *pq, int ebytes, int num_element, impl_mem_type type) {
    unsigned char *buf_ptr = NULL;
    queue q                = *(queue *)pq;
    int ebytes2index[5]    = {0, 0, 1, 0, 2};
    int switch_id          = (int)type * 3 + ebytes2index[ebytes];
    switch (switch_id) {
    case 0:
        buf_ptr = (unsigned char *)sycl::malloc_device<unsigned char>(num_element, q);
        break;
    case 1:
        buf_ptr = (unsigned char *)sycl::malloc_device<unsigned short>(num_element, q);
        break;
    case 2:
        buf_ptr = (unsigned char *)sycl::malloc_device<unsigned int>(num_element, q);
        break;

    case 3:
        buf_ptr = (unsigned char *)sycl::malloc_host<unsigned char>(num_element, q);
        break;
    case 4:
        buf_ptr = (unsigned char *)sycl::malloc_host<unsigned short>(num_element, q);
        break;
    case 5:
        buf_ptr = (unsigned char *)sycl::malloc_host<unsigned int>(num_element, q);
        break;

    case 6:
        buf_ptr = (unsigned char *)sycl::malloc_shared<unsigned char>(num_element, q);
        break;
    case 7:
        buf_ptr = (unsigned char *)sycl::malloc_shared<unsigned short>(num_element, q);
        break;
    case 8:
        buf_ptr = (unsigned char *)sycl::malloc_shared<unsigned int>(num_element, q);
        break;
    }
    IMPL_ASSERT(buf_ptr != NULL, "buf malloc fail");

    return (void *)buf_ptr;
}

unsigned char *impl_image_mem_alloc(void *pq, impl_video_format format, int width, int height, impl_mem_type type,
                                    size_t *allocated_size) {
    IMPL_ASSERT(pq != NULL, "image memory alloc failed, pq is null");
    IMPL_ASSERT((width % 2 == 0) && (height % 2 == 0), "width and height must be multiples of 2");
    queue q                = *(queue *)pq;
    unsigned char *buf_ptr = NULL;
    size_t byte_count, data_num;
    int width_pad = width;
    int switch_id;
    /*format_map : 0,1 - compression ratio; 2 - Element Byte length; 3 - type index*/
    std::unordered_map<impl_video_format, std::vector<int>> format_map{
        {IMPL_VIDEO_NV12, {3, 2, 1, 0}},           {IMPL_VIDEO_V210, {2, 3, 4, 2}},
        {IMPL_VIDEO_Y210, {2, 1, 2, 1}},           {IMPL_VIDEO_YUV422P10LE, {2, 1, 2, 1}},
        {IMPL_VIDEO_YUV420P10LE, {3, 2, 2, 1}},    {IMPL_VIDEO_I420, {3, 2, 1, 0}},
        {IMPL_VIDEO_P010, {3, 2, 2, 1}},           {IMPL_VIDEO_YUV422YCBCR10BE, {5, 2, 1, 0}},
        {IMPL_VIDEO_YUV422YCBCR10LE, {5, 2, 1, 0}}};
    std::vector<int> format_info = format_map[format];
    if (IMPL_VIDEO_V210 == format) {
        width_pad = (width + 47) / 48 * 48;
    }
    data_num   = width_pad * height * format_info[0] / format_info[1];
    byte_count = data_num * format_info[2];
    switch_id  = (int)type * 3 + format_info[3];

    switch (switch_id) {
    case 0:
        buf_ptr = (unsigned char *)sycl::malloc_device<unsigned char>(data_num, q);
        break;
    case 1:
        buf_ptr = (unsigned char *)sycl::malloc_device<unsigned short>(data_num, q);
        break;
    case 2:
        buf_ptr = (unsigned char *)sycl::malloc_device<unsigned int>(data_num, q);
        break;

    case 3:
        buf_ptr = (unsigned char *)sycl::malloc_host<unsigned char>(data_num, q);
        break;
    case 4:
        buf_ptr = (unsigned char *)sycl::malloc_host<unsigned short>(data_num, q);
        break;
    case 5:
        buf_ptr = (unsigned char *)sycl::malloc_host<unsigned int>(data_num, q);
        break;

    case 6:
        buf_ptr = (unsigned char *)sycl::malloc_shared<unsigned char>(data_num, q);
        break;
    case 7:
        buf_ptr = (unsigned char *)sycl::malloc_shared<unsigned short>(data_num, q);
        break;
    case 8:
        buf_ptr = (unsigned char *)sycl::malloc_shared<unsigned int>(data_num, q);
        break;
    }
    if (allocated_size != NULL) {
        *allocated_size = byte_count;
    }
    return buf_ptr;
}

void impl_common_mem_copy(void *pq, void *evt, void *dst, void *src, int size, void *dep_evt, bool is_sync) {
    queue q = *(queue *)pq;
    event copy_event;
    if (dep_evt)
        copy_event = q.memcpy(dst, src, size, *(event *)dep_evt);
    else
        copy_event = q.memcpy(dst, src, size);
    if (is_sync)
        copy_event.wait();
    if (evt)
        *(sycl::event *)evt = copy_event;
}

void impl_common_queue_sync(void *pq) {
    IMPL_ASSERT(pq != NULL, "sync failed, queue is null");
    queue q = *(queue *)pq;
    q.wait();
}

double impl_common_event_profiling(void *evt) {
    double gpu_time_ns = 0;
    IMPL_ASSERT(evt != NULL, "profiling failed, event is null");
    sycl::event event1 = *(sycl::event *)evt;
    gpu_time_ns        = event1.get_profiling_info<sycl::info::event_profiling::command_end>() -
                  event1.get_profiling_info<sycl::info::event_profiling::command_start>();
    return gpu_time_ns;
}

void impl_common_mem_free(void *pq, void *buf) {
    IMPL_ASSERT(buf != NULL, "free memory failed, buf is null");
    IMPL_ASSERT(pq != NULL, "free memory failed, pq is null");
    queue q = *(queue *)pq;
    sycl::free(buf, q);
}

void *impl_common_new_event() {
    event *evt = new event;
    return (void *)evt;
}

void impl_common_free_event(void *evt) {
    IMPL_ASSERT(evt != NULL, "free event failed, event is null");
    delete (event *)evt;
}

void impl_common_event_sync(void *evt) {
    IMPL_ASSERT(evt != NULL, " event sync failed, event is null");
    event *pevent = (event *)evt;
    pevent->wait();
}

void impl_common_uninit(void *pq) {
    IMPL_ASSERT(pq != NULL, " queue uninit failed, queue is null");
    delete (queue *)pq;
}
