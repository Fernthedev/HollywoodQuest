#include "CustomTypes/AsyncGPUReadbackPluginRequest.hpp"

#include "main.hpp"
#include "opengl_replay/Shader.hpp"

#include <string>
#include <shared_mutex>
#include <thread>

#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>
#include "CustomTypes/TypeHelpers.hpp"

#include "UnityEngine/GL.hpp"
#include "shaders.hpp"

// Looks like this was from https://github.com/Alabate/AsyncGPUReadbackPlugin

#pragma region ReadPixels OpenGL
struct Task {
	GLuint origTexture;
	GLuint newTexture;
	GLuint fbo;
	GLuint pbo;
	GLsync fence;
	bool initialized = false;
	bool error = false;
	bool done = false;
	rgb24* data;
	int miplevel;
	size_t size;
	int height;
	int width;
	int depth;
	GLint internal_format;
};

static std::unordered_map<int,std::shared_ptr<Task>> tasks;
static std::shared_mutex tasks_mutex;
static int next_event_id = 1;

extern "C" int makeRequest_mainThread(GLuint texture, GLuint texture2, int miplevel) {
	// Create the task
	std::shared_ptr<Task> task = std::make_shared<Task>();
	task->origTexture = texture;
	task->newTexture = texture2;
	task->miplevel = miplevel;
	int event_id = next_event_id;
	next_event_id++;

	// Save it (lock because possible vector resize)
	std::unique_lock lock(tasks_mutex);
	tasks[event_id] = task;
	lock.unlock();

	return event_id;
}

// Code from xyonico, thank you very much!
void BlitShader(GLuint cameraSrcTexture, Shader& shader) {
    // This function assumes that a framebuffer has already been bound with a texture different from cameraSrcTexture.

    // Prepare the shader
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, cameraSrcTexture);
    shader.use();

    // Prepare the VBO
    GLuint quadVertices;
    glGenVertexArrays(1, &quadVertices);

    glBindVertexArray(quadVertices);

    // For some reason this is needed. Likely as an optimization
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDeleteVertexArrays(1, &quadVertices);
}

extern "C" void makeRequest_renderThread(int event_id) {

	// Get task back
    std::shared_lock lock(tasks_mutex);
	std::shared_ptr<Task> task = tasks[event_id];
	lock.unlock();

	// Get texture informations
	glBindTexture(GL_TEXTURE_2D, task->newTexture);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, task->miplevel, GL_TEXTURE_WIDTH, &(task->width));
	glGetTexLevelParameteriv(GL_TEXTURE_2D, task->miplevel, GL_TEXTURE_HEIGHT, &(task->height));
	glGetTexLevelParameteriv(GL_TEXTURE_2D, task->miplevel, GL_TEXTURE_DEPTH, &(task->depth));
	glGetTexLevelParameteriv(GL_TEXTURE_2D, task->miplevel, GL_TEXTURE_INTERNAL_FORMAT, &(task->internal_format));
	auto pixelSize = getPixelSizeFromInternalFormat(task->internal_format);

	// In our current state, this turns out to be 32 * 1920 * 1080 * 1 (66,355,200)
	// However, we've been expecting this to be 1920 * 1080 * 3 (6,220,800)

	// According to this, we don't need depth: https://github.com/sirjuddington/SLADE/blob/4959a4ab95de4eb59b9dd0c9a05ccd2641aa9bea/src/UI/Canvas/MapPreviewCanvas.cpp#L963
	task->size = calculateFrameSize(task->width, task->height);

	// The format is GL_UNSIGNED_BYTE which is correct.

    if (task->size == 0
        || getFormatFromInternalFormat(task->internal_format) == 0
        || getTypeFromInternalFormat(task->internal_format) == 0) {
        task->error = true;
        return;
    }

    // Start the read request

    // Allocate the final data buffer !!! WARNING: free, will have to be done on script side !!!!
    task->data = static_cast<rgb24*>(std::malloc(task->size));

    // Create the fbo (frame buffer object) from the given texture
	glGenFramebuffers(1, &(task->fbo));

	// Bind the texture to the fbo
	glBindFramebuffer(GL_FRAMEBUFFER, task->fbo);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, task->newTexture, 0);

	GLenum DrawBuffers = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &DrawBuffers);

	glViewport(0,0, task->width, task->height);

    IL2CPP_CATCH_HANDLER(
		// Enable sRGB shader
		static Shader sRGBShader = shaderRGBGammaConvert();
		BlitShader(task->origTexture, sRGBShader);
	)

	// Create and bind pbo (pixel buffer object) to fbo
	glGenBuffers(1, &(task->pbo));
	glBindBuffer(GL_PIXEL_PACK_BUFFER, task->pbo);
	glBufferData(GL_PIXEL_PACK_BUFFER, task->size, 0, GL_DYNAMIC_READ);


    // Start the read request
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, task->width, task->height, getFormatFromInternalFormat(task->internal_format), getTypeFromInternalFormat(task->internal_format), 0);


	// Unbind buffers
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Fence to know when it's ready
	task->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

	// Done init
	task->initialized = true;
}


