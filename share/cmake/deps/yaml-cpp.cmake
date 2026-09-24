# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.
#
# Build yaml-cpp
#
# Global targets defined by the yaml-cpp CMake configuration files:
#   yaml-cpp::yaml-cpp
#

set(_yaml-cpp_URL_HASH_0.8.0 "SHA256=fbe74bbdcee21d656715688706da3c8becfd946d92cd44705cc6098bb23b3a16")

set(_yaml-cpp_CXX_FLAGS "")
if(MSVC)
    string(APPEND _yaml-cpp_CXX_FLAGS " /EHsc")
elseif(USE_CLANG)
    # Remove some global 'shadow' warnings.
    string(APPEND _yaml-cpp_CXX_FLAGS " -Wno-shadow")
endif()
string(STRIP "${_yaml-cpp_CXX_FLAGS}" _yaml-cpp_CXX_FLAGS)

# In v0.8.0 yaml-cpp switched from "yaml-cpp-vA.B.C" to "vA.B.C" format for tags.
ocio_build_dependency(yaml-cpp
    VERSION     ${OCIO_yaml-cpp_RECOMMENDED_VERSION}
    URL         "https://github.com/jbeder/yaml-cpp/archive/refs/tags/${OCIO_yaml-cpp_RECOMMENDED_VERSION}.tar.gz"
    URL_HASH    "${_yaml-cpp_URL_HASH_${OCIO_yaml-cpp_RECOMMENDED_VERSION}}"
    CMAKE_ARGS
        -DCMAKE_CXX_FLAGS=${_yaml-cpp_CXX_FLAGS}
        -DYAML_BUILD_SHARED_LIBS=OFF
        -DYAML_CPP_BUILD_TESTS=OFF
        -DYAML_CPP_BUILD_TOOLS=OFF
        -DYAML_CPP_BUILD_CONTRIB=OFF
)

ocio_find_built_dependency(yaml-cpp ${OCIO_yaml-cpp_RECOMMENDED_VERSION} EXACT)
