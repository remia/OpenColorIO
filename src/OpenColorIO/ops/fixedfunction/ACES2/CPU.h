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
constexpr float smin(float a, float b, float s)
{
    const float h = std::max(s - std::abs(a - b), 0.f) / s;
    return std::min(a, b) - h * h * h * s * (1.f / 6.f);
}

float wrap_to_360(float hue)
{
    float y = std::fmod(hue, 360.f);
    if ( y < 0.f)
    {
        y = y + 360.f;
    }
    return y;
}

constexpr float radians_to_degrees(float radians)
{
    return radians * 180.f / M_PI;
}

constexpr float degrees_to_radians(float degrees)
{
    return degrees / 180.f * M_PI;
}


//////////////////////////////////////////////////////////////////////////
// Gamut lookup table utilities
//////////////////////////////////////////////////////////////////////////

int hue_position_in_uniform_table(float hue, int table_size) 
{
    const float wrapped_hue = wrap_to_360( hue);
    const int result = (wrapped_hue / 360.f * table_size);
    return result;
}

constexpr int next_position_in_table(int entry, int table_size)
{
    const int result = (entry + 1) % table_size;
    return result;
}

constexpr float base_hue_for_position(int i_lo, int table_size) 
{
    const float result = i_lo * 360. / table_size;
    return result;
}

constexpr f2 cuspFromTable(float h, const Table3D &gt)
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

constexpr float reachMFromTable(float h, const Table1D &gt)
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
constexpr float panlrc_forward(float v, float F_L)
{
    // Can we have negatives here?
    float F_L_v = pow(F_L * std::abs(v) / reference_luminance, 0.42f);
    float c = (400.f * std::copysign(1., v) * F_L_v) / (27.13f + F_L_v);
    return c;
}

constexpr float panlrc_inverse(float v, float F_L)
{
    float p = std::copysign(1., v) * reference_luminance / F_L * pow((27.13f * std::abs(v) / (400.f - std::abs(v))), 1.f / 0.42f);
    return p;
}

constexpr float Hellwig_J_to_Y(float J, const JMhParams &params)
{
    const float A = params.A_w_J * pow(std::abs(J) / reference_luminance, 1.f / (surround[1] * params.z));
    return std::copysign(1.f, J) * reference_luminance / params.F_L * pow((27.13f * A) / (400.f - A), 1.f / 0.42f);
}

constexpr float Y_to_Hellwig_J(float Y, const JMhParams &params)
{
    float F_L_Y = pow(params.F_L * std::abs(Y) / 100., 0.42);
    return std::copysign(1.f, Y) * reference_luminance * pow(((400.f * F_L_Y) / (27.13f + F_L_Y)) / params.A_w_J, surround[1] * params.z);
}

constexpr f3 XYZ_to_Hellwig2022_JMh(const f3 &XYZ, const JMhParams &params)
{
    // Step 1 - Converting CIE XYZ tristimulus values to sharpened RGB values
    const f3 RGB = mult_f3_f33(XYZ, params.MATRIX_16);

    // Step 2
    const f3 RGB_c = {
        params.D_RGB[0] * RGB[0],
        params.D_RGB[1] * RGB[1],
        params.D_RGB[2] * RGB[2]
    };

    // Step 3 - apply  forward post-adaptation non-linear response compression
    const f3 RGB_a = {
        panlrc_forward(RGB_c[0], params.F_L),
        panlrc_forward(RGB_c[1], params.F_L),
        panlrc_forward(RGB_c[2], params.F_L)
    };

    // Converting to preliminary cartesian coordinates
    // TODO: Can be a matrix, then use inverse in the reverse function
    // to simplify code. Or just use the inverse formula.
    // Can also hardcode ra and ba directly to remove need for panlrcm
    const float A = ra * RGB_a[0] + RGB_a[1] + ba * RGB_a[2];
    const float a = RGB_a[0] - 12.f * RGB_a[1] / 11.f + RGB_a[2] / 11.f;
    const float b = (RGB_a[0] + RGB_a[1] - 2.f * RGB_a[2]) / 9.f;

    // Computing the hue angle math h
    const float hr = atan2(b, a);
    const float h = wrap_to_360(radians_to_degrees(hr));

    // Computing the correlate of lightness, J
    const float J = reference_luminance * pow( A / params.A_w, surround[1] * params.z);

    // Computing the correlate of colourfulness, M
    const float M = J == 0.f ? 0.f : 43.f * surround[2] * sqrt(a * a + b * b);

    return {J, M, h};
}

