// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.


#include <array>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <csignal>
#include <cstring>
#include <iostream>
#include <fstream>
#include <map>
#include <sstream>
#include <utility>
#include <vector>

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <GLUT/glut.h>
#elif _WIN32
#include <GL/glew.h>
#include <GL/glut.h>
#else
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glut.h>
#endif
#if __APPLE__
#include "metalapp.h"
#endif

#include "glsl.h"
#include "oglapp.h"
#include "imageio.h"

#include "tables.h"

// #define RENDER_IMAGE
constexpr auto RENDER_PATH = "/user_data/RND/dev/OpenColorIO/src/apps/ociodisplay/aces2_tex.exr";

#define HARDCODED_SHADER
constexpr auto SHADER_PATH = "/user_data/RND/dev/OpenColorIO/src/apps/ociodisplay/aces2.glsl";

#define OPENGL_DEBUG_CB
#define OPENGL_QUERY
// #define USE_SSBO
#define USE_UBO
// #define USE_TEXTURE

// #define PRINT_SHADER
#define PRINT_TIMING

bool g_verbose   = false;
bool g_gpulegacy = false;
bool g_gpuinfo   = false;
#if __APPLE__
bool g_useMetal  = false;
#endif

std::string g_filename;

long g_imageWidth;
long g_imageHeight;
float g_imageAspect;

std::string g_inputColorSpace;
std::string g_display;
std::string g_transformName;
std::string g_look;
OCIO::OptimizationFlags g_optimization{ OCIO::OPTIMIZATION_DEFAULT };

static const std::array<std::pair<const char*, OCIO::OptimizationFlags>, 5> OptmizationMenu = { {
    { "None",      OCIO::OPTIMIZATION_NONE },
    { "Lossless",  OCIO::OPTIMIZATION_LOSSLESS },
    { "Very good", OCIO::OPTIMIZATION_VERY_GOOD },
    { "Good",      OCIO::OPTIMIZATION_GOOD },
    { "Draft",     OCIO::OPTIMIZATION_DRAFT } } };

float g_exposure_fstop{ 0.0f };
float g_display_gamma{ 1.0f };
int g_channelHot[4]{ 1, 1, 1, 1 };  // show rgb

OCIO::OglAppRcPtr g_oglApp;

void debug_message_callback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei,
    const GLchar *msg,
    const void *) {

    std::string _source;
    std::string _type;
    std::string _severity;

    switch (source) {
        case GL_DEBUG_SOURCE_API:
        _source = "API";
        break;

        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        _source = "WINDOW SYSTEM";
        break;

        case GL_DEBUG_SOURCE_SHADER_COMPILER:
        _source = "SHADER COMPILER";
        break;

        case GL_DEBUG_SOURCE_THIRD_PARTY:
        _source = "THIRD PARTY";
        break;

        case GL_DEBUG_SOURCE_APPLICATION:
        _source = "APPLICATION";
        break;

        case GL_DEBUG_SOURCE_OTHER:
        _source = "OTHER";
        break;

        default:
        _source = "UNKNOWN";
        break;
    }

    switch (type) {
        case GL_DEBUG_TYPE_ERROR:
        _type = "ERROR";
        break;

        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        _type = "DEPRECATED BEHAVIOR";
        break;

        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        _type = "UNDEFINED BEHAVIOR";
        break;

        case GL_DEBUG_TYPE_PORTABILITY:
        _type = "PORTABILITY";
        break;

        case GL_DEBUG_TYPE_PERFORMANCE:
        _type = "PERFORMANCE";
        break;

        case GL_DEBUG_TYPE_OTHER:
        _type = "OTHER";
        break;

        case GL_DEBUG_TYPE_MARKER:
        _type = "MARKER";
        break;

        default:
        _type = "UNKNOWN";
        break;
    }

    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:
        _severity = "HIGH";
        break;

        case GL_DEBUG_SEVERITY_MEDIUM:
        _severity = "MEDIUM";
        break;

        case GL_DEBUG_SEVERITY_LOW:
        _severity = "LOW";
        break;

        case GL_DEBUG_SEVERITY_NOTIFICATION:
        _severity = "NOTIFICATION";
        break;

        default:
        _severity = "UNKNOWN";
        break;
    }

    // if (severity == GL_DEBUG_SEVERITY_HIGH) {
        std::cerr
            << id << ": " << _type
            << " of " << _severity << " severity, raised from "
            << _source << ": " << msg << "\n";
    // }

    // Uncomment to allow simple gdb usage, if GL_DEBUG_OUTPUT_SYNCHRONOUS
    // is enabled, the stacktrace in gdb will directly point to the
    // problematic opengl call.
    // std::raise(SIGINT);
}

