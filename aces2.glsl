// TODO: Remove, just for integer modulus
#extension GL_EXT_gpu_shader4 : enable
#extension GL_ARB_shader_storage_buffer_object : require

// Use uniform buffer array to avoid the following error:
// cannot locate suitable resource to bind variable "@TMP494". Possibly large array
const int table_size = 360;

layout (std430, binding = 1) buffer TablesBuffer {
    vec4 reach_gamut_table[table_size];
    float reach_cusp_table[table_size];
    vec4 gamut_cusp_table[table_size];
    float upperHullGammaTable[table_size];
} tablesBuffer;

#define M_PI 3.1415926535897932384626433832795

// Declaration of all helper methods

const mat3 AP0toXYZ = transpose(mat3(
     0.952552557, 0.000000000,  0.000093679,
     0.343966514, 0.728166044, -0.072132550,
    -0.000000039, 0.000000000,  1.008825183));
const mat3 XYZtoAP1 = transpose(mat3(
     1.641023159, -0.324803293, -0.236424685,
    -0.663663030,  1.615332007,  0.016756365,
     0.011721910, -0.008284439,  0.988394737));
const mat3 AP1toXYZ = transpose(mat3(
     0.662454247, 0.134004191, 0.156187713,
     0.272228748, 0.674081624, 0.053689521,
    -0.005574661, 0.004060729, 1.010339141));

const mat3 MATRIX_16 = transpose(mat3(
     0.364074469, 0.594700873, 0.041101277,
    -0.222245112, 1.073855400, 0.147945315,
    -0.002067621, 0.048825976, 0.950387657));
const mat3 MATRIX_16_INV = transpose(mat3(
     2.051275492, -1.140031576,  0.088755645,
     0.426939040,  0.700583577, -0.127522483,
    -0.017471245, -0.038472541,  1.058946729));

const mat3 LIMIT_RGB_TO_XYZ = transpose(mat3(
    0.412390828, 0.357584327, 0.180480868,
    0.212639034, 0.715168655, 0.072192341,
    0.019330820, 0.119194724, 0.950532556));
const mat3 LIMIT_XYZ_TO_RGB = transpose(mat3(
    3.240969658, -1.537383080, -0.498610765,
   -0.969243705,  1.875967503,  0.041555081,
    0.055630006, -0.203976765,  1.056970954));

const float reference_luminance = 100.f;
const float L_A = 100.f;
const float Y_b = 20.f;
const float ac_resp = 1.f;
const float ra = 2.f * ac_resp;
const float ba = 0.05f + (2.f - ra);
const mat3 panlrcm = transpose(mat3(
    460.0,  450.999938965,  288.0,
    460.0, -891.000061035, -261.0,
    460.0, -220.0        , -6300.0));
const vec3 surround = vec3(0.9f, 0.59f, 0.9f); // Dim surround

const float INPUT_F_L = 0.793700576;
const float INPUT_z = 1.927213669;
const vec3  INPUT_D_RGB = vec3(1.017295003, 0.988742769, 0.994400442);
const float INPUT_A_w = 39.488994598;
const float INPUT_A_w_J = 12.947210312;

const float LIMIT_F_L = 0.793700576;
const float LIMIT_z = 1.927213669;
const vec3  LIMIT_D_RGB = vec3(1.014714122, 0.976808310, 0.924309909);
const float LIMIT_A_w = 39.488994598;
const float LIMIT_A_w_J = 12.947210312;

const float TS_n_r = 100.000000000;
const float TS_g   = 1.149999976;
const float TS_t_1 = 0.039999999;
const float TS_c_t = 0.100129992;
const float TS_s_2 = 0.919858396;
const float TS_u_2 = 0.991799057;
const float TS_m_2 = 1.047103763;

const float limit_J_max = 100.000015259;
const float mid_J       = 34.096538544;
const float model_gamma = 0.879464149;
const float sat         = 1.299999952;
const float sat_thr     = 0.005000000;
const float compr       = 2.400000095;
const float focus_dist  = 1.350000024;

const float smooth_cusps = 0.12f;
const float smooth_m = 0.27f;
const float cusp_mid_blend = 1.3f;
const float focus_gain_blend = 0.3f;
const float focus_adjust_gain = 0.55f;
const float focus_distance = 1.35f;
const float focus_distance_scaling = 1.75f;
const float lower_hull_gamma = 1.14f;
const vec4  compr_func_params = vec4(0.75f, 1.1f, 1.3f, 1.f);

