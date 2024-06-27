#include <cstring>
#include <vector>

#include <OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;
#include <imageioapphelpers/imageio.h>

#include "ACES2CPUHelpers.h"


// #define RANDOM

#ifdef RANDOM

std::vector<float> setup_data()
{
    // Create an arbitrary 4K RGBA image.
    static constexpr size_t width  = 3840;
    static constexpr size_t height = 2160;
    static constexpr size_t numChannels = 4;

    static constexpr size_t maxElts = width * height;

    std::vector<float> img_f32_ref;

    // Generate a synthetic image by emulating a LUT3D identity algorithm that steps through
    // many different colors.  Need to avoid a constant image, simple gradients, or anything
    // that would result in more cache hits than a typical image.  Also, want to step through a 
    // wide range of colors, including outside [0,1], in case some algorithms are faster or
    // slower for certain colors.

    static constexpr size_t length   = 201;
    static constexpr float stepValue = 1.0f / ((float)length - 1.0f);

    static constexpr float min   = -1.0f;
    static constexpr float max   =  2.0f;
    static constexpr float range = max - min;

    img_f32_ref.resize(maxElts * numChannels);

    // Retrofit value in the range.
    auto adjustValue = [](float val) -> float
    {
        return val * range + min;
    };

    for (size_t idx = 0; idx < maxElts; ++idx)
    {
        img_f32_ref[numChannels * idx + 0] = adjustValue( ((idx / length / length) % length) * stepValue );
        img_f32_ref[numChannels * idx + 1] = adjustValue( ((idx / length) % length) * stepValue );
        img_f32_ref[numChannels * idx + 2] = adjustValue( (idx % length) * stepValue );

        img_f32_ref[numChannels * idx + 3] = adjustValue( float(idx) / maxElts );
    }

    return img_f32_ref;
}

#else

std::vector<float> setup_data()
{
    const std::string inputImage = "/user_data/RND/dev/aces-images/ACES/DigitalLAD.2048x1556.exr";

    OCIO::ImageIO imgInput;
    imgInput.read(inputImage, OCIO::BIT_DEPTH_F32);
    std::cerr << "nChannels: " << imgInput.getNumChannels() << "\n";

    const unsigned int size_float = imgInput.getImageBytes() / sizeof(float);
    std::vector<float> vec_data(size_float);
    std::memcpy(vec_data.data(), imgInput.getData(), imgInput.getImageBytes());

    return vec_data;
}

#endif

int main(int, char **)
{
    using namespace OCIO::ACES2CPUHelpers;

    // Debug prints

    print_m33("AP0 to XYZ", srgb_100nits_odt.INPUT_RGB_TO_XYZ);
    print_m33("XYZ_TO_AP1", XYZ_TO_AP1);
    print_m33("AP1_TO_XYZ", AP1_TO_XYZ);

    print_m33("LIMIT_RGB_TO_XYZ", srgb_100nits_odt.LIMIT_RGB_TO_XYZ);
    print_m33("LIMIT_XYZ_TO_RGB", srgb_100nits_odt.LIMIT_XYZ_TO_RGB);

    print_m33("MATRIX_16", MATRIX_16);
    print_m33("MATRIX_16_INV", MATRIX_16_INV);
    print_m33("panlrcm", panlrcm);

    print_v("Input F_L", srgb_100nits_odt.inputJMhParams.F_L);
    print_v("Input z", srgb_100nits_odt.inputJMhParams.z);
    print_v3("Input D_RGB", srgb_100nits_odt.inputJMhParams.D_RGB);
    print_v("Input A_w", srgb_100nits_odt.inputJMhParams.A_w);
    print_v("Input A_w_J", srgb_100nits_odt.inputJMhParams.A_w_J);

    print_v("Limit F_L", srgb_100nits_odt.limitJMhParams.F_L);
    print_v("Limit z", srgb_100nits_odt.limitJMhParams.z);
    print_v3("Limit D_RGB", srgb_100nits_odt.limitJMhParams.D_RGB);
    print_v("Limit A_w", srgb_100nits_odt.limitJMhParams.A_w);
    print_v("Limit A_w_J", srgb_100nits_odt.limitJMhParams.A_w_J);

    print_v("n_r", srgb_100nits_odt.n_r);
    print_v("g  ", srgb_100nits_odt.g  );
    print_v("t_1", srgb_100nits_odt.t_1);
    print_v("c_t", srgb_100nits_odt.c_t);
    print_v("s_2", srgb_100nits_odt.s_2);
    print_v("u_2", srgb_100nits_odt.u_2);
    print_v("m_2", srgb_100nits_odt.m_2);

    print_v("limit_J_max", srgb_100nits_odt.limit_J_max);
    print_v("mid_J      ", srgb_100nits_odt.mid_J      );
    print_v("model_gamma", srgb_100nits_odt.model_gamma);
    print_v("sat        ", srgb_100nits_odt.sat        );
    print_v("sat_thr    ", srgb_100nits_odt.sat_thr    );
    print_v("compr      ", srgb_100nits_odt.compr      );
    print_v("focus_dist ", srgb_100nits_odt.focus_dist );
    print_gt("reach_gamut_table", srgb_100nits_odt.reach_gamut_table);
    print_gt("reach_cusp_table", srgb_100nits_odt.reach_cusp_table);
    print_gt("gamut_cusp_table", srgb_100nits_odt.gamut_cusp_table);
    print_gmt("upperHullGammaTable", srgb_100nits_odt.upperHullGammaTable);

    return 0;
    //

    const auto pixels = setup_data();

    for (unsigned int i = 0; i < pixels.size(); i += 4)
    {
        const f3 ACES = {
            pixels[i],
            pixels[i + 1],
            pixels[i + 2]
        };
        const f3 RGB = outputTransform_fwd(
            ACES,
            srgb_100nits_odt
        );
    }

    return 0;
}