void UpdateOCIOGLState();

static void InitImageTexture(const char * filename)
{
    OCIO::ImageIO img;

    if (filename && *filename)
    {
        std::cout << "Loading: " << filename << std::endl;

        try
        {
            img.read(filename, OCIO::BIT_DEPTH_F32);
        }
        catch (const std::exception & e)
        {
            std::cerr << "ERROR: Loading file failed: " << e.what() << std::endl;
            exit(1);
        }
        catch (...)
        {
            std::cerr << "ERROR: Loading file failed." << std::endl;
            exit(1);
        }
    }
    // If no file is provided, use a default gradient texture
    else
    {
        std::cout << "No image specified, loading gradient." << std::endl;

        img.init(512, 512, OCIO::CHANNEL_ORDERING_RGBA, OCIO::BIT_DEPTH_F32);

        float * pixels = (float *) img.getData();
        const long width = img.getWidth();
        const long channels = img.getNumChannels();

        for (int y=0; y<img.getHeight(); ++y)
        {
            for (int x=0; x<img.getWidth(); ++x)
            {
                float c = (float)x / ((float)width-1.0f);
                pixels[channels*(width*y+x) + 0] = c;
                pixels[channels*(width*y+x) + 1] = c;
                pixels[channels*(width*y+x) + 2] = c;
                pixels[channels*(width*y+x) + 3] = 1.0f;
            }
        }
    }

    OCIO::OglApp::Components comp = OCIO::OglApp::COMPONENTS_RGBA;
    if (img.getNumChannels() == 4)
    {
        comp = OCIO::OglApp::COMPONENTS_RGBA;
    }
    else if (img.getNumChannels() == 3)
    {
        comp = OCIO::OglApp::COMPONENTS_RGB;
    }
    else
    {
        std::cerr << "Cannot load image with " << img.getNumChannels()
                  << " components." << std::endl;
        exit(1);
    }

    g_imageAspect = 1.0;
    if (img.getHeight()!=0)
    {
        g_imageAspect = (float) img.getWidth() / (float) img.getHeight();
        g_imageWidth = img.getWidth();
        g_imageHeight = img.getHeight();
    }

    if (g_oglApp)
    {
        g_oglApp->initImage(img.getWidth(),
                            img.getHeight(),
                            comp,
                            (float*) img.getData());
    }

}

void InitOCIO(const char * filename)
{
    OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();
    g_display = config->getDefaultDisplay();
    g_transformName = config->getDefaultView(g_display.c_str());
    g_look = config->getDisplayViewLooks(g_display.c_str(), g_transformName.c_str());
    g_inputColorSpace = OCIO::ROLE_SCENE_LINEAR;

    if (filename && *filename)
    {
        std::string cs = config->getColorSpaceFromFilepath(filename);
        if (!cs.empty())
        {
            g_inputColorSpace = cs;
            std::cout << "colorspace: " << cs << std::endl;
        }
        else
        {
            std::cout << "colorspace: " << g_inputColorSpace 
                      << " \t(could not determine from filename, using default)"
                      << std::endl;
        }
    }
}

