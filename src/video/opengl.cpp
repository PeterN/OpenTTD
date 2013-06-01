/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file opengl_v.cpp OpenGL video driver support. */

#include "../stdafx.h"

#ifdef WITH_OPENGL

#if defined(WIN32)
	#include <windows.h>
#else
	#include <dlfcn.h>
#endif

#if defined(__APPLE__)
	#include <OpenGL/OpenGL.h>
	#include <OpenGL/gl.h>
#else
	#include <GL/gl.h>
	#include <GL/glext.h>
#if defined(UNIX)
	#include <GL/glx.h>
#endif
#endif

#include "opengl.h"
#include "../core/mem_func.hpp"
#include "../gfx_func.h"
#include "../debug.h"

#include "../table/opengl_shader.h"


#include "../safeguards.h"


static PFNGLDEBUGMESSAGECONTROLPROC _glDebugMessageControl;
static PFNGLDEBUGMESSAGECALLBACKPROC _glDebugMessageCallback;

static PFNGLGENBUFFERSPROC _glGenBuffers;
static PFNGLDELETEBUFFERSPROC _glDeleteBuffers;
static PFNGLBINDBUFFERPROC _glBindBuffer;
static PFNGLBUFFERDATAPROC _glBufferData;
static PFNGLMAPBUFFERPROC _glMapBuffer;
static PFNGLUNMAPBUFFERPROC _glUnmapBuffer;

static PFNGLGENVERTEXARRAYSPROC _glGenVertexArrays;
static PFNGLDELETEVERTEXARRAYSPROC _glDeleteVertexArrays;
static PFNGLBINDVERTEXARRAYPROC _glBindVertexArray;

static PFNGLCREATEPROGRAMPROC _glCreateProgram;
static PFNGLDELETEPROGRAMPROC _glDeleteProgram;
static PFNGLLINKPROGRAMPROC _glLinkProgram;
static PFNGLUSEPROGRAMPROC _glUseProgram;
static PFNGLGETPROGRAMIVPROC _glGetProgramiv;
static PFNGLGETPROGRAMINFOLOGPROC _glGetProgramInfoLog;
static PFNGLCREATESHADERPROC _glCreateShader;
static PFNGLDELETESHADERPROC _glDeleteShader;
static PFNGLSHADERSOURCEPROC _glShaderSource;
static PFNGLCOMPILESHADERPROC _glCompileShader;
static PFNGLATTACHSHADERPROC _glAttachShader;
static PFNGLGETSHADERIVPROC _glGetShaderiv;
static PFNGLGETSHADERINFOLOGPROC _glGetShaderInfoLog;
static PFNGLGETUNIFORMLOCATIONPROC _glGetUniformLocation;
static PFNGLUNIFORM1IPROC _glUniform1i;

static PFNGLGETATTRIBLOCATIONPROC _glGetAttribLocation;
static PFNGLENABLEVERTEXATTRIBARRAYPROC _glEnableVertexAttribArray;
static PFNGLDISABLEVERTEXATTRIBARRAYPROC _glDisableVertexAttribArray;
static PFNGLVERTEXATTRIBPOINTERARBPROC _glVertexAttribPointer;

/** A simple 2D vertex with just position and texture. */
struct Simple2DVertex {
	float x, y;
	float u, v;
};

/* static */ OpenGLBackend *OpenGLBackend::instance = NULL;

/**
 * Load an OpenGL extension function.
 * @param proc Name of the function to load.
 * @return Pointer to the function or NULL on error.
 */
void *GetOGLProcAddress(const char *proc)
{
#if defined(WIN32)
	return wglGetProcAddress(proc);
#elif defined(UNIX) && !defined(__APPLE__)
	return glXGetProcAddressARB(proc);
#else
	static void *lib = NULL;

#if defined(__APPLE__)
	if (lib == NULL) lib = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY | RTLD_GLOBAL);
#else
	if (lib == NULL) lib = dlopen("libGL.so", RTLD_LAZY);
#endif

	return lib != NULL ? dlsym(lib, proc) : NULL;
#endif
}

