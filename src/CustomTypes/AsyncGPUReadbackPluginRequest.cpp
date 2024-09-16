#include "CustomTypes/AsyncGPUReadbackPluginRequest.hpp"

#include <GLES3/gl3.h>
#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>

#include <shared_mutex>
#include <string>
#include <thread>

#include "CustomTypes/TypeHelpers.hpp"
#include "main.hpp"

// Looks like this was from https://github.com/Alabate/AsyncGPUReadbackPlugin

#pragma region ReadPixels OpenGL

#define gl_err_check(id) if (int e = glGetError()) logger.error(id " error {}", e);
// #define gl_err_check(id)

static bool openglDebug = false;

void GLDebugMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, GLchar const* msg, void const* data) {
    std::string _source;
    std::string _type;
    std::string _severity;

    switch (source) {
        case GL_DEBUG_SOURCE_API:
            _source = "API";
            break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
            _source = "WINDOW SYSTEM";
            break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
            _source = "SHADER COMPILER";
            break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:
            _source = "THIRD PARTY";
            break;
        case GL_DEBUG_SOURCE_APPLICATION:
            _source = "APPLICATION";
            break;
        default:
            _source = "UNKNOWN";
            break;
    }

    switch (type) {
        case GL_DEBUG_TYPE_ERROR:
            _type = "ERROR";
            break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            _type = "DEPRECATED BEHAVIOR";
            break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            _type = "UDEFINED BEHAVIOR";
            break;
        case GL_DEBUG_TYPE_PORTABILITY:
            _type = "PORTABILITY";
            break;
        case GL_DEBUG_TYPE_PERFORMANCE:
            _type = "PERFORMANCE";
            return;
            // break;
        case GL_DEBUG_TYPE_OTHER:
            _type = "OTHER";
            break;
        case GL_DEBUG_TYPE_MARKER:
            _type = "MARKER";
            break;
        default:
            _type = "UNKNOWN";
            break;
    }

    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:
            _severity = "HIGH";
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            _severity = "MEDIUM";
            break;
        case GL_DEBUG_SEVERITY_LOW:
            _severity = "LOW";
            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            _severity = "NOTIFICATION";
            return;
            // break;
        default:
            _severity = "UNKNOWN";
            break;
    }

    logger.debug("{}: {} of {} severity, raised from {}: {}", id, _type, _severity, _source, msg);
}

struct Task {
    GLuint origTexture;
    GLuint fbo;
    GLuint pbo;
    GLsync fence;
    bool initialized = false;
    bool error = false;
    bool done = false;
    rgba* data;
    // keep alive
    FramePool::Frame frameRef;
    size_t size;
    int height;
    int width;
};

static std::unordered_map<int, std::shared_ptr<Task>> tasks;
static std::shared_mutex tasks_mutex;
static int next_event_id = 1;

extern "C" int makeRequest_mainThread(GLuint texture, int width, int height, FramePool::Frame const& reference) {
    // Create the task
    std::shared_ptr<Task> task = std::make_shared<Task>();
    task->origTexture = texture;
    task->width = width;
    task->height = height;
    task->frameRef = reference;
    task->data = reference->frameData;
    int event_id = next_event_id++;

    // Save it (lock because possible vector resize)
    std::unique_lock lock(tasks_mutex);
    tasks[event_id] = task;
    lock.unlock();

    return event_id;
}

extern "C" void makeRequest_renderThread(int event_id) {
    if (openglDebug) {
        logger.debug("setting gl debug callback");
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(GLDebugMessageCallback, 0);
        gl_err_check("callback");
        openglDebug = false;
        logger.debug("opengl ver {}", (char*) glGetString(GL_VERSION));
    }
    // Get task back
    std::shared_lock lock(tasks_mutex);
    std::shared_ptr<Task> task = tasks[event_id];
    lock.unlock();

    // The format is GL_UNSIGNED_BYTE which is correct.
    task->size = sizeof(rgba) * task->width * task->height;

    if (task->size == 0) {
        logger.debug("request size error {} {}", task->width, task->height);
        task->error = true;
        return;
    }

    // Start the read request

    // Allocate the final data buffer !!! WARNING: free, will have to be done on script side !!!!
    // task->data = static_cast<rgba*>(std::malloc(task->size));

    // Create the fbo (frame buffer object) from the given texture
    glGenFramebuffers(1, &task->fbo);

    // Bind the texture to the fbo
    glBindFramebuffer(GL_FRAMEBUFFER, task->fbo);
    gl_err_check("created framebuffer");

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, task->origTexture, 0);
    gl_err_check("attached texture to framebuffer");

    // Create and bind pbo (pixel buffer object) to fbo
    glGenBuffers(1, &task->pbo);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, task->pbo);
    gl_err_check("created pixel buffer");

    glBufferData(GL_PIXEL_PACK_BUFFER, task->size, 0, GL_DYNAMIC_READ);
    gl_err_check("created pixel buffer data");

    // Start the read request
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    // since we're assuming the output pixels data type anyway, I don't think there's much point in being dynamic here
    glReadPixels(0, 0, task->width, task->height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    gl_err_check("read pixels");

    // Unbind buffers
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    gl_err_check("unbinds (makeRequest)");

    // Fence to know when it's ready
    task->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    gl_err_check("fence");

    // Done init
    task->initialized = true;
}

