// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <cmath>

#include <OpenColorIO/OpenColorIO.h>

#include "utils/StringUtils.h"
#include "ops/fixedfunction/FixedFunctionOpGPU.h"
#include "ACES2/Transform.h"


namespace OCIO_NAMESPACE
{

void Add_hue_weight_shader(GpuShaderCreatorRcPtr & shaderCreator, GpuShaderText & ss, float width)
{
    float center = 0.f;  // If changed, must uncomment code below.

    // Convert from degrees to radians.
    const float PI = 3.14159265358979f;
    center = center * PI / 180.f;
    const float widthR = width * PI / 180.f;
    // Actually want to multiply by (4/width).
    const float inv_width = 4.f / widthR;

    const std::string pxl(shaderCreator->getPixelName());

    // TODO: Use formatters in GPUShaderUtils but increase precision.
    // (See the CPU renderer for more info on the algorithm.)

    //! \todo There is a performance todo in the GPUHueVec shader that would also apply here.
    ss.newLine() << ss.floatDecl("a") << " = 2.0 * " << pxl << ".rgb.r - (" << pxl << ".rgb.g + " << pxl << ".rgb.b);";
    ss.newLine() << ss.floatDecl("b") << " = 1.7320508075688772 * (" << pxl << ".rgb.g - " << pxl << ".rgb.b);";
    ss.newLine() << ss.floatDecl("hue") << " = " << ss.atan2("b", "a") << ";";

    // Since center is currently zero, commenting these lines out as a performance optimization
    //  << "    hue = hue - float(" << center << ");\n"
    //  << "    hue = mix( hue, hue + 6.28318530717959, step( hue, -3.14159265358979));\n"
    //  << "    hue = mix( hue, hue - 6.28318530717959, step( 3.14159265358979, hue));\n"

    ss.newLine() << ss.floatDecl("knot_coord") << " = clamp(2. + hue * float(" << inv_width << "), 0., 4.);";
    ss.newLine() << "int j = int(min(knot_coord, 3.));";
    ss.newLine() << ss.floatDecl("t") << " = knot_coord - float(j);";
    ss.newLine() << ss.float4Decl("monomials") << " = " << ss.float4Const("t*t*t", "t*t", "t", "1.") << ";";
    ss.newLine() << ss.float4Decl("m0") << " = " << ss.float4Const(0.25,  0.00,  0.00,  0.00) << ";";
    ss.newLine() << ss.float4Decl("m1") << " = " << ss.float4Const(-0.75,  0.75,  0.75,  0.25) << ";";
    ss.newLine() << ss.float4Decl("m2") << " = " << ss.float4Const(0.75, -1.50,  0.00,  1.00) << ";";
    ss.newLine() << ss.float4Decl("m3") << " = " << ss.float4Const(-0.25,  0.75, -0.75,  0.25) << ";";
    ss.newLine() << ss.float4Decl("coefs") << " = " << ss.lerp( "m0", "m1", "float(j == 1)") << ";";
    ss.newLine() << "coefs = " << ss.lerp("coefs", "m2", "float(j == 2)") << ";";
    ss.newLine() << "coefs = " << ss.lerp("coefs", "m3", "float(j == 3)") << ";";
    ss.newLine() << ss.floatDecl("f_H") << " = dot(coefs, monomials);";
}

void Add_RedMod_03_Fwd_Shader(GpuShaderCreatorRcPtr & shaderCreator, GpuShaderText & ss)
{
    const float _1minusScale = 1.f - 0.85f;  // (1. - scale) from the original ctl code
    const float _pivot = 0.03f;

    Add_hue_weight_shader(shaderCreator, ss, 120.f);

    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.floatDecl("maxval") << " = max( " << pxl << ".rgb.r, max( " << pxl << ".rgb.g, " << pxl << ".rgb.b));";
    ss.newLine() << ss.floatDecl("minval") << " = min( " << pxl << ".rgb.r, min( " << pxl << ".rgb.g, " << pxl << ".rgb.b));";

    ss.newLine() << ss.floatDecl("oldChroma") << " = max(1e-10, maxval - minval);";
    ss.newLine() << ss.float3Decl("delta") << " = " << pxl << ".rgb - minval;";

    ss.newLine() << ss.floatDecl("f_S") << " = ( max(1e-10, maxval) - max(1e-10, minval) ) / max(1e-2, maxval);";

    ss.newLine() << pxl << ".rgb.r = " << pxl << ".rgb.r + f_H * f_S * (" << _pivot
                 << " - " << pxl << ".rgb.r) * " << _1minusScale << ";";

    ss.newLine() << ss.floatDecl("maxval2") << " = max( " << pxl << ".rgb.r, max( " << pxl << ".rgb.g, " << pxl << ".rgb.b));";
    ss.newLine() << ss.floatDecl("newChroma") << " = maxval2 - minval;";
    ss.newLine() << pxl << ".rgb = minval + delta * newChroma / oldChroma;";
}

void Add_RedMod_03_Inv_Shader(GpuShaderCreatorRcPtr & shaderCreator, GpuShaderText & ss)
{
    const float _1minusScale = 1.f - 0.85f;  // (1. - scale) from the original ctl code
    const float _pivot = 0.03f;

    Add_hue_weight_shader(shaderCreator, ss, 120.f);

    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << "if (f_H > 0.)";
    ss.newLine() << "{";
    ss.indent();

    ss.newLine() << ss.floatDecl("maxval") << " = max( " << pxl << ".rgb.r, max( " << pxl << ".rgb.g, " << pxl << ".rgb.b));";
    ss.newLine() << ss.floatDecl("minval") << " = min( " << pxl << ".rgb.r, min( " << pxl << ".rgb.g, " << pxl << ".rgb.b));";

    ss.newLine() << ss.floatDecl("oldChroma") << " = max(1e-10, maxval - minval);";
    ss.newLine() << ss.float3Decl("delta") << " = " << pxl << ".rgb - minval;";

    // Note: If f_H == 0, the following generally doesn't change the red value,
    //       but it does for R < 0, hence the need for the if-statement above.
    ss.newLine() << ss.floatDecl("ka") << " = f_H * " << _1minusScale << " - 1.;";
    ss.newLine() << ss.floatDecl("kb") << " = " << pxl << ".rgb.r - f_H * (" << _pivot << " + minval) * "
                 << _1minusScale << ";";
    ss.newLine() << ss.floatDecl("kc") << " = f_H * " << _pivot << " * minval * " << _1minusScale << ";";
    ss.newLine() << pxl << ".rgb.r = ( -kb - sqrt( kb * kb - 4. * ka * kc)) / ( 2. * ka);";

    ss.newLine() << ss.floatDecl("maxval2") << " = max( " << pxl << ".rgb.r, max( " << pxl << ".rgb.g, " << pxl << ".rgb.b));";
    ss.newLine() << ss.floatDecl("newChroma") << " = maxval2 - minval;";
    ss.newLine() << pxl << ".rgb = minval + delta * newChroma / oldChroma;";

    ss.dedent();
    ss.newLine() << "}";
}

void Add_RedMod_10_Fwd_Shader(GpuShaderCreatorRcPtr & shaderCreator, GpuShaderText & ss)
{
    const float _1minusScale = 1.f - 0.82f;  // (1. - scale) from the original ctl code
    const float _pivot = 0.03f;

    Add_hue_weight_shader(shaderCreator, ss, 135.f);

    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.floatDecl("maxval") << " = max( " << pxl << ".rgb.r, max( " << pxl << ".rgb.g, " << pxl << ".rgb.b));";
    ss.newLine() << ss.floatDecl("minval") << " = min( " << pxl << ".rgb.r, min( " << pxl << ".rgb.g, " << pxl << ".rgb.b));";

    ss.newLine() << ss.floatDecl("f_S") << " = ( max(1e-10, maxval) - max(1e-10, minval) ) / max(1e-2, maxval);";

    ss.newLine() << pxl << ".rgb.r = " << pxl << ".rgb.r + f_H * f_S * (" << _pivot
                 << " - " << pxl << ".rgb.r) * " << _1minusScale << ";";
}

void Add_RedMod_10_Inv_Shader(GpuShaderCreatorRcPtr & shaderCreator, GpuShaderText & ss)
{
    const float _1minusScale = 1.f - 0.82f;  // (1. - scale) from the original ctl code
    const float _pivot = 0.03f;

    Add_hue_weight_shader(shaderCreator, ss, 135.f);

    ss.newLine() << "if (f_H > 0.)";
    ss.newLine() << "{";
    ss.indent();

    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.floatDecl("minval") << " = min( " << pxl << ".rgb.g, " << pxl << ".rgb.b);";

    // Note: If f_H == 0, the following generally doesn't change the red value
    //       but it does for R < 0, hence the if.
    ss.newLine() << ss.floatDecl("ka") << " = f_H * " << _1minusScale << " - 1.;";
    ss.newLine() << ss.floatDecl("kb") << " = " << pxl << ".rgb.r - f_H * (" << _pivot << " + minval) * "
                 << _1minusScale << ";";
    ss.newLine() << ss.floatDecl("kc") << " = f_H * " << _pivot << " * minval * " << _1minusScale << ";";
    ss.newLine() << pxl << ".rgb.r = ( -kb - sqrt( kb * kb - 4. * ka * kc)) / ( 2. * ka);";

    ss.dedent();
    ss.newLine() << "}";
}

void Add_Glow_03_Fwd_Shader(GpuShaderCreatorRcPtr & shaderCreator, GpuShaderText & ss, float glowGain, float glowMid)
{
    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.floatDecl("chroma") << " = sqrt( " << pxl << ".rgb.b * (" << pxl << ".rgb.b - " << pxl << ".rgb.g)"
                 << " + " << pxl << ".rgb.g * (" << pxl << ".rgb.g - " << pxl << ".rgb.r)"
                 << " + " << pxl << ".rgb.r * (" << pxl << ".rgb.r - " << pxl << ".rgb.b) );";
    ss.newLine() << ss.floatDecl("YC") << " = (" << pxl << ".rgb.b + " << pxl << ".rgb.g + " << pxl << ".rgb.r + 1.75 * chroma) / 3.;";

    ss.newLine() << ss.floatDecl("maxval") << " = max( " << pxl << ".rgb.r, max( " << pxl << ".rgb.g, " << pxl << ".rgb.b));";
    ss.newLine() << ss.floatDecl("minval") << " = min( " << pxl << ".rgb.r, min( " << pxl << ".rgb.g, " << pxl << ".rgb.b));";

    ss.newLine() << ss.floatDecl("sat") << " = ( max(1e-10, maxval) - max(1e-10, minval) ) / max(1e-2, maxval);";

    ss.newLine() << ss.floatDecl("x") << " = (sat - 0.4) * 5.;";
    ss.newLine() << ss.floatDecl("t") << " = max( 0., 1. - 0.5 * abs(x));";
    ss.newLine() << ss.floatDecl("s") << " = 0.5 * (1. + sign(x) * (1. - t * t));";

    ss.newLine() << ss.floatDecl("GlowGain")    << " = " << glowGain << " * s;";
    ss.newLine() << ss.floatDecl("GlowMid")     << " = " << glowMid << ";";
    ss.newLine() << ss.floatDecl("glowGainOut") << " = " << ss.lerp( "GlowGain", "GlowGain * (GlowMid / YC - 0.5)",
                                                                     "float( YC > GlowMid * 2. / 3. )" ) << ";";
    ss.newLine() << "glowGainOut = " << ss.lerp( "glowGainOut", "0.", "float( YC > GlowMid * 2. )" ) << ";";

    ss.newLine() << pxl << ".rgb = " << pxl << ".rgb * glowGainOut + " << pxl << ".rgb;";
}

void Add_Glow_03_Inv_Shader(GpuShaderCreatorRcPtr & shaderCreator, GpuShaderText & ss, float glowGain, float glowMid)
{
    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.floatDecl("chroma") << " = sqrt( " << pxl << ".rgb.b * (" << pxl << ".rgb.b - " << pxl << ".rgb.g)"
                 << " + " << pxl << ".rgb.g * (" << pxl << ".rgb.g - " << pxl << ".rgb.r)"
                 << " + " << pxl << ".rgb.r * (" << pxl << ".rgb.r - " << pxl << ".rgb.b) );";
    ss.newLine() << ss.floatDecl("YC") << " = (" << pxl << ".rgb.b + " << pxl << ".rgb.g + " << pxl << ".rgb.r + 1.75 * chroma) / 3.;";

    ss.newLine() << ss.floatDecl("maxval") << " = max( " << pxl << ".rgb.r, max( " << pxl << ".rgb.g, " << pxl << ".rgb.b));";
    ss.newLine() << ss.floatDecl("minval") << " = min( " << pxl << ".rgb.r, min( " << pxl << ".rgb.g, " << pxl << ".rgb.b));";

    ss.newLine() << ss.floatDecl("sat") << " = ( max(1e-10, maxval) - max(1e-10, minval) ) / max(1e-2, maxval);";

    ss.newLine() << ss.floatDecl("x") << " = (sat - 0.4) * 5.;";
    ss.newLine() << ss.floatDecl("t") << " = max( 0., 1. - 0.5 * abs(x));";
    ss.newLine() << ss.floatDecl("s") << " = 0.5 * (1. + sign(x) * (1. - t * t));";

    ss.newLine() << ss.floatDecl("GlowGain")    << " = " << glowGain << " * s;";
    ss.newLine() << ss.floatDecl("GlowMid")     << " = " << glowMid << ";";
    ss.newLine() << ss.floatDecl("glowGainOut") << " = "
                 << ss.lerp( "-GlowGain / (1. + GlowGain)",
                             "GlowGain * (GlowMid / YC - 0.5) / (GlowGain * 0.5 - 1.)",
                             "float( YC > (1. + GlowGain) * GlowMid * 2. / 3. )" ) << ";";
    ss.newLine() << "glowGainOut = " << ss.lerp( "glowGainOut", "0.", "float( YC > GlowMid * 2. )" ) << ";";

    ss.newLine() << pxl << ".rgb = " << pxl << ".rgb * glowGainOut + " << pxl << ".rgb;";
}

void Add_GamutComp_13_Shader_Compress(GpuShaderText & ss,
                                      const char * dist,
                                      const char * cdist,
                                      float scl,
                                      float thr,
                                      float power)
{
    // Only compress if greater or equal than threshold.
    ss.newLine() << "if (" << dist << " >= " << thr << ")";
    ss.newLine() << "{";
    ss.indent();

    // Normalize distance outside threshold by scale factor.
    ss.newLine() << ss.floatDecl("nd") << " = (" << dist << " - " << thr << ") / " << scl << ";";
    ss.newLine() << ss.floatDecl("p") << " = pow(nd, " << power << ");";
    ss.newLine() << cdist << " = " << thr << " + " << scl << " * nd / (pow(1.0 + p, " << 1.0f / power << "));";

    ss.dedent();
    ss.newLine() << "}"; // if (dist >= thr)
}

void Add_GamutComp_13_Shader_UnCompress(GpuShaderText & ss,
                                        const char * dist,
                                        const char * cdist,
                                        float scl,
                                        float thr,
                                        float power)
{
    // Only compress if greater or equal than threshold, avoid singularity.
    ss.newLine() << "if (" << dist << " >= " << thr << " && " << dist << " < " << thr + scl << " )";
    ss.newLine() << "{";
    ss.indent();

    // Normalize distance outside threshold by scale factor.
    ss.newLine() << ss.floatDecl("nd") << " = (" << dist << " - " << thr << ") / " << scl << ";";
    ss.newLine() << ss.floatDecl("p") << " = pow(nd, " << power << ");";
    ss.newLine() << cdist << " = " << thr << " + " << scl << " * pow(-(p / (p - 1.0)), " << 1.0f / power << ");";

    ss.dedent();
    ss.newLine() << "}"; // if (dist >= thr && dist < thr + scl)
}

template <typename Func>
void Add_GamutComp_13_Shader(GpuShaderText & ss,
                             GpuShaderCreatorRcPtr & sc,
                             float limCyan,
                             float limMagenta,
                             float limYellow,
                             float thrCyan,
                             float thrMagenta,
                             float thrYellow,
                             float power,
                             Func f)
{
    // Precompute scale factor for y = 1 intersect
    auto f_scale = [power](float lim, float thr) {
        return (lim - thr) / std::pow(std::pow((1.0f - thr) / (lim - thr), -power) - 1.0f, 1.0f / power);
    };
    const float scaleCyan      = f_scale(limCyan,    thrCyan);
    const float scaleMagenta   = f_scale(limMagenta, thrMagenta);
    const float scaleYellow    = f_scale(limYellow,  thrYellow);

    const char * pix = sc->getPixelName();

    // Achromatic axis
    ss.newLine() << ss.floatDecl("ach") << " = max( " << pix << ".rgb.r, max( " << pix << ".rgb.g, " << pix << ".rgb.b ) );";

    ss.newLine() << "if ( ach != 0. )";
    ss.newLine() << "{";
    ss.indent();

    // Distance from the achromatic axis for each color component aka inverse rgb ratios.
    ss.newLine() << ss.float3Decl("dist") << " = (ach - " << pix << ".rgb) / abs(ach);";
    ss.newLine() << ss.float3Decl("cdist") << " = dist;";

    f(ss, "dist.x", "cdist.x", scaleCyan,    thrCyan,    power);
    f(ss, "dist.y", "cdist.y", scaleMagenta, thrMagenta, power);
    f(ss, "dist.z", "cdist.z", scaleYellow,  thrYellow,  power);

    // Recalculate rgb from compressed distance and achromatic.
    // Effectively this scales each color component relative to achromatic axis by the compressed distance.
    ss.newLine() << pix << ".rgb = ach - cdist * abs(ach);";

    ss.dedent();
    ss.newLine() << "}"; // if ( ach != 0.0f )
}

void Add_GamutComp_13_Fwd_Shader(GpuShaderText & ss,
                                 GpuShaderCreatorRcPtr & sc,
                                 float limCyan,
                                 float limMagenta,
                                 float limYellow,
                                 float thrCyan,
                                 float thrMagenta,
                                 float thrYellow,
                                 float power)
{
    Add_GamutComp_13_Shader(
        ss,
        sc,
        limCyan,
        limMagenta,
        limYellow,
        thrCyan,
        thrMagenta,
        thrYellow,
        power,
        Add_GamutComp_13_Shader_Compress
    );
}

void Add_GamutComp_13_Inv_Shader(GpuShaderText & ss,
                                 GpuShaderCreatorRcPtr & sc,
                                 float limCyan,
                                 float limMagenta,
                                 float limYellow,
                                 float thrCyan,
                                 float thrMagenta,
                                 float thrYellow,
                                 float power)
{
    Add_GamutComp_13_Shader(
        ss,
        sc,
        limCyan,
        limMagenta,
        limYellow,
        thrCyan,
        thrMagenta,
        thrYellow,
        power,
        Add_GamutComp_13_Shader_UnCompress
    );
}

void _Add_RGB_to_JMh_Shader(
    GpuShaderCreatorRcPtr & shaderCreator,
    GpuShaderText & ss,
    const ACES2::JMhParams & p)
{
    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.float3Decl("lms") << " = " << ss.mat3fMul(&p.MATRIX_RGB_to_CAM16[0], pxl + ".rgb") << ";";
    ss.newLine() << "lms = " << "lms * " << ss.float3Const(p.D_RGB[0], p.D_RGB[1], p.D_RGB[2]) << ";";

    ss.newLine() << ss.float3Decl("F_L_v") << " = pow(" << p.F_L << " * abs(lms) / 100.0, " << ss.float3Const(0.42f) << ");";
    ss.newLine() << ss.float3Decl("rgb_a") << " = (400.0 * sign(lms) * F_L_v) / (27.13 + F_L_v);";

    ss.newLine() << ss.floatDecl("A") << " = 2.0 * rgb_a.r + rgb_a.g + 0.05 * rgb_a.b;";
    ss.newLine() << ss.floatDecl("a") << " = rgb_a.r - 12.0 * rgb_a.g / 11.0 + rgb_a.b / 11.0;";
    ss.newLine() << ss.floatDecl("b") << " = (rgb_a.r + rgb_a.g - 2.0 * rgb_a.b) / 9.0;";

    ss.newLine() << ss.floatDecl("J") << " = 100.0 * pow(A / " << p.A_w << ", " << ACES2::surround[1] << " * " << p.z << ");";

    ss.newLine() << ss.floatDecl("M") << " = (J == 0.0) ? 0.0 : 43.0 * " << ACES2::surround[2] << " * sqrt(a * a + b * b);";

    ss.newLine() << ss.floatDecl("h") << " = (a == 0.0) ? 0.0 : " << ss.atan2("b", "a") << " * 180.0 / 3.14159265358979;";
    ss.newLine() << "h = h - floor(h / 360.0) * 360.0;";
    ss.newLine() << "h = (h < 0.0) ? h + 360.0 : h;";

    ss.newLine() << pxl << ".rgb = " << ss.float3Const("J", "M", "h") << ";";
}

void _Add_JMh_to_RGB_Shader(
    GpuShaderCreatorRcPtr & shaderCreator,
    GpuShaderText & ss,
    const ACES2::JMhParams & p)
{
    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.floatDecl("h") << " = " << pxl << ".b * 3.14159265358979 / 180.0;";

    ss.newLine() << ss.floatDecl("scale") << " = " << pxl << ".g / (43.0 * " << ACES2::surround[2] << ");";
    ss.newLine() << ss.floatDecl("A") << " = " << p.A_w << " * pow(" << pxl << ".r / 100.0, 1.0 / (" << ACES2::surround[1] << " * " << p.z << "));";
    ss.newLine() << ss.floatDecl("a") << " = scale * cos(h);";
    ss.newLine() << ss.floatDecl("b") << " = scale * sin(h);";

    ss.newLine() << ss.float3Decl("rgb_a") << ";";
    ss.newLine() << "rgb_a.r = (460.0 * A + 451.0 * a + 288.0 *b) / 1403.0;";
    ss.newLine() << "rgb_a.g = (460.0 * A - 891.0 * a - 261.0 *b) / 1403.0;";
    ss.newLine() << "rgb_a.b = (460.0 * A - 220.0 * a - 6300.0 *b) / 1403.0;";

    ss.newLine() << ss.float3Decl("lms") << " = sign(rgb_a) * 100.0 / " << p.F_L << " * pow(27.13 * abs(rgb_a) / (400.0 - abs(rgb_a)), " << ss.float3Const(1.f / 0.42f) << ");";
    ss.newLine() << "lms = " << "lms / " << ss.float3Const(p.D_RGB[0], p.D_RGB[1], p.D_RGB[2]) << ";";

    ss.newLine() << pxl << ".rgb = " << ss.mat3fMul(&p.MATRIX_CAM16_to_RGB[0], "lms") << ";";
}

std::string _Add_Reach_table(
    GpuShaderCreatorRcPtr & shaderCreator,
    unsigned resourceIndex,
    const ACES2::Table1D & table)
{
    // Reserve name
    std::ostringstream resName;
    resName << shaderCreator->getResourcePrefix()
            << std::string("_")
            << std::string("reach_m_table_")
            << resourceIndex;

    // Note: Remove potentially problematic double underscores from GLSL resource names.
    std::string name(resName.str());
    StringUtils::ReplaceInPlace(name, "__", "_");

    // Register texture
    GpuShaderDesc::TextureDimensions dimensions = GpuShaderDesc::TEXTURE_1D;
    if (shaderCreator->getLanguage() == GPU_LANGUAGE_GLSL_ES_1_0
        || shaderCreator->getLanguage() == GPU_LANGUAGE_GLSL_ES_3_0
        || !shaderCreator->getAllowTexture1D())
    {
        dimensions = GpuShaderDesc::TEXTURE_2D;
    }

    shaderCreator->addTexture(
        name.c_str(),
        GpuShaderText::getSamplerName(name).c_str(),
        table.size,
        1,
        GpuShaderCreator::TEXTURE_RED_CHANNEL,
        dimensions,
        INTERP_NEAREST,
        &(table.table[0]));


    if (dimensions == GpuShaderDesc::TEXTURE_1D)
    {
        GpuShaderText ss(shaderCreator->getLanguage());
        ss.declareTex1D(name);
        shaderCreator->addToDeclareShaderCode(ss.string().c_str());
    }
    else
    {
        GpuShaderText ss(shaderCreator->getLanguage());
        ss.declareTex2D(name);
        shaderCreator->addToDeclareShaderCode(ss.string().c_str());
    }

    // Sampler function
    GpuShaderText ss(shaderCreator->getLanguage());

    ss.newLine() << ss.floatKeyword() << " " << name << "_sample(float h)";
    ss.newLine() << "{";
    ss.indent();

    ss.newLine() << ss.floatDecl("hwrap") << " = h;";
    ss.newLine() << "hwrap = hwrap - floor(hwrap / 360.0) * 360.0;";
    ss.newLine() << "hwrap = (hwrap < 0.0) ? hwrap + 360.0 : hwrap;";

    ss.newLine() << ss.floatDecl("i_lo") << " = floor(hwrap);";
    ss.newLine() << ss.floatDecl("i_hi") << " = (i_lo + 1);";
    ss.newLine() << "i_hi = i_hi - floor(i_hi / 360.0) * 360.0;";

    if (dimensions == GpuShaderDesc::TEXTURE_1D)
    {
        ss.newLine() << ss.floatDecl("lo") << " = " << ss.sampleTex1D(name, "(i_lo + 0.5) / 360.0") << ".r;";
        ss.newLine() << ss.floatDecl("hi") << " = " << ss.sampleTex1D(name, "(i_hi + 0.5) / 360.0") << ".r;";
    }
    else
    {
        ss.newLine() << ss.floatDecl("lo") << " = " << ss.sampleTex2D(name, ss.float2Const("(i_lo + 0.5) / 360.0", "0.0")) << ".r;";
        ss.newLine() << ss.floatDecl("hi") << " = " << ss.sampleTex2D(name, ss.float2Const("(i_hi + 0.5) / 360.0", "0.5")) << ".r;";
    }

    ss.newLine() << ss.floatDecl("t") << " = (h - i_lo) / (i_hi - i_lo);";
    ss.newLine() << "return " << ss.lerp("lo", "hi", "t") << ";";

    ss.dedent();
    ss.newLine() << "}";

    shaderCreator->addToHelperShaderCode(ss.string().c_str());

    return name;
}

std::string _Add_Toe_func(
    GpuShaderCreatorRcPtr & shaderCreator,
    unsigned resourceIndex,
    bool invert)
{
    // Reserve name
    std::ostringstream resName;
    resName << shaderCreator->getResourcePrefix()
            << std::string("_")
            << std::string("toe")
            << (invert ? std::string("_inv") : std::string("_fwd"))
            << resourceIndex;

    // Note: Remove potentially problematic double underscores from GLSL resource names.
    std::string name(resName.str());
    StringUtils::ReplaceInPlace(name, "__", "_");

    GpuShaderText ss(shaderCreator->getLanguage());

    ss.newLine() << ss.floatKeyword() << " " << name << "(float x, float limit, float k1_in, float k2_in)";
    ss.newLine() << "{";
    ss.indent();

    ss.newLine() << ss.floatDecl("k2") << " = max(k2_in, 0.001);";
    ss.newLine() << ss.floatDecl("k1") << " = sqrt(k1_in * k1_in + k2 * k2);";
    ss.newLine() << ss.floatDecl("k3") << " = (limit + k1) / (limit + k2);";

    if (invert)
    {
        ss.newLine() << "return (x > limit) ? x : (x * x + k1 * x) / (k3 * (x + k2));";
    }
    else
    {
        ss.newLine() << "return (x > limit) ? x : 0.5 * (k3 * x - k1 + sqrt((k3 * x - k1) * (k3 * x - k1) + 4.0 * k2 * k3 * x));";
    }

    ss.dedent();
    ss.newLine() << "}";

    shaderCreator->addToHelperShaderCode(ss.string().c_str());

    return name;
}

void _Add_Tonescale_Compress_Fwd_Shader(
    GpuShaderCreatorRcPtr & shaderCreator,
    GpuShaderText & ss,
    unsigned resourceIndex,
    const ACES2::JMhParams & p,
    const ACES2::ToneScaleParams & t,
    const ACES2::ChromaCompressParams & c,
    const std::string & reachName)
{

#ifdef TONESCALE_LUT

    static constexpr float tonescaleJ[1024] = {0.0, 0.007508731158294459, 0.03699081563480184, 0.09396498326932942, 0.1819120429406957, 0.30334639597195356, 0.46011529590862027, 0.6535290046906179, 0.8844306316120482, 1.153241170180272, 1.4599944502191637, 1.8043688747665063, 2.185719232593326, 2.603110002437569, 3.0553504873526682, 3.541031473416213, 4.058562733835746, 4.60621051420448, 5.182134086687146, 5.784420513069797, 6.411116877859099, 7.060259415886554, 7.729899140889026, 8.418123763310884, 9.123075852797966, 9.842967343947356, 10.576090597622159, 11.32082631301251, 12.075648638940612, 12.839127859930963, 13.609931037681452, 14.386820976586193, 15.168653857596762, 15.95437585223719, 16.743018991639527, 17.533696526947303, 18.325597979531967, 19.117984043724597, 19.910181472165213, 20.701578044954108, 21.49161769874918, 22.279795870739, 23.065655094814606, 23.848780872943077, 24.628797833338474, 25.40536617814168, 26.178178416576927, 26.946956374593416, 27.71144846850211, 28.471427227794976, 29.226687050943024, 29.977042177299683, 30.722324858116796, 31.462383709967728, 32.19708223445189, 32.92629748883481, 33.64991889318676, 34.36784716056481, 35.079993337794036, 35.7862779454166, 36.486630206362484, 37.180987353846106, 37.869294009888186, 38.551501626705615, 39.22756798399117, 39.89745673582709, 40.561137001635004, 41.218582996167626, 41.869773694093524, 42.51469252522001, 43.15332709684442, 43.7856689401244, 44.411713277715705, 45.03145881024677, 45.64490751948527, 46.252064486306814, 46.85293772180169, 47.447538010057166, 48.03587876133076, 48.61797587448725, 49.19384760771213, 49.763514456636, 50.32699903911433, 50.88432598600067, 51.435521837336964, 51.98061494345668, 52.51963537056209, 53.05261481039295, 53.5795864936538, 54.10058510690933, 54.615646712695906, 55.12480867262899, 55.6281095733155, 56.125589154904, 56.61728824212749, 57.103248677711264, 57.583513258034756, 58.05812567094931, 58.52713043566576, 58.9905728446354, 59.44849890735642, 59.900955296044714, 60.34798929311472, 60.78964874041991, 61.225981990207664, 61.657037857746076, 62.08286557558387, 62.503514749405845, 62.919035315449534, 63.32947749944889, 63.73489177707333, 64.13532883583001, 64.53083953839972, 64.92147488737561, 65.30728599137508, 65.68832403249625, 66.06464023508896, 66.43628583581189, 66.803312054947, 67.16577006894212, 67.52371098415301, 67.87718581175655, 68.22624544380591, 68.57094063039924, 68.91132195793354, 69.24743982841474, 69.57934443979592, 69.90708576731544, 70.23071354580644, 70.55027725295022, 70.86582609344552, 71.17740898406586, 71.48507453957832, 71.78887105949624, 72.08884651563913, 72.38504854047387, 72.67752441621057, 72.966321064628, 73.25148503760305, 73.53306250831932, 73.81109926313081, 74.0856406940564, 74.35673179188153, 74.62441713984477, 74.88874090788593, 75.14974684743395, 75.40747828671329, 75.66197812654748, 75.91328883663934, 76.16145245230811, 76.40651057166357, 76.64850435319876, 76.88747451378194, 77.12346132703117, 77.35650462205274, 77.5866437825274, 77.81391774612757, 78.03836500425004, 78.26002360204811, 78.47893113874905, 78.69512476824174, 78.90864119992077, 79.11951669977408, 79.3277870917001, 79.5334877590425, 79.73665364633052, 79.93731926121265, 80.13551867657284, 80.3312855328184, 80.52465304032881, 80.71565398205574, 80.90432071626446, 81.09068517940766, 81.27477888912217, 81.45663294734071, 81.63627804351022, 81.81374445790863, 81.98906206505298, 82.16226033719161, 82.33336834787322, 82.50241477558635, 82.6694279074635, 82.83443564304281, 82.9974654980825, 83.1585446084221, 83.31769973388545, 83.4749572622204, 83.63034321307009, 83.78388324197229, 83.93560264438142, 84.08552635970956, 84.23367897538292, 84.38008473090957, 84.52476752195521, 84.66775090442346, 84.80905809853797, 84.94871199292284, 85.08673514867887, 85.22314980345293, 85.35797787549791, 85.49124096772108, 85.62296037171818, 85.75315707179193, 85.88185174895199, 86.00906478489559, 86.1348162659663, 86.25912598708982, 86.38201345568508, 86.50349789554906, 86.62359825071464, 86.74233318927975, 86.85972110720648, 86.9757801320902, 87.0905281268963, 87.20398269366476, 87.31616117718137, 87.4270806686147, 87.5367580091186, 87.645209793399, 87.75245237324523, 87.85850186102445, 87.96337413313992, 88.0670848334516, 88.16964937665944, 88.27108295164889, 88.37140052479835, 88.47061684324851, 88.56874643813308, 88.66580362777127, 88.76180252082182, 88.85675701939782, 88.95068082214378, 89.04358742727318, 89.13549013556823, 89.22640205334054, 89.31633609535375, 89.40530498770762, 89.49332127068419, 89.58039730155572, 89.6665452573551, 89.75177713760812, 89.83610476702907, 89.9195397981785, 90.00209371408448, 90.08377783082696, 90.16460330008567, 90.24458111165235, 90.3237220959065, 90.40203692625603, 90.47953612154275, 90.5562300484126, 90.63212892365166, 90.70724281648774, 90.78158165085823, 90.8551552076442, 90.92797312687154, 91.00004490987916, 91.07137992145455, 91.14198739193765, 91.21187641929247, 91.28105597114772, 91.34953488680625, 91.41732187922398, 91.48442553695837, 91.55085432608716, 91.61661659209757, 91.68172056174652, 91.74617434489144, 91.8099859362939, 91.87316321739384, 91.93571395805718, 91.99764581829567, 92.05896634996012, 92.11968299840674, 92.17980310413775, 92.23933390441572, 92.29828253485303, 92.3566560309757, 92.41446132976282, 92.47170527116161, 92.52839459957836, 92.58453596534581, 92.640135926167, 92.69520094853658, 92.74973740913897, 92.80375159622474, 92.85724971096447, 92.91023786878165, 92.96272210066365, 93.01470835445214, 93.06620249611261, 93.11721031098381, 93.1677375050068, 93.21778970593472, 93.26737246452271, 93.31649125569918, 93.36515147971785, 93.41335846329163, 93.46111746070795, 93.5084336549265, 93.55531215865909, 93.60175801543228, 93.64777620063266, 93.69337162253572, 93.73854912331795, 93.78331348005247, 93.82766940568908, 93.87162155001822, 93.91517450061939, 93.9583327837947, 94.00110086548693, 94.0434831521831, 94.08548399180357, 94.12710767457658, 94.168358433899, 94.20924044718328, 94.24975783669036, 94.28991467034989, 94.32971496256668, 94.36916267501458, 94.4082617174175, 94.44701594831797, 94.48542917583332, 94.52350515839963, 94.56124760550442, 94.59866017840656, 94.6357464908456, 94.67251010973942, 94.70895455587055, 94.74508330456202, 94.78089978634186, 94.81640738759752, 94.85160945121967, 94.88650927723556, 94.92111012343268, 94.95541520597224, 94.9894276999931, 95.0231507402059, 95.05658742147803, 95.08974079940909, 95.1226138908974, 95.15520967469759, 95.18753109196925, 95.21958104681697, 95.2513624068219, 95.28287800356489, 95.31413063314146, 95.34512305666861, 95.3758580007837, 95.40633815813545, 95.43656618786738, 95.46654471609352, 95.49627633636668, 95.52576361013962, 95.55500906721865, 95.58401520621067, 95.61278449496255, 95.64131937099441, 95.66962224192554, 95.6976954858942, 95.72554145197046, 95.75316246056313, 95.78056080381984, 95.80773874602144, 95.83469852396995, 95.86144234737071, 95.88797239920865, 95.9142908361185, 95.94039978874983, 95.96630136212583, 95.99199763599722, 96.01749066519025, 96.04278247994958, 96.0678750862761, 96.0927704662591, 96.11747057840392, 96.14197735795422, 96.16629271720956, 96.19041854583806, 96.2143567111844, 96.23810905857297, 96.2616774116067, 96.28506357246115, 96.3082693221742, 96.33129642093155, 96.35414660834769, 96.37682160374271, 96.39932310641508, 96.42165279591009, 96.44381233228468, 96.46580335636776, 96.48762749001703, 96.50928633637193, 96.53078148010275, 96.5521144876559, 96.57328690749584, 96.59430027034315, 96.61515608940923, 96.63585586062744, 96.65640106288083, 96.67679315822669, 96.69703359211726, 96.71712379361806, 96.73706517562208, 96.75685913506152, 96.77650705311608, 96.79601029541821, 96.81537021225559, 96.83458813877034, 96.85366539515556, 96.87260328684894, 96.89140310472352, 96.91006612527568, 96.92859361081044, 96.94698680962416, 96.96524695618433, 96.98337527130708, 97.0013729623319, 97.01924122329402, 97.03698123509407, 97.05459416566573, 97.07208117014041, 97.08944339101, 97.10668195828717, 97.12379798966337, 97.14079259066457, 97.15766685480473, 97.17442186373727, 97.19105868740418, 97.20757838418308, 97.22398200103237, 97.24027057363406, 97.25644512653467, 97.27250667328438, 97.28845621657378, 97.30429474836916, 97.3200232500455, 97.33564269251788, 97.351154036371, 97.36655823198672, 97.38185621967004, 97.39704892977312, 97.41213728281788, 97.42712218961648, 97.44200455139051, 97.45678525988825, 97.47146519750046, 97.4860452373747, 97.50052624352762, 97.51490907095626, 97.52919456574737, 97.54338356518555, 97.55747689785966, 97.57147538376799, 97.58537983442177, 97.59919105294756, 97.61290983418782, 97.62653696480068, 97.6400732233577, 97.65351938044093, 97.66687619873814, 97.68014443313713, 97.69332483081857, 97.70641813134762, 97.71942506676442, 97.7323463616732, 97.74518273333027, 97.75793489173083, 97.7706035396947, 97.78318937295076, 97.79569308022033, 97.80811534329943, 97.82045683713999, 97.83271822992985, 97.84490018317197, 97.85700335176212, 97.86902838406618, 97.88097592199576, 97.89284660108333, 97.90464105055608, 97.91635989340888, 97.92800374647635, 97.93957322050392, 97.9510689202179, 97.9624914443947, 97.97384138592926, 97.98511933190242, 97.99632586364739, 98.00746155681544, 98.01852698144093, 98.02952270200502, 98.04044927749901, 98.05130726148661, 98.06209720216549, 98.0728196424281, 98.08347511992145, 98.09406416710658, 98.10458731131672, 98.11504507481514, 98.12543797485203, 98.13576652372082, 98.14603122881351, 98.15623259267558, 98.16637111306011, 98.17644728298121, 98.18646159076653, 98.19641452010968, 98.20630655012138, 98.21613815538026, 98.22590980598311, 98.23562196759417, 98.24527510149423, 98.25486966462867, 98.26440610965523, 98.27388488499106, 98.28330643485906, 98.29267119933395, 98.30197961438752, 98.31123211193325, 98.32042911987072, 98.32957106212908, 98.33865835871018, 98.3476914257313, 98.356670675467, 98.36559651639071, 98.37446935321577, 98.383289586936, 98.39205761486555, 98.40077383067852, 98.40943862444807, 98.41805238268486, 98.42661548837518, 98.43512832101864, 98.44359125666523, 98.4520046679521, 98.4603689241398, 98.46868439114822, 98.47695143159179, 98.48517040481462, 98.49334166692499, 98.5014655708295, 98.50954246626668, 98.51757269984046, 98.52555661505305, 98.53349455233734, 98.54138684908919, 98.54923383969918, 98.5570358555837, 98.56479322521639, 98.57250627415836, 98.5801753250887, 98.58780069783431, 98.59538270939944, 98.60292167399489, 98.61041790306689, 98.61787170532563, 98.62528338677323, 98.63265325073205, 98.63998159787172, 98.64726872623652, 98.6545149312725, 98.6617205058536, 98.66888574030816, 98.67601092244496, 98.68309633757863, 98.69014226855519, 98.69714899577694, 98.70411679722739, 98.71104594849555, 98.71793672280035, 98.72478939101434, 98.73160422168739, 98.73838148107019, 98.74512143313716, 98.7518243396094, 98.75849045997714, 98.76512005152217, 98.77171336933995, 98.77827066636118, 98.78479219337356, 98.79127819904303, 98.79772892993483, 98.80414463053438, 98.81052554326779, 98.81687190852226, 98.82318396466626, 98.82946194806932, 98.83570609312183, 98.84191663225434, 98.84809379595704, 98.85423781279846, 98.86034890944465, 98.86642731067737, 98.87247323941277, 98.87848691671955, 98.8844685618368, 98.89041839219188, 98.89633662341798, 98.90222346937149, 98.90807914214928, 98.9139038521055, 98.91969780786873, 98.92546121635817, 98.93119428280053, 98.93689721074601, 98.94257020208448, 98.94821345706131, 98.95382717429337, 98.95941155078411, 98.96496678193948, 98.97049306158281, 98.97599058197001, 98.98145953380431, 98.98690010625128, 98.99231248695311, 98.99769686204316, 99.00305341616021, 99.0083823324626, 99.01368379264201, 99.01895797693753, 99.02420506414906, 99.02942523165092, 99.0346186554053, 99.03978550997523, 99.04492596853794, 99.05004020289758, 99.05512838349819, 99.06019067943616, 99.06522725847289, 99.07023828704715, 99.07522393028738, 99.0801843520237, 99.08511971480006, 99.09003017988601, 99.09491590728841, 99.09977705576327, 99.10461378282697, 99.10942624476777, 99.1142145966571, 99.11897899236075, 99.12371958454969, 99.1284365247111, 99.13312996315929, 99.13780004904615, 99.14244693037179, 99.1470707539952, 99.15167166564423, 99.15624980992624, 99.16080533033794, 99.16533836927543, 99.1698490680444, 99.17433756686965, 99.17880400490496, 99.1832485202427, 99.18767124992334, 99.19207232994486, 99.1964518952721, 99.20081007984601, 99.20514701659275, 99.20946283743278, 99.21375767328972, 99.21803165409938, 99.22228490881831, 99.22651756543269, 99.2307297509668, 99.23492159149153, 99.23909321213294, 99.24324473708029, 99.2473762895947, 99.25148799201686, 99.25557996577555, 99.2596523313954, 99.26370520850477, 99.26773871584379, 99.27175297127191, 99.27574809177575, 99.27972419347665, 99.2836813916382, 99.28761980067361, 99.2915395341533, 99.295440704812, 99.29932342455614, 99.30318780447087, 99.30703395482732, 99.31086198508945, 99.3146720039212, 99.31846411919311, 99.32223843798945, 99.3259950666147, 99.3297341106004, 99.33345567471162, 99.33715986295373, 99.3408467785787, 99.34451652409156, 99.34816920125684, 99.35180491110472, 99.35542375393752, 99.35902582933551, 99.36261123616342, 99.3661800725762, 99.36973243602519, 99.37326842326402, 99.37678813035434, 99.38029165267196, 99.38377908491232, 99.38725052109632, 99.39070605457604, 99.39414577804018, 99.39756978351988, 99.40097816239388, 99.40437100539418, 99.40774840261149, 99.4111104435003, 99.41445721688443, 99.4177888109622, 99.42110531331141, 99.42440681089487, 99.42769339006522, 99.43096513656987, 99.43422213555641, 99.43746447157714, 99.44069222859414, 99.44390548998415, 99.44710433854326, 99.45028885649181, 99.45345912547914, 99.45661522658797, 99.45975724033939, 99.46288524669721, 99.46599932507259, 99.46909955432855, 99.47218601278436, 99.47525877822002, 99.47831792788061, 99.48136353848066, 99.48439568620836, 99.4874144467299, 99.49041989519364, 99.49341210623427, 99.49639115397703, 99.49935711204164, 99.50231005354664, 99.50525005111304, 99.5081771768687, 99.51109150245192, 99.51399309901562, 99.51688203723108, 99.51975838729183, 99.5226222189174, 99.5254736013572, 99.52831260339413, 99.53113929334837, 99.53395373908103, 99.53675600799771, 99.53954616705222, 99.54232428275012, 99.5450904211522, 99.54784464787795, 99.55058702810925, 99.55331762659353, 99.55603650764738, 99.55874373515982, 99.5614393725957, 99.56412348299905, 99.56679612899627, 99.56945737279949, 99.57210727620972, 99.57474590062019, 99.57737330701929, 99.57998955599385, 99.58259470773235, 99.58518882202793, 99.5877719582813, 99.59034417550394, 99.59290553232118, 99.59545608697495, 99.59799589732692, 99.60052502086138, 99.60304351468801, 99.605551435545, 99.60804883980167, 99.61053578346146, 99.61301232216474, 99.61547851119131, 99.6179344054635, 99.62038005954868, 99.62281552766211, 99.62524086366953, 99.62765612108969, 99.6300613530973, 99.63245661252535, 99.63484195186786, 99.63721742328235, 99.63958307859245, 99.64193896929044, 99.6442851465396, 99.64662166117688, 99.6489485637152, 99.65126590434599, 99.65357373294158, 99.65587209905745, 99.6581610519349, 99.66044064050308, 99.66271091338157, 99.66497191888253, 99.66722370501301, 99.66946631947738, 99.67169980967934, 99.67392422272431, 99.67613960542162, 99.67834600428671, 99.68054346554321, 99.68273203512527, 99.68491175867955, 99.68708268156735, 99.68924484886685, 99.69139830537505, 99.69354309560985, 99.69567926381224, 99.69780685394811, 99.69992590971049, 99.70203647452136, 99.70413859153375, 99.70623230363368, 99.70831765344207, 99.71039468331678, 99.71246343535428, 99.71452395139185, 99.71657627300934, 99.7186204415309, 99.72065649802711, 99.7226844833166, 99.72470443796777, 99.72671640230111, 99.72872041639036, 99.7307165200647, 99.73270475291031, 99.73468515427231, 99.73665776325622, 99.73862261872988, 99.74057975932519, 99.74252922343963, 99.74447104923807, 99.74640527465432, 99.74833193739289, 99.75025107493056, 99.7521627245181, 99.75406692318163, 99.75596370772455, 99.75785311472887, 99.75973518055685, 99.76160994135267, 99.76347743304376, 99.76533769134251, 99.76719075174768, 99.76903664954601, 99.7708754198136, 99.77270709741751, 99.7745317170172, 99.77634931306572, 99.7781599198117, 99.77996357130033, 99.78176030137486, 99.78355014367818, 99.78533313165417, 99.78710929854896, 99.78887867741234, 99.79064130109933, 99.79239720227123, 99.7941464133972, 99.79588896675548, 99.7976248944348, 99.79935422833555, 99.80107700017122, 99.80279324146962, 99.80450298357422, 99.80620625764536, 99.80790309466157, 99.80959352542071, 99.81127758054139, 99.81295529046403, 99.81462668545218, 99.81629179559371, 99.817950650802, 99.81960328081719, 99.8212497152072, 99.82288998336917, 99.82452411453039, 99.82615213774953, 99.82777408191782, 99.82938997576021, 99.83099984783644, 99.83260372654212, 99.83420164010998, 99.83579361661089, 99.83737968395482, 99.83895986989225, 99.84053420201501, 99.84210270775729, 99.84366541439694, 99.84522234905637, 99.84677353870353, 99.84831901015315, 99.84985879006759, 99.85139290495792, 99.85292138118498, 99.85444424496029, 99.85596152234714, 99.85747323926151, 99.85897942147307, 99.86048009460615, 99.86197528414084, 99.86346501541364, 99.86494931361877, 99.8664282038088, 99.86790171089585, 99.86936985965234, 99.87083267471199, 99.87229018057073, 99.8737424015876, 99.87518936198568, 99.87663108585284, 99.8780675971429, 99.87949891967632, 99.88092507714107, 99.88234609309352, 99.88376199095941, 99.8851727940346, 99.88657852548592, 99.88797920835198, 99.88937486554418, 99.8907655198473, 99.89215119392057, 99.89353191029826, 99.89490769139059, 99.8962785594846, 99.89764453674486, 99.89900564521427, 99.90036190681491, 99.90171334334875, 99.90305997649844, 99.90440182782817, 99.90573891878427, 99.90707127069611, 99.90839890477679, 99.90972184212389, 99.91104010372031, 99.91235371043476, 99.91366268302274, 99.91496704212717, 99.91626680827913, 99.91756200189853, 99.91885264329487, 99.9201387526679, 99.92142035010832, 99.92269745559858, 99.92397008901345, 99.92523827012073, 99.9265020185819, 99.9277613539529, 99.92901629568472, 99.93026686312399, 99.93151307551389, 99.9327549519945, 99.93399251160365, 99.9352257732775, 99.93645475585112, 99.93767947805935, 99.9388999585371, 99.94011621582027, 99.94132826834621, 99.94253613445433, 99.94373983238683, 99.94493938028917, 99.94613479621088, 99.94732609810588, 99.94851330383328, 99.94969643115795, 99.95087549775093, 99.95205052119032, 99.95322151896163, 99.95438850845836, 99.95555150698269, 99.95671053174595, 99.95786559986922, 99.95901672838389, 99.96016393423221, 99.96130723426785, 99.96244664525636, 99.96358218387586, 99.96471386671747, 99.96584171028594, 99.96696573099999, 99.96808594519312, 99.9692023691139, 99.97031501892651, 99.9714239107115, 99.97252906046594, 99.97363048410419, 99.97472819745833, 99.97582221627862, 99.97691255623408, 99.97799923291286, 99.97908226182291, 99.9801616583923, 99.98123743796985, 99.98230961582541, 99.98337820715057, 99.984443227059, 99.98550469058691, 99.98656261269358, 99.98761700826182, 99.98866789209839, 99.98971527893448, 99.99075918342618, 99.99179962015488, 99.9928366036278, 99.99387014827835, 99.99490026846665, 99.9959269784799, 99.99695029253289, 99.99797022476832, 99.99898678925734, 100.0};

    // Reserve name
    std::ostringstream resNameTonescale;
    resNameTonescale << shaderCreator->getResourcePrefix()
            << std::string("_")
            << std::string("tonescale_lut_")
            << resourceIndex;

    // Note: Remove potentially problematic double underscores from GLSL resource names.
    std::string nameTonescale(resNameTonescale.str());
    StringUtils::ReplaceInPlace(nameTonescale, "__", "_");

    // Register texture
    GpuShaderDesc::TextureDimensions dimensionsIndex = GpuShaderDesc::TEXTURE_1D;
    if (shaderCreator->getLanguage() == GPU_LANGUAGE_GLSL_ES_1_0
        || shaderCreator->getLanguage() == GPU_LANGUAGE_GLSL_ES_3_0
        || !shaderCreator->getAllowTexture1D())
    {
        dimensionsIndex = GpuShaderDesc::TEXTURE_2D;
    }

    shaderCreator->addTexture(
        nameTonescale.c_str(),
        GpuShaderText::getSamplerName(nameTonescale).c_str(),
        1024,
        1,
        GpuShaderCreator::TEXTURE_RED_CHANNEL,
        dimensionsIndex,
        INTERP_NEAREST,
        tonescaleJ);

    if (dimensionsIndex == GpuShaderDesc::TEXTURE_1D)
    {
        GpuShaderText ss(shaderCreator->getLanguage());
        ss.declareTex1D(nameTonescale);
        shaderCreator->addToDeclareShaderCode(ss.string().c_str());
    }
    else
    {
        GpuShaderText ss(shaderCreator->getLanguage());
        ss.declareTex2D(nameTonescale);
        shaderCreator->addToDeclareShaderCode(ss.string().c_str());
    }

#endif


    std::string toeName = _Add_Toe_func(shaderCreator, resourceIndex, false);

    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.floatDecl("J") << " = " << pxl << ".r;";
    ss.newLine() << ss.floatDecl("M") << " = " << pxl << ".g;";
    ss.newLine() << ss.floatDecl("h") << " = " << pxl << ".b;";

#ifdef TONESCALE_LUT

    ss.newLine() << ss.intDecl("lut_size") << " = 1024;";
    ss.newLine() << ss.floatDecl("Jmax") << " = 812.3;";
    ss.newLine() << ss.floatDecl("lut_pos") << " = (J / Jmax) * (lut_size - 1);";
    ss.newLine() << ss.intDecl("lut_lo") << " = int(lut_pos);";
    ss.newLine() << ss.intDecl("lut_hi") << " = lut_lo + 1;";
    ss.newLine() << ss.floatDecl("f") << " = lut_pos - lut_lo;";

    if (dimensionsIndex == GpuShaderDesc::TEXTURE_1D)
    {
        ss.newLine() << ss.floatDecl("lo") << " = " << ss.sampleTex1D(nameTonescale, std::string("(lut_lo + 0.5) / ") + std::to_string(1024)) << ".r;";
        ss.newLine() << ss.floatDecl("hi") << " = " << ss.sampleTex1D(nameTonescale, std::string("(lut_hi + 0.5) / ") + std::to_string(1024)) << ".r;";
    }
    else
    {
        ss.newLine() << ss.floatDecl("lo") << " = " << ss.sampleTex2D(nameTonescale, ss.float2Const(std::string("(lut_lo + 0.5) / ") + std::to_string(1024), "0.5")) << ".r;";
        ss.newLine() << ss.floatDecl("hi") << " = " << ss.sampleTex2D(nameTonescale, ss.float2Const(std::string("(lut_hi + 0.5) / ") + std::to_string(1024), "0.5")) << ".r;";
    }

    ss.newLine() << ss.floatDecl("J_ts") << " = " << ss.lerp("lo", "hi", "f") << ";";

#else

    // Tonescale applied in Y (convert to and from J)
    ss.newLine() << ss.floatDecl("A") << " = " << p.A_w_J << " * pow(abs(J) / 100.0, 1.0 / (" << ACES2::surround[1] << " * " << p.z << "));";
    ss.newLine() << ss.floatDecl("Y") << " = sign(J) * 100.0 / " << p.F_L << " * pow((27.13 * A) / (400.0 - A), 1.0 / 0.42) / 100.0;";

    ss.newLine() << ss.floatDecl("f") << " = " << t.m_2  << " * pow(max(0.0, Y) / (Y + " << t.s_2 << "), " << t.g << ");";
    ss.newLine() << ss.floatDecl("Y_ts") << " = max(0.0, f * f / (f + " << t.t_1 << ")) * " << t.n_r << ";";

    ss.newLine() << ss.floatDecl("F_L_Y") << " = pow(" << p.F_L << " * abs(Y_ts) / 100.0, 0.42);";
    ss.newLine() << ss.floatDecl("J_ts") << " = sign(Y_ts) * 100.0 * pow(((400.0 * F_L_Y) / (27.13 + F_L_Y)) / " << p.A_w_J << ", " << ACES2::surround[1] << " * " << p.z << ");";

#endif

    // ChromaCompress
    ss.newLine() << ss.floatDecl("M_cp") << " = M;";

    ss.newLine() << "if (M != 0.0)";
    ss.newLine() << "{";
    ss.indent();

    ss.newLine() << ss.floatDecl("nJ") << " = J_ts / " << c.limit_J_max << ";";
    ss.newLine() << ss.floatDecl("snJ") << " = max(0.0, 1.0 - nJ);";

    // Mnorm
    ss.newLine() << ss.floatDecl("Mnorm") << ";";
    ss.newLine() << "{";
    ss.indent();

    ss.newLine() << ss.floatDecl("PI") << " = 3.14159265358979;";
    ss.newLine() << ss.floatDecl("h_rad") << " = h / 180.0 * PI;";
    ss.newLine() << ss.floatDecl("a") << " = cos(h_rad);";
    ss.newLine() << ss.floatDecl("b") << " = sin(h_rad);";
    ss.newLine() << ss.floatDecl("cos_hr2") << " = a * a - b * b;";
    ss.newLine() << ss.floatDecl("sin_hr2") << " = 2.0 * a * b;";
    ss.newLine() << ss.floatDecl("cos_hr3") << " = 4.0 * a * a * a - 3.0 * a;";
    ss.newLine() << ss.floatDecl("sin_hr3") << " = 3.0 * b - 4.0 * b * b * b;";
    ss.newLine() << ss.floatDecl("M") << " = 11.34072 * a + 16.46899 * cos_hr2 + 7.88380 * cos_hr3 + 14.66441 * b + -6.37224 * sin_hr2 + 9.19364 * sin_hr3 + 77.12896;";
    ss.newLine() << "Mnorm = M * " << c.chroma_compress_scale << ";";

    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << ss.floatDecl("reachM") << " = " << reachName << "_sample(h);";
    ss.newLine() << ss.floatDecl("limit") << " = pow(nJ, " << c.model_gamma << ") * reachM / Mnorm;";
    ss.newLine() << "M_cp = M * pow(J_ts / J, " << c.model_gamma << ");";
    ss.newLine() << "M_cp = M_cp / Mnorm;";

    ss.newLine() << "M_cp = limit - " << toeName << "(limit - M_cp, limit - 0.001, snJ * " << c.sat << ", sqrt(nJ * nJ + " << c.sat_thr << "));";
    ss.newLine() << "M_cp = " << toeName << "(M_cp, limit, nJ * " << c.compr << ", snJ);";
    ss.newLine() << "M_cp = M_cp * Mnorm;";

    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << pxl << ".rgb = " << ss.float3Const("J_ts", "M_cp", "h") << ";";
}

void _Add_Tonescale_Compress_Inv_Shader(
    GpuShaderCreatorRcPtr & shaderCreator,
    GpuShaderText & ss,
    unsigned resourceIndex,
    const ACES2::JMhParams & p,
    const ACES2::ToneScaleParams & t,
    const ACES2::ChromaCompressParams & c,
    const std::string & reachName)
{
    std::string toeName = _Add_Toe_func(shaderCreator, resourceIndex, true);

    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.floatDecl("J_ts") << " = " << pxl << ".r;";
    ss.newLine() << ss.floatDecl("M_cp") << " = " << pxl << ".g;";
    ss.newLine() << ss.floatDecl("h") << " = " << pxl << ".b;";

    // Inverse Tonescale applied in Y (convert to and from J)
    ss.newLine() << ss.floatDecl("A") << " = " << p.A_w_J << " * pow(abs(J_ts) / 100.0, 1.0 / (" << ACES2::surround[1] << " * " << p.z << "));";
    ss.newLine() << ss.floatDecl("Y_ts") << " = sign(J_ts) * 100.0 / " << p.F_L << " * pow((27.13 * A) / (400.0 - A), 1.0 / 0.42) / 100.0;";

    ss.newLine() << ss.floatDecl("Z") << " = max(0.0, min(" << t.n << " / (" << t.u_2 * t.n_r << "), Y_ts));";
    ss.newLine() << ss.floatDecl("ht") << " = (Z + sqrt(Z * (4.0 * " << t.t_1 << " + Z))) / 2.0;";
    ss.newLine() << ss.floatDecl("Y") << " = " << t.s_2 << " / (pow((" << t.m_2 << " / ht), (1.0 / " << t.g << ")) - 1.0);";

    ss.newLine() << ss.floatDecl("F_L_Y") << " = pow(" << p.F_L << " * abs(Y * 100.0) / 100.0, 0.42);";
    ss.newLine() << ss.floatDecl("J") << " = sign(Y) * 100.0 * pow(((400.0 * F_L_Y) / (27.13 + F_L_Y)) / " << p.A_w_J << ", " << ACES2::surround[1] << " * " << p.z << ");";

    // ChromaCompress
    ss.newLine() << ss.floatDecl("M") << " = M_cp;";

    ss.newLine() << "if (M_cp != 0.0)";
    ss.newLine() << "{";
    ss.indent();

    ss.newLine() << ss.floatDecl("nJ") << " = J_ts / " << c.limit_J_max << ";";
    ss.newLine() << ss.floatDecl("snJ") << " = max(0.0, 1.0 - nJ);";

    // Mnorm
    ss.newLine() << ss.floatDecl("Mnorm") << ";";
    ss.newLine() << "{";
    ss.indent();

    ss.newLine() << ss.floatDecl("PI") << " = 3.14159265358979;";
    ss.newLine() << ss.floatDecl("h_rad") << " = h / 180.0 * PI;";
    ss.newLine() << ss.floatDecl("a") << " = cos(h_rad);";
    ss.newLine() << ss.floatDecl("b") << " = sin(h_rad);";
    ss.newLine() << ss.floatDecl("cos_hr2") << " = a * a - b * b;";
    ss.newLine() << ss.floatDecl("sin_hr2") << " = 2.0 * a * b;";
    ss.newLine() << ss.floatDecl("cos_hr3") << " = 4.0 * a * a * a - 3.0 * a;";
    ss.newLine() << ss.floatDecl("sin_hr3") << " = 3.0 * b - 4.0 * b * b * b;";
    ss.newLine() << ss.floatDecl("M") << " = 11.34072 * a + 16.46899 * cos_hr2 + 7.88380 * cos_hr3 + 14.66441 * b + -6.37224 * sin_hr2 + 9.19364 * sin_hr3 + 77.12896;";
    ss.newLine() << "Mnorm = M * " << c.chroma_compress_scale << ";";

    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << ss.floatDecl("reachM") << " = " << reachName << "_sample(h);";
    ss.newLine() << ss.floatDecl("limit") << " = pow(nJ, " << c.model_gamma << ") * reachM / Mnorm;";

    ss.newLine() << "M = M_cp / Mnorm;";
    ss.newLine() << "M = " << toeName << "(M, limit, nJ * " << c.compr << ", snJ);";
    ss.newLine() << "M = limit - " << toeName << "(limit - M, limit - 0.001, snJ * " << c.sat << ", sqrt(nJ * nJ + " << c.sat_thr << "));";
    ss.newLine() << "M = M * Mnorm;";
    ss.newLine() << "M = M * pow(J_ts / J, " << -c.model_gamma << ");";

    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << pxl << ".rgb = " << ss.float3Const("J", "M", "h") << ";";
}

std::string _Add_Cusp_table(
    GpuShaderCreatorRcPtr & shaderCreator,
    unsigned resourceIndex,
    const ACES2::GamutCompressParams & g)
{
    // Reserve name
    std::ostringstream resName;
    resName << shaderCreator->getResourcePrefix()
            << std::string("_")
            << std::string("gamut_cusp_table_")
            << resourceIndex;

    // Note: Remove potentially problematic double underscores from GLSL resource names.
    std::string name(resName.str());
    StringUtils::ReplaceInPlace(name, "__", "_");

    // Register texture
    GpuShaderDesc::TextureDimensions dimensions = GpuShaderDesc::TEXTURE_1D;
    if (shaderCreator->getLanguage() == GPU_LANGUAGE_GLSL_ES_1_0
        || shaderCreator->getLanguage() == GPU_LANGUAGE_GLSL_ES_3_0
        || !shaderCreator->getAllowTexture1D())
    {
        dimensions = GpuShaderDesc::TEXTURE_2D;
    }

    shaderCreator->addTexture(
        name.c_str(),
        GpuShaderText::getSamplerName(name).c_str(),
        g.gamut_cusp_table.total_size,
        1,
        GpuShaderCreator::TEXTURE_RGB_CHANNEL,
        dimensions,
        INTERP_NEAREST,
        &(g.gamut_cusp_table.table[0][0]));

    if (dimensions == GpuShaderDesc::TEXTURE_1D)
    {
        GpuShaderText ss(shaderCreator->getLanguage());
        ss.declareTex1D(name);
        shaderCreator->addToDeclareShaderCode(ss.string().c_str());
    }
    else
    {
        GpuShaderText ss(shaderCreator->getLanguage());
        ss.declareTex2D(name);
        shaderCreator->addToDeclareShaderCode(ss.string().c_str());
    }

#ifdef NEW_CUSP_SAMPLING

    // Reserve name
    std::ostringstream resNameIndex;
    resNameIndex << shaderCreator->getResourcePrefix()
            << std::string("_")
            << std::string("gamut_cusp_table_hues_")
            << resourceIndex;

    // Note: Remove potentially problematic double underscores from GLSL resource names.
    std::string nameIndex(resNameIndex.str());
    StringUtils::ReplaceInPlace(nameIndex, "__", "_");

    // Register texture
    GpuShaderDesc::TextureDimensions dimensionsIndex = GpuShaderDesc::TEXTURE_1D;
    if (shaderCreator->getLanguage() == GPU_LANGUAGE_GLSL_ES_1_0
        || shaderCreator->getLanguage() == GPU_LANGUAGE_GLSL_ES_3_0
        || !shaderCreator->getAllowTexture1D())
    {
        dimensionsIndex = GpuShaderDesc::TEXTURE_2D;
    }

    shaderCreator->addTexture(
        nameIndex.c_str(),
        GpuShaderText::getSamplerName(nameIndex).c_str(),
        g.gamut_cusp_index_table.total_size,
        1,
        GpuShaderCreator::TEXTURE_RED_CHANNEL,
        dimensionsIndex,
        INTERP_NEAREST,
        &(g.gamut_cusp_index_table.table[0]));

    if (dimensionsIndex == GpuShaderDesc::TEXTURE_1D)
    {
        GpuShaderText ss(shaderCreator->getLanguage());
        ss.declareTex1D(nameIndex);
        shaderCreator->addToDeclareShaderCode(ss.string().c_str());
    }
    else
    {
        GpuShaderText ss(shaderCreator->getLanguage());
        ss.declareTex2D(nameIndex);
        shaderCreator->addToDeclareShaderCode(ss.string().c_str());
    }

#endif

    // Sampler function
    GpuShaderText ss(shaderCreator->getLanguage());

#ifndef NEW_CUSP_SAMPLING
    const std::string hues_array_name = name + "_hues_array";

    std::vector<float> hues_array(g.gamut_cusp_table.total_size);
    for (int i = 0; i < g.gamut_cusp_table.total_size; ++i)
    {
        hues_array[i] = g.gamut_cusp_table.table[i][2];
    }
    ss.declareFloatArrayConst(hues_array_name, (int) hues_array.size(), hues_array.data());
#endif

    ss.newLine() << ss.float2Keyword() << " " << name << "_sample(float h)";
    ss.newLine() << "{";
    ss.indent();

#ifdef NEW_CUSP_SAMPLING

    ss.newLine() << ss.floatDecl("lut_h_min") << " = " << g.gamut_cusp_index_table.start << ";";
    ss.newLine() << ss.floatDecl("lut_h_max") << " = " << g.gamut_cusp_index_table.end << ";";
    ss.newLine() << ss.floatDecl("lut_h_range") << " = lut_h_max - lut_h_min;";
    ss.newLine() << ss.floatDecl("lut_h") << " = ((h / 360.0) - lut_h_min) / lut_h_range;";
    ss.newLine() << ss.floatDecl("f_lo") << " = lut_h * (" << (g.gamut_cusp_index_table.total_size - 1) << ");";

    ss.newLine() << ss.intDecl("ii_lo") << " = " << ss.intKeyword() << "(f_lo);";
    ss.newLine() << ss.intDecl("ii_hi") << " = ii_lo + 1;";
    ss.newLine() << ss.floatDecl("f") << " = f_lo - " << ss.intKeyword() << "(f_lo);";

    if (dimensionsIndex == GpuShaderDesc::TEXTURE_1D)
    {
        ss.newLine() << ss.floatDecl("loo") << " = " << ss.sampleTex1D(nameIndex, std::string("(ii_lo + 0.5) / ") + std::to_string(g.gamut_cusp_index_table.total_size)) << ".r;";
        ss.newLine() << ss.floatDecl("hii") << " = " << ss.sampleTex1D(nameIndex, std::string("(ii_hi + 0.5) / ") + std::to_string(g.gamut_cusp_index_table.total_size)) << ".r;";
    }
    else
    {
        ss.newLine() << ss.floatDecl("loo") << " = " << ss.sampleTex2D(nameIndex, ss.float2Const(std::string("(ii_lo + 0.5) / ") + std::to_string(g.gamut_cusp_index_table.total_size), "0.5")) << ".r;";
        ss.newLine() << ss.floatDecl("hii") << " = " << ss.sampleTex2D(nameIndex, ss.float2Const(std::string("(ii_hi + 0.5) / ") + std::to_string(g.gamut_cusp_index_table.total_size), "0.5")) << ".r;";
    }

    ss.newLine() << ss.intDecl("i_lo") << " = " << ss.intKeyword() << "(" << ss.lerp("loo", "hii", "f") << " * (" << g.gamut_cusp_table.total_size - 1 << "));";
    ss.newLine() << ss.intDecl("i_hi") << " = i_lo + 1;";

#else

    ss.newLine() << ss.floatDecl("i_lo") << " = 0;";
    ss.newLine() << ss.floatDecl("i_hi") << " = " << g.gamut_cusp_table.base_index + g.gamut_cusp_table.size << ";";

    ss.newLine() << ss.floatDecl("hwrap") << " = h;";
    ss.newLine() << "hwrap = hwrap - floor(hwrap / 360.0) * 360.0;";
    ss.newLine() << "hwrap = (hwrap < 0.0) ? hwrap + 360.0 : hwrap;";
    ss.newLine() << ss.intDecl("i") << " = " << ss.intKeyword() << "(hwrap + " << g.gamut_cusp_table.base_index << ");";

    ss.newLine() << "while (i_lo + 1 < i_hi)";
    ss.newLine() << "{";
    ss.indent();

    ss.newLine() << ss.floatDecl("hcur") << " = " << hues_array_name << "[i];";

    ss.newLine() << "if (h > hcur)";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "i_lo = i;";
    ss.dedent();
    ss.newLine() << "}";
    ss.newLine() << "else";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "i_hi = i;";
    ss.dedent();
    ss.newLine() << "}";
    ss.newLine() << "i = " << ss.intKeyword() << "((i_lo + i_hi) / 2.0);";

    ss.dedent();
    ss.newLine() << "}";

#endif

    if (dimensions == GpuShaderDesc::TEXTURE_1D)
    {
        ss.newLine() << ss.float3Decl("lo") << " = " << ss.sampleTex1D(name, std::string("(i_hi - 1 + 0.5) / ") + std::to_string(g.gamut_cusp_table.total_size)) << ".rgb;";
        ss.newLine() << ss.float3Decl("hi") << " = " << ss.sampleTex1D(name, std::string("(i_hi + 0.5) / ") + std::to_string(g.gamut_cusp_table.total_size)) << ".rgb;";
    }
    else
    {
        ss.newLine() << ss.float3Decl("lo") << " = " << ss.sampleTex2D(name, ss.float2Const(std::string("(i_hi - 1 + 0.5) / ") + std::to_string(g.gamut_cusp_table.total_size), "0.5")) << ".rgb;";
        ss.newLine() << ss.float3Decl("hi") << " = " << ss.sampleTex2D(name, ss.float2Const(std::string("(i_hi + 0.5) / ") + std::to_string(g.gamut_cusp_table.total_size), "0.5")) << ".rgb;";
    }

    ss.newLine() << ss.floatDecl("t") << " = (h - lo.b) / (hi.b - lo.b);";
    ss.newLine() << ss.floatDecl("cuspJ") << " = " << ss.lerp("lo.r", "hi.r", "t") << ";";
    ss.newLine() << ss.floatDecl("cuspM") << " = " << ss.lerp("lo.g", "hi.g", "t") << ";";

    ss.newLine() << "return " << ss.float2Const("cuspJ", "cuspM") << ";";

    ss.dedent();
    ss.newLine() << "}";

    shaderCreator->addToHelperShaderCode(ss.string().c_str());

    return name;
}

std::string _Add_Gamma_table(
    GpuShaderCreatorRcPtr & shaderCreator,
    unsigned resourceIndex,
    const ACES2::GamutCompressParams & g)
{
    // Reserve name
    std::ostringstream resName;
    resName << shaderCreator->getResourcePrefix()
            << std::string("_")
            << std::string("upper_hull_gamma_table_")
            << resourceIndex;

    // Note: Remove potentially problematic double underscores from GLSL resource names.
    std::string name(resName.str());
    StringUtils::ReplaceInPlace(name, "__", "_");

    // Register texture
    GpuShaderDesc::TextureDimensions dimensions = GpuShaderDesc::TEXTURE_1D;
    if (shaderCreator->getLanguage() == GPU_LANGUAGE_GLSL_ES_1_0
        || shaderCreator->getLanguage() == GPU_LANGUAGE_GLSL_ES_3_0
        || !shaderCreator->getAllowTexture1D())
    {
        dimensions = GpuShaderDesc::TEXTURE_2D;
    }

    shaderCreator->addTexture(
        name.c_str(),
        GpuShaderText::getSamplerName(name).c_str(),
        g.gamut_cusp_table.total_size,
        1,
        GpuShaderCreator::TEXTURE_RED_CHANNEL,
        dimensions,
        INTERP_NEAREST,
        &(g.upper_hull_gamma_table.table[0]));


    if (dimensions == GpuShaderDesc::TEXTURE_1D)
    {
        GpuShaderText ss(shaderCreator->getLanguage());
        ss.declareTex1D(name);
        shaderCreator->addToDeclareShaderCode(ss.string().c_str());
    }
    else
    {
        GpuShaderText ss(shaderCreator->getLanguage());
        ss.declareTex2D(name);
        shaderCreator->addToDeclareShaderCode(ss.string().c_str());
    }

    // Sampler function
    GpuShaderText ss(shaderCreator->getLanguage());

    ss.newLine() << ss.floatKeyword() << " " << name << "_sample(float h)";
    ss.newLine() << "{";
    ss.indent();

    ss.newLine() << ss.floatDecl("hwrap") << " = h;";
    ss.newLine() << "hwrap = hwrap - floor(hwrap / 360.0) * 360.0;";
    ss.newLine() << "hwrap = (hwrap < 0.0) ? hwrap + 360.0 : hwrap;";

    ss.newLine() << ss.floatDecl("i_lo") << " = floor(hwrap) + " << g.upper_hull_gamma_table.base_index << ";";
    ss.newLine() << ss.floatDecl("i_hi") << " = (i_lo + 1);";
    ss.newLine() << "i_hi = i_hi - floor(i_hi / 360.0) * 360.0;";

    ss.newLine() << ss.floatDecl("base_hue") << " = i_lo - " << g.upper_hull_gamma_table.base_index << ";";

    if (dimensions == GpuShaderDesc::TEXTURE_1D)
    {
        ss.newLine() << ss.floatDecl("lo") << " = " << ss.sampleTex1D(name, std::string("(i_lo + 0.5) / ") + std::to_string(g.upper_hull_gamma_table.total_size)) << ".r;";
        ss.newLine() << ss.floatDecl("hi") << " = " << ss.sampleTex1D(name, std::string("(i_hi + 0.5) / ") + std::to_string(g.upper_hull_gamma_table.total_size)) << ".r;";
    }
    else
    {
        ss.newLine() << ss.floatDecl("lo") << " = " << ss.sampleTex2D(name, ss.float2Const(std::string("(i_lo + 0.5) / ") + std::to_string(g.upper_hull_gamma_table.total_size), "0.5")) << ".r;";
        ss.newLine() << ss.floatDecl("hi") << " = " << ss.sampleTex2D(name, ss.float2Const(std::string("(i_hi + 0.5) / ") + std::to_string(g.upper_hull_gamma_table.total_size), "0.5")) << ".r;";
    }

    ss.newLine() << ss.floatDecl("t") << " = hwrap - base_hue;";
    ss.newLine() << "return " << ss.lerp("lo", "hi", "t") << ";";

    ss.dedent();
    ss.newLine() << "}";

    shaderCreator->addToHelperShaderCode(ss.string().c_str());

    return name;
}

std::string _Add_Focus_Gain_func(
    GpuShaderCreatorRcPtr & shaderCreator,
    unsigned resourceIndex,
    const ACES2::GamutCompressParams & g)
{
    // Reserve name
    std::ostringstream resName;
    resName << shaderCreator->getResourcePrefix()
            << std::string("_")
            << std::string("get_focus_gain")
            << resourceIndex;

    // Note: Remove potentially problematic double underscores from GLSL resource names.
    std::string name(resName.str());
    StringUtils::ReplaceInPlace(name, "__", "_");

    GpuShaderText ss(shaderCreator->getLanguage());

    ss.newLine() << ss.floatKeyword() << " " << name << "(float J, float cuspJ)";
    ss.newLine() << "{";
    ss.indent();

    ss.newLine() << ss.floatDecl("thr") << " = " << ss.lerp("cuspJ", std::to_string(g.limit_J_max), std::to_string(ACES2::focus_gain_blend)) << ";";

    ss.newLine() << "if (J > thr)";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << ss.floatDecl("gain") << " = ( " << g.limit_J_max << " - thr) / max(0.0001, (" << g.limit_J_max << " - min(" << g.limit_J_max << ", J)));";
    ss.newLine() << "return pow(log(gain)/log(10.0), 1.0 / " << ACES2::focus_adjust_gain << ") + 1.0;";
    ss.dedent();
    ss.newLine() << "}";
    ss.newLine() << "else";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "return 1.0;";
    ss.dedent();
    ss.newLine() << "}";

    ss.dedent();
    ss.newLine() << "}";

    shaderCreator->addToHelperShaderCode(ss.string().c_str());

    return name;
}

std::string _Add_Solve_J_Intersect_func(
    GpuShaderCreatorRcPtr & shaderCreator,
    unsigned resourceIndex,
    const ACES2::GamutCompressParams & g)
{
    // Reserve name
    std::ostringstream resName;
    resName << shaderCreator->getResourcePrefix()
            << std::string("_")
            << std::string("solve_J_intersect")
            << resourceIndex;

    // Note: Remove potentially problematic double underscores from GLSL resource names.
    std::string name(resName.str());
    StringUtils::ReplaceInPlace(name, "__", "_");

    GpuShaderText ss(shaderCreator->getLanguage());

    ss.newLine() << ss.floatKeyword() << " " << name << "(float J, float M, float focusJ, float slope_gain)";
    ss.newLine() << "{";
    ss.indent();

    ss.newLine() << ss.floatDecl("a") << " = " << "M / (focusJ * slope_gain);";
    ss.newLine() << ss.floatDecl("b") << " = 0.0;";
    ss.newLine() << ss.floatDecl("c") << " = 0.0;";
    ss.newLine() << ss.floatDecl("intersectJ") << " = 0.0;";

    ss.newLine() << "if (J < focusJ)";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "b = 1.0 - M / slope_gain;";
    ss.dedent();
    ss.newLine() << "}";
    ss.newLine() << "else";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "b = - (1.0 + M / slope_gain + " << g.limit_J_max << " * M / (focusJ * slope_gain));";
    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << "if (J < focusJ)";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "c = -J;";
    ss.dedent();
    ss.newLine() << "}";
    ss.newLine() << "else";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "c = " << g.limit_J_max << " * M / slope_gain + J;";
    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << ss.floatDecl("root") << " = sqrt(b * b - 4.0 * a * c);";

    ss.newLine() << "if (J < focusJ)";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "intersectJ = 2.0 * c / (-b - root);";
    ss.dedent();
    ss.newLine() << "}";
    ss.newLine() << "else";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "intersectJ = 2.0 * c / (-b + root);";
    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << "return intersectJ;";

    ss.dedent();
    ss.newLine() << "}";

    shaderCreator->addToHelperShaderCode(ss.string().c_str());

    return name;
}

std::string _Add_Find_Gamut_Boundary_Intersection_func(
    GpuShaderCreatorRcPtr & shaderCreator,
    unsigned resourceIndex,
    const ACES2::GamutCompressParams & g,
    const std::string & solveJIntersectName)
{
    // Reserve name
    std::ostringstream resName;
    resName << shaderCreator->getResourcePrefix()
            << std::string("_")
            << std::string("find_gamut_boundary_intersection")
            << resourceIndex;

    // Note: Remove potentially problematic double underscores from GLSL resource names.
    std::string name(resName.str());
    StringUtils::ReplaceInPlace(name, "__", "_");

    GpuShaderText ss(shaderCreator->getLanguage());

    ss.newLine() << ss.float3Keyword() << " " << name << "(" << ss.float3Keyword() << " JMh_s, " << ss.float2Keyword() << " JM_cusp_in, float J_focus, float slope_gain, float gamma_top, float gamma_bottom)";
    ss.newLine() << "{";
    ss.indent();

    ss.newLine() << ss.float2Decl("JM_cusp") << " = " << ss.float2Const("JM_cusp_in.r", std::string("JM_cusp_in.g * (1.0 + ") + std::to_string(ACES2::smooth_m) + " * " + std::to_string(ACES2::smooth_cusps) + ")") << ";";

    ss.newLine() << ss.floatDecl("J_intersect_source") << " = " << solveJIntersectName << "(JMh_s.r, JMh_s.g, J_focus, slope_gain);";
    ss.newLine() << ss.floatDecl("J_intersect_cusp") << " = " << solveJIntersectName << "(JM_cusp.r, JM_cusp.g, J_focus, slope_gain);";

    ss.newLine() << ss.floatDecl("slope") << " = 0.0;";
    ss.newLine() << "if (J_intersect_source < J_focus)";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "slope = J_intersect_source * (J_intersect_source - J_focus) / (J_focus * slope_gain);";
    ss.dedent();
    ss.newLine() << "}";
    ss.newLine() << "else";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "slope = (" << g.limit_J_max << " - J_intersect_source) * (J_intersect_source - J_focus) / (J_focus * slope_gain);";
    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << ss.floatDecl("M_boundary_lower ") << " = J_intersect_cusp * pow(J_intersect_source / J_intersect_cusp, 1.0 / gamma_bottom) / (JM_cusp.r / JM_cusp.g - slope);";
    ss.newLine() << ss.floatDecl("M_boundary_upper") << " = JM_cusp.g * (" << g.limit_J_max << " - J_intersect_cusp) * pow((" << g.limit_J_max << " - J_intersect_source) / (" << g.limit_J_max << " - J_intersect_cusp), 1.0 / gamma_top) / (slope * JM_cusp.g + " << g.limit_J_max << " - JM_cusp.r);";

    ss.newLine() << ss.floatDecl("smin") << " = 0.0;";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << ss.floatDecl("a") << " = M_boundary_lower / JM_cusp.g;";
    ss.newLine() << ss.floatDecl("b") << " = M_boundary_upper / JM_cusp.g;";
    ss.newLine() << ss.floatDecl("s") << " = " << ACES2::smooth_cusps << ";";

    ss.newLine() << ss.floatDecl("h") << " = max(s - abs(a - b), 0.0) / s;";
    ss.newLine() << "smin = min(a, b) - h * h * h * s * (1.0 / 6.0);";

    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << ss.floatDecl("M_boundary") << " = JM_cusp.g * smin;";
    ss.newLine() << ss.floatDecl("J_boundary") << "= J_intersect_source + slope * M_boundary;";

    ss.newLine() << "return " << ss.float3Const("J_boundary", "M_boundary", "J_intersect_source") << ";";

    ss.dedent();
    ss.newLine() << "}";

    shaderCreator->addToHelperShaderCode(ss.string().c_str());

    return name;
}

std::string _Add_Reach_Boundary_func(
    GpuShaderCreatorRcPtr & shaderCreator,
    unsigned resourceIndex,
    const ACES2::GamutCompressParams & g,
    const std::string & reachName,
    const std::string & getFocusGainName,
    const std::string & solveJIntersectName)
{
    // Reserve name
    std::ostringstream resName;
    resName << shaderCreator->getResourcePrefix()
            << std::string("_")
            << std::string("get_reach_boundary")
            << resourceIndex;

    // Note: Remove potentially problematic double underscores from GLSL resource names.
    std::string name(resName.str());
    StringUtils::ReplaceInPlace(name, "__", "_");

    GpuShaderText ss(shaderCreator->getLanguage());

    ss.newLine() << ss.float3Keyword() << " " << name << "(float J, float M, float h, " << ss.float2Keyword() << " JMcusp, float focusJ)";
    ss.newLine() << "{";
    ss.indent();

    ss.newLine() << ss.floatDecl("reachMaxM") << " = " << reachName << "_sample(h);";
    ss.newLine() << ss.floatDecl("slope_gain") << " = " << g.limit_J_max << " * " << g.focus_dist << " * " << getFocusGainName << "(J, JMcusp.r);";
    ss.newLine() << ss.floatDecl("intersectJ") << " = " << solveJIntersectName << "(J, M, focusJ, slope_gain);";

    ss.newLine() << ss.floatDecl("slope") << " = 0.0;";
    ss.newLine() << "if (intersectJ < focusJ)";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "slope = intersectJ * (intersectJ - focusJ) / (focusJ * slope_gain);";
    ss.dedent();
    ss.newLine() << "}";
    ss.newLine() << "else";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "slope = (" << g.limit_J_max << " - intersectJ) * (intersectJ - focusJ) / (focusJ * slope_gain);";
    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << ss.floatDecl("boundary") << " = " << g.limit_J_max << " * pow(intersectJ / " << g.limit_J_max << ", " << g.model_gamma << ") * reachMaxM / (" << g.limit_J_max << " - slope * reachMaxM);";

    ss.newLine() << "return " << ss.float3Const("J", "boundary", "h") << ";";

    ss.dedent();
    ss.newLine() << "}";

    shaderCreator->addToHelperShaderCode(ss.string().c_str());

    return name;
}

std::string _Add_Compression_func(
    GpuShaderCreatorRcPtr & shaderCreator,
    unsigned resourceIndex,
    bool invert)
{
    // Reserve name
    std::ostringstream resName;
    resName << shaderCreator->getResourcePrefix()
            << std::string("_")
            << std::string("compression")
            << (invert ? std::string("_inv") : std::string("_fwd"))
            << resourceIndex;

    // Note: Remove potentially problematic double underscores from GLSL resource names.
    std::string name(resName.str());
    StringUtils::ReplaceInPlace(name, "__", "_");

    GpuShaderText ss(shaderCreator->getLanguage());

    ss.newLine() << ss.floatKeyword() << " " << name << "(float v, float thr, float lim)";
    ss.newLine() << "{";
    ss.indent();

    ss.newLine() << ss.floatDecl("s") << " = (lim - thr) * (1.0 - thr) / (lim - 1.0);";
    ss.newLine() << ss.floatDecl("nd") << " = (v - thr) / s;";


    ss.newLine() << ss.floatDecl("vCompressed") << " = 0.0;";

    if (invert)
    {
        ss.newLine() << "if (v < thr || lim <= 1.0001 || v > thr + s)";
        ss.newLine() << "{";
        ss.indent();
        ss.newLine() << "vCompressed = v;";
        ss.dedent();
        ss.newLine() << "}";
        ss.newLine() << "else";
        ss.newLine() << "{";
        ss.indent();
        ss.newLine() << "vCompressed = thr + s * (-nd / (nd - 1));";
        ss.dedent();
        ss.newLine() << "}";
    }
    else
    {
        ss.newLine() << "if (v < thr || lim <= 1.0001)";
        ss.newLine() << "{";
        ss.indent();
        ss.newLine() << "vCompressed = v;";
        ss.dedent();
        ss.newLine() << "}";
        ss.newLine() << "else";
        ss.newLine() << "{";
        ss.indent();
        ss.newLine() << "vCompressed = thr + s * nd / (1.0 + nd);";
        ss.dedent();
        ss.newLine() << "}";
    }

    ss.newLine() << "return vCompressed;";

    ss.dedent();
    ss.newLine() << "}";

    shaderCreator->addToHelperShaderCode(ss.string().c_str());

    return name;
}

std::string _Add_Compress_Gamut_func(
    GpuShaderCreatorRcPtr & shaderCreator,
    unsigned resourceIndex,
    const ACES2::GamutCompressParams & g,
    const std::string & cuspName,
    const std::string & getFocusGainName,
    const std::string & gammaName,
    const std::string & findGamutBoundaryIntersectionName,
    const std::string & getReachBoundaryName,
    const std::string & compressionName)
{
    // Reserve name
    std::ostringstream resName;
    resName << shaderCreator->getResourcePrefix()
            << std::string("_")
            << std::string("gamut_compress")
            << resourceIndex;

    // Note: Remove potentially problematic double underscores from GLSL resource names.
    std::string name(resName.str());
    StringUtils::ReplaceInPlace(name, "__", "_");

    GpuShaderText ss(shaderCreator->getLanguage());

    ss.newLine() << ss.float3Keyword() << " " << name << "(" << ss.float3Keyword() << " JMh, float Jx)";
    ss.newLine() << "{";
    ss.indent();

    ss.newLine() << ss.floatDecl("J") << " = JMh.r;";
    ss.newLine() << ss.floatDecl("M") << " = JMh.g;";
    ss.newLine() << ss.floatDecl("h") << " = JMh.b;";

    ss.newLine() << "if (M < 0.0001 || J > " << g.limit_J_max << ")";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "return " << ss.float3Const("J", "0.0", "h") << ";";
    ss.dedent();
    ss.newLine() << "}";
    ss.newLine() << "else";
    ss.newLine() << "{";
    ss.indent();

    ss.newLine() << ss.float2Decl("project_from") << " = " << ss.float2Const("J", "M") << ";";
    ss.newLine() << ss.float2Decl("JMcusp") << " = " << cuspName << "_sample(h);";

    ss.newLine() << ss.floatDecl("focusJ") << " = " << ss.lerp("JMcusp.r", std::to_string(g.mid_J), std::string("min(1.0, ") + std::to_string(ACES2::cusp_mid_blend) + " - (JMcusp.r / " + std::to_string(g.limit_J_max)) << "));";
    ss.newLine() << ss.floatDecl("slope_gain") << " = " << g.limit_J_max << " * " << g.focus_dist << " * " << getFocusGainName << "(Jx, JMcusp.r);";

    ss.newLine() << ss.floatDecl("gamma_top") << " = " << gammaName << "_sample(h);";
    ss.newLine() << ss.floatDecl("gamma_bottom") << " = " << g.lower_hull_gamma << ";";

    ss.newLine() << ss.float3Decl("boundaryReturn") << " = " << findGamutBoundaryIntersectionName << "(" << ss.float3Const("J", "M", "h") << ", JMcusp, focusJ, slope_gain, gamma_top, gamma_bottom);";
    ss.newLine() << ss.float2Decl("JMboundary") << " = " << ss.float2Const("boundaryReturn.r", "boundaryReturn.g") << ";";
    ss.newLine() << ss.float2Decl("project_to") << " = " << ss.float2Const("boundaryReturn.b", "0.0") << ";";

    ss.newLine() << "if (JMboundary.g <= 0.0)";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "return " << ss.float3Const("J", "0.0", "h") << ";";
    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << ss.float3Decl("reachBoundary") << " = " << getReachBoundaryName << "(JMboundary.r, JMboundary.g, h, JMcusp, focusJ);";

    ss.newLine() << ss.floatDecl("difference") << " = max(1.0001, reachBoundary.g / JMboundary.g);";
    ss.newLine() << ss.floatDecl("threshold") << " = max(" << ACES2::compression_threshold << ", 1.0 / difference);";

    ss.newLine() << ss.floatDecl("v") << " = project_from.g / JMboundary.g;";
    ss.newLine() << "v = " << compressionName << "(v, threshold, difference);";

    ss.newLine() << ss.float2Decl("JMcompressed") << " = " << ss.float2Const(
        "project_to.r + v * (JMboundary.r - project_to.r)",
        "project_to.g + v * (JMboundary.g - project_to.g)"
    ) << ";";

    ss.newLine() << "return " << ss.float3Const("JMcompressed.r", "JMcompressed.g", "h") << ";";

    ss.dedent();
    ss.newLine() << "}";

    ss.dedent();
    ss.newLine() << "}";

    shaderCreator->addToHelperShaderCode(ss.string().c_str());

    return name;
}

void _Add_Gamut_Compress_Fwd_Shader(
    GpuShaderCreatorRcPtr & shaderCreator,
    GpuShaderText & ss,
    unsigned int resourceIndex,
    const ACES2::GamutCompressParams & g,
    const std::string & reachName)
{
    std::string cuspName = _Add_Cusp_table(shaderCreator, resourceIndex, g);
    std::string gammaName = _Add_Gamma_table(shaderCreator, resourceIndex, g);
    std::string getFocusGainName = _Add_Focus_Gain_func(shaderCreator, resourceIndex, g);
    std::string solveJIntersectName = _Add_Solve_J_Intersect_func(shaderCreator, resourceIndex, g);
    std::string findGamutBoundaryIntersectionName = _Add_Find_Gamut_Boundary_Intersection_func(shaderCreator, resourceIndex, g, solveJIntersectName);
    std::string getReachBoundaryName = _Add_Reach_Boundary_func(shaderCreator, resourceIndex, g, reachName, getFocusGainName, solveJIntersectName);
    std::string compressionName = _Add_Compression_func(shaderCreator, resourceIndex, false);
    std::string gamutCompressName = _Add_Compress_Gamut_func(shaderCreator, resourceIndex, g, cuspName, getFocusGainName, gammaName, findGamutBoundaryIntersectionName, getReachBoundaryName, compressionName);

    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << pxl << ".rgb = " << gamutCompressName << "(" << pxl << ".rgb, " << pxl << ".r);";
}

void _Add_Gamut_Compress_Inv_Shader(
    GpuShaderCreatorRcPtr & shaderCreator,
    GpuShaderText & ss,
    unsigned int resourceIndex,
    const ACES2::GamutCompressParams & g,
    const std::string & reachName)
{
    std::string cuspName = _Add_Cusp_table(shaderCreator, resourceIndex, g);
    std::string gammaName = _Add_Gamma_table(shaderCreator, resourceIndex, g);
    std::string getFocusGainName = _Add_Focus_Gain_func(shaderCreator, resourceIndex, g);
    std::string solveJIntersectName = _Add_Solve_J_Intersect_func(shaderCreator, resourceIndex, g);
    std::string findGamutBoundaryIntersectionName = _Add_Find_Gamut_Boundary_Intersection_func(shaderCreator, resourceIndex, g, solveJIntersectName);
    std::string getReachBoundaryName = _Add_Reach_Boundary_func(shaderCreator, resourceIndex, g, reachName, getFocusGainName, solveJIntersectName);
    std::string compressionName = _Add_Compression_func(shaderCreator, resourceIndex, true);
    std::string gamutCompressName = _Add_Compress_Gamut_func(shaderCreator, resourceIndex, g, cuspName, getFocusGainName, gammaName, findGamutBoundaryIntersectionName, getReachBoundaryName, compressionName);

    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.float2Decl("JMcusp") << " = " << cuspName << "_sample(" << pxl << ".b);";
    ss.newLine() << ss.floatDecl("Jx") << " = " << pxl << ".r;";
    ss.newLine() << ss.float3Decl("unCompressedJMh") << ";";

    // Analytic inverse below threshold
    ss.newLine() << "if (Jx <= " << ss.lerp("JMcusp.r", std::to_string(g.limit_J_max), std::to_string(ACES2::focus_gain_blend)) << ")";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "unCompressedJMh = " << gamutCompressName << "(" << pxl << ".rgb, Jx);";
    ss.dedent();
    ss.newLine() << "}";
    // Approximation above threshold
    ss.newLine() << "else";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "Jx = " << gamutCompressName << "(" << pxl << ".rgb, Jx).r;";
    ss.newLine() << "unCompressedJMh = " << gamutCompressName << "(" << pxl << ".rgb, Jx);";
    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << pxl << ".rgb = unCompressedJMh;";
}

void Add_ACES_OutputTransform_Fwd_Shader(
    GpuShaderCreatorRcPtr & shaderCreator,
    GpuShaderText & ss,
    const FixedFunctionOpData::Params & params)
{
    const float peak_luminance = (float) params[0];

    const float red_x   = (float) params[1];
    const float red_y   = (float) params[2];
    const float green_x = (float) params[3];
    const float green_y = (float) params[4];
    const float blue_x  = (float) params[5];
    const float blue_y  = (float) params[6];
    const float white_x = (float) params[7];
    const float white_y = (float) params[8];

    const Primaries in_primaries = ACES_AP0::primaries;

    const Primaries lim_primaries = {
        {red_x  , red_y  },
        {green_x, green_y},
        {blue_x , blue_y },
        {white_x, white_y}
    };

    ACES2::JMhParams pIn = ACES2::init_JMhParams(in_primaries);
    ACES2::JMhParams pLim = ACES2::init_JMhParams(lim_primaries);
    ACES2::ToneScaleParams t = ACES2::init_ToneScaleParams(peak_luminance);
    ACES2::ChromaCompressParams c = ACES2::init_ChromaCompressParams(peak_luminance);
    const ACES2::GamutCompressParams g = ACES2::init_GamutCompressParams(peak_luminance, lim_primaries);

    unsigned resourceIndex = shaderCreator->getNextResourceIndex();

    std::string reachName = _Add_Reach_table(shaderCreator, resourceIndex, g.reach_m_table);

    ss.newLine() << "";
    ss.newLine() << "// Add RGB to JMh";
    ss.newLine() << "";
    ss.newLine() << "{";
    ss.indent();
        _Add_RGB_to_JMh_Shader(shaderCreator, ss, pIn);
    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << "";
    ss.newLine() << "// Add ToneScale and ChromaCompress (fwd)";
    ss.newLine() << "";
    ss.newLine() << "{";
    ss.indent();
        _Add_Tonescale_Compress_Fwd_Shader(shaderCreator, ss, resourceIndex, pIn, t, c, reachName);
    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << "";
    ss.newLine() << "// Add GamutCompress (fwd)";
    ss.newLine() << "";
    ss.newLine() << "{";
    ss.indent();
        _Add_Gamut_Compress_Fwd_Shader(shaderCreator, ss, resourceIndex, g, reachName);
    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << "";
    ss.newLine() << "// Add JMh to RGB";
    ss.newLine() << "";
    ss.newLine() << "{";
    ss.indent();
        _Add_JMh_to_RGB_Shader(shaderCreator, ss, pLim);
    ss.dedent();
    ss.newLine() << "}";

}

void Add_ACES_OutputTransform_Inv_Shader(
    GpuShaderCreatorRcPtr & shaderCreator,
    GpuShaderText & ss,
    const FixedFunctionOpData::Params & params)
{
    const float peak_luminance = (float) params[0];

    const float red_x   = (float) params[1];
    const float red_y   = (float) params[2];
    const float green_x = (float) params[3];
    const float green_y = (float) params[4];
    const float blue_x  = (float) params[5];
    const float blue_y  = (float) params[6];
    const float white_x = (float) params[7];
    const float white_y = (float) params[8];

    const Primaries in_primaries = ACES_AP0::primaries;

    const Primaries lim_primaries = {
        {red_x  , red_y  },
        {green_x, green_y},
        {blue_x , blue_y },
        {white_x, white_y}
    };

    ACES2::JMhParams pIn = ACES2::init_JMhParams(in_primaries);
    ACES2::JMhParams pLim = ACES2::init_JMhParams(lim_primaries);
    ACES2::ToneScaleParams t = ACES2::init_ToneScaleParams(peak_luminance);
    ACES2::ChromaCompressParams c = ACES2::init_ChromaCompressParams(peak_luminance);
    const ACES2::GamutCompressParams g = ACES2::init_GamutCompressParams(peak_luminance, lim_primaries);

    unsigned resourceIndex = shaderCreator->getNextResourceIndex();

    std::string reachName = _Add_Reach_table(shaderCreator, resourceIndex, c.reach_m_table);

    ss.newLine() << "";
    ss.newLine() << "// Add RGB to JMh";
    ss.newLine() << "";
    ss.newLine() << "{";
    ss.indent();
        _Add_RGB_to_JMh_Shader(shaderCreator, ss, pLim);
    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << "";
    ss.newLine() << "// Add GamutCompress (inv)";
    ss.newLine() << "";
    ss.newLine() << "{";
    ss.indent();
        _Add_Gamut_Compress_Inv_Shader(shaderCreator, ss, resourceIndex, g, reachName);
    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << "";
    ss.newLine() << "// Add ToneScale and ChromaCompress (inv)";
    ss.newLine() << "";
    ss.newLine() << "{";
    ss.indent();
        _Add_Tonescale_Compress_Inv_Shader(shaderCreator, ss, resourceIndex, pIn, t, c, reachName);
    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << "";
    ss.newLine() << "// Add JMh to RGB";
    ss.newLine() << "";
    ss.newLine() << "{";
    ss.indent();
        _Add_JMh_to_RGB_Shader(shaderCreator, ss, pIn);
    ss.dedent();
    ss.newLine() << "}";
}

void Add_RGB_to_JMh_Shader(
    GpuShaderCreatorRcPtr & shaderCreator,
    GpuShaderText & ss,
    const FixedFunctionOpData::Params & params)
{
    const float red_x   = (float) params[0];
    const float red_y   = (float) params[1];
    const float green_x = (float) params[2];
    const float green_y = (float) params[3];
    const float blue_x  = (float) params[4];
    const float blue_y  = (float) params[5];
    const float white_x = (float) params[6];
    const float white_y = (float) params[7];

    const Primaries primaries = {
        {red_x  , red_y  },
        {green_x, green_y},
        {blue_x , blue_y },
        {white_x, white_y}
    };
    ACES2::JMhParams p = ACES2::init_JMhParams(primaries);

    _Add_RGB_to_JMh_Shader(shaderCreator, ss, p);
}

void Add_JMh_to_RGB_Shader(
    GpuShaderCreatorRcPtr & shaderCreator,
    GpuShaderText & ss,
    const FixedFunctionOpData::Params & params)
{
    const float red_x   = (float) params[0];
    const float red_y   = (float) params[1];
    const float green_x = (float) params[2];
    const float green_y = (float) params[3];
    const float blue_x  = (float) params[4];
    const float blue_y  = (float) params[5];
    const float white_x = (float) params[6];
    const float white_y = (float) params[7];

    const Primaries primaries = {
        {red_x  , red_y  },
        {green_x, green_y},
        {blue_x , blue_y },
        {white_x, white_y}
    };

    ACES2::JMhParams p = ACES2::init_JMhParams(primaries);

    _Add_JMh_to_RGB_Shader(shaderCreator, ss, p);
}

void Add_Tonescale_Compress_Fwd_Shader(
    GpuShaderCreatorRcPtr & shaderCreator,
    GpuShaderText & ss,
    const FixedFunctionOpData::Params & params)
{
    const float peak_luminance = (float) params[0];

    ACES2::JMhParams p = ACES2::init_JMhParams(ACES_AP0::primaries);
    ACES2::ToneScaleParams t = ACES2::init_ToneScaleParams(peak_luminance);
    ACES2::ChromaCompressParams c = ACES2::init_ChromaCompressParams(peak_luminance);

    unsigned resourceIndex = shaderCreator->getNextResourceIndex();

    std::string reachName = _Add_Reach_table(shaderCreator, resourceIndex, c.reach_m_table);

    _Add_Tonescale_Compress_Fwd_Shader(shaderCreator, ss, resourceIndex, p, t, c, reachName);
}

void Add_Tonescale_Compress_Inv_Shader(
    GpuShaderCreatorRcPtr & shaderCreator,
    GpuShaderText & ss,
    const FixedFunctionOpData::Params & params)
{
    const float peak_luminance = (float) params[0];

    ACES2::JMhParams p = ACES2::init_JMhParams(ACES_AP0::primaries);
    ACES2::ToneScaleParams t = ACES2::init_ToneScaleParams(peak_luminance);
    ACES2::ChromaCompressParams c = ACES2::init_ChromaCompressParams(peak_luminance);

    unsigned resourceIndex = shaderCreator->getNextResourceIndex();

    std::string reachName = _Add_Reach_table(shaderCreator, resourceIndex, c.reach_m_table);

    _Add_Tonescale_Compress_Inv_Shader(shaderCreator, ss, resourceIndex, p, t, c, reachName);
}

void Add_Gamut_Compress_Fwd_Shader(
    GpuShaderCreatorRcPtr & shaderCreator,
    GpuShaderText & ss,
    const FixedFunctionOpData::Params & params)
{
    const float peak_luminance = (float) params[0];

    const float red_x   = (float) params[1];
    const float red_y   = (float) params[2];
    const float green_x = (float) params[3];
    const float green_y = (float) params[4];
    const float blue_x  = (float) params[5];
    const float blue_y  = (float) params[6];
    const float white_x = (float) params[7];
    const float white_y = (float) params[8];

    const Primaries primaries = {
        {red_x  , red_y  },
        {green_x, green_y},
        {blue_x , blue_y },
        {white_x, white_y}
    };

    const ACES2::GamutCompressParams g = ACES2::init_GamutCompressParams(peak_luminance, primaries);

    unsigned resourceIndex = shaderCreator->getNextResourceIndex();

    std::string reachName = _Add_Reach_table(shaderCreator, resourceIndex, g.reach_m_table);

    _Add_Gamut_Compress_Fwd_Shader(shaderCreator, ss, resourceIndex, g, reachName);
}

void Add_Gamut_Compress_Inv_Shader(
    GpuShaderCreatorRcPtr & shaderCreator,
    GpuShaderText & ss,
    const FixedFunctionOpData::Params & params)
{
    const float peak_luminance = (float) params[0];

    const float red_x   = (float) params[1];
    const float red_y   = (float) params[2];
    const float green_x = (float) params[3];
    const float green_y = (float) params[4];
    const float blue_x  = (float) params[5];
    const float blue_y  = (float) params[6];
    const float white_x = (float) params[7];
    const float white_y = (float) params[8];

    const Primaries primaries = {
        {red_x  , red_y  },
        {green_x, green_y},
        {blue_x , blue_y },
        {white_x, white_y}
    };

    const ACES2::GamutCompressParams g = ACES2::init_GamutCompressParams(peak_luminance, primaries);

    unsigned resourceIndex = shaderCreator->getNextResourceIndex();

    std::string reachName = _Add_Reach_table(shaderCreator, resourceIndex, g.reach_m_table);

    _Add_Gamut_Compress_Inv_Shader(shaderCreator, ss, resourceIndex, g, reachName);
}

void Add_Surround_10_Fwd_Shader(GpuShaderCreatorRcPtr & shaderCreator, GpuShaderText & ss, float gamma)
{
    const std::string pxl(shaderCreator->getPixelName());

    // TODO: -- add vector inner product to GPUShaderUtils
    ss.newLine() << ss.floatDecl("Y")
                 << " = max( 1e-10, 0.27222871678091454 * " << pxl << ".rgb.r + "
                 <<                "0.67408176581114831 * " << pxl << ".rgb.g + "
                 <<                "0.053689517407937051 * " << pxl << ".rgb.b );";

    ss.newLine() << ss.floatDecl("Ypow_over_Y") << " = pow( Y, " << gamma - 1.f << ");";

    ss.newLine() << pxl << ".rgb = " << pxl << ".rgb * Ypow_over_Y;";
}

void Add_Rec2100_Surround_Shader(GpuShaderCreatorRcPtr & shaderCreator, GpuShaderText & ss, 
                                 float gamma, bool isForward)
{
    const std::string pxl(shaderCreator->getPixelName());

    float minLum = 1e-4f;
    if (!isForward)
    {
        minLum = powf(minLum, gamma);
        gamma = 1.f / gamma;
    }

    ss.newLine() << ss.floatDecl("Y") 
                 << " = 0.2627 * " << pxl << ".rgb.r + "
                 <<    "0.6780 * " << pxl << ".rgb.g + "
                 <<    "0.0593 * " << pxl << ".rgb.b;";

    ss.newLine() << "Y = max( " << minLum << ", abs(Y) );";

    ss.newLine() << ss.floatDecl("Ypow_over_Y") << " = pow( Y, " << (gamma - 1.f) << ");";

    ss.newLine() << "" << pxl << ".rgb = " << pxl << ".rgb * Ypow_over_Y;";
}

void Add_RGB_TO_HSV(GpuShaderCreatorRcPtr & shaderCreator, GpuShaderText & ss)
{
    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.floatDecl("minRGB") << " = min( " << pxl << ".rgb.r, min( " << pxl << ".rgb.g, " << pxl << ".rgb.b ) );";
    ss.newLine() << ss.floatDecl("maxRGB") << " = max( " << pxl << ".rgb.r, max( " << pxl << ".rgb.g, " << pxl << ".rgb.b ) );";
    ss.newLine() << ss.floatDecl("val") << " = maxRGB;";

    ss.newLine() << ss.floatDecl("sat") << " = 0.0, hue = 0.0;";
    ss.newLine() << "if (minRGB != maxRGB)";
    ss.newLine() << "{";
    ss.indent();

    ss.newLine() << "if (val != 0.0) sat = (maxRGB - minRGB) / val;";
    ss.newLine() << ss.floatDecl("OneOverMaxMinusMin") << " = 1.0 / (maxRGB - minRGB);";
    ss.newLine() << "if ( maxRGB == " << pxl << ".rgb.r ) hue = (" << pxl << ".rgb.g - " << pxl << ".rgb.b) * OneOverMaxMinusMin;";
    ss.newLine() << "else if ( maxRGB == " << pxl << ".rgb.g ) hue = 2.0 + (" << pxl << ".rgb.b - " << pxl << ".rgb.r) * OneOverMaxMinusMin;";
    ss.newLine() << "else hue = 4.0 + (" << pxl << ".rgb.r - " << pxl << ".rgb.g) * OneOverMaxMinusMin;";
    ss.newLine() << "if ( hue < 0.0 ) hue += 6.0;";

    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << "if ( minRGB < 0.0 ) val += minRGB;";
    ss.newLine() << "if ( -minRGB > maxRGB ) sat = (maxRGB - minRGB) / -minRGB;";

    ss.newLine() << pxl << ".rgb = " << ss.float3Const("hue * 1./6.", "sat", "val") << ";";
}

void Add_HSV_TO_RGB(GpuShaderCreatorRcPtr & shaderCreator, GpuShaderText & ss)
{
    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.floatDecl("Hue") << " = ( " << pxl << ".rgb.r - floor( " << pxl << ".rgb.r ) ) * 6.0;";
    ss.newLine() << ss.floatDecl("Sat") << " = clamp( " << pxl << ".rgb.g, 0., 1.999 );";
    ss.newLine() << ss.floatDecl("Val") << " = " << pxl << ".rgb.b;";

    ss.newLine() << ss.floatDecl("R") << " = abs(Hue - 3.0) - 1.0;";
    ss.newLine() << ss.floatDecl("G") << " = 2.0 - abs(Hue - 2.0);";
    ss.newLine() << ss.floatDecl("B") << " = 2.0 - abs(Hue - 4.0);";
    ss.newLine() << ss.float3Decl("RGB") << " = " << ss.float3Const("R", "G", "B") << ";";
    ss.newLine() << "RGB = clamp( RGB, 0., 1. );";

    ss.newLine() << ss.floatKeyword() << " rgbMax = Val;";
    ss.newLine() << ss.floatKeyword() << " rgbMin = Val * (1.0 - Sat);";

    ss.newLine() << "if ( Sat > 1.0 )";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "rgbMin = Val * (1.0 - Sat) / (2.0 - Sat);";
    ss.newLine() << "rgbMax = Val - rgbMin;";
    ss.dedent();
    ss.newLine() << "}";
    ss.newLine() << "if ( Val < 0.0 )";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << "rgbMin = Val / (2.0 - Sat);";
    ss.newLine() << "rgbMax = Val - rgbMin;";
    ss.dedent();
    ss.newLine() << "}";

    ss.newLine() << "RGB = RGB * (rgbMax - rgbMin) + rgbMin;";

    ss.newLine() << "" << pxl << ".rgb = RGB;";
}

void Add_XYZ_TO_xyY(GpuShaderCreatorRcPtr & shaderCreator, GpuShaderText & ss)
{
    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.floatDecl("d") << " = " << pxl << ".rgb.r + " << pxl << ".rgb.g + " << pxl << ".rgb.b;";
    ss.newLine() << "d = (d == 0.) ? 0. : 1. / d;";
    ss.newLine() << pxl << ".rgb.b = " << pxl << ".rgb.g;";
    ss.newLine() << pxl << ".rgb.r *= d;";
    ss.newLine() << pxl << ".rgb.g *= d;";
}

void Add_xyY_TO_XYZ(GpuShaderCreatorRcPtr & shaderCreator, GpuShaderText & ss)
{
    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.floatDecl("d") << " = (" << pxl << ".rgb.g == 0.) ? 0. : 1. / " << pxl << ".rgb.g;";
    ss.newLine() << ss.floatDecl("Y") << " = " << pxl << ".rgb.b;";
    ss.newLine() << pxl << ".rgb.b = Y * (1. - " << pxl << ".rgb.r - " << pxl << ".rgb.g) * d;";
    ss.newLine() << pxl << ".rgb.r *= Y * d;";
    ss.newLine() << pxl << ".rgb.g = Y;";
}

void Add_XYZ_TO_uvY(GpuShaderCreatorRcPtr & shaderCreator, GpuShaderText & ss)
{
    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.floatDecl("d") << " = " << pxl << ".rgb.r + 15. * " << pxl << ".rgb.g + 3. * " << pxl << ".rgb.b;";
    ss.newLine() << "d = (d == 0.) ? 0. : 1. / d;";
    ss.newLine() << pxl << ".rgb.b = " << pxl << ".rgb.g;";
    ss.newLine() << pxl << ".rgb.r *= 4. * d;";
    ss.newLine() << pxl << ".rgb.g *= 9. * d;";
}

void Add_uvY_TO_XYZ(GpuShaderCreatorRcPtr & shaderCreator, GpuShaderText & ss)
{
    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.floatDecl("d") << " = (" << pxl << ".rgb.g == 0.) ? 0. : 1. / " << pxl << ".rgb.g;";
    ss.newLine() << ss.floatDecl("Y") << " = " << pxl << ".rgb.b;";
    ss.newLine() << pxl << ".rgb.b = (3./4.) * Y * (4. - " << pxl << ".rgb.r - 6.6666666666666667 * " << pxl << ".rgb.g) * d;";
    ss.newLine() << pxl << ".rgb.r *= (9./4.) * Y * d;";
    ss.newLine() << pxl << ".rgb.g = Y;";
}

void Add_XYZ_TO_LUV(GpuShaderCreatorRcPtr & shaderCreator, GpuShaderText & ss)
{
    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.floatDecl("d") << " = " << pxl << ".rgb.r + 15. * " << pxl << ".rgb.g + 3. * " << pxl << ".rgb.b;";
    ss.newLine() << "d = (d == 0.) ? 0. : 1. / d;";
    ss.newLine() << ss.floatDecl("u") << " = " << pxl << ".rgb.r * 4. * d;";
    ss.newLine() << ss.floatDecl("v") << " = " << pxl << ".rgb.g * 9. * d;";
    ss.newLine() << ss.floatDecl("Y") << " = " << pxl << ".rgb.g;";

    ss.newLine() << ss.floatDecl("Lstar") << " = " << ss.lerp( "1.16 * pow( max(0., Y), 1./3. ) - 0.16", "9.0329629629629608 * Y",
                                                               "float(Y <= 0.008856451679)" ) << ";";
    ss.newLine() << ss.floatDecl("ustar") << " = 13. * Lstar * (u - 0.19783001);";
    ss.newLine() << ss.floatDecl("vstar") << " = 13. * Lstar * (v - 0.46831999);";

    ss.newLine() << pxl << ".rgb = " << ss.float3Const("Lstar", "ustar", "vstar") << ";";
}

void Add_LUV_TO_XYZ(GpuShaderCreatorRcPtr & shaderCreator, GpuShaderText & ss)
{
    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.floatDecl("Lstar") << " = " << pxl << ".rgb.r;";
    ss.newLine() << ss.floatDecl("d") << " = (Lstar == 0.) ? 0. : 0.076923076923076927 / Lstar;";
    ss.newLine() << ss.floatDecl("u") << " = " << pxl << ".rgb.g * d + 0.19783001;";
    ss.newLine() << ss.floatDecl("v") << " = " << pxl << ".rgb.b * d + 0.46831999;";

    ss.newLine() << ss.floatDecl("tmp") << " = (Lstar + 0.16) * 0.86206896551724144;";
    ss.newLine() << ss.floatDecl("Y") << " = " << ss.lerp( "tmp * tmp * tmp", "0.11070564598794539 * Lstar", "float(Lstar <= 0.08)" ) << ";";

    ss.newLine() << ss.floatDecl("dd") << " = (v == 0.) ? 0. : 0.25 / v;";
    ss.newLine() << pxl << ".rgb.r = 9. * Y * u * dd;";
    ss.newLine() << pxl << ".rgb.b = Y * (12. - 3. * u - 20. * v) * dd;";
    ss.newLine() << pxl << ".rgb.g = Y;";
}


namespace 
{
namespace ST_2084
{
    static constexpr double m1 = 0.25 * 2610. / 4096.;
    static constexpr double m2 = 128. * 2523. / 4096.;
    static constexpr double c2 = 32. * 2413. / 4096.;
    static constexpr double c3 = 32. * 2392. / 4096.;
    static constexpr double c1 = c3 - c2 + 1.;
}
} // anonymous

void Add_LIN_TO_PQ(GpuShaderCreatorRcPtr& shaderCreator, GpuShaderText& ss)
{
    using namespace ST_2084;
    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.float3Decl("sign3") << " = sign(" << pxl << ".rgb);";
    ss.newLine() << ss.float3Decl("L") << " = abs(0.01 * " << pxl << ".rgb);";
    ss.newLine() << ss.float3Decl("y") << " = pow(L, " << ss.float3Const(m1) << ");";
    ss.newLine() << ss.float3Decl("ratpoly") << " = (" << ss.float3Const(c1) << " + " << c2 << " * y) / ("
        << ss.float3Const(1.0) << " + " << c3 << " * y);";
    ss.newLine() << pxl << ".rgb = sign3 * pow(ratpoly, " << ss.float3Const(m2) << ");";

    // The sign transfer here is very slightly different than in the CPU path,
    // resulting in a PQ value of 0 at 0 rather than the true value of
    // 0.836^78.84 = 7.36e-07, however, this is well below visual threshold.
}

void Add_PQ_TO_LIN(GpuShaderCreatorRcPtr& shaderCreator, GpuShaderText& ss)
{
    using namespace ST_2084;
    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.float3Decl("sign3") << " = sign(" << pxl << ".rgb);";
    ss.newLine() << ss.float3Decl("x") << " = pow(abs(" << pxl << ".rgb), " << ss.float3Const(1.0 / m2) << ");";
    ss.newLine() << pxl << ".rgb = 100. * sign3 * pow(max(" << ss.float3Const(0.0) << ", x - " << ss.float3Const(c1) << ") / ("
        << ss.float3Const(c2) << " - " << c3 << " * x), " << ss.float3Const(1.0 / m1) << ");";
}

void Add_LIN_TO_GAMMA_LOG(
    GpuShaderCreatorRcPtr& shaderCreator, 
    GpuShaderText& ss,
    const FixedFunctionOpData::Params& params)
{
    // Get parameters, baking the log base conversion into 'logSlope'.
    double mirrorPt          = params[0];
    double breakPt           = params[1];
    double gammaSeg_power    = params[2];
    double gammaSeg_slope    = params[3];
    double gammaSeg_off      = params[4];
    double logSeg_base       = params[5];
    double logSeg_logSlope   = params[6] / std::log(logSeg_base);
    double logSeg_logOff     = params[7];
    double logSeg_linSlope   = params[8];
    double logSeg_linOff     = params[9];

    // float mirrorin = in - m_mirror;
    // float E = std::abs(mirrorin) + m_mirror;
    // float Eprime;
    // if (E < m_break)
    //     Eprime = m_gammaSeg.slope * std::pow(E + m_gammaSeg.off, m_gammaSeg.power);
    // else
    //     Eprime = m_logSeg.logSlope * std::log(m_logSeg.linSlope * E + m_logSeg.linOff) + m_logSeg.logOff;
    // out = Eprime * std::copysign(1.0, in);

    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.float3Decl("mirrorin") << " = " << pxl << ".rgb - " << ss.float3Const(mirrorPt) << ";";
    ss.newLine() << ss.float3Decl("sign3") << " = sign(mirrorin);";
    ss.newLine() << ss.float3Decl("E") << " = abs(mirrorin) + " << ss.float3Const(mirrorPt) << ";";
    ss.newLine() << ss.float3Decl("isAboveBreak") << " = " << ss.float3GreaterThan("E", ss.float3Const(breakPt)) << ";";
    ss.newLine() << ss.float3Decl("Ep_gamma") << " = " << ss.float3Const(gammaSeg_slope)
        << " * pow( E - " << ss.float3Const(gammaSeg_off) << ", " << ss.float3Const(gammaSeg_power) << ");";
    ss.newLine() << ss.float3Decl("Ep_log") << " = " << ss.float3Const(logSeg_logSlope) << " * log( E * "
        << ss.float3Const(logSeg_linSlope) <<  " +" << ss.float3Const(logSeg_linOff) << ") + " 
        << ss.float3Const(logSeg_logOff) << ";";

    // Combine log and gamma parts.
    ss.newLine() << pxl << ".rgb = sign3 * (isAboveBreak * Ep_log + ( " << ss.float3Const(1.0f) << " - isAboveBreak ) * Ep_gamma);";
}

void Add_GAMMA_LOG_TO_LIN(
    GpuShaderCreatorRcPtr& shaderCreator, 
    GpuShaderText& ss,
    const FixedFunctionOpData::Params& params)
{
    // Get parameters, baking the log base conversion into 'logSlope'.
    double mirrorPt          = params[0];
    double breakPt           = params[1];
    double gammaSeg_power    = params[2];
    double gammaSeg_slope    = params[3];
    double gammaSeg_off      = params[4];
    double logSeg_base       = params[5];
    double logSeg_logSlope   = params[6] / std::log(logSeg_base);
    double logSeg_logOff     = params[7];
    double logSeg_linSlope   = params[8];
    double logSeg_linOff     = params[9];

    double primeBreak  = gammaSeg_slope * std::pow(breakPt  + gammaSeg_off, gammaSeg_power);
    double primeMirror = gammaSeg_slope * std::pow(mirrorPt + gammaSeg_off, gammaSeg_power);

    // float mirrorin = in - primeMirror;
    // float Eprime = std::abs(mirrorin) + primeMirror;
    // if (Eprime < m_primeBreak)
    //     E = std::pow(Eprime / m_gammaSeg.slope, 1.0f / m_gammaSeg.power) - m_gammaSeg.off;
    // else
    //     E = (std::exp((Eprime - m_logSeg.logOff) / m_logSeg.logSlope) - m_logSeg.linOff) / m_logSeg.linSlope;
    // out = std::copysign(E, Eprimein);

    const std::string pxl(shaderCreator->getPixelName());

    ss.newLine() << ss.float3Decl("mirrorin") << " = " << pxl << ".rgb - " << ss.float3Const(primeMirror) << ";";
    ss.newLine() << ss.float3Decl("sign3") << " = sign(mirrorin);";
    ss.newLine() << ss.float3Decl("Eprime") << " = abs(mirrorin) + " << ss.float3Const(primeMirror) << ";";
    ss.newLine() << ss.float3Decl("isAboveBreak") << " = " << ss.float3GreaterThan("Eprime", ss.float3Const(primeBreak)) << ";";
   
    // Gamma Segment.
    ss.newLine() << ss.float3Decl("E_gamma") << " = pow( Eprime * " << ss.float3Const(1.0/gammaSeg_slope) << ","
        << ss.float3Const(1.0/gammaSeg_power) << ") - " << ss.float3Const(gammaSeg_off) << ";";
    
    // Log Segment.
    ss.newLine() << ss.float3Decl("E_log") << " = (exp((Eprime - " << ss.float3Const(logSeg_logOff) << ") * " 
        << ss.float3Const(1.0/logSeg_logSlope) << ") - " << ss.float3Const(logSeg_linOff) << ") * "
        << ss.float3Const(1.0/logSeg_linSlope) << ";";
    
    // Combine log and gamma parts.
    ss.newLine() << pxl << ".rgb = sign3 * (isAboveBreak * E_log + ( " << ss.float3Const(1.0f) << " - isAboveBreak ) * E_gamma);";
}

void Add_LIN_TO_DOUBLE_LOG(
    GpuShaderCreatorRcPtr& shaderCreator, 
    GpuShaderText& ss, 
    const FixedFunctionOpData::Params& params)
{
    // Get parameters, baking the log base conversion into 'logSlope'.
    double base             = params[0];
    double break1           = params[1];
    double break2           = params[2];
    double logSeg1_logSlope = params[3] / std::log(base);
    double logSeg1_logOff   = params[4];
    double logSeg1_linSlope = params[5];
    double logSeg1_linOff   = params[6];
    double logSeg2_logSlope = params[7] / std::log(base);
    double logSeg2_logOff   = params[8];
    double logSeg2_linSlope = params[9];
    double logSeg2_linOff   = params[10];
    double linSeg_slope     = params[11];
    double linSeg_off       = params[12];

    // Linear segment may not exist or be valid, thus we include the break
    // points in the log segments. Also passing zero or negative value to the
    // log functions are not guarded for, it should be guaranteed by the
    // parameters for the expected working range.
    
    //if (in <= m_break1)
    //    out = m_logSeg1.logSlope * std::log(m_logSeg1.linSlope * x + m_logSeg1.linOff) + m_logSeg1.logOff;
    //else if (in < m_break2)
    //    out = m_linSeg.slope * x + m_linSeg.off;
    //else
    //    out = m_logSeg2.logSlope * std::log(m_logSeg2.linSlope * x + m_logSeg2.linOff) + m_logSeg2.logOff;
    
    const std::string pix(shaderCreator->getPixelName());
    const std::string pix3 = pix + ".rgb";

    ss.newLine() << ss.float3Decl("isSegment1") << " = " << ss.float3GreaterThanEqual(ss.float3Const(break1), pix3) << ";";
    ss.newLine() << ss.float3Decl("isSegment3") << " = " << ss.float3GreaterThanEqual(pix3, ss.float3Const(break2)) << ";";
    ss.newLine() << ss.float3Decl("isSegment2") << " = " << ss.float3Const(1.0f) << " - isSegment1 - isSegment3;";

    // Log Segment 1.
    // TODO: This segment usually handles very dark (even negative) values, thus
    // is rarely hit. As an optimization we can use "any()" to skip this in a
    // branch (needs benchmarking to see if it's worth the effort).
    ss.newLine();
    ss.newLine() << ss.float3Decl("logSeg1") << " = " << 
        pix3 << " * " << ss.float3Const(logSeg1_linSlope) << " + " << ss.float3Const(logSeg1_linOff) << ";";
    ss.newLine() << "logSeg1 = " << 
        ss.float3Const(logSeg1_logSlope) << " * log( logSeg1 ) + " << ss.float3Const(logSeg1_logOff) << ";";

    // Log Segment 2.
    ss.newLine();
    ss.newLine() << ss.float3Decl("logSeg2") << " = " <<
        pix3 << " * " << ss.float3Const(logSeg2_linSlope) << " + " << ss.float3Const(logSeg2_linOff) << ";";
    ss.newLine() << "logSeg2 = " <<
        ss.float3Const(logSeg2_logSlope) << " * log( logSeg2 ) + " << ss.float3Const(logSeg2_logOff) << ";";

    // Linear Segment.
    ss.newLine();
    ss.newLine() << ss.float3Decl("linSeg") << "= " << 
        ss.float3Const(linSeg_slope) << " * " << pix3 << " + " << ss.float3Const(linSeg_off) << ";";

    // Combine segments.
    ss.newLine();
    ss.newLine() << pix3 << " = isSegment1 * logSeg1 + isSegment2 * linSeg + isSegment3 * logSeg2;"; 
}

void Add_DOUBLE_LOG_TO_LIN(
    GpuShaderCreatorRcPtr& shaderCreator, 
    GpuShaderText& ss, 
    const FixedFunctionOpData::Params& params)
{
    // Baking the log base conversion into 'logSlope'.
    double base             = params[0];
    double break1           = params[1];
    double break2           = params[2];
    double logSeg1_logSlope = params[3] / std::log(base);
    double logSeg1_logOff   = params[4];
    double logSeg1_linSlope = params[5];
    double logSeg1_linOff   = params[6];
    double logSeg2_logSlope = params[7] / std::log(base);
    double logSeg2_logOff   = params[8];
    double logSeg2_linSlope = params[9];
    double logSeg2_linOff   = params[10];
    double linSeg_slope     = params[11];
    double linSeg_off       = params[12];

    double break1Log = logSeg1_logSlope * std::log(logSeg1_linSlope * break1 + logSeg1_linOff) + logSeg1_logOff;
    double break2Log = logSeg2_logSlope * std::log(logSeg2_linSlope * break2 + logSeg2_linOff) + logSeg2_logOff;

    // if (y <= m_break1Log)
    //     x = (std::exp((y - m_logSeg1.logOff) / m_logSeg1.logSlope) - m_logSeg1.linOff) / m_logSeg1.linSlope;
    // else if (y < m_break2Log)
    //     x = (y - m_linSeg.off) / m_linSeg.slope;
    // else
    //     x = (std::exp((y - m_logSeg2.logOff) / m_logSeg2.logSlope) - m_logSeg2.linOff) / m_logSeg2.linSlope;

    const std::string pix(shaderCreator->getPixelName());
    const std::string pix3 = pix + ".rgb";

    // This assumes the forward function is monotonically increasing.
    ss.newLine() << ss.float3Decl("isSegment1") << " = " << ss.float3GreaterThanEqual(ss.float3Const(break1Log), pix3) << ";";
    ss.newLine() << ss.float3Decl("isSegment3") << " = " << ss.float3GreaterThanEqual(pix3, ss.float3Const(break2Log)) << ";";
    ss.newLine() << ss.float3Decl("isSegment2") << " = " << ss.float3Const(1.0f) << " - isSegment1 - isSegment3;";

    // Log Segment 1.
    // TODO: This segment usually handles very dark (even negative) values, thus
    // is rarely hit. As an optimization we can use "any()" to skip this in a
    // branch (needs benchmarking to see if it's worth the effort).
    ss.newLine();
    ss.newLine() << ss.float3Decl("logSeg1") << " = (" <<
        pix3 << " - " << ss.float3Const(logSeg1_logOff) << ") * " << ss.float3Const(1.0 / logSeg1_logSlope) << ";";
    ss.newLine() << "logSeg1 = (" <<
        "exp(logSeg1) - " << ss.float3Const(logSeg1_linOff) << ") * " << ss.float3Const(1.0 / logSeg1_linSlope) << ";";

    // Log Segment 2.
    ss.newLine();
    ss.newLine() << ss.float3Decl("logSeg2") << " = (" <<
        pix3 << " - " << ss.float3Const(logSeg2_logOff) << ") * " << ss.float3Const(1.0 / logSeg2_logSlope) << ";";
    ss.newLine() << "logSeg2 = (" <<
        "exp(logSeg2) - " << ss.float3Const(logSeg2_linOff) << ") * " << ss.float3Const(1.0 / logSeg2_linSlope) << ";";

    // Linear Segment.
    ss.newLine();
    ss.newLine() << ss.float3Decl("linSeg") << " = (" <<
        pix3 << " - " << ss.float3Const(linSeg_off) << ") * " << ss.float3Const(1.0 / linSeg_slope) << ";";

    // Combine segments.
    ss.newLine();
    ss.newLine() << pix3 << " = isSegment1 * logSeg1 + isSegment2 * linSeg + isSegment3 * logSeg2;";
}

void GetFixedFunctionGPUShaderProgram(GpuShaderCreatorRcPtr & shaderCreator,
                                      ConstFixedFunctionOpDataRcPtr & func)
{
    GpuShaderText ss(shaderCreator->getLanguage());
    ss.indent();

    ss.newLine() << "";
    ss.newLine() << "// Add FixedFunction '"
                 << FixedFunctionOpData::ConvertStyleToString(func->getStyle(), true)
                 << "' processing";
    ss.newLine() << "";
    ss.newLine() << "{";
    ss.indent();

    switch(func->getStyle())
    {
        case FixedFunctionOpData::ACES_RED_MOD_03_FWD:
        {
            Add_RedMod_03_Fwd_Shader(shaderCreator, ss);
            break;
         }
        case FixedFunctionOpData::ACES_RED_MOD_03_INV:
        {
            Add_RedMod_03_Inv_Shader(shaderCreator, ss);
            break;
        }
        case FixedFunctionOpData::ACES_RED_MOD_10_FWD:
        {
            Add_RedMod_10_Fwd_Shader(shaderCreator, ss);
            break;
        }
        case FixedFunctionOpData::ACES_RED_MOD_10_INV:
        {
            Add_RedMod_10_Inv_Shader(shaderCreator, ss);
            break;
        }
        case FixedFunctionOpData::ACES_GLOW_03_FWD:
        {
            Add_Glow_03_Fwd_Shader(shaderCreator, ss, 0.075f, 0.1f);
            break;
        }
        case FixedFunctionOpData::ACES_GLOW_03_INV:
        {
            Add_Glow_03_Inv_Shader(shaderCreator, ss, 0.075f, 0.1f);
            break;
        }
        case FixedFunctionOpData::ACES_GLOW_10_FWD:
        {
            // Use 03 renderer with different params.
            Add_Glow_03_Fwd_Shader(shaderCreator, ss, 0.05f, 0.08f);
            break;
        }
        case FixedFunctionOpData::ACES_GLOW_10_INV:
        {
            // Use 03 renderer with different params.
            Add_Glow_03_Inv_Shader(shaderCreator, ss, 0.05f, 0.08f);
            break;
        }
        case FixedFunctionOpData::ACES_DARK_TO_DIM_10_FWD:
        {
            Add_Surround_10_Fwd_Shader(shaderCreator, ss, 0.9811f);
            break;
        }
        case FixedFunctionOpData::ACES_DARK_TO_DIM_10_INV:
        {
            // Call forward renderer with the inverse gamma.
            Add_Surround_10_Fwd_Shader(shaderCreator, ss, 1.0192640913260627f);
            break;
        }
        case FixedFunctionOpData::ACES_GAMUT_COMP_13_FWD:
        {
            Add_GamutComp_13_Fwd_Shader(
                ss,
                shaderCreator,
                (float) func->getParams()[0],
                (float) func->getParams()[1],
                (float) func->getParams()[2],
                (float) func->getParams()[3],
                (float) func->getParams()[4],
                (float) func->getParams()[5],
                (float) func->getParams()[6]
            );
            break;
        }
        case FixedFunctionOpData::ACES_GAMUT_COMP_13_INV:
        {
            Add_GamutComp_13_Inv_Shader(
                ss,
                shaderCreator,
                (float) func->getParams()[0],
                (float) func->getParams()[1],
                (float) func->getParams()[2],
                (float) func->getParams()[3],
                (float) func->getParams()[4],
                (float) func->getParams()[5],
                (float) func->getParams()[6]
            );
            break;
        }
        case FixedFunctionOpData::ACES_OUTPUT_TRANSFORM_20_FWD:
        {
            Add_ACES_OutputTransform_Fwd_Shader(shaderCreator, ss, func->getParams());
            break;
        }
        case FixedFunctionOpData::ACES_OUTPUT_TRANSFORM_20_INV:
        {
            Add_ACES_OutputTransform_Inv_Shader(shaderCreator, ss, func->getParams());
            break;
        }
        case FixedFunctionOpData::ACES_RGB_TO_JMh_20:
        {
            Add_RGB_to_JMh_Shader(shaderCreator, ss, func->getParams());
            break;
        }
        case FixedFunctionOpData::ACES_JMh_TO_RGB_20:
        {
            Add_JMh_to_RGB_Shader(shaderCreator, ss, func->getParams());
            break;
        }
        case FixedFunctionOpData::ACES_TONESCALE_COMPRESS_20_FWD:
        {
            Add_Tonescale_Compress_Fwd_Shader(shaderCreator, ss, func->getParams());
            break;
        }
        case FixedFunctionOpData::ACES_TONESCALE_COMPRESS_20_INV:
        {
            Add_Tonescale_Compress_Inv_Shader(shaderCreator, ss, func->getParams());
            break;
        }
        case FixedFunctionOpData::ACES_GAMUT_COMPRESS_20_FWD:
        {
            Add_Gamut_Compress_Fwd_Shader(shaderCreator, ss, func->getParams());
            break;
        }
        case FixedFunctionOpData::ACES_GAMUT_COMPRESS_20_INV:
        {
            Add_Gamut_Compress_Inv_Shader(shaderCreator, ss, func->getParams());
            break;
        }
        case FixedFunctionOpData::REC2100_SURROUND_FWD:
        {
            Add_Rec2100_Surround_Shader(shaderCreator, ss, 
                                        (float) func->getParams()[0], true);
            break;
        }
        case FixedFunctionOpData::REC2100_SURROUND_INV:
        {
            Add_Rec2100_Surround_Shader(shaderCreator, ss, 
                                        (float) func->getParams()[0], false);
            break;
        }
        case FixedFunctionOpData::RGB_TO_HSV:
        {
            Add_RGB_TO_HSV(shaderCreator, ss);
            break;
        }
        case FixedFunctionOpData::HSV_TO_RGB:
        {
            Add_HSV_TO_RGB(shaderCreator, ss);
            break;
        }
        case FixedFunctionOpData::XYZ_TO_xyY:
        {
            Add_XYZ_TO_xyY(shaderCreator, ss);
            break;
        }
        case FixedFunctionOpData::xyY_TO_XYZ:
        {
            Add_xyY_TO_XYZ(shaderCreator, ss);
            break;
        }
        case FixedFunctionOpData::XYZ_TO_uvY:
        {
            Add_XYZ_TO_uvY(shaderCreator, ss);
            break;
        }
        case FixedFunctionOpData::uvY_TO_XYZ:
        {
            Add_uvY_TO_XYZ(shaderCreator, ss);
            break;
        }
        case FixedFunctionOpData::XYZ_TO_LUV:
        {
            Add_XYZ_TO_LUV(shaderCreator, ss);
            break;
        }
        case FixedFunctionOpData::LUV_TO_XYZ:
        {
            Add_LUV_TO_XYZ(shaderCreator, ss);
            break;
        }
        case FixedFunctionOpData::LIN_TO_PQ:
        {
            Add_LIN_TO_PQ(shaderCreator, ss);
            break;
        }
        case FixedFunctionOpData::PQ_TO_LIN:
        {
            Add_PQ_TO_LIN(shaderCreator, ss);
            break;
        }
        case FixedFunctionOpData::LIN_TO_GAMMA_LOG:
        {
            Add_LIN_TO_GAMMA_LOG(shaderCreator, ss, func->getParams());
            break;
        }
        case FixedFunctionOpData::GAMMA_LOG_TO_LIN:
        {
            Add_GAMMA_LOG_TO_LIN(shaderCreator, ss, func->getParams());
            break;
        }
        case FixedFunctionOpData::LIN_TO_DOUBLE_LOG:
        {
            Add_LIN_TO_DOUBLE_LOG(shaderCreator, ss, func->getParams());
            break;
        }
        case FixedFunctionOpData::DOUBLE_LOG_TO_LIN:
        {
            Add_DOUBLE_LOG_TO_LIN(shaderCreator, ss, func->getParams());
            break;
        }

    }

    ss.dedent();
    ss.newLine() << "}";

    ss.dedent();
    shaderCreator->addToFunctionShaderCode(ss.string().c_str());
}

} // OCIO_NAMESPACE