/**
 * Find a substring in a string made of space delimited elements. The substring
 * has to match the complete element, partial matches don't count.
 * @param string List of space delimited elements.
 * @param substring Substring to find.
 * @return Pointer to the start of the match or NULL if the substring is not present.
 */
const char *FindStringInExtensionList(const char *string, const char *substring)
{
	while (1) {
		/* Is the extension string present at all? */
		const char *pos = strstr(string, substring);
		if (pos == NULL) break;

		/* Is this a real match, i.e. are the chars before and after the matched string
		 * indeed spaces (or the start or end of the string, respectively)? */
		const char *end = pos + strlen(substring);
		if ((pos == string || pos[-1] == ' ') && (*end == ' ' || *end == '\0')) return pos;

		/* False hit, try again for the remaining string. */
		string = end;
	}

	return NULL;
}

/**
 * Check if an OpenGL extension is supported by the current context.
 * @param extension The extension string to test.
 * @return True if the extension is supported, false if not.
 */
static bool IsOpenGLExtensionSupported(const char *extension)
{
	static PFNGLGETSTRINGIPROC glGetStringi = NULL;
	static bool glGetStringi_loaded = false;

	/* Starting with OpenGL 3.0 the preferred API to get the extensions
	 * has changed. Try to load the required function once. */
	if (!glGetStringi_loaded) {
		if (IsOpenGLVersionAtLeast(3, 0)) glGetStringi = (PFNGLGETSTRINGIPROC)GetOGLProcAddress("glGetStringi");
		glGetStringi_loaded = true;
	}

	if (glGetStringi != NULL) {
		/* New style: Each supported extension can be queried and compared independently. */
		GLint num_exts;
		glGetIntegerv(GL_NUM_EXTENSIONS, &num_exts);

		for (GLint i = 0; i < num_exts; i++) {
			const char *entry = (const char *)glGetStringi(GL_EXTENSIONS, i);
			if (strcmp(entry, extension) == 0) return true;
		}
	} else {
		/* Old style: A single, space-delimited string for all extensions. */
		return FindStringInExtensionList((const char *)glGetString(GL_EXTENSIONS), extension) != NULL;
	}

	return false;
}

static byte _gl_major_ver = 0; ///< Major OpenGL version.
static byte _gl_minor_ver = 0; ///< Minor OpenGL version.

/**
 * Check if the current OpenGL version is equal or higher than a given one.
 * @param major Minimal major version.
 * @param minor Minimal minor version.
 * @pre OpenGL was initialized.
 * @return True if the OpenGL version is equal or higher than the requested one.
 */
bool IsOpenGLVersionAtLeast(byte major, byte minor)
{
	return (_gl_major_ver > major) || (_gl_major_ver == major && _gl_minor_ver >= minor);
}

/** Bind vertex buffer object extension functions. */
static bool BindVBOExtension()
{
	if (IsOpenGLVersionAtLeast(1, 5)) {
		_glGenBuffers = (PFNGLGENBUFFERSPROC)GetOGLProcAddress("glGenBuffers");
		_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)GetOGLProcAddress("glDeleteBuffers");
		_glBindBuffer = (PFNGLBINDBUFFERPROC)GetOGLProcAddress("glBindBuffer");
		_glBufferData = (PFNGLBUFFERDATAPROC)GetOGLProcAddress("glBufferData");
		_glMapBuffer = (PFNGLMAPBUFFERPROC)GetOGLProcAddress("glMapBuffer");
		_glUnmapBuffer = (PFNGLUNMAPBUFFERPROC)GetOGLProcAddress("glUnmapBuffer");
	} else {
		_glGenBuffers = (PFNGLGENBUFFERSPROC)GetOGLProcAddress("glGenBuffersARB");
		_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)GetOGLProcAddress("glDeleteBuffersARB");
		_glBindBuffer = (PFNGLBINDBUFFERPROC)GetOGLProcAddress("glBindBufferARB");
		_glBufferData = (PFNGLBUFFERDATAPROC)GetOGLProcAddress("glBufferDataARB");
		_glMapBuffer = (PFNGLMAPBUFFERPROC)GetOGLProcAddress("glMapBufferARB");
		_glUnmapBuffer = (PFNGLUNMAPBUFFERPROC)GetOGLProcAddress("glUnmapBufferARB");
	}

	return _glGenBuffers != NULL && _glDeleteBuffers != NULL && _glBindBuffer != NULL && _glBufferData != NULL && _glMapBuffer != NULL && _glUnmapBuffer != NULL;
}

