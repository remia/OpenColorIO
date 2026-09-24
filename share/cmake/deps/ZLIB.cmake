# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.
#
# Build ZLIB
#
# Global targets defined by CMake's FindZLIB module:
#   ZLIB::ZLIB
#

set(_ZLIB_URL_HASH_1.3.1 "SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23")

set(_ZLIB_VERSION ${OCIO_ZLIB_RECOMMENDED_VERSION})

ocio_build_dependency(ZLIB
    VERSION     ${_ZLIB_VERSION}
    URL         "https://github.com/madler/zlib/releases/download/v${_ZLIB_VERSION}/zlib-${_ZLIB_VERSION}.tar.gz"
    URL_HASH    "${_ZLIB_URL_HASH_${_ZLIB_VERSION}}"
    CMAKE_ARGS
        # ZLIB requires an old CMake version that has no knowledge of this policy.
        -DCMAKE_POLICY_DEFAULT_CMP0042=NEW
        -DZLIB_BUILD_EXAMPLES=OFF
)

# ZLIB 1.3.1 always installs both a shared and a static library. Remove the shared one, so that
# anything searching ZLIB in ${OCIO_EXT_DIST_ROOT} (e.g. minizip-ng, or consumers of a static
# OCIO) uses the static library OCIO links with. The shared library is named zlib on Windows.
foreach(_ZLIB_NAME z zd zlib zlibd)
    file(GLOB _ZLIB_SHARED_FILES
        "${OCIO_EXT_DIST_ROOT}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}${_ZLIB_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}*"
        "${OCIO_EXT_DIST_ROOT}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}${_ZLIB_NAME}.*${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "${OCIO_EXT_DIST_ROOT}/bin/${CMAKE_SHARED_LIBRARY_PREFIX}${_ZLIB_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    if(WIN32)
        # Import library of the DLL.
        list(APPEND _ZLIB_SHARED_FILES
            "${OCIO_EXT_DIST_ROOT}/lib/${CMAKE_IMPORT_LIBRARY_PREFIX}${_ZLIB_NAME}${CMAKE_IMPORT_LIBRARY_SUFFIX}")
    endif()
    if(_ZLIB_SHARED_FILES)
        file(REMOVE ${_ZLIB_SHARED_FILES})
    endif()
endforeach()

# Discard the results of a previous search, e.g. a system ZLIB rejected for being too old, as
# find_library and find_path don't search again when the cache variables are already set.
foreach(_ZLIB_VAR ZLIB_LIBRARY ZLIB_LIBRARY_RELEASE ZLIB_LIBRARY_DEBUG ZLIB_INCLUDE_DIR)
    unset(${_ZLIB_VAR})
    unset(${_ZLIB_VAR} CACHE)
endforeach()

# ZLIB 1.3.1 doesn't provide CMake configuration files. Point CMake's FindZLIB module to the static
# library, as it would do with ZLIB_USE_STATIC_LIBS (CMake 3.24+). The static library is named
# zlibstatic on Windows.
set(_ZLIB_NAMES z zlibstatic)
foreach(_ZLIB_CONFIG RELEASE DEBUG)
    set(_ZLIB_STATIC_NAMES "")
    foreach(_ZLIB_NAME ${_ZLIB_NAMES})
        list(APPEND _ZLIB_STATIC_NAMES
            "${CMAKE_STATIC_LIBRARY_PREFIX}${_ZLIB_NAME}${CMAKE_STATIC_LIBRARY_SUFFIX}")
    endforeach()

    find_library(ZLIB_LIBRARY_${_ZLIB_CONFIG}
        NAMES ${_ZLIB_STATIC_NAMES}
        PATHS "${OCIO_EXT_DIST_ROOT}/lib"
        NO_DEFAULT_PATH)

    # The Debug library has a "d" suffix.
    list(TRANSFORM _ZLIB_NAMES APPEND "d")
endforeach()

find_path(ZLIB_INCLUDE_DIR
    NAMES zlib.h
    PATHS "${OCIO_EXT_DIST_ROOT}/include"
    NO_DEFAULT_PATH)

find_package(ZLIB ${_ZLIB_VERSION} EXACT MODULE REQUIRED)