constexpr f3 Hellwig2022_JMh_to_XYZ(const f3 &JMh, const JMhParams &params)
{
    const float J = JMh[0];
    const float M = JMh[1];
    const float h = JMh[2];

    const float hr = degrees_to_radians(h);

    // Computing achromatic respons A for the stimulus
    const float A = params.A_w * pow(J / reference_luminance, 1. / (surround[1] * params.z));

    // Computing opponent colour dimensions a and b
    const float scale = M / (43.f * surround[2]);
    const float a = scale * cos(hr);
    const float b = scale * sin(hr);

    // Step 4 - Applying post-adaptation non-linear response compression matrix
    const f3 vec = {A, a, b};
    const f3 RGB_a = mult_f_f3( 1.0/1403.0, mult_f3_f33(vec, params.panlrcm));

    // Step 5 - Applying the inverse post-adaptation non-linear respnose compression
    const f3 RGB_c = {
        panlrc_inverse(RGB_a[0], params.F_L),
        panlrc_inverse(RGB_a[1], params.F_L),
        panlrc_inverse(RGB_a[2], params.F_L)
    };

    // Step 6
    const f3 RGB = {
        RGB_c[0] / params.D_RGB[0],
        RGB_c[1] / params.D_RGB[1],
        RGB_c[2] / params.D_RGB[2]
    };

    // Step 7
    f3 XYZ = mult_f3_f33(RGB, params.MATRIX_16_INV);

    return XYZ;
}

constexpr f3 RGB_to_JMh(const f3 &RGB, const m33f &RGB_TO_XYZ, float luminance, const JMhParams &params)
{
    const f3 luminanceRGB = mult_f_f3(luminance, RGB);
    const f3 XYZ = mult_f3_f33(luminanceRGB, RGB_TO_XYZ);
    const f3 JMh = XYZ_to_Hellwig2022_JMh(XYZ, params);
    return JMh;
}

constexpr f3 JMh_to_RGB(const f3 &JMh, const m33f &XYZ_TO_RGB, float luminance, const JMhParams &params)
{
    const f3 luminanceXYZ = Hellwig2022_JMh_to_XYZ(JMh, params);
    const f3 luminanceRGB = mult_f3_f33(luminanceXYZ, XYZ_TO_RGB);
    const f3 RGB = mult_f_f3(1.f / luminance, luminanceRGB);
    return RGB;
}


//////////////////////////////////////////////////////////////////////////
// Tonescale and Chroma compression
//////////////////////////////////////////////////////////////////////////

constexpr float tonescale_fwd(float x, const OutputTransformParams &params)
{
    float f = params.m_2 * pow(std::max(0.f, x) / (x + params.s_2), params.g);
    float h = std::max(0.f, f * f / (f + params.t_1));

    return h * params.n_r;
}

constexpr float tonescale_inv(float Y, const OutputTransformParams &params)
{
    float Z = std::max(0.f, std::min(params.peakLuminance / (params.u_2 * params.n_r), Y));
    float h = (Z + sqrt(Z * (4.f * params.t_1 + Z))) / 2.f;
    float f = params.s_2 / (pow((params.m_2 / h), (1.f / params.g)) - 1.f);

    return f;
}