/** Bind vertex array object extension functions. */
static bool BindVBAExtension()
{
	/* The APPLE and ARB variants have different semantics (that don't matter for us).
	 *  Successfully getting pointers to one variant doesn't mean it is supported for
	 *  the current context. Always check the extension strings as well. */
	if (IsOpenGLVersionAtLeast(3, 0) || IsOpenGLExtensionSupported("GL_ARB_vertex_array_object")) {
		_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)GetOGLProcAddress("glGenVertexArrays");
		_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)GetOGLProcAddress("glDeleteVertexArrays");
		_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)GetOGLProcAddress("glBindVertexArray");
	} else if (IsOpenGLExtensionSupported("GL_APPLE_vertex_array_object")) {
		_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)GetOGLProcAddress("glGenVertexArraysAPPLE");
		_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)GetOGLProcAddress("glDeleteVertexArraysAPPLE");
		_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)GetOGLProcAddress("glBindVertexArrayAPPLE");
	}

	return _glGenVertexArrays != NULL && _glDeleteVertexArrays != NULL && _glBindVertexArray != NULL;
}

/** Bind extension functions for shader support. */
static bool BindShaderExtensions()
{
	if (IsOpenGLVersionAtLeast(2, 0)) {
		_glCreateProgram = (PFNGLCREATEPROGRAMPROC)GetOGLProcAddress("glCreateProgram");
		_glDeleteProgram = (PFNGLDELETEPROGRAMPROC)GetOGLProcAddress("glDeleteProgram");
		_glLinkProgram = (PFNGLLINKPROGRAMPROC)GetOGLProcAddress("glLinkProgram");
		_glUseProgram = (PFNGLUSEPROGRAMPROC)GetOGLProcAddress("glUseProgram");
		_glGetProgramiv = (PFNGLGETPROGRAMIVPROC)GetOGLProcAddress("glGetProgramiv");
		_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)GetOGLProcAddress("glGetProgramInfoLog");
		_glCreateShader = (PFNGLCREATESHADERPROC)GetOGLProcAddress("glCreateShader");
		_glDeleteShader = (PFNGLDELETESHADERPROC)GetOGLProcAddress("glDeleteShader");
		_glShaderSource = (PFNGLSHADERSOURCEPROC)GetOGLProcAddress("glShaderSource");
		_glCompileShader = (PFNGLCOMPILESHADERPROC)GetOGLProcAddress("glCompileShader");
		_glAttachShader = (PFNGLATTACHSHADERPROC)GetOGLProcAddress("glAttachShader");
		_glGetShaderiv = (PFNGLGETSHADERIVPROC)GetOGLProcAddress("glGetShaderiv");
		_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)GetOGLProcAddress("glGetShaderInfoLog");
		_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)GetOGLProcAddress("glGetUniformLocation");
		_glUniform1i = (PFNGLUNIFORM1IPROC)GetOGLProcAddress("glUniform1i");

		_glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)GetOGLProcAddress("glGetAttribLocation");
		_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)GetOGLProcAddress("glEnableVertexAttribArray");
		_glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)GetOGLProcAddress("glDisableVertexAttribArray");
		_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERARBPROC)GetOGLProcAddress("glVertexAttribPointer");
	} else {
		/* In the ARB extension programs and shaders are in the same object space. */
		_glCreateProgram = (PFNGLCREATEPROGRAMPROC)GetOGLProcAddress("glCreateProgramObjectARB");
		_glDeleteProgram = (PFNGLDELETEPROGRAMPROC)GetOGLProcAddress("glDeleteObjectARB");
		_glLinkProgram = (PFNGLLINKPROGRAMPROC)GetOGLProcAddress("glLinkProgramARB");
		_glUseProgram = (PFNGLUSEPROGRAMPROC)GetOGLProcAddress("glUseProgramObjectARB");
		_glGetProgramiv = (PFNGLGETPROGRAMIVPROC)GetOGLProcAddress("glGetObjectParameterivARB");
		_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)GetOGLProcAddress("glGetInfoLogARB");
		_glCreateShader = (PFNGLCREATESHADERPROC)GetOGLProcAddress("glCreateShaderObjectARB");
		_glDeleteShader = (PFNGLDELETESHADERPROC)GetOGLProcAddress("glDeleteObjectARB");
		_glShaderSource = (PFNGLSHADERSOURCEPROC)GetOGLProcAddress("glShaderSourceARB");
		_glCompileShader = (PFNGLCOMPILESHADERPROC)GetOGLProcAddress("glCompileShaderARB");
		_glAttachShader = (PFNGLATTACHSHADERPROC)GetOGLProcAddress("glAttachObjectARB");
		_glGetShaderiv = (PFNGLGETSHADERIVPROC)GetOGLProcAddress("glGetObjectParameterivARB");
		_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)GetOGLProcAddress("glGetInfoLogARB");
		_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)GetOGLProcAddress("glGetUniformLocationARB");
		_glUniform1i = (PFNGLUNIFORM1IPROC)GetOGLProcAddress("glUniform1iARB");

		_glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)GetOGLProcAddress("glGetAttribLocationARB");
		_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)GetOGLProcAddress("glEnableVertexAttribArrayARB");
		_glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)GetOGLProcAddress("glDisableVertexAttribArrayARB");
		_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERARBPROC)GetOGLProcAddress("glVertexAttribPointerARB");
	}

	return _glCreateProgram != NULL && _glDeleteProgram != NULL && _glLinkProgram != NULL && _glGetProgramiv != NULL && _glGetProgramInfoLog != NULL &&
		_glCreateShader != NULL && _glDeleteShader != NULL && _glShaderSource != NULL && _glCompileShader != NULL && _glAttachShader != NULL &&
		_glGetShaderiv != NULL && _glGetShaderInfoLog != NULL && _glGetUniformLocation != NULL && _glUniform1i != NULL &&
		_glGetAttribLocation != NULL && _glEnableVertexAttribArray != NULL && _glDisableVertexAttribArray != NULL && _glVertexAttribPointer != NULL;
}

