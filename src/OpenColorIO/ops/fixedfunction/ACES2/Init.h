// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#ifndef INCLUDED_OCIO_ACES2INIT_H
#define INCLUDED_OCIO_ACES2INIT_H

#include "Common.h"

namespace OCIO_NAMESPACE
{

namespace ACES2
{

m33f generate_panlrcm(float ra, float ba);

Table3D make_gamut_table(const Primaries &P, float peakLuminance);

bool any_below_zero(const f3 &rgb);

Table1D make_reach_m_table(const Primaries &P, float peakLuminance);

bool outside_hull(const f3 &rgb);

Table1D make_upper_hull_gamma(
    const Table3D &gamutCuspTable,
    float peakLuminance,
    float limit_J_max,
    float mid_J,
    float focus_dist,
    const m33f &limit_XYZ_to_RGB,
    const JMhParams &limitJMhParams);

JMhParams init_JMhParams(const Primaries &P);

ToneScaleParams init_ToneScaleParams(float peakLuminance);

ChromaCompressParams init_ChromaCompressParams(float peakLuminance);

GamutCompressParams init_GamutCompressParams(float peakLuminance, const Primaries &P);

OutputTransformParams init_OutputTransformParams(
    float peakLuminance,
    Primaries limitingPrimaries,
    Primaries encodingPrimaries,
    bool clampAP1);

// TODO: Use map to cache results for custom parameters
// TODO: Encoding primaries needed for creative white point handling?
OutputTransformParams get_transform_params(
    float peakLuminance,
    const Primaries &limitingPrimaries,
    const Primaries &encodingPrimaries,
    bool ap1Clamp);

} // namespace ACES2

} // OCIO namespace

#endif