constexpr float toe( float x, float limit, float k1_in, float k2_in, bool invert = false)
{
    if (x > limit)
    {
        return x;
    }

    const float k2 = std::max(k2_in, 0.001f);
    const float k1 = sqrt(k1_in * k1_in + k2 * k2);
    const float k3 = (limit + k1) / (limit + k2);

    if (invert)
    {
        return (x * x + k1 * x) / (k3 * (x + k2));
    }
    else
    {
        return 0.5f * (k3 * x - k1 + sqrt((k3 * x - k1) * (k3 * x - k1) + 4.f * k2 * k3 * x));
    }
}

constexpr float chromaCompressNorm(float h, float chromaCompressScale)
{
    const float hr = degrees_to_radians(h);
    const float a = cos(hr);
    const float b = sin(hr);
    const float cos_hr2 = a * a - b * b;
    const float sin_hr2 = 2.0f * a * b;
    const float cos_hr3 = 4.0f * a * a * a - 3.0f * a;
    const float sin_hr3 = 3.0f * b - 4.0f * b * b * b;

    const float M = 11.34072f * a +
              16.46899f * cos_hr2 +
               7.88380f * cos_hr3 +
              14.66441f * b +
              -6.37224f * sin_hr2 +
               9.19364f * sin_hr3 +
              77.12896f;

    return M * chromaCompressScale;
}

constexpr float chromaCompression_fwd(const f3 &JMh, float origJ, const OutputTransformParams &params)
{
    const float J = JMh[0];
    float M = JMh[1];
    const float h = JMh[2];

    if (M == 0.0) {
        return M;
    }

    const float nJ = J / params.limit_J_max;
    const float snJ = std::max(0.f, 1.f - nJ);
#ifdef CHROMA_CURVE
    const float Mnorm = chromaCompressNorm(h, params.chromaCompressScale);
#else
    const float Mnorm = cuspFromTable(h, params.reach_gamut_table)[1];
#endif
    const float limit = pow(nJ, params.model_gamma) * reachMFromTable(h, params.reach_m_table) / Mnorm;

    M = M * pow(J / origJ, params.model_gamma);
    M = M / Mnorm;
    M = limit - toe(limit - M, limit - 0.001f, snJ * params.sat, sqrt(nJ * nJ + params.sat_thr), false);
    M = toe(M, limit, nJ * params.compr, snJ, false);

    M = M * Mnorm;

    return M;
}

constexpr float chromaCompression_inv(const f3 &JMh, float origJ, const OutputTransformParams &params)
{
    const float J = JMh[0];
    float M = JMh[1];
    const float h = JMh[2];

    if (M == 0.0) {
        return M;
    }

    const float nJ = J / params.limit_J_max;
    const float snJ = std::max(0.f, 1.f - nJ);
#ifdef CHROMA_CURVE
    const float Mnorm = chromaCompressNorm(h, params.chromaCompressScale);
#else
    const float Mnorm = cuspFromTable(h, params.reach_gamut_table)[1];
#endif
    const float limit = pow(nJ, params.model_gamma) * reachMFromTable(h, params.reach_m_table) / Mnorm;

    M = M / Mnorm;
    M = toe(M, limit, nJ * params.compr, snJ, true);
    M = limit - toe(limit - M, limit - 0.001f, snJ * params.sat, sqrt(nJ * nJ + params.sat_thr), true);
    M = M * Mnorm;
    M = M * pow( J / origJ, -params.model_gamma);

    return M;
}

f3 tonemapAndCompress_fwd(const f3 &inputJMh, const OutputTransformParams &params)
{
    const float linear = Hellwig_J_to_Y(inputJMh[0], params.inputJMhParams) / reference_luminance;
    // print_v("Tonemap Y", linear);
    const float luminanceTS = tonescale_fwd(linear, params);
    const float tonemappedJ = Y_to_Hellwig_J(luminanceTS, params.inputJMhParams);
    const f3 tonemappedJMh = {tonemappedJ, inputJMh[1], inputJMh[2]};

    const f3 outputJMh = tonemappedJMh;
    const float M = chromaCompression_fwd(outputJMh, inputJMh[0], params);

    return {outputJMh[0], M, outputJMh[2]};
}