#ifdef OPENGL_QUERY

    // two query buffers: front and back
    #define QUERY_BUFFERS 2
    // the number of required queries
    // in this example there is only one query per frame
    #define QUERY_COUNT 1
    // the array to store the two sets of queries.
    unsigned int queryID[QUERY_BUFFERS][QUERY_COUNT];
    unsigned int queryBackBuffer = 0, queryFrontBuffer = 1;

    // call this function when initializating the OpenGL settings
    void genQueries()
    {
        glGenQueries(QUERY_COUNT, queryID[queryBackBuffer]);
        glGenQueries(QUERY_COUNT, queryID[queryFrontBuffer]);

        // dummy query to prevent OpenGL errors from popping out
        // for the first frame, the previous (front buffer) query
        // would otherwise be empty
        glBeginQuery(GL_TIME_ELAPSED, queryID[queryFrontBuffer][0]);
        glEndQuery(GL_TIME_ELAPSED);
    }

    void swapQueryBuffers()
    {
        if (queryBackBuffer) {
            queryBackBuffer = 0;
            queryFrontBuffer = 1;
        }
        else {
            queryBackBuffer = 1;
            queryFrontBuffer = 0;
        }
    }

#endif

#if defined USE_UBO || defined USE_SSBO

    GLuint buffer_id = 0;
    unsigned int buffer_size = (4*360 + 4*360 + 360 + 360) * sizeof(float);

    void genBuffer()
    {
        glCreateBuffers(1, &buffer_id);
    }

    void fillBuffer()
    {
        glNamedBufferData(buffer_id, buffer_size, nullptr, GL_STATIC_DRAW);

        float *buf = (float*) glMapNamedBuffer(buffer_id, GL_WRITE_ONLY);

        memcpy(buf, reach_gamut_table, table_size*4*sizeof(float));
        buf += table_size*4;

        memcpy(buf, gamut_cusp_table, table_size*4*sizeof(float));
        buf += table_size*4;

        memcpy(buf, reach_cusp_table, table_size*sizeof(float));
        buf += table_size;

        memcpy(buf, upper_hull_gamma_table, table_size*sizeof(float));
        buf += table_size;

        glUnmapNamedBuffer(buffer_id);
    }

#elif defined USE_TEXTURE

    const GLsizei tex_width = 360;
    GLuint tex_id[4] {0, 0, 0, 0};

    void genBuffer()
    {
        glCreateTextures(GL_TEXTURE_1D, 4, tex_id);

        glTextureStorage1D(tex_id[0], 1, GL_RGBA32F, tex_width);
        glTextureStorage1D(tex_id[1], 1, GL_RGBA32F, tex_width);
        glTextureStorage1D(tex_id[2], 1, GL_R32F, tex_width);
        glTextureStorage1D(tex_id[3], 1, GL_R32F, tex_width);
    }

    void fillBuffer()
    {
        glTextureSubImage1D(tex_id[0], 0, 0, tex_width, GL_RGBA, GL_FLOAT, reach_gamut_table);
        glTextureSubImage1D(tex_id[1], 0, 0, tex_width, GL_RGBA, GL_FLOAT, gamut_cusp_table);
        glTextureSubImage1D(tex_id[2], 0, 0, tex_width, GL_RED, GL_FLOAT, reach_cusp_table);
        glTextureSubImage1D(tex_id[3], 0, 0, tex_width, GL_RED, GL_FLOAT, upper_hull_gamma_table);
    }

#endif

void Redisplay(void)
{
    if (g_oglApp)
    {
#ifdef OPENGL_QUERY
        glBeginQuery(GL_TIME_ELAPSED, queryID[queryBackBuffer][0]);
#endif

#ifdef USE_SSBO
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer_id);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer_id);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
#elif defined USE_UBO
        glBindBuffer(GL_UNIFORM_BUFFER, buffer_id);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, buffer_id);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
#elif defined USE_TEXTURE
        glBindTextureUnit(2, tex_id[0]);
        glBindTextureUnit(3, tex_id[1]);
        glBindTextureUnit(4, tex_id[2]);
        glBindTextureUnit(5, tex_id[3]);
#endif

        g_oglApp->redisplay_noswap();

#ifdef OPENGL_QUERY
        glEndQuery(GL_TIME_ELAPSED);
#endif

        glutSwapBuffers();

#ifdef OPENGL_QUERY
        GLuint64 elapsed_time;
        glGetQueryObjectui64v(
            queryID[queryFrontBuffer][0],
            GL_QUERY_RESULT, &elapsed_time);
        swapQueryBuffers();

#ifdef PRINT_TIMING
        std::cerr << "elapsed_time: " << elapsed_time / 1e6 << "ms"
                  << " for viewport: " << g_oglApp->m_viewportWidth << "x" << g_oglApp->m_viewportHeight
                  << "\n";
#endif

#endif

        glutPostRedisplay();
    }
}

static void Reshape(int width, int height)
{
    if (g_oglApp)
    {
        g_oglApp->reshape(width, height);
    }
}

static void CleanUp(void)
{
    g_oglApp.reset();
}

static void Key(unsigned char key, int /*x*/, int /*y*/)
{
    if (key == 'c' || key == 'C')
    {
        g_channelHot[0] = 1;
        g_channelHot[1] = 1;
        g_channelHot[2] = 1;
        g_channelHot[3] = 1;
    }
    else if (key == 'r' || key == 'R')
    {
        g_channelHot[0] = 1;
        g_channelHot[1] = 0;
        g_channelHot[2] = 0;
        g_channelHot[3] = 0;
    }
    else if (key == 'g' || key == 'G')
    {
        g_channelHot[0] = 0;
        g_channelHot[1] = 1;
        g_channelHot[2] = 0;
        g_channelHot[3] = 0;
    }
    else if (key == 'b' || key == 'B')
    {
        g_channelHot[0] = 0;
        g_channelHot[1] = 0;
        g_channelHot[2] = 1;
        g_channelHot[3] = 0;
    }
    else if (key == 'a' || key == 'A')
    {
        g_channelHot[0] = 0;
        g_channelHot[1] = 0;
        g_channelHot[2] = 0;
        g_channelHot[3] = 1;
    }
    else if (key == 'l' || key == 'L')
    {
        g_channelHot[0] = 1;
        g_channelHot[1] = 1;
        g_channelHot[2] = 1;
        g_channelHot[3] = 0;
    }
    else if (key == 27)
    {
        CleanUp();
        exit(0);
    }

    UpdateOCIOGLState();
    glutPostRedisplay();
}

static void SpecialKey(int key, int x, int y)
{
    (void) x;
    (void) y;

    int mod = glutGetModifiers();

    if(key == GLUT_KEY_UP && (mod & GLUT_ACTIVE_CTRL))
    {
        g_exposure_fstop += 0.25f;
    }
    else if(key == GLUT_KEY_DOWN && (mod & GLUT_ACTIVE_CTRL))
    {
        g_exposure_fstop -= 0.25f;
    }
    else if(key == GLUT_KEY_HOME && (mod & GLUT_ACTIVE_CTRL))
    {
        g_exposure_fstop = 0.0f;
        g_display_gamma = 1.0f;
    }

    else if(key == GLUT_KEY_UP && (mod & GLUT_ACTIVE_ALT))
    {
        g_display_gamma *= 1.1f;
    }
    else if(key == GLUT_KEY_DOWN && (mod & GLUT_ACTIVE_ALT))
    {
        g_display_gamma /= 1.1f;
    }
    else if(key == GLUT_KEY_HOME && (mod & GLUT_ACTIVE_ALT))
    {
        g_exposure_fstop = 0.0f;
        g_display_gamma = 1.0f;
    }

    UpdateOCIOGLState();

    glutPostRedisplay();
}