// Utilities

float smin(float a, float b, float s)
{
    float h = max(s - abs(a - b), 0.f) / s;
    return min(a, b) - h * h * h * s * (1.f / 6.f);
}

float wrap_to_360(float hue)
{
    float y = mod(hue, 360.f);
    if ( y < 0.f)
    {
        y = y + 360.f;
    }
    return y;
}

float radians_to_degrees(float radians)
{
    return radians * 180.f / M_PI;
}

float degrees_to_radians(float degrees)
{
    return degrees / 180.f * M_PI;
}

// Functions

float base_hue_for_position(int i_lo, int table_size)
{
    float result = i_lo * 360.f / table_size;
    return result;
}

int next_position_in_table(int entry, int table_size)
{
    int result = (entry + 1) % table_size;
    return result;
}

int hue_position_in_uniform_table(float hue, int table_size)
{
    float wrapped_hue = wrap_to_360(hue);
    // TODO:
    int result = int(wrapped_hue / 360.f * table_size);
    return result;
}

vec2 cuspFromTable(float h, vec4 gt[table_size])
{
    vec3 lo;
    vec3 hi;

    if (h <= gt[0][2])
    {
        lo[0] = gt[table_size-1][0];
        lo[1] = gt[table_size-1][1];
        lo[2] = gt[table_size-1][2] - 360.f;

        hi[0] = gt[0][0];
        hi[1] = gt[0][1];
        hi[2] = gt[0][2];
    }
    else if (h >= gt[table_size-1][2])
    {
        lo[0] = gt[table_size-1][0];
        lo[1] = gt[table_size-1][1];
        lo[2] = gt[table_size-1][2];

        hi[0] = gt[0][0];
        hi[1] = gt[0][1];
        hi[2] = gt[0][2] + 360.f;
    }
    else
    {
        int low_i = 0;
        int high_i = table_size;
        // allowed as we have an extra entry in the table
        int i = hue_position_in_uniform_table(h, table_size);

        while (low_i + 1 < high_i)
        {
            if (h > gt[i][2])
            {
                low_i = i;
            }
            else
            {
                high_i = i;
            }
            // TODO:
            i = int((low_i + high_i) / 2.f);
        }

        lo[0] = gt[high_i-1][0];
        lo[1] = gt[high_i-1][1];
        lo[2] = gt[high_i-1][2];

        hi[0] = gt[high_i][0];
        hi[1] = gt[high_i][1];
        hi[2] = gt[high_i][2];
    }

    float t = (h - lo[2]) / (hi[2] - lo[2]);
    float cuspJ = mix(lo[0], hi[0], t);
    float cuspM = mix(lo[1], hi[1], t);

    return vec2(cuspJ, cuspM);
}

float reachFromTable(float h, float gt[table_size])
{
    int i_lo = hue_position_in_uniform_table( h, table_size);
    int i_hi = next_position_in_table( i_lo, table_size);

    float lo = gt[i_lo];
    float hi = gt[i_hi];

    float t = (h - i_lo) / (i_hi - i_lo);

    return mix(lo, hi, t);
}

float toe( float x, float limit, float k1_in, float k2_in)
{
    if (x > limit)
    {
        return x;
    }

    float k2 = max(k2_in, 0.001f);
    float k1 = sqrt(k1_in * k1_in + k2 * k2);
    float k3 = (limit + k1) / (limit + k2);

    return 0.5f * (k3 * x - k1 + sqrt((k3 * x - k1) * (k3 * x - k1) + 4.f * k2 * k3 * x));
}

float get_focus_gain(float J, float cuspJ, float limit_J_max)
{
    float thr = mix(cuspJ, limit_J_max, focus_gain_blend);

    if (J > thr)
    {
        // Approximate inverse required above threshold
        float gain = (limit_J_max - thr) / max(0.0001f, (limit_J_max - min(limit_J_max, J)));
        return pow(log(gain) / log(10.), 1. / focus_adjust_gain) + 1.f;
    }
    else
    {
        // Analytic inverse possible below cusp
        return 1.f;
    }
}

