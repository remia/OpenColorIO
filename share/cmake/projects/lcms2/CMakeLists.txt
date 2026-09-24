# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.
#
# CMake project building lcms2 (Little CMS), as it only provides Autotools and Meson builds.
#
#   cmake -S share/cmake/projects/lcms2 -DOCIO_DEP_SOURCE_DIR=<Little-CMS sources> ...
#
# Global targets defined by the installed CMake configuration files:
#   lcms2::lcms2
#

cmake_minimum_required(VERSION 3.14)

if(NOT OCIO_DEP_SOURCE_DIR)
    message(FATAL_ERROR "OCIO_DEP_SOURCE_DIR must point to the Little-CMS sources.")
endif()

if(OCIO_DEP_VERSION)
    set(_lcms2_VERSION_ARGS VERSION ${OCIO_DEP_VERSION})
endif()

project(lcms2 ${_lcms2_VERSION_ARGS} LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

file(GLOB _lcms2_HEADERS "${OCIO_DEP_SOURCE_DIR}/include/*.h")
file(GLOB _lcms2_SOURCES "${OCIO_DEP_SOURCE_DIR}/src/*.c")

add_library(lcms2 STATIC ${_lcms2_SOURCES})

set_target_properties(lcms2 PROPERTIES
    PUBLIC_HEADER "${_lcms2_HEADERS}"
)

target_include_directories(lcms2
    PUBLIC
        $<BUILD_INTERFACE:${OCIO_DEP_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/lcms2>
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU")
    target_compile_options(lcms2 PRIVATE -Wno-aggressive-loop-optimizations)
endif()

set(_lcms2_CONFIG_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/lcms2")

install(TARGETS lcms2
    EXPORT lcms2Targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lcms2
)

# The exported targets file is a complete configuration file as lcms2 has no dependencies.
install(EXPORT lcms2Targets
    NAMESPACE lcms2::
    FILE lcms2Config.cmake
    DESTINATION ${_lcms2_CONFIG_DIR}
)

if(PROJECT_VERSION)
    write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/lcms2ConfigVersion.cmake"
        COMPATIBILITY SameMajorVersion
    )
    install(FILES "${PROJECT_BINARY_DIR}/lcms2ConfigVersion.cmake"
        DESTINATION ${_lcms2_CONFIG_DIR}
    )
endif()
