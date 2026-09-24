# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.
#
# Build pybind11
#
# Global targets defined by the pybind11 CMake configuration files:
#   pybind11::module
#   pybind11::headers
#
# Variables defined by the pybind11 CMake configuration files:
#   pybind11_INCLUDE_DIR
#

set(_pybind11_URL_HASH_3.0.1 "SHA256=741633da746b7c738bb71f1854f957b9da660bcd2dce68d71949037f0969d0ca")

set(_pybind11_VERSION ${OCIO_pybind11_RECOMMENDED_VERSION})

# pybind11 is header only, there is no need to search for Python when installing it. OCIO's own
# Python search is used when loading the pybind11 CMake configuration files.
ocio_build_dependency(pybind11
    VERSION     ${_pybind11_VERSION}
    URL         "https://github.com/pybind/pybind11/archive/refs/tags/v${_pybind11_VERSION}.tar.gz"
    URL_HASH    "${_pybind11_URL_HASH_${_pybind11_VERSION}}"
    CMAKE_ARGS
        -DPYBIND11_INSTALL=ON
        -DPYBIND11_TEST=OFF
        -DPYBIND11_NOPYTHON=ON
)

ocio_find_built_dependency(pybind11 ${_pybind11_VERSION} EXACT)
