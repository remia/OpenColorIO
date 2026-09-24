# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.
#
# Install openfx headers
#
# Global targets defined by the openfx CMake configuration files:
#   openfx::module
#
# Variables defined by this module:
#   openfx_INCLUDE_DIR      - Location of the header files
#

set(_openfx_URL_HASH_1.5 "SHA256=553a8e2c24eabd263e6ece8d85fd08650c9601b689dd8c16dfa0c5895bb078d3")

set(_openfx_VERSION ${OCIO_openfx_RECOMMENDED_VERSION})

# The openfx CMakeLists.txt requires Conan provided dependencies, OCIO only needs the headers.
ocio_build_dependency(openfx
    VERSION     ${_openfx_VERSION}
    URL         "https://github.com/AcademySoftwareFoundation/openfx/archive/refs/tags/OFX_Release_${_openfx_VERSION}.tar.gz"
    URL_HASH    "${_openfx_URL_HASH_${_openfx_VERSION}}"
    PROJECT_DIR "${PROJECT_SOURCE_DIR}/share/cmake/projects/openfx"
)

ocio_find_built_dependency(openfx ${_openfx_VERSION} EXACT)

get_target_property(openfx_INCLUDE_DIR openfx::module INTERFACE_INCLUDE_DIRECTORIES)
