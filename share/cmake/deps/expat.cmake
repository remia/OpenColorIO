# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.
#
# Build expat
#
# Global targets defined by the expat CMake configuration files:
#   expat::expat
#

set(_expat_URL_HASH_2.7.2 "SHA256=21b778b34ec837c2ac285aef340f9fb5fa063a811b21ea4d2412a9702c88995c")

set(_expat_VERSION ${OCIO_expat_RECOMMENDED_VERSION})
string(REPLACE "." "_" _expat_TAG "R_${_expat_VERSION}")

set(_expat_CMAKE_ARGS
    -DEXPAT_BUILD_DOCS=OFF
    -DEXPAT_BUILD_EXAMPLES=OFF
    -DEXPAT_BUILD_TESTS=OFF
    -DEXPAT_BUILD_TOOLS=OFF
    -DEXPAT_SHARED_LIBS=OFF
)

# expat ignores CMAKE_DEBUG_POSTFIX and only uses a Debug postfix on Windows. With multi-config
# generators, the Debug and Release libraries must not collide.
get_property(_expat_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_expat_IS_MULTI_CONFIG AND NOT WIN32)
    list(APPEND _expat_CMAKE_ARGS -DEXPAT_DEBUG_POSTFIX=d)
endif()

ocio_build_dependency(expat
    VERSION     ${_expat_VERSION}
    URL         "https://github.com/libexpat/libexpat/releases/download/${_expat_TAG}/expat-${_expat_VERSION}.tar.xz"
    URL_HASH    "${_expat_URL_HASH_${_expat_VERSION}}"
    CMAKE_ARGS  ${_expat_CMAKE_ARGS}
)

ocio_find_built_dependency(expat ${_expat_VERSION} EXACT)
