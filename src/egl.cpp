#include "egl.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl32.h>

#include "main.hpp"

Hollywood::EGLState::EGLState() {
    display = eglGetCurrentDisplay();
    draw = eglGetCurrentSurface(EGL_DRAW);
    read = eglGetCurrentSurface(EGL_READ);
    context = eglGetCurrentContext();
}

Hollywood::EGLState::~EGLState() {
    if (!display || !context)
        return;
    EGL_BOOL_CHECK(eglMakeCurrent(display, draw, read, context), "restore egl");
}

// clang-format off
// if you change these, it will probably break
static EGLint const attribs[] = {
    EGL_RED_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
    EGL_RECORDABLE_ANDROID, 1,
    EGL_NONE
};
// clang-format on

static bool init = false;
static EGLDisplay display;
static EGLConfig config;
static EGLContext context;

void Hollywood::InitEGL() {
    if (init)
        return;

    logger.info("Initializing EGL render thread globals");

    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGL_BOOL_CHECK(eglInitialize(display, 0, 0), "initialize");

    logger.debug("EGL version {}", eglQueryString(display, EGL_VERSION));

    context = eglGetCurrentContext();

    EGLint numConfigs;
    EGL_BOOL_CHECK(eglChooseConfig(display, attribs, &config, 1, &numConfigs), "choose config");

#ifdef GL_DEBUG
    eglLogConfig(display, config);

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(glMessageCallback, 0);
    GL_ERR_CHECK("callback");
#endif

    init = true;
}

EGLSurface Hollywood::MakeSurface(ANativeWindow* window) {
    InitEGL();

    return eglCreateWindowSurface(display, config, window, 0);
}

void Hollywood::MakeCurrent(EGLSurface surface) {
    EGL_BOOL_CHECK(eglMakeCurrent(display, surface, surface, context), "make current");
}

void Hollywood::SwapBuffers(EGLSurface surface) {
    EGL_BOOL_CHECK(eglSwapBuffers(display, surface), "swap buffers");
}

void Hollywood::DestroySurface(EGLSurface surface) {
    EGL_BOOL_CHECK(eglDestroySurface(display, surface), "destroy surface");
}

#ifdef GL_DEBUG
#define CASE_STR(value) \
    case value: return #value

char const* Hollywood::eglGetErrorString() {
    switch (eglGetError()) {
        CASE_STR(EGL_SUCCESS);
        CASE_STR(EGL_NOT_INITIALIZED);
        CASE_STR(EGL_BAD_ACCESS);
        CASE_STR(EGL_BAD_ALLOC);
        CASE_STR(EGL_BAD_ATTRIBUTE);
        CASE_STR(EGL_BAD_CONTEXT);
        CASE_STR(EGL_BAD_CONFIG);
        CASE_STR(EGL_BAD_CURRENT_SURFACE);
        CASE_STR(EGL_BAD_DISPLAY);
        CASE_STR(EGL_BAD_SURFACE);
        CASE_STR(EGL_BAD_MATCH);
        CASE_STR(EGL_BAD_PARAMETER);
        CASE_STR(EGL_BAD_NATIVE_PIXMAP);
        CASE_STR(EGL_BAD_NATIVE_WINDOW);
        CASE_STR(EGL_CONTEXT_LOST);
        default:
            return "Unknown";
    }
}
#undef CASE_STR

#define LOG_ATTR(attr) \
    eglGetConfigAttrib(display, config, attr, &value); \
    logger.debug(#attr ": {}", value);

void Hollywood::eglLogConfig(EGLDisplay display, EGLConfig config) {
    EGLint value;
    logger.debug("--- logging egl config ---");
    LOG_ATTR(EGL_CONFIG_ID);
    LOG_ATTR(EGL_COLOR_BUFFER_TYPE);
    LOG_ATTR(EGL_BUFFER_SIZE);
    LOG_ATTR(EGL_RED_SIZE);
    LOG_ATTR(EGL_GREEN_SIZE);
    LOG_ATTR(EGL_BLUE_SIZE);
    LOG_ATTR(EGL_ALPHA_SIZE);
    LOG_ATTR(EGL_DEPTH_SIZE);
    LOG_ATTR(EGL_STENCIL_SIZE);
    LOG_ATTR(EGL_LUMINANCE_SIZE);
    LOG_ATTR(EGL_ALPHA_MASK_SIZE);
    LOG_ATTR(EGL_SAMPLE_BUFFERS);
    LOG_ATTR(EGL_SAMPLES);
    LOG_ATTR(EGL_MAX_PBUFFER_WIDTH);
    LOG_ATTR(EGL_MAX_PBUFFER_HEIGHT);
    LOG_ATTR(EGL_MAX_PBUFFER_PIXELS);
    LOG_ATTR(EGL_MAX_SWAP_INTERVAL);
    LOG_ATTR(EGL_MIN_SWAP_INTERVAL);
    LOG_ATTR(EGL_NATIVE_RENDERABLE);
    LOG_ATTR(EGL_NATIVE_VISUAL_ID);
    LOG_ATTR(EGL_NATIVE_VISUAL_TYPE);
    LOG_ATTR(EGL_SURFACE_TYPE);
    LOG_ATTR(EGL_TRANSPARENT_TYPE);
}

#define CASE_STR(value) \
    case GL_DEBUG_SOURCE_##value: return #value

static char const* glSourceString(GLenum source) {
    switch (source) {
        CASE_STR(API);
        CASE_STR(WINDOW_SYSTEM);
        CASE_STR(SHADER_COMPILER);
        CASE_STR(THIRD_PARTY);
        CASE_STR(APPLICATION);
        default:
            return "UNKNOWN";
    }
}
#undef CASE_STR

#define CASE_STR(value) \
    case GL_DEBUG_TYPE_##value: return #value

static char const* glTypeString(GLenum type) {
    switch (type) {
        CASE_STR(ERROR);
        CASE_STR(DEPRECATED_BEHAVIOR);
        CASE_STR(UNDEFINED_BEHAVIOR);
        CASE_STR(PORTABILITY);
        CASE_STR(PERFORMANCE);
        CASE_STR(OTHER);
        CASE_STR(MARKER);
        default:
            return "UNKNOWN";
    }
}
#undef CASE_STR

#define CASE_STR(value) \
    case GL_DEBUG_SEVERITY_##value: return #value

static char const* glSeverityString(GLenum severity) {
    switch (severity) {
        CASE_STR(HIGH);
        CASE_STR(MEDIUM);
        CASE_STR(LOW);
        CASE_STR(NOTIFICATION);
        default:
            return "UNKNOWN";
    }
}
#undef CASE_STR

void Hollywood::glMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, GLchar const* msg, void const* data) {
    logger.debug("{}: {} of {} severity, raised from {}: {}", id, glTypeString(type), glSeverityString(severity), glSourceString(source), msg);
}
#endif
