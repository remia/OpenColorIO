// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.


#include <iostream>
#include <sstream>
#include <utility>

#ifdef __APPLE__

#include <OpenGL/gl3.h>
#include <GLUT/glut.h>

#elif _WIN32

#include <GL/glew.h>
#include <GL/glut.h>

#else

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glut.h>

#endif


#include <OpenColorIO/OpenColorIO.h>

#include "oglapp.h"


namespace OCIO_NAMESPACE
{

OglApp::OglApp(int winWidth, int winHeight)
    : m_viewportWidth(winWidth)
    , m_viewportHeight(winHeight)
{}

OglApp::~OglApp()
{
    if (m_quadVBO)
    {
        glDeleteBuffers(1, &m_quadVBO);
        m_quadVBO = 0;
    }
    if (m_quadVAO)
    {
        glDeleteVertexArrays(1, &m_quadVAO);
        m_quadVAO = 0;
    }
    m_oglBuilder.reset();
}

void OglApp::setImageDimensions(int imgWidth, int imgHeight, Components comp)
{
    m_imageWidth = imgWidth;
    m_imageHeight = imgHeight;
    m_components = comp;
    if (m_imageHeight != 0)
    {
        m_imageAspect = (float)m_imageWidth / (float)m_imageHeight;
    }
}

void OglApp::initImage(int imgWidth, int imgHeight, Components comp, const float * image)
{
    setImageDimensions(imgWidth, imgHeight, comp);

    glGenTextures(1, &m_imageTexID);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_imageTexID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    updateImage(image);
}

void OglApp::updateImage(const float * image)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_imageTexID);

    const GLenum format = m_components == COMPONENTS_RGB ? GL_RGB : GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_imageWidth, m_imageHeight, 0,
                 format, GL_FLOAT, &image[0]);
}

void OglApp::redisplay()
{
    float viewportAspect = 1.0f;
    if (m_viewportHeight != 0)
        viewportAspect = (float)m_viewportWidth / (float)m_viewportHeight;

    // Compute letterbox quad corners in NDC space [-1, 1].
    float ndcX0, ndcX1, ndcY0, ndcY1;
    if (viewportAspect >= m_imageAspect)
    {
        // Pillarbox: image fills full height, centred horizontally.
        ndcX0 = -(m_imageAspect / viewportAspect);
        ndcX1 =  (m_imageAspect / viewportAspect);
        ndcY0 = -1.0f;
        ndcY1 =  1.0f;
    }
    else
    {
        // Letterbox: image fills full width, centred vertically.
        ndcX0 = -1.0f;
        ndcX1 =  1.0f;
        ndcY0 = -(viewportAspect / m_imageAspect);
        ndcY1 =  (viewportAspect / m_imageAspect);
    }

    // UV (0,0) = bottom-left of texture.  yMirror swaps the vertical sampling.
    float uvY0 = 0.0f, uvY1 = 1.0f;
    if (m_yMirror)
        std::swap(uvY0, uvY1);

    // Triangle strip: BL, BR, TL, TR.
    // Each vertex: { pos.x, pos.y, uv.x, uv.y }.
    const GLfloat quad[4][4] = {
        { ndcX0, ndcY0, 0.0f, uvY0 },
        { ndcX1, ndcY0, 1.0f, uvY0 },
        { ndcX0, ndcY1, 0.0f, uvY1 },
        { ndcX1, ndcY1, 1.0f, uvY1 },
    };

    if (m_oglBuilder)
        m_oglBuilder->useAllUniforms();

    glClearColor(0.2f, 0.2f, 0.2f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad), quad);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void OglApp::reshape(int width, int height)
{
    m_viewportWidth = width;
    m_viewportHeight = height;
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
}

void OglApp::createGLBuffers()
{
    GLuint fboId;
    glGenFramebuffers(1, &fboId);
    glBindFramebuffer(GL_FRAMEBUFFER, fboId);

    GLuint rboId;
    glGenRenderbuffers(1, &rboId);
    glBindRenderbuffer(GL_RENDERBUFFER, rboId);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, m_imageWidth, m_imageHeight);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_imageTexID, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rboId);
}

void OglApp::readImage(float * image)
{
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    const GLenum format = m_components == COMPONENTS_RGB ? GL_RGB : GL_RGBA;
    glReadPixels(0, 0, m_imageWidth, m_imageHeight, format, GL_FLOAT, (GLvoid*)&image[0]);
}

void OglApp::setShader(GpuShaderDescRcPtr & shaderDesc)
{
    // Create oglBuilder using the shaderDesc.
    m_oglBuilder = OpenGLBuilder::Create(shaderDesc);
    m_oglBuilder->setVerbose(m_printShader);

    // Allocate & upload all the LUTs in a dedicated GPU texture.
    // Note: The start index for the texture indices is 1 as one texture
    //       was already created for the input image.
    m_oglBuilder->allocateAllTextures(1);

    std::ostringstream main;
    main << std::endl
         << "in vec2 texCoord;" << std::endl
         << "out vec4 color;" << std::endl
         << "uniform sampler2D img;" << std::endl
         << std::endl
         << "void main()" << std::endl
         << "{" << std::endl
         << "    vec4 col = texture(img, texCoord);" << std::endl
         << "    color = " << shaderDesc->getFunctionName() << "(col);" << std::endl
         << "}" << std::endl;

    // Build the full shader program (vertex + fragment).
    m_oglBuilder->buildProgram(main.str().c_str(), false);

    // Enable the shader program and all needed resources.
    m_oglBuilder->useProgram();
    // The image texture.
    glUniform1i(glGetUniformLocation(m_oglBuilder->getProgramHandle(), "img"), 0);
    // The LUT textures.
    m_oglBuilder->useAllTextures();
    // Enable uniforms for dynamic properties.
    m_oglBuilder->useAllUniforms();
}

void OglApp::printGLInfo() const noexcept
{
    std::cout << std::endl
              << "GL Vendor:    " << glGetString(GL_VENDOR) << std::endl
              << "GL Renderer:  " << glGetString(GL_RENDERER) << std::endl
              << "GL Version:   " << glGetString(GL_VERSION) << std::endl
              << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
}

void OglApp::setupCommon()
{
#if !defined(__APPLE__)
    glewInit();
    if (!glewIsSupported("GL_VERSION_3_0"))
    {
        throw Exception("OpenGL 3.0 not supported.");
    }
#endif

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    // Create the persistent VAO/VBO used to draw the full-screen processing quad.
    setupQuadGeometry();
}

void OglApp::setupQuadGeometry()
{
    // Allocate a VAO + dynamic VBO for a 4-vertex triangle strip.
    // Each vertex stores: { pos.x, pos.y, uv.x, uv.y } (4 floats, 16 bytes).
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);

    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, 4 * 4 * sizeof(GLfloat), nullptr, GL_DYNAMIC_DRAW);

    // Attribute 0: position (vec2), stride 4 floats, offset 0.
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);

    // Attribute 1: texCoordIn (vec2), stride 4 floats, offset 2 floats.
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

OglAppRcPtr OglApp::CreateOglApp(const char * winTitle, int winWidth, int winHeight, bool useGLES)
{
#ifdef OCIO_HEADLESS_ENABLED
    return std::make_shared<HeadlessApp>(winTitle, winWidth, winHeight, useGLES);
#else
    (void)useGLES; // ScreenApp always uses desktop OpenGL via GLUT.
    return std::make_shared<ScreenApp>(winTitle, winWidth, winHeight);
#endif
}

// #ifndef OCIO_HEADLESS_ENABLED

