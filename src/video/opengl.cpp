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

#include "../safeguards.h"


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
static const char *FindStringInExtensionList(const char *string, const char *substring)
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
	glDeleteTextures(1, &this->vid_texture);
	free(this->vid_buffer);
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

	/* OpenGL 1.3 is the absolute minimum. */
	if (!IsOpenGLVersionAtLeast(1, 3)) return "OpenGL versions >= 1.3 required";
	/* Check for non-power-of-two texture support. */
	if (!IsOpenGLVersionAtLeast(2, 0) && !IsOpenGLExtensionSupported("GL_ARB_texture_non_power_of_two")) return "Non-power-of-two textures not supported";

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

	/* Prime vertex array with a full-screen quad. */
	static const float vert_array[] = { 1.f, -1.f, 1.f, 1.f, -1.f, -1.f, -1.f, 1.f };
	static const float tex_array[]  = { 1.f,  1.f, 1.f, 0.f,  0.f,  1.f,  0.f, 0.f };

	glVertexPointer(2, GL_FLOAT, 0, vert_array);
	glTexCoordPointer(2, GL_FLOAT, 0, tex_array);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	return NULL;
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
	if (!force && _screen.width == w && _screen.height == h && this->vid_buffer != NULL) return false;

	glViewport(0, 0, w, h);

	/* Re-allocate video buffer texture. */
	free(this->vid_buffer);
	this->vid_buffer = CallocT<uint32>(w * h); // 32bpp

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
	assert(this->vid_buffer != NULL);

	glClear(GL_COLOR_BUFFER_BIT);

	/* Update changed rect of the video buffer texture. */
	glBindTexture(GL_TEXTURE_2D, this->vid_texture);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, _screen.pitch);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _screen.width, _screen.height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, this->vid_buffer);

	/* Blit video buffer to screen. */
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

#endif /* WITH_OPENGL */
