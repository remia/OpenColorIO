# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.
#
# Build minizip-ng
#
# Global targets defined by the minizip-ng CMake configuration files:
#   MINIZIP::minizip-ng
#
# Link against the following target:
#   ZLIB::ZLIB
#

set(_minizip-ng_URL_HASH_4.0.10 "SHA256=c362e35ee973fa7be58cc5e38a4a6c23cc8f7e652555daf4f115a9eb2d3a6be7")

set(_minizip-ng_VERSION ${OCIO_minizip-ng_RECOMMENDED_VERSION})

set(_minizip-ng_CMAKE_ARGS
    -DMZ_OPENSSL=OFF
    -DMZ_LIBBSD=OFF
    -DMZ_BUILD_TESTS=OFF
    -DMZ_COMPAT=OFF
    -DMZ_BZIP2=OFF
    -DMZ_LZMA=OFF
    -DMZ_LIBCOMP=OFF
    -DMZ_ZSTD=OFF
    -DMZ_PKCRYPT=OFF
    -DMZ_WZAES=OFF
    -DMZ_SIGNING=OFF
    -DMZ_ZLIB=ON
    -DMZ_ICONV=OFF
    -DMZ_FETCH_LIBS=OFF
    -DMZ_FORCE_FETCH_LIBS=OFF
    -DCMAKE_DISABLE_FIND_PACKAGE_ZLIBNG=ON
)

# Build against the ZLIB used by OCIO (minizip-ng records the library path in its exported
# targets). Pass the per-configuration libraries to support multi-config generators.
get_target_property(_minizip-ng_ZLIB_INCLUDE_DIRS ZLIB::ZLIB INTERFACE_INCLUDE_DIRECTORIES)
list(GET _minizip-ng_ZLIB_INCLUDE_DIRS 0 _minizip-ng_ZLIB_INCLUDE_DIR)
list(APPEND _minizip-ng_CMAKE_ARGS -DZLIB_INCLUDE_DIR=${_minizip-ng_ZLIB_INCLUDE_DIR})

get_target_property(_minizip-ng_ZLIB_LOCATION ZLIB::ZLIB IMPORTED_LOCATION)
if(_minizip-ng_ZLIB_LOCATION)
    list(APPEND _minizip-ng_CMAKE_ARGS -DZLIB_LIBRARY=${_minizip-ng_ZLIB_LOCATION})
endif()
foreach(_minizip-ng_CONFIG RELEASE DEBUG)
    get_target_property(_minizip-ng_ZLIB_LOCATION ZLIB::ZLIB IMPORTED_LOCATION_${_minizip-ng_CONFIG})
    if(_minizip-ng_ZLIB_LOCATION)
        list(APPEND _minizip-ng_CMAKE_ARGS
            -DZLIB_LIBRARY_${_minizip-ng_CONFIG}=${_minizip-ng_ZLIB_LOCATION})
    endif()
endforeach()

ocio_build_dependency(minizip-ng
    VERSION     ${_minizip-ng_VERSION}
    URL         "https://github.com/zlib-ng/minizip-ng/archive/refs/tags/${_minizip-ng_VERSION}.tar.gz"
    URL_HASH    "${_minizip-ng_URL_HASH_${_minizip-ng_VERSION}}"
    CMAKE_ARGS  ${_minizip-ng_CMAKE_ARGS}
)

ocio_find_built_dependency(minizip-ng ${_minizip-ng_VERSION} EXACT)