void UpdateOCIOGLState()
{
    if (!g_oglApp)
    {
        return;
    }

    // Step 0: Get the processor using any of the pipelines mentioned above.
    OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();

    OCIO::DisplayViewTransformRcPtr transform = OCIO::DisplayViewTransform::Create();
    transform->setSrc( g_inputColorSpace.c_str() );
    transform->setDisplay( g_display.c_str() );
    transform->setView( g_transformName.c_str() );

    OCIO::LegacyViewingPipelineRcPtr vp = OCIO::LegacyViewingPipeline::Create();
    vp->setDisplayViewTransform(transform);
    vp->setLooksOverrideEnabled(true);
    vp->setLooksOverride(g_look.c_str());

    if (g_verbose)
    {
        std::cout << std::endl;
        std::cout << "Color transformation composed of:" << std::endl;
        std::cout << "      Image ColorSpace is:\t" << g_inputColorSpace << std::endl;
        std::cout << "      Transform is:\t\t" << g_transformName << std::endl;
        std::cout << "      Device is:\t\t" << g_display << std::endl;
        std::cout << "      Looks Override is:\t'" << g_look << "'" << std::endl;
        std::cout << "  with:" << std::endl;
        std::cout << "    exposure_fstop = " << g_exposure_fstop << std::endl;
        std::cout << "    display_gamma  = " << g_display_gamma << std::endl;
        std::cout << "    channels       = " 
                  << (g_channelHot[0] ? "R" : "")
                  << (g_channelHot[1] ? "G" : "")
                  << (g_channelHot[2] ? "B" : "")
                  << (g_channelHot[3] ? "A" : "") << std::endl;

        for (const auto & opt : OptmizationMenu)
        {
            if (opt.second == g_optimization)
            {
                std::cout << std::endl << "Optimization: " << opt.first << std::endl;
            }
        }

    }

    // Add optional transforms to create a full-featured, "canonical" display pipeline
    // Fstop exposure control (in SCENE_LINEAR)
    {
        double gain = powf(2.0f, g_exposure_fstop);
        const double slope4f[] = { gain, gain, gain, gain };
        double m44[16];
        double offset4[4];
        OCIO::MatrixTransform::Scale(m44, offset4, slope4f);
        OCIO::MatrixTransformRcPtr mtx =  OCIO::MatrixTransform::Create();
        mtx->setMatrix(m44);
        mtx->setOffset(offset4);
        vp->setLinearCC(mtx);
    }

    // Channel swizzling
    {
        double lumacoef[3];
        config->getDefaultLumaCoefs(lumacoef);
        double m44[16];
        double offset[4];
        OCIO::MatrixTransform::View(m44, offset, g_channelHot, lumacoef);
        OCIO::MatrixTransformRcPtr swizzle = OCIO::MatrixTransform::Create();
        swizzle->setMatrix(m44);
        swizzle->setOffset(offset);
        vp->setChannelView(swizzle);
    }

    // Post-display transform gamma
    {
        double exponent = 1.0/std::max(1e-6, static_cast<double>(g_display_gamma));
        const double exponent4f[4] = { exponent, exponent, exponent, exponent };
        OCIO::ExponentTransformRcPtr expTransform =  OCIO::ExponentTransform::Create();
        expTransform->setValue(exponent4f);
        vp->setDisplayCC(expTransform);
    }

    OCIO::ConstProcessorRcPtr processor;
    try
    {
        processor = vp->getProcessor(config, config->getCurrentContext());
    }
    catch (const OCIO::Exception & e)
    {
        std::cerr << e.what() << std::endl;
        return;
    }
    catch (...)
    {
        return;
    }

    // Set the shader context.
    OCIO::GpuShaderDescRcPtr shaderDesc = OCIO::GpuShaderDesc::CreateShaderDesc();
    shaderDesc->setLanguage(
#if __APPLE__
                            g_useMetal ?
                            OCIO::GPU_LANGUAGE_MSL_2_0 :
#endif
                            OCIO::GPU_LANGUAGE_GLSL_1_3);
    shaderDesc->setFunctionName("OCIODisplay");
    shaderDesc->setResourcePrefix("ocio_");

    // Extract the shader information.
    OCIO::ConstGPUProcessorRcPtr gpu
        = g_gpulegacy ? processor->getOptimizedLegacyGPUProcessor(g_optimization, 32)
                    : processor->getOptimizedGPUProcessor(g_optimization);
    gpu->extractGpuShaderInfo(shaderDesc);


#ifdef HARDCODED_SHADER

    std::ifstream fileStream(SHADER_PATH);
    if( !fileStream.is_open() )
        std::cout << "Could not open : " << SHADER_PATH << std::endl;

    std::string str((std::istreambuf_iterator<char>(fileStream)),
            std::istreambuf_iterator<char>());
    fileStream.close();

    std::ostringstream oss;
#ifdef USE_SSBO
    oss << "#define USE_SSBO\n";
#elif defined USE_UBO
    oss << "#define USE_UBO\n";
#elif defined USE_TEXTURE
    oss << "#define USE_TEXTURE\n";
#endif
    oss << "\n\n" << str;

#ifdef PRINT_SHADER
    std::cout << "Shader is:\n" << oss.str() << "\n";
#endif

    g_oglApp->setShader(shaderDesc, oss.str());

#else

    g_oglApp->setShader(shaderDesc);

#endif
}

