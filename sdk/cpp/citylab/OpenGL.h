#pragma once

// One include for classic (fixed-function) OpenGL on Windows, macOS and Linux.
//
// CityLab uses only the OpenGL 1.x API taught in the course: glBegin/glEnd,
// the matrix stack, classic lighting, materials, textures and display lists.
// Buffer objects, vertex-array objects and shaders are intentionally not used.
#if defined(_WIN32)
    // Windows' OpenGL header expects a few Windows types to exist first.
    // NOMINMAX keeps windows.h from defining min/max macros that would clash
    // with std::min and std::max.
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    // windows.h also defines `near` and `far` as empty macros (a leftover
    // from 16-bit Windows) that would break ordinary variable names.
    #undef near
    #undef far
    #include <GL/gl.h>
#elif defined(__APPLE__)
    // Apple still ships classic OpenGL but marks it deprecated; this keeps the
    // build output readable.
    #ifndef GL_SILENCE_DEPRECATION
        #define GL_SILENCE_DEPRECATION
    #endif
    #include <OpenGL/gl.h>
#else
    #include <GL/gl.h>
#endif

// Constants that some platform headers (notably Windows' gl.h, which stops at
// OpenGL 1.1) do not define. Their values are fixed by the OpenGL
// specification, so defining them here is safe everywhere.
#ifndef GL_CLAMP_TO_EDGE
    #define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_RESCALE_NORMAL
    #define GL_RESCALE_NORMAL 0x803A
#endif
#ifndef GL_TEXTURE_MAX_LEVEL
    #define GL_TEXTURE_MAX_LEVEL 0x813D
#endif
#ifndef GL_MULTISAMPLE
    #define GL_MULTISAMPLE 0x809D
#endif
#ifndef GL_LIGHT_MODEL_COLOR_CONTROL
    #define GL_LIGHT_MODEL_COLOR_CONTROL 0x81F8
    #define GL_SEPARATE_SPECULAR_COLOR 0x81FA
    #define GL_SINGLE_COLOR 0x81F9
#endif
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
    #define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
    #define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif
