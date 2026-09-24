# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.
#
# CMake project installing the openfx headers, as the openfx CMakeLists.txt requires Conan
# provided dependencies to build its examples and support libraries, which OCIO doesn't use.
#
#   cmake -S share/cmake/projects/openfx -DOCIO_DEP_SOURCE_DIR=<openfx sources> ...
#
# Global targets defined by the installed CMake configuration files:
#   openfx::module
#

cmake_minimum_required(VERSION 3.14)

if(NOT OCIO_DEP_SOURCE_DIR)
    message(FATAL_ERROR "OCIO_DEP_SOURCE_DIR must point to the openfx sources.")
endif()

if(OCIO_DEP_VERSION)
    set(_openfx_VERSION_ARGS VERSION ${OCIO_DEP_VERSION})
endif()

project(openfx ${_openfx_VERSION_ARGS} LANGUAGES NONE)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(openfx_module INTERFACE)

set_target_properties(openfx_module PROPERTIES
    EXPORT_NAME module
)

target_include_directories(openfx_module
    INTERFACE
        $<BUILD_INTERFACE:${OCIO_DEP_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/openfx>
)

set(_openfx_CONFIG_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/openfx")

install(TARGETS openfx_module
    EXPORT openfxTargets
)

install(DIRECTORY "${OCIO_DEP_SOURCE_DIR}/include/"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/openfx
)

# The exported targets file is a complete configuration file as openfx has no dependencies.
install(EXPORT openfxTargets
    NAMESPACE openfx::
    FILE openfxConfig.cmake
    DESTINATION ${_openfx_CONFIG_DIR}
)

if(PROJECT_VERSION)
    write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/openfxConfigVersion.cmake"
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT
    )
    install(FILES "${PROJECT_BINARY_DIR}/openfxConfigVersion.cmake"
        DESTINATION ${_openfx_CONFIG_DIR}
    )
endif()
