// GL's functions through SDL, which asks EGL (the Switch's Mesa), GLX or WGL for them.

#include <SDL3/SDL.h>

#include "gl.hpp"

namespace gfx::detail {

namespace glapi {
#define KZ_GL_DEFINE(type, name) type name = nullptr;
KZ_GL_FUNCTIONS(KZ_GL_DEFINE)
KZ_GL_OPTIONAL_FUNCTIONS(KZ_GL_DEFINE)
#undef KZ_GL_DEFINE
} // namespace glapi

const char *LoadGlFunctions() {
#define KZ_GL_LOAD(type, name)                                                                                         \
    glapi::name = reinterpret_cast<type>(SDL_GL_GetProcAddress(#name));                                                \
    if (glapi::name == nullptr) {                                                                                      \
        return #name;                                                                                                  \
    }
    KZ_GL_FUNCTIONS(KZ_GL_LOAD)
#undef KZ_GL_LOAD
#define KZ_GL_LOAD_OPTIONAL(type, name) glapi::name = reinterpret_cast<type>(SDL_GL_GetProcAddress(#name));
    KZ_GL_OPTIONAL_FUNCTIONS(KZ_GL_LOAD_OPTIONAL)
#undef KZ_GL_LOAD_OPTIONAL
    return nullptr;
}

} // namespace gfx::detail
