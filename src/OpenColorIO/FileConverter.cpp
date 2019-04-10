#include <vector>
#include <iostream>

#include <OpenColorIO/OpenColorIO.h>

#include "transforms/FileTransform.h"
#include "MathUtils.h"
#include "OpBuilders.h"
#include "UnitTestUtils.h"
#include "pystring/pystring.h"

OCIO_NAMESPACE_ENTER
{

    FileConverterRcPtr FileConverter::Create()
    {
        return FileConverterRcPtr(new FileConverter(), &deleter);
    }

    void FileConverter::deleter(FileConverter* c)
    {
        delete c;
    }

    class FileConverter::Impl
    {
    public:

        ConfigRcPtr config_;
        std::string formatName_;
        std::string metadata_;
        std::string inputFile_;

        Impl()
        {
        }

        ~Impl()
        {
        }

        Impl& operator= (const Impl & rhs)
        {
            if (this != &rhs)
            {
                config_ = rhs.config_;
                formatName_ = rhs.formatName_;
            }
            return *this;
        }
    };

    FileConverter::FileConverter()
    : m_impl(new FileConverter::Impl)
    {
    }

    FileConverter::~FileConverter()
    {
        delete m_impl;
        m_impl = NULL;
    }

    FileConverterRcPtr FileConverter::createEditableCopy() const
    {
        FileConverterRcPtr oven = FileConverter::Create();
        *oven->m_impl = *m_impl;
        return oven;
    }

    void FileConverter::setConfig(const ConstConfigRcPtr & config)
    {
        getImpl()->config_ = config->createEditableCopy();
    }

    ConstConfigRcPtr FileConverter::getConfig() const
    {
        return getImpl()->config_;
    }

    int FileConverter::getNumFormats()
    {
        return FormatRegistry::GetInstance().getNumFormats(FORMAT_CAPABILITY_CONVERT);
    }

    const char * FileConverter::getFormatNameByIndex(int index)
    {
        return FormatRegistry::GetInstance().getFormatNameByIndex(FORMAT_CAPABILITY_CONVERT, index);
    }

    const char * FileConverter::getFormatExtensionByIndex(int index)
    {
        return FormatRegistry::GetInstance().getFormatExtensionByIndex(FORMAT_CAPABILITY_CONVERT, index);
    }

    void FileConverter::setInputFile(const char * lutPath)
    {
        getImpl()->inputFile_ = lutPath;
    }

    const char * FileConverter::getInputFile() const
    {
        return getImpl()->inputFile_.c_str();
    }

    void FileConverter::setFormat(const char * formatName)
    {
        getImpl()->formatName_ = formatName;
    }

    const char * FileConverter::getFormat() const
    {
        return getImpl()->formatName_.c_str();
    }

    void FileConverter::setMetadata(const char * metadata)
    {
        getImpl()->metadata_ = metadata;
    }

    const char * FileConverter::getMetadata() const
    {
        return getImpl()->metadata_.c_str();
    }

    void FileConverter::convert(std::ostream & os) const
    {
        FileFormat* fmt = FormatRegistry::GetInstance().getFileFormatByName(getImpl()->formatName_);

        if(!fmt)
        {
            std::ostringstream err;
            err << "The format named '" << getImpl()->formatName_;
            err << "' could not be found. ";
            throw Exception(err.str().c_str());
        }

        FormatInfoVec formatInfoVec;
        fmt->GetFormatInfo(formatInfoVec);

        for(unsigned int i=0; i<formatInfoVec.size(); ++i)
        {
            if(!(formatInfoVec[i].capabilities & FORMAT_CAPABILITY_CONVERT))
            {
                std::ostringstream os;
                os << "The format named " << getImpl()->formatName_;
                os << " does not support convertion.";
                throw Exception(os.str().c_str());
            }
        }

        try
        {
            // Extract Ops from input LUT
            OpRcPtrVec ops;

            ContextRcPtr pContext = Context::Create();
            FileTransformRcPtr ft = FileTransform::Create();
            ft->setSrc(getImpl()->inputFile_.c_str());
            ft->setInterpolation(INTERP_BEST);

            BuildFileOps(ops, *getImpl()->config_, pContext, *ft, TRANSFORM_DIR_FORWARD);
            fmt->Write(ops, getImpl()->formatName_, os);
        }
        catch(std::exception & e)
        {
            std::ostringstream err;
            err << "Error converting " << getImpl()->formatName_ << ":";
            err << e.what();
            throw Exception(err.str().c_str());
        }
    }

}
OCIO_NAMESPACE_EXIT