/** Callback to receive OpenGL debug messages. */
void APIENTRY DebugOutputCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
	/* Make severity human readable. */
	const char *severity_str = "";
	switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH:   severity_str = "high"; break;
		case GL_DEBUG_SEVERITY_MEDIUM: severity_str = "medium"; break;
		case GL_DEBUG_SEVERITY_LOW:    severity_str = "low"; break;
	}

	/* Make type human readable.*/
	const char *type_str = "Other";
	switch (type) {
		case GL_DEBUG_TYPE_ERROR:               type_str = "Error"; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: type_str = "Deprecated"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  type_str = "Undefined behaviour"; break;
		case GL_DEBUG_TYPE_PERFORMANCE:         type_str = "Performance"; break;
		case GL_DEBUG_TYPE_PORTABILITY:         type_str = "Portability"; break;
	}

	DEBUG(driver, 6, "OpenGL: %s (%s) - %s", type_str, severity_str, message);
}

/** Enable OpenGL debug messages if supported. */
void SetupDebugOutput()
{
#ifndef NO_DEBUG_MESSAGES
	if (_debug_driver_level < 6) return;

	if (IsOpenGLVersionAtLeast(4, 3)) {
		_glDebugMessageControl = (PFNGLDEBUGMESSAGECONTROLPROC)GetOGLProcAddress("glDebugMessageControl");
		_glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC)GetOGLProcAddress("glDebugMessageCallback");
	} else if (IsOpenGLExtensionSupported("GL_ARB_debug_output")) {
		_glDebugMessageControl = (PFNGLDEBUGMESSAGECONTROLPROC)GetOGLProcAddress("glDebugMessageControlARB");
		_glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC)GetOGLProcAddress("glDebugMessageCallbackARB");
	}

	if (_glDebugMessageControl != NULL && _glDebugMessageCallback != NULL) {
		/* Enable debug output. As synchronous debug output costs performance, we only enable it with a high debug level. */
		glEnable(GL_DEBUG_OUTPUT);
		if (_debug_driver_level >= 8) glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

		_glDebugMessageCallback(&DebugOutputCallback, NULL);
		/* Enable all messages on highest debug level.*/
		_glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, _debug_driver_level >= 9 ? GL_TRUE : GL_FALSE);
		/* Get debug messages for errors and undefined/deprecated behaviour. */
		_glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, 0, NULL, GL_TRUE);
		_glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, GL_DONT_CARE, 0, NULL, GL_TRUE);
		_glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, GL_DONT_CARE, 0, NULL, GL_TRUE);
	}
