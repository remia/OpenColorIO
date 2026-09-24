# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.
#
# Build lcms2
#
# Global targets defined by the lcms2 CMake configuration files:
#   lcms2::lcms2
#

set(_lcms2_URL_HASH_2.17 "SHA256=6e6f6411db50e85ae8ff7777f01b2da0614aac13b7b9fcbea66dc56a1bc71418")

set(_lcms2_VERSION ${OCIO_lcms2_RECOMMENDED_VERSION})

# Little-CMS only provides Autotools and Meson builds.
ocio_build_dependency(lcms2
    VERSION     ${_lcms2_VERSION}
    URL         "https://github.com/mm2/Little-CMS/archive/refs/tags/lcms${_lcms2_VERSION}.tar.gz"
    URL_HASH    "${_lcms2_URL_HASH_${_lcms2_VERSION}}"
    PROJECT_DIR "${PROJECT_SOURCE_DIR}/share/cmake/projects/lcms2"
)

ocio_find_built_dependency(lcms2 ${_lcms2_VERSION} EXACT)