void menuCallback(int /*id*/)
{
    glutPostRedisplay();
}

void imageColorSpace_CB(int id)
{
    OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();
    const char * name = config->getColorSpaceNameByIndex(id);
    if (!name)
    {
        return;
    }

    g_inputColorSpace = name;

    UpdateOCIOGLState();
    glutPostRedisplay();
}

void displayDevice_CB(int id)
{
    OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();
    const char * display = config->getDisplay(id);
    if (!display)
    {
        return;
    }

    g_display = display;

    const char * csname = config->getDisplayViewColorSpaceName(g_display.c_str(), g_transformName.c_str());
    if (!csname || !*csname)
    {
        g_transformName = config->getDefaultView(g_display.c_str());
    }

    g_look = config->getDisplayViewLooks(g_display.c_str(), g_transformName.c_str());

    UpdateOCIOGLState();
    glutPostRedisplay();
}

void transform_CB(int id)
{
    OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();

    const char * transform = config->getView(g_display.c_str(), id);
    if (!transform)
    {
        return;
    }

    g_transformName = transform;

    g_look = config->getDisplayViewLooks(g_display.c_str(), g_transformName.c_str());

    UpdateOCIOGLState();
    glutPostRedisplay();
}

void look_CB(int id)
{
    OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();

    const char * look = config->getLookNameByIndex(id);
    if (!look || !*look)
    {
        return;
    }

    g_look = look;

    UpdateOCIOGLState();
    glutPostRedisplay();
}

void optimization_CB(int id)
{
    g_optimization = OptmizationMenu[id].second;

    UpdateOCIOGLState();
    glutPostRedisplay();
}

