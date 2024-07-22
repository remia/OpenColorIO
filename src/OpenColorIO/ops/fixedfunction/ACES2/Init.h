// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#ifndef INCLUDED_OCIO_ACES2INIT_H
#define INCLUDED_OCIO_ACES2INIT_H

#include "Common.h"
#include "CPU.h"


namespace OCIO_NAMESPACE
{

namespace ACES2
{

inline m33f generate_panlrcm(float ra, float ba)
{
    m33f panlrcm_data = {
        ra,   1.f      ,  1.f/9.f,
        1.f, -12.f/11.f,  1.f/9.f,
        ba,   1.f/11.f , -2.f/9.f
    };
    const m33f panlrcm = invert_f33(panlrcm_data);

    // Normalize columns so that first row is 460
    const float s1 = 460.f / panlrcm[0];
    const float s2 = 460.f / panlrcm[1];
    const float s3 = 460.f / panlrcm[2];

    const m33f scaled_panlrcm = {
        panlrcm[0] * s1, panlrcm[1] * s2, panlrcm[2] * s3,
        panlrcm[3] * s1, panlrcm[4] * s2, panlrcm[5] * s3,
        panlrcm[6] * s1, panlrcm[7] * s2, panlrcm[8] * s3
    };

    return transpose_f33(scaled_panlrcm);
}

inline JMhParams initJMhParams(const Primaries &P)
{
    JMhParams p;

    const m33f MATRIX_16 = XYZtoRGB_f33(CAM16::primaries);
    const m33f MATRIX_16_INV = RGBtoXYZ_f33(CAM16::primaries);

    const m33f RGB_to_XYZ = RGBtoXYZ_f33(P);
    const f3 XYZ_w = mult_f3_f33(f3_from_f(reference_luminance), RGB_to_XYZ);

    const float Y_W = XYZ_w[1];

    const f3 RGB_w = mult_f3_f33(XYZ_w, MATRIX_16);

    // Viewing condition dependent parameters
    const float K = 1.f / (5.f * L_A + 1.f);
    const float K4 = pow(K, 4.f);
    const float N = Y_b / Y_W;
    const float F_L = 0.2f * K4 * (5.f * L_A) + 0.1f * pow((1.f - K4), 2.f) * pow(5.f * L_A, 1.f/3.f);
    const float z = 1.48f + sqrt(N);

    const f3 D_RGB = {
        Y_W / RGB_w[0],
        Y_W / RGB_w[1],
        Y_W / RGB_w[2]
    };

    const f3 RGB_WC {
        D_RGB[0] * RGB_w[0],
        D_RGB[1] * RGB_w[1],
        D_RGB[2] * RGB_w[2]
    };

    const f3 RGB_AW = {
        panlrc_forward(RGB_WC[0], F_L),
        panlrc_forward(RGB_WC[1], F_L),
        panlrc_forward(RGB_WC[2], F_L)
    };

    const float A_w = ra * RGB_AW[0] + RGB_AW[1] + ba * RGB_AW[2];

    const float F_L_W = pow(F_L, 0.42f);
    const float A_w_J   = (400. * F_L_W) / (27.13 + F_L_W);

    p.XYZ_w = XYZ_w;
    p.F_L = F_L;
    p.z = z;
    p.D_RGB = D_RGB;
    p.A_w = A_w;
    p.A_w_J = A_w_J;

    p.panlrcm = generate_panlrcm(ra, ba);
    p.MATRIX_16 = MATRIX_16;
    p.MATRIX_16_INV = MATRIX_16_INV;

    return p;
}

inline constexpr Table3D make_gamut_table(const Primaries &P, float peakLuminance)
{
    // TODO: Don't compute here?
    const m33f RGB_TO_XYZ = RGBtoXYZ_f33(P);
    const JMhParams params = initJMhParams(P);

    Table3D gamutCuspTableUnsorted{};
    for (int i = 0; i < gamutCuspTableUnsorted.size; i++)
    {
        const float hNorm = (float) i / gamutCuspTableUnsorted.size;
        const f3 HSV = {hNorm, 1., 1.};
        const f3 RGB = HSV_to_RGB(HSV);
        const f3 JMh = RGB_to_JMh(RGB, RGB_TO_XYZ, peakLuminance, params);

        gamutCuspTableUnsorted.table[i][0] = JMh[0];
        gamutCuspTableUnsorted.table[i][1] = JMh[1];
        gamutCuspTableUnsorted.table[i][2] = JMh[2];
    }

    int minhIndex = 0;
    for (int i = 0; i < gamutCuspTableUnsorted.size; i++)
    {
        if ( gamutCuspTableUnsorted.table[i][2] < gamutCuspTableUnsorted.table[minhIndex][2])
            minhIndex = i;
    }

    Table3D gamutCuspTable{};
    for (int i = 0; i < gamutCuspTableUnsorted.size; i++)
    {
        gamutCuspTable.table[i + gamutCuspTable.base_index][0] = gamutCuspTableUnsorted.table[(minhIndex+i) % gamutCuspTableUnsorted.size][0];
        gamutCuspTable.table[i + gamutCuspTable.base_index][1] = gamutCuspTableUnsorted.table[(minhIndex+i) % gamutCuspTableUnsorted.size][1];
        gamutCuspTable.table[i + gamutCuspTable.base_index][2] = gamutCuspTableUnsorted.table[(minhIndex+i) % gamutCuspTableUnsorted.size][2];
    }

    // Copy last populated entry to first empty spot
    gamutCuspTable.table[0][0] = gamutCuspTable.table[gamutCuspTable.base_index + gamutCuspTable.size-1][0];
    gamutCuspTable.table[0][1] = gamutCuspTable.table[gamutCuspTable.base_index + gamutCuspTable.size-1][1];
    gamutCuspTable.table[0][2] = gamutCuspTable.table[gamutCuspTable.base_index + gamutCuspTable.size-1][2];

    // Copy first populated entry to last empty spot
    gamutCuspTable.table[gamutCuspTable.base_index + gamutCuspTable.size][0] = gamutCuspTable.table[gamutCuspTable.base_index][0];
    gamutCuspTable.table[gamutCuspTable.base_index + gamutCuspTable.size][1] = gamutCuspTable.table[gamutCuspTable.base_index][1];
    gamutCuspTable.table[gamutCuspTable.base_index + gamutCuspTable.size][2] = gamutCuspTable.table[gamutCuspTable.base_index][2];

    // Wrap the hues, to maintain monotonicity. These entries will fall outside [0.0, 360.0]
    gamutCuspTable.table[0][2] = gamutCuspTable.table[0][2] - 360.0;
    gamutCuspTable.table[gamutCuspTable.size+1][2] = gamutCuspTable.table[gamutCuspTable.size+1][2] + 360.0;

    return gamutCuspTable;
}

inline constexpr bool any_below_zero(const f3 &rgb)
{
    return (rgb[0] < 0. || rgb[1] < 0. || rgb[2] < 0.);
}

inline constexpr Table1D make_reach_m_table(const Primaries &P, float peakLuminance)
{
    // TODO: Don't compute here?
    const m33f XYZ_TO_RGB = XYZtoRGB_f33(P);
    const JMhParams params = initJMhParams(P);
    const float limit_J_max = Y_to_Hellwig_J(peakLuminance, params);

    Table1D gamutReachTable{};

    for (int i = 0; i < gamutReachTable.size; i++) {
        const float hue = i;

        const float search_range = 50.f;
        float low = 0.;
        float high = low + search_range;
        bool outside = false;

        while ((outside != true) & (high < 1300.f))
        {
            const f3 searchJMh = {limit_J_max, high, hue};
            const f3 newLimitRGB = JMh_to_RGB(searchJMh, XYZ_TO_RGB, peakLuminance, params);
            outside = any_below_zero(newLimitRGB);
            if (outside == false)
            {
                low = high;
                high = high + search_range;
            }
        }

        while (high-low > 1e-4)
        {
            const float sampleM = (high + low) / 2.f;
            const f3 searchJMh = {limit_J_max, sampleM, hue};
            const f3 newLimitRGB = JMh_to_RGB(searchJMh, XYZ_TO_RGB, peakLuminance, params);
            outside = any_below_zero(newLimitRGB);
            if (outside)
            {
                high = sampleM;
            }
            else
            {
                low = sampleM;
            }
        }

        gamutReachTable.table[i] = high;
    }

    return gamutReachTable;
}

inline constexpr bool outside_hull(const f3 &rgb)
{
    // limit value, once we cross this value, we are outside of the top gamut shell
    const float maxRGBtestVal = 1.0;
    return rgb[0] > maxRGBtestVal || rgb[1] > maxRGBtestVal || rgb[2] > maxRGBtestVal;
}

inline constexpr bool evaluate_gamma_fit(
    const f2 &JMcusp,
    const f3 testJMh[3],
    float topGamma,
    float peakLuminance,
    float limit_J_max,
    float mid_J,
    float focus_dist,
    const m33f &limit_XYZ_to_RGB,
    const JMhParams &limitJMhParams)
{
    const float focusJ = lerpf(JMcusp[0], mid_J, std::min(1.f, cusp_mid_blend - (JMcusp[0] / limit_J_max)));

    for (size_t testIndex = 0; testIndex < 3; testIndex++)
    {
        const float slope_gain = limit_J_max * focus_dist * get_focus_gain(testJMh[testIndex][0], JMcusp[0], limit_J_max);
        const f3 approxLimit = find_gamut_boundary_intersection(testJMh[testIndex], JMcusp, focusJ, limit_J_max, slope_gain, topGamma, lower_hull_gamma);
        const f3 approximate_JMh = {approxLimit[0], approxLimit[1], testJMh[testIndex][2]};
        const f3 newLimitRGB = JMh_to_RGB(approximate_JMh, limit_XYZ_to_RGB, peakLuminance, limitJMhParams);

        if (!outside_hull(newLimitRGB))
        {
            return false;
        }
    }

    return true;
}

inline constexpr Table1D make_upper_hull_gamma(
    const Table3D &gamutCuspTable,
    float peakLuminance,
    float limit_J_max,
    float mid_J,
    float focus_dist,
    const m33f &limit_XYZ_to_RGB,
    const JMhParams &limitJMhParams)
{
    const int test_count = 3;
    const float testPositions[test_count] = {0.01f, 0.5f, 0.99f};

    Table1D gammaTable{};
    Table1D gamutTopGamma{};

    for (int i = 0; i < gammaTable.size; i++)
    {
        gammaTable.table[i] = -1.f;

        const float hue = i;
        const f2 JMcusp = cuspFromTable(hue, gamutCuspTable);

        f3 testJMh[test_count]{};
        for (int testIndex = 0; testIndex < test_count; testIndex++)
        {
            const float testJ = JMcusp[0] + ((limit_J_max - JMcusp[0]) * testPositions[testIndex]);
            testJMh[testIndex] = {
                testJ,
                JMcusp[1],
                hue
            };
        }

        const float search_range = gammaSearchStep;
        float low = gammaMinimum;
        float high = low + search_range;
        bool outside = false;

        while (!(outside) && (high < 5.f))
        {
            const bool gammaFound = evaluate_gamma_fit(JMcusp, testJMh, high, peakLuminance, limit_J_max, mid_J, focus_dist, limit_XYZ_to_RGB, limitJMhParams);
            if (!gammaFound)
            {
                low = high;
                high = high + search_range;
            }
            else
            {
                outside = true;
            }
        }

        float testGamma = -1.f;
        while ( (high-low) > gammaAccuracy)
        {
            testGamma = (high + low) / 2.f;
            const bool gammaFound = evaluate_gamma_fit(JMcusp, testJMh, testGamma, peakLuminance, limit_J_max, mid_J, focus_dist, limit_XYZ_to_RGB, limitJMhParams);
            if (gammaFound)
            {
                high = testGamma;
                gammaTable.table[i] = high;
            }
            else
            {
                low = testGamma;
            }
        }

        // Duplicate gamma value to array, leaving empty entries at first and last position
        gamutTopGamma.table[i+gamutTopGamma.base_index] = gammaTable.table[i];
    }

    // Copy last populated entry to first empty spot
    gamutTopGamma.table[0] = gammaTable.table[gammaTable.size-1];

    // Copy first populated entry to last empty spot
    gamutTopGamma.table[gamutTopGamma.total_size-1] = gammaTable.table[0];

    return gamutTopGamma;
}

// Tonescale pre-calculations
inline ToneScaleParams init_ToneScaleParams(float peakLuminance)
{
    // Preset constants that set the desired behavior for the curve
    const float n = peakLuminance;

    const float n_r = 100.0;    // normalized white in nits (what 1.0 should be)
    const float g = 1.15;       // surround / contrast
    const float c = 0.18;       // anchor for 18% grey
    const float c_d = 10.013;   // output luminance of 18% grey (in nits)
    const float w_g = 0.14;     // change in grey between different peak luminance
    const float t_1 = 0.04;     // shadow toe or flare/glare compensation
    const float r_hit_min = 128.;   // scene-referred value "hitting the roof"
    const float r_hit_max = 896.;   // scene-referred value "hitting the roof"

    // Calculate output constants
    const float r_hit = r_hit_min + (r_hit_max - r_hit_min) * (log(n/n_r)/log(10000.f/100.f));
    const float m_0 = (n / n_r);
    const float m_1 = 0.5f * (m_0 + sqrt(m_0 * (m_0 + 4.f * t_1)));
    const float u = pow((r_hit/m_1)/((r_hit/m_1)+1),g);
    const float m = m_1 / u;
    const float w_i = log(n/100.f)/log(2.f);
    const float c_t = c_d/n_r * (1. + w_i * w_g);
    const float g_ip = 0.5f * (c_t + sqrt(c_t * (c_t + 4.f * t_1)));
    const float g_ipp2 = -(m_1 * pow((g_ip/m),(1.f/g))) / (pow(g_ip/m , 1.f/g)-1.f);
    const float w_2 = c / g_ipp2;
    const float s_2 = w_2 * m_1;
    const float u_2 = pow((r_hit/m_1)/((r_hit/m_1) + w_2), g);
    const float m_2 = m_1 / u_2;

    ToneScaleParams TonescaleParams = {
        n,
        n_r,
        g,
        t_1,
        c_t,
        s_2,
        u_2,
        m_2
    };

    return TonescaleParams;
}

inline OutputTransformParams init_OutputTransformParams(
    float peakLuminance,
    Primaries limitingPrimaries,
    Primaries encodingPrimaries,
    bool clampAP1)
{
    const ToneScaleParams tsParams = init_ToneScaleParams(peakLuminance);
    const JMhParams inputJMhParams = initJMhParams(ACES_AP0::primaries);

    float limit_J_max = Y_to_Hellwig_J(peakLuminance, inputJMhParams);
    float mid_J = Y_to_Hellwig_J(tsParams.c_t * 100.f, inputJMhParams);

    // Calculated chroma compress variables
    const float log_peak = log10( tsParams.n / tsParams.n_r);
    const float compr = chroma_compress + (chroma_compress * chroma_compress_fact) * log_peak;
    const float sat = std::max(0.2f, chroma_expand - (chroma_expand * chroma_expand_fact) * log_peak);
    const float sat_thr = chroma_expand_thr / tsParams.n;
    const float model_gamma = 1.f / (surround[1] * (1.48f + sqrt(Y_b / L_A)));
    const float focus_dist = focus_distance + focus_distance * focus_distance_scaling * log_peak;

    OutputTransformParams params{};

    params.clamp_ap1 = clampAP1;

    params.peakLuminance = peakLuminance;

    params.AP1_TO_XYZ = RGBtoXYZ_f33(ACES_AP1::primaries);
    params.XYZ_TO_AP1 = XYZtoRGB_f33(ACES_AP1::primaries);
    params.AP0_TO_AP1 = RGBtoRGB_f33(ACES_AP0::primaries, ACES_AP1::primaries);

    params.n_r = tsParams.n_r;
    params.g = tsParams.g;
    params.t_1 = tsParams.t_1;
    params.c_t = tsParams.c_t;
    params.s_2 = tsParams.s_2;
    params.u_2 = tsParams.u_2;
    params.m_2 = tsParams.m_2;

    params.limit_J_max = limit_J_max;
    params.mid_J = mid_J;
    params.model_gamma = model_gamma;
    params.sat = sat;
    params.sat_thr = sat_thr;
    params.compr = compr;
    params.focus_dist = focus_dist;

    params.chromaCompressScale = pow(0.03379f * peakLuminance, 0.30596f) - 0.45135f;

    // Input
    params.INPUT_RGB_TO_XYZ = RGBtoXYZ_f33(ACES_AP0::primaries);
    params.INPUT_XYZ_TO_RGB = XYZtoRGB_f33(ACES_AP0::primaries);
    params.inputJMhParams = inputJMhParams;

    // Limit
    params.LIMIT_RGB_TO_XYZ = RGBtoXYZ_f33(limitingPrimaries);
    params.LIMIT_XYZ_TO_RGB = XYZtoRGB_f33(limitingPrimaries);
    params.limitJMhParams = initJMhParams(limitingPrimaries);

    // Output
    params.OUTPUT_XYZ_TO_RGB = XYZtoRGB_f33(encodingPrimaries);
    params.LIMIT_TO_OUTPUT = RGBtoRGB_f33(limitingPrimaries, encodingPrimaries);

    // params.reach_gamut_table = make_gamut_table(ACES_AP1::primaries, peakLuminance);
    params.reach_m_table = make_reach_m_table(ACES_AP1::primaries, peakLuminance);
    params.gamut_cusp_table = make_gamut_table(limitingPrimaries, peakLuminance);
    params.upper_hull_gamma_table = make_upper_hull_gamma(
        params.gamut_cusp_table,
        peakLuminance,
        limit_J_max,
        mid_J,
        focus_dist,
        params.LIMIT_XYZ_TO_RGB,
        params.limitJMhParams);

    // White scaling
    const f3 RGB_w = mult_f3_f33(params.limitJMhParams.XYZ_w, params.OUTPUT_XYZ_TO_RGB);
    const f3 RGB_w_f = mult_f_f3( 1.f / reference_luminance, RGB_w);
    params.white_scale = 1. / std::max( std::max(RGB_w_f[0], RGB_w_f[1]), RGB_w_f[2]); 

    return params;
}

// TODO: Use map to cache results for custom parameters
// TODO: Encoding primaries needed for creative white point handling?
inline OutputTransformParams get_transform_params(
    float peakLuminance,
    const Primaries &limitingPrimaries,
    const Primaries &encodingPrimaries,
    bool ap1Clamp)
{
    // if (peakLuminance == 100.f && limitingPrimaries == REC709_PRI /*&& encodingPrimaries == REC709_PRI*/ && ap1Clamp == true)
    // {
    //     return srgb_100nits_odt;
    // }
    // else if (peakLuminance == 1000.f && limitingPrimaries == P3D65_PRI /*&& encodingPrimaries == REC2020_PRI*/ && ap1Clamp == true)
    // {
    //     return rec2100_p3lim_1000nits_odt;
    // }
    // else
    {
        return init_OutputTransformParams(peakLuminance, limitingPrimaries, encodingPrimaries, ap1Clamp);
    }
}

} // namespace ACES2

} // OCIO namespace

#endif
