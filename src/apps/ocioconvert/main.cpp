// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;

#include "apputils/argparse.h"

#ifdef OCIO_GPU_ENABLED
#include "oglapp.h"
#endif // OCIO_GPU_ENABLED

#include "imageio.h"

// Array of non OpenColorIO arguments.
static std::vector<std::string> args;

// Fill 'args' array with OpenColorIO arguments.
static int parse_end_args(int argc, const char *argv[])
{
    while (argc>0)
    {
        args.push_back(argv[0]);
        argc--;
        argv++;
    }

    return 0;
}

bool ParseNameValuePair(std::string& name, std::string& value, const std::string& input);

bool StringToFloat(float * fval, const char * str);

bool StringToInt(int * ival, const char * str);

bool StringToVector(std::vector<int> * ivector, const char * str);

int main(int argc, const char **argv)
{
    ArgParse ap;

    std::vector<std::string> floatAttrs;
    std::vector<std::string> intAttrs;
    std::vector<std::string> stringAttrs;
    std::string keepChannels;

    bool croptofull     = false;
    bool usegpu         = false;
    bool usegpuLegacy   = false;
    bool outputgpuInfo  = false;
    bool verbose        = false;
    bool help           = false;
    bool useLut         = false;
    bool useDisplayView = false;

    ap.options("ocioconvert -- apply colorspace transform to an image \n\n"
               "usage: ocioconvert [options] inputimage inputcolorspace outputimage outputcolorspace\n"
               "   or: ocioconvert [options] --lut lutfile inputimage outputimage\n"
               "   or: ocioconvert [options] --view inputimage inputcolorspace outputimage displayname viewname\n\n",
               "%*", parse_end_args, "",
               "<SEPARATOR>", "Options:",
               "--lut",       &useLut,         "Convert using a LUT rather than a config file",
               "--view",      &useDisplayView, "Convert to a (display,view) pair rather than to "
                                               "an output color space",
               "--gpu",       &usegpu,         "Use GPU color processing instead of CPU (CPU is the default)",
               "--gpulegacy", &usegpuLegacy,   "Use the legacy (i.e. baked) GPU color processing "
                                               "instead of the CPU one (--gpu is ignored)",
               "--gpuinfo",  &outputgpuInfo,   "Output the OCIO shader program",
               "--help",     &help,            "Print help message",
               "-v" ,        &verbose,         "Display general information",
               "<SEPARATOR>", "\nImage options:",
               "--float-attribute %L",  &floatAttrs,   "\"name=float\" pair defining a float attribute "
                                                       "for outputimage",
               "--int-attribute %L",    &intAttrs,     "\"name=int\" pair defining an int attribute "
                                                       "for outputimage",
               "--string-attribute %L", &stringAttrs,  "\"name=string\" pair defining a string attribute "
                                                       "for outputimage",
               "--croptofull",          &croptofull,   "Crop or pad to make pixel data region match the "
                                                       "\"full\" region",
               "--ch %s",               &keepChannels, "Select channels (e.g., \"2,3,4\")",
               NULL
               );

    if (ap.parse (argc, argv) < 0)
    {
        std::cerr << ap.geterror() << std::endl;
        ap.usage ();
        exit(1);
    }

    if (help)
    {
        ap.usage();
        return 0;
    }

#ifndef OCIO_GPU_ENABLED
    if (usegpu || outputgpuInfo || usegpuLegacy)
    {
        std::cerr << "Compiled without OpenGL support, GPU options are not available.";
        std::cerr << std::endl;
        exit(1);
    }
#endif // OCIO_GPU_ENABLED

    const char * inputimage       = nullptr;
    const char * inputcolorspace  = nullptr;
    const char * outputimage      = nullptr;
    const char * outputcolorspace = nullptr;
    const char * lutFile          = nullptr;
    const char * display          = nullptr;
    const char * view             = nullptr;

    if (!useLut && !useDisplayView)
    {
        if (args.size() != 4)
        {
            std::cerr << "ERROR: Expecting 4 arguments, found " 
                      << args.size() << "." << std::endl;
            ap.usage();
            exit(1);
        }
        inputimage       = args[0].c_str();
        inputcolorspace  = args[1].c_str();
        outputimage      = args[2].c_str();
        outputcolorspace = args[3].c_str();
    }
    else if (useLut && useDisplayView)
    {
        std::cerr << "ERROR: Options lut & view can't be used at the same time." << std::endl;
        ap.usage();
        exit(1);
    }
    else if (useLut)
    {
        if (args.size() != 3)
        {
            std::cerr << "ERROR: Expecting 3 arguments for --lut option, found "
                      << args.size() << "." << std::endl;
            ap.usage();
            exit(1);
        }
        lutFile     = args[0].c_str();
        inputimage  = args[1].c_str();
        outputimage = args[2].c_str();
    }
    else if (useDisplayView)
    {
        if (args.size() != 5)
        {
            std::cerr << "ERROR: Expecting 5 arguments for --view option, found "
                      << args.size() << "." << std::endl;
            ap.usage();
            exit(1);
        }
        inputimage      = args[0].c_str();
        inputcolorspace = args[1].c_str();
        outputimage     = args[2].c_str();
        display         = args[3].c_str();
        view            = args[4].c_str();
    }

    if (verbose)
    {
        std::cout << std::endl;
        std::cout << OCIO::ImageIO::GetVersion() << std::endl;
        std::cout << "OCIO Version: " << OCIO::GetVersion() << std::endl;
        const char * env = OCIO::GetEnvVariable("OCIO");
        if (env && *env && !useLut)
        {
            try
            {
                std::cout << std::endl;
                std::cout << "OCIO Config. file:    '" << env << "'" << std::endl;
                OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();
                std::cout << "OCIO Config. version: " << config->getMajorVersion() << "." 
                                                      << config->getMinorVersion() << std::endl;
                std::cout << "OCIO search_path:     " << config->getSearchPath() << std::endl;
            }
            catch (const OCIO::Exception & e)
            {
                std::cout << "ERROR loading config file: " << e.what() << std::endl;
                exit(1);
            }
            catch (...)
            {

                std::cerr << "ERROR loading config file: '" << env << "'" << std::endl;
                exit(1);
            }
        }
    }

    if (usegpuLegacy)
    {
        std::cout << std::endl;
        std::cout << "Using legacy OCIO v1 GPU color processing." << std::endl;
    }
    else if (usegpu)
    {
        std::cout << std::endl;
        std::cout << "Using GPU color processing." << std::endl;
    }

    OCIO::ImageIO img;

    // Load the image.
    std::cout << std::endl;
    std::cout << "Loading " << inputimage << std::endl;
    try
    {
        if (usegpu || usegpuLegacy)
        {
            if (croptofull)
            {
                std::cerr << "ERROR: Crop disabled in GPU mode." << std::endl;
                exit(1);
            }

            img.read(inputimage, OCIO::BIT_DEPTH_F32);
        }
        else
        {
            img.read(inputimage);
        }

        std::cout << img.getImageDescStr() << std::endl;

        // Parse --ch argument.
        std::vector<int> kchannels;
        if (keepChannels != "" && !StringToVector(&kchannels, keepChannels.c_str()))
        {
            std::cerr << "ERROR: --ch: '" << keepChannels << "'"
                      << " should be comma-seperated integers." << std::endl;
            exit(1);
        }

        if (!kchannels.empty())
        {
            img.filterChannels(kchannels);
            std::cout << "Filter channels " << keepChannels.c_str() << std::endl;
        }

        if (croptofull)
        {
            img.cropToFull();
            std::cout << "Cropping to "
                      << img.getWidth() << "x" << img.getHeight()
                      << std::endl;
        }
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

#ifdef OCIO_GPU_ENABLED
    // Initialize GPU.
    OCIO::OglAppRcPtr oglApp;

    if (usegpu || usegpuLegacy)
    {
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
            std::cerr << "Cannot convert image with " << img.getNumChannels()
                      << " components." << std::endl;
            exit(1);
        }

        try
        {
            oglApp = OCIO::OglApp::CreateOglApp("ocioconvert", 256, 20);
        }
        catch (const OCIO::Exception & e)
        {
            std::cerr << std::endl << e.what() << std::endl;
            exit(1);
        }

        if (verbose)
        {
            oglApp->printGLInfo();
        }

        oglApp->setPrintShader(outputgpuInfo);

        oglApp->initImage(img.getWidth(), img.getHeight(), comp, (float *)img.getData());
        
        oglApp->createGLBuffers();
    }
#endif // OCIO_GPU_ENABLED

    // Process the image.
    try
    {
        // Load the current config.
        OCIO::ConstConfigRcPtr config
            = useLut ? OCIO::Config::CreateRaw() : OCIO::GetCurrentConfig();

        // Get the processor.
        OCIO::ConstProcessorRcPtr processor;

        try
        {
            if (useLut)
            {
                // Create the OCIO processor for the specified transform.
                OCIO::FileTransformRcPtr t = OCIO::FileTransform::Create();
                t->setSrc(lutFile);
                t->setInterpolation(OCIO::INTERP_BEST);
    
                processor = config->getProcessor(t);
            }
            else if (useDisplayView)
            {
                OCIO::DisplayViewTransformRcPtr t = OCIO::DisplayViewTransform::Create();
                t->setSrc(inputcolorspace);
                t->setDisplay(display);
                t->setView(view);
                processor = config->getProcessor(t);
            }
            else
            {
                processor = config->getProcessor(inputcolorspace, outputcolorspace);
            }
        }
        catch (const OCIO::Exception & e)
        {
            std::cout << "ERROR: OCIO failed with: " << e.what() << std::endl;
            exit(1);
        }
        catch (...)
        {
            std::cout << "ERROR: Creating processor unknown failure." << std::endl;
            exit(1);
        }

#ifdef OCIO_GPU_ENABLED
        if (usegpu || usegpuLegacy)
        {
            // Get the GPU shader program from the processor and set oglApp to use it.
            OCIO::GpuShaderDescRcPtr shaderDesc = OCIO::GpuShaderDesc::CreateShaderDesc();
            shaderDesc->setLanguage(OCIO::GPU_LANGUAGE_GLSL_1_2);

            OCIO::ConstGPUProcessorRcPtr gpu
                = usegpuLegacy ? processor->getOptimizedLegacyGPUProcessor(OCIO::OPTIMIZATION_DEFAULT, 32)
                               : processor->getDefaultGPUProcessor();
            gpu->extractGpuShaderInfo(shaderDesc);

            oglApp->setShader(shaderDesc);
            oglApp->reshape(img.getWidth(), img.getHeight());
            oglApp->redisplay();
            oglApp->readImage((float *)img.getData());
        }
        else
#endif // OCIO_GPU_ENABLED
        {
            const OCIO::BitDepth bitDepth = img.getBitDepth();

            OCIO::ConstCPUProcessorRcPtr cpuProcessor 
                = processor->getOptimizedCPUProcessor(bitDepth, bitDepth,
                                                      OCIO::OPTIMIZATION_DEFAULT);

            const std::chrono::high_resolution_clock::time_point start
                = std::chrono::high_resolution_clock::now();

            auto imgDesc = img.getImageDesc();
            cpuProcessor->apply(*imgDesc);

            if (verbose)
            {
                const std::chrono::high_resolution_clock::time_point end
                    = std::chrono::high_resolution_clock::now();

                std::chrono::duration<float, std::milli> duration = end - start;

                std::cout << std::endl;
                std::cout << "CPU processing took: " 
                          << duration.count()
                          <<  " ms" << std::endl;
            }
        }
    }
    catch (const OCIO::Exception & exception)
    {
        std::cerr << "ERROR: OCIO failed with: " << exception.what() << std::endl;
        exit(1);
    }
    catch (...)
    {
        std::cerr << "ERROR: Unknown error processing the image." << std::endl;
        exit(1);
    }

    // Set the provided image attributes.
    bool parseError = false;
    for (unsigned int i=0; i<floatAttrs.size(); ++i)
    {
        std::string name, value;
        float fval = 0.0f;

        if (!ParseNameValuePair(name, value, floatAttrs[i]) ||
           !StringToFloat(&fval,value.c_str()))
        {
            std::cerr << "ERROR: Attribute string '" << floatAttrs[i]
                      << "' should be in the form name=floatvalue." << std::endl;
            parseError = true;
            continue;
        }

        img.attribute(name, fval);
    }

    for (unsigned int i=0; i<intAttrs.size(); ++i)
    {
        std::string name, value;
        int ival = 0;
        if (!ParseNameValuePair(name, value, intAttrs[i]) ||
           !StringToInt(&ival,value.c_str()))
        {
            std::cerr << "ERROR: Attribute string '" << intAttrs[i]
                      << "' should be in the form name=intvalue." << std::endl;
            parseError = true;
            continue;
        }

        img.attribute(name, ival);
    }

    for (unsigned int i=0; i<stringAttrs.size(); ++i)
    {
        std::string name, value;
        if (!ParseNameValuePair(name, value, stringAttrs[i]))
        {
            std::cerr << "ERROR: Attribute string '" << stringAttrs[i]
                      << "' should be in the form name=value." << std::endl;
            parseError = true;
            continue;
        }

        img.attribute(name, value);
    }

    if (parseError)
    {
        exit(1);
    }

    // Write out the result.
    try
    {
        if (useDisplayView)
        {
            OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();
            outputcolorspace = config->getDisplayViewColorSpaceName(display, view);
        }

        if (outputcolorspace)
        {
            img.attribute("oiio:ColorSpace", outputcolorspace);
        }

        img.write(outputimage);
    }
    catch (...)
    {
        std::cerr << "ERROR: Writing file \"" << outputimage << "\"." << std::endl;
        exit(1);
    }

    std::cout << std::endl;
    std::cout << "Wrote " << outputimage << std::endl;

    return 0;
}


// Parse name=value parts.
// return true on success.

bool ParseNameValuePair(std::string& name,
                        std::string& value,
                        const std::string& input)
{
    // split string into name=value.
    size_t pos = input.find('=');
    if (pos==std::string::npos)
    {
        return false;
    }

    name = input.substr(0,pos);
    value = input.substr(pos+1);
    return true;
}

// return true on success.
bool StringToFloat(float * fval, const char * str)
{
    if (!str)
    {
        return false;
    }

    std::istringstream inputStringstream(str);
    float x;
    if (!(inputStringstream >> x))
    {
        return false;
    }

    if (fval)
    {
        *fval = x;
    }
    return true;
}

bool StringToInt(int * ival, const char * str)
{
    if (!str)
    {
        return false;
    }

    std::istringstream inputStringstream(str);
    int x;
    if (!(inputStringstream >> x))
    {
        return false;
    }

    if (ival)
    {
        *ival = x;
    }
    return true;
}

bool StringToVector(std::vector<int> * ivector, const char * str)
{
    std::stringstream ss(str);
    int i;
    while (ss >> i)
    {
        ivector->push_back(i);
        if (ss.peek() == ',')
        {
            ss.ignore();
        }
    }
    return ivector->size() != 0;
}
