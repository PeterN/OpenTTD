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

	bool MakeWindow(bool full_screen);

	void ClientSizeChanged(int w, int h);
	void CheckPaletteAnim();

	/** (Re-)create the backing store. */
	virtual bool AllocateBackingStore(int w, int h, bool force = false) = 0;
	/** Palette of the window has changed. */
	virtual void PaletteChanged(HWND hWnd) = 0;
	/** Window got a paint message. */
	virtual void Paint(HWND hWnd, bool in_sizemove) = 0;
	/** Thread function for threaded drawing. */
	virtual void PaintThread() = 0;

	static void PaintWindowThreadThunk(void *data);
	friend LRESULT CALLBACK WndProcGdi(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

/** The GDI video driver for windows. */
class VideoDriver_Win32GDI : public VideoDriver_Win32Base {
public:
	VideoDriver_Win32GDI() : dib_sect(NULL), gdi_palette(NULL) {}

	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ bool AfterBlitterChange();

	/* virtual */ const char *GetName() const { return "win32"; }

protected:
	HBITMAP  dib_sect;      ///< Blitter target.
	HPALETTE gdi_palette;   ///< Handle to windows palette.
	RECT     update_rect;   ///< Rectangle to update during the next paint event.

	/* virtual */ bool AllocateBackingStore(int w, int h, bool force = false);
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

#endif /* VIDEO_WIN32_H */
