# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.
#
# Build OpenEXR
#
# Global targets defined by the OpenEXR CMake configuration files:
#   OpenEXR::OpenEXR
#   OpenEXR::OpenEXRCore
#   OpenEXR::OpenEXRUtil
#   OpenEXR::Iex
#   OpenEXR::IlmThread
#   (and their *Config interface targets)
#
# Depending on user options when configuring OCIO, OpenEXR can be used to
# build libOpenColorIOimageioapphelpers.
#

set(_OpenEXR_URL_HASH_3.4.0 "SHA256=d7b31637d7adc359f5e5a7517ba918cb5997bc1a4ae7a808ec874cdf91da93c0")

set(_OpenEXR_VERSION ${OCIO_OpenEXR_RECOMMENDED_VERSION})

set(_OpenEXR_CMAKE_ARGS
    -DBUILD_TESTING=OFF
    -DOPENEXR_BUILD_EXAMPLES=OFF
    -DOPENEXR_BUILD_TOOLS=OFF
    -DOPENEXR_INSTALL_PKG_CONFIG=OFF
    -DOPENEXR_FORCE_INTERNAL_DEFLATE=ON
    -DOPENEXR_FORCE_INTERNAL_OPENJPH=ON
    # OpenEXR 3.4.0 would use OpenJPH 0.22.0 by default, request the latest version at release
    # time including build fixes.
    -DOPENEXR_OPENJPH_TAG=0.23.1
)

# Build against the Imath used by OCIO, otherwise OpenEXR would download its own copy, which might
# result in version conflicts. An Imath built by OCIO is found through CMAKE_PREFIX_PATH.
if(Imath_DIR)
    list(APPEND _OpenEXR_CMAKE_ARGS -DImath_DIR=${Imath_DIR})
endif()

ocio_build_dependency(OpenEXR
    VERSION     ${_OpenEXR_VERSION}
    URL         "https://github.com/AcademySoftwareFoundation/openexr/archive/refs/tags/v${_OpenEXR_VERSION}.tar.gz"
    URL_HASH    "${_OpenEXR_URL_HASH_${_OpenEXR_VERSION}}"
    CMAKE_ARGS  ${_OpenEXR_CMAKE_ARGS}
)

ocio_find_built_dependency(OpenEXR ${_OpenEXR_VERSION} EXACT)