#endif
}

/**
 * Create and initialize the singleton back-end class.
 */
/* static */ const char *OpenGLBackend::Create()
{
	if (OpenGLBackend::instance != NULL) OpenGLBackend::Destroy();

	OpenGLBackend::instance = new OpenGLBackend();
	return OpenGLBackend::instance->Init();
}

/**
 * Free resources and destroy singleton back-end class.
 */
/* static */ void OpenGLBackend::Destroy()
{
	delete OpenGLBackend::instance;
	OpenGLBackend::instance = NULL;
}

/**
 * Construct OpenGL back-end class.
 */
OpenGLBackend::OpenGLBackend()
{
}

/**
 * Free allocated resources.
 */
OpenGLBackend::~OpenGLBackend()
{
	if (_glDeleteProgram != NULL) {
		_glDeleteProgram(this->vid_program);
	}
	if (_glDeleteVertexArrays != NULL) _glDeleteVertexArrays(1, &this->vao_quad);
	if (_glDeleteBuffers != NULL) {
		_glDeleteBuffers(1, &this->vbo_quad);
		_glDeleteBuffers(1, &this->vid_pbo);
	}
	glDeleteTextures(1, &this->vid_texture);
}

/**
 * Check for the needed OpenGL functionality and allocate all resources.
 * @return Error string or NULL if successful.
 */
