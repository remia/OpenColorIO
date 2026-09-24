# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.
#
# Build Imath
#
# Global targets defined by the Imath CMake configuration files:
#   Imath::Imath
#   Imath::ImathConfig
#

set(_Imath_URL_HASH_3.2.1 "SHA256=b2c8a44c3e4695b74e9644c76f5f5480767355c6f98cde58ba0e82b4ad8c63ce")

set(_Imath_VERSION ${OCIO_Imath_RECOMMENDED_VERSION})

ocio_build_dependency(Imath
    VERSION     ${_Imath_VERSION}
    URL         "https://github.com/AcademySoftwareFoundation/Imath/archive/refs/tags/v${_Imath_VERSION}.tar.gz"
    URL_HASH    "${_Imath_URL_HASH_${_Imath_VERSION}}"
    CMAKE_ARGS
        -DBUILD_TESTING=OFF
        -DPYTHON=OFF
        -DDOCS=OFF
        -DIMATH_HALF_USE_LOOKUP_TABLE=OFF
)

ocio_find_built_dependency(Imath ${_Imath_VERSION} EXACT)