static void PopulateOCIOMenus()
{
    OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();

    int csMenuID = glutCreateMenu(imageColorSpace_CB);

    std::map<std::string, int> families;
    for (int i=0; i<config->getNumColorSpaces(); ++i)
    {
        const char * csName = config->getColorSpaceNameByIndex(i);
        if (csName && *csName)
        {
            OCIO::ConstColorSpaceRcPtr cs = config->getColorSpace(csName);
            if (cs)
            {
                const char * family = cs->getFamily();
                if (family && *family)
                {
                    if (families.find(family)==families.end())
                    {
                        families[family] = glutCreateMenu(imageColorSpace_CB);
                        glutAddMenuEntry(csName, i);

                        glutSetMenu(csMenuID);
                        glutAddSubMenu(family, families[family]);
                    }
                    else
                    {
                        glutSetMenu(families[family]);
                        glutAddMenuEntry(csName, i);
                    }
                }
                else
                {
                    glutSetMenu(csMenuID);
                    glutAddMenuEntry(csName, i);
                }
            }
        }
    }

    int deviceMenuID = glutCreateMenu(displayDevice_CB);
    for (int i=0; i<config->getNumDisplays(); ++i)
    {
        glutAddMenuEntry(config->getDisplay(i), i);
    }

    int transformMenuID = glutCreateMenu(transform_CB);
    const char * defaultDisplay = config->getDefaultDisplay();
    for (int i=0; i<config->getNumViews(defaultDisplay); ++i)
    {
        glutAddMenuEntry(config->getView(defaultDisplay, i), i);
    }

    int lookMenuID = glutCreateMenu(look_CB);
    for (int i=0; i<config->getNumLooks(); ++i)
    {
        glutAddMenuEntry(config->getLookNameByIndex(i), i);
    }

    int optimizationMenuID = glutCreateMenu(optimization_CB);
    for (size_t i = 0; i<OptmizationMenu.size(); ++i)
    {
        glutAddMenuEntry(OptmizationMenu[i].first, static_cast<int>(i));
    }

    glutCreateMenu(menuCallback);
    glutAddSubMenu("Image ColorSpace", csMenuID);
    glutAddSubMenu("Transform", transformMenuID);
    glutAddSubMenu("Device", deviceMenuID);
    glutAddSubMenu("Looks Override", lookMenuID);
    glutAddSubMenu("Optimization", optimizationMenuID);

    glutAttachMenu(GLUT_RIGHT_BUTTON);
}

const char * USAGE_TEXT = "\n"
"Keys:\n"
"\tCtrl+Up:   Exposure +1/4 stop (in scene linear)\n"
"\tCtrl+Down: Exposure -1/4 stop (in scene linear)\n"
"\tCtrl+Home: Reset Exposure + Gamma\n"
"\n"
"\tAlt+Up:    Gamma up (post display transform)\n"
"\tAlt+Down:  Gamma down (post display transform)\n"
"\tAlt+Home:  Reset Exposure + Gamma\n"
"\n"
"\tC:   View Color\n"
"\tR:   View Red\n"
"\tG:   View Green\n"
"\tB:   View Blue\n"
"\tA:   View Alpha\n"
"\tL:   View Luma\n"
"\n"
"\tRight-Mouse Button:   Configure Display / Transform / ColorSpace / Looks / Optimization\n"
"\n"
"\tEsc: Quit\n";

void parseArguments(int argc, char **argv)
{
    for (int i=1; i<argc; ++i)
    {
        if (0==strcmp(argv[i], "-v"))
        {
            g_verbose = true;
        }
        else if (0==strcmp(argv[i], "-gpulegacy"))
        {
            g_gpulegacy = true;
        }
        else if (0==strcmp(argv[i], "-gpuinfo"))
        {
            g_gpuinfo = true;
        }
#if __APPLE__
        else if (0==strcmp(argv[i], "-metal"))
        {
            g_useMetal = true;
        }
#endif
        else if (0==strcmp(argv[i], "-h"))
        {
            std::cout << std::endl;
            std::cout << "help:" << std::endl;
            std::cout << "  ociodisplay [OPTIONS] [image]  where" << std::endl;
            std::cout << std::endl;
            std::cout << "  OPTIONS:" << std::endl;
            std::cout << "     -h         :  displays the help and exit" << std::endl;
            std::cout << "     -v         :  displays the color space information" << std::endl;
            std::cout << "     -gpulegacy :  use the legacy (i.e. baked) GPU color processing" << std::endl;
            std::cout << "     -gpuinfo   :  output the OCIO shader program" << std::endl;
#if __APPLE__
            std::cout << "     -metal     :  use metal OCIO shader backend " << std::endl;
#endif
            std::cout << std::endl;
            exit(0);
        }
        else
        {
            g_filename = argv[i];
        }
    }
}

