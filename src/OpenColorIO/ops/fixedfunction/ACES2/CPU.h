// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#ifndef INCLUDED_OCIO_ACES2HELPERS_H
#define INCLUDED_OCIO_ACES2HELPERS_H

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <iostream>

#include "Common.h"


namespace OCIO_NAMESPACE
{

namespace ACES2
{


#define CHROMA_CURVE


//////////////////////////////////////////////////////////////////////////
// Utilities
//////////////////////////////////////////////////////////////////////////

// smooth minimum of a and b
inline float smin(float a, float b, float s)
{
    const float h = std::max(s - std::abs(a - b), 0.f) / s;
    return std::min(a, b) - h * h * h * s * (1.f / 6.f);
}

inline float wrap_to_360(float hue)
{
    float y = std::fmod(hue, 360.f);
    if ( y < 0.f)
    {
        y = y + 360.f;
    }
    return y;
}

inline float radians_to_degrees(float radians)
{
    return radians * 180.f / M_PI;
}

inline float degrees_to_radians(float degrees)
{
    return degrees / 180.f * M_PI;
}


//////////////////////////////////////////////////////////////////////////
// Gamut lookup table utilities
//////////////////////////////////////////////////////////////////////////

inline int hue_position_in_uniform_table(float hue, int table_size) 
{
    const float wrapped_hue = wrap_to_360( hue);
    const int result = (wrapped_hue / 360.f * table_size);
    return result;
}

inline int next_position_in_table(int entry, int table_size)
{
    const int result = (entry + 1) % table_size;
    return result;
}

inline float base_hue_for_position(int i_lo, int table_size) 
{
    const float result = i_lo * 360. / table_size;
    return result;
}

inline f2 cuspFromTable(float h, const Table3D &gt)
{
    float lo[3]{};
    float hi[3]{};

    int low_i = 0;
    int high_i = gt.base_index + gt.size; // allowed as we have an extra entry in the table
    int i = hue_position_in_uniform_table(h, gt.size) + gt.base_index;

    while (low_i + 1 < high_i)
    {
        if (h > gt.table[i][2])
        {
            low_i = i;
        }
        else
        {
            high_i = i;
        }
        i = (low_i + high_i) / 2.f;
    }

    lo[0] = gt.table[high_i-1][0];
    lo[1] = gt.table[high_i-1][1];
    lo[2] = gt.table[high_i-1][2];

    hi[0] = gt.table[high_i][0];
    hi[1] = gt.table[high_i][1];
    hi[2] = gt.table[high_i][2];
    
    float t = (h - lo[2]) / (hi[2] - lo[2]);
    float cuspJ = lerpf( lo[0], hi[0], t);
    float cuspM = lerpf( lo[1], hi[1], t);
    
    return { cuspJ, cuspM };
}

inline float reachMFromTable(float h, const Table1D &gt)
{
    int i_lo = hue_position_in_uniform_table( h, gt.size);
    int i_hi = next_position_in_table( i_lo, gt.size);
    
    const float t = (h - i_lo) / (i_hi - i_lo);

    return lerpf(gt.table[i_lo], gt.table[i_hi], t);
}


//////////////////////////////////////////////////////////////////////////
// RGB to and from JMh
//////////////////////////////////////////////////////////////////////////

// Post adaptation non linear response compression
float panlrc_forward(float v, float F_L);

float Hellwig_J_to_Y(float J, const JMhParams &params);

float Y_to_Hellwig_J(float Y, const JMhParams &params);

f3 XYZ_to_Hellwig2022_JMh(const f3 &XYZ, const JMhParams &params);

f3 Hellwig2022_JMh_to_XYZ(const f3 &JMh, const JMhParams &params);

f3 RGB_to_JMh(const f3 &RGB, const m33f &RGB_TO_XYZ, float luminance, const JMhParams &params);

f3 JMh_to_RGB(const f3 &JMh, const m33f &XYZ_TO_RGB, float luminance, const JMhParams &params);


//////////////////////////////////////////////////////////////////////////
// Tonescale and Chroma compression
//////////////////////////////////////////////////////////////////////////

float tonescale_fwd(float x, const OutputTransformParams &params);

float tonescale_inv(float Y, const OutputTransformParams &params);

float toe( float x, float limit, float k1_in, float k2_in, bool invert = false);

float chromaCompressNorm(float h, float chromaCompressScale);

float chromaCompression_fwd(const f3 &JMh, float origJ, const OutputTransformParams &params);

float chromaCompression_inv(const f3 &JMh, float origJ, const OutputTransformParams &params);

f3 tonemapAndCompress_fwd(const f3 &inputJMh, const OutputTransformParams &params);

f3 tonemapAndCompress_inv(const f3 &JMh, const OutputTransformParams &params);


//////////////////////////////////////////////////////////////////////////
// Gamut Compression
//////////////////////////////////////////////////////////////////////////

float get_focus_gain(float J, float cuspJ, float limit_J_max);

float solve_J_intersect(float J, float M, float focusJ, float maxJ, float slope_gain);

f3 find_gamut_boundary_intersection(const f3 &JMh_s, const f2 &JM_cusp_in, float J_focus, float J_max, float slope_gain, float gamma_top, float gamma_bottom);

f3 get_reach_boundary(float J, float M, float h, const f2 &JMcusp, const OutputTransformParams &params);

float hue_dependent_upper_hull_gamma(float h, const Table1D &gt);

float compression_function(float x, float threshold, float limit, bool invert=false);

f3 compressGamut(const f3 &JMh, const OutputTransformParams& params, float Jx, bool invert=false);

f3 gamutMap_fwd(const f3 &JMh, const OutputTransformParams &params);

f3 gamutMap_inv(const f3 &JMh, const OutputTransformParams &params);


//////////////////////////////////////////////////////////////////////////
// Full transform
//////////////////////////////////////////////////////////////////////////

f3 outputTransform_fwd(const f3 &RGB, const OutputTransformParams &params);

f3 outputTransform_inv(const f3 &RGB, const OutputTransformParams &params);

} // namespace ACES2

} // namespace OCIO_NAMESPACE

#endif