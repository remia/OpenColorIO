// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include "Transform.h"
#include "ops/lut1d/Lut1DOpData.h"


namespace OCIO_NAMESPACE
{

namespace ACES2
{

//
// Table lookups
//

float wrap_to_360(float hue)
{
    float y = std::fmod(hue, 360.f);
    if ( y < 0.f)
    {
        y = y + 360.f;
    }
    return y;
}

float base_hue_for_position(int i_lo, int table_size) 
{
    const float result = i_lo * 360.f / table_size;
    return result;
}

int hue_position_in_uniform_table(float hue, int table_size)
{
    const float wrapped_hue = wrap_to_360(hue);
    return int(wrapped_hue / 360.f * (float) table_size);
}

int next_position_in_table(int entry, int table_size)
{
    return (entry + 1) % table_size;
}

int clamp_to_table_bounds(int entry, int table_size)
{
    return std::min(table_size - 1, std::max(0, entry));
}

f2 cusp_from_table(float h, const Table3D &gt, const Table1DLookup &ht)
{
    h = SanitizeFloat(h);

#ifdef NEW_CUSP_SAMPLING

    float lut_h_min = ht.start;
    float lut_h_max = ht.end;
    float lut_h_range = lut_h_max - lut_h_min;
    float lut_h = ((h / 360.f) - lut_h_min) / lut_h_range;
    float f_lo = lut_h * (ht.total_size - 1);

    int ii_lo = int(f_lo);
    int ii_hi = ii_lo + 1;
    float f = f_lo - int(f_lo);
    int i_lo = int(lerpf(ht.table[ii_lo], ht.table[ii_hi], f) * (gt.total_size - 1));
    int i_hi = clamp_to_table_bounds(i_lo + 1, gt.total_size);

#else

    int i_lo = 0;
    int i_hi = gt.base_index + gt.size; // allowed as we have an extra entry in the table
    int i = clamp_to_table_bounds(hue_position_in_uniform_table(h, gt.size) + gt.base_index, gt.total_size);

    while (i_lo + 1 < i_hi)
    {
        if (h > gt.table[i][2])
        {
            i_lo = i;
        }
        else
        {
            i_hi = i;
        }
        i = clamp_to_table_bounds((i_lo + i_hi) / 2, gt.total_size);
    }

    i_hi = std::max(1, i_hi);

#endif

    const f3 lo {
        gt.table[i_hi-1][0],
        gt.table[i_hi-1][1],
        gt.table[i_hi-1][2]
    };

    const f3 hi {
        gt.table[i_hi][0],
        gt.table[i_hi][1],
        gt.table[i_hi][2]
    };

    const float t = (h - lo[2]) / (hi[2] - lo[2]);
    const float cuspJ = lerpf(lo[0], hi[0], t);
    const float cuspM = lerpf(lo[1], hi[1], t);

    return { cuspJ, cuspM };
}

float reach_m_from_table(float h, const ACES2::Table1D &gt)
{
    const int i_lo = clamp_to_table_bounds(hue_position_in_uniform_table(h, gt.size), gt.total_size);
    const int i_hi = clamp_to_table_bounds(next_position_in_table(i_lo, gt.size), gt.total_size);

    const float t = (h - i_lo) / (i_hi - i_lo);
    return lerpf(gt.table[i_lo], gt.table[i_hi], t);
}

float hue_dependent_upper_hull_gamma(float h, const ACES2::Table1D &gt)
{
    const int i_lo = clamp_to_table_bounds(hue_position_in_uniform_table(h, gt.size) + gt.base_index, gt.total_size);
    const int i_hi = clamp_to_table_bounds(next_position_in_table(i_lo, gt.size), gt.total_size);

    const float base_hue = (float) (i_lo - gt.base_index);

    const float t = wrap_to_360(h) - base_hue;

    return lerpf(gt.table[i_lo], gt.table[i_hi], t);
}

//
// CAM
//

// Post adaptation non linear response compression
float panlrc_forward(float v, float F_L)
{
    const float F_L_v = powf(F_L * std::abs(v) / reference_luminance, 0.42f);
    // Note that std::copysign(1.f, 0.f) returns 1 but the CTL copysign(1.,0.) returns 0.
    // TODO: Should we change the behaviour?
    return (400.f * std::copysign(1.f, v) * F_L_v) / (27.13f + F_L_v);
}

float panlrc_inverse(float v, float F_L)
{
    return std::copysign(1.f, v) * reference_luminance / F_L * powf((27.13f * std::abs(v) / (400.f - std::abs(v))), 1.f / 0.42f);
}

// Optimization used during initialization
float Y_to_J(float Y, const JMhParams &params)
{
    float F_L_Y = powf(params.F_L * std::abs(Y) / reference_luminance, 0.42f);
    return std::copysign(1.f, Y) * reference_luminance * powf(((400.f * F_L_Y) / (27.13f + F_L_Y)) / params.A_w_J, surround[1] * params.z);
}

f3 RGB_to_JMh(const f3 &RGB, const JMhParams &p)
{
    const float red = RGB[0];
    const float grn = RGB[1];
    const float blu = RGB[2];

    const float red_m = red * p.MATRIX_RGB_to_CAM16[0] + grn * p.MATRIX_RGB_to_CAM16[1] + blu * p.MATRIX_RGB_to_CAM16[2];
    const float grn_m = red * p.MATRIX_RGB_to_CAM16[3] + grn * p.MATRIX_RGB_to_CAM16[4] + blu * p.MATRIX_RGB_to_CAM16[5];
    const float blu_m = red * p.MATRIX_RGB_to_CAM16[6] + grn * p.MATRIX_RGB_to_CAM16[7] + blu * p.MATRIX_RGB_to_CAM16[8];

    const float red_a =  panlrc_forward(red_m * p.D_RGB[0], p.F_L);
    const float grn_a =  panlrc_forward(grn_m * p.D_RGB[1], p.F_L);
    const float blu_a =  panlrc_forward(blu_m * p.D_RGB[2], p.F_L);

    const float A = 2.f * red_a + grn_a + 0.05f * blu_a;
    const float a = red_a - 12.f * grn_a / 11.f + blu_a / 11.f;
    const float b = (red_a + grn_a - 2.f * blu_a) / 9.f;

    const float J = 100.f * powf(A / p.A_w, surround[1] * p.z);

    const float M = J == 0.f ? 0.f : 43.f * surround[2] * sqrt(a * a + b * b);

    const float PI = 3.14159265358979f;
    const float h_rad = std::atan2(b, a);
    float h = std::fmod(h_rad * 180.f / PI, 360.f);
    if (h < 0.f)
    {
        h += 360.f;
    }

    return {J, M, h};
}

f3 JMh_to_RGB(const f3 &JMh, const JMhParams &p)
{
    const float J = JMh[0];
    const float M = JMh[1];
    const float h = JMh[2];

    const float PI = 3.14159265358979f;
    const float h_rad = h * PI / 180.f;

    const float scale = M / (43.f * surround[2]);
    const float A = p.A_w * powf(J / 100.f, 1.f / (surround[1] * p.z));
    const float a = scale * cos(h_rad);
    const float b = scale * sin(h_rad);

    const float red_a = (460.f * A + 451.f * a + 288.f * b) / 1403.f;
    const float grn_a = (460.f * A - 891.f * a - 261.f * b) / 1403.f;
    const float blu_a = (460.f * A - 220.f * a - 6300.f * b) / 1403.f;

    float red_m = panlrc_inverse(red_a, p.F_L) / p.D_RGB[0];
    float grn_m = panlrc_inverse(grn_a, p.F_L) / p.D_RGB[1];
    float blu_m = panlrc_inverse(blu_a, p.F_L) / p.D_RGB[2];

    const float red = red_m * p.MATRIX_CAM16_to_RGB[0] + grn_m * p.MATRIX_CAM16_to_RGB[1] + blu_m * p.MATRIX_CAM16_to_RGB[2];
    const float grn = red_m * p.MATRIX_CAM16_to_RGB[3] + grn_m * p.MATRIX_CAM16_to_RGB[4] + blu_m * p.MATRIX_CAM16_to_RGB[5];
    const float blu = red_m * p.MATRIX_CAM16_to_RGB[6] + grn_m * p.MATRIX_CAM16_to_RGB[7] + blu_m * p.MATRIX_CAM16_to_RGB[8];

    return {red, grn, blu};
}

//
// Tonescale / Chroma compress
//

float chroma_compress_norm(float h, float chroma_compress_scale)
{
    const float PI = 3.14159265358979f;
    const float h_rad = h / 180.f * PI;
    const float a = cos(h_rad);
    const float b = sin(h_rad);
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

    return M * chroma_compress_scale;
}

float toe_fwd( float x, float limit, float k1_in, float k2_in)
{
    if (x > limit)
    {
        return x;
    }

    const float k2 = std::max(k2_in, 0.001f);
    const float k1 = sqrt(k1_in * k1_in + k2 * k2);
    const float k3 = (limit + k1) / (limit + k2);
    return 0.5f * (k3 * x - k1 + sqrt((k3 * x - k1) * (k3 * x - k1) + 4.f * k2 * k3 * x));
}

float toe_inv( float x, float limit, float k1_in, float k2_in)
{
    if (x > limit)
    {
        return x;
    }

    const float k2 = std::max(k2_in, 0.001f);
    const float k1 = sqrt(k1_in * k1_in + k2 * k2);
    const float k3 = (limit + k1) / (limit + k2);
    return (x * x + k1 * x) / (k3 * (x + k2));
}

#ifdef TONESCALE_LUT

static constexpr float tonescaleJ[1024] = {0.0, 0.007508731158294459, 0.03699081563480184, 0.09396498326932942, 0.1819120429406957, 0.30334639597195356, 0.46011529590862027, 0.6535290046906179, 0.8844306316120482, 1.153241170180272, 1.4599944502191637, 1.8043688747665063, 2.185719232593326, 2.603110002437569, 3.0553504873526682, 3.541031473416213, 4.058562733835746, 4.60621051420448, 5.182134086687146, 5.784420513069797, 6.411116877859099, 7.060259415886554, 7.729899140889026, 8.418123763310884, 9.123075852797966, 9.842967343947356, 10.576090597622159, 11.32082631301251, 12.075648638940612, 12.839127859930963, 13.609931037681452, 14.386820976586193, 15.168653857596762, 15.95437585223719, 16.743018991639527, 17.533696526947303, 18.325597979531967, 19.117984043724597, 19.910181472165213, 20.701578044954108, 21.49161769874918, 22.279795870739, 23.065655094814606, 23.848780872943077, 24.628797833338474, 25.40536617814168, 26.178178416576927, 26.946956374593416, 27.71144846850211, 28.471427227794976, 29.226687050943024, 29.977042177299683, 30.722324858116796, 31.462383709967728, 32.19708223445189, 32.92629748883481, 33.64991889318676, 34.36784716056481, 35.079993337794036, 35.7862779454166, 36.486630206362484, 37.180987353846106, 37.869294009888186, 38.551501626705615, 39.22756798399117, 39.89745673582709, 40.561137001635004, 41.218582996167626, 41.869773694093524, 42.51469252522001, 43.15332709684442, 43.7856689401244, 44.411713277715705, 45.03145881024677, 45.64490751948527, 46.252064486306814, 46.85293772180169, 47.447538010057166, 48.03587876133076, 48.61797587448725, 49.19384760771213, 49.763514456636, 50.32699903911433, 50.88432598600067, 51.435521837336964, 51.98061494345668, 52.51963537056209, 53.05261481039295, 53.5795864936538, 54.10058510690933, 54.615646712695906, 55.12480867262899, 55.6281095733155, 56.125589154904, 56.61728824212749, 57.103248677711264, 57.583513258034756, 58.05812567094931, 58.52713043566576, 58.9905728446354, 59.44849890735642, 59.900955296044714, 60.34798929311472, 60.78964874041991, 61.225981990207664, 61.657037857746076, 62.08286557558387, 62.503514749405845, 62.919035315449534, 63.32947749944889, 63.73489177707333, 64.13532883583001, 64.53083953839972, 64.92147488737561, 65.30728599137508, 65.68832403249625, 66.06464023508896, 66.43628583581189, 66.803312054947, 67.16577006894212, 67.52371098415301, 67.87718581175655, 68.22624544380591, 68.57094063039924, 68.91132195793354, 69.24743982841474, 69.57934443979592, 69.90708576731544, 70.23071354580644, 70.55027725295022, 70.86582609344552, 71.17740898406586, 71.48507453957832, 71.78887105949624, 72.08884651563913, 72.38504854047387, 72.67752441621057, 72.966321064628, 73.25148503760305, 73.53306250831932, 73.81109926313081, 74.0856406940564, 74.35673179188153, 74.62441713984477, 74.88874090788593, 75.14974684743395, 75.40747828671329, 75.66197812654748, 75.91328883663934, 76.16145245230811, 76.40651057166357, 76.64850435319876, 76.88747451378194, 77.12346132703117, 77.35650462205274, 77.5866437825274, 77.81391774612757, 78.03836500425004, 78.26002360204811, 78.47893113874905, 78.69512476824174, 78.90864119992077, 79.11951669977408, 79.3277870917001, 79.5334877590425, 79.73665364633052, 79.93731926121265, 80.13551867657284, 80.3312855328184, 80.52465304032881, 80.71565398205574, 80.90432071626446, 81.09068517940766, 81.27477888912217, 81.45663294734071, 81.63627804351022, 81.81374445790863, 81.98906206505298, 82.16226033719161, 82.33336834787322, 82.50241477558635, 82.6694279074635, 82.83443564304281, 82.9974654980825, 83.1585446084221, 83.31769973388545, 83.4749572622204, 83.63034321307009, 83.78388324197229, 83.93560264438142, 84.08552635970956, 84.23367897538292, 84.38008473090957, 84.52476752195521, 84.66775090442346, 84.80905809853797, 84.94871199292284, 85.08673514867887, 85.22314980345293, 85.35797787549791, 85.49124096772108, 85.62296037171818, 85.75315707179193, 85.88185174895199, 86.00906478489559, 86.1348162659663, 86.25912598708982, 86.38201345568508, 86.50349789554906, 86.62359825071464, 86.74233318927975, 86.85972110720648, 86.9757801320902, 87.0905281268963, 87.20398269366476, 87.31616117718137, 87.4270806686147, 87.5367580091186, 87.645209793399, 87.75245237324523, 87.85850186102445, 87.96337413313992, 88.0670848334516, 88.16964937665944, 88.27108295164889, 88.37140052479835, 88.47061684324851, 88.56874643813308, 88.66580362777127, 88.76180252082182, 88.85675701939782, 88.95068082214378, 89.04358742727318, 89.13549013556823, 89.22640205334054, 89.31633609535375, 89.40530498770762, 89.49332127068419, 89.58039730155572, 89.6665452573551, 89.75177713760812, 89.83610476702907, 89.9195397981785, 90.00209371408448, 90.08377783082696, 90.16460330008567, 90.24458111165235, 90.3237220959065, 90.40203692625603, 90.47953612154275, 90.5562300484126, 90.63212892365166, 90.70724281648774, 90.78158165085823, 90.8551552076442, 90.92797312687154, 91.00004490987916, 91.07137992145455, 91.14198739193765, 91.21187641929247, 91.28105597114772, 91.34953488680625, 91.41732187922398, 91.48442553695837, 91.55085432608716, 91.61661659209757, 91.68172056174652, 91.74617434489144, 91.8099859362939, 91.87316321739384, 91.93571395805718, 91.99764581829567, 92.05896634996012, 92.11968299840674, 92.17980310413775, 92.23933390441572, 92.29828253485303, 92.3566560309757, 92.41446132976282, 92.47170527116161, 92.52839459957836, 92.58453596534581, 92.640135926167, 92.69520094853658, 92.74973740913897, 92.80375159622474, 92.85724971096447, 92.91023786878165, 92.96272210066365, 93.01470835445214, 93.06620249611261, 93.11721031098381, 93.1677375050068, 93.21778970593472, 93.26737246452271, 93.31649125569918, 93.36515147971785, 93.41335846329163, 93.46111746070795, 93.5084336549265, 93.55531215865909, 93.60175801543228, 93.64777620063266, 93.69337162253572, 93.73854912331795, 93.78331348005247, 93.82766940568908, 93.87162155001822, 93.91517450061939, 93.9583327837947, 94.00110086548693, 94.0434831521831, 94.08548399180357, 94.12710767457658, 94.168358433899, 94.20924044718328, 94.24975783669036, 94.28991467034989, 94.32971496256668, 94.36916267501458, 94.4082617174175, 94.44701594831797, 94.48542917583332, 94.52350515839963, 94.56124760550442, 94.59866017840656, 94.6357464908456, 94.67251010973942, 94.70895455587055, 94.74508330456202, 94.78089978634186, 94.81640738759752, 94.85160945121967, 94.88650927723556, 94.92111012343268, 94.95541520597224, 94.9894276999931, 95.0231507402059, 95.05658742147803, 95.08974079940909, 95.1226138908974, 95.15520967469759, 95.18753109196925, 95.21958104681697, 95.2513624068219, 95.28287800356489, 95.31413063314146, 95.34512305666861, 95.3758580007837, 95.40633815813545, 95.43656618786738, 95.46654471609352, 95.49627633636668, 95.52576361013962, 95.55500906721865, 95.58401520621067, 95.61278449496255, 95.64131937099441, 95.66962224192554, 95.6976954858942, 95.72554145197046, 95.75316246056313, 95.78056080381984, 95.80773874602144, 95.83469852396995, 95.86144234737071, 95.88797239920865, 95.9142908361185, 95.94039978874983, 95.96630136212583, 95.99199763599722, 96.01749066519025, 96.04278247994958, 96.0678750862761, 96.0927704662591, 96.11747057840392, 96.14197735795422, 96.16629271720956, 96.19041854583806, 96.2143567111844, 96.23810905857297, 96.2616774116067, 96.28506357246115, 96.3082693221742, 96.33129642093155, 96.35414660834769, 96.37682160374271, 96.39932310641508, 96.42165279591009, 96.44381233228468, 96.46580335636776, 96.48762749001703, 96.50928633637193, 96.53078148010275, 96.5521144876559, 96.57328690749584, 96.59430027034315, 96.61515608940923, 96.63585586062744, 96.65640106288083, 96.67679315822669, 96.69703359211726, 96.71712379361806, 96.73706517562208, 96.75685913506152, 96.77650705311608, 96.79601029541821, 96.81537021225559, 96.83458813877034, 96.85366539515556, 96.87260328684894, 96.89140310472352, 96.91006612527568, 96.92859361081044, 96.94698680962416, 96.96524695618433, 96.98337527130708, 97.0013729623319, 97.01924122329402, 97.03698123509407, 97.05459416566573, 97.07208117014041, 97.08944339101, 97.10668195828717, 97.12379798966337, 97.14079259066457, 97.15766685480473, 97.17442186373727, 97.19105868740418, 97.20757838418308, 97.22398200103237, 97.24027057363406, 97.25644512653467, 97.27250667328438, 97.28845621657378, 97.30429474836916, 97.3200232500455, 97.33564269251788, 97.351154036371, 97.36655823198672, 97.38185621967004, 97.39704892977312, 97.41213728281788, 97.42712218961648, 97.44200455139051, 97.45678525988825, 97.47146519750046, 97.4860452373747, 97.50052624352762, 97.51490907095626, 97.52919456574737, 97.54338356518555, 97.55747689785966, 97.57147538376799, 97.58537983442177, 97.59919105294756, 97.61290983418782, 97.62653696480068, 97.6400732233577, 97.65351938044093, 97.66687619873814, 97.68014443313713, 97.69332483081857, 97.70641813134762, 97.71942506676442, 97.7323463616732, 97.74518273333027, 97.75793489173083, 97.7706035396947, 97.78318937295076, 97.79569308022033, 97.80811534329943, 97.82045683713999, 97.83271822992985, 97.84490018317197, 97.85700335176212, 97.86902838406618, 97.88097592199576, 97.89284660108333, 97.90464105055608, 97.91635989340888, 97.92800374647635, 97.93957322050392, 97.9510689202179, 97.9624914443947, 97.97384138592926, 97.98511933190242, 97.99632586364739, 98.00746155681544, 98.01852698144093, 98.02952270200502, 98.04044927749901, 98.05130726148661, 98.06209720216549, 98.0728196424281, 98.08347511992145, 98.09406416710658, 98.10458731131672, 98.11504507481514, 98.12543797485203, 98.13576652372082, 98.14603122881351, 98.15623259267558, 98.16637111306011, 98.17644728298121, 98.18646159076653, 98.19641452010968, 98.20630655012138, 98.21613815538026, 98.22590980598311, 98.23562196759417, 98.24527510149423, 98.25486966462867, 98.26440610965523, 98.27388488499106, 98.28330643485906, 98.29267119933395, 98.30197961438752, 98.31123211193325, 98.32042911987072, 98.32957106212908, 98.33865835871018, 98.3476914257313, 98.356670675467, 98.36559651639071, 98.37446935321577, 98.383289586936, 98.39205761486555, 98.40077383067852, 98.40943862444807, 98.41805238268486, 98.42661548837518, 98.43512832101864, 98.44359125666523, 98.4520046679521, 98.4603689241398, 98.46868439114822, 98.47695143159179, 98.48517040481462, 98.49334166692499, 98.5014655708295, 98.50954246626668, 98.51757269984046, 98.52555661505305, 98.53349455233734, 98.54138684908919, 98.54923383969918, 98.5570358555837, 98.56479322521639, 98.57250627415836, 98.5801753250887, 98.58780069783431, 98.59538270939944, 98.60292167399489, 98.61041790306689, 98.61787170532563, 98.62528338677323, 98.63265325073205, 98.63998159787172, 98.64726872623652, 98.6545149312725, 98.6617205058536, 98.66888574030816, 98.67601092244496, 98.68309633757863, 98.69014226855519, 98.69714899577694, 98.70411679722739, 98.71104594849555, 98.71793672280035, 98.72478939101434, 98.73160422168739, 98.73838148107019, 98.74512143313716, 98.7518243396094, 98.75849045997714, 98.76512005152217, 98.77171336933995, 98.77827066636118, 98.78479219337356, 98.79127819904303, 98.79772892993483, 98.80414463053438, 98.81052554326779, 98.81687190852226, 98.82318396466626, 98.82946194806932, 98.83570609312183, 98.84191663225434, 98.84809379595704, 98.85423781279846, 98.86034890944465, 98.86642731067737, 98.87247323941277, 98.87848691671955, 98.8844685618368, 98.89041839219188, 98.89633662341798, 98.90222346937149, 98.90807914214928, 98.9139038521055, 98.91969780786873, 98.92546121635817, 98.93119428280053, 98.93689721074601, 98.94257020208448, 98.94821345706131, 98.95382717429337, 98.95941155078411, 98.96496678193948, 98.97049306158281, 98.97599058197001, 98.98145953380431, 98.98690010625128, 98.99231248695311, 98.99769686204316, 99.00305341616021, 99.0083823324626, 99.01368379264201, 99.01895797693753, 99.02420506414906, 99.02942523165092, 99.0346186554053, 99.03978550997523, 99.04492596853794, 99.05004020289758, 99.05512838349819, 99.06019067943616, 99.06522725847289, 99.07023828704715, 99.07522393028738, 99.0801843520237, 99.08511971480006, 99.09003017988601, 99.09491590728841, 99.09977705576327, 99.10461378282697, 99.10942624476777, 99.1142145966571, 99.11897899236075, 99.12371958454969, 99.1284365247111, 99.13312996315929, 99.13780004904615, 99.14244693037179, 99.1470707539952, 99.15167166564423, 99.15624980992624, 99.16080533033794, 99.16533836927543, 99.1698490680444, 99.17433756686965, 99.17880400490496, 99.1832485202427, 99.18767124992334, 99.19207232994486, 99.1964518952721, 99.20081007984601, 99.20514701659275, 99.20946283743278, 99.21375767328972, 99.21803165409938, 99.22228490881831, 99.22651756543269, 99.2307297509668, 99.23492159149153, 99.23909321213294, 99.24324473708029, 99.2473762895947, 99.25148799201686, 99.25557996577555, 99.2596523313954, 99.26370520850477, 99.26773871584379, 99.27175297127191, 99.27574809177575, 99.27972419347665, 99.2836813916382, 99.28761980067361, 99.2915395341533, 99.295440704812, 99.29932342455614, 99.30318780447087, 99.30703395482732, 99.31086198508945, 99.3146720039212, 99.31846411919311, 99.32223843798945, 99.3259950666147, 99.3297341106004, 99.33345567471162, 99.33715986295373, 99.3408467785787, 99.34451652409156, 99.34816920125684, 99.35180491110472, 99.35542375393752, 99.35902582933551, 99.36261123616342, 99.3661800725762, 99.36973243602519, 99.37326842326402, 99.37678813035434, 99.38029165267196, 99.38377908491232, 99.38725052109632, 99.39070605457604, 99.39414577804018, 99.39756978351988, 99.40097816239388, 99.40437100539418, 99.40774840261149, 99.4111104435003, 99.41445721688443, 99.4177888109622, 99.42110531331141, 99.42440681089487, 99.42769339006522, 99.43096513656987, 99.43422213555641, 99.43746447157714, 99.44069222859414, 99.44390548998415, 99.44710433854326, 99.45028885649181, 99.45345912547914, 99.45661522658797, 99.45975724033939, 99.46288524669721, 99.46599932507259, 99.46909955432855, 99.47218601278436, 99.47525877822002, 99.47831792788061, 99.48136353848066, 99.48439568620836, 99.4874144467299, 99.49041989519364, 99.49341210623427, 99.49639115397703, 99.49935711204164, 99.50231005354664, 99.50525005111304, 99.5081771768687, 99.51109150245192, 99.51399309901562, 99.51688203723108, 99.51975838729183, 99.5226222189174, 99.5254736013572, 99.52831260339413, 99.53113929334837, 99.53395373908103, 99.53675600799771, 99.53954616705222, 99.54232428275012, 99.5450904211522, 99.54784464787795, 99.55058702810925, 99.55331762659353, 99.55603650764738, 99.55874373515982, 99.5614393725957, 99.56412348299905, 99.56679612899627, 99.56945737279949, 99.57210727620972, 99.57474590062019, 99.57737330701929, 99.57998955599385, 99.58259470773235, 99.58518882202793, 99.5877719582813, 99.59034417550394, 99.59290553232118, 99.59545608697495, 99.59799589732692, 99.60052502086138, 99.60304351468801, 99.605551435545, 99.60804883980167, 99.61053578346146, 99.61301232216474, 99.61547851119131, 99.6179344054635, 99.62038005954868, 99.62281552766211, 99.62524086366953, 99.62765612108969, 99.6300613530973, 99.63245661252535, 99.63484195186786, 99.63721742328235, 99.63958307859245, 99.64193896929044, 99.6442851465396, 99.64662166117688, 99.6489485637152, 99.65126590434599, 99.65357373294158, 99.65587209905745, 99.6581610519349, 99.66044064050308, 99.66271091338157, 99.66497191888253, 99.66722370501301, 99.66946631947738, 99.67169980967934, 99.67392422272431, 99.67613960542162, 99.67834600428671, 99.68054346554321, 99.68273203512527, 99.68491175867955, 99.68708268156735, 99.68924484886685, 99.69139830537505, 99.69354309560985, 99.69567926381224, 99.69780685394811, 99.69992590971049, 99.70203647452136, 99.70413859153375, 99.70623230363368, 99.70831765344207, 99.71039468331678, 99.71246343535428, 99.71452395139185, 99.71657627300934, 99.7186204415309, 99.72065649802711, 99.7226844833166, 99.72470443796777, 99.72671640230111, 99.72872041639036, 99.7307165200647, 99.73270475291031, 99.73468515427231, 99.73665776325622, 99.73862261872988, 99.74057975932519, 99.74252922343963, 99.74447104923807, 99.74640527465432, 99.74833193739289, 99.75025107493056, 99.7521627245181, 99.75406692318163, 99.75596370772455, 99.75785311472887, 99.75973518055685, 99.76160994135267, 99.76347743304376, 99.76533769134251, 99.76719075174768, 99.76903664954601, 99.7708754198136, 99.77270709741751, 99.7745317170172, 99.77634931306572, 99.7781599198117, 99.77996357130033, 99.78176030137486, 99.78355014367818, 99.78533313165417, 99.78710929854896, 99.78887867741234, 99.79064130109933, 99.79239720227123, 99.7941464133972, 99.79588896675548, 99.7976248944348, 99.79935422833555, 99.80107700017122, 99.80279324146962, 99.80450298357422, 99.80620625764536, 99.80790309466157, 99.80959352542071, 99.81127758054139, 99.81295529046403, 99.81462668545218, 99.81629179559371, 99.817950650802, 99.81960328081719, 99.8212497152072, 99.82288998336917, 99.82452411453039, 99.82615213774953, 99.82777408191782, 99.82938997576021, 99.83099984783644, 99.83260372654212, 99.83420164010998, 99.83579361661089, 99.83737968395482, 99.83895986989225, 99.84053420201501, 99.84210270775729, 99.84366541439694, 99.84522234905637, 99.84677353870353, 99.84831901015315, 99.84985879006759, 99.85139290495792, 99.85292138118498, 99.85444424496029, 99.85596152234714, 99.85747323926151, 99.85897942147307, 99.86048009460615, 99.86197528414084, 99.86346501541364, 99.86494931361877, 99.8664282038088, 99.86790171089585, 99.86936985965234, 99.87083267471199, 99.87229018057073, 99.8737424015876, 99.87518936198568, 99.87663108585284, 99.8780675971429, 99.87949891967632, 99.88092507714107, 99.88234609309352, 99.88376199095941, 99.8851727940346, 99.88657852548592, 99.88797920835198, 99.88937486554418, 99.8907655198473, 99.89215119392057, 99.89353191029826, 99.89490769139059, 99.8962785594846, 99.89764453674486, 99.89900564521427, 99.90036190681491, 99.90171334334875, 99.90305997649844, 99.90440182782817, 99.90573891878427, 99.90707127069611, 99.90839890477679, 99.90972184212389, 99.91104010372031, 99.91235371043476, 99.91366268302274, 99.91496704212717, 99.91626680827913, 99.91756200189853, 99.91885264329487, 99.9201387526679, 99.92142035010832, 99.92269745559858, 99.92397008901345, 99.92523827012073, 99.9265020185819, 99.9277613539529, 99.92901629568472, 99.93026686312399, 99.93151307551389, 99.9327549519945, 99.93399251160365, 99.9352257732775, 99.93645475585112, 99.93767947805935, 99.9388999585371, 99.94011621582027, 99.94132826834621, 99.94253613445433, 99.94373983238683, 99.94493938028917, 99.94613479621088, 99.94732609810588, 99.94851330383328, 99.94969643115795, 99.95087549775093, 99.95205052119032, 99.95322151896163, 99.95438850845836, 99.95555150698269, 99.95671053174595, 99.95786559986922, 99.95901672838389, 99.96016393423221, 99.96130723426785, 99.96244664525636, 99.96358218387586, 99.96471386671747, 99.96584171028594, 99.96696573099999, 99.96808594519312, 99.9692023691139, 99.97031501892651, 99.9714239107115, 99.97252906046594, 99.97363048410419, 99.97472819745833, 99.97582221627862, 99.97691255623408, 99.97799923291286, 99.97908226182291, 99.9801616583923, 99.98123743796985, 99.98230961582541, 99.98337820715057, 99.984443227059, 99.98550469058691, 99.98656261269358, 99.98761700826182, 99.98866789209839, 99.98971527893448, 99.99075918342618, 99.99179962015488, 99.9928366036278, 99.99387014827835, 99.99490026846665, 99.9959269784799, 99.99695029253289, 99.99797022476832, 99.99898678925734, 100.0};

#endif

f3 tonescale_chroma_compress_fwd(const f3 &JMh, const JMhParams &p, const ToneScaleParams &pt, const ChromaCompressParams &pc)
{
    const float J = JMh[0];
    const float M = JMh[1];
    const float h = JMh[2];

#ifdef TONESCALE_LUT

    const int lut_size = 1024;
    const float Jmax = 812.3f;
    const float lut_pos = (J / Jmax) * (lut_size - 1);
    const int lut_lo = int(lut_pos);
    const int lut_hi = lut_lo + 1;
    const float f = lut_pos - lut_lo;

    const float J_ts = lerpf(tonescaleJ[lut_lo], tonescaleJ[lut_hi], f);

#else

    // Tonescale applied in Y (convert to and from J)
    const float A = p.A_w_J * powf(std::abs(J) / 100.f, 1.f / (surround[1] * p.z));
    const float Y = std::copysign(1.f, J) * 100.f / p.F_L * powf((27.13f * A) / (400.f - A), 1.f / 0.42f) / 100.f;

    const float f = pt.m_2 * powf(std::max(0.f, Y) / (Y + pt.s_2), pt.g);
    const float Y_ts = std::max(0.f, f * f / (f + pt.t_1)) * pt.n_r;

    const float F_L_Y = powf(p.F_L * std::abs(Y_ts) / 100.f, 0.42f);
    const float J_ts = std::copysign(1.f, Y_ts) * 100.f * powf(((400.f * F_L_Y) / (27.13f + F_L_Y)) / p.A_w_J, surround[1] * p.z);
#endif

    // ChromaCompress
    float M_cp = M;

    if (M != 0.0)
    {
        const float nJ = J_ts / pc.limit_J_max;
        const float snJ = std::max(0.f, 1.f - nJ);
        const float Mnorm = chroma_compress_norm(h, pc.chroma_compress_scale);
        const float limit = powf(nJ, pc.model_gamma) * reach_m_from_table(h, pc.reach_m_table) / Mnorm;

        M_cp = M * powf(J_ts / J, pc.model_gamma);
        M_cp = M_cp / Mnorm;
        M_cp = limit - toe_fwd(limit - M_cp, limit - 0.001f, snJ * pc.sat, sqrt(nJ * nJ + pc.sat_thr));
        M_cp = toe_fwd(M_cp, limit, nJ * pc.compr, snJ);
        M_cp = M_cp * Mnorm;
    }

    return {J_ts, M_cp, h};
}

f3 tonescale_chroma_compress_inv(const f3 &JMh, const JMhParams &p, const ToneScaleParams &pt, const ChromaCompressParams &pc)
{
    const float J_ts = JMh[0];
    const float M_cp = JMh[1];
    const float h    = JMh[2];

    // Inverse Tonescale applied in Y (convert to and from J)
    const float A = p.A_w_J * powf(std::abs(J_ts) / 100.f, 1.f / (surround[1] * p.z));
    const float Y_ts = std::copysign(1.f, J_ts) * 100.f / p.F_L * powf((27.13f * A) / (400.f - A), 1.f / 0.42f) / 100.f;

    const float Z = std::max(0.f, std::min(pt.n / (pt.u_2 * pt.n_r), Y_ts));
    const float ht = (Z + sqrt(Z * (4.f * pt.t_1 + Z))) / 2.f;
    const float Y = pt.s_2 / (powf((pt.m_2 / ht), (1.f / pt.g)) - 1.f) * pt.n_r;

    const float F_L_Y = powf(p.F_L * std::abs(Y) / 100.f, 0.42f);
    const float J = std::copysign(1.f, Y) * 100.f * powf(((400.f * F_L_Y) / (27.13f + F_L_Y)) / p.A_w_J, surround[1] * p.z);

    // Inverse ChromaCompress
    float M = M_cp;

    if (M_cp != 0.0)
    {
        const float nJ = J_ts / pc.limit_J_max;
        const float snJ = std::max(0.f, 1.f - nJ);
        const float Mnorm = chroma_compress_norm(h, pc.chroma_compress_scale);
        const float limit = powf(nJ, pc.model_gamma) * reach_m_from_table(h, pc.reach_m_table) / Mnorm;

        M = M_cp / Mnorm;
        M = toe_inv(M, limit, nJ * pc.compr, snJ);
        M = limit - toe_inv(limit - M, limit - 0.001f, snJ * pc.sat, sqrt(nJ * nJ + pc.sat_thr));
        M = M * Mnorm;
        M = M * powf(J_ts / J, -pc.model_gamma);
    }

    return {J, M, h};
}

JMhParams init_JMhParams(const Primaries &P)
{
    JMhParams p;

    const m33f MATRIX_16 = XYZtoRGB_f33(CAM16::primaries);
    const m33f RGB_to_XYZ = RGBtoXYZ_f33(P);
    const f3 XYZ_w = mult_f3_f33(f3_from_f(reference_luminance), RGB_to_XYZ);

    const float Y_W = XYZ_w[1];

    const f3 RGB_w = mult_f3_f33(XYZ_w, MATRIX_16);

    // Viewing condition dependent parameters
    const float K = 1.f / (5.f * L_A + 1.f);
    const float K4 = powf(K, 4.f);
    const float N = Y_b / Y_W;
    const float F_L = 0.2f * K4 * (5.f * L_A) + 0.1f * powf((1.f - K4), 2.f) * powf(5.f * L_A, 1.f/3.f);
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

    const float F_L_W = powf(F_L, 0.42f);
    const float A_w_J   = (400.f * F_L_W) / (27.13f + F_L_W);

    p.XYZ_w = XYZ_w;
    p.F_L = F_L;
    p.z = z;
    p.D_RGB = D_RGB;
    p.A_w = A_w;
    p.A_w_J = A_w_J;

    p.MATRIX_RGB_to_CAM16 = mult_f33_f33(RGBtoRGB_f33(P, CAM16::primaries), scale_f33(Identity_M33, f3_from_f(100.f)));
    p.MATRIX_CAM16_to_RGB = invert_f33(p.MATRIX_RGB_to_CAM16);

    return p;
}

Table3D make_gamut_table(const Primaries &P, float peakLuminance)
{
    const JMhParams params = init_JMhParams(P);

    Table3D gamutCuspTableUnsorted{};
    for (int i = 0; i < gamutCuspTableUnsorted.size; i++)
    {
        const float hNorm = (float) i / gamutCuspTableUnsorted.size;
        const f3 HSV = {hNorm, 1., 1.};
        const f3 RGB = HSV_to_RGB(HSV);
        const f3 scaledRGB = mult_f_f3(peakLuminance / reference_luminance, RGB);
        const f3 JMh = RGB_to_JMh(scaledRGB, params);

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
    gamutCuspTable.table[0][2] = gamutCuspTable.table[0][2] - 360.f;
    gamutCuspTable.table[gamutCuspTable.size+1][2] = gamutCuspTable.table[gamutCuspTable.size+1][2] + 360.f;

    return gamutCuspTable;
}

Table1DLookup make_gamut_table_lookup(const Table3D & gt)
{
    unsigned int lookup_size = gt.total_size;
    float lut_start = gt.table[0][2];
    float lut_end = gt.table[lookup_size-1][2];

    Lut1DOpDataRcPtr lutData = std::make_shared<Lut1DOpData>(lookup_size);
    Array::Values & vals = lutData->getArray().getValues();
    for (unsigned int i = 0, p = 0; i < lookup_size; ++i)
    {
        for (int j = 0; j < 3; ++j, ++p)
        {
            vals[p] = (gt.table[i][2] - lut_start) / (lut_end - lut_start);
        }
    }

    Table1DLookup gtl;
    gtl.start = lut_start / 360.f;
    gtl.end = lut_end / 360.f;

    unsigned int inv_lookup_size = gtl.total_size;

    auto invLut = lutData->inverse();
    invLut->validate();
    invLut->finalize();
    ConstLut1DOpDataRcPtr constInvLut = invLut;
    ConstLut1DOpDataRcPtr newDomainLut = std::make_shared<Lut1DOpData>(Lut1DOpData::LUT_STANDARD, inv_lookup_size, true);
    ConstLut1DOpDataRcPtr fastInvLutData = Lut1DOpData::Compose(newDomainLut, constInvLut, Lut1DOpData::COMPOSE_RESAMPLE_NO);

    auto & invLutArray = fastInvLutData->getArray().getValues();
    for (unsigned int i = 0, p = 0; i < inv_lookup_size; ++i)
    {
        for (int j = 0; j < 3; ++j, ++p)
        {
            gtl.table[i] = invLutArray[p];
        }
    }

    return gtl;
}

bool any_below_zero(const f3 &rgb)
{
    return (rgb[0] < 0. || rgb[1] < 0. || rgb[2] < 0.);
}

Table1D make_reach_m_table(const Primaries &P, float peakLuminance)
{
    const JMhParams params = init_JMhParams(P);
    const float limit_J_max = Y_to_J(peakLuminance, params);

    Table1D gamutReachTable{};

    for (int i = 0; i < gamutReachTable.size; i++) {
        const float hue = (float) i;

        const float search_range = 50.f;
        float low = 0.;
        float high = low + search_range;
        bool outside = false;

        while ((outside != true) & (high < 1300.f))
        {
            const f3 searchJMh = {limit_J_max, high, hue};
            const f3 newLimitRGB = JMh_to_RGB(searchJMh, params);
            outside = any_below_zero(newLimitRGB);
            if (outside == false)
            {
                low = high;
                high = high + search_range;
            }
        }

        while (high-low > 1e-2)
        {
            const float sampleM = (high + low) / 2.f;
            const f3 searchJMh = {limit_J_max, sampleM, hue};
            const f3 newLimitRGB = JMh_to_RGB(searchJMh, params);
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

bool outside_hull(const f3 &rgb)
{
    // limit value, once we cross this value, we are outside of the top gamut shell
    const float maxRGBtestVal = 1.0;
    return rgb[0] > maxRGBtestVal || rgb[1] > maxRGBtestVal || rgb[2] > maxRGBtestVal;
}

float get_focus_gain(float J, float cuspJ, float limit_J_max)
{
    const float thr = lerpf(cuspJ, limit_J_max, focus_gain_blend);

    if (J > thr)
    {
        // Approximate inverse required above threshold
        float gain = (limit_J_max - thr) / std::max(0.0001f, (limit_J_max - std::min(limit_J_max, J)));
        return powf(log10(gain), 1.f / focus_adjust_gain) + 1.f;
    }
    else
    {
        // Analytic inverse possible below cusp
        return 1.f;
    }
}

float solve_J_intersect(float J, float M, float focusJ, float maxJ, float slope_gain)
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

float smin(float a, float b, float s)
{
    const float h = std::max(s - std::abs(a - b), 0.f) / s;
    return std::min(a, b) - h * h * h * s * (1.f / 6.f);
}

f3 find_gamut_boundary_intersection(const f3 &JMh_s, const f2 &JM_cusp_in, float J_focus, float J_max, float slope_gain, float gamma_top, float gamma_bottom)
{
    const float s = std::max(0.000001f, smooth_cusps);
    const f2 JM_cusp = {
        JM_cusp_in[0],
        JM_cusp_in[1] * (1.f + smooth_m * s)
    };

    const float J_intersect_source = solve_J_intersect(JMh_s[0], JMh_s[1], J_focus, J_max, slope_gain);
    const float J_intersect_cusp = solve_J_intersect(JM_cusp[0], JM_cusp[1], J_focus, J_max, slope_gain);

    float slope = 0.f;
    if (J_intersect_source < J_focus)
    {
        slope = J_intersect_source * (J_intersect_source - J_focus) / (J_focus * slope_gain);
    }
    else
    {
        slope = (J_max - J_intersect_source) * (J_intersect_source - J_focus) / (J_focus * slope_gain);
    }

    const float M_boundary_lower = J_intersect_cusp * powf(J_intersect_source / J_intersect_cusp, 1.f / gamma_bottom) / (JM_cusp[0] / JM_cusp[1] - slope);
    const float M_boundary_upper = JM_cusp[1] * (J_max - J_intersect_cusp) * powf((J_max - J_intersect_source) / (J_max - J_intersect_cusp), 1.f / gamma_top) / (slope * JM_cusp[1] + J_max - JM_cusp[0]);
    const float M_boundary = JM_cusp[1] * smin(M_boundary_lower / JM_cusp[1], M_boundary_upper / JM_cusp[1], s);
    const float J_boundary = J_intersect_source + slope * M_boundary;

    return {J_boundary, M_boundary, J_intersect_source};
}

f3 get_reach_boundary(
    float J,
    float M,
    float h,
    const f2 &JMcusp,
    float focusJ,
    float limit_J_max,
    float model_gamma,
    float focus_dist,
    const ACES2::Table1D & reach_m_table
)
{
    const float reachMaxM = reach_m_from_table(h, reach_m_table);

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

    const float boundary = limit_J_max * powf(intersectJ / limit_J_max, model_gamma) * reachMaxM / (limit_J_max - slope * reachMaxM);
    return {J, boundary, h};
}

float compression_function(
    float v,
    float thr,
    float lim,
    bool invert)
{
    float s = (lim - thr) * (1.f - thr) / (lim - 1.f);
    float nd = (v - thr) / s;

    float vCompressed;

    if (invert) {
        if (v < thr || lim <= 1.0001f || v > thr + s) {
            vCompressed = v;
        } else {
            vCompressed = thr + s * (-nd / (nd - 1.f));
        }
    } else {
        if (v < thr || lim <= 1.0001f) {
            vCompressed = v;
        } else {
            vCompressed = thr + s * nd / (1.f + nd);
        }
    }

    return vCompressed;
}

f3 compressGamut(const f3 &JMh, float Jx, const ACES2::GamutCompressParams& p, bool invert)
{
    const float J = JMh[0];
    const float M = JMh[1];
    const float h = JMh[2];

    if (M < 0.0001f || J > p.limit_J_max)
    {
        return {J, 0.f, h};
    }
    else
    {
        const f2 project_from = {J, M};
        const f2 JMcusp = cusp_from_table(h, p.gamut_cusp_table, p.gamut_cusp_index_table);
        const float focusJ = lerpf(JMcusp[0], p.mid_J, std::min(1.f, cusp_mid_blend - (JMcusp[0] / p.limit_J_max)));
        const float slope_gain = p.limit_J_max * p.focus_dist * get_focus_gain(Jx, JMcusp[0], p.limit_J_max);

        const float gamma_top = hue_dependent_upper_hull_gamma(h, p.upper_hull_gamma_table);
        const float gamma_bottom = p.lower_hull_gamma;

        const f3 boundaryReturn = find_gamut_boundary_intersection({J, M, h}, JMcusp, focusJ, p.limit_J_max, slope_gain, gamma_top, gamma_bottom);
        const f2 JMboundary = {boundaryReturn[0], boundaryReturn[1]};
        const f2 project_to = {boundaryReturn[2], 0.f};

        if (JMboundary[1] <= 0.0f)
        {
            return {J, 0.f, h};
        }

        const f3 reachBoundary = get_reach_boundary(JMboundary[0], JMboundary[1], h, JMcusp, focusJ, p.limit_J_max, p.model_gamma, p.focus_dist, p.reach_m_table);

        const float difference = std::max(1.0001f, reachBoundary[1] / JMboundary[1]);
        const float threshold = std::max(compression_threshold, 1.f / difference);

        float v = project_from[1] / JMboundary[1];
        v = compression_function(v, threshold, difference, invert);

        const f2 JMcompressed {
            project_to[0] + v * (JMboundary[0] - project_to[0]),
            project_to[1] + v * (JMboundary[1] - project_to[1])
        };

        return {JMcompressed[0], JMcompressed[1], h};
    }
}

f3 gamut_compress_fwd(const f3 &JMh, const GamutCompressParams &p)
{
    return compressGamut(JMh, JMh[0], p, false);
}

f3 gamut_compress_inv(const f3 &JMh, const GamutCompressParams &p)
{
    const f2 JMcusp = cusp_from_table(JMh[2], p.gamut_cusp_table, p.gamut_cusp_index_table);
    float Jx = JMh[0];

    f3 unCompressedJMh;

    // Analytic inverse below threshold
    if (Jx <= lerpf(JMcusp[0], p.limit_J_max, focus_gain_blend))
    {
        unCompressedJMh = compressGamut(JMh, Jx, p, true);
    }
    // Approximation above threshold
    else
    {
        Jx = compressGamut(JMh, Jx, p, true)[0];
        unCompressedJMh = compressGamut(JMh, Jx, p, true);
    }

    return unCompressedJMh;
}

bool evaluate_gamma_fit(
    const f2 &JMcusp,
    const f3 testJMh[3],
    float topGamma,
    float peakLuminance,
    float limit_J_max,
    float mid_J,
    float focus_dist,
    float lower_hull_gamma,
    const JMhParams &limitJMhParams)
{
    const float focusJ = lerpf(JMcusp[0], mid_J, std::min(1.f, cusp_mid_blend - (JMcusp[0] / limit_J_max)));

    for (size_t testIndex = 0; testIndex < 3; testIndex++)
    {
        const float slope_gain = limit_J_max * focus_dist * get_focus_gain(testJMh[testIndex][0], JMcusp[0], limit_J_max);
        const f3 approxLimit = find_gamut_boundary_intersection(testJMh[testIndex], JMcusp, focusJ, limit_J_max, slope_gain, topGamma, lower_hull_gamma);
        const f3 approximate_JMh = {approxLimit[0], approxLimit[1], testJMh[testIndex][2]};
        const f3 newLimitRGB = JMh_to_RGB(approximate_JMh, limitJMhParams);
        const f3 newLimitRGBScaled = mult_f_f3(reference_luminance / peakLuminance, newLimitRGB);

        if (!outside_hull(newLimitRGBScaled))
        {
            return false;
        }
    }

    return true;
}

Table1D make_upper_hull_gamma(
    const Table3D &gamutCuspTable,
    const Table1DLookup &gamutCuspIndexTable,
    float peakLuminance,
    float limit_J_max,
    float mid_J,
    float focus_dist,
    float lower_hull_gamma,
    const JMhParams &limitJMhParams)
{
    const int test_count = 3;
    const float testPositions[test_count] = {0.01f, 0.5f, 0.99f};

    Table1D gammaTable{};
    Table1D gamutTopGamma{};

    for (int i = 0; i < gammaTable.size; i++)
    {
        gammaTable.table[i] = -1.f;

        const float hue = (float) i;
        const f2 JMcusp = cusp_from_table(hue, gamutCuspTable, gamutCuspIndexTable);

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
            const bool gammaFound = evaluate_gamma_fit(JMcusp, testJMh, high, peakLuminance, limit_J_max, mid_J, focus_dist, lower_hull_gamma, limitJMhParams);
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
            const bool gammaFound = evaluate_gamma_fit(JMcusp, testJMh, testGamma, peakLuminance, limit_J_max, mid_J, focus_dist, lower_hull_gamma, limitJMhParams);
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
ToneScaleParams init_ToneScaleParams(float peakLuminance)
{
    // Preset constants that set the desired behavior for the curve
    const float n = peakLuminance;

    const float n_r = 100.0f;    // normalized white in nits (what 1.0 should be)
    const float g = 1.15f;       // surround / contrast
    const float c = 0.18f;       // anchor for 18% grey
    const float c_d = 10.013f;   // output luminance of 18% grey (in nits)
    const float w_g = 0.14f;     // change in grey between different peak luminance
    const float t_1 = 0.04f;     // shadow toe or flare/glare compensation
    const float r_hit_min = 128.f;   // scene-referred value "hitting the roof"
    const float r_hit_max = 896.f;   // scene-referred value "hitting the roof"

    // Calculate output constants
    const float r_hit = r_hit_min + (r_hit_max - r_hit_min) * (log(n/n_r)/log(10000.f/100.f));
    const float m_0 = (n / n_r);
    const float m_1 = 0.5f * (m_0 + sqrt(m_0 * (m_0 + 4.f * t_1)));
    const float u = powf((r_hit/m_1)/((r_hit/m_1)+1.f),g);
    const float m = m_1 / u;
    const float w_i = log(n/100.f)/log(2.f);
    const float c_t = c_d/n_r * (1.f + w_i * w_g);
    const float g_ip = 0.5f * (c_t + sqrt(c_t * (c_t + 4.f * t_1)));
    const float g_ipp2 = -(m_1 * powf((g_ip/m),(1.f/g))) / (powf(g_ip/m , 1.f/g)-1.f);
    const float w_2 = c / g_ipp2;
    const float s_2 = w_2 * m_1;
    const float u_2 = powf((r_hit/m_1)/((r_hit/m_1) + w_2), g);
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

ChromaCompressParams init_ChromaCompressParams(float peakLuminance)
{
    const ToneScaleParams tsParams = init_ToneScaleParams(peakLuminance);
    const JMhParams inputJMhParams = init_JMhParams(ACES_AP0::primaries);

    float limit_J_max = Y_to_J(peakLuminance, inputJMhParams);

    // Calculated chroma compress variables
    const float log_peak = log10( tsParams.n / tsParams.n_r);
    const float compr = chroma_compress + (chroma_compress * chroma_compress_fact) * log_peak;
    const float sat = std::max(0.2f, chroma_expand - (chroma_expand * chroma_expand_fact) * log_peak);
    const float sat_thr = chroma_expand_thr / tsParams.n;
    const float model_gamma = 1.f / (surround[1] * (1.48f + sqrt(Y_b / L_A)));

    ChromaCompressParams params{};
    params.limit_J_max = limit_J_max;
    params.model_gamma = model_gamma;
    params.sat = sat;
    params.sat_thr = sat_thr;
    params.compr = compr;
    params.chroma_compress_scale = powf(0.03379f * peakLuminance, 0.30596f) - 0.45135f;
    params.reach_m_table = make_reach_m_table(ACES_AP1::primaries, peakLuminance);
    return params;
}

GamutCompressParams init_GamutCompressParams(float peakLuminance, const Primaries &limitingPrimaries)
{
    const ToneScaleParams tsParams = init_ToneScaleParams(peakLuminance);
    const JMhParams inputJMhParams = init_JMhParams(ACES_AP0::primaries);

    float limit_J_max = Y_to_J(peakLuminance, inputJMhParams);
    float mid_J = Y_to_J(tsParams.c_t * 100.f, inputJMhParams);

    // Calculated chroma compress variables
    const float log_peak = log10( tsParams.n / tsParams.n_r);
    const float model_gamma = 1.f / (surround[1] * (1.48f + sqrt(Y_b / L_A)));
    const float focus_dist = focus_distance + focus_distance * focus_distance_scaling * log_peak;
    const float lower_hull_gamma =  1.14f + 0.07f * log_peak;

    const JMhParams limitJMhParams = init_JMhParams(limitingPrimaries);

    GamutCompressParams params{};
    params.limit_J_max = limit_J_max;
    params.mid_J = mid_J;
    params.model_gamma = model_gamma;
    params.focus_dist = focus_dist;
    params.lower_hull_gamma = lower_hull_gamma;
    params.reach_m_table = make_reach_m_table(ACES_AP1::primaries, peakLuminance);
    params.gamut_cusp_table = make_gamut_table(limitingPrimaries, peakLuminance);
    params.gamut_cusp_index_table = make_gamut_table_lookup(params.gamut_cusp_table);

    params.upper_hull_gamma_table = make_upper_hull_gamma(
        params.gamut_cusp_table,
        params.gamut_cusp_index_table,
        peakLuminance,
        limit_J_max,
        mid_J,
        focus_dist,
        lower_hull_gamma,
        limitJMhParams);

    return params;
}

} // namespace ACES2

} // OCIO namespace
