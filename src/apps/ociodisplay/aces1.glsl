
// Declaration of all helper methods


const int ocio_grading_rgbcurve_knotsOffsets_0[8] = int[8](-1, 0, -1, 0, -1, 0, 0, 9);
const float ocio_grading_rgbcurve_knots_0[9] = float[9](-5.26017761, -3.75502753, -2.24987745, -0.744727492, 1.06145251, 1.96573484, 2.86763239, 3.77526045, 4.67381239);
const int ocio_grading_rgbcurve_coefsOffsets_0[8] = int[8](-1, 0, -1, 0, -1, 0, 0, 24);
const float ocio_grading_rgbcurve_coefs_0[24] = float[24](0.185970441, 0.403778881, -0.0748505071, -0.185833707, -0.192129433, -0.19314684, -0.0501050949, -0.0511224195, 0., 0.559826851, 1.77532244, 1.54999995, 0.878701687, 0.531223178, 0.182825878, 0.0918722972, -4., -3.57868838, -1.82131326, 0.681241214, 2.87457752, 3.51206255, 3.8340621, 3.95872402);

float ocio_grading_rgbcurve_evalBSplineCurve_0(in int curveIdx, in float x)
{
  int knotsOffs = ocio_grading_rgbcurve_knotsOffsets_0[curveIdx * 2];
  int knotsCnt = ocio_grading_rgbcurve_knotsOffsets_0[curveIdx * 2 + 1];
  int coefsOffs = ocio_grading_rgbcurve_coefsOffsets_0[curveIdx * 2];
  int coefsCnt = ocio_grading_rgbcurve_coefsOffsets_0[curveIdx * 2 + 1];
  int coefsSets = coefsCnt / 3;
  if (coefsSets == 0)
  {
    return x;
  }
  float knStart = ocio_grading_rgbcurve_knots_0[knotsOffs];
  float knEnd = ocio_grading_rgbcurve_knots_0[knotsOffs + knotsCnt - 1];
  if (x <= knStart)
  {
    float B = ocio_grading_rgbcurve_coefs_0[coefsOffs + coefsSets];
    float C = ocio_grading_rgbcurve_coefs_0[coefsOffs + coefsSets * 2];
    return (x - knStart) * B + C;
  }
  else if (x >= knEnd)
  {
    float A = ocio_grading_rgbcurve_coefs_0[coefsOffs + coefsSets - 1];
    float B = ocio_grading_rgbcurve_coefs_0[coefsOffs + coefsSets * 2 - 1];
    float C = ocio_grading_rgbcurve_coefs_0[coefsOffs + coefsSets * 3 - 1];
    float kn = ocio_grading_rgbcurve_knots_0[knotsOffs + knotsCnt - 2];
    float t = knEnd - kn;
    float slope = 2. * A * t + B;
    float offs = ( A * t + B ) * t + C;
    return (x - knEnd) * slope + offs;
  }
  int i = 0;
  for (i = 0; i < knotsCnt - 2; ++i)
  {
    if (x < ocio_grading_rgbcurve_knots_0[knotsOffs + i + 1])
    {
      break;
    }
  }
  float A = ocio_grading_rgbcurve_coefs_0[coefsOffs + i];
  float B = ocio_grading_rgbcurve_coefs_0[coefsOffs + coefsSets + i];
  float C = ocio_grading_rgbcurve_coefs_0[coefsOffs + coefsSets * 2 + i];
  float kn = ocio_grading_rgbcurve_knots_0[knotsOffs + i];
  float t = x - kn;
  return ( A * t + B ) * t + C;
}

