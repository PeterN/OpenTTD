/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file win32_v.h Base of the Windows video driver. */

#ifndef VIDEO_WIN32_H
#define VIDEO_WIN32_H

#include "video_driver.hpp"

/** Base class for Windows video drivers. */
class VideoDriver_Win32Base : public VideoDriver {
public:
	VideoDriver_Win32Base() : main_wnd(NULL) {}

	/* virtual */ void Stop();

	/* virtual */ void MakeDirty(int left, int top, int width, int height);

	/* virtual */ void MainLoop();

	/* virtual */ bool ChangeResolution(int w, int h);

	/* virtual */ bool ToggleFullscreen(bool fullscreen);

	/* virtual */ void AcquireBlitterLock();

	/* virtual */ void ReleaseBlitterLock();

	/* virtual */ bool ClaimMousePointer();

	/* virtual */ void EditBoxLostFocus();

protected:
	HWND    main_wnd;      ///< Window handle.

	void Initialize();
	bool MakeWindow(bool full_screen);
	virtual uint8 GetFullscreenBpp();

	void ClientSizeChanged(int w, int h, bool force = false);
	void CheckPaletteAnim();

	/** (Re-)create the backing store. */
	virtual bool AllocateBackingStore(int w, int h, bool force = false) = 0;
	/** Get a pointer to the video buffer. */
	virtual void *GetVideoPointer() = 0;
	/** Hand video buffer back to the painting backend. */
	virtual void ReleaseVideoPointer() {}
	/** Palette of the window has changed. */
	virtual void PaletteChanged(HWND hWnd) = 0;
	/** Window got a paint message. */
	virtual void Paint(HWND hWnd, bool in_sizemove) = 0;
	/** Thread function for threaded drawing. */
	virtual void PaintThread() = 0;
	/** Lock video buffer for drawing if it isn't already mapped. */
	virtual bool LockVideoBuffer();
	/** Unlock video buffer. */
	virtual void UnlockVideoBuffer();

	static void PaintWindowThreadThunk(void *data);
	friend LRESULT CALLBACK WndProcGdi(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

/** The GDI video driver for windows. */
class VideoDriver_Win32GDI : public VideoDriver_Win32Base {
public:
	VideoDriver_Win32GDI() : dib_sect(NULL), gdi_palette(NULL), buffer_bits(NULL) {}

	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ bool AfterBlitterChange();

	/* virtual */ const char *GetName() const { return "win32"; }

protected:
	HBITMAP  dib_sect;      ///< Blitter target.
	HPALETTE gdi_palette;   ///< Handle to windows palette.
	RECT     update_rect;   ///< Rectangle to update during the next paint event.
	void     *buffer_bits;  ///< Video buffer memory.

	/* virtual */ uint8 GetFullscreenBpp() { return 32; } // OpenGL is always 32 bpp.
	/* virtual */ bool AllocateBackingStore(int w, int h, bool force = false);
	/* virtual */ void *GetVideoPointer() { return this->buffer_bits; }
	/* virtual */ void PaletteChanged(HWND hWnd);
	/* virtual */ void Paint(HWND hWnd, bool in_sizemove);
	/* virtual */ void PaintThread();

	void MakePalette();
	void UpdatePalette(HDC dc, uint start, uint count);
	void PaintWindow(HDC dc);

#ifdef _DEBUG
public:
	static int RedrawScreenDebug();
#endif
};

/** The factory for Windows' video driver. */
class FVideoDriver_Win32GDI : public DriverFactoryBase {
public:
	FVideoDriver_Win32GDI() : DriverFactoryBase(Driver::DT_VIDEO, 10, "win32", "Win32 GDI Video Driver") {}
	/* virtual */ Driver *CreateInstance() const { return new VideoDriver_Win32GDI(); }
};

#ifdef WITH_OPENGL

/** The OpenGL video driver for windows. */
class VideoDriver_Win32OpenGL : public VideoDriver_Win32Base {
public:
	VideoDriver_Win32OpenGL() : dc(NULL), gl_rc(NULL) {}

	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void MakeDirty(int left, int top, int width, int height);

	/* virtual */ bool ChangeResolution(int w, int h);

	/* virtual */ bool ToggleFullscreen(bool fullscreen);

	/* virtual */ bool AfterBlitterChange();

	/* virtual */ const char *GetName() const { return "win32-opengl"; }

protected:
	HDC   dc;          ///< Window device context.
	HGLRC gl_rc;       ///< OpenGL context.
	Rect  dirty_rect;  ///< Rectangle encompassing the dirty area of the video buffer.

	/* virtual */ bool AllocateBackingStore(int w, int h, bool force = false);
	/* virtual */ void *GetVideoPointer();
	/* virtual */ void ReleaseVideoPointer();
	/* virtual */ void PaletteChanged(HWND hWnd);
	/* virtual */ void Paint(HWND hWnd, bool in_sizemove);
	/* virtual */ void PaintThread() {}

	const char *AllocateContext();
	void DestroyContext();
};

/** The factory for Windows' OpenGL video driver. */
class FVideoDriver_Win32OpenGL : public DriverFactoryBase {
public:
	FVideoDriver_Win32OpenGL() : DriverFactoryBase(Driver::DT_VIDEO, 9, "win32-opengl", "Win32 OpenGL Video Driver") {}
	/* virtual */ Driver *CreateInstance() const { return new VideoDriver_Win32OpenGL(); }
};

#endif /* WITH_OPENGL */

#endif /* VIDEO_WIN32_H */
