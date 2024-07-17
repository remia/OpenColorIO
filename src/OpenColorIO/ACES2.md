ACES2
=====

Usage
-----

To build OCIO 2.4dev

    # Add glew to opencolorio builder's deps
    bob build-build-world opencolorio --ctx=platform-pipe6-gcc9-1 -w
    /builds/world/b401024ed1

To use gperftool

    # Add gperftools to openecolorio builder's build deps
    bob build-build-world opencolorio --ctx=platform-pipe6-gcc9-1 -w

    /builds/devtoolset/9.1/98d8da3a82/bin/c++ -DOpenColorIO_EXPORTS -I/u/reac/dev/OpenColorIO/src/libutils -I/u/reac/dev/OpenColorIO/include/OpenColorIO/.. -I/u/reac/dev/OpenColorIO/include/OpenColorIO -I/u/reac/dev/OpenColorIO/src/OpenColorIO -I/u/reac/dev/OpenColorIO/build/include/OpenColorIO -I/u/reac/dev/OpenColorIO/build/src/OpenColorIO -I/u/reac/dev/OpenColorIO/build/generated_include -isystem /builds/expat/2.4.9/89abf7dfef/include -isystem /builds/imath/3.1.9/0f31c2d7ce/include -isystem /builds/imath/3.1.9/0f31c2d7ce/include/Imath -isystem /builds/pystring/1.1.3/bc3fa88746/include/pystring -isystem /u/reac/dev/OpenColorIO/ext/sampleicc/src/include -isystem /u/reac/dev/OpenColorIO/src/utils/.. -isystem /u/reac/dev/OpenColorIO/ext/xxHash/src/include -isystem /builds/yaml_cpp/0.7.0/cfb81c8313/include -isystem /builds/minizip_ng/3.0.10/8ca22624a7/include -O3 -DNDEBUG -g -std=c++14 -fPIC -fvisibility=hidden -fvisibility-inlines-hidden -DUSE_GCC -fconstexpr-ops-limit=1000000000 -Wall -Wextra -Wswitch-enum /u/reac/dev/OpenColorIO/src/OpenColorIO/ops/fixedfunction/aces2bench.cpp -L/u/reac/dev/OpenColorIO/build/myinstall/lib64 -lOpenColorIOimageioapphelpers -lOpenColorIO -lprofiler -L/builds/openexr/3.1.11/3768154b6d/lib64 -lOpenEXR-3_1 -L/builds/imath/3.1.9/0f31c2d7ce/lib64 -lImath -Wl,-rpath,/u/reac/dev/OpenColorIO/build/myinstall/lib64 -o aces2bench

    CPUPROFILE=prof.data CPUPROFILE_FREQUENCY=1000 ./aces2bench
    pprof --pdf ./aces2bench prof.data > prof.pdf


TODO List
---------

* Optimize CPU computation (remove unecessarry steps, merge, use OCIO ops like Matrix)
* Remove constexpr (protability issues, C++14 not modern enough for our use case?)
* Add creative white point / surround gamma adjustments
* Add GPU shader generation


Performance number
------------------

AMD EPYC 75F3 32-Core Processor (Centos7)

2048 x 1556 image (Marcie)
    CPU ACES2 ~ 0.4s
    CPU ACES2 ~ 3.8s

    CPU ACES2 - CHROMA_CURVE ~ 4.2s


NVIDIA A40-12Q (Centos7)

1920x1047 viewport  (Marcie)
    GPU ACES1 ~ 0.480256ms for 10 iterations or ~ 0.0480256ms

    GPU ACES2 ~ 0.135168ms
    GPU ACES2 ~ 1.14ms for 10 iterrations or ~ 0.14ms

    GPU ACES2 ~ 2.8 times slower

3840x2007 viewport  (Marcie)
    GPU ACES2 - const buffer arrays ~ 67ms for 10 iterations 4K or 6.7ms
    GPU ACES2 - const buffer arrays and no arrays argument passing to functions ~ 51ms for 10 iterations 4K or 5.1ms
    GPU ACES2 - ssbo ~ 4.5ms for 10 iterations 4K or 0.45ms
    GPU ACES2 - ubo (std430) ~ 41ms for 10 iterations 4K or 4.1ms
    GPU ACES2 - ubo (packed) ~ 41ms for 10 iterations 4K or 4.2ms
    GPU ACES2 - ubo (std140) float array padded to 4 floats ~ 52ms for 10 iterations 4K or 5.2ms

    GPU ACES2 - USE_SSBO ~ 4.4ms for 10 iterations 4K
    GPU ACES2 - USE_SSBO + CHROMA_CURVE ~ 3.95ms for 10 iterations 4K
    GPU ACES2 - USE_TEXTURE ~ 6.5ms for 10 iterations 4K
    GPU ACES2 - USE_TEXTURE + CHROMA_CURVE ~ 4.8ms for 10 iterations 4K

1920x1079 viewport  (Marcie)

    GPU ACES1 - DEFAULT - 0.5ms for 10 iterations

    GPU ACES2 - CONST BUF - 18ms for 10 iterations
    GPU ACES2 - USE_TEXTURE - 1.85ms for 10 iterations
    GPU ACES2 - USE_UBO - 14.5ms for 10 iterations
    GPU ACES2 - USE_SSBO - 1.21ms for 10 iterations


AMD Radeon Pro 560X (macOS 14.5)

1920x1147 viewport  (Marcie)

    GPU ACES1 - USE DEFAULT - 12.3ms for 10 iterations
    GPU ACES2 - USE TEXTURE - 32.3357ms for 10 iterations
    GPU ACES2 - USE TEXTURE + CHROMA_CURVE - 22.8ms for 10 iterations

    GPU ACES2 - USE NON CONST ARRAYS - 19.8ms for 10 iterations
    GPU ACES2 - USE NON CONST ARRAYS + CHROMA_CURVE - 17.9ms for 10 iterations


Remarks
-------

OpenGL ES 2.0 GLSL version doesn't support const array officially in the spec, individual implementations might
Other shading languages might not support large arrays altogether, would need to be tested thoroughly

GLSL on mac OS do not work correctly with const arrays but is fine (and faster than textures) with non-const arrays
GLSL on Linux do not work with non-const arrays but is fine with const arrays

Use of chroma curve generally speed up GPU implementation by 10% (up to 30% on macOS with texture, but the AMD card seem to have trouble with texture bandwith?)
On CPU, with the naive implementation, it's reversed and 10% slower, didn't investigate thoroughly why but probably the additional arithmetic operations vs the lookup
