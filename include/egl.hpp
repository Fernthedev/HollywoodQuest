#pragma once

#include <EGL/egl.h>
#include <EGL/eglplatform.h>
#include <GLES3/gl3.h>

#ifdef GL_DEBUG
#define EGL_BOOL_CHECK(action, msg) \
    if (!action) logger.error(msg " egl error {}", Hollywood::eglGetErrorString())
#define GL_ERR_CHECK(msg) \
    if (int e = glGetError()) logger.error(msg " gl error {}", e)
#else
#define EGL_BOOL_CHECK(action, msg) action
#define GL_ERR_CHECK(msg)
#endif

namespace Hollywood {
    struct EGLState {
        EGLDisplay display;
        EGLSurface draw;
        EGLSurface read;
        EGLContext context;

        EGLState();
        ~EGLState();
    };

    void InitEGL();
    EGLSurface MakeSurface(ANativeWindow* window);
    void MakeCurrent(EGLSurface surface);
    void SwapBuffers(EGLSurface surface);
    void DestroySurface(EGLSurface surface);

#ifdef GL_DEBUG
    char const* eglGetErrorString();
    void eglLogConfig(EGLDisplay display, EGLConfig config);
    void glMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, GLchar const* msg, void const* data);
#endif
}
