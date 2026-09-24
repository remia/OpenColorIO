# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.
#
# Build pystring
#
# Global targets defined by the pystring CMake configuration files:
#   pystring::pystring
#

set(_pystring_URL_HASH_1.1.4 "SHA256=49da0fe2a049340d3c45cce530df63a2278af936003642330287b68cefd788fb")

set(_pystring_VERSION ${OCIO_pystring_RECOMMENDED_VERSION})

# The pystring CMakeLists.txt doesn't install the headers nor CMake configuration files.
ocio_build_dependency(pystring
    VERSION     ${_pystring_VERSION}
    URL         "https://github.com/imageworks/pystring/archive/refs/tags/v${_pystring_VERSION}.tar.gz"
    URL_HASH    "${_pystring_URL_HASH_${_pystring_VERSION}}"
    PROJECT_DIR "${PROJECT_SOURCE_DIR}/share/cmake/projects/pystring"
)

ocio_find_built_dependency(pystring ${_pystring_VERSION} EXACT)