constexpr f3 tonemapAndCompress_inv(const f3 &JMh, const OutputTransformParams &params)
{
    float luminance = Hellwig_J_to_Y(JMh[0], params.inputJMhParams);
    float linear = tonescale_inv(luminance / reference_luminance, params);

    const f3 tonemappedJMh = JMh;
    const float untonemappedJ = Y_to_Hellwig_J(linear * reference_luminance, params.inputJMhParams);
    const f3 untonemappedColorJMh = {untonemappedJ, tonemappedJMh[1], tonemappedJMh[2]};

    const float M = chromaCompression_inv(tonemappedJMh, untonemappedColorJMh[0], params);

    return {untonemappedColorJMh[0], M, untonemappedColorJMh[2]};
}


//////////////////////////////////////////////////////////////////////////
// Gamut Compression
//////////////////////////////////////////////////////////////////////////

constexpr float get_focus_gain(float J, float cuspJ, float limit_J_max)
{
    const float thr = lerpf(cuspJ, limit_J_max, focus_gain_blend);

    if (J > thr)
    {
        // Approximate inverse required above threshold
        float gain = (limit_J_max - thr) / std::max(0.0001f, (limit_J_max - std::min(limit_J_max, J)));
        return pow(log10(gain), 1. / focus_adjust_gain) + 1.f;
    }
    else
    {
        // Analytic inverse possible below cusp
        return 1.f;
    }
}

constexpr float solve_J_intersect(float J, float M, float focusJ, float maxJ, float slope_gain)
{
    const float a = M / (focusJ * slope_gain);
    float b = 0.f;
    float c = 0.f;
    float intersectJ = 0.f;

    if (J < focusJ)
    {
        b = 1.f - M / slope_gain;
    }
    else
    {
        b = - (1.f + M / slope_gain + maxJ * M / (focusJ * slope_gain));
    }

    if (J < focusJ)
    {
        c = -J;
    }
    else
    {
        c = maxJ * M / slope_gain + J;
    }

    const float root = sqrt(b * b - 4.f * a * c);

    if (J < focusJ)
    {
        intersectJ = 2.f * c / (-b - root);
    }
    else
    {
        intersectJ = 2.f * c / (-b + root);
    }

    return intersectJ;
}

constexpr f3 find_gamut_boundary_intersection(const f3 &JMh_s, const f2 &JM_cusp_in, float J_focus, float J_max, float slope_gain, float gamma_top, float gamma_bottom)
{
    float slope = 0.f;

    const float s = std::max(0.000001f, smooth_cusps);
    const f2 JM_cusp = {
        JM_cusp_in[0],
        JM_cusp_in[1] * (1.f + smooth_m * s)
    };

    const float J_intersect_source = solve_J_intersect(JMh_s[0], JMh_s[1], J_focus, J_max, slope_gain);
    const float J_intersect_cusp = solve_J_intersect(JM_cusp[0], JM_cusp[1], J_focus, J_max, slope_gain);

    if (J_intersect_source < J_focus)
    {
        slope = J_intersect_source * (J_intersect_source - J_focus) / (J_focus * slope_gain);
    }
    else
    {
        slope = (J_max - J_intersect_source) * (J_intersect_source - J_focus) / (J_focus * slope_gain);
    }

    const float M_boundary_lower = J_intersect_cusp * pow(J_intersect_source / J_intersect_cusp, 1. / gamma_bottom) / (JM_cusp[0] / JM_cusp[1] - slope);
    const float M_boundary_upper = JM_cusp[1] * (J_max - J_intersect_cusp) * pow((J_max - J_intersect_source) / (J_max - J_intersect_cusp), 1. / gamma_top) / (slope * JM_cusp[1] + J_max - JM_cusp[0]);
    const float M_boundary = JM_cusp[1] * smin(M_boundary_lower / JM_cusp[1], M_boundary_upper / JM_cusp[1], s);
    const float J_boundary = J_intersect_source + slope * M_boundary;

    return {J_boundary, M_boundary, J_intersect_source};
}