extern "C" void update_renderThread(int event_id) {
    // Get task back
    std::shared_lock lock(tasks_mutex);
	std::shared_ptr<Task> task = tasks[event_id];
	lock.unlock();

	// Check if task has not been already deleted by main thread
	if(task == nullptr) {
		return;
	}

	// Do something only if initialized (thread safety)
	if (!task->initialized || task->done) {
		return;
	}

	// Check fence state
	GLint status = 0;
	GLsizei length = 0;
	glGetSynciv(task->fence, GL_SYNC_STATUS, sizeof(GLint), &length, &status);
	if (length <= 0) {
		task->error = true;
		task->done = true;
		return;
	}

	// When it's done
	if (status == GL_SIGNALED) {

		// Bind back the pbo
		glBindBuffer(GL_PIXEL_PACK_BUFFER, task->pbo);

		// Map the buffer and copy it to data
		auto* ptr = reinterpret_cast<rgb24*>(glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, task->size, GL_MAP_READ_BIT));
        memcpy(task->data, ptr, task->size);

		// Unmap and unbind
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

		// Clear buffers
		glDeleteFramebuffers(1, &(task->fbo));
		glDeleteBuffers(1, &(task->pbo));
		glDeleteSync(task->fence);

		// yeah task is done!
		task->done = true;
	}
}

extern "C" void getData_mainThread(int event_id, rgb24*& buffer, size_t& length) {
	// Get task back
    std::shared_lock lock(tasks_mutex);
	std::shared_ptr<Task> task = tasks[event_id];
	lock.unlock();

	// Do something only if initialized (thread safety)
	if (!task->done) {
		return;
	}

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
    std::shared_lock lock(tasks_mutex);;
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
    static GLIssuePluginEvent issuePluginEvent = reinterpret_cast<GLIssuePluginEvent>(il2cpp_functions::resolve_icall("UnityEngine.GL::GLIssuePluginEvent"));
    return issuePluginEvent;
}

DEFINE_TYPE(AsyncGPUReadbackPlugin, AsyncGPUReadbackPluginRequest);

void AsyncGPUReadbackPluginRequest::ctor(UnityEngine::RenderTexture* src) {
    disposed = false;
    GLuint textureId = reinterpret_cast<uintptr_t>(src->GetNativeTexturePtr().m_value);


    UnityEngine::RenderTexture* newTexture = UnityEngine::RenderTexture::GetTemporary(src->get_descriptor());
    newTexture->Create();

    this->texture = newTexture;

    // TODO: Should this be configurable if someone didn't want the gamma correction?
    this->tempTexture = true;

    GLuint newTextureId = reinterpret_cast<uintptr_t>(newTexture->GetNativeTexturePtr().m_value);
    eventId = makeRequest_mainThread(textureId, newTextureId, 0);
    GetGLIssuePluginEvent()(reinterpret_cast<void*>(makeRequest_renderThread), eventId);
}

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
        if (tempTexture && il2cpp_utils::AssignableFrom<UnityEngine::RenderTexture*>(texture->klass)) {
            UnityEngine::RenderTexture::ReleaseTemporary(reinterpret_cast<UnityEngine::RenderTexture *>(texture));
        }
        disposed = true;
    }
    // Comment if using the same texture
    //    UnityEngine::RenderTexture::ReleaseTemporary((UnityEngine::RenderTexture*)texture);
}

AsyncGPUReadbackPluginRequest::~AsyncGPUReadbackPluginRequest() {
    Dispose();
}

void AsyncGPUReadbackPluginRequest::GetRawData(rgb24*& buffer, size_t& length) const {
    getData_mainThread(eventId, buffer, length);
}

AsyncGPUReadbackPluginRequest* AsyncGPUReadbackPlugin::Request(UnityEngine::RenderTexture* src) {
    return il2cpp_utils::New<AsyncGPUReadbackPluginRequest*>(src).value();
}
#pragma endregion