const int ocio_grading_rgbcurve_knotsOffsets_1[8] = int[8](-1, 0, -1, 0, -1, 0, 0, 15);
const float ocio_grading_rgbcurve_knots_1[15] = float[15](-2.54062366, -2.08035731, -1.62009084, -1.15982437, -0.69955802, -0.239291579, 0.220974833, 0.681241214, 1.01284635, 1.34445143, 1.6760565, 2.00766158, 2.33926654, 2.67087173, 3.00247669);
const int ocio_grading_rgbcurve_coefsOffsets_1[8] = int[8](-1, 0, -1, 0, -1, 0, 0, 42);
const float ocio_grading_rgbcurve_coefs_1[42] = float[42](0.521772683, 0.0654487088, 0.272604734, 0.123911291, 0.0858645961, -0.0171162505, 0.0338416733, -0.194834962, -0.201688975, -0.476983279, -0.276004612, -0.139139131, -0.0922630876, -0.0665909499, 0., 0.480308801, 0.54055649, 0.791498125, 0.90556252, 0.984603703, 0.968847632, 1., 0.870783448, 0.737021267, 0.420681119, 0.237632066, 0.145353615, 0.0841637775, -1.69896996, -1.58843505, -1.35350001, -1.04694998, -0.656400025, -0.221410006, 0.22814402, 0.681241214, 0.991421878, 1.25800002, 1.44994998, 1.55910003, 1.62259996, 1.66065454);

float ocio_grading_rgbcurve_evalBSplineCurve_1(in int curveIdx, in float x)
{
  int knotsOffs = ocio_grading_rgbcurve_knotsOffsets_1[curveIdx * 2];
  int knotsCnt = ocio_grading_rgbcurve_knotsOffsets_1[curveIdx * 2 + 1];
  int coefsOffs = ocio_grading_rgbcurve_coefsOffsets_1[curveIdx * 2];
  int coefsCnt = ocio_grading_rgbcurve_coefsOffsets_1[curveIdx * 2 + 1];
  int coefsSets = coefsCnt / 3;
  if (coefsSets == 0)
  {
    return x;
  }
  float knStart = ocio_grading_rgbcurve_knots_1[knotsOffs];
  float knEnd = ocio_grading_rgbcurve_knots_1[knotsOffs + knotsCnt - 1];
  if (x <= knStart)
  {
    float B = ocio_grading_rgbcurve_coefs_1[coefsOffs + coefsSets];
    float C = ocio_grading_rgbcurve_coefs_1[coefsOffs + coefsSets * 2];
    return (x - knStart) * B + C;
  }
  else if (x >= knEnd)
  {
    float A = ocio_grading_rgbcurve_coefs_1[coefsOffs + coefsSets - 1];
    float B = ocio_grading_rgbcurve_coefs_1[coefsOffs + coefsSets * 2 - 1];
    float C = ocio_grading_rgbcurve_coefs_1[coefsOffs + coefsSets * 3 - 1];
    float kn = ocio_grading_rgbcurve_knots_1[knotsOffs + knotsCnt - 2];
    float t = knEnd - kn;
    float slope = 2. * A * t + B;
    float offs = ( A * t + B ) * t + C;
    return (x - knEnd) * slope + offs;
  }
  int i = 0;
  for (i = 0; i < knotsCnt - 2; ++i)
  {
    if (x < ocio_grading_rgbcurve_knots_1[knotsOffs + i + 1])
    {
      break;
    }
  }
  float A = ocio_grading_rgbcurve_coefs_1[coefsOffs + i];
  float B = ocio_grading_rgbcurve_coefs_1[coefsOffs + coefsSets + i];
  float C = ocio_grading_rgbcurve_coefs_1[coefsOffs + coefsSets * 2 + i];
  float kn = ocio_grading_rgbcurve_knots_1[knotsOffs + i];
  float t = x - kn;
  return ( A * t + B ) * t + C;
}

// Declaration of the OCIO shader function