f3 get_reach_boundary(float J, float M, float h, const f2 &JMcusp, const OutputTransformParams &params)
{
    float limit_J_max = params.limit_J_max;
    float mid_J = params.mid_J;
    float model_gamma = params.model_gamma;
    float focus_dist = params.focus_dist;

    const float reachMaxM = reachMFromTable(h, params.reach_m_table);

    const float focusJ = lerpf(JMcusp[0], mid_J, std::min(1.f, cusp_mid_blend - (JMcusp[0] / limit_J_max)));
    const float slope_gain = limit_J_max * focus_dist * get_focus_gain(J, JMcusp[0], limit_J_max);

    const float intersectJ = solve_J_intersect(J, M, focusJ, limit_J_max, slope_gain);

    float slope;
    if (intersectJ < focusJ)
    {
        slope = intersectJ * (intersectJ - focusJ) / (focusJ * slope_gain);
    }
    else
    {
        slope = (limit_J_max - intersectJ) * (intersectJ - focusJ) / (focusJ * slope_gain);
    }

    const float boundary = limit_J_max * pow(intersectJ / limit_J_max, model_gamma) * reachMaxM / (limit_J_max - slope * reachMaxM);
    return {J, boundary, h};
}

float hue_dependent_upper_hull_gamma(float h, const Table1D &gt)
{
    const int i_lo = hue_position_in_uniform_table(h, gt.size) + gt.base_index;
    const int i_hi = next_position_in_table(i_lo, gt.size) ;

    const float base_hue = base_hue_for_position(i_lo - gt.base_index, gt.size);

    const float t = wrap_to_360(h) - base_hue;

    return lerpf(gt.table[i_lo], gt.table[i_hi], t);
}


float compression_function(float x, float threshold, float limit, bool invert=false)
{
    const float s = (limit - threshold) / (((limit-threshold) / (1.f - threshold)) - 1.f);
    const float g = (x - threshold) / s;

    float xCompressed;

    if (invert)
    {
        if (x < threshold || g < 1.f)
        {
            xCompressed = x;
        }
        else
        {
            xCompressed = threshold + s * -g / (g - 1.f);
        }
    }
    else
    {
        if (x < threshold || limit < 1.0001f)
        {
            xCompressed = x;
        }
        else
        {
            xCompressed = threshold + s * g / (1.f + g);
        }
    }

    return xCompressed;
}

f3 compressGamut(const f3 &JMh, const OutputTransformParams& params, float Jx, bool invert=false)
{
    float limit_J_max = params.limit_J_max;
    float mid_J = params.mid_J;
    float focus_dist = params.focus_dist;

    const f2 project_from = {JMh[0], JMh[1]};
    const f2 JMcusp = cuspFromTable(JMh[2], params.gamut_cusp_table);

    if (JMh[1] < 0.0001f || JMh[0] > limit_J_max)
    {
        return {JMh[0], 0.f, JMh[2]};
    }

    const float focusJ = lerpf(JMcusp[0], mid_J, std::min(1.f, cusp_mid_blend - (JMcusp[0] / limit_J_max)));
    const float slope_gain = limit_J_max * focus_dist * get_focus_gain(Jx, JMcusp[0], limit_J_max);

    const float gamma_top = hue_dependent_upper_hull_gamma(JMh[2], params.upper_hull_gamma_table);
    const float gamma_bottom = lower_hull_gamma;

    const f3 boundaryReturn = find_gamut_boundary_intersection(JMh, JMcusp, focusJ, limit_J_max, slope_gain, gamma_top, gamma_bottom);
    const f2 JMboundary = {boundaryReturn[0], boundaryReturn[1]};
    const f2 project_to = {boundaryReturn[2], 0.f};

    const f3 reachBoundary = get_reach_boundary(JMboundary[0], JMboundary[1], JMh[2], JMcusp, params);

    const float difference = std::max(1.0001f, reachBoundary[1] / JMboundary[1]);
    const float threshold = std::max(compression_threshold, 1.f / difference);

    float v = project_from[1] / JMboundary[1];
    v = compression_function(v, threshold, difference, invert);

    const f2 JMcompressed {
        project_to[0] + v * (JMboundary[0] - project_to[0]),
        project_to[1] + v * (JMboundary[1] - project_to[1])
    };

    return {JMcompressed[0], JMcompressed[1], JMh[2]};
}