///////////////////////////////////////////////////////////////////////////////

#ifdef OCIO_UNIT_TEST

namespace OCIO = OCIO_NAMESPACE;
#include "unittest.h"

OIIO_ADD_TEST(FileConverter_Unit_Tests, convert)
{
    OCIO::FileConverterRcPtr convert = OCIO::FileConverter::Create();

    const std::string EXPECTED = R"(
        <?xml version="1.0" ?>
        <ProcessList compCLFversion="2.0" id="" name="" xmlns="urn:NATAS:AMPAS:LUT:v2.0">
            <LUT1D inBitDepth="16f" name="input - Shaper" outBitDepth="16f">
                <IndexMap dim="2">-0.5000000000@0 1.5000000000@5</IndexMap>
                <Array dim="6 3">
                    1.0000000000 1.0000000000 1.0000000000
                    0.8000000000 0.8000000000 0.8000000000
                    0.6000000000 0.6000000000 0.6000000000
                    0.4000000000 0.4000000000 0.4000000000
                    0.2000000000 0.2000000000 0.2000000000
                    0.0000000000 0.0000000000 0.0000000000
                </Array>
            </LUT1D>
            <LUT3D inBitDepth="16f" name="input - Cube" outBitDepth="16f">
                <IndexMap dim="2">0.2500000000@0 0.7500000000@2</IndexMap>
                <Description>Comment 1</Description>
                <Description>Comment 2</Description>
                <Array dim="3 3 3 3">
                    1.0000000000 1.0000000000 1.0000000000
                    1.0000000000 1.0000000000 0.5000000000
                    1.0000000000 1.0000000000 0.0000000000
                    1.0000000000 0.5000000000 1.0000000000
                    1.0000000000 0.5000000000 0.5000000000
                    1.0000000000 0.5000000000 0.0000000000
                    1.0000000000 0.0000000000 1.0000000000
                    1.0000000000 0.0000000000 0.5000000000
                    1.0000000000 0.0000000000 0.0000000000
                    0.5000000000 1.0000000000 1.0000000000
                    0.5000000000 1.0000000000 0.5000000000
                    0.5000000000 1.0000000000 0.0000000000
                    0.5000000000 0.5000000000 1.0000000000
                    0.5000000000 0.5000000000 0.5000000000
                    0.5000000000 0.5000000000 0.0000000000
                    0.5000000000 0.0000000000 1.0000000000
                    0.5000000000 0.0000000000 0.5000000000
                    0.5000000000 0.0000000000 0.0000000000
                    0.0000000000 1.0000000000 1.0000000000
                    0.0000000000 1.0000000000 0.5000000000
                    0.0000000000 1.0000000000 0.0000000000
                    0.0000000000 0.5000000000 1.0000000000
                    0.0000000000 0.5000000000 0.5000000000
                    0.0000000000 0.5000000000 0.0000000000
                    0.0000000000 0.0000000000 1.0000000000
                    0.0000000000 0.0000000000 0.5000000000
                    0.0000000000 0.0000000000 0.0000000000
                </Array>
            </LUT3D>
        </ProcessList>
    )";

    OCIO::ConstConfigRcPtr config;
    OIIO_CHECK_NO_THROW(config = OCIO::Config::Create());
    convert->setConfig(config);
    convert->setFormat("Academy/ASC Common LUT Format");
    OIIO_CHECK_EQUAL("Academy/ASC Common LUT Format", std::string(convert->getFormat()));
    convert->setMetadata("this is some metadata!");
    OIIO_CHECK_EQUAL("this is some metadata!", std::string(convert->getMetadata()));
    std::string lutPath = std::string(OCIO::getTestFilesDir()) + "/" + "resolve_cube_1d3d.cube";
    convert->setInputFile(lutPath.c_str());
    OIIO_CHECK_EQUAL(lutPath.c_str(), std::string(convert->getInputFile()));

    std::ostringstream os;
    convert->convert(os);
    OIIO_CHECK_EQUAL(EXPECTED, os.str());
    OIIO_CHECK_EQUAL(1, convert->getNumFormats());
}

#endif // OCIO_BUILD_TESTS