extern "C" void update_renderThread(int event_id) {
    // Get task back
    std::shared_lock lock(tasks_mutex);
    std::shared_ptr<Task> task = tasks[event_id];
    lock.unlock();

    // Check if task has not been already deleted by main thread
    if (task == nullptr)
        return;

    // Do something only if initialized (thread safety)
    if (!task->initialized || task->done)
        return;

    // Check fence state
    GLint status = 0;
    GLsizei length = 0;
    glGetSynciv(task->fence, GL_SYNC_STATUS, sizeof(GLint), &length, &status);
    if (length <= 0) {
        logger.debug("request error on update");
        task->error = true;
        task->done = true;
        return;
    }

    // When it's done
    if (status == GL_SIGNALED) {
        // Bind back the pbo
        glBindBuffer(GL_PIXEL_PACK_BUFFER, task->pbo);
        gl_err_check("bind pbo");

        // Map the buffer and copy it to data
        auto* ptr = reinterpret_cast<rgba*>(glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, task->size, GL_MAP_READ_BIT));
        gl_err_check("map range");
        memcpy(task->data, ptr, task->size);

        // Unmap and unbind
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        gl_err_check("unbinds (update)");

        // Clear buffers
        glDeleteFramebuffers(1, &task->fbo);
        glDeleteBuffers(1, &task->pbo);
        glDeleteSync(task->fence);
        gl_err_check("delete");

        // yeah task is done!
        task->done = true;
    }
}

extern "C" void getData_mainThread(int event_id, rgba*& buffer, size_t& length) {
    // Get task back
    std::shared_lock lock(tasks_mutex);
    std::shared_ptr<Task> task = tasks[event_id];
    lock.unlock();

    // Do something only if initialized (thread safety)
    if (!task->done)
        return;

    // Copy the pointer. Warning: free will have to be done on script side
    length = task->size;
    buffer = task->data;
}

extern "C" bool isRequestDone(int event_id) {
    // Get task back
    std::shared_lock lock(tasks_mutex);
    std::shared_ptr<Task> task = tasks[event_id];
    lock.unlock();

    return task->done;
}

extern "C" bool isRequestError(int event_id) {
    // Get task back
    std::shared_lock lock(tasks_mutex);
    std::shared_ptr<Task> task = tasks[event_id];
    lock.unlock();

    return task->error;
}

extern "C" void dispose(int event_id) {
    // Remove from tasks
    std::unique_lock lock(tasks_mutex);
    std::shared_ptr<Task> task = tasks[event_id];
    // free(task->data);
    tasks.erase(event_id);
    lock.unlock();
}

#pragma endregion
#pragma region AsyncGPU Custom Type

using namespace AsyncGPUReadbackPlugin;

GLIssuePluginEvent AsyncGPUReadbackPlugin::GetGLIssuePluginEvent() {
    static GLIssuePluginEvent issuePluginEvent =
        reinterpret_cast<GLIssuePluginEvent>(il2cpp_functions::resolve_icall("UnityEngine.GL::GLIssuePluginEvent"));
    return issuePluginEvent;
}

DEFINE_TYPE(AsyncGPUReadbackPlugin, AsyncGPUReadbackPluginRequest);

bool AsyncGPUReadbackPluginRequest::IsDone() {
    return isRequestDone(eventId);
}

bool AsyncGPUReadbackPluginRequest::HasError() {
    return isRequestError(eventId);
}

void AsyncGPUReadbackPluginRequest::Update() {
    GetGLIssuePluginEvent()(reinterpret_cast<void*>(update_renderThread), eventId);
}

void AsyncGPUReadbackPluginRequest::Dispose() {
    if (!disposed) {
        dispose(eventId);
        if (tempTexture && texture)
            UnityEngine::RenderTexture::ReleaseTemporary(texture);
        disposed = true;
    }
}

AsyncGPUReadbackPluginRequest::~AsyncGPUReadbackPluginRequest() {
    Dispose();
}

void AsyncGPUReadbackPluginRequest::GetRawData(rgba*& buffer, size_t& length) const {
    getData_mainThread(eventId, buffer, length);
}

AsyncGPUReadbackPluginRequest* AsyncGPUReadbackPlugin::Request(UnityEngine::RenderTexture* src, int width, int height, FramePool& framePool) {
    auto request = CRASH_UNLESS(il2cpp_utils::New<AsyncGPUReadbackPluginRequest*>());
    request->disposed = false;
    GLuint textureId = (uintptr_t) src->GetNativeTexturePtr().m_value.convert();

    request->frameReference = framePool.getFrame();

    request->eventId = makeRequest_mainThread(textureId, width, height, request->frameReference);
    GetGLIssuePluginEvent()(reinterpret_cast<void*>(makeRequest_renderThread), request->eventId);

    return request;
}
#pragma endregion