const char *OpenGLBackend::Init()
{
	/* Always query the supported OpenGL version as the current context might have changed. */
	const char *ver = (const char *)glGetString(GL_VERSION);
	const char *vend = (const char *)glGetString(GL_VENDOR);
	DEBUG(driver, 1, "OpenGL driver: %s (%s)", vend, ver);

	_gl_major_ver = ver[0] - '0';
	_gl_minor_ver = ver[2] - '0';

	SetupDebugOutput();

	/* OpenGL 1.3 is the absolute minimum. */
	if (!IsOpenGLVersionAtLeast(1, 3)) return "OpenGL versions >= 1.3 required";
	/* Check for non-power-of-two texture support. */
	if (!IsOpenGLVersionAtLeast(2, 0) && !IsOpenGLExtensionSupported("GL_ARB_texture_non_power_of_two")) return "Non-power-of-two textures not supported";
	/* Check for vertex buffer objects. */
	if (!IsOpenGLVersionAtLeast(1, 5) && !IsOpenGLExtensionSupported("ARB_vertex_buffer_object")) return "Vertex buffer objects not supported";
	if (!BindVBOExtension()) return "Failed to bind VBO extension functions";
	/* Check for pixel buffer objects. */
	if (!IsOpenGLVersionAtLeast(2, 1) && !IsOpenGLExtensionSupported("GL_ARB_pixel_buffer_object")) return "Pixel buffer objects not supported";
	/* Check for vertex array objects. */
	if (!IsOpenGLVersionAtLeast(3, 0) && (!IsOpenGLExtensionSupported("GL_ARB_vertex_array_object") || !IsOpenGLExtensionSupported("GL_APPLE_vertex_array_object"))) return "Vertex array objects not supported";
	if (!BindVBAExtension()) return "Failed to bind VBA extension functions";
	/* Check for shader objects. */
	if (!IsOpenGLVersionAtLeast(2, 0) && (!IsOpenGLExtensionSupported("GL_ARB_shader_objects") || !IsOpenGLExtensionSupported("GL_ARB_fragment_shader") || !IsOpenGLExtensionSupported("GL_ARB_vertex_shader"))) return "No shader support";
	if (!BindShaderExtensions()) return "Failed to bind shader extension functions";

	DEBUG(driver, 2, "OpenGL shading language version: %s", (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION));

	if (!this->InitShaders()) return "Failed to initialize shaders";

	/* Setup video buffer texture. */
	glGenTextures(1, &this->vid_texture);
	glBindTexture(GL_TEXTURE_2D, this->vid_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	if (glGetError() != GL_NO_ERROR) return "Can't generate video buffer texture";

	/* Bind texture to shader program. */
	GLint tex_location = _glGetUniformLocation(this->vid_program, "colour_tex");
	_glUseProgram(this->vid_program);
	_glUniform1i(tex_location, 0); // Texture unit 0.

	/* Create pixel buffer object as video buffer storage. */
	_glGenBuffers(1, &this->vid_pbo);
	_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->vid_pbo);
	if (glGetError() != GL_NO_ERROR) return "Can't allocate pixel buffer for video buffer";

	/* Prime vertex buffer with a full-screen quad and store
	 * the corresponding state in a vertex array object. */
	static const Simple2DVertex vert_array[] = {
		//  x     y    u    v
		{  1.f, -1.f, 1.f, 1.f },
		{  1.f,  1.f, 1.f, 0.f },
		{ -1.f, -1.f, 0.f, 1.f },
		{ -1.f,  1.f, 0.f, 0.f },
	};

	/* Create VAO. */
	_glGenVertexArrays(1, &this->vao_quad);
	_glBindVertexArray(this->vao_quad);

	/* Create and fill VBO. */
	_glGenBuffers(1, &this->vbo_quad);
	_glBindBuffer(GL_ARRAY_BUFFER, this->vbo_quad);
	_glBufferData(GL_ARRAY_BUFFER, sizeof(vert_array), vert_array, GL_STATIC_DRAW);
	if (glGetError() != GL_NO_ERROR) return "Can't generate VBO for fullscreen quad";

	/* Set vertex state. */
	GLint loc_position = _glGetAttribLocation(this->vid_program, "position");
	GLint colour_position = _glGetAttribLocation(this->vid_program, "colour_uv");
	_glEnableVertexAttribArray(loc_position);
	_glEnableVertexAttribArray(colour_position);
	_glVertexAttribPointer(loc_position, 2, GL_FLOAT, GL_FALSE, sizeof(Simple2DVertex), (GLvoid *)offsetof(Simple2DVertex, x));
	_glVertexAttribPointer(colour_position, 2, GL_FLOAT, GL_FALSE, sizeof(Simple2DVertex), (GLvoid *)offsetof(Simple2DVertex, u));
	_glBindVertexArray(0);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glDisable(GL_DEPTH_TEST);

	return NULL;
}

/**
 * Check a shader for compilation errors and log them if necessary.
 * @param shader Shader to check.
 * @return True if the shader is valid.
 */
static bool VerifyShader(GLuint shader)
{
	static ReusableBuffer<char> log_buf;

	GLint result;
	_glGetShaderiv(shader, GL_COMPILE_STATUS, &result);

	/* Output log if there is one. */
	GLint log_len;
	_glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
	if (log_len > 0) {
		_glGetShaderInfoLog(shader, log_len, NULL, log_buf.Allocate(log_len));
		DEBUG(driver, result != GL_TRUE ? 0 : 2, "%s", log_buf.GetBuffer()); // Always print on failure.
	}

	return result == GL_TRUE;
}

/**
 * Check a program for link errors and log them if necessary.
 * @param program Program to check.
 * @return True if the program is valid.
 */
static bool VerifyProgram(GLuint program)
{
	static ReusableBuffer<char> log_buf;

	GLint result;
	_glGetProgramiv(program, GL_LINK_STATUS, &result);

	/* Output log if there is one. */
	GLint log_len;
	_glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
	if (log_len > 0) {
		_glGetProgramInfoLog(program, log_len, NULL, log_buf.Allocate(log_len));
		DEBUG(driver, result != GL_TRUE ? 0 : 2, "%s", log_buf.GetBuffer()); // Always print on failure.
	}

	return result == GL_TRUE;
}

/**
 * Create all needed shader programs.
 * @return True if successful, false otherwise.
 */