ScreenApp::ScreenApp(const char * winTitle, int winWidth, int winHeight):
    OglApp(winWidth, winHeight)
{
    int argc = 2;
    const char * argv[] = { winTitle, "-glDebug" };

    glutInit(&argc, const_cast<char**>(&argv[0]));

    // On macOS, without GLUT_3_2_CORE_PROFILE the driver falls back to an OpenGL 2.1
    // compatibility context, which rejects "#version 400 core" shader directives.
    // Passing GLUT_3_2_CORE_PROFILE requests the highest Core Profile available (up to
    // OpenGL 4.1 on macOS), making GLSL 4.x shaders compile correctly.
#ifdef __APPLE__
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_3_2_CORE_PROFILE);
#else
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif
    glutInitWindowSize(m_viewportWidth, m_viewportHeight);
    glutInitWindowPosition(0, 0);

    m_mainWin = glutCreateWindow(argv[0]);

    setupCommon();
}

ScreenApp::~ScreenApp()
{
    glutDestroyWindow(m_mainWin);
}

void ScreenApp::redisplay()
{
    OglApp::redisplay();
    glutSwapBuffers();
}

void ScreenApp::printGLInfo() const noexcept
{
    OglApp::printGLInfo();
}

// #endif // !OCIO_HEADLESS_ENABLED

#ifdef OCIO_HEADLESS_ENABLED

HeadlessApp::HeadlessApp(const char * /* winTitle */, int bufWidth, int bufHeight, bool useGLES)
    : OglApp(bufWidth, bufHeight)
    , m_pixBufferWidth(bufWidth)
    , m_pixBufferHeight(bufHeight)
{
    // Choose the EGL renderable type based on the requested context type.
    const EGLint renderableType = useGLES ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_BIT;

    m_configAttribs =
    {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 8,
        EGL_RENDERABLE_TYPE, renderableType,
        EGL_NONE
    };

    m_pixBufferAttribs =
    {
        EGL_WIDTH, m_pixBufferWidth,
        EGL_HEIGHT, m_pixBufferHeight,
        EGL_NONE,
    };

    m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if(m_eglDisplay == EGL_NO_DISPLAY )
    {
        throw Exception("EGL could not be initialized.");
    }

    EGLint eglMajor, eglMinor;
    if(eglInitialize(m_eglDisplay, &eglMajor, &eglMinor) != EGL_TRUE)
    {
        throw Exception("EGL display connection couldn't be started.");
    }

    // Choose an appropriate configuration.
    EGLint numConfigs;
    if(eglChooseConfig(m_eglDisplay, &m_configAttribs[0], &m_eglConfig, 1, &numConfigs) != EGL_TRUE)
    {
        throw Exception("Failed to choose EGL configuration.");
    }

    m_eglSurface = eglCreatePbufferSurface(m_eglDisplay, m_eglConfig, &m_pixBufferAttribs[0]);

    if (useGLES)
    {
        eglBindAPI(EGL_OPENGL_ES_API);
        // Request an OpenGL ES 3.0 context.
        EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
        m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, contextAttribs);
    }
    else
    {
        eglBindAPI(EGL_OPENGL_API);
        // Request a desktop OpenGL context; the driver selects the best available version.
        EGLint contextAttribs[] = { EGL_NONE };
        m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, contextAttribs);
    }

    if(eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext) != EGL_TRUE)
    {
        throw Exception("Could not make EGL context current.");
    }

    // Must be set before setupCommon() so GLEW is initialised only for desktop GL.
    m_useGLES = useGLES;
    setupCommon();
}

HeadlessApp::~HeadlessApp()
{
    eglTerminate(m_eglDisplay);
}

void HeadlessApp::printGLInfo() const noexcept
{
    OglApp::printGLInfo();
    printEGLInfo();
}

void HeadlessApp::printEGLInfo() const noexcept
{
    std::cout << std::endl
              << "EGL Vendor:   " << eglQueryString(m_eglDisplay, EGL_VENDOR) << std::endl
              << "EGL Version:  " << eglQueryString(m_eglDisplay, EGL_VERSION) << std::endl;
}

void HeadlessApp::redisplay()
{
    OglApp::redisplay();
    eglSwapBuffers(m_eglDisplay, m_eglSurface);
}

#endif

} // namespace OCIO_NAMESPACE