float solve_J_intersect(float J, float M, float focusJ, float maxJ, float slope_gain)
{
    float a = M / (focusJ * slope_gain);
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

    float root = sqrt(b * b - 4.f * a * c);

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

vec3 get_reach_boundary(float J, float M, float h)
{
    int i_lo = hue_position_in_uniform_table(h, table_size);
    int i_hi = next_position_in_table(i_lo, table_size);

    float lo = tablesBuffer.reach_cusp_table[i_lo];
    float hi = tablesBuffer.reach_cusp_table[i_hi];

    float t = (h - i_lo) / (360.f / table_size);

    float reachMaxM = mix(lo, hi, t);

    vec2 JMcusp = cuspFromTable(h, tablesBuffer.gamut_cusp_table);
    float focusJ = mix(JMcusp[0], mid_J, min(1.f, cusp_mid_blend - (JMcusp[0] / limit_J_max)));
    float slope_gain = limit_J_max * focus_dist * get_focus_gain(J, JMcusp[0], limit_J_max);

    float intersectJ = solve_J_intersect(J, M, focusJ, limit_J_max, slope_gain);

    float slope;
    if (intersectJ < focusJ)
    {
        slope = intersectJ * (intersectJ - focusJ) / (focusJ * slope_gain);
    }
    else
    {
        slope = (limit_J_max - intersectJ) * (intersectJ - focusJ) / (focusJ * slope_gain);
    }

    float boundary = limit_J_max * pow(intersectJ / limit_J_max, model_gamma) * reachMaxM / (limit_J_max - slope * reachMaxM);
    return vec3(J, boundary, h);
}

float hueDependentUpperHullGamma(float h, float gt[table_size])
{
    int i_lo = hue_position_in_uniform_table(h, table_size);
    int i_hi = next_position_in_table(i_lo, table_size);

    float base_hue = base_hue_for_position(i_lo, table_size);
    float t = wrap_to_360(h) - base_hue;

    return mix(gt[i_lo], gt[i_hi], t);
}

vec3 find_gamut_boundary_intersection(vec3 JMh_s, vec2 JM_cusp_in, float J_focus, float J_max, float slope_gain, float gamma_top, float gamma_bottom)
{
    float slope = 0.f;

    float s = max(0.000001f, smooth_cusps);
    vec2 JM_cusp = vec2(
        JM_cusp_in[0],
        JM_cusp_in[1] * (1.f + smooth_m * s)
    );

    float J_intersect_source = solve_J_intersect(JMh_s[0], JMh_s[1], J_focus, J_max, slope_gain);
    float J_intersect_cusp = solve_J_intersect(JM_cusp[0], JM_cusp[1], J_focus, J_max, slope_gain);

    if (J_intersect_source < J_focus)
    {
        slope = J_intersect_source * (J_intersect_source - J_focus) / (J_focus * slope_gain);
    }
    else
    {
        slope = (J_max - J_intersect_source) * (J_intersect_source - J_focus) / (J_focus * slope_gain);
    }

    float M_boundary_lower = J_intersect_cusp * pow(J_intersect_source / J_intersect_cusp, 1. / gamma_bottom) / (JM_cusp[0] / JM_cusp[1] - slope);
    float M_boundary_upper = JM_cusp[1] * (J_max - J_intersect_cusp) * pow((J_max - J_intersect_source) / (J_max - J_intersect_cusp), 1. / gamma_top) / (slope * JM_cusp[1] + J_max - JM_cusp[0]);
    float M_boundary = JM_cusp[1] * smin(M_boundary_lower / JM_cusp[1], M_boundary_upper / JM_cusp[1], s);
    float J_boundary = J_intersect_source + slope * M_boundary;

    return vec3(J_boundary, M_boundary, J_intersect_source);
}

float compressPowerP(float v, float thr, float lim, float power)
{
    float s = (lim-thr) / pow(pow((1.f-thr) / (lim-thr), -power) - 1.f, 1.f/power);
    float nd = (v - thr) / s;
    float p = pow(nd, power);

    float vCompressed;

    if ( v < thr || lim < 1.0001f)
    {
        vCompressed = v;
    }
    else
    {
        vCompressed = thr + s * nd / ( pow(1.f + p, 1.f / power));
    }

    return vCompressed;
}


// Declaration of the OCIO shader function

vec4 OCIODisplay(vec4 inPixel)
{
    vec4 outColor;

    for (int i = 0; i < 10; ++i)
    {
        outColor = inPixel;

    // Clamp AP1 and convert to XYZ
    {
        vec3 XYZ = AP0toXYZ * outColor.rgb;
        vec3 AP1 = XYZtoAP1 * XYZ;
        vec3 AP1_clamped = max(AP1, vec3(0.f));
        XYZ = AP1toXYZ * AP1_clamped;

        outColor.rgb = XYZ;
    }

    // XYZ to JMh
    {
        vec3 XYZ = outColor.rgb * reference_luminance;

        // Step 1 - Converting CIE XYZ tristimulus values to sharpened RGB values
        vec3 RGB = MATRIX_16 * XYZ;

        // Step 2
        vec3 RGB_c = INPUT_D_RGB * RGB;

        // Step 3 - apply  forward post-adaptation non-linear response compression
        vec3 F_L_v = pow(INPUT_F_L * abs(RGB_c) / 100.f, vec3(0.42f));
        vec3 RGB_a = (400.f * sign(RGB_c) * F_L_v) / (27.13f + F_L_v);

        // Step 4 - Converting to preliminary cartesian  coordinates
        float a = RGB_a.r - 12.f * RGB_a.g / 11.f + RGB_a.b / 11.f;
        float b = (RGB_a.r + RGB_a.g - 2.f * RGB_a.b) / 9.f;

        // Computing the hue angle math h
        float hr = atan(b, a);
        float h = wrap_to_360(radians_to_degrees(hr));

        // Step 6 - Computing achromatic responses for the stimulus
        float A = ra * RGB_a.r + RGB_a.g + ba * RGB_a.b;

        // Step 7 - Computing the correlate of lightness, J
        float J = 100.f * pow( A / INPUT_A_w, surround[1] * INPUT_z);

        // Step 9 - Computing the correlate of colourfulness, M
        float M = J == 0.f ? 0.f : 43.f * surround[2] * sqrt(a * a + b * b);

        outColor.rgb = vec3(J, M, h);
    }

    // ToneMap and ChromaCompress
    {
        float J = outColor[0];
        float M = outColor[1];
        float h = outColor[2];

        // Hellwig_J_to_Y
        float A = INPUT_A_w_J * pow(abs(J) / 100.f, 1.f / (surround[1] * INPUT_z));
        float linear =  sign(J) * 100.f / INPUT_F_L * pow((27.13f * A) / (400.f - A), 1.f / 0.42f);
        linear /= reference_luminance;

        // ToneScale
        float f = TS_m_2 * pow(max(0.f, linear) / (linear + TS_s_2), TS_g);
        float hh = max(0.f, f * f / (f + TS_t_1));
        float Y = hh * TS_n_r;

        // Y_to_Hellwig_J
        float F_L_Y = pow(INPUT_F_L * abs(Y) / 100., 0.42);
        float tonemappedJ = sign(Y) * 100.f * pow(((400.f * F_L_Y) / (27.13f + F_L_Y)) / INPUT_A_w_J, surround[1] * INPUT_z);

        // Chroma Compression
        float origJ = J;
        J = tonemappedJ;

        if (M != 0.0)
        {
            float nJ = J / limit_J_max;
            float snJ = max(0.f, 1.f - nJ);
            float Mnorm = cuspFromTable(h, tablesBuffer.reach_gamut_table)[1];
            float limit = pow(nJ, model_gamma) * reachFromTable(h, tablesBuffer.reach_cusp_table) / Mnorm;

            M = M * pow(J / origJ, model_gamma);
            M = M / Mnorm;
            M = limit - toe(limit - M, limit - 0.001f, snJ * sat, sqrt(nJ * nJ + sat_thr));
            M = toe(M, limit, nJ * compr, snJ);

            M = M * Mnorm;
        }

        outColor.rgb = vec3(tonemappedJ, M, h);
    }

    // Gamut Compress
    {
        vec3 JMh = outColor.rgb;
        float J = outColor[0];
        float Jx = J;
        float M = outColor[1];
        float h = outColor[2];

        vec2 project_from = vec2(J, M);
        vec2 JMcusp = cuspFromTable(h, tablesBuffer.gamut_cusp_table);

        if (M < 0.0001f || J > limit_J_max)
        {
            outColor.rgb = vec3(J, 0.f, h);
        }

        float focusJ = mix(JMcusp[0], mid_J, min(1.f, cusp_mid_blend - (JMcusp[0] / limit_J_max)));
        float slope_gain = limit_J_max * focus_dist * get_focus_gain(Jx, JMcusp[0], limit_J_max);

        float gamma_top = hueDependentUpperHullGamma(h, tablesBuffer.upperHullGammaTable);
        float gamma_bottom = lower_hull_gamma;

        vec3 boundaryReturn = find_gamut_boundary_intersection(JMh, JMcusp, focusJ, limit_J_max, slope_gain, gamma_top, gamma_bottom);
        vec2 JMboundary = vec2(boundaryReturn[0], boundaryReturn[1]);
        vec2 project_to = vec2(boundaryReturn[2], 0.f);

        vec3 reachBoundary = get_reach_boundary(JMboundary[0], JMboundary[1], JMh[2]);

        float difference = max(1.0001f, reachBoundary[1] / JMboundary[1]);
        float threshold = max(compr_func_params[0], 1.f / difference);

        float v = project_from[1] / JMboundary[1];
        v = compressPowerP(v, threshold, difference, compr_func_params[3]);

        vec2 JMcompressed = vec2(
            project_to[0] + v * (JMboundary[0] - project_to[0]),
            project_to[1] + v * (JMboundary[1] - project_to[1])
        );

        outColor.rgb = vec3(JMcompressed[0], JMcompressed[1], JMh[2]);
    }

    // JMh to RGB
    {
        float J = outColor[0];
        float M = outColor[1];
        float h = outColor[2];

        float hr = degrees_to_radians(h);

        // Computing achromatic respons A for the stimulus
        float A = LIMIT_A_w * pow(J / 100.f, 1. / (surround[1] * LIMIT_z));

        // Computing P_p_1 to P_p_2
        float P_p_1 = 43.f * surround[2];
        float P_p_2 = A;

        // Step 3 - Computing opponent colour dimensions a and b
        float gamma = M / P_p_1;
        float a = gamma * cos(hr);
        float b = gamma * sin(hr);

        // Step 4 - Applying post-adaptation non-linear response compression matrix
        vec3 vec = vec3(P_p_2, a, b);
        vec3 RGB_a = 1.0/1403.0 * (panlrcm * vec);

        // Step 5 - Applying the inverse post-adaptation non-linear respnose compression
        vec3 RGB_c = sign(RGB_a) * 100.f / LIMIT_F_L * pow((27.13f * abs(RGB_a) / (400.f - abs(RGB_a))), vec3(1.f / 0.42f));

        // Step 6
        vec3 RGB = RGB_c / LIMIT_D_RGB;

        // Step 7
        vec3 XYZ = MATRIX_16_INV * RGB;

        vec3 luminanceRGB = LIMIT_XYZ_TO_RGB * XYZ;
        luminanceRGB = luminanceRGB / reference_luminance;

        outColor.rgb = luminanceRGB.rgb;
    }

    // Add Gamma 'monCurveRev' processing

    {
        vec4 breakPnt = vec4(0.00303993467, 0.00303993467, 0.00303993467, 1.);
        vec4 slope = vec4(12.9232101, 12.9232101, 12.9232101, 1.);
        vec4 scale = vec4(1.05499995, 1.05499995, 1.05499995, 1.00000095);
        vec4 offset = vec4(0.0549999997, 0.0549999997, 0.0549999997, 9.99999997e-07);
        vec4 gamma = vec4(0.416666657, 0.416666657, 0.416666657, 0.999998987);
        vec4 isAboveBreak = vec4(greaterThan( outColor, breakPnt));
        vec4 linSeg = outColor * slope;
        vec4 powSeg = pow( max( vec4(0., 0., 0., 0.), outColor ), gamma ) * scale - offset;
        vec4 res = isAboveBreak * powSeg + ( vec4(1., 1., 1., 1.) - isAboveBreak ) * linSeg;
        outColor.rgb = vec3(res.x, res.y, res.z);
        outColor.a = res.w;
    }

    // Add Range processing

    {
        outColor.rgb = max(vec3(0., 0., 0.), outColor.rgb);
    }

    }

    return outColor;
}