bool OpenGLBackend::InitShaders()
{
	const char *ver = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
	if (ver == NULL) return false;

	int glsl_major  = ver[0] - '0';
	int glsl_minor = ver[2] - '0';

	bool glsl_150 = (IsOpenGLVersionAtLeast(3, 2) || glsl_major > 1 || (glsl_major == 1 && glsl_minor >= 5)) && _glBindFragDataLocation != NULL;

	/* Create vertex shader. */
	GLuint vert_shader = _glCreateShader(GL_VERTEX_SHADER);
	_glShaderSource(vert_shader, glsl_150 ? lengthof(_vertex_shader_direct_150) : lengthof(_vertex_shader_direct), glsl_150 ? _vertex_shader_direct_150 : _vertex_shader_direct, NULL);
	_glCompileShader(vert_shader);
	if (!VerifyShader(vert_shader)) return false;

	/* Create fragment shader. */
	GLuint frag_shader = _glCreateShader(GL_FRAGMENT_SHADER);
	_glShaderSource(frag_shader, glsl_150 ? lengthof(_frag_shader_direct_150) : lengthof(_frag_shader_direct), glsl_150 ? _frag_shader_direct_150 : _frag_shader_direct, NULL);
	_glCompileShader(frag_shader);
	if (!VerifyShader(frag_shader)) return false;

	/* Link shaders to program. */
	this->vid_program = _glCreateProgram();
	_glAttachShader(this->vid_program, vert_shader);
	_glAttachShader(this->vid_program, frag_shader);
	_glLinkProgram(this->vid_program);
	if (!VerifyProgram(this->vid_program)) return false;

	_glDeleteShader(vert_shader);
	_glDeleteShader(frag_shader);

	return true;
}

/**
 * Change the size of the drawing window and allocate matching resources.
 * @param w New width of the window.
 * @param h New height of the window.
 * @param force Recreate resources even if size didn't change.
 * @param False if nothing had to be done, true otherwise.
 */
bool OpenGLBackend::Resize(int w, int h, bool force)
{
	if (!force && _screen.width == w && _screen.height == h) return false;

	glViewport(0, 0, w, h);

	/* Re-allocate video buffer texture and backing store. */
	_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->vid_pbo);
	_glBufferData(GL_PIXEL_UNPACK_BUFFER, w * h * 4, NULL, GL_DYNAMIC_READ); // Buffer content has to persist from frame to frame and is read back by the blitter, which means a READ usage hint.
	_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	glBindTexture(GL_TEXTURE_2D, this->vid_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);

	/* Set new viewport. */
	_screen.height = h;
	_screen.width = w;
	_screen.pitch = w;
	_screen.dst_ptr = this->GetVideoBuffer();

	return true;
}

/**
 * Render video buffer to the screen.
 */
void OpenGLBackend::Paint()
{
	glClear(GL_COLOR_BUFFER_BIT);

	/* Blit video buffer to screen. */
	glBindTexture(GL_TEXTURE_2D, this->vid_texture);
	_glUseProgram(this->vid_program);
	_glBindVertexArray(this->vao_quad);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/**
 * Get a pointer to the memory for the video driver to draw to.
 * @return Pointer to draw on.
 */
void *OpenGLBackend::GetVideoBuffer()
{
	_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->vid_pbo);
	return _glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_WRITE);
}

/**
 * Update video buffer texture after the video buffer was filled.
 * @param update_rect Rectangle encompassing the dirty region of the video buffer.
 */
void OpenGLBackend::ReleaseVideoBuffer(const Rect &update_rect)
{
	assert(this->vid_pbo != 0);

	_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->vid_pbo);
	_glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

	/* Update changed rect of the video buffer texture. */
	if (update_rect.left != update_rect.right) {
		glBindTexture(GL_TEXTURE_2D, this->vid_texture);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, _screen.pitch);
		glTexSubImage2D(GL_TEXTURE_2D, 0, update_rect.left, update_rect.top, update_rect.right - update_rect.left, update_rect.bottom - update_rect.top, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, (GLvoid *)(size_t)(update_rect.top * _screen.pitch + update_rect.left));
	}
}

#endif /* WITH_OPENGL */