f3 gamutMap_fwd(const f3 &JMh, const OutputTransformParams &params)
{
    return compressGamut(JMh, params, JMh[0], false);
}

f3 gamutMap_inv(const f3 &JMh, const OutputTransformParams &params)
{
    const f2 JMcusp = cuspFromTable( JMh[2], params.gamut_cusp_table);
    float Jx = JMh[0];

    // Analytic inverse below threshold
    if (Jx <= lerpf(JMcusp[0], params.limit_J_max, focus_gain_blend))
    {
        return compressGamut(JMh, params, Jx, true);
    }

    // Approximation above threshold
    Jx = compressGamut(JMh, params, Jx, true)[0];
    return compressGamut(JMh, params, Jx, true);
}


//////////////////////////////////////////////////////////////////////////
// Full transform
//////////////////////////////////////////////////////////////////////////

f3 outputTransform_fwd(const f3 &RGB, const OutputTransformParams &params)
{
    f3 XYZ = mult_f3_f33(RGB, params.INPUT_RGB_TO_XYZ);

    // Clamp to AP1
    if (params.clamp_ap1)
    {
        const f3 AP1 = mult_f3_f33(XYZ, params.XYZ_TO_AP1);
        const f3 AP1_clamped = clamp_f3(AP1, 0.f, std::numeric_limits<float>::max());
        XYZ = mult_f3_f33(AP1_clamped, params.AP1_TO_XYZ);
    }

    const f3 JMh = RGB_to_JMh(XYZ, Identity_M33, reference_luminance, params.inputJMhParams);
    const f3 tonemappedJMh = tonemapAndCompress_fwd(JMh, params);
    const f3 compressedJMh = gamutMap_fwd(tonemappedJMh, params);
    const f3 limitRGB = JMh_to_RGB(compressedJMh, params.LIMIT_XYZ_TO_RGB, reference_luminance, params.limitJMhParams);

    f3 outputRGB = mult_f3_f33(limitRGB, params.LIMIT_TO_OUTPUT);

    // White point scaling
    if (params.white_scale) {
        outputRGB = mult_f_f3(params.white_scale, outputRGB);
    }

    return outputRGB;
}

f3 outputTransform_inv(const f3 &RGB, const OutputTransformParams &params)
{
    f3 outputRGB = RGB;

    // White point scaling
    if (params.white_scale) {
        outputRGB = mult_f_f3(1. / params.white_scale, outputRGB);
    }

    const f3 compressedJMh = RGB_to_JMh(RGB, params.LIMIT_RGB_TO_XYZ, params.peakLuminance, params.limitJMhParams);
    const f3 tonemappedJMh = gamutMap_inv(compressedJMh, params);
    const f3 JMh = tonemapAndCompress_inv(tonemappedJMh, params);
    const f3 inputRGB = JMh_to_RGB(JMh, params.INPUT_XYZ_TO_RGB, params.peakLuminance, params.inputJMhParams);

    return inputRGB;
}

} // namespace ACES2

} // namespace OCIO_NAMESPACE

#endif