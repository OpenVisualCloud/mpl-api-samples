/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef __IMPL_MIXER_HPP__
#define __IMPL_MIXER_HPP__
#include "impl_api.h"
#include "impl_common.hpp"

struct impl_mixer_context {
    /** IMPL Mixer function index */
    int mixer_func_index[IMPL_MIXER_MAX_FIELDS];
};

IMPL_STATUS composition_i420(struct impl_mixer_params *pmixer, struct impl_mixer_field_params *pfield,
                             struct impl_mixer_field_params *pfield0, void *dep_evt);

IMPL_STATUS composition_v210(struct impl_mixer_params *pmixer, struct impl_mixer_field_params *pfield,
                             struct impl_mixer_field_params *pfield0, void *dep_evt);

IMPL_STATUS alphablending_alphasurf_i420(struct impl_mixer_params *pmixer, struct impl_mixer_field_params *pfield,
                                         struct impl_mixer_field_params *pfield0, void *dep_evt);

IMPL_STATUS alphablending_staticalpha_i420(struct impl_mixer_params *pmixer, struct impl_mixer_field_params *pfield,
                                           struct impl_mixer_field_params *pfield0, void *dep_evt);

IMPL_STATUS alphablending_alphasurf_v210(struct impl_mixer_params *pmixer, struct impl_mixer_field_params *pfield,
                                         struct impl_mixer_field_params *pfield0, void *dep_evt);

IMPL_STATUS alphablending_staticalpha_v210(struct impl_mixer_params *pmixer, struct impl_mixer_field_params *pfield,
                                           struct impl_mixer_field_params *pfield0, void *dep_evt);

IMPL_STATUS alphablending_alphasurf_yuv420p10le(struct impl_mixer_params *pmixer,
                                                struct impl_mixer_field_params *pfield,
                                                struct impl_mixer_field_params *pfield0, void *dep_evt);

IMPL_STATUS alphablending_staticalpha_yuv420p10le(struct impl_mixer_params *pmixer,
                                                  struct impl_mixer_field_params *pfield,
                                                  struct impl_mixer_field_params *pfield0, void *dep_evt);

#endif // __IMPL_MIXER_HPP__
