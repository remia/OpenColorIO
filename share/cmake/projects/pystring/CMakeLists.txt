# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.
#
# CMake project building pystring, as its own CMakeLists.txt doesn't install the headers nor
# CMake configuration files.
#
#   cmake -S share/cmake/projects/pystring -DOCIO_DEP_SOURCE_DIR=<pystring sources> ...
#
# Global targets defined by the installed CMake configuration files:
#   pystring::pystring
#

cmake_minimum_required(VERSION 3.14)

if(NOT OCIO_DEP_SOURCE_DIR)
    message(FATAL_ERROR "OCIO_DEP_SOURCE_DIR must point to the pystring sources.")
endif()

if(OCIO_DEP_VERSION)
    set(_pystring_VERSION_ARGS VERSION ${OCIO_DEP_VERSION})
endif()

project(pystring ${_pystring_VERSION_ARGS} LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(pystring STATIC
    ${OCIO_DEP_SOURCE_DIR}/pystring.cpp
)

set_target_properties(pystring PROPERTIES
    PUBLIC_HEADER "${OCIO_DEP_SOURCE_DIR}/pystring.h"
)

target_include_directories(pystring
    PUBLIC
        $<BUILD_INTERFACE:${OCIO_DEP_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/pystring>
)

set(_pystring_CONFIG_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/pystring")

install(TARGETS pystring
    EXPORT pystringTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pystring
)

# The exported targets file is a complete configuration file as pystring has no dependencies.
install(EXPORT pystringTargets
    NAMESPACE pystring::
    FILE pystringConfig.cmake
    DESTINATION ${_pystring_CONFIG_DIR}
)

if(PROJECT_VERSION)
    write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/pystringConfigVersion.cmake"
        COMPATIBILITY SameMajorVersion
    )
    install(FILES "${PROJECT_BINARY_DIR}/pystringConfigVersion.cmake"
        DESTINATION ${_pystring_CONFIG_DIR}
    )
endif()