vec4 OCIODisplay(vec4 inPixel)
{
  vec4 outColor;

  for (int i = 0; i < 10; ++i)
  {
    outColor = inPixel;

  // Add FixedFunction 'ACES_Glow10 (Forward)' processing
  
  {
    float chroma = sqrt( outColor.rgb.b * (outColor.rgb.b - outColor.rgb.g) + outColor.rgb.g * (outColor.rgb.g - outColor.rgb.r) + outColor.rgb.r * (outColor.rgb.r - outColor.rgb.b) );
    float YC = (outColor.rgb.b + outColor.rgb.g + outColor.rgb.r + 1.75 * chroma) / 3.;
    float maxval = max( outColor.rgb.r, max( outColor.rgb.g, outColor.rgb.b));
    float minval = min( outColor.rgb.r, min( outColor.rgb.g, outColor.rgb.b));
    float sat = ( max(1e-10, maxval) - max(1e-10, minval) ) / max(1e-2, maxval);
    float x = (sat - 0.4) * 5.;
    float t = max( 0., 1. - 0.5 * abs(x));
    float s = 0.5 * (1. + sign(x) * (1. - t * t));
    float GlowGain = 0.0500000007 * s;
    float GlowMid = 0.0799999982;
    float glowGainOut = mix(GlowGain, GlowGain * (GlowMid / YC - 0.5), float( YC > GlowMid * 2. / 3. ));
    glowGainOut = mix(glowGainOut, 0., float( YC > GlowMid * 2. ));
    outColor.rgb = outColor.rgb * glowGainOut + outColor.rgb;
  }
  
  // Add FixedFunction 'ACES_RedMod10 (Forward)' processing
  
  {
    float a = 2.0 * outColor.rgb.r - (outColor.rgb.g + outColor.rgb.b);
    float b = 1.7320508075688772 * (outColor.rgb.g - outColor.rgb.b);
    float hue = atan(b, a);
    float knot_coord = clamp(2. + hue * float(1.6976527), 0., 4.);
    int j = int(min(knot_coord, 3.));
    float t = knot_coord - float(j);
    vec4 monomials = vec4(t*t*t, t*t, t, 1.);
    vec4 m0 = vec4(0.25, 0., 0., 0.);
    vec4 m1 = vec4(-0.75, 0.75, 0.75, 0.25);
    vec4 m2 = vec4(0.75, -1.5, 0., 1.);
    vec4 m3 = vec4(-0.25, 0.75, -0.75, 0.25);
    vec4 coefs = mix(m0, m1, float(j == 1));
    coefs = mix(coefs, m2, float(j == 2));
    coefs = mix(coefs, m3, float(j == 3));
    float f_H = dot(coefs, monomials);
    float maxval = max( outColor.rgb.r, max( outColor.rgb.g, outColor.rgb.b));
    float minval = min( outColor.rgb.r, min( outColor.rgb.g, outColor.rgb.b));
    float f_S = ( max(1e-10, maxval) - max(1e-10, minval) ) / max(1e-2, maxval);
    outColor.rgb.r = outColor.rgb.r + f_H * f_S * (0.0299999993 - outColor.rgb.r) * 0.180000007;
  }
  
  // Add Range processing
  
  {
    outColor.rgb = max(vec3(0., 0., 0.), outColor.rgb);
  }
  
  // Add Matrix processing
  
  {
    vec4 res = vec4(outColor.rgb.r, outColor.rgb.g, outColor.rgb.b, outColor.a);
    vec4 tmp = res;
    res = mat4(1.4514393161456653, -0.07655377339602043, 0.008316148425697719, 0., -0.23651074689374019, 1.1762296998335731, -0.0060324497910210278, 0., -0.21492856925192524, -0.099675926437552201, 0.9977163013653233, 0., 0., 0., 0., 1.) * tmp;
    outColor.rgb = vec3(res.x, res.y, res.z);
    outColor.a = res.w;
  }
  
  // Add Range processing
  
  {
    outColor.rgb = max(vec3(0., 0., 0.), outColor.rgb);
  }
  
  // Add Matrix processing
  
  {
    vec4 res = vec4(outColor.rgb.r, outColor.rgb.g, outColor.rgb.b, outColor.a);
    vec4 tmp = res;
    res = mat4(0.97088914867099996, 0.010889148671, 0.010889148671, 0., 0.026963270631999998, 0.98696327063199996, 0.026963270631999998, 0., 0.0021475806959999999, 0.0021475806959999999, 0.96214758069600004, 0., 0., 0., 0., 1.) * tmp;
    outColor.rgb = vec3(res.x, res.y, res.z);
    outColor.a = res.w;
  }
  
  // Add Log processing
  
  {
    outColor.rgb = max( vec3(1.17549435e-38, 1.17549435e-38, 1.17549435e-38), outColor.rgb);
    outColor.rgb = log(outColor.rgb) * vec3(0.434294462, 0.434294462, 0.434294462);
  }
  
  // Add GradingRGBCurve 'log' forward processing
  
  {
    outColor.rgb.r = ocio_grading_rgbcurve_evalBSplineCurve_0(0, outColor.rgb.r);
    outColor.rgb.g = ocio_grading_rgbcurve_evalBSplineCurve_0(1, outColor.rgb.g);
    outColor.rgb.b = ocio_grading_rgbcurve_evalBSplineCurve_0(2, outColor.rgb.b);
    outColor.rgb.r = ocio_grading_rgbcurve_evalBSplineCurve_0(3, outColor.rgb.r);
    outColor.rgb.g = ocio_grading_rgbcurve_evalBSplineCurve_0(3, outColor.rgb.g);
    outColor.rgb.b = ocio_grading_rgbcurve_evalBSplineCurve_0(3, outColor.rgb.b);
  }
  
  // Add GradingRGBCurve 'log' forward processing
  
  {
    outColor.rgb.r = ocio_grading_rgbcurve_evalBSplineCurve_1(0, outColor.rgb.r);
    outColor.rgb.g = ocio_grading_rgbcurve_evalBSplineCurve_1(1, outColor.rgb.g);
    outColor.rgb.b = ocio_grading_rgbcurve_evalBSplineCurve_1(2, outColor.rgb.b);
    outColor.rgb.r = ocio_grading_rgbcurve_evalBSplineCurve_1(3, outColor.rgb.r);
    outColor.rgb.g = ocio_grading_rgbcurve_evalBSplineCurve_1(3, outColor.rgb.g);
    outColor.rgb.b = ocio_grading_rgbcurve_evalBSplineCurve_1(3, outColor.rgb.b);
  }
  
  // Add Log 'Anti-Log' processing
  
  {
    outColor.rgb = pow( vec3(10., 10., 10.), outColor.rgb);
  }
  
  // Add Matrix processing
  
  {
    vec4 res = vec4(outColor.rgb.r, outColor.rgb.g, outColor.rgb.b, outColor.a);
    res = vec4(0.0208420176, 0.0208420176, 0.0208420176, 1.) * res;
    res = vec4(-0.000416840339, -0.000416840339, -0.000416840339, 0.) + res;
    outColor.rgb = vec3(res.x, res.y, res.z);
    outColor.a = res.w;
  }
  
  // Add FixedFunction 'ACES_DarkToDim10 (Forward)' processing
  
  {
    float Y = max( 1e-10, 0.27222871678091454 * outColor.rgb.r + 0.67408176581114831 * outColor.rgb.g + 0.053689517407937051 * outColor.rgb.b );
    float Ypow_over_Y = pow( Y, -0.0188999772);
    outColor.rgb = outColor.rgb * Ypow_over_Y;
  }
  
  // Add Matrix processing
  
  {
    vec4 res = vec4(outColor.rgb.r, outColor.rgb.g, outColor.rgb.b, outColor.a);
    vec4 tmp = res;
    res = mat4(1.604753433346922, -0.10208245810655031, -0.0032671116532946819, 0., -0.531080948604018, 1.1081341286221253, -0.072755424133422703, 0., -0.073672484741910349, -0.0060516705145729488, 1.0760225357877193, 0., 0., 0., 0., 1.) * tmp;
    outColor.rgb = vec3(res.x, res.y, res.z);
    outColor.a = res.w;
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

  // Dummy

  // for (int i = 0; i < 1000; ++i)
  // {
  //   outColor.r = atan(outColor.g, outColor.b);
  // }

  return outColor;
}