int main(int argc, char **argv)
{
    parseArguments(argc, argv);

    try
    {
#if __APPLE__
        if (g_useMetal)
        {
            g_oglApp = std::make_shared<OCIO::MetalApp>("ociodisplay", 512, 512);
        }
        else
#endif
        {
            g_oglApp = std::make_shared<OCIO::ScreenApp>("ociodisplay", 512, 512);
        }
    }
    catch (const OCIO::Exception & e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    if (g_verbose)
    {
        g_oglApp->printGLInfo();
    }

    g_oglApp->setYMirror();
    g_oglApp->setPrintShader(g_gpuinfo);

#ifdef OPENGL_DEBUG_CB
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(debug_message_callback, nullptr);
#endif

#ifdef OPENGL_QUERY
    genQueries();
#endif

#if defined USE_UBO || defined USE_SSBO || defined USE_TEXTURE
    genBuffer();
    fillBuffer();
#endif

    GLint maxLocs = 0, maxComp = 0, maxVec = 0;
    glGetIntegerv(GL_MAX_UNIFORM_LOCATIONS, &maxLocs);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &maxComp);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &maxVec);
    std::cerr << "\n";
    std::cerr << "GL_MAX_UNIFORM_LOCATIONS: " << maxLocs << "\n";
    std::cerr << "GL_MAX_FRAGMENT_UNIFORM_COMPONENTS: " << maxComp << "\n";
    std::cerr << "GL_MAX_FRAGMENT_UNIFORM_VECTORS: " << maxVec << "\n";

    glutReshapeFunc(Reshape);
    glutKeyboardFunc(Key);
    glutSpecialFunc(SpecialKey);
    glutDisplayFunc(Redisplay);

    if (g_verbose)
    {
        if (!g_filename.empty())
        {
            std::cout << std::endl;
            std::cout << "Image: " << g_filename << std::endl;
        }
        std::cout << std::endl;
        std::cout << OCIO::ImageIO::GetVersion() << std::endl;
        std::cout << "OCIO Version: " << OCIO::GetVersion() << std::endl;
    }

    OCIO::ConstConfigRcPtr config;
    try
    {
        config = OCIO::GetCurrentConfig();
    }
    catch (...)
    {
        const char * env = OCIO::GetEnvVariable("OCIO");
        std::cerr << "Error loading the config file: '" << (env ? env : "") << "'";
        exit(1);
    }

    if (g_verbose)
    {
        const char * env = OCIO::GetEnvVariable("OCIO");

        if (env && *env)
        {
            std::cout << std::endl;
            std::cout << "OCIO Config. file   : '" << env << "'" << std::endl;
            std::cout << "OCIO Config. version: " << config->getMajorVersion() << "." 
                                                  << config->getMinorVersion() << std::endl;
            std::cout << "OCIO search_path    : " << config->getSearchPath() << std::endl;
        }
    }

    std::cout << std::endl;
    std::cout << USAGE_TEXT << std::endl;

    InitImageTexture(g_filename.c_str());
    try
    {
        InitOCIO(g_filename.c_str());
    }
    catch(OCIO::Exception & e)
    {
        std::cerr << e.what() << std::endl;
        exit(1);
    }

    PopulateOCIOMenus();

    try
    {
        UpdateOCIOGLState();
    }
    catch (const OCIO::Exception & e)
    {
        std::cerr << e.what() << std::endl;
        exit(1);
    }

#ifndef RENDER_IMAGE

    Redisplay();

    glutMainLoop();

#else

    g_oglApp->createGLBuffers();

    OCIO::ImageIO outImg(
        g_imageWidth, g_imageHeight,
        OCIO::CHANNEL_ORDERING_RGBA,
        OCIO::BIT_DEPTH_F32);

    g_oglApp->reshape(g_imageWidth, g_imageHeight);
    Redisplay();
    g_oglApp->readImage((float *)outImg.getData());
    outImg.write(RENDER_PATH);

#endif

    return 0;
}